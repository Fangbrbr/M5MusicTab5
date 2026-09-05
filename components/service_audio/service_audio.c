/**
 * @file service_audio.c
 * @brief 音频服务
 *
 * 主音频源为 SF2 采样合成器，辅助混音流供 TTS/MP3 使用。
 * 负责 I2S/Codec 输出、混音收口（软限幅）、音量、麦克风。
 */

#include "service_audio.h"
#include "service_audio_config.h"
#include "board_hal.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "service_power.h"
#include "service_sd.h"
#include "string.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

static const char *TAG = "service_audio";

/** @brief 最大注册音频源数量（仅 NONE + SF2） */
#define AUDIO_MAX_SOURCES       2

/** @brief 辅助混音流环形缓冲帧数（约 2s @44.1kHz）
 *
 * Why: xiaozhi TTS 下行受网络抖动与服务端生成停顿（如工具调用）影响，
 * 到帧速率会瞬时慢于实时。缓冲过小会欠载出现静音空隙（听感卡顿），
 * 故按 2s 深度吸收抖动；@PSRAM 约 345KB，成本可接受。 */
#define AUX_RING_FRAMES         (SERVICE_AUDIO_SAMPLE_RATE * 2)

/** @brief aux 抖动缓冲预充门限（约 400ms）
 *
 * Why: 达到该水位再开始出声，避免边收边放导致的碎帧卡顿；
 * 一旦缓冲排空（欠载）则重新预充，用一次短暂停顿换取平滑播放。
 * 真机实测服务端 TTS 到达速率突发明显（单 3s 窗口可低至实时 20%），
 * 旧值 250ms 会陷入“排空→预充→播 250ms→再排空”的永久断续循环
 * （听感：TTS 完全异常），加深到 400ms 提高抗突发能力。 */
#define AUX_PRIME_FRAMES        (SERVICE_AUDIO_SAMPLE_RATE * 2 / 5)

static const audio_source_ops_t *s_sources[AUDIO_MAX_SOURCES] = {NULL};
static const audio_source_ops_t *s_active_ops = NULL;
static audio_source_t            s_active_source = AUDIO_SOURCE_NONE;
static esp_codec_dev_handle_t    s_spk_codec = NULL;
static float                     s_render_buffer[SERVICE_AUDIO_FRAMES_PER_PERIOD * 2];
static int16_t                   s_i2s_buffer[SERVICE_AUDIO_FRAMES_PER_PERIOD * 2];
/* 输出级遥测（杂音定位）：对比 out peak 与 sf2 渲染峰值，区分数字域注入与模拟域畸变 */
static volatile int16_t          s_out_peak_abs = 0;
static volatile uint32_t         s_out_knee_cnt = 0;
static bool                      s_codec_opened = false;
static bool                      s_last_headphone_state = false;
/* 默认 UI 音量 70 → 经 perceptual 曲线映射 codec 83 ≈ -10 dB，安静环境合适；
 * 原线性 55 ≈ -33 dB 过小，用户反馈需手动拉到 80-90 才够用。 */
static int32_t                   s_volume = 70;
static int32_t                   s_pending_volume = 70;
static volatile bool             s_volume_pending = false;

/* 耳机插入时音量自动衰减系数：实际应用档位 = 显示档位 × 0.6（线性 -4.4 dB）。
 * 经 perceptual 曲线后高音量档衰减更明显（UI 70 → 42 → codec≈56 ≈ -16 dB），
 * 显示音量与 NVS 保持不变，拔出耳机恢复原值。听感偏大/偏小调此处系数。 */
#define SERVICE_AUDIO_HEADPHONE_VOL_SCALE  0.6f

/* Perceptual 音量曲线：UI 档位 (0-100) → ES8388 codec 档位 (0-100)
 *
 * 假设 esp_codec_dev_set_out_vol(v) 内部把 0-100 线性映射到 -60 dB ~ 0 dB
 * （消费 codec 典型范围），则每个 UI 锚点对应的系统总衰减为：
 *   UI  0 → codec   0 ≈ -60 dB（近乎静音）
 *   UI 50 → codec  67 ≈ -20 dB（舒适室内聆听）
 *   UI 70 → codec  83 ≈ -10 dB（稍大，嘈杂环境）
 *   UI 85 → codec  95 ≈  -3 dB（接近最大，留 5% 余量防削波）
 *   UI 100 → codec 100 ≈   0 dB（最大）
 * 锚点之间线性插值。曲线形状：低档每步 dB 变化大（细调静音档）、
 * 高档每步 dB 变化小（避免"越往大调节奏越粗暴"的线性通病）。 */
typedef struct {
    int32_t ui;
    int32_t codec;
} vol_anchor_t;

static const vol_anchor_t s_vol_curve[] = {
    {  0,   0 },
    { 50,  67 },
    { 70,  83 },
    { 85,  95 },
    {100, 100 },
};
#define VOL_CURVE_NUM  (sizeof(s_vol_curve) / sizeof(s_vol_curve[0]))

/** @brief UI 音量 0-100 → codec 音量 0-100，锚点间线性插值
 *
 * Why 展开显式 if-else 链而非 for 循环：避免 GCC -Wreturn-type 对
 * "循环内 return + 末尾兜底" 误报（锚点数固定 5，完全展开成本可忽略）。 */
static int32_t service_audio_ui_to_codec(int32_t ui)
{
    if (ui <= s_vol_curve[0].ui) {
        return s_vol_curve[0].codec;
    }
    if (ui <= s_vol_curve[1].ui) {
        return s_vol_curve[0].codec + (s_vol_curve[1].codec - s_vol_curve[0].codec) *
               (ui - s_vol_curve[0].ui) / (s_vol_curve[1].ui - s_vol_curve[0].ui);
    }
    if (ui <= s_vol_curve[2].ui) {
        return s_vol_curve[1].codec + (s_vol_curve[2].codec - s_vol_curve[1].codec) *
               (ui - s_vol_curve[1].ui) / (s_vol_curve[2].ui - s_vol_curve[1].ui);
    }
    if (ui <= s_vol_curve[3].ui) {
        return s_vol_curve[2].codec + (s_vol_curve[3].codec - s_vol_curve[2].codec) *
               (ui - s_vol_curve[2].ui) / (s_vol_curve[3].ui - s_vol_curve[2].ui);
    }
    return s_vol_curve[3].codec + (s_vol_curve[4].codec - s_vol_curve[3].codec) *
           (ui - s_vol_curve[3].ui) / (s_vol_curve[4].ui - s_vol_curve[3].ui);
}

/* 辅助混音流（TTS/MP3 第二发声通道）：SPSC 环形缓冲，PSRAM 分配。
 * s_aux_head 仅生产者写，s_aux_tail 仅消费者（Core 1 渲染）写，标量读写天然原子。
 *
 * AUX_MIX_GAIN：TTS 语音是响度归一化的近满幅信号，而 SF2 采样合成经包络/
 * InitialAttenuation 后平均电平远低于满幅。单位增益直接混入会让小智语音
 * 在同一音量档位下比演奏类 App 响很多。旧值 0.2 真机听感过轻，用户
 * 反馈 TTS “一丁点滋滋声”，提到 0.5（约 -6dB），过载由混音出口软限幅兑底。 */
#define AUX_MIX_GAIN    0.5f
static int16_t                  *s_aux_ring = NULL;
static volatile uint32_t         s_aux_head = 0;
static volatile uint32_t         s_aux_tail = 0;
/* 抖动缓冲预充标志（仅消费者 Core 1 渲染访问）：true 表示正在攒够门限再出声 */
static bool                      s_aux_priming = true;
/* 流结束（tts_stop）一次性解除预充请求：xz_task（Core 0）置位、消费者
 * Core 1 应用并自清，保持 s_aux_priming 单写者 */
static volatile bool             s_aux_eos_pending = false;

/* 临时遥测：auxstruct 写入/丢弃/混出/预充计数，3s 窗口有活动即打印，
 * 定位 TTS 播放异常（滋滋声/无声）是写端丢帧还是混端欠载 */
static uint32_t s_aux_dbg_wr = 0;
static uint32_t s_aux_dbg_drop = 0;
static uint32_t s_aux_dbg_mix = 0;
static uint32_t s_aux_dbg_prime = 0;
static int64_t  s_aux_dbg_us = 0;

/* 麦克风关闭后扬声器恢复延迟到 audio 任务执行，避免在 task_comm 上下文中
 * 因 DMA 内存不足导致 I2S 重配置失败而崩溃。 */
static bool                      s_spk_resume_pending = false;

/* 重开扬声器所需的最小 DMA-capable 内部 RAM（字节）。
 * I2S 实际分配约 3-4 KB（256 帧×2ch×2B×6 desc + 描述符）。
 * AFE 打开后内部 RAM 最大连续块约 5 KB，DMA 可用块更小（约 4 KB），
 * 故阈值设为 4 KB 留安全余量。 */
#define SERVICE_AUDIO_MIN_DMA_BYTES 4096

/** @brief AEC 参考信号环形缓冲帧数（约 1s @44.1kHz 单声道，约 88KB PSRAM） */
#define AEC_REF_RING_FRAMES     SERVICE_AUDIO_SAMPLE_RATE

static const audio_source_ops_t *service_audio_find_ops(audio_source_t source);
static esp_err_t service_audio_codec_open(void);
static void service_audio_headphone_route_update(void);
static void service_audio_apply_pending_volume(void);
static int32_t service_audio_effective_volume(int32_t ui);
static void service_audio_power_event_cb(service_power_event_t evt, void *user_data);
static inline float service_audio_soft_limit(float x);
static void service_audio_mix_aux(float *buffer_lr, uint32_t frames);
static void service_audio_push_ref(const int16_t *stereo_lr, uint32_t frames);

static SemaphoreHandle_t        s_codec_mutex = NULL;
static esp_codec_dev_handle_t   s_mic_codec = NULL;
static volatile bool              s_mic_recording = false;
static uint8_t                   s_mic_channels = 1;
static uint32_t                  s_mic_sample_rate = 0;
/* 当前 mic 是否处于全双工模式：采样率/声道数与扬声器输出一致，
 * 开 mic 时不关扬声器，渲染任务继续运行。 */
static volatile bool              s_mic_full_duplex = false;

/* AEC 参考信号（混音出口下混 mono @44.1kHz）：SPSC 环形缓冲。
 * s_ref_head 仅 Core 1 渲染任务写，s_ref_tail 仅 xz_task 读。 */
static int16_t                  *s_ref_ring = NULL;
static volatile uint32_t         s_ref_head = 0;
static volatile uint32_t         s_ref_tail = 0;
static volatile bool             s_ref_enabled = false;

esp_err_t service_audio_init(void)
{
    if (s_spk_codec != NULL) {
        return ESP_OK;
    }

    if (s_codec_mutex == NULL) {
        s_codec_mutex = xSemaphoreCreateMutex();
        if (s_codec_mutex == NULL) {
            ESP_LOGE(TAG, "codec mutex create failed");
            return ESP_ERR_NO_MEM;
        }
    }

    board_i2c_init();

    s_spk_codec = board_audio_speaker_codec_init();
    if (s_spk_codec == NULL) {
        ESP_LOGE(TAG, "speaker codec init failed");
        return ESP_FAIL;
    }

    esp_codec_dev_set_out_vol(s_spk_codec,
                              service_audio_ui_to_codec(service_audio_effective_volume(s_volume)));

    /* 根据耳机插入状态初始化扬声器/耳机路由 */
    bool headphone_connected = service_power_is_headphone_connected();
    s_last_headphone_state = headphone_connected;
    ESP_LOGI(TAG, "headphone %s, speaker %s",
             headphone_connected ? "connected" : "disconnected",
             headphone_connected ? "off" : "on");
    esp_err_t ret_route = board_audio_speaker_pa_set(!headphone_connected);
    if (ret_route != ESP_OK) {
        ESP_LOGW(TAG, "speaker route init failed: %d", ret_route);
    }

    /* 音量设置统一在 Core 1 音频任务中应用，避免 Core 0 直接访问 codec 造成时序抖动。 */
    s_pending_volume = s_volume;
    s_volume_pending = true;

    memset(s_sources, 0, sizeof(s_sources));
    s_active_ops = NULL;
    s_active_source = AUDIO_SOURCE_NONE;
    s_codec_opened = false;

    /* 辅助混音流环形缓冲：PSRAM 分配，失败时 aux_write 静默丢弃（仅警告） */
    if (s_aux_ring == NULL) {
        s_aux_ring = heap_caps_malloc(AUX_RING_FRAMES * 2 * sizeof(int16_t), MALLOC_CAP_SPIRAM);
        if (s_aux_ring == NULL) {
            ESP_LOGW(TAG, "aux mix ring alloc failed, TTS/MP3 stream disabled");
        }
    }

    /* 注册电源事件回调，耳机插入/拔出由 service_power 实时通知，
     * audio 任务不再轮询 IO 扩展器。 */
    service_power_register_event_callback(service_audio_power_event_cb, NULL);

    ESP_LOGI(TAG, "audio service init, sample_rate=%d, frames=%d",
             SERVICE_AUDIO_SAMPLE_RATE, SERVICE_AUDIO_FRAMES_PER_PERIOD);

    /* 在 AFE 打开前强制打开 codec，分配 DMA 描述符。
     * Why: AFE 打开后内部 RAM 从 ~126KB 降到 ~7KB，DMA 分配必败。
     * DMA 描述符一旦分配不会被释放，AFE 用剩余空间即可。 */
    service_audio_codec_open();

    return ESP_OK;
}

esp_err_t service_audio_register_source(const audio_source_ops_t *ops)
{
    if (ops == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (ops->source <= AUDIO_SOURCE_NONE || ops->source >= AUDIO_SOURCE_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_sources[ops->source] != NULL) {
        ESP_LOGW(TAG, "source %d already registered, overwriting", ops->source);
    }

    s_sources[ops->source] = ops;
    ESP_LOGI(TAG, "registered audio source: %s", ops->name ? ops->name : "unknown");

    return ESP_OK;
}

esp_err_t service_audio_activate_sf2(void)
{
    if (s_active_source == AUDIO_SOURCE_SF2) {
        return ESP_OK;
    }

    /* 反初始化旧源（理论上不会发生） */
    if (s_active_ops != NULL && s_active_ops->deinit != NULL) {
        ESP_LOGI(TAG, "deinit source: %s", s_active_ops->name ? s_active_ops->name : "unknown");
        s_active_ops->deinit();
    }

    s_active_ops = NULL;
    s_active_source = AUDIO_SOURCE_NONE;

    /* 初始化 SF2 采样合成器 */
    const audio_source_ops_t *new_ops = service_audio_find_ops(AUDIO_SOURCE_SF2);
    if (new_ops == NULL) {
        ESP_LOGE(TAG, "SF2 source not registered");
        return ESP_ERR_NOT_FOUND;
    }

    if (new_ops->init != NULL) {
        ESP_LOGI(TAG, "init source: %s", new_ops->name ? new_ops->name : "unknown");
        esp_err_t ret = new_ops->init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "source init failed: %d", ret);
            return ret;
        }
    }

    s_active_ops = new_ops;
    s_active_source = AUDIO_SOURCE_SF2;
    return ESP_OK;
}

audio_source_t service_audio_get_active_source(void)
{
    return s_active_source;
}

esp_err_t service_audio_deactivate_sf2(void)
{
    if (s_active_source != AUDIO_SOURCE_SF2) {
        return ESP_ERR_INVALID_STATE;
    }

    /* 不调 s_active_ops->deinit()：保留 SF2 源注册与音色缓存，
     * 仅置 active 为 NONE 使渲染路径静音；恢复走 activate_sf2 轻量重入 */
    s_active_ops = NULL;
    s_active_source = AUDIO_SOURCE_NONE;
    ESP_LOGI(TAG, "deactivate source (render muted)");
    return ESP_OK;
}

void service_audio_process(void)
{
    /* 全双工路径（mic 与扬声器同采样率）：I2S TX/RX 并行，渲染继续。
     * 半双工路径（旧 16kHz mic）：扬声器 I2S 被占用，跳过渲染。 */
    if (s_mic_recording && !s_mic_full_duplex) {
        /* 半双工录音期 TX 停摆、本函数空转：task_audio 节奏本由 I2S TX 阻塞
         * 写定速，此时须主动让出 Core 1，否则最高优先级空转饿死同核任务 */
        vTaskDelay(pdMS_TO_TICKS(2));
        return;
    }

    /* 扬声器打开与音量变更走配置锁；渲染/写操作放开锁，避免与 mic 读互相阻塞。 */
    if (s_codec_mutex != NULL &&
        xSemaphoreTake(s_codec_mutex, portMAX_DELAY) == pdTRUE) {
        if (!s_codec_opened) {
            service_audio_codec_open();
        }
        service_audio_apply_pending_volume();
        xSemaphoreGive(s_codec_mutex);
    }

    /* 渲染当前源（float 域），混入辅助 PCM 流（TTS/MP3 第二发声通道）。
     * 无活跃源时主声道保持静音，但 aux 仍须照常输出：MP3 独占播放期
     * SF2 被 deactivate（active=NONE），若在此短路则整个 I2S 输出被跳过，
     * aux 的 MP3 PCM 也不会有声音。 */
    memset(s_render_buffer, 0, sizeof(s_render_buffer));
    if (s_active_ops != NULL && s_active_ops->render_stereo != NULL) {
        s_active_ops->render_stereo(s_render_buffer, SERVICE_AUDIO_FRAMES_PER_PERIOD);
    }
    service_audio_mix_aux(s_render_buffer, SERVICE_AUDIO_FRAMES_PER_PERIOD);

    /* 软限幅收口后转 int16 输出，顺带统计峰值/拐点次数 */
    int16_t period_peak = 0;
    uint32_t period_knee = 0;
    for (uint32_t i = 0; i < SERVICE_AUDIO_FRAMES_PER_PERIOD * 2; i++) {
        float limited = service_audio_soft_limit(s_render_buffer[i]);
        float mag = (s_render_buffer[i] < 0.0f) ? -s_render_buffer[i] : s_render_buffer[i];
        if (mag > 0.75f) {
            period_knee++;
        }
        s_i2s_buffer[i] = (int16_t)(limited * 32767.0f);
        int16_t a = (s_i2s_buffer[i] < 0) ? (int16_t)(-s_i2s_buffer[i]) : s_i2s_buffer[i];
        if (a > period_peak) {
            period_peak = a;
        }
    }
    if (period_peak > s_out_peak_abs) {
        s_out_peak_abs = period_peak;
    }
    s_out_knee_cnt += period_knee;

    if (s_spk_codec != NULL && s_codec_opened) {
        int ret = esp_codec_dev_write(s_spk_codec, s_i2s_buffer,
                                      SERVICE_AUDIO_FRAMES_PER_PERIOD * 2 * sizeof(int16_t));
        if (ret < 0) {
            ESP_LOGE(TAG, "codec write failed: %d", ret);
        }
    }

    /* 全双工 AEC：把混音出口下混为 mono，作为参考信号写入环形缓冲 */
    if (s_ref_enabled) {
        service_audio_push_ref(s_i2s_buffer, SERVICE_AUDIO_FRAMES_PER_PERIOD);
    }
}

uint32_t service_audio_get_sample_rate(void)
{
    return SERVICE_AUDIO_SAMPLE_RATE;
}

uint32_t service_audio_get_frames_per_period(void)
{
    return SERVICE_AUDIO_FRAMES_PER_PERIOD;
}

esp_err_t service_audio_set_volume(int32_t volume)
{
    if (volume < 0) {
        volume = 0;
    } else if (volume > 100) {
        volume = 100;
    }

    s_volume = volume;
    s_pending_volume = volume;
    s_volume_pending = true;

    return ESP_OK;
}

/* 实际应用的 UI 音量档位：耳机插入时按系数衰减，显示音量（s_volume）不变。
 * 无耳机检测硬件的板（service_power stub 返回 false）恒为不衰减。 */
static int32_t service_audio_effective_volume(int32_t ui)
{
    if (service_power_is_headphone_connected()) {
        ui = (int32_t)(ui * SERVICE_AUDIO_HEADPHONE_VOL_SCALE);
    }
    return ui;
}

static void service_audio_apply_pending_volume(void)
{
    if (!s_volume_pending) {
        return;
    }

    s_volume_pending = false;

    if (s_spk_codec == NULL) {
        return;
    }

    int ret = esp_codec_dev_set_out_vol(s_spk_codec,
                                        service_audio_ui_to_codec(
                                            service_audio_effective_volume(s_pending_volume)));
    if (ret != 0) {
        ESP_LOGW(TAG, "set volume failed: %d", ret);
    }
}

/* 软限幅：3/4 满幅拐点，超出部分经有理函数渐进压缩到满幅，拐点一阶连续。
 * 混音出口唯一限幅点，替代各源各自的硬裁剪，防复音/混音过载爆音。 */
static inline float service_audio_soft_limit(float x)
{
    const float knee = 0.75f;
    float mag = (x < 0.0f) ? -x : x;
    if (mag <= knee) {
        return x;
    }
    float over = mag - knee;
    mag = knee + over * (1.0f - knee) / (over + (1.0f - knee));
    return (x < 0.0f) ? -mag : mag;
}

/* 消费辅助混音流：按可用量混入 float 渲染缓冲，无数据时零开销跳过 */
static void service_audio_mix_aux(float *buffer_lr, uint32_t frames)
{
    if (s_aux_ring == NULL) {
        return;
    }

    /* 流结束一次性解除预充：tts_stop 意味着不会再有新数据，尾音不足 400ms
     * 门限也必须立即排出——否则尾音永久卡门、跨轮残留成下一句开头的插播 */
    if (s_aux_eos_pending) {
        s_aux_eos_pending = false;
        s_aux_priming = false;
    }

    uint32_t tail = s_aux_tail;
    uint32_t head = s_aux_head;   /* 快照，生产者可能并发推进 */
    uint32_t avail = (head >= tail) ? (head - tail) : (AUX_RING_FRAMES - (tail - head));

    /* 临时遥测：必须放在预充早退之前——否则预充/无声期窗口被无限拉长且
     * 完全无输出，TTS 无声期成诊断盲区（真机实测）。固定 3s 窗口。 */
    if (s_aux_dbg_us == 0) {
        s_aux_dbg_us = esp_timer_get_time();
    } else if ((esp_timer_get_time() - s_aux_dbg_us) >= 3000000) {
        if (s_aux_dbg_wr > 0 || s_aux_dbg_mix > 0 || s_aux_dbg_drop > 0 || s_aux_priming) {
            ESP_LOGD(TAG, "[dbg] aux/3s wr=%lu drop=%lu mix=%lu prime=%lu avail=%lu priming=%d",
                     (unsigned long)s_aux_dbg_wr, (unsigned long)s_aux_dbg_drop,
                     (unsigned long)s_aux_dbg_mix, (unsigned long)s_aux_dbg_prime,
                     (unsigned long)avail, (int)s_aux_priming);
        }
        s_aux_dbg_wr = 0;
        s_aux_dbg_drop = 0;
        s_aux_dbg_mix = 0;
        s_aux_dbg_prime = 0;
        s_aux_dbg_us = esp_timer_get_time();
    }

    /* 预充门限：攒够 AUX_PRIME_FRAMES 再出声，避免网络抖动/服务端停顿造成碎帧卡顿 */
    if (s_aux_priming) {
        if (avail < AUX_PRIME_FRAMES) {
            return;
        }
        s_aux_priming = false;
        s_aux_dbg_prime++;
    }

    uint32_t n = (frames < avail) ? frames : avail;

    for (uint32_t i = 0; i < n; i++) {
        uint32_t pos = (tail + i) % AUX_RING_FRAMES;
        buffer_lr[i * 2]     += (float)s_aux_ring[pos * 2]     * (AUX_MIX_GAIN / 32768.0f);
        buffer_lr[i * 2 + 1] += (float)s_aux_ring[pos * 2 + 1] * (AUX_MIX_GAIN / 32768.0f);
    }
    s_aux_tail = (tail + n) % AUX_RING_FRAMES;
    s_aux_dbg_mix += n;

    /* 欠载（本周期未取满）：缓冲已排空，重新预充避免持续碎帧 */
    if (n < frames) {
        s_aux_priming = true;
    }
}

int32_t service_audio_get_volume(void)
{
    return s_volume;
}

uint32_t service_audio_aux_write(const int16_t *data, uint32_t frames)
{
    if (data == NULL || s_aux_ring == NULL || frames == 0) {
        return 0;
    }

    uint32_t head = s_aux_head;
    uint32_t tail = s_aux_tail;
    uint32_t used = (head >= tail) ? (head - tail) : (AUX_RING_FRAMES - (tail - head));
    /* 预留 1 帧区分满/空；缓冲已满时截断本次写入（生产者应控制水位） */
    uint32_t free_frames = AUX_RING_FRAMES - used - 1;
    if (frames > free_frames) {
        s_aux_dbg_drop += (frames - free_frames);
        frames = free_frames;
    }

    for (uint32_t i = 0; i < frames; i++) {
        uint32_t pos = (head + i) % AUX_RING_FRAMES;
        s_aux_ring[pos * 2]     = data[i * 2];
        s_aux_ring[pos * 2 + 1] = data[i * 2 + 1];
    }
    /* 数据先行落笔，再发布写位置（SPSC 无锁协议） */
    __sync_synchronize();
    s_aux_head = (head + frames) % AUX_RING_FRAMES;
    s_aux_dbg_wr += frames;

    return frames;
}

uint32_t service_audio_aux_free_frames(void)
{
    if (s_aux_ring == NULL) {
        return 0;
    }
    uint32_t head = s_aux_head;
    uint32_t tail = s_aux_tail;
    uint32_t used = (head >= tail) ? (head - tail) : (AUX_RING_FRAMES - (tail - head));
    /* 预留 1 帧区分满/空，与 aux_write 判满口径一致 */
    return AUX_RING_FRAMES - used - 1;
}

/**
 * @brief 查询辅助混音流是否已排空（生产/消费指针重合）
 *
 * Why: 小智 auto 续听需等 TTS 尾巴播完再开 mic，防残音被掐或被当语音上传。
 * Trap: 仅判指针重合；预充（priming）状态下缓冲可能积压未播，调用方需配合
 * 超时兜底，防对端停顿导致永不排空。
 */
bool service_audio_aux_is_idle(void)
{
    return s_aux_ring == NULL || s_aux_head == s_aux_tail;
}

void service_audio_get_out_stats(int16_t *peak_abs, uint32_t *knee_cnt)
{
    if (peak_abs != NULL) {
        *peak_abs = s_out_peak_abs;
        s_out_peak_abs = 0;
    }
    if (knee_cnt != NULL) {
        *knee_cnt = s_out_knee_cnt;
        s_out_knee_cnt = 0;
    }
}

void service_audio_aux_clear(void)
{
    s_aux_head = 0;
    s_aux_tail = 0;
    s_aux_priming = true;
    s_aux_eos_pending = false;
}

void service_audio_aux_end_of_stream(void)
{
    /* 仅置位，消费者在 Core 1 上下文应用（见 mix_aux 顶部），跨核无撕裂 */
    s_aux_eos_pending = true;
}

/* ---------------------------------------------------------------------------
 * AEC 参考信号（全双工路径）
 * ------------------------------------------------------------------------- */

/**
 * @brief 把混音出口立体声下混为单声道并写入参考环形缓冲
 */
static void service_audio_push_ref(const int16_t *stereo_lr, uint32_t frames)
{
    if (s_ref_ring == NULL) {
        return;
    }

    uint32_t head = s_ref_head;
    uint32_t tail = s_ref_tail;
    uint32_t used = (head >= tail) ? (head - tail) : (AEC_REF_RING_FRAMES - (tail - head));
    uint32_t free_frames = AEC_REF_RING_FRAMES - used - 1;
    if (frames > free_frames) {
        frames = free_frames;
    }

    for (uint32_t i = 0; i < frames; i++) {
        int32_t l = stereo_lr[i * 2];
        int32_t r = stereo_lr[i * 2 + 1];
        int16_t mono = (int16_t)((l + r) >> 1);
        uint32_t pos = (head + i) % AEC_REF_RING_FRAMES;
        s_ref_ring[pos] = mono;
    }
    __sync_synchronize();
    s_ref_head = (head + frames) % AEC_REF_RING_FRAMES;
}

void service_audio_aec_ref_start(void)
{
    if (s_ref_ring == NULL) {
        s_ref_ring = heap_caps_malloc(AEC_REF_RING_FRAMES * sizeof(int16_t), MALLOC_CAP_SPIRAM);
        if (s_ref_ring == NULL) {
            ESP_LOGE(TAG, "AEC ref ring alloc failed");
            return;
        }
    }
    s_ref_head = 0;
    s_ref_tail = 0;
    s_ref_enabled = true;
    ESP_LOGI(TAG, "AEC reference capture started");
}

void service_audio_aec_ref_stop(void)
{
    s_ref_enabled = false;
}

uint32_t service_audio_aec_ref_read(int16_t *buffer, uint32_t frames)
{
    if (buffer == NULL || s_ref_ring == NULL) {
        return 0;
    }

    uint32_t head = s_ref_head;
    uint32_t tail = s_ref_tail;
    uint32_t avail = (head >= tail) ? (head - tail) : (AEC_REF_RING_FRAMES - (tail - head));
    if (frames > avail) {
        frames = avail;
    }

    for (uint32_t i = 0; i < frames; i++) {
        buffer[i] = s_ref_ring[(tail + i) % AEC_REF_RING_FRAMES];
    }
    __sync_synchronize();
    s_ref_tail = (tail + frames) % AEC_REF_RING_FRAMES;
    return frames;
}

uint32_t service_audio_aec_ref_available(void)
{
    if (s_ref_ring == NULL) {
        return 0;
    }
    uint32_t head = s_ref_head;
    uint32_t tail = s_ref_tail;
    return (head >= tail) ? (head - tail) : (AEC_REF_RING_FRAMES - (tail - head));
}

esp_err_t service_audio_mic_open(uint32_t sample_rate, uint8_t channels)
{
    if (s_codec_mutex == NULL || s_spk_codec == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_codec_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_mic_recording) {
        xSemaphoreGive(s_codec_mutex);
        return ESP_OK;
    }

    s_mic_channels = (channels == 0) ? 1 : channels;
    s_mic_sample_rate = sample_rate;
    /* 当 mic 参数与扬声器输出一致时，走全双工路径：I2S TX/RX 共享同一时钟，
     * 无需关闭扬声器，渲染任务继续运行。 */
    s_mic_full_duplex = (s_mic_sample_rate == SERVICE_AUDIO_SAMPLE_RATE && s_mic_channels == 1);

    if (!s_mic_full_duplex) {
        /* 半双工：临时关闭扬声器，释放 I2S 时钟给麦克风 */
        if (s_codec_opened) {
            esp_codec_dev_close(s_spk_codec);
            s_codec_opened = false;
        }
    }

    if (s_mic_codec == NULL) {
        s_mic_codec = board_audio_mic_codec_init();
        if (s_mic_codec == NULL) {
            ESP_LOGE(TAG, "mic codec init failed");
            xSemaphoreGive(s_codec_mutex);
            return ESP_FAIL;
        }
    }

    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = s_mic_channels,
        .sample_rate = sample_rate,
        .mclk_multiple = 0,
    };

    int ret = esp_codec_dev_open(s_mic_codec, &fs);
    if (ret != 0) {
        ESP_LOGE(TAG, "mic codec open failed: %d", ret);
        s_mic_full_duplex = false;
        xSemaphoreGive(s_codec_mutex);
        return ESP_FAIL;
    }

    /* 必须在 esp_codec_dev_open 之后设置增益。
     * esp_codec_dev_open 内部会调用 _update_codec_setting()，若此前未设置有效增益
     * 会把 mic_gain 初始值 0 dB 写入 ES7210，导致录音电平极低。
     * 32.0f 经 get_db() 3dB 步进量化实际落 30dB（上限 37.5dB，官方出厂 demo 录音即顶满 37.5dB）。 */
    ret = esp_codec_dev_set_in_gain(s_mic_codec, 32.0f);
    if (ret != 0) {
        ESP_LOGW(TAG, "mic set_in_gain failed: %d", ret);
    }

    s_mic_recording = true;

    if (s_mic_full_duplex) {
        service_audio_aec_ref_start();
    }

    xSemaphoreGive(s_codec_mutex);

    ESP_LOGI(TAG, "mic opened: sr=%lu, ch=%u, full_duplex=%d",
             (unsigned long)sample_rate, s_mic_channels, (int)s_mic_full_duplex);
    return ESP_OK;
}

int32_t service_audio_mic_read(int16_t *buffer, uint32_t frames)
{
    if (!s_mic_recording || s_mic_codec == NULL || buffer == NULL) {
        return -1;
    }

    int bytes = (int)(frames * s_mic_channels * sizeof(int16_t));
    int ret = esp_codec_dev_read(s_mic_codec, buffer, bytes);
    if (ret < 0) {
        ESP_LOGW(TAG, "mic read error: %d", ret);
        return ret;
    }

    /* esp_codec_dev_read 返回 0 表示驱动读取成功，但该 API 不返回实际字节数。
     * 底层 I2S 在成功时通常已填满请求长度；这里按请求帧数返回，避免始终读到 0 帧。
     * 请求长度应保持小于等于 I2S DMA 缓冲（当前 256 帧），由调用方控制。 */
    return (int32_t)frames;
}

void service_audio_mic_close(void)
{
    if (s_codec_mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(s_codec_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }

    bool full_duplex = s_mic_full_duplex;

    if (s_mic_codec != NULL) {
        esp_codec_dev_close(s_mic_codec);
    }

    s_mic_recording = false;
    s_mic_full_duplex = false;
    s_mic_sample_rate = 0;

    if (full_duplex) {
        service_audio_aec_ref_stop();
    }

    /* 半双工场景：扬声器恢复不在这里同步执行，避免 task_comm 上下文因 DMA 内存
     * 不足导致 I2S 重配置失败。全双工路径扬声器从未关闭，无需恢复。 */
    if (!full_duplex && s_spk_codec != NULL) {
        s_codec_opened = false;
        s_spk_resume_pending = true;
    }

    xSemaphoreGive(s_codec_mutex);

    ESP_LOGI(TAG, "mic closed%s", full_duplex ? " (full_duplex)" : ", speaker resume pending");
}

bool service_audio_mic_is_open(void)
{
    return s_mic_recording;
}

esp_err_t service_audio_mic_set_gain(float gain_db)
{
    if (s_mic_codec == NULL || !s_mic_recording) {
        return ESP_ERR_INVALID_STATE;
    }
    int ret = esp_codec_dev_set_in_gain(s_mic_codec, gain_db);
    return (ret == 0) ? ESP_OK : ESP_FAIL;
}

static const audio_source_ops_t *service_audio_find_ops(audio_source_t source)
{
    if (source != AUDIO_SOURCE_SF2) {
        return NULL;
    }

    return s_sources[source];
}

static void service_audio_headphone_route_update(void)
{
    bool headphone_connected = service_power_is_headphone_connected();

    if (headphone_connected == s_last_headphone_state) {
        return;
    }

    s_last_headphone_state = headphone_connected;

    /* 插入耳机时关闭扬声器功放，拔出时重新开启 */
    ESP_LOGI(TAG, "headphone %s, speaker %s",
             headphone_connected ? "connected" : "disconnected",
             headphone_connected ? "off" : "on");

    esp_err_t ret = board_audio_speaker_pa_set(!headphone_connected);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "speaker route failed: %d", ret);
    }

    /* 耳机音量自动衰减策略：插入/拔出后重应用当前音量，实际输出按耳机状态缩放
     * （显示音量不变）。复用 Core 1 pending 机制，事件上下文不直接写 codec */
    s_pending_volume = s_volume;
    s_volume_pending = true;
}

static void service_audio_power_event_cb(service_power_event_t evt, void *user_data)
{
    (void)user_data;

    switch (evt) {
        case SERVICE_POWER_EVT_HEADPHONE_INSERT:
        case SERVICE_POWER_EVT_HEADPHONE_REMOVE:
            service_audio_headphone_route_update();
            break;
        default:
            break;
    }
}

static esp_err_t service_audio_codec_open(void)
{
    if (s_spk_codec == NULL || s_codec_opened) {
        return ESP_OK;
    }

    /* I2S 重配置需要分配 DMA 描述符与缓冲，若 DMA-capable 内部 RAM 不足，
     * 底层驱动在失败路径中可能访问未初始化的描述符导致 panic。
     * 这里先做保守检查，不足时让 audio 任务下一周期重试。 */
    size_t dma_free = heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (dma_free < SERVICE_AUDIO_MIN_DMA_BYTES) {
        static uint32_t s_last_dma_warn_ms = 0;
        uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now_ms - s_last_dma_warn_ms >= 1000) {
            s_last_dma_warn_ms = now_ms;
            ESP_LOGW(TAG, "insufficient DMA memory (%u bytes), defer speaker open", (unsigned)dma_free);
        }
        return ESP_FAIL;
    }

    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 2,
        .sample_rate = SERVICE_AUDIO_SAMPLE_RATE,
        .mclk_multiple = 0,
    };

    int ret = esp_codec_dev_open(s_spk_codec, &fs);
    if (ret != 0) {
        ESP_LOGE(TAG, "codec open failed: %d", ret);
        return ESP_FAIL;
    }

    s_codec_opened = true;
    s_spk_resume_pending = false;
    return ESP_OK;
}
