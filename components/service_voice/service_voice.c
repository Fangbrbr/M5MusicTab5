/**
 * @file service_voice.c
 * @brief 语音前端主控：mic/AEC/AFE/WakeNet/Opus 编解码
 *
 * 由 task_audio 每周期驱动，把 44.1kHz 全双工 mic/ref 流送入 AFE，产生
 * 唤醒/VAD/上行 Opus 包事件队列，供 service_xiaozhi 协议状态机消费。
 */

#include "service_voice.h"
#include "service_voice_opus.h"
#include "service_voice_wake.h"

#include "sdkconfig.h"

#include "service_audio.h"
#include "service_audio_config.h"

#include "esp_ae_rate_cvt.h"
#include "esp_audio_types.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_timer.h"

#include "string.h"

static const char *TAG = "service_voice";

/* --------------------------------------------------------------------------
 * 配置常量
 * ------------------------------------------------------------------------ */

/** @brief 命令队列深度 */
#define SERVICE_VOICE_CMD_QUEUE_LEN 8

/** @brief 事件队列深度 */
#define SERVICE_VOICE_EVT_QUEUE_LEN 32

/** @brief 单次 mic 读取帧数上限（@44.1kHz，约 1.45ms） */
#define SERVICE_VOICE_MIC_READ_FRAMES SERVICE_AUDIO_FRAMES_PER_PERIOD

/** @brief 重采样最大输出帧数：需覆盖 64@44.1k→16k 的转换结果加滤波器
 * 尾延迟（complexity=2 时尾延迟可达 100+ 帧，原 32 帧上限导致
 * "max out overflow" 刷屏且 AEC 路径永不喂入 AFE） */
#define SERVICE_VOICE_RS_OUT_MAX_FRAMES 256

/** @brief 重采样后 MR 缓冲帧数（约 250ms@16k） */
#define SERVICE_VOICE_FEED_OUT_RING_FRAMES 4096

/** @brief 进入 LISTENING 后丢弃前 120ms AFE 输出，避免 mic 启动噪声上传 */
#define SERVICE_VOICE_LISTEN_WARMUP_MS            120


/* --------------------------------------------------------------------------
 * 命令队列
 * ------------------------------------------------------------------------ */

typedef enum {
    VOICE_CMD_START_LISTEN = 0,
    VOICE_CMD_STOP_LISTEN,
    VOICE_CMD_ENABLE_WAKE,
} service_voice_cmd_type_t;

typedef struct {
    service_voice_cmd_type_t type;
    bool enable;
} service_voice_cmd_t;

/* --------------------------------------------------------------------------
 * 静态状态
 * ------------------------------------------------------------------------ */

static bool s_initialized = false;
static bool s_afe_active = false;
static bool s_listening = false;
static bool s_wake_enabled = false;
static bool s_mic_open = false;
static bool s_mic_error_posted = false;
/* AFE 路径正在 task_audio 执行中：关闭 AFE 前必须等其静默（quiesce），
 * 否则 Core 0 侧 destroy 会从 Core 1 的 AFE/重采样中段脚下抽空
 * （真机：Core1 Saved PC 落在 fa_resample_process，Core 0 中断向量被
 * 堆栈/堆破坏污染 → Instruction access fault） */
static volatile bool s_afe_busy = false;

static QueueHandle_t s_cmd_queue = NULL;
static QueueHandle_t s_evt_queue = NULL;

/* 上行编码缓冲（PSRAM） */
static int16_t *s_mic_buf = NULL;      /*!< 攒满 960 采样后编码 */
static uint8_t *s_enc_out = NULL;      /*!< 编码工作缓冲 */
static int16_t *s_afe_buf = NULL;      /*!< AFE feed 缓冲（MR 格式） */

/* AEC 重采样：44.1k MR → 16k MR；AFE 未激活时退化为 44.1k mono → 16k mono */
static esp_ae_rate_cvt_handle_t s_rate_cvt = NULL;
static int s_rate_cvt_channels = 0;   /*!< 当前重采样器声道数，变更需重开 */
static int16_t s_aec_mic_raw[SERVICE_VOICE_MIC_READ_FRAMES];
static int16_t s_aec_ref_raw[SERVICE_VOICE_MIC_READ_FRAMES];
static int16_t s_aec_in[SERVICE_VOICE_MIC_READ_FRAMES * 2];
static int16_t s_aec_out_tmp[SERVICE_VOICE_RS_OUT_MAX_FRAMES * 2];
/* AFE 输出环形缓冲 16KB：落 PSRAM 省内部 RAM（初始化时分配） */
static int16_t *s_aec_out_ring = NULL;
static uint32_t s_aec_out_count = 0;
static uint32_t s_aec_out_rpos = 0;

/* manual 路径复用同一重采样器，但输入/输出缓冲独立命名以清晰 */
static int16_t s_manual_raw[SERVICE_VOICE_MIC_READ_FRAMES];
static int16_t s_manual_out_tmp[SERVICE_VOICE_RS_OUT_MAX_FRAMES];

/* AFE 状态 */
static int32_t s_afe_feed_chunk = 0;
static int s_last_vad = 0;

/* 聆听状态 */
static uint32_t s_mic_frames = 0;
static uint32_t s_listen_warmup_samples = 0;

/* 录音 tap（task_audio 上下文回调，仅允许 memcpy 级操作） */
static service_voice_mic_tap_cb_t s_mic_tap_cb = NULL;
static void *s_mic_tap_ctx = NULL;
static bool s_mic_tap_aec = false;
static int16_t s_mic_tap_raw_buf[SERVICE_VOICE_RS_OUT_MAX_FRAMES];

/* --------------------------------------------------------------------------
 * 前向声明
 * ------------------------------------------------------------------------ */

static void service_voice_aec_push_and_feed(uint32_t new_frames);
static void service_voice_post_event(const service_voice_event_t *evt);
static void service_voice_post_packet(const uint8_t *data, uint32_t len);
static void service_voice_post_vad(bool is_speech);
static void service_voice_post_wake(const char *word);
static void service_voice_post_error(const char *text);
static bool service_voice_open_resampler(int channels);
static void service_voice_reset_resampler(void);
static void service_voice_process_commands(void);
static void service_voice_manage_mic(void);
static void service_voice_process_afe_path_inner(void);
static void service_voice_process_afe_path(void);
static void service_voice_process_manual_path(void);
static void service_voice_append_and_encode(const int16_t *pcm, uint32_t samples);

/* --------------------------------------------------------------------------
 * 事件投递
 * ------------------------------------------------------------------------ */

/* 唤醒去重：已投递未消费的唤醒事件计数，防阻塞期间积压刷屏溢队。
 * Trap: ++ 在生产者(task_audio)、-- 在消费者(task_ai)，跨任务读-改-写
 * 必须原子操作，否则丢更新后计数卡 ≥1，唤醒被永久静默直至重启。 */
static volatile uint32_t s_wake_pending = 0;

/* 唤醒检出冷却：真实命中后 2s 内的重复检出沿直接丢弃（同一段语音的
 * 残响尾可能让 WakeNet 二次检出），不影响之后的正常唤醒。 */
#define SERVICE_VOICE_WAKE_COOLDOWN_MS 2000
static TickType_t s_last_wake_tick = 0;

/* 唤醒检出重新武装时刻：武装后短暂宽限内的命中沿一律丢弃。
 * Why: 重新武装（enable）与清环指令异步在 fetch 任务上下文生效，宽限
 * 覆盖指令落地前的检出尾，防武装前残存检出被当新命中（对话刚结束
 * 又自启一轮对话）。 */
#define SERVICE_VOICE_WAKE_ARM_GRACE_MS 500
static TickType_t s_wake_armed_tick = 0;

static void service_voice_post_event(const service_voice_event_t *evt)
{
    if (s_evt_queue == NULL || evt == NULL) {
        return;
    }
    if (evt->type == SERVICE_VOICE_EVT_WAKE &&
        __atomic_load_n(&s_wake_pending, __ATOMIC_ACQUIRE) > 0) {
        return;
    }
    if (xQueueSend(s_evt_queue, evt, 0) != pdTRUE) {
        if (evt->type == SERVICE_VOICE_EVT_PACKET && evt->data != NULL) {
            heap_caps_free((void *)evt->data);
        }
        ESP_LOGW(TAG, "event queue full, drop type=%d", evt->type);
        return;
    }
    if (evt->type == SERVICE_VOICE_EVT_WAKE) {
        __atomic_fetch_add(&s_wake_pending, 1, __ATOMIC_ACQ_REL);
    }
}

static void service_voice_post_packet(const uint8_t *data, uint32_t len)
{
    if (s_evt_queue == NULL) {
        return;
    }
    uint8_t *copy = heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
    if (copy == NULL) {
        ESP_LOGW(TAG, "packet alloc failed, drop %lu bytes", (unsigned long)len);
        return;
    }
    memcpy(copy, data, len);
    service_voice_event_t evt = {
        .type = SERVICE_VOICE_EVT_PACKET,
        .data = copy,
        .len = len,
    };
    service_voice_post_event(&evt);
}

static void service_voice_post_vad(bool is_speech)
{
    service_voice_event_t evt = {
        .type = SERVICE_VOICE_EVT_VAD,
        .is_speech = is_speech,
    };
    service_voice_post_event(&evt);
}

static void service_voice_post_wake(const char *word)
{
    service_voice_event_t evt = {
        .type = SERVICE_VOICE_EVT_WAKE,
        .text = word,
    };
    service_voice_post_event(&evt);
}

static void service_voice_post_error(const char *text)
{
    service_voice_event_t evt = {
        .type = SERVICE_VOICE_EVT_ERROR,
        .text = text,
    };
    service_voice_post_event(&evt);
}

/* --------------------------------------------------------------------------
 * 重采样器
 * ------------------------------------------------------------------------ */

static bool service_voice_open_resampler(int channels)
{
    if (s_rate_cvt != NULL) {
        if (channels == s_rate_cvt_channels) {
            esp_ae_rate_cvt_reset(s_rate_cvt);
            return true;
        }
        /* 声道数变化（mono↔MR）：esp_ae_rate_cvt 不支持在位改声道，
         * 只 reset 会用 1ch 实例处理 2ch 交错数据导致 AEC 输入错乱，必须重开 */
        esp_ae_rate_cvt_close(s_rate_cvt);
        s_rate_cvt = NULL;
    }
    esp_ae_rate_cvt_cfg_t cfg = {
        .src_rate        = SERVICE_AUDIO_SAMPLE_RATE,
        .dest_rate       = SERVICE_VOICE_OPUS_ENC_SAMPLE_RATE,
        .channel         = channels,
        .bits_per_sample = ESP_AUDIO_BIT16,
        .complexity      = 2,
        .perf_type       = ESP_AE_RATE_CVT_PERF_TYPE_SPEED,
    };
    esp_audio_err_t ret = esp_ae_rate_cvt_open(&cfg, &s_rate_cvt);
    if (ret != ESP_AUDIO_ERR_OK || s_rate_cvt == NULL) {
        ESP_LOGE(TAG, "rate cvt open failed: %d", (int)ret);
        s_rate_cvt = NULL;
        s_rate_cvt_channels = 0;
        return false;
    }
    s_rate_cvt_channels = channels;
    return true;
}

static void service_voice_reset_resampler(void)
{
    if (s_rate_cvt != NULL) {
        esp_ae_rate_cvt_reset(s_rate_cvt);
    }
}

/* --------------------------------------------------------------------------
 * AFE feed ring 与编码缓冲
 * ------------------------------------------------------------------------ */

/* 临时调试：上行管道各级速率计数（3s 窗口），定位半速断点 */
static uint32_t s_dbg_mic_samples = 0;
static uint32_t s_dbg_rs_frames = 0;
static uint32_t s_dbg_feed_fail = 0;
static uint32_t s_dbg_polled_samples = 0;
static int64_t s_dbg_window_us = 0;

static void service_voice_aec_push_and_feed(uint32_t new_frames)
{
    if (new_frames == 0 || s_afe_feed_chunk <= 0 || s_aec_out_ring == NULL) {
        return;
    }
    /* ring 满时丢弃最旧帧，保证新数据不溢出 */
    while (new_frames >= SERVICE_VOICE_FEED_OUT_RING_FRAMES - s_aec_out_count) {
        s_aec_out_rpos = (s_aec_out_rpos + (uint32_t)s_afe_feed_chunk) % SERVICE_VOICE_FEED_OUT_RING_FRAMES;
        s_aec_out_count = (s_aec_out_count > (uint32_t)s_afe_feed_chunk) ?
                          (s_aec_out_count - (uint32_t)s_afe_feed_chunk) : 0;
    }

    /* 追加到 ring */
    for (uint32_t i = 0; i < new_frames; i++) {
        uint32_t wpos = (s_aec_out_rpos + s_aec_out_count) % SERVICE_VOICE_FEED_OUT_RING_FRAMES;
        s_aec_out_ring[wpos * 2]     = s_aec_out_tmp[i * 2];
        s_aec_out_ring[wpos * 2 + 1] = s_aec_out_tmp[i * 2 + 1];
        s_aec_out_count++;
    }

    /* 每次喂一个完整的 feed_chunk */
    while (s_aec_out_count >= (uint32_t)s_afe_feed_chunk) {
        for (int i = 0; i < s_afe_feed_chunk; i++) {
            uint32_t rpos = (s_aec_out_rpos + (uint32_t)i) % SERVICE_VOICE_FEED_OUT_RING_FRAMES;
            s_afe_buf[i * 2]     = s_aec_out_ring[rpos * 2];
            s_afe_buf[i * 2 + 1] = s_aec_out_ring[rpos * 2 + 1];
        }
        s_aec_out_rpos = (s_aec_out_rpos + (uint32_t)s_afe_feed_chunk) % SERVICE_VOICE_FEED_OUT_RING_FRAMES;
        s_aec_out_count -= (uint32_t)s_afe_feed_chunk;

        if (service_voice_wake_feed(s_afe_buf) != ESP_OK) {
            s_dbg_feed_fail++;
            break;
        }
    }
}

static void service_voice_append_and_encode(const int16_t *pcm, uint32_t samples)
{
    if (pcm == NULL || samples == 0 || s_mic_buf == NULL || s_enc_out == NULL) {
        return;
    }
    uint32_t off = 0;
    while (off < samples) {
        uint32_t space = SERVICE_VOICE_OPUS_ENC_FRAME_SAMPLES - s_mic_frames;
        uint32_t rest = samples - off;
        uint32_t take = (rest < space) ? rest : space;
        if (take > 0) {
            memcpy(s_mic_buf + s_mic_frames, pcm + off, take * sizeof(int16_t));
            s_mic_frames += take;
            off += take;
        }
        if (s_mic_frames >= SERVICE_VOICE_OPUS_ENC_FRAME_SAMPLES) {
            int bytes = service_voice_opus_encode(s_mic_buf, s_enc_out, SERVICE_VOICE_OPUS_ENC_OUT_SIZE);
            if (bytes > 0) {
                service_voice_post_packet(s_enc_out, (uint32_t)bytes);
                /* 临时调试：上行编码包计数，验证编码链路是否在产包 */
                static uint32_t s_enc_dbg_cnt = 0;
                s_enc_dbg_cnt++;
                if ((s_enc_dbg_cnt % 20) == 1) {
                    ESP_LOGD(TAG, "[dbg] enc pkt #%lu, %d bytes",
                             (unsigned long)s_enc_dbg_cnt, bytes);
                }
            }
            s_mic_frames = 0;
        }
    }
}

/* --------------------------------------------------------------------------
 * 命令处理
 * ------------------------------------------------------------------------ */

static void service_voice_process_commands(void)
{
    service_voice_cmd_t cmd;
    while (xQueueReceive(s_cmd_queue, &cmd, 0) == pdTRUE) {
        switch (cmd.type) {
        case VOICE_CMD_START_LISTEN:
            if (!s_listening) {
                s_listening = true;
                s_mic_frames = 0;
                s_last_vad = 0;
                s_listen_warmup_samples = SERVICE_VOICE_OPUS_ENC_SAMPLE_RATE * SERVICE_VOICE_LISTEN_WARMUP_MS / 1000;
                service_voice_opus_encoder_open();
                service_voice_reset_resampler();
                /* 丢弃滞留旧音频，避免把 mic 关闭期间的陈旧语音编码上传 */
                service_voice_wake_reset_buffer();
            }
            break;
        case VOICE_CMD_STOP_LISTEN:
            if (s_listening) {
                s_listening = false;
                s_mic_frames = 0;
                s_listen_warmup_samples = 0;
                service_voice_opus_encoder_close();
            }
            break;
        case VOICE_CMD_ENABLE_WAKE:
            s_wake_enabled = cmd.enable;
            /* 检出侧同步启停 WakeNet（上游 xiaozhi 检出即停 WakeNet 的等价物）；
             * 重新使能前重置 AFE 环，丢弃滞留的旧语音（含唤醒词），
             * 否则重开后 WakeNet/VAD 对旧音频重复检出造成误唤醒 */
            service_voice_wake_set_detection(cmd.enable);
            if (cmd.enable) {
                service_voice_wake_reset_buffer();
                s_wake_armed_tick = xTaskGetTickCount();
            }
            break;
        default:
            break;
        }
    }
}

/* --------------------------------------------------------------------------
 * mic 生命周期
 * ------------------------------------------------------------------------ */

static void service_voice_manage_mic(void)
{
    /* AFE 常驻期间 mic 长开（全双工）。Why: 配对 I2S 上关闭 RX（mic）会
     * 干扰/停摆 TX DMA，task_audio 的 codec 写随之阻塞（真机：TTS 播报
     * 期间关 mic 后音频冻结 7s，仅零星滋滋声），且每次开关引入 I2S 重配
     * 与 AEC 参考复位瞬态；AFE 断喂还会刷空环告警。manual 模式（AFE 未
     * 打开）仍按需开 mic。 */
    bool need_mic = s_listening || s_afe_active;
    if (need_mic && !s_mic_open) {
        if (service_audio_mic_open(SERVICE_AUDIO_SAMPLE_RATE, 1) == ESP_OK) {
            s_mic_open = true;
            s_mic_error_posted = false;
            if (s_afe_active) {
                service_audio_aec_ref_start();
            }
        } else {
            if (!s_mic_error_posted) {
                s_mic_error_posted = true;
                service_voice_post_error("麦克风打开失败");
            }
        }
    } else if (!need_mic && s_mic_open) {
        service_audio_mic_close();
        if (s_afe_active) {
            service_audio_aec_ref_stop();
        }
        s_mic_open = false;
        s_mic_error_posted = false;
    }
}

/* --------------------------------------------------------------------------
 * AFE 路径：44.1k mic/ref → 16k MR → AFE → 编码/唤醒
 * ------------------------------------------------------------------------ */

static void service_voice_process_afe_path_inner(void)
{
    /* 先保证参考信号有足够数据，再按同一长度读取 mic，确保 M/R 严格对齐 */
    uint32_t ref_avail = service_audio_aec_ref_available();
    if (ref_avail == 0) {
        return;
    }
    uint32_t want = (ref_avail < SERVICE_VOICE_MIC_READ_FRAMES) ? ref_avail : SERVICE_VOICE_MIC_READ_FRAMES;

    int32_t n = service_audio_mic_read(s_aec_mic_raw, want);
    if (n < 0) {
        ESP_LOGW(TAG, "mic read error, close mic");
        service_audio_mic_close();
        s_mic_open = false;
        service_audio_aec_ref_stop();
        service_voice_post_error("麦克风读取失败");
        return;
    }
    if (n == 0) {
        return;
    }

    uint32_t ref_n = service_audio_aec_ref_read(s_aec_ref_raw, (uint32_t)n);
    if (ref_n < (uint32_t)n) {
        /* 理论上不应发生：已按 avail 读取；不足时补零保持 M/R 长度一致 */
        memset(s_aec_ref_raw + ref_n, 0, ((uint32_t)n - ref_n) * sizeof(int16_t));
    }

    /* 构造 44.1k MR 交错输入 */
    for (uint32_t i = 0; i < (uint32_t)n; i++) {
        s_aec_in[i * 2]     = s_aec_mic_raw[i];
        s_aec_in[i * 2 + 1] = s_aec_ref_raw[i];
    }

    /* 重采样到 16k MR */
    uint32_t out_max = 0;
    esp_audio_err_t rs_ret = esp_ae_rate_cvt_get_max_out_sample_num(s_rate_cvt, (uint32_t)n, &out_max);
    if (rs_ret != ESP_AUDIO_ERR_OK || out_max > SERVICE_VOICE_RS_OUT_MAX_FRAMES) {
        ESP_LOGW(TAG, "resampler max out error or overflow: %d/%lu", (int)rs_ret, (unsigned long)out_max);
        return;
    }
    uint32_t out_frames = out_max;
    rs_ret = esp_ae_rate_cvt_process(s_rate_cvt, (esp_ae_sample_t)s_aec_in, (uint32_t)n,
                                       (esp_ae_sample_t)s_aec_out_tmp, &out_frames);
    if (rs_ret != ESP_AUDIO_ERR_OK) {
        ESP_LOGW(TAG, "resampler process failed: %d", (int)rs_ret);
        return;
    }
    s_dbg_mic_samples += (uint32_t)n;
    s_dbg_rs_frames += out_frames;

    /* 录音 tap（原始路）：取重采样后 MR 的 M 通道去交错为 16k mono，
     * 在 AFE 馈入前旁路——不携带 AEC 处理痕迹，但含扬声器串音 */
    if (s_mic_tap_cb != NULL && !s_mic_tap_aec) {
        uint32_t tap_n = (out_frames < SERVICE_VOICE_RS_OUT_MAX_FRAMES)
                             ? out_frames : SERVICE_VOICE_RS_OUT_MAX_FRAMES;
        for (uint32_t i = 0; i < tap_n; i++) {
            s_mic_tap_raw_buf[i] = s_aec_out_tmp[i * 2];
        }
        s_mic_tap_cb(s_mic_tap_raw_buf, tap_n, s_mic_tap_ctx);
    }

    service_voice_aec_push_and_feed(out_frames);

    /* 从独立 fetch 任务的结果队列非阻塞取（可能多帧积压，循环取尽） */
    service_voice_wake_result_t res;
    while (service_voice_wake_poll(&res)) {
        s_dbg_polled_samples += (uint32_t)res.samples;
        /* 录音 tap（AEC 路）：AFE 输出即 AEC 后信号；res.data 为单缓冲，
         * 必须在本轮 poll 内消费完（tap 契约已限定 memcpy 级） */
        if (s_mic_tap_cb != NULL && s_mic_tap_aec) {
            s_mic_tap_cb(res.data, (uint32_t)res.samples, s_mic_tap_ctx);
        }
        if (s_listening) {
            /* 本地 VAD 边沿透传 App 做收音指示 */
            if (res.vad_state != s_last_vad) {
                s_last_vad = res.vad_state;
                service_voice_post_vad(res.vad_state == 1);
            }
            const int16_t *data = res.data;
            int samples = res.samples;
            /* 进入 LISTENING 后前 120ms 丢弃，避免 mic 路径启动噪声上传 */
            if (s_listen_warmup_samples > 0) {
                uint32_t skip = ((uint32_t)samples < s_listen_warmup_samples) ? (uint32_t)samples : s_listen_warmup_samples;
                data += skip;
                samples -= (int)skip;
                s_listen_warmup_samples -= skip;
            }
            service_voice_append_and_encode(data, (uint32_t)samples);
        } else if (s_wake_enabled) {
            /* 唤醒命中只认检出沿（wake_detected，即 esp-sr wakeup_state 的
             * WAKENET_DETECTED）：wakenet_model_index 是锁存状态字段，命中后
             * 长期保持 >0，误当事件会把一次命中在每个 fetch 块上重复触发
             * （真机：唤醒刷屏且永远无法完成一次消费）。
             * 门控仅留两层：武装宽限（防检出尾自启新一轮）+ 2s 冷却（防残响
             * 二次检出）。 */
            if (res.wake_detected) {
                TickType_t gate_now = xTaskGetTickCount();
                bool grace_ok =
                    (gate_now - s_wake_armed_tick) >=
                        pdMS_TO_TICKS(SERVICE_VOICE_WAKE_ARM_GRACE_MS);
                bool cooldown_ok = s_last_wake_tick == 0 ||
                    (gate_now - s_last_wake_tick) >=
                        pdMS_TO_TICKS(SERVICE_VOICE_WAKE_COOLDOWN_MS);
                if (grace_ok && cooldown_ok) {
                    s_last_wake_tick = gate_now;
                    ESP_LOGI(TAG, "唤醒词命中 (model_index=%d)", res.wakenet_model_index);
                    service_voice_post_wake(service_voice_wake_get_word());
                } else {
                    /* 被门控丢弃的命中沿 2s 限流打印，观察宽限/冷却是否误伤 */
                    static TickType_t s_gate_dbg_tick = 0;
                    if ((gate_now - s_gate_dbg_tick) > pdMS_TO_TICKS(2000)) {
                        s_gate_dbg_tick = gate_now;
                        ESP_LOGW(TAG, "[dbg] 唤醒命中沿被门控丢弃: grace=%d cd=%d",
                                 (int)grace_ok, (int)cooldown_ok);
                    }
                }
            }
        }
    }

    /* 临时调试：3s 窗口打印各级采样速率，定位半速断点。
     * 期望值（实时）：mic≈13230/3s(44.1k)、rs≈4800/3s(16k)、polled≈4800/3s */
    if (s_dbg_window_us == 0) {
        s_dbg_window_us = esp_timer_get_time();
    } else if ((esp_timer_get_time() - s_dbg_window_us) >= 3000000) {
        ESP_LOGD(TAG, "[dbg] pipe/3s mic=%lu rs=%lu polled=%lu feed_fail=%lu",
                 (unsigned long)s_dbg_mic_samples, (unsigned long)s_dbg_rs_frames,
                 (unsigned long)s_dbg_polled_samples, (unsigned long)s_dbg_feed_fail);
        s_dbg_mic_samples = 0;
        s_dbg_rs_frames = 0;
        s_dbg_polled_samples = 0;
        s_dbg_feed_fail = 0;
        s_dbg_window_us = esp_timer_get_time();
    }
}

/* AFE 路径包装：busy 标志 + active 双检（关停竞态防护，见 s_afe_busy 注释） */
static void service_voice_process_afe_path(void)
{
    s_afe_busy = true;
    __sync_synchronize();
    if (s_afe_active) {
        service_voice_process_afe_path_inner();
    }
    __sync_synchronize();
    s_afe_busy = false;
}

/* --------------------------------------------------------------------------
 * manual 路径：AFE 不可用时，44.1k mono → 16k mono → 编码
 * ------------------------------------------------------------------------ */

static void service_voice_process_manual_path(void)
{
    int32_t n = service_audio_mic_read(s_manual_raw, SERVICE_VOICE_MIC_READ_FRAMES);
    if (n < 0) {
        ESP_LOGW(TAG, "mic read error, close mic");
        service_audio_mic_close();
        s_mic_open = false;
        service_voice_post_error("麦克风读取失败");
        return;
    }
    if (n == 0) {
        return;
    }

    uint32_t out_max = 0;
    esp_audio_err_t rs_ret = esp_ae_rate_cvt_get_max_out_sample_num(s_rate_cvt, (uint32_t)n, &out_max);
    if (rs_ret != ESP_AUDIO_ERR_OK || out_max > SERVICE_VOICE_RS_OUT_MAX_FRAMES) {
        ESP_LOGW(TAG, "resampler max out error or overflow: %d/%lu", (int)rs_ret, (unsigned long)out_max);
        return;
    }
    uint32_t out_frames = out_max;
    rs_ret = esp_ae_rate_cvt_process(s_rate_cvt, (esp_ae_sample_t)s_manual_raw, (uint32_t)n,
                                       (esp_ae_sample_t)s_manual_out_tmp, &out_frames);
    if (rs_ret != ESP_AUDIO_ERR_OK) {
        ESP_LOGW(TAG, "resampler process failed: %d", (int)rs_ret);
        return;
    }

    /* 与 AFE 路径一致的暖机逻辑 */
    const int16_t *data = s_manual_out_tmp;
    uint32_t samples = out_frames;
    if (s_listen_warmup_samples > 0) {
        uint32_t skip = (samples < s_listen_warmup_samples) ? samples : s_listen_warmup_samples;
        data += skip;
        samples -= skip;
        s_listen_warmup_samples -= skip;
    }
    service_voice_append_and_encode(data, samples);
}

/* --------------------------------------------------------------------------
 * 对外 API
 * ------------------------------------------------------------------------ */

esp_err_t service_voice_init(void)
{
#if !CONFIG_BOARD_HAS_MIC
    /* Why: 无 mic 的板（jc4880p443）不启动语音前端，避免 AFE 白白占用约 110KB
     * 内部 RAM；main.c 用 ESP_ERROR_CHECK_WITHOUT_ABORT 容忍本返回值，
     * 其余公开接口经 s_initialized / 队列空值守卫全部安全降级。 */
    ESP_LOGW(TAG, "board has no mic, voice frontend disabled");
    return ESP_ERR_NOT_SUPPORTED;
#endif

    if (s_initialized) {
        return ESP_OK;
    }

    /* 队列与音频缓冲一次性从 PSRAM 分配；任何一块失败即整体失败。
     * Why: AFE 张量必须整体落内部 RAM，其余能外迁的内占一律外迁。 */
    s_cmd_queue = xQueueCreate(SERVICE_VOICE_CMD_QUEUE_LEN, sizeof(service_voice_cmd_t));
    s_evt_queue = xQueueCreate(SERVICE_VOICE_EVT_QUEUE_LEN, sizeof(service_voice_event_t));
    s_mic_buf = heap_caps_malloc(SERVICE_VOICE_OPUS_ENC_FRAME_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    s_enc_out = heap_caps_malloc(SERVICE_VOICE_OPUS_ENC_OUT_SIZE, MALLOC_CAP_SPIRAM);
    s_afe_buf = heap_caps_malloc(SERVICE_VOICE_AFE_MAX_CHUNK * 2 * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    /* AFE 输出环形缓冲 16KB 落 PSRAM，省内部 RAM */
    s_aec_out_ring = heap_caps_malloc(SERVICE_VOICE_FEED_OUT_RING_FRAMES * 2 * sizeof(int16_t), MALLOC_CAP_SPIRAM);

    if (s_cmd_queue == NULL || s_evt_queue == NULL ||
        s_mic_buf == NULL || s_enc_out == NULL || s_afe_buf == NULL ||
        s_aec_out_ring == NULL) {
        ESP_LOGE(TAG, "init alloc failed");
        goto fail;
    }

    /* AFE 在 boot 阶段由 main.c 提前打开（WiFi 前内存充足）；此处仅初始化缓冲与
     * manual 路径，保持 AFE 生命周期与 Service 初始化解耦。音乐 App 不受影响。 */
    s_afe_active = false;
    s_afe_feed_chunk = 0;
    ESP_LOGI(TAG, "voice frontend initialized (AFE deferred, manual mode ready)");

    /* 初始化为 mono 重采样器（无 AEC）；AFE 打开后切换到 MR */
    if (!service_voice_open_resampler(1)) {
        ESP_LOGE(TAG, "resampler open failed");
        goto fail;
    }

    service_voice_wake_open_deferred();
    s_initialized = true;
    return ESP_OK;

fail:
    if (s_cmd_queue != NULL) {
        vQueueDelete(s_cmd_queue);
        s_cmd_queue = NULL;
    }
    if (s_evt_queue != NULL) {
        vQueueDelete(s_evt_queue);
        s_evt_queue = NULL;
    }
    heap_caps_free(s_mic_buf);
    heap_caps_free(s_enc_out);
    heap_caps_free(s_afe_buf);
    heap_caps_free(s_aec_out_ring);
    s_mic_buf = NULL;
    s_enc_out = NULL;
    s_afe_buf = NULL;
    s_aec_out_ring = NULL;
    s_afe_active = false;
    s_afe_feed_chunk = 0;
    return ESP_FAIL;
}

/**
 * @brief 按需打开 AFE 唤醒前端（boot 阶段由 main.c 调用，小智 App 启动时也可幂等调用）
 *
 * Why: AFE 占用 ~110KB 内部 RAM，必须在 WiFi 初始化之前打开；否则 WiFi/TLS/WS
 * 启动后内部 RAM 仅剩 ~32KB，AFE 必败。打开后保持常驻，会话重启时不再关闭。
 * 打开后切换到 MR 重采样器（AEC 全双工）。
 * @return ESP_OK AFE 就绪；ESP_FAIL 不可用（退化为 manual）
 */
esp_err_t service_voice_wake_open_deferred(void)
{
#if !CONFIG_BOARD_HAS_MIC
    /* Why: 与 service_voice_init 同一门控；service_xiaozhi 激活路径会调用本函数，
     * 无 mic 的板直接拒绝，防止 AFE/WakeNet 被旁路打开 */
    return ESP_ERR_NOT_SUPPORTED;
#endif

    if (s_afe_active) {
        return ESP_OK;
    }

    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    ESP_LOGI(TAG, "AFE 按需打开: 空闲 %u 字节, 最大连续块 %u 字节",
             (unsigned)internal_free, (unsigned)largest_block);

    if (service_voice_wake_open() != ESP_OK) {
        ESP_LOGW(TAG, "AFE 不可用，仅支持 manual 按住说话");
        return ESP_FAIL;
    }

    s_afe_feed_chunk = service_voice_wake_get_feed_chunk();

    /* 先备好 MR 重采样器再发布 s_afe_active：反转顺序会让 task_audio 拿旧
     * mono 重采样器跑 MR 输入（重开窗口期 Invalid parameter 红日志刷屏） */
    if (!service_voice_open_resampler(2)) {
        ESP_LOGW(TAG, "MR resampler open failed, fallback to manual");
        s_afe_feed_chunk = 0;
        service_voice_wake_close();
        return ESP_FAIL;
    }

    s_afe_active = true;
    ESP_LOGI(TAG, "AFE 就绪，auto 连续对话可用（唤醒词 + AEC 全双工）");
    return ESP_OK;
}

/**
 * @brief 关闭 AFE 唤醒前端（仅整机休眠/彻底关闭语音时调用）
 */
void service_voice_wake_close_deferred(void)
{
    if (!s_afe_active) {
        return;
    }
    /* 先撤活（task_audio 下一拍起不再进 AFE 路径），再等路径静默；
     * 不经静默直接 destroy 会从 Core 1 处理中段脚下抽空（真机崩溃） */
    s_afe_active = false;
    __sync_synchronize();
    for (int i = 0; i < 100 && s_afe_busy; i++) {
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    if (s_afe_busy) {
        ESP_LOGW(TAG, "afe busy timeout, force close");
    }
    service_voice_wake_close();
    s_afe_feed_chunk = 0;
    /* 切回 mono 重采样器（此刻无并发使用者，安全） */
    service_voice_open_resampler(1);
    ESP_LOGI(TAG, "AFE 已关闭，释放内部 RAM");
}

void service_voice_process(void)
{
    if (!s_initialized) {
        return;
    }
    service_voice_process_commands();
    service_voice_manage_mic();
    if (s_mic_open) {
        if (s_afe_active) {
            service_voice_process_afe_path();
        } else if (s_listening) {
            service_voice_process_manual_path();
        }
    }
}

esp_err_t service_voice_start_listen(void)
{
    if (s_cmd_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    service_voice_cmd_t cmd = {
        .type = VOICE_CMD_START_LISTEN,
    };
    if (xQueueSend(s_cmd_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void service_voice_stop_listen(void)
{
    if (s_cmd_queue == NULL) {
        return;
    }
    service_voice_cmd_t cmd = {
        .type = VOICE_CMD_STOP_LISTEN,
    };
    xQueueSend(s_cmd_queue, &cmd, 0);
}

void service_voice_enable_wake(bool enable)
{
    if (s_cmd_queue == NULL) {
        return;
    }
    service_voice_cmd_t cmd = {
        .type = VOICE_CMD_ENABLE_WAKE,
        .enable = enable,
    };
    xQueueSend(s_cmd_queue, &cmd, 0);
}

bool service_voice_is_listening(void)
{
    return s_listening;
}

bool service_voice_poll_event(service_voice_event_t *out)
{
    if (s_evt_queue == NULL || out == NULL) {
        return false;
    }
    if (xQueueReceive(s_evt_queue, out, 0) != pdTRUE) {
        return false;
    }
    if (out->type == SERVICE_VOICE_EVT_WAKE && s_wake_pending > 0) {
        __atomic_fetch_sub(&s_wake_pending, 1, __ATOMIC_ACQ_REL);
    }
    return true;
}

esp_err_t service_voice_decoder_open(uint32_t sample_rate)
{
    return service_voice_opus_decoder_open(sample_rate);
}

esp_err_t service_voice_decode_packet(const uint8_t *data, uint32_t len)
{
    return service_voice_opus_decode_to_aux(data, len);
}

void service_voice_decoder_close(void)
{
    service_voice_opus_decoder_close();
}

void service_voice_decoder_reset_phase(void)
{
    service_voice_opus_reset_phase();
}

const char *service_voice_get_wake_word(void)
{
    return service_voice_wake_get_word();
}

void service_voice_set_mic_tap(service_voice_mic_tap_cb_t cb, void *ctx, bool aec_processed)
{
    /* Trap: 先清回调再落参数，task_audio 侧始终看到一致的 (cb,ctx,aec) 三元组；
     * 单字存储原子 + 屏障防重排，切换瞬间最多丢失一个 32ms 块 */
    s_mic_tap_cb = NULL;
    __sync_synchronize();
    s_mic_tap_aec = aec_processed;
    s_mic_tap_ctx = ctx;
    __sync_synchronize();
    s_mic_tap_cb = cb;
}

bool service_voice_is_afe_active(void)
{
    return s_afe_active;
}
