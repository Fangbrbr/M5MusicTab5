/**
 * @file service_voice_wake.c
 * @brief 服务语音唤醒词前端：esp-sr AFE + WakeNet 封装实现
 *
 * 架构参考 xiaozhi-esp32 AfeAudioEngine：
 *  - feed 由调用方（xz_task 泵）驱动，写入 AFE 输入 ringbuffer；
 *  - fetch 在独立任务中阻塞等待（fetch_with_delay），AFE 内部 DSP 管线
 *    （NS/VAD/WakeNet 推理）在 fetch 上下文执行，栈需求较大；
 *  - 结果经内部队列传回调用方（service_voice_wake_poll 非阻塞取）。
 */

#include "service_voice_wake.h"

#include "service_voice.h"

#include "esp_afe_sr_models.h"
#include "esp_vadn_models.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "string.h"

static const char *TAG = "xiaozhi_wake";

/** @brief 模型分区标签（与 partitions.csv 一致） */
#define XIAOZHI_WAKE_MODEL_PARTITION "model"

/** @brief WakeNet9 模型索引到唤醒词文本映射 */
static const char *s_wake_words[] = {
    NULL,                       /* 0: 无唤醒 */
    "Hi，喵喵",                   /* 1: Hi Miaomiao */
};

/** @brief fetch 任务栈大小：推理调用链极深，实测需 ≥12KB */
#define WAKE_FETCH_TASK_STACK  16384
/* Trap: 必须钉 Core 1。Core 0 上 task_gui（优先级 10）的 45~56ms flush 连续
 * 抢占，优先级 5 的本任务只拿到半 CPU，AFE 处理降到半速，上行音频被
 * 拉长一倍、服务器 VAD/ASR 全废。Core 1 仅 task_audio 占用且在 codec
 * 写阻塞间隙有大量富余；PSRAM 栈安全（本任务无 flash 写路径）。 */
#define WAKE_FETCH_TASK_PRIO   5
#define WAKE_FETCH_TASK_CORE   1
/** @brief fetch 阻塞超时（ms）：保证 close 时及时退出 */
#define WAKE_FETCH_TIMEOUT_MS  100

/* AFE 打开前的内部 RAM 门槛：esp-dl 的 P4 SIMD 核仅能寻址内部 RAM 张量，
 * MORE_INTERNAL 在内部不足时会溢出到 PSRAM 触发崩溃，故门槛须保证 AFE
 * 整体放得下（实测 ~70KB + 会话余量）；低于门槛退化 manual 按住说话 */
#define WAKE_MIN_INTERNAL_FREE    (96 * 1024)
#define WAKE_MIN_INTERNAL_LARGEST (44 * 1024)

/* 模型列表必须存活到 AFE 销毁之后：create_from_config 持有其 mmap 数据指针 */
static srmodel_list_t *s_models = NULL;
static const esp_afe_sr_iface_t *s_afe_iface = NULL;
static esp_afe_sr_data_t *s_afe_data = NULL;
static int32_t s_feed_chunk = 0;

/* fetch 任务与结果队列 */
static TaskHandle_t s_fetch_task = NULL;
static volatile bool s_fetch_stop = false;
static QueueHandle_t s_result_queue = NULL;

/** @brief 最近检出的唤醒词文本（AFE 可用时） */
static const char *s_last_wake_word = NULL;

/* wakenet 启停指令：0=无 1=disable 2=enable。实际执行在 fetch 任务上下文，
 * 避免与 fetch_with_delay 跨线程并发操作 AFE 实例。 */
static volatile int s_wakenet_cmd = 0;

/* AFE 环形缓冲重置请求：mic 关闭期间旧音频（含唤醒词）滞留在 AFE 内部
 * 环中，重新使能后被重新喂给管线导致 WakeNet/VAD 对旧语音重复检出
 * （重开后 20~30ms 内立即“命中”）。在 fetch 任务上下文执行。 */
static volatile bool s_reset_buf_req = false;

/** @brief 队列中传递的 fetch 结果快照 */
typedef struct {
    int16_t data[SERVICE_VOICE_AFE_MAX_CHUNK];
    int samples;
    int wakeup_state;        /*!< esp-sr 检出沿状态（wakenet_state_t） */
    int wakenet_model_index;
    int vad_state;
} wake_result_msg_t;

/* ------------------------------------------------------------------------ */

static void wake_fetch_task_entry(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "fetch task started");

    /* 临时遥测：fetch 产出速率、耗时拆分与队列挤旧丢块数，3s 窗口；
     * 实时时应 ≈94 块/3s 且 drops=0。wait 为 fetch_with_delay 阻塞等数据
     * 时长：wait 大而 total 小=上游喂养不足；total 长期 >16ms/块=DSP 超重 */
    uint32_t dbg_chunks = 0;
    uint32_t dbg_max_us = 0;
    uint32_t dbg_max_wait_us = 0;
    uint32_t dbg_drops = 0;
    int64_t dbg_window_us = esp_timer_get_time();

    while (!s_fetch_stop) {
        /* wakenet 启停指令在本任务上下文执行（AFE 非 fetch/feed 的 API 不允许
         * 与 fetch 并发），阻塞等待期间到达的指令最多晚一个超时周期生效 */
        if (s_wakenet_cmd != 0) {
            if (s_wakenet_cmd == 1) {
                s_afe_iface->disable_wakenet(s_afe_data);
            } else {
                s_afe_iface->enable_wakenet(s_afe_data);
            }
            s_wakenet_cmd = 0;
        }

        /* 重置 AFE 环：丢弃滞留旧音频，并清空结果队列中的陈旧检出 */
        if (s_reset_buf_req) {
            s_reset_buf_req = false;
            if (s_afe_iface != NULL && s_afe_data != NULL) {
                s_afe_iface->reset_buffer(s_afe_data);
            }
            if (s_result_queue != NULL) {
                wake_result_msg_t discard;
                while (xQueueReceive(s_result_queue, &discard, 0) == pdTRUE) {
                }
            }
        }

        int64_t t_fetch0 = esp_timer_get_time();
        afe_fetch_result_t *res = s_afe_iface->fetch_with_delay(
            s_afe_data, pdMS_TO_TICKS(WAKE_FETCH_TIMEOUT_MS));
        int64_t dt_wait = esp_timer_get_time() - t_fetch0;
        if (dt_wait > (int64_t)dbg_max_wait_us) {
            dbg_max_wait_us = (uint32_t)dt_wait;
        }
        if (res != NULL && res->ret_value != ESP_FAIL) {
            dbg_chunks++;
        }
        if ((esp_timer_get_time() - dbg_window_us) >= 3000000) {
            ESP_LOGD(TAG, "[dbg] fetch/3s chunks=%lu max_wait=%lu us max_total=%lu us drops=%lu",
                     (unsigned long)dbg_chunks, (unsigned long)dbg_max_wait_us,
                     (unsigned long)dbg_max_us, (unsigned long)dbg_drops);
            dbg_chunks = 0;
            dbg_max_us = 0;
            dbg_max_wait_us = 0;
            dbg_drops = 0;
            dbg_window_us = esp_timer_get_time();
        }
        if (s_fetch_stop) {
            break;
        }
        if (res == NULL || res->ret_value == ESP_FAIL) {
            continue;
        }
        /* 拷贝到消息并投递队列；满则挤掉最旧一帧（检出结果保新不保旧，
         * 避免 task_ai 繁忙时新检出被静默丢弃） */
        wake_result_msg_t msg;
        int samples = res->data_size / (int)sizeof(int16_t);
        if (samples > SERVICE_VOICE_AFE_MAX_CHUNK) {
            samples = SERVICE_VOICE_AFE_MAX_CHUNK;
        }
        memcpy(msg.data, res->data, (size_t)samples * sizeof(int16_t));
        msg.samples = samples;
        msg.wakeup_state = (int)res->wakeup_state;
        msg.wakenet_model_index = res->wakenet_model_index;
        msg.vad_state = res->vad_state;
        if (xQueueSend(s_result_queue, &msg, 0) != pdTRUE) {
            wake_result_msg_t dropped;
            xQueueReceive(s_result_queue, &dropped, 0);
            xQueueSend(s_result_queue, &msg, 0);
            dbg_drops++;
        }
        {
            int64_t dt_total = esp_timer_get_time() - t_fetch0;
            if (dt_total > (int64_t)dbg_max_us) {
                dbg_max_us = (uint32_t)dt_total;
            }
        }
    }

    ESP_LOGI(TAG, "fetch task exiting");
    s_fetch_task = NULL;
    vTaskDelete(NULL);
}

/* ------------------------------------------------------------------------ */

esp_err_t service_voice_wake_open(void)
{
    if (s_afe_data != NULL) {
        return ESP_OK;
    }

    /* AFE 必须「整体」落内部 RAM：MORE_INTERNAL 只是优先，不足会溢出到
     * PSRAM 触发 PIE 总线错误。另查最大连续块，防碎片化导致张量无着落。 */
    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    ESP_LOGI(TAG, "AFE 打开前内部 RAM: 空闲 %u 字节, 最大连续块 %u 字节",
             (unsigned)internal_free, (unsigned)largest_block);
    if (internal_free < WAKE_MIN_INTERNAL_FREE || largest_block < WAKE_MIN_INTERNAL_LARGEST) {
        ESP_LOGW(TAG, "internal RAM low (free %u/%u, block %u/%u), skip wake frontend",
                 (unsigned)internal_free, (unsigned)WAKE_MIN_INTERNAL_FREE,
                 (unsigned)largest_block, (unsigned)WAKE_MIN_INTERNAL_LARGEST);
        return ESP_FAIL;
    }

    s_models = esp_srmodel_init(XIAOZHI_WAKE_MODEL_PARTITION);
    if (s_models == NULL || s_models->num <= 0) {
        ESP_LOGE(TAG, "no sr model found in partition \"%s\"", XIAOZHI_WAKE_MODEL_PARTITION);
        goto fail;
    }

    char *wn_name = esp_srmodel_filter(s_models, ESP_WN_PREFIX, NULL);
    if (wn_name == NULL) {
        ESP_LOGE(TAG, "no wakenet model in partition");
        goto fail;
    }

    /* MR 双通道：M=mic, R=扬声器参考（AEC 全双工） */
    afe_config_t *cfg = afe_config_init("MR", s_models, AFE_TYPE_VC, AFE_MODE_HIGH_PERF);
    if (cfg == NULL) {
        ESP_LOGE(TAG, "afe config init failed");
        goto fail;
    }
    cfg->aec_init = true;           /* 开启 AEC：消除扬声器回采，实现全双工 */
    /* AEC 模式选 SR_LOW_COST（官方 esp_aec 文档推荐档）：fetch 与 task_audio
     * 分核 Core 1，HIGH_PERF 档单次推理 ~35ms/16ms 块，DSP 超重拖到半速，
     * 上行缺半采样致服务器 ASR 乱码/无响应（真机 LISTENING 关 WakeNet 后
     * 仍半速即此因）。NLP 用默认 AGGR：VERYAGGR 会把静音期的真实人声也
     * 抑掉，导致唤醒词成功率归零（真机整段日志零命中）；播报期回声误
     * 唤醒改由 TTS 开播清环+宽限拦截，不再依赖最强 NLP */
    cfg->aec_mode = AEC_MODE_SR_LOW_COST;
    cfg->aec_nlp_level = AEC_NLP_LEVEL_AGGR;
    cfg->ns_init = false;           /* AEC 工作时关闭 NS，避免双重降噪 */
    cfg->vad_init = true;
    cfg->wakenet_init = true;
    cfg->wakenet_model_name = wn_name;
    /* Trap: MORE_PSRAM 在本平台必崩（esp-dl P4 SIMD 核仅能寻址内部 RAM），
     * 必须用 MORE_INTERNAL 并靠 WAKE_MIN_INTERNAL_FREE 保底 */
    cfg->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_INTERNAL;

    char *vad_name = esp_srmodel_filter(s_models, ESP_VADN_PREFIX, NULL);
    if (vad_name != NULL) {
        cfg->vad_model_name = vad_name;
    }

    s_afe_iface = esp_afe_handle_from_config(cfg);
    if (s_afe_iface != NULL) {
        s_afe_data = s_afe_iface->create_from_config(cfg);
    }
    afe_config_free(cfg);

    if (s_afe_iface == NULL || s_afe_data == NULL) {
        ESP_LOGE(TAG, "afe create failed");
        s_afe_iface = NULL;
        goto fail;
    }

    s_feed_chunk = s_afe_iface->get_feed_chunksize(s_afe_data);
    if (s_feed_chunk <= 0 || s_feed_chunk > SERVICE_VOICE_AFE_MAX_CHUNK) {
        ESP_LOGE(TAG, "unexpected feed chunk %ld", (long)s_feed_chunk);
        s_afe_iface->destroy(s_afe_data);
        s_afe_data = NULL;
        s_afe_iface = NULL;
        s_feed_chunk = 0;
        goto fail;
    }

    /* 创建结果队列（深度 16：约 0.5s fetch 缓冲，满时挤旧，见 fetch 任务）；
     * 深度过小（旧值 4）时消费侧偶发停顿即丢块，上行音频缺块导致服务器
     * ASR 识别残缺（真机：唤醒词被识成乱码）。消息含音频块体积较大，
     * 落 PSRAM 省内部 RAM：仅 fetch 任务写、task_audio 读，无 ISR 上下文 */
    s_result_queue = xQueueCreateWithCaps(16, sizeof(wake_result_msg_t), MALLOC_CAP_SPIRAM);
    if (s_result_queue == NULL) {
        ESP_LOGE(TAG, "result queue create failed");
        s_afe_iface->destroy(s_afe_data);
        s_afe_data = NULL;
        s_afe_iface = NULL;
        s_feed_chunk = 0;
        goto fail;
    }

    /* 启动独立 fetch 任务：栈用 PSRAM 静态分配，省下 16KB 内部 RAM
     * （本任务无 flash 写路径，PSRAM 栈安全）；PSRAM 分配失败时回退内部动态栈 */
    s_fetch_stop = false;
    static StackType_t *s_fetch_stack = NULL;
    static StaticTask_t s_fetch_tcb;
    if (s_fetch_stack == NULL) {
        s_fetch_stack = heap_caps_malloc(WAKE_FETCH_TASK_STACK,
                                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    BaseType_t ok = pdFAIL;
    if (s_fetch_stack != NULL) {
        s_fetch_task = xTaskCreateStaticPinnedToCore(
            wake_fetch_task_entry, "wake_fetch",
            WAKE_FETCH_TASK_STACK, NULL,
            WAKE_FETCH_TASK_PRIO, s_fetch_stack, &s_fetch_tcb,
            WAKE_FETCH_TASK_CORE);
        ok = (s_fetch_task != NULL) ? pdPASS : pdFAIL;
    } else {
        ok = xTaskCreatePinnedToCore(
            wake_fetch_task_entry, "wake_fetch",
            WAKE_FETCH_TASK_STACK, NULL,
            WAKE_FETCH_TASK_PRIO, &s_fetch_task, WAKE_FETCH_TASK_CORE);
    }
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "fetch task create failed");
        vQueueDeleteWithCaps(s_result_queue);
        s_result_queue = NULL;
        s_afe_iface->destroy(s_afe_data);
        s_afe_data = NULL;
        s_afe_iface = NULL;
        s_feed_chunk = 0;
        goto fail;
    }

    s_afe_iface->print_pipeline(s_afe_data);
    /* AFE 常驻后，pump 停喂期间 esp-sr 每 100ms 刷 "Ringbuffer of AFE is empty"
     * 警告（空闲期是常态），降噪到 ERROR；本模块自身日志在 xiaozhi_wake 标签不受影响 */
    esp_log_level_set("AFE", ESP_LOG_ERROR);
    ESP_LOGI(TAG, "wake frontend ready: wakenet=%s feed_chunk=%ld fetch_chunk=%d",
             wn_name, (long)s_feed_chunk, s_afe_iface->get_fetch_chunksize(s_afe_data));
    ESP_LOGI(TAG, "AFE 打开后内部 RAM: 空闲 %u 字节, 最大连续块 %u 字节",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    return ESP_OK;

fail:
    if (s_models != NULL) {
        esp_srmodel_deinit(s_models);
        s_models = NULL;
    }
    return ESP_FAIL;
}

void service_voice_wake_close(void)
{
    /* 先通知 fetch 任务退出 */
    s_fetch_stop = true;
    if (s_fetch_task != NULL) {
        /* 等待任务自行退出（最多 200ms = 2 × WAKE_FETCH_TIMEOUT_MS） */
        for (int i = 0; i < 20 && s_fetch_task != NULL; i++) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    if (s_result_queue != NULL) {
        vQueueDeleteWithCaps(s_result_queue);
        s_result_queue = NULL;
    }

    if (s_afe_data != NULL && s_afe_iface != NULL) {
        s_afe_iface->destroy(s_afe_data);
    }
    s_afe_data = NULL;
    s_afe_iface = NULL;
    s_feed_chunk = 0;

    if (s_models != NULL) {
        esp_srmodel_deinit(s_models);
        s_models = NULL;
    }
}

bool service_voice_wake_is_active(void)
{
    return s_afe_data != NULL;
}

int32_t service_voice_wake_get_feed_chunk(void)
{
    return s_feed_chunk;
}

int32_t service_voice_wake_get_fetch_chunk(void)
{
    if (s_afe_data == NULL || s_afe_iface == NULL) {
        return 0;
    }
    return s_afe_iface->get_fetch_chunksize(s_afe_data);
}

esp_err_t service_voice_wake_feed(const int16_t *pcm)
{
    if (s_afe_data == NULL || pcm == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    int ret = s_afe_iface->feed(s_afe_data, pcm);
    if (ret < 0) {
        ESP_LOGW(TAG, "afe feed failed: %d", ret);
        return ESP_FAIL;
    }
    return ESP_OK;
}

bool service_voice_wake_poll(service_voice_wake_result_t *out)
{
    if (s_result_queue == NULL || out == NULL) {
        return false;
    }
    wake_result_msg_t msg;
    if (xQueueReceive(s_result_queue, &msg, 0) != pdTRUE) {
        return false;
    }
    out->samples = msg.samples;
    /* 检出沿在此翻译：esp-sr 的 wakenet_model_index 是锁存状态字段（命中后
     * 长期保持 >0），只有 wakeup_state==WAKENET_DETECTED 才是一次性命中事件；
     * 调用方只认 wake_detected，避免把锁存态当事件重复消费（真机：一次命中
     * 每个 fetch 块重复"命中"，刷屏且永远无法完成一次消费） */
    out->wake_detected = (msg.wakeup_state == WAKENET_DETECTED);
    out->wakenet_model_index = msg.wakenet_model_index;
    out->vad_state = msg.vad_state;
    /* 数据在消息内，调用方需在下一次 poll 前使用完毕；
     * 为安全起见直接指向队列内部已不可行（消息已出队），
     * 改为把指针指向静态暂存区 */
    static int16_t s_poll_buf[SERVICE_VOICE_AFE_MAX_CHUNK];
    memcpy(s_poll_buf, msg.data, (size_t)msg.samples * sizeof(int16_t));
    out->data = s_poll_buf;
    /* 仅在检出沿更新唤醒词文本（锁存的 model_index 在沿到达时有效） */
    if (out->wake_detected &&
        msg.wakenet_model_index > 0 &&
        msg.wakenet_model_index <= (int)(sizeof(s_wake_words)/sizeof(s_wake_words[0])) - 1) {
        s_last_wake_word = s_wake_words[msg.wakenet_model_index];
    }
    return true;
}

const char *service_voice_wake_get_word(void)
{
    return s_last_wake_word;
}

void service_voice_wake_set_detection(bool enable)
{
    /* 只登记目标态，实际启停由 fetch 任务上下文执行（线程安全） */
    s_wakenet_cmd = enable ? 2 : 1;
}

void service_voice_wake_reset_buffer(void)
{
    s_reset_buf_req = true;
}
