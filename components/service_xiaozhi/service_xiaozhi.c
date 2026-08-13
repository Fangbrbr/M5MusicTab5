/**
 * @file service_xiaozhi.c
 * @brief 小智语音助手核心协议栈：状态机 + xz_task + 事件队列
 */

#include "service_xiaozhi.h"

#include "service_xiaozhi_config.h"
#include "xiaozhi_ota.h"
#include "service_ws.h"      /* WebSocket 已独立为 service_ws/ */
#include "service_voice.h"   /* 语音前端统一入口：AFE/WakeNet/Opus/重采样 */
#include "xiaozhi_mcp.h"

#include "service_audio.h"
#include "service_nvs.h"
#include "service_wifi.h"
#include "service_power.h"

#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "stdio.h"
#include "stdlib.h"
#include "string.h"

static const char *TAG = "service_xiaozhi";
/** @brief aux 反压水位（帧）：剩余空间不足一个最长 TTS 包（120ms 立体声
 *  ≈5.3K 帧 @44.1kHz）即节流，把积压倒灌回 WS 事件队列（编码域更省内存） */
#define XZ_AUX_BACKPRESSURE_FREE_FRAMES 6000
/** @brief aux 反压单包等待上限（ms）：超时仍解码，aux_write 截断兜底，
 *  防对端失控持续超前时消费链路锁死 */
#define XZ_AUX_BACKPRESSURE_MAX_MS      300


/** @brief 内部消息类型（ws 回调 → xz_task） */
typedef enum {
    XZ_MSG_CONNECTED = 0,   /*!< WebSocket 已连接 */
    XZ_MSG_DISCONNECTED,    /*!< WebSocket 断开 */
    XZ_MSG_HELLO,           /*!< 服务器 hello 已解析 */
    XZ_MSG_TTS_START,       /*!< 下行播报开始 */
    XZ_MSG_TTS_STOP,        /*!< 下行播报结束 */
    XZ_MSG_TTS_SENTENCE,    /*!< AI 回复句子（text） */
    XZ_MSG_STT,             /*!< 用户语音识别文本（text） */
    XZ_MSG_ALERT,           /*!< 服务器 alert（text=message） */
    XZ_MSG_SYSTEM_REBOOT,   /*!< 服务器要求重启 */
    XZ_MSG_MCP,             /*!< MCP JSON-RPC 请求（heap_text 为堆上 payload JSON） */
    XZ_MSG_WS_ERROR,        /*!< WebSocket 错误（aux=握手 HTTP 状态码，无则 0） */
} xz_msg_type_t;

/** @brief 内部消息 */
typedef struct {
    xz_msg_type_t type;
    char *heap_text;        /*!< XZ_MSG_MCP 专用：cJSON_PrintUnformatted 产物，接收方 cJSON_free */
    int aux;                /*!< XZ_MSG_WS_ERROR 专用：握手 HTTP 状态码 */
    char text[256];
} xz_msg_t;

/** @brief 控制命令（App 等任意上下文 → 任务） */
typedef enum {
    XZ_CMD_START = 0,   /* 开启会话（App 进入） */
    XZ_CMD_TALK_START,
    XZ_CMD_TALK_STOP,
    XZ_CMD_SHUTDOWN,    /* 结束会话（App 切出），任务本体常驻不退 */
} xz_cmd_t;

static QueueHandle_t s_cmd_queue = NULL;
static QueueHandle_t s_msg_queue = NULL;
static QueueHandle_t s_evt_queue = NULL;

/* 会话运行中收到 START 时置位，会话收尾后由入口循环接续新会话（reset 流程依赖） */
static volatile bool s_start_pending = false;
static volatile bool s_shutdown = false;
static volatile service_xiaozhi_state_t s_state = SERVICE_XIAOZHI_STATE_IDLE;

/* 会话参数：hello 到达时在 ws 回调写一次，此后 xz_task 只读 */
static char s_session_id[64] = {0};
static uint32_t s_server_sample_rate = SERVICE_XIAOZHI_OPUS_DEC_DEFAULT_SAMPLE_RATE;
static uint32_t s_server_frame_duration = SERVICE_XIAOZHI_OPUS_FRAME_DURATION_MS;

/* 会话模式（run_session 入口 afe_open 后确定一次，此后只读）：
 * AFE 打开成功 → auto（唤醒词 + 服务器 VAD 连续对话）；失败 → manual（按住说话）。 */
static bool s_use_auto = false;

/* 最近一次本地 VAD 状态（边沿检测，仅用于生成 VAD 事件） */
static int s_voice_last_vad = 0;

/* 语音通道（ws + 解码器）是否已打开。按需开关：待触发态关闭不占用网络通道，
 * 触发时开、对话结束/空闲超时关，消除「断线即紧密重连」的风暴。 */
static bool s_channel_open = false;

/* 唤醒命中标志：STANDBY 下语音前端检出唤醒词时置位，主循环据此开对话，
 * 避免在语音前端回调中递归打开通道（open_channel 会阻塞等 hello）。 */
static bool s_trigger_conv = false;

/* 开机自启标志：常驻会话不再依赖 AI App 前台，开机即进入待触发态，
 * 实现全局离线唤醒词监听（对齐原版 Idle 常驻行为） */
static bool s_boot_autostart = false;

/* 开机配置刷新标志：ws 配置刷新仅开机首个会话执行一次，
 * 避免 App 每次进出都阻塞一个 HTTP 超时 */
static bool s_config_refreshed = false;

/* WiFi 上线沿跟踪与未激活重试计时（app_loop 内使用） */
static bool s_wifi_was_up = false;

/* AI UI 进入时请求激活：由 service_xiaozhi_set_ai_ui_active(true) 设置，
 * 在主循环中检查并执行 xz_do_activation() */
static bool s_activation_requested = false;

static TickType_t s_last_channel_fail = 0;  /* 通道打开失败时刻，退避窗口用 */
static TickType_t s_last_offline_notify = 0; /* 离线唤醒通知限流 */

/* 全局唤醒开关（AI App 内 ai_switch_wake_anywhere，NVS 持久化，默认关）：
 * 关=仅 AI 屏前台时响应唤醒；开=任意界面唤醒直接后台对话（不拉起 AI 屏） */
static bool s_wake_anywhere = false;
/* AI App UI 是否在前台（由 app_ai_agent 生命周期同步） */
static volatile bool s_ai_ui_active = false;

/* 系统级暂停（FTP 独占页等）：置位时停止会话、关闭唤醒检出并丢弃唤醒
 * 事件，re-arm 点（enter_standby/process/set_wake_anywhere/set_ai_ui_active）
 * 全部跳过 enable_wake；清除后按激活状态恢复检出 */
static bool s_suspended = false;

/* 待重放的激活码：当 AI 屏不在前台时收到的激活码暂存此处，
 * AI 屏回前台时自动重放，防止事件被丢弃导致绑定气泡不显示 */
#define XZ_ACTIVATION_CODE_LEN 16
static char s_pending_activation_code[XZ_ACTIVATION_CODE_LEN] = {0};

/* 最近一次服务器活动 tick（STT/TTS/句子刷新）：空闲超时判据，SPEAKING 不计入。 */
static TickType_t s_last_activity = 0;

/* 最近一次入站帧 tick（任意文本/二进制，ws 回调写）：半开连接看门狗判据。
 * Trap: 对端死而无 FIN 时 DISCONNECTED 永不触发，必须靠应用层判活。 */
static volatile TickType_t s_last_rx_tick = 0;

/* auto 续听延迟标志：TTS_STOP 后等 aux 排空再开 mic，防回答尾巴被掐 */
static bool s_pending_listen = false;
static TickType_t s_pending_listen_at = 0;

/* 退下请求（MCP self.standby）：等告别 TTS 播完再关通道；置位时刻供兜底超时 */
static volatile bool s_standby_pending = false;
static volatile TickType_t s_standby_pending_at = 0;

/* 会话标识（run_session 入口解析一次，此后只读）：按需开通道时 ws 连接所需 */
static char s_client_id[SERVICE_NVS_XZ_UUID_MAX_LEN] = {0};
static char s_device_id[18] = {0};

/* 下行音频统计（ws 回调写，xz_task 于 TTS 边界清零）：仅供日志，轻度竞态可忽 */
static volatile uint32_t s_dl_audio_frames = 0;
static volatile uint32_t s_dl_audio_bytes = 0;

static void xz_run_session(void);
static void xz_on_ws_text(const char *json, int len, void *ctx);
static void xz_on_ws_audio(const uint8_t *data, int len, void *ctx);
static void xz_on_ws_connected(void *ctx);
static void xz_on_ws_disconnected(void *ctx);
static esp_err_t xz_open_channel(void);
static void xz_close_channel(void);
static void xz_enter_standby(void);
static void xz_begin_conversation(const char *mode);
static void xz_drain_cmds(void);
static void xz_handle_ws_error(int http_status);
static void xz_config_refresh_once(void);

/* ws 回调上下文（文件作用域，按需开通道时复用） */

/** @brief WS 事件统一回调 */
static void xz_on_ws_event_handler(const service_ws_event_t *evt, void *user_data)
{
    (void)user_data;
    switch (evt->type) {
    case SERVICE_WS_EVT_TEXT:
        xz_on_ws_text((const char *)evt->data, (int)evt->len, NULL);
        break;
    case SERVICE_WS_EVT_BINARY:
        xz_on_ws_audio(evt->data, evt->len, NULL);
        break;
    case SERVICE_WS_EVT_CONNECTED:
        xz_on_ws_connected(NULL);
        break;
    case SERVICE_WS_EVT_DISCONNECTED:
        xz_on_ws_disconnected(NULL);
        break;
    case SERVICE_WS_EVT_ERROR:
        /* 握手状态码（401/403=凭据失效）需交 xz_task 决策：清凭据走重新绑定 */
        if (evt->http_status != 0) {
            if (s_msg_queue != NULL) {
                xz_msg_t msg = { .type = XZ_MSG_WS_ERROR, .aux = evt->http_status, .text = {0} };
                if (xQueueSend(s_msg_queue, &msg, 0) != pdTRUE) {
                    ESP_LOGW(TAG, "msg queue full, drop ws error status=%d", evt->http_status);
                }
            }
        }
        break;
    default:
        break;
    }
}

/**
 * @brief 投递 App 侧事件（队列满丢弃，任何上下文可调）
 */
static void xz_post_event(service_xiaozhi_event_type_t type, const char *text)
{
    if (s_evt_queue == NULL) {
        return;
    }
    service_xiaozhi_event_t evt = { .type = type, .state = s_state, .text = {0} };
    if (text != NULL) {
        strncpy(evt.text, text, sizeof(evt.text) - 1);
    }
    /* 激活码事件特殊处理：即使 AI 屏不在前台也暂存，AI 屏回前台时重放 */
    if (type == SERVICE_XIAOZHI_EVT_ACTIVATION_CODE && text != NULL) {
        strncpy(s_pending_activation_code, text, XZ_ACTIVATION_CODE_LEN - 1);
        s_pending_activation_code[XZ_ACTIVATION_CODE_LEN - 1] = '\0';
    }
    /* AI 屏不在前台时事件无消费者：直接丢弃防积压。激活码已暂存 */
    if (!s_ai_ui_active) {
        if (type == SERVICE_XIAOZHI_EVT_STATE) {
            ESP_LOGD(TAG, "[ui-sync] STATE 事件丢弃（AI 屏不在前台）state=%d", (int)evt.state);
        }
        return;
    }
    if (xQueueSend(s_evt_queue, &evt, 0) != pdTRUE) {
        ESP_LOGW(TAG, "event queue full, drop type=%d", type);
    } else if (type == SERVICE_XIAOZHI_EVT_STATE) {
        ESP_LOGD(TAG, "[ui-sync] STATE 事件已投递 App（状态栏同步）state=%d", (int)evt.state);
    }
}

static const char *const s_state_names[] = {
    /* 顺序必须与 service_xiaozhi_state_t 枚举一致：READY=2, CONNECTING=3
     * （曾写反导致日志状态名互换，误导排查） */
    "IDLE", "ACTIVATING", "READY", "CONNECTING", "LISTENING", "SPEAKING",
};

/* 合法转换边表（bit = 目标状态）：对齐上游 DeviceStateMachine 的校验语义，
 * 防御未来改动引入的非法跳转（如打断/重连路径漏掉清理仪式）。非法仅告警。 */
#define XZ_ST_BIT(st) (1u << (st))
static const uint8_t s_state_edges[] = {
    [SERVICE_XIAOZHI_STATE_IDLE] =
        XZ_ST_BIT(SERVICE_XIAOZHI_STATE_ACTIVATING) | XZ_ST_BIT(SERVICE_XIAOZHI_STATE_READY),
    [SERVICE_XIAOZHI_STATE_ACTIVATING] =
        XZ_ST_BIT(SERVICE_XIAOZHI_STATE_READY) | XZ_ST_BIT(SERVICE_XIAOZHI_STATE_IDLE),
    [SERVICE_XIAOZHI_STATE_CONNECTING] =
        XZ_ST_BIT(SERVICE_XIAOZHI_STATE_LISTENING) | XZ_ST_BIT(SERVICE_XIAOZHI_STATE_READY) |
        XZ_ST_BIT(SERVICE_XIAOZHI_STATE_IDLE),
    [SERVICE_XIAOZHI_STATE_READY] =
        /* READY→SPEAKING：manual 模式松手后停在 READY（通道保留等应答），
         * 服务器回答的 tts_start 必须能进 SPEAKING，否则下行音频因
         * state!=SPEAKING 全部被丢，TTS 一帧都播不出（真机实测无声） */
        XZ_ST_BIT(SERVICE_XIAOZHI_STATE_CONNECTING) | XZ_ST_BIT(SERVICE_XIAOZHI_STATE_LISTENING) |
        XZ_ST_BIT(SERVICE_XIAOZHI_STATE_SPEAKING) | XZ_ST_BIT(SERVICE_XIAOZHI_STATE_IDLE),
    [SERVICE_XIAOZHI_STATE_LISTENING] =
        XZ_ST_BIT(SERVICE_XIAOZHI_STATE_SPEAKING) | XZ_ST_BIT(SERVICE_XIAOZHI_STATE_READY) |
        XZ_ST_BIT(SERVICE_XIAOZHI_STATE_IDLE) | XZ_ST_BIT(SERVICE_XIAOZHI_STATE_CONNECTING),
    [SERVICE_XIAOZHI_STATE_SPEAKING] =
        XZ_ST_BIT(SERVICE_XIAOZHI_STATE_LISTENING) | XZ_ST_BIT(SERVICE_XIAOZHI_STATE_READY) |
        XZ_ST_BIT(SERVICE_XIAOZHI_STATE_IDLE) | XZ_ST_BIT(SERVICE_XIAOZHI_STATE_CONNECTING),
};

/**
 * @brief 切换状态并投递 STATE 事件（仅 xz_task 调用；非法转换告警并拒绝）
 * @param reason 迁移原因，随日志打印，便于真机回放状态机轨迹
 */
static void xz_set_state(service_xiaozhi_state_t state, const char *reason)
{
    if (s_state == state) {
        return;
    }
    unsigned to = (unsigned)state;
    if (to >= sizeof(s_state_edges) / sizeof(s_state_edges[0]) ||
        (s_state_edges[s_state] & XZ_ST_BIT(state)) == 0) {
        ESP_LOGW(TAG, "illegal state transition %s -> %s (%s), rejected",
                 s_state_names[s_state], (to < 6) ? s_state_names[to] : "?",
                 reason ? reason : "?");
        return;
    }
    s_state = state;
    const char *name = (state < sizeof(s_state_names) / sizeof(s_state_names[0]))
                           ? s_state_names[state] : "?";
    ESP_LOGI(TAG, "state -> %s (%s)", name, reason ? reason : "?");
    xz_post_event(SERVICE_XIAOZHI_EVT_STATE, NULL);

    /* 对话进行中（CONNECTING/LISTENING/SPEAKING）保持屏幕常亮 */
    bool in_conversation = (state == SERVICE_XIAOZHI_STATE_CONNECTING ||
                            state == SERVICE_XIAOZHI_STATE_LISTENING ||
                            state == SERVICE_XIAOZHI_STATE_SPEAKING);
    service_power_hold_screen_on(in_conversation);
}

/**
 * @brief 投递内部消息（ws 回调上下文，队列满丢弃）
 */
static void xz_post_msg(xz_msg_type_t type, const char *text)
{
    if (s_msg_queue == NULL) {
        return;
    }
    xz_msg_t msg = {
        .type = type,
        .text = {0},
    };
    if (text != NULL) {
        strncpy(msg.text, text, sizeof(msg.text) - 1);
    }
    if (xQueueSend(s_msg_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "msg queue full, drop type=%d", type);
    }
}

/**
 * @brief 投递 MCP 消息（ws 回调上下文，堆字符串所有权移交队列；入队失败则释放）
 */
static void xz_post_mcp(char *heap_text)
{
    if (s_msg_queue == NULL || heap_text == NULL) {
        if (heap_text != NULL) {
            cJSON_free(heap_text);
        }
        return;
    }
    xz_msg_t msg = {
        .type = XZ_MSG_MCP,
        .heap_text = heap_text,
        .text = {0},
    };
    if (xQueueSend(s_msg_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "msg queue full, drop mcp");
        cJSON_free(heap_text);
    }
}

/**
 * @brief 清空内部消息队列并释放滞留的 MCP 堆消息
 */
static void xz_msg_queue_clear(void)
{
    if (s_msg_queue == NULL) {
        return;
    }
    xz_msg_t msg;
    while (xQueueReceive(s_msg_queue, &msg, 0) == pdTRUE) {
        if (msg.heap_text != NULL) {
            cJSON_free(msg.heap_text);
        }
    }
}

/**
 * @brief 可中断睡眠（100ms 分片，shutdown 时提前返回）
 *
 * 每片结束兜底消费命令队列：本函数只服务于激活等阻塞路径，其间主循环
 * 不消费命令，不兜底则 SHUTDOWN 在长退避（最长 160s）期间被整体吞掉。
 */
static void xz_sleep_ms(uint32_t ms)
{
    while (ms > 0 && !s_shutdown) {
        uint32_t slice = (ms > 100) ? 100 : ms;
        vTaskDelay(pdMS_TO_TICKS(slice));
        ms -= slice;
        xz_drain_cmds();
    }
}

/* ---------------------------------------------------------------------------
 * WebSocket 回调（service_ws_process 调用方 / task_comm 任务上下文）
 * ------------------------------------------------------------------------- */

static void xz_on_ws_connected(void *ctx)
{
    (void)ctx;
    xz_post_msg(XZ_MSG_CONNECTED, NULL);
}

static void xz_on_ws_disconnected(void *ctx)
{
    (void)ctx;
    xz_post_msg(XZ_MSG_DISCONNECTED, NULL);
}

static void xz_on_ws_audio(const uint8_t *data, int len, void *ctx)
{
    (void)ctx;
    s_last_rx_tick = xTaskGetTickCount();
    /* Why: 对齐上游，仅 SPEAKING 消费下行音频；READY/聆听态的迟到包直接丢弃，
     * 防突兀出声。s_state 由 xz_task 写、本回调（task_comm）读，
     * volatile 枚举的轻度竞态可接受（误判代价仅一帧 60ms 音频）。 */
    if (s_state != SERVICE_XIAOZHI_STATE_SPEAKING) {
        return;
    }
    /* BinaryProtocol3 防御性剥离：握手虽已声明 Protocol-Version: 1（裸 opus），
     * 若服务器仍按 v3 封包下发（[type=0:1][reserved:1][payload_size:2 BE]
     * [payload]），剥掉 4 字节头再解码。误伤概率≈0：v1 裸包首字节为 opus
     * TOC（24k 流不会取 0x00），且尺寸须精确吻合 */
    if (len >= 4 && data[0] == 0 && data[1] == 0 &&
        ((uint32_t)((data[2] << 8) | data[3])) == (uint32_t)(len - 4)) {
        data += 4;
        len -= 4;
    }
    /* aux 反压：剩余空间不足一个最长 TTS 包时等播放端排水（10ms 切片轮询），
     * 积压倒灌回 WS 事件队列——编码包 ~200B/60ms，比解码后 PCM 省约 50 倍
     * 内存，是全链路最廉价的抖动缓冲层；队列再满则 ws 任务入队阻塞，经 TCP
     * 接收窗口反压服务器发送速率。
     * Trap: 等待结束发现已离开 SPEAKING（abort/打断）必须丢帧返回，否则
     * 刚清空的 aux 里会残留下一句开头的旧语音片段 */
    uint32_t bp_wait = 0;
    while (s_state == SERVICE_XIAOZHI_STATE_SPEAKING &&
           service_audio_aux_free_frames() < XZ_AUX_BACKPRESSURE_FREE_FRAMES &&
           bp_wait < XZ_AUX_BACKPRESSURE_MAX_MS) {
        vTaskDelay(pdMS_TO_TICKS(10));
        bp_wait += 10;
    }
    if (s_state != SERVICE_XIAOZHI_STATE_SPEAKING) {
        return;
    }
    /* 解码 + 重采样 + aux 写入，全部在本上下文完成；aux 满则截断不阻塞 */
    esp_err_t ret = service_voice_decode_packet(data, (uint32_t)len);
    s_dl_audio_frames++;
    s_dl_audio_bytes += (uint32_t)len;
    /* 首帧确认下行链路打通，此后每 50 帧（约 3s）打点，避免刷屏 */
    if (s_dl_audio_frames == 1 || (s_dl_audio_frames % 50) == 0) {
        ESP_LOGD(TAG, "下行音频: 第%lu帧 本帧%d字节 累计%lu字节 decode=%s",
                 (unsigned long)s_dl_audio_frames, len,
                 (unsigned long)s_dl_audio_bytes, (ret == ESP_OK) ? "ok" : "fail");
    }
}

static void xz_on_ws_text(const char *json, int len, void *ctx)
{
    (void)len;
    (void)ctx;
    s_last_rx_tick = xTaskGetTickCount();

    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        ESP_LOGW(TAG, "invalid json: %.64s", json);
        return;
    }

    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (!cJSON_IsString(type)) {
        cJSON_Delete(root);
        return;
    }
    /* 临时调试：下行消息类型，确认服务器是否有响应 */
    ESP_LOGD(TAG, "[dbg] rx msg type=%s", type->valuestring);

    if (strcmp(type->valuestring, "hello") == 0) {
        cJSON *session_id = cJSON_GetObjectItem(root, "session_id");
        if (cJSON_IsString(session_id)) {
            strncpy(s_session_id, session_id->valuestring, sizeof(s_session_id) - 1);
            s_session_id[sizeof(s_session_id) - 1] = '\0';
        }
        cJSON *audio_params = cJSON_GetObjectItem(root, "audio_params");
        if (cJSON_IsObject(audio_params)) {
            cJSON *sample_rate = cJSON_GetObjectItem(audio_params, "sample_rate");
            cJSON *frame_duration = cJSON_GetObjectItem(audio_params, "frame_duration");
            if (cJSON_IsNumber(sample_rate)) {
                s_server_sample_rate = (uint32_t)sample_rate->valueint;
            }
            if (cJSON_IsNumber(frame_duration)) {
                s_server_frame_duration = (uint32_t)frame_duration->valueint;
            }
        }
        xz_post_msg(XZ_MSG_HELLO, NULL);
    } else if (strcmp(type->valuestring, "tts") == 0) {
        cJSON *state = cJSON_GetObjectItem(root, "state");
        if (cJSON_IsString(state)) {
            if (strcmp(state->valuestring, "start") == 0) {
                xz_post_msg(XZ_MSG_TTS_START, NULL);
            } else if (strcmp(state->valuestring, "stop") == 0) {
                xz_post_msg(XZ_MSG_TTS_STOP, NULL);
            } else if (strcmp(state->valuestring, "sentence_start") == 0) {
                cJSON *text = cJSON_GetObjectItem(root, "text");
                if (cJSON_IsString(text)) {
                    xz_post_msg(XZ_MSG_TTS_SENTENCE, text->valuestring);
                }
            }
        }
    } else if (strcmp(type->valuestring, "stt") == 0) {
        cJSON *text = cJSON_GetObjectItem(root, "text");
        if (cJSON_IsString(text)) {
            xz_post_msg(XZ_MSG_STT, text->valuestring);
        }
    } else if (strcmp(type->valuestring, "system") == 0) {
        cJSON *command = cJSON_GetObjectItem(root, "command");
        if (cJSON_IsString(command) && strcmp(command->valuestring, "reboot") == 0) {
            xz_post_msg(XZ_MSG_SYSTEM_REBOOT, NULL);
        }
    } else if (strcmp(type->valuestring, "alert") == 0) {
        cJSON *message = cJSON_GetObjectItem(root, "message");
        if (cJSON_IsString(message)) {
            xz_post_msg(XZ_MSG_ALERT, message->valuestring);
        }
    } else if (strcmp(type->valuestring, "mcp") == 0) {
        /* payload 可能远超 msg.text 容量，堆拷贝整个 JSON 交 xz_task 处理 */
        cJSON *payload = cJSON_GetObjectItem(root, "payload");
        if (cJSON_IsObject(payload)) {
            char *str = cJSON_PrintUnformatted(payload);
            if (str != NULL) {
                xz_post_mcp(str);
            }
        }
    } else {
        ESP_LOGD(TAG, "unhandled json type: %s", type->valuestring);
    }

    cJSON_Delete(root);
}

/* ---------------------------------------------------------------------------
 * 协议发送（仅 xz_task 上下文）
 * ------------------------------------------------------------------------- */

/**
 * @brief 发送客户端 hello（version 1，Opus 16kHz 单声道 60ms）
 */
static void xz_send_hello(void)
{
    static const char *hello =
        "{\"type\":\"hello\",\"version\":1,\"features\":{\"mcp\":true},"
        "\"transport\":\"websocket\",\"audio_params\":{\"format\":\"opus\","
        "\"sample_rate\":16000,\"channels\":1,\"frame_duration\":60}}";
    service_ws_send_text(hello, (int)strlen(hello));
}

/**
 * @brief 发送 abort 打断下行播报
 */
static void xz_send_abort(void)
{
    char buf[160];
    int n = snprintf(buf, sizeof(buf),
                     "{\"session_id\":\"%s\",\"type\":\"abort\",\"reason\":\"wake_word_detected\"}",
                     s_session_id);
    if (n > 0) {
        service_ws_send_text(buf, n);
    }
}

/**
 * @brief 发送 listen start
 * @param[in] mode "manual"（按钮，靠 listen stop 结束）或 "auto"（唤醒，服务器 VAD 断句）
 */
static void xz_send_listen_start(const char *mode)
{
    char buf[192];
    int n = snprintf(buf, sizeof(buf),
                     "{\"session_id\":\"%s\",\"type\":\"listen\",\"state\":\"start\",\"mode\":\"%s\"}",
                     s_session_id, (mode != NULL) ? mode : "manual");
    if (n > 0) {
        /* 临时调试：listen start 发送结果，确认服务器是否收到上行起始消息 */
        int ret = service_ws_send_text(buf, n);
        ESP_LOGD(TAG, "[dbg] listen start sent ret=%d mode=%s", ret,
                 (mode != NULL) ? mode : "manual");
    }
}

/**
 * @brief 发送 listen stop
 */
static void xz_send_listen_stop(void)
{
    char buf[160];
    int n = snprintf(buf, sizeof(buf),
                     "{\"session_id\":\"%s\",\"type\":\"listen\",\"state\":\"stop\"}",
                     s_session_id);
    if (n > 0) {
        service_ws_send_text(buf, n);
    }
}

/* ---------------------------------------------------------------------------
 * mic 泵（仅 xz_task 上下文）
 * ------------------------------------------------------------------------- */

/**
 * @brief 打开唤醒词监听（仅启动 AFE 与使能唤醒，mic 由语音前端按需管理）
 *
 * @return true AFE 就绪（auto 模式）；false 不可用（manual 按住兼底）
 */
static bool xz_voice_open_wake(void)
{
    if (service_voice_wake_open_deferred() != ESP_OK) {
        ESP_LOGW(TAG, "AFE 不可用，退化为 manual 按住说话");
        xz_post_event(SERVICE_XIAOZHI_EVT_ERROR, "语音唤醒不可用，本次仅支持按住说话");
        return false;
    }
    service_voice_enable_wake(true);
    ESP_LOGI(TAG, "AFE 就绪，采用 auto 连续对话（唤醒词可用，全双工）");
    return true;
}

/**
 * @brief 启动语音上行（开始编码并发送 Opus 包）
 *
 * 实际由 service_voice 在 audio 任务中完成采集/编码，xz_task 只需消费其事件队列。
 */
static esp_err_t xz_voice_start_listen(void)
{
    return service_voice_start_listen();
}

/**
 * @brief 停止语音上行
 */
static void xz_voice_stop_listen(void)
{
    service_voice_stop_listen();
}

/**
 * @brief 从语音前端消费事件并转换为小智状态/上行包/VAD 事件
 *
 * 仅 xz_task 上下文调用；PACKET 事件中的 data 由本函数释放。
 */
static void xz_voice_poll(void)
{
    service_voice_event_t evt;
    while (service_voice_poll_event(&evt)) {
        switch (evt.type) {
        case SERVICE_VOICE_EVT_WAKE:
            if (!s_use_auto) {
                break;
            }
            if (s_suspended) {
                /* 系统暂停期（FTP 独占页）：唤醒事件直接丢弃，检出本就关闭 */
                ESP_LOGI(TAG, "唤醒丢弃: suspended");
                break;
            }
            ESP_LOGI(TAG, "唤醒词命中: %s", evt.text ? evt.text : "?");
            /* 命中沿已被语音前端消费（s_wake_pending 去重，一次命中只进一
             * 个事件），检出保持开启：门控丢弃时（未激活/全局唤醒关且非 AI
             * 屏前台）下一个命中仍可受理；真正发起对话时由 begin_conversation
             * 统一关检出（对齐官方 HandleWakeWordResult 清 kWakeWordEnabled）。
             * 仅登记触发：是否开对话由主循环按激活状态/全局唤醒开关裁决；
             * 不在此置 wake_launch 拉屏（默认配置下会把用户误拽进 AI 屏） */
            s_trigger_conv = true;
            /* 唤醒词命中即亮屏并重置熄屏计时，与点击亮屏体验一致 */
            service_power_wake_screen();
            break;

        case SERVICE_VOICE_EVT_VAD:
            if (s_state == SERVICE_XIAOZHI_STATE_LISTENING) {
                bool is_speech = evt.is_speech;
                if (is_speech != s_voice_last_vad) {
                    s_voice_last_vad = is_speech;
                    xz_post_event(SERVICE_XIAOZHI_EVT_VAD, is_speech ? "speech" : "silence");
                }
            }
            break;

        case SERVICE_VOICE_EVT_PACKET:
            if (s_state == SERVICE_XIAOZHI_STATE_LISTENING && s_channel_open && evt.data != NULL) {
                int sent = service_ws_send_bin(evt.data, (int)evt.len);
                /* 临时调试：上行发包计数与结果，定位“语音上报无响应” */
                static uint32_t s_up_dbg_cnt = 0;
                static uint32_t s_up_dbg_fail = 0;
                if (sent < 0) {
                    s_up_dbg_fail++;
                }
                s_up_dbg_cnt++;
                if ((s_up_dbg_cnt % 20) == 1 || sent < 0) {
                    ESP_LOGD(TAG, "[dbg] uplink pkt #%lu ret=%d len=%u fail_total=%lu",
                             (unsigned long)s_up_dbg_cnt, sent, (unsigned)evt.len,
                             (unsigned long)s_up_dbg_fail);
                }
            } else if (evt.data != NULL) {
                /* 临时调试：状态/通道未就绪导致的丢包，5s 限流 */
                static TickType_t s_drop_dbg_tick = 0;
                if ((xTaskGetTickCount() - s_drop_dbg_tick) > pdMS_TO_TICKS(5000)) {
                    s_drop_dbg_tick = xTaskGetTickCount();
                    ESP_LOGW(TAG, "[dbg] uplink pkt dropped: state=%d ch_open=%d",
                             (int)s_state, (int)s_channel_open);
                }
            }
            if (evt.data != NULL) {
                heap_caps_free((void *)evt.data);
            }
            break;

        case SERVICE_VOICE_EVT_ERROR:
            ESP_LOGW(TAG, "voice frontend error: %s", evt.text ? evt.text : "?");
            xz_post_event(SERVICE_XIAOZHI_EVT_ERROR, evt.text ? evt.text : "语音前端错误");
            break;

        default:
            break;
        }
    }
}

/* ---------------------------------------------------------------------------
 * 命令处理（仅 xz_task 上下文）
 * ------------------------------------------------------------------------- */

static void xz_handle_cmd(xz_cmd_t cmd)
{
    switch (cmd) {
    case XZ_CMD_START:
        /* 会话中重复 START 只登记，会话收尾后入口循环接续；空闲等 START 由入口直接消费 */
        s_start_pending = true;
        break;

    case XZ_CMD_TALK_START:
        /* 播报中先打断，再按当前模式发起对话（auto=服务器VAD / manual=按住）；
         * 激活/连接中忽略（通道尚未就绪） */
        if (s_state == SERVICE_XIAOZHI_STATE_ACTIVATING ||
            s_state == SERVICE_XIAOZHI_STATE_CONNECTING) {
            ESP_LOGW(TAG, "talk_start in state %d, ignored", s_state);
            return;
        }
        if (s_state == SERVICE_XIAOZHI_STATE_SPEAKING) {
            xz_send_abort();
            /* 对齐唤醒打断路径：按钮打断同样清 aux，防被掐播报残留到下一句 */
            service_audio_aux_clear();
        }
        xz_begin_conversation(s_use_auto ? "auto" : "manual");
        break;

    case XZ_CMD_TALK_STOP:
        /* auto：断句交给服务器 VAD，忽略松手；manual：发 listen stop 停泵等应答
         * （通道保留，等 TTS 后由 handle_msg 关通道回 STANDBY） */
        if (s_use_auto) {
            break;
        }
        if (s_state != SERVICE_XIAOZHI_STATE_LISTENING) {
            break;
        }
        xz_send_listen_stop();
        xz_voice_stop_listen();
        xz_set_state(SERVICE_XIAOZHI_STATE_READY, "manual talk_stop");
        s_last_activity = xTaskGetTickCount();
        break;

    case XZ_CMD_SHUTDOWN:
        s_shutdown = true;
        break;

    default:
        break;
    }
}

/**
 * @brief 非阻塞掏空命令队列（阻塞路径兜底消费，语义与主循环一致）
 */
static void xz_drain_cmds(void)
{
    xz_cmd_t cmd;
    while (xQueueReceive(s_cmd_queue, &cmd, 0) == pdTRUE) {
        xz_handle_cmd(cmd);
    }
}

/* ---------------------------------------------------------------------------
 * 激活流程（仅 xz_task 上下文，阻塞 HTTP 可接受）
 * ------------------------------------------------------------------------- */

/**
 * @brief 保存 websocket 配置到 NVS 并立即落盘
 */
static void xz_save_channel_config(const xiaozhi_ota_result_t *result)
{
    service_nvs_set_xz_ws_url(result->ws_url);
    service_nvs_set_xz_ws_token(result->ws_token);
    service_nvs_commit();
    ESP_LOGI(TAG, "channel config saved: %s", result->ws_url);
}

/**
 * @brief OTA 激活流：检查 → （必要时）激活码轮询 → 取得通道配置
 *
 * @return ESP_OK 已取得 websocket url/token
 */
static esp_err_t xz_do_activation(void)
{
    static xiaozhi_ota_result_t result;
    uint32_t backoff = SERVICE_XIAOZHI_OTA_RETRY_BASE_MS;
    int fail_count = 0;

    while (!s_shutdown) {
        esp_err_t ret = xiaozhi_ota_check(&result);
        if (ret == ESP_OK) {
            fail_count = 0;
            backoff = SERVICE_XIAOZHI_OTA_RETRY_BASE_MS;

            /* Trap: 如果服务器返回 activation code，即使已有 WebSocket 配置也要优先处理
             * 这意味着新设备首次绑定或管理员触发了重新绑定 */
            if (result.has_activation) {
                ESP_LOGI(TAG, "received activation code: %s", result.activation_code);
                ESP_LOGI(TAG, "sending ACTIVATION_CODE event to app");
                xz_post_event(SERVICE_XIAOZHI_EVT_ACTIVATION_CODE, result.activation_code);

                /* 激活码有效期以服务器下发为准，缺省 10 分钟；超时回外层
                 * 重新 check 换新码，避免死等一个已过期码 */
                uint32_t act_timeout = result.activation_timeout_ms;
                if (act_timeout == 0) {
                    act_timeout = SERVICE_XIAOZHI_ACTIVATE_DEFAULT_TIMEOUT_MS;
                }
                TickType_t act_start = xTaskGetTickCount();
                esp_err_t act_ret = ESP_ERR_TIMEOUT;
                while (!s_shutdown && act_ret != ESP_OK) {
                    if ((xTaskGetTickCount() - act_start) > pdMS_TO_TICKS(act_timeout)) {
                        ESP_LOGW(TAG, "激活码已过期（%lu ms），重新获取", (unsigned long)act_timeout);
                        xz_post_event(SERVICE_XIAOZHI_EVT_ERROR, "激活码已过期，正在重新获取");
                        break;
                    }
                    act_ret = xiaozhi_ota_activate();
                    if (act_ret == ESP_ERR_TIMEOUT) {
                        ESP_LOGD(TAG, "polling activation status...");
                        /* xz_sleep_ms 分片内已兜底消费命令（SHUTDOWN 可逃逸） */
                        xz_sleep_ms(SERVICE_XIAOZHI_ACTIVATE_POLL_MS);
                    } else if (act_ret != ESP_OK) {
                        xz_sleep_ms(SERVICE_XIAOZHI_OTA_RETRY_BASE_MS);
                    }
                }
                if (s_shutdown) {
                    break;
                }
                /* 激活成功（或换码）后重新 check 获取通道配置 */
                continue;
            }

            if (result.has_websocket) {
                /* 没有 activation code，直接保存配置进入会话 */
                xz_save_channel_config(&result);
                s_config_refreshed = true;   /* 新配置即最新配置，免开机刷新 */
                return ESP_OK;
            }

            ESP_LOGW(TAG, "ota response has neither websocket nor activation");
        }

        fail_count++;
        if (fail_count >= SERVICE_XIAOZHI_OTA_MAX_RETRY) {
            break;
        }
        ESP_LOGW(TAG, "ota check retry %d/%d in %lu ms",
                 fail_count, SERVICE_XIAOZHI_OTA_MAX_RETRY, (unsigned long)backoff);
        xz_sleep_ms(backoff);
        backoff *= 2;
    }

    if (!s_shutdown) {
        ESP_LOGW(TAG, "激活失败，请检查网络");
    }
    return ESP_FAIL;
}

/* ---------------------------------------------------------------------------
 * 会话管理
 * ------------------------------------------------------------------------- */

/**
 * @brief 等待服务器 hello（处理期间到达的连接/断开消息）
 *
 * @return true 收到 hello
 */
static bool xz_wait_hello(void)
{
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(SERVICE_XIAOZHI_HELLO_TIMEOUT_MS);

    while (!s_shutdown) {
        TickType_t now = xTaskGetTickCount();
        if (now >= deadline) {
            ESP_LOGW(TAG, "wait server hello timeout");
            return false;
        }

        /* 分片等待（≤200ms），片间消费命令：否则握手期 SHUTDOWN 最长被吞
         * 整个 hello 超时 */
        xz_drain_cmds();
        if (s_shutdown) {
            break;
        }
        TickType_t wait = deadline - now;
        if (wait > pdMS_TO_TICKS(200)) {
            wait = pdMS_TO_TICKS(200);
        }

        xz_msg_t msg;
        if (xQueueReceive(s_msg_queue, &msg, wait) != pdTRUE) {
            continue;
        }

        switch (msg.type) {
        case XZ_MSG_CONNECTED:
            xz_send_hello();
            break;
        case XZ_MSG_HELLO:
            ESP_LOGI(TAG, "server hello: session=%s sr=%lu fd=%lu",
                     s_session_id, (unsigned long)s_server_sample_rate,
                     (unsigned long)s_server_frame_duration);
            return true;
        case XZ_MSG_DISCONNECTED:
            ESP_LOGW(TAG, "disconnected before hello");
            return false;
        case XZ_MSG_MCP:
            /* 会话未就绪不处理 MCP，直接释放防泄漏 */
            if (msg.heap_text != NULL) {
                cJSON_free(msg.heap_text);
            }
            break;
        case XZ_MSG_WS_ERROR:
            xz_handle_ws_error(msg.aux);
            break;
        default:
            break;
        }
    }
    return false;
}

/**
 * @brief 处理 ws 握手错误：401/403 视为凭据失效，清 token 走重新绑定
 *
 * 服务器轮换 token/迁移 URL 后旧凭据握手必败，原实现仅打日志形成
 * 「唤醒→失败→退避→再失败」的永久静默循环。清 token 后 is_activated
 * 变 false，后续触发走激活流重新绑定。
 */
static void xz_handle_ws_error(int http_status)
{
    if (http_status != 401 && http_status != 403) {
        return;
    }
    ESP_LOGW(TAG, "ws 握手被拒（%d），凭据已失效，清除 token 待重新绑定", http_status);
    service_nvs_set_xz_ws_token("");
    if (s_channel_open) {
        xz_close_channel();
        xz_enter_standby();
    }
    xz_post_event(SERVICE_XIAOZHI_EVT_ERROR, "AI 绑定已失效，请打开 AI 助手重新激活");
}

/**
 * @brief 按需打开语音通道：连 ws → 等 hello → 开解码器
 *
 * Contract: 成功后 s_channel_open=true；失败已自行清理（调用方负责回 STANDBY）。
 * @return ESP_OK 通道就绪
 */
static esp_err_t xz_open_channel(void)
{
    if (s_channel_open) {
        return ESP_OK;
    }
    /* 失败退避：通道打开失败后 3s 内拒绝重试，避免唤醒/按键连续触发
     * 形成重连风暴（日志刷屏 + TLS 反复吃内存） */
    if (s_last_channel_fail != 0 &&
        (xTaskGetTickCount() - s_last_channel_fail) < pdMS_TO_TICKS(3000)) {
        return ESP_FAIL;
    }
    char ws_url[SERVICE_NVS_XZ_WS_URL_MAX_LEN];
    char ws_token[SERVICE_NVS_XZ_WS_TOKEN_MAX_LEN];
    service_nvs_get_xz_ws_url(ws_url, sizeof(ws_url));
    service_nvs_get_xz_ws_token(ws_token, sizeof(ws_token));
    if (ws_url[0] == '\0' || ws_token[0] == '\0') {
        ESP_LOGE(TAG, "通道配置缺失");
        s_last_channel_fail = xTaskGetTickCount();
        return ESP_FAIL;
    }

    xz_set_state(SERVICE_XIAOZHI_STATE_CONNECTING, "open_channel");
    
    // 构造额外的 Header (增大缓冲区防止截断警告)
    /* Trap: Protocol-Version 头必须声明（上游同款）。漏发时服务器按默认
     * v3 封包格式下发下行音频（BinaryProtocol3 4 字节头），封包头会被误当
     * opus TOC 解出 10ms 噪声——真机"每句话滋一下"即此 */
    char headers[384];
    snprintf(headers, sizeof(headers),
             "Authorization: Bearer %s\r\n"
             "Protocol-Version: 1\r\n"
             "Device-Id: %s\r\n"
             "Client-Id: %s\r\n",
             ws_token, s_device_id, s_client_id);
    
    /* 临时诊断：TLS 走硬件 AES 需 DMA-capable 内部 RAM，AFE(~98KB)+任务栈
     * 占满内部 RAM 后碎片化会致 esp-aes 分配 DMA 描述符失败、握手起不来。
     * 连接前打印水位，若 dma largest 过小即坐实内存根因 */
    ESP_LOGD(TAG, "[dbg] connect mem: dma free=%u largest=%u | int free=%u largest=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));

    if (service_ws_connect(ws_url, headers, xz_on_ws_event_handler, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "语音通道连接失败");
        s_last_channel_fail = xTaskGetTickCount();
        return ESP_FAIL;
    }
    if (!xz_wait_hello()) {
        service_ws_disconnect();
        ESP_LOGE(TAG, "语音通道握手超时");
        s_last_channel_fail = xTaskGetTickCount();
        return ESP_FAIL;
    }
    if (service_voice_decoder_open(s_server_sample_rate) != ESP_OK) {
        ESP_LOGE(TAG, "解码器打开失败");
        service_ws_disconnect();
        s_last_channel_fail = xTaskGetTickCount();
        return ESP_FAIL;
    }
    s_last_rx_tick = xTaskGetTickCount();
    s_last_channel_fail = 0;
    s_channel_open = true;
    return ESP_OK;
}

/**
 * @brief 关闭语音通道（不动 mic 泵与 AFE）：解码器 + 消息栓 + ws，幂等
 */
static void xz_close_channel(void)
{
    if (!s_channel_open) {
        return;
    }
    s_channel_open = false;
    s_pending_listen = false;
    s_session_id[0] = '\0';
    service_voice_decoder_close();
    xz_msg_queue_clear();
    service_ws_disconnect();
}

/**
 * @brief 回到待触发态（STANDBY）：通道已关，state=READY
 *
 * auto（AFE 可用）：保持语音前端监听唤醒词（不上传）；
 * manual：停止语音上行，扬声器可用，静待按钮。
 */
static void xz_enter_standby(void)
{
    /* 必须先停上行：s_listening=true 会遮蔽语音前端的唤醒检测分支，
     * 不停则首次对话结束后唤醒词永久失效且编码永久空转 */
    xz_voice_stop_listen();
    if (s_use_auto) {
        /* 唤醒使能按激活状态门控：未激活设备的唤醒必然被门控丢弃，留着
         * 检测只会对残响/噪声反复误触发刷屏；激活成功后的 enter_standby
         * 会重新打开 */
        if (!s_suspended) {
            service_voice_enable_wake(service_xiaozhi_is_activated());
        }
    }
    xz_set_state(SERVICE_XIAOZHI_STATE_READY, "enter_standby");
}

/**
 * @brief 唤醒被门控丢弃：仅消费触发位，检出保持常驻
 *
 * 检出沿语义下无锁存重触发问题（一次命中只产生一个事件），丢弃后无需
 * 关检出——留着它，状态变化（回 AI 屏/开全局唤醒）后的下一句唤醒词
 * 才能立即受理。
 */
static void xz_wake_drop(void)
{
    s_trigger_conv = false;
    ESP_LOGI(TAG, "唤醒丢弃: ai_ui=%d anywhere=%d activated=%d",
             (int)s_ai_ui_active, (int)s_wake_anywhere,
             (int)service_xiaozhi_is_activated());
}

/**
 * @brief 发起一轮对话：必要时开通道 → 启动语音上行 → listen start → LISTENING
 * @param[in] mode "auto"（服务器 VAD 断句/续听）或 "manual"（按住说话）
 */
static void xz_begin_conversation(const char *mode)
{
    /* 新对话发起即取消未完成的退下请求（用户改主意继续聊） */
    s_standby_pending = false;
    if (!s_channel_open) {
        if (xz_open_channel() != ESP_OK) {
            xz_enter_standby();
            return;
        }
    }
    if (xz_voice_start_listen() != ESP_OK) {
        xz_post_event(SERVICE_XIAOZHI_EVT_ERROR, "麦克风打开失败");
        /* 上行起不来：收掉刚开的通道并回待触发，避免通道悬空且
         * 唤醒停在关闭态再也无法触发 */
        xz_close_channel();
        xz_enter_standby();
        return;
    }
    xz_send_listen_start(mode);
    xz_set_state(SERVICE_XIAOZHI_STATE_LISTENING, "begin_conversation");
    /* LISTENING 期关唤醒检测：一是省 WakeNet 推理，AFE fetch 负载减半，
     * 上行才能跟上实时（半速时音频缺半采样，服务器 ASR 乱码/无响应）；
     * 二是避免监听期产生的命中积压在结果队列，到 TTS 开播时被当新命中
     * 打断播报。聆听中无需唤醒（本就已在听），回 STANDBY 时统一恢复 */
    if (s_use_auto) {
        service_voice_enable_wake(false);
    }
    s_last_activity = xTaskGetTickCount();
}

/**
 * @brief 处理一条服务器消息（主循环调用；heap_text 所有权在此释放）
 */
static void xz_handle_msg(xz_msg_t *msg)
{
    switch (msg->type) {
    case XZ_MSG_DISCONNECTED: {
        /* 通道断开：关通道回 STANDBY，不自动重连（消除重连风暴）。
         * 仅当断连打断进行中的交互（LISTENING/CONNECTING）才判为意外并
         * 告知 App；告别播报后的正常收尾（如用户说“退下”，LLM 只说
         * 告别语不调 self.standby 就由服务器收线，此时 state 仍是
         * SPEAKING）或已回待触发后的回送不报错误（真机：退下后屏上
         * 弹“语音通道已断开”错误泡）。 */
        bool unexpected = (s_state == SERVICE_XIAOZHI_STATE_LISTENING ||
                           s_state == SERVICE_XIAOZHI_STATE_CONNECTING);
        ESP_LOGW(TAG, "语音通道断开，回到待触发 state=%d%s", (int)s_state,
                 unexpected ? "（意外）" : "（正常收尾）");
        xz_close_channel();
        xz_enter_standby();
        if (unexpected) {
            xz_post_event(SERVICE_XIAOZHI_EVT_ERROR, "语音通道已断开");
        }
        break;
    }
    case XZ_MSG_TTS_START:
        /* 新一句开播清场：异常收尾（断链无 tts_stop）时 aux 可躺 ≤2s 旧音频，
         * 开播即播旧渣；正常路径尾音已随 end_of_stream 排完，此处为幂等兜底 */
        service_audio_aux_clear();
        /* 播报期停止编码上行（省 CPU/带宽）并关闭唤醒检测。
         * Why 不做唤醒打断：当前 AEC 为 SR_LOW_COST+NLP_OFF（保上行实时
         * 的代价），扬声器回声压不干净，回声会被 VAD 当成人声、WakeNet
         * 误命中并在宽限到期后精确打断播报（真机：每轮 TTS 开播 1.2s
         * 被自触发打断+aux_clear，连预充都攒不够、全程无声）。官方也仅
         * 在回声消除有效时才开播报期唤醒。打断能力暂由按钮承担，待 AEC
         * 升档后再恢复。 */
        xz_voice_stop_listen();
        s_voice_last_vad = 0;
        s_dl_audio_frames = 0;
        s_dl_audio_bytes = 0;
        if (s_use_auto) {
            service_voice_enable_wake(false);
        }
        ESP_LOGI(TAG, "[对话] TTS 播报开始（播报期关唤醒防回声自触发）");
        xz_set_state(SERVICE_XIAOZHI_STATE_SPEAKING, "tts_start");
        s_last_activity = xTaskGetTickCount();
        break;
    case XZ_MSG_TTS_STOP:
        /* 流已结束（音频帧同序先于本消息落 aux）：立即解除预充门放尾音，
         * 否则不足 400ms 门限的尾音永久卡住，跨轮残留成下一句开头的插播；
         * auto 续听也能在几百 ms 内等到 aux 排空，不再吃 2500ms 兜底 */
        service_audio_aux_end_of_stream();
        /* 同复位：下一句首帧重采样不再从本句尾采样插值（帧间断续 click） */
        service_voice_decoder_reset_phase();
        ESP_LOGI(TAG, "[对话] TTS 播报结束（累计下行 %lu 帧 %lu 字节）",
                 (unsigned long)s_dl_audio_frames, (unsigned long)s_dl_audio_bytes);
        if (s_standby_pending) {
            /* 退下请求在途：告别播报已完，关通道回待触发，不再续听 */
            s_standby_pending = false;
            ESP_LOGI(TAG, "[对话] 告别播报结束，退出对话回待机");
            xz_close_channel();
            xz_enter_standby();
            break;
        }
        if (s_state == SERVICE_XIAOZHI_STATE_SPEAKING) {
            s_last_activity = xTaskGetTickCount();
            if (s_use_auto && !s_shutdown) {
                /* auto 连续对话：等 aux 播报排空再续听（主循环轮询），防开 mic
                 * 关扬声器把回答尾巴掐掉、或残音被当用户语音上传 */
                s_pending_listen = true;
                s_pending_listen_at = xTaskGetTickCount();
            } else {
                /* manual：一轮问答完毕，关通道回待触发等按钮 */
                xz_close_channel();
                xz_enter_standby();
            }
        }
        break;
    case XZ_MSG_TTS_SENTENCE:
        ESP_LOGI(TAG, "[对话] 小智: %s", msg->text);
        xz_post_event(SERVICE_XIAOZHI_EVT_TTS_SENTENCE, msg->text);
        s_last_activity = xTaskGetTickCount();
        break;
    case XZ_MSG_STT:
        ESP_LOGI(TAG, "[对话] 用户: %s", msg->text);
        xz_post_event(SERVICE_XIAOZHI_EVT_STT, msg->text);
        s_last_activity = xTaskGetTickCount();
        break;
    case XZ_MSG_ALERT:
        xz_post_event(SERVICE_XIAOZHI_EVT_ERROR, msg->text);
        break;
    case XZ_MSG_WS_ERROR:
        xz_handle_ws_error(msg->aux);
        break;
    case XZ_MSG_SYSTEM_REBOOT:
        ESP_LOGW(TAG, "server requested reboot");
        esp_restart();
        break;
    case XZ_MSG_MCP:
        if (msg->heap_text != NULL) {
            cJSON *payload = cJSON_Parse(msg->heap_text);
            if (payload != NULL) {
                xiaozhi_mcp_handle(payload, s_session_id);
                cJSON_Delete(payload);
            } else {
                ESP_LOGW(TAG, "mcp payload parse failed");
            }
            cJSON_free(msg->heap_text);
        }
        break;
    default:
        break;
    }
}

/**
 * @brief 会话主循环：待触发 ↔ 对话统一驱动，直至 SHUTDOWN
 *
 * 单循环覆盖 STANDBY（通道关：auto 嗂 AFE 盯唤醒 / manual 静待按钮）与对话态；
 * 触发经命令（按钮）或唤醒（s_trigger_conv）开通道，结束/空闲超时关通道回
 * STANDBY，不进重连退避循环。
 */
static void xz_app_loop(void)
{
    s_last_activity = xTaskGetTickCount();

    while (!s_shutdown) {
        xz_cmd_t cmd;
        while (xQueueReceive(s_cmd_queue, &cmd, 0) == pdTRUE) {
            xz_handle_cmd(cmd);
        }
        if (s_shutdown) {
            break;
        }

        /* 唤醒命中：SPEAKING 打断播报并续听；STANDBY 开对话；
         * 激活/连接中保留触发位待就绪重试（不重入阻塞流程、不刷屏） */
        if (s_trigger_conv) {
            if (s_suspended) {
                /* 系统暂停期：唤醒裁决直接丢弃，不开对话不拉屏 */
                s_trigger_conv = false;
            } else if (!service_xiaozhi_is_activated()) {
                /* 设备未激活（未配网/未注册）：直接放弃唤醒指令，
                 * 不补激活、不拉屏、不刷通知；激活走 AI App 手动流程 */
                xz_wake_drop();
            } else if (!s_ai_ui_active && !s_wake_anywhere) {
                /* 全局唤醒默认关：仅 AI 屏前台时响应，其他界面静默丢弃；
                 * 检出保持常驻（检出沿语义下无周期误检），状态变化后即可受理 */
                xz_wake_drop();
            } else if (s_state == SERVICE_XIAOZHI_STATE_CONNECTING ||
                       s_state == SERVICE_XIAOZHI_STATE_ACTIVATING) {
                /* 过渡态：保留触发，后续轮次重试 */
            } else if (s_state == SERVICE_XIAOZHI_STATE_LISTENING) {
                s_trigger_conv = false;   /* 已在聆听，忽略重复触发 */
            } else {
                s_trigger_conv = false;
                /* 唤醒不再拉起 AI 屏：AI 屏前台时正常显示文字交互；
                 * 其他界面（wake_anywhere 开）直接后台对话，供 MCP
                 * 音量/亮度等系统参数控制，界面保持用户当前画面 */
                if (s_state == SERVICE_XIAOZHI_STATE_SPEAKING) {
                    /* 打断：发 abort、清 aux 尾巴、立即续听 */
                    xz_send_abort();
                    service_audio_aux_clear();
                    ESP_LOGI(TAG, "[对话] 唤醒打断播报，续听");
                    xz_begin_conversation("auto");
                } else if (service_wifi_is_connected()) {
                    /* 通道失败退避期内：消费触发不重试，避免唤醒风暴刷屏 */
                    if (s_last_channel_fail != 0 &&
                        (xTaskGetTickCount() - s_last_channel_fail) < pdMS_TO_TICKS(3000)) {
                        /* 静默吞掉本次触发 */
                    } else {
                        ESP_LOGI(TAG, "唤醒命中，发起对话 (ai_ui=%d anywhere=%d)",
                                 (int)s_ai_ui_active, (int)s_wake_anywhere);
                        xz_begin_conversation("auto");
                    }
                } else {
                    /* 离线唤醒：5s 限流通知，防反复提醒 */
                    TickType_t now2 = xTaskGetTickCount();
                    if (s_last_offline_notify == 0 ||
                        (now2 - s_last_offline_notify) > pdMS_TO_TICKS(5000)) {
                        s_last_offline_notify = now2;
                        xz_post_event(SERVICE_XIAOZHI_EVT_ERROR, "唤醒成功但网络未连接，无法开启对话");
                    }
                }
            }
        }

        /* WiFi 上线沿：已激活设备在网络恢复时刷新配置（token/url 等） */
        bool wifi_now = service_wifi_is_connected();
        if (wifi_now && !s_wifi_was_up && service_xiaozhi_is_activated()) {
            xz_config_refresh_once();
        }
        s_wifi_was_up = wifi_now;

        /* 激活请求处理：AI App 进入时触发，仅在 AI UI active 且未激活时执行 */
        if (s_activation_requested && !service_xiaozhi_is_activated()) {
            if (wifi_now) {
                s_activation_requested = false;
                ESP_LOGI(TAG, "processing activation request...");
                xz_set_state(SERVICE_XIAOZHI_STATE_ACTIVATING, "AI App entry");
                if (xz_do_activation() == ESP_OK) {
                    xz_set_state(SERVICE_XIAOZHI_STATE_READY, "activated");
                } else {
                    xz_set_state(SERVICE_XIAOZHI_STATE_READY, "activation failed");
                }
                xz_enter_standby();
            } else {
                /* 未联网时等待，下一轮再检查 */
                ESP_LOGD(TAG, "activation requested but wifi not ready, waiting...");
            }
        }

        xz_msg_t msg;
        if (xQueueReceive(s_msg_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            xz_handle_msg(&msg);
        }

        /* 消费语音前端事件：唤醒/VAD/上行包/错误，全部转交给小智状态机 */
        xz_voice_poll();

        TickType_t now = xTaskGetTickCount();

        /* auto 续听延迟：等 aux 排空（TTS 尾巴播完）再开对话；2500ms 兜底防
         * 缓冲积压（如对端停顿）导致永远等不到排空。兜底须大于 aux 环深
         * 度 2s，否则长句尾巴会被当用户语音上传（旧值 1.5s 过小） */
        if (s_pending_listen) {
            if (service_audio_aux_is_idle() ||
                (now - s_pending_listen_at) > pdMS_TO_TICKS(2500)) {
                s_pending_listen = false;
                if (!service_audio_aux_is_idle()) {
                    /* 2500ms 兜底仍带渣：清场防跨轮残留（正常路径
                     * end_of_stream 已在几百 ms 内排空，走不到这里） */
                    service_audio_aux_clear();
                }
                if (s_state == SERVICE_XIAOZHI_STATE_SPEAKING) {
                    xz_begin_conversation("auto");
                }
            }
        }

        /* 退下请求兜底：3s 内无告别播报到来（LLM 未调 TTS 或消息丢失），
         * 直接收尾，不卡死在等待 */
        if (s_standby_pending &&
            s_state != SERVICE_XIAOZHI_STATE_SPEAKING &&
            (int32_t)(now - s_standby_pending_at) > (int32_t)pdMS_TO_TICKS(3000)) {
            s_standby_pending = false;
            ESP_LOGW(TAG, "[对话] 退下请求 3s 无告别播报，兜底收尾");
            if (s_channel_open) {
                xz_close_channel();
                xz_enter_standby();
            }
        }

        /* 空闲超时：通道开且非播报/连接，持续无服务器活动 → 关通道回 STANDBY，
         * 避免无限「卡 Listen」（auto 续听与 manual 等应答均适用）。
         * Trap: now 必须在本块前重新取并用有符号比较——上方续听/发起对话
         * 刚把 s_last_activity 刷到≥旧 now 的 tick，无符号减法下溢成巨值，
         * 会在对话刚恢复的同一拍误关通道（真机：TTS 后续听立即被断）。 */
        now = xTaskGetTickCount();
        if (s_channel_open &&
            s_state != SERVICE_XIAOZHI_STATE_SPEAKING &&
            s_state != SERVICE_XIAOZHI_STATE_CONNECTING &&
            (int32_t)(now - s_last_activity) > (int32_t)pdMS_TO_TICKS(SERVICE_XIAOZHI_LISTEN_IDLE_MS)) {
            ESP_LOGI(TAG, "空闲超时 %d ms，关闭通道回待触发", SERVICE_XIAOZHI_LISTEN_IDLE_MS);
            xz_close_channel();
            xz_enter_standby();
        }

        /* CONNECTING 卡滞保护：长时间停留于连接中且通道未建成
         * （如异步连接失败事件丢失），回待触发态恢复唤醒链路 */
        if (s_state == SERVICE_XIAOZHI_STATE_CONNECTING && !s_channel_open &&
            (int32_t)(now - s_last_activity) > (int32_t)pdMS_TO_TICKS(15000)) {
            ESP_LOGW(TAG, "CONNECTING 卡滞超时，回待触发态");
            xz_close_channel();
            xz_enter_standby();
        }

        /* 入站看门狗：通道开但 120s 无任何入站帧（含 SPEAKING）→ 判半开死亡，
         * 关通道回待触发（对端死而无 FIN 时 DISCONNECTED 永不触发，应用层兜底） */
        if (s_channel_open &&
            (int32_t)(now - s_last_rx_tick) > (int32_t)pdMS_TO_TICKS(120000)) {
            ESP_LOGW(TAG, "入站看门狗超时（120s 无数据），关闭通道回待触发");
            xz_close_channel();
            xz_enter_standby();
            xz_post_event(SERVICE_XIAOZHI_EVT_ERROR, "语音通道超时已断开");
        }
    }
}

/* ---------------------------------------------------------------------------
 * 任务主流程（task_ai 常驻任务，开机创建一次）
 * ------------------------------------------------------------------------- */

/**
 * @brief 已激活设备的 ws 配置刷新（每开机一次）：OTA check → 变化才落盘
 *
 * 服务器轮换 token/迁移 URL 后靠此自愈；服务器返回 activation 段视为要求
 * 重新绑定，转完整激活流。检查失败非致命，沿用本地配置。
 */
static void xz_config_refresh_once(void)
{
    if (s_config_refreshed) {
        return;
    }
    s_config_refreshed = true;

    static xiaozhi_ota_result_t refresh;
    if (xiaozhi_ota_check(&refresh) != ESP_OK) {
        ESP_LOGW(TAG, "config refresh failed, keep local config");
        return;
    }
    if (refresh.has_activation) {
        /* 服务器要求重新绑定（管理员重置等）：走完整激活流 */
        ESP_LOGW(TAG, "server requests re-activation");
        xz_set_state(SERVICE_XIAOZHI_STATE_IDLE, "server re-activation");
        xz_set_state(SERVICE_XIAOZHI_STATE_ACTIVATING, "server re-activation");
        xz_do_activation();
        xz_enter_standby();
        return;
    }
    if (refresh.has_websocket) {
        char cur_url[SERVICE_NVS_XZ_WS_URL_MAX_LEN];
        char cur_token[SERVICE_NVS_XZ_WS_TOKEN_MAX_LEN];
        service_nvs_get_xz_ws_url(cur_url, sizeof(cur_url));
        service_nvs_get_xz_ws_token(cur_token, sizeof(cur_token));
        /* 仅在配置变化时落盘，避免无效 NVS 写入磨损 */
        if (strcmp(cur_url, refresh.ws_url) != 0 ||
            strcmp(cur_token, refresh.ws_token) != 0) {
            ESP_LOGI(TAG, "ws config changed, refresh saved");
            xz_save_channel_config(&refresh);
        }
    }
}

/**
 * @brief 单次会话：激活 → AFE 常驻 → 待触发主循环，直至 SHUTDOWN
 */
static void xz_run_session(void)
{
    ESP_LOGI(TAG, "session start");

    s_channel_open = false;

    if (xiaozhi_ota_ensure_client_id(s_client_id, sizeof(s_client_id)) != ESP_OK) {
        xz_post_event(SERVICE_XIAOZHI_EVT_ERROR, "Client-Id 初始化失败");
        return;
    }
    xiaozhi_ota_get_device_id(s_device_id, sizeof(s_device_id));

    /* 离线常驻：Wi-Fi 未连接时不再直接退出，仍开 AFE 监听唤醒词
     * （WakeNet 为本地推理，不依赖网络）；激活流程在用户进入 AI App 时触发 */
    bool wifi_up = service_wifi_is_connected();
    if (!wifi_up) {
        ESP_LOGW(TAG, "wifi not connected yet: offline standby (wake word available, activate on AI App entry)");
    }

    /* AFE 常驻：ws 连接前（内部 RAM 峰值点）打开唤醒前端并整段常驻，
     * 据此定会话模式：auto（唤醒 + 服务器 VAD 连续对话）/ manual（按住说话）。 */
#if SERVICE_XIAOZHI_WAKE_WORD_ENABLE
    s_use_auto = xz_voice_open_wake();
#else
    s_use_auto = false;
    ESP_LOGI(TAG, "唤醒词编译关闭，采用 manual 按住说话");
#endif

    /* 进入待触发态：auto 使能唤醒监听；manual 静待按钮。通道按需开启，不预连。 */
    xz_enter_standby();

    /* 已激活设备开机刷新一次 ws 配置；开机时 WiFi 未就绪的由 app_loop
     * 上线沿补做（s_wifi_was_up 初始化为会话起点状态，避免重复触发） */
    s_wifi_was_up = wifi_up;
    if (wifi_up && service_xiaozhi_is_activated()) {
        xz_config_refresh_once();
    }

    xz_app_loop();

    /* 会话收尾：关通道 + 停止语音上行；AFE 与唤醒词前端保持常驻，
     * 避免会话重启时因 WiFi 后内存不足无法重新打开 AFE。 */
    xz_close_channel();
    xz_voice_stop_listen();
    xz_msg_queue_clear();
}

void service_xiaozhi_process(void)
{
    /* 常驻会话：首次进入自启，不再阻塞等 AI App 的 START */
    if (!s_boot_autostart) {
        s_boot_autostart = true;
        s_start_pending = true;
    }

    /* 无会话期：AFE 已在 boot 阶段常驻，继续轮询语音前端事件。
     * 识别到唤醒词即自动启动会话并请求拉起 AI App UI，避免「唤醒词
     * 只能在会话内消费」的先有鸡还是先有蛋问题。
     * 未激活设备关闭检出（唤醒必然被丢弃，留着只会反复误触发刷屏）。 */
    if (!s_suspended) {
        service_voice_enable_wake(service_xiaozhi_is_activated());
    }
    while (!s_start_pending && !s_shutdown) {
        service_voice_event_t evt;
        if (service_voice_poll_event(&evt)) {
            switch (evt.type) {
            case SERVICE_VOICE_EVT_WAKE:
                if (s_suspended) {
                    /* 系统暂停期（FTP 独占页）：不启动会话 */
                    ESP_LOGI(TAG, "待机唤醒丢弃: suspended");
                    break;
                }
                ESP_LOGI(TAG, "待机唤醒命中: %s", evt.text ? evt.text : "?");
                s_start_pending = true;
                s_trigger_conv = true;
                /* 唤醒词命中即亮屏并重置熄屏计时，与点击亮屏体验一致 */
                service_power_wake_screen();
                /* 不再置 s_wake_launch：唤醒不拉起 AI 屏，是否对话由
                 * 会话循环按激活状态与全局唤醒开关裁决 */
                break;
            case SERVICE_VOICE_EVT_PACKET:
                if (evt.data != NULL) {
                    heap_caps_free((void *)evt.data);
                }
                break;
            case SERVICE_VOICE_EVT_ERROR:
                ESP_LOGW(TAG, "standby voice error: %s", evt.text ? evt.text : "?");
                break;
            default:
                break;
            }
        }

        xz_cmd_t cmd;
        if (xQueueReceive(s_cmd_queue, &cmd, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (cmd == XZ_CMD_START) {
                s_start_pending = true;
            } else if (cmd == XZ_CMD_SHUTDOWN) {
                s_shutdown = true;
            }
            /* TALK 等命令在待机期无意义，直接丢弃 */
        }
    }

    if (s_shutdown) {
        s_shutdown = false;
        s_trigger_conv = false;
        return;
    }

    s_start_pending = false;
    s_shutdown = false;

    xz_run_session();

    /* 会话收尾：保持 AFE 唤醒前端常驻，仅关闭 WebSocket 通道与上行编码，
     * 确保下次唤醒或 START 能立即恢复，无需在 WiFi 后冒险重开 AFE。 */
    xz_close_channel();
    xz_voice_stop_listen();
    xz_msg_queue_clear();
    s_trigger_conv = false;
    xz_set_state(SERVICE_XIAOZHI_STATE_IDLE, "session end");
    ESP_LOGI(TAG, "session end");
}

/* ---------------------------------------------------------------------------
 * 对外 API
 * ------------------------------------------------------------------------- */

esp_err_t service_xiaozhi_init(void)
{
    if (s_cmd_queue != NULL) {
        return ESP_OK;
    }

    /* 从小智参数组（NVS）恢复全局唤醒开关（默认关） */
    s_wake_anywhere = service_nvs_get_xz_wake_anywhere();

    /* 队列放 PSRAM：初始化发生在 WiFi 启动后内部 RAM 枯竭期，三条队列仅
     * 任务上下文使用（ws 回调/task_ai/App poll，无 ISR），落 PSRAM 安全且
     * 省内部 RAM（xz_msg_t 含 256B 文本×16 深度，约 4KB+） */
    s_cmd_queue = xQueueCreateWithCaps(SERVICE_XIAOZHI_CMD_QUEUE_LEN, sizeof(xz_cmd_t), MALLOC_CAP_SPIRAM);
    s_msg_queue = xQueueCreateWithCaps(SERVICE_XIAOZHI_MSG_QUEUE_LEN, sizeof(xz_msg_t), MALLOC_CAP_SPIRAM);
    s_evt_queue = xQueueCreateWithCaps(SERVICE_XIAOZHI_EVT_QUEUE_LEN, sizeof(service_xiaozhi_event_t), MALLOC_CAP_SPIRAM);
    if (s_cmd_queue == NULL || s_msg_queue == NULL || s_evt_queue == NULL) {
        ESP_LOGE(TAG, "queue create failed");
        return ESP_ERR_NO_MEM;
    }

    /* 调试：检查当前 NVS 中的凭据状态 */
    char uuid[SERVICE_NVS_XZ_UUID_MAX_LEN] = {0};
    char ws_url[SERVICE_NVS_XZ_WS_URL_MAX_LEN] = {0};
    char ws_token[SERVICE_NVS_XZ_WS_TOKEN_MAX_LEN] = {0};
    
    service_nvs_get_xz_uuid(uuid, sizeof(uuid));
    service_nvs_get_xz_ws_url(ws_url, sizeof(ws_url));
    service_nvs_get_xz_ws_token(ws_token, sizeof(ws_token));
    
    ESP_LOGI(TAG, "xiaozhi init: uuid=%s ws_url=%s token=%s", 
             uuid[0] ? uuid : "(empty)",
             ws_url[0] ? ws_url : "(empty)",
             ws_token[0] ? "(set)" : "(empty)");

    ESP_LOGI(TAG, "initialized");
    return ESP_OK;
}

esp_err_t service_xiaozhi_start(void)
{
    if (s_cmd_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* 任务开机常驻（task_ai 创建），START 经命令队列传递；会话中重复 START 由任务侧登记接续 */
    xz_cmd_t cmd = XZ_CMD_START;
    if (xQueueSend(s_cmd_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "start cmd send failed");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t service_xiaozhi_stop(void)
{
    if (s_cmd_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* 异步结束会话：任务在会话循环各检查点收敛后自行收尾并回 IDLE，调用方不等完成 */
    xz_cmd_t cmd = XZ_CMD_SHUTDOWN;
    if (xQueueSend(s_cmd_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "shutdown cmd send failed");
    }
    return ESP_OK;
}

void service_xiaozhi_reset_device_identity(void)
{
    xiaozhi_ota_regenerate_device_id();
}

void service_xiaozhi_request_activation(void)
{
    /* 清除当前激活状态：清空 token、重新生成设备身份、清除暂存激活码 */
    service_nvs_set_xz_ws_token("");
    service_nvs_commit();
    xiaozhi_ota_regenerate_device_id();
    s_pending_activation_code[0] = '\0';
    /* 设置激活请求标志：主循环中会检查并执行激活流程 */
    s_activation_requested = true;
    ESP_LOGI(TAG, "activation requested: identity reset, waiting for AI UI active");
}

void service_xiaozhi_talk_start(void)
{
    if (s_cmd_queue == NULL) {
        return;
    }
    xz_cmd_t cmd = XZ_CMD_TALK_START;
    xQueueSend(s_cmd_queue, &cmd, 0);
}

void service_xiaozhi_talk_stop(void)
{
    if (s_cmd_queue == NULL) {
        return;
    }
    xz_cmd_t cmd = XZ_CMD_TALK_STOP;
    xQueueSend(s_cmd_queue, &cmd, 0);
}

void service_xiaozhi_request_standby(void)
{
    /* 只置标志：收尾在 xz_task 主循环执行（等告别 TTS 的 tts stop 或 3s 兜底），
     * 本 API 可从任意上下文调用 */
    s_standby_pending = true;
    s_standby_pending_at = xTaskGetTickCount();
    ESP_LOGI(TAG, "[对话] 收到退下请求，等告别播报结束后收尾");
}

bool service_xiaozhi_is_activated(void)
{
    char token[SERVICE_NVS_XZ_WS_TOKEN_MAX_LEN];
    if (service_nvs_get_xz_ws_token(token, sizeof(token)) != ESP_OK) {
        return false;
    }
    return token[0] != '\0';
}

void service_xiaozhi_set_wake_anywhere(bool enable)
{
    s_wake_anywhere = enable;
    ESP_LOGI(TAG, "wake anywhere: %d", (int)enable);
    /* 开关打开：立即确保检出已武装（此前可能因未激活/会话收尾被关）。
     * 但对话中（LISTENING/SPEAKING）不重开：播报期重开会被扬声器回声
     * 自触发打断 TTS，对话期唤醒本就由生命周期统一管理（回待机才开）。 */
    if (enable) {
        if (!s_suspended &&
            s_state != SERVICE_XIAOZHI_STATE_LISTENING &&
            s_state != SERVICE_XIAOZHI_STATE_SPEAKING) {
            service_voice_enable_wake(service_xiaozhi_is_activated());
        }
    }
}

bool service_xiaozhi_get_wake_anywhere(void)
{
    return s_wake_anywhere;
}

void service_xiaozhi_set_ai_ui_active(bool active)
{
    s_ai_ui_active = active;
    /* AI 屏回前台：清掉无消费者期间的陈旧事件积压，防旧 STATE 回放污染 UI；
     * 并确保唤醒检出已武装（检出沿语义下丢弃不再关检出，此为兜底）。
     * 但对话中不重开：播报期重开会被回声自触发打断 TTS，唤醒由生命周期统一管理。 */
    if (active) {
        if (s_evt_queue != NULL) {
            xQueueReset(s_evt_queue);
        }
        /* 重放暂存的激活码：AI App 重新进入时绑定气泡依赖此事件 */
        if (s_pending_activation_code[0] != '\0') {
            ESP_LOGI(TAG, "replaying pending activation code: %s", s_pending_activation_code);
            xz_post_event(SERVICE_XIAOZHI_EVT_ACTIVATION_CODE, s_pending_activation_code);
        }
        if (!s_suspended &&
            s_state != SERVICE_XIAOZHI_STATE_LISTENING &&
            s_state != SERVICE_XIAOZHI_STATE_SPEAKING) {
            service_voice_enable_wake(service_xiaozhi_is_activated());
        }
        /* 未激活时请求激活：AI App 进入时触发绑定流程 */
        if (!service_xiaozhi_is_activated()) {
            ESP_LOGI(TAG, "AI UI active, requesting activation...");
            s_activation_requested = true;
        }
    } else {
        /* AI 屏退出：清除激活请求，防止后台继续获取激活码 */
        s_activation_requested = false;
    }
}

void service_xiaozhi_set_suspended(bool suspended)
{
    if (suspended) {
        s_suspended = true;
        ESP_LOGI(TAG, "xiaozhi suspended (state=%d)", (int)s_state);
        /* 会话活跃则异步收尾回 IDLE（SHUTDOWN 经命令队列，任务侧收敛后退出），
         * ws 随之断开释放 socket/TLS；唤醒检出立即关闭 */
        if (s_state != SERVICE_XIAOZHI_STATE_IDLE) {
            service_xiaozhi_stop();
        }
        service_voice_enable_wake(false);
    } else {
        s_suspended = false;
        ESP_LOGI(TAG, "xiaozhi resumed");
        service_voice_enable_wake(service_xiaozhi_is_activated());
    }
}

service_xiaozhi_state_t service_xiaozhi_get_state(void)
{
    return s_state;
}

bool service_xiaozhi_is_auto_mode(void)
{
    return s_use_auto;
}

int service_xiaozhi_poll_event(service_xiaozhi_event_t *out)
{
    if (out == NULL || s_evt_queue == NULL) {
        return 0;
    }
    return (xQueueReceive(s_evt_queue, out, 0) == pdTRUE) ? 1 : 0;
}
