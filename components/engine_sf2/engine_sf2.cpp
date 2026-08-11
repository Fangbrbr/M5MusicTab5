/**
 * @file engine_sf2.cpp
 * @brief SF2 采样合成引擎 C API 实现（适配层）
 *
 * 封装上游 SF2Sampler 的 SF2Parser / Synth，对外暴露纯 C 接口并注册为
 * service_audio 的音频源。vendor 代码位于 upstream/，经 shim/ 阴影层
 * 零修改编译（除 UPSTREAM_PATCHES.md 登记的补丁外）。
 *
 * 线程模型：MIDI 回调运行于 task_comm（Core 0），渲染运行于
 * task_audio（Core 1），所有 Synth 入口由 s_sf2_mutex 递归互斥锁保护。
 */

#include "engine_sf2.h"
#include "service_audio.h"
#include "engine_midi.h"
#include "config.h"
#include "synth.h"
#include "SF2Parser.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_cpu.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "math.h"
#include "string.h"
#include "stdio.h"
#include "dirent.h"
#include "strings.h"

static const char *TAG = "engine_sf2";

/** @brief 主增益（float 混音域），限幅收口在 service_audio
 *
 * Why 3.0: volume_scaler = 1/√48 ≈ 0.144 (-16.8 dB) 按 48 声部 RMS 归一化
 * 过于保守（实际演奏极少 10+ 声部同时满幅），SF2 InitialAttenuation 对钢琴音色
 * 再吃 ~10 dB，两级叠加导致 MIDI 触发音量整体过小（用户反馈需开到 codec
 * 音量 80-90 才合适）。×3.0 补偿 +9.5 dB，单音 vel=127 仍距软限幅拐点 0.75
 * 有 7+ dB 余量，20 声部和弦进入软限幅由 service_audio_soft_limit 渐进压缩兜底，
 * 不会硬削波；同时修复 TTS 混音比例（AUX_MIX_GAIN=0.5 原设计是"SF2 响而 TTS
 * 不压过"，当前 SF2 过小导致 TTS 相对过响，boost 后两者回归设计平衡）。 */
#define SF2_MASTER_GAIN 3.0f

static SF2Parser           s_parser("");
static Synth              *s_synth = nullptr;
static bool                s_initialized = false;
static SemaphoreHandle_t   s_sf2_mutex = nullptr;

static engine_sf2_progress_cb_t s_progress_cb = nullptr;
static void                  *s_progress_user_data = nullptr;

static float               s_render_l[DMA_BUFFER_LEN];
static float               s_render_r[DMA_BUFFER_LEN];

/* 渲染块耗时统计常开（rdcycle 开销可忽略）：音频杂音/节拍不稳排查的
 * 唯一客观判据，max 长期超 1450us 预算即渲染被饿死 */
static uint64_t            s_render_cycles_sum = 0;
static uint32_t            s_render_cycles_max = 0;
static uint32_t            s_render_blocks = 0;
/* 输出峰值幅度（乘 master gain 后）：杂音必现时的判别判据——
 * 轻弹单音却长期接近 1.0 = 削波/增益异常；随机满幅 = 采样数据损坏 */
static float               s_render_peak_amp = 0.0f;

static void s_midi_handler(const engine_midi_event_t *evt, void *user_data);
static bool s_load_default_soundfont(void);

static esp_err_t source_init(void);
static void      source_deinit(void);
static void      source_render_stereo(float *buffer_lr, uint32_t frames);
static bool      source_is_ready(void);

static const audio_source_ops_t s_sf2_source = {
    .source = AUDIO_SOURCE_SF2,
    .name = "SF2",
    .init = source_init,
    .deinit = source_deinit,
    .render_stereo = source_render_stereo,
    .is_ready = source_is_ready,
    .load_file = engine_sf2_load_file,
};

esp_err_t engine_sf2_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    /* 上游 SF2Sampler 的 INFO 日志带多行格式且量大（解析过程逐 chunk 打印），
     * 默认只保留 WARNING 以上；调试时按 TAG 单独调回 */
    esp_log_level_set("Synth", ESP_LOG_WARN);
    esp_log_level_set("SF2Parser", ESP_LOG_WARN);

    if (s_sf2_mutex == nullptr) {
        s_sf2_mutex = xSemaphoreCreateRecursiveMutex();
        if (s_sf2_mutex == nullptr) {
            ESP_LOGE(TAG, "failed to create mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    if (s_synth == nullptr) {
        s_synth = new (std::nothrow) Synth(s_parser);
        if (s_synth == nullptr) {
            ESP_LOGE(TAG, "failed to allocate synth instance");
            return ESP_ERR_NO_MEM;
        }
    }

    esp_err_t ret = engine_midi_subscribe(
        ENGINE_MIDI_MASK_NOTE_ON |
        ENGINE_MIDI_MASK_NOTE_OFF |
        ENGINE_MIDI_MASK_CONTROL_CHANGE |
        ENGINE_MIDI_MASK_PROGRAM_CHANGE |
        ENGINE_MIDI_MASK_PITCH_BEND |
        ENGINE_MIDI_MASK_SYSEX,
        0xFFFF,
        s_midi_handler,
        nullptr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "midi subscribe failed: %d", ret);
        delete s_synth;
        s_synth = nullptr;
        return ret;
    }

    s_initialized = true;

    /* 默认音色缺失不视为初始化失败：引擎保持就绪但静默，等待显式 load_file */
    if (!s_load_default_soundfont()) {
        ESP_LOGW(TAG, "no default soundfont found, synth stays silent");
    }

    ESP_LOGI(TAG, "init ok, max_voices=%d", MAX_VOICES);
    return ESP_OK;
}

void engine_sf2_deinit(void)
{
    engine_midi_unsubscribe(s_midi_handler);
    if (s_synth != nullptr) {
        delete s_synth;
        s_synth = nullptr;
    }
    s_parser.clear();
    s_initialized = false;
}

void engine_sf2_render_stereo(float *buffer_lr, uint32_t frames)
{
    if (s_synth == nullptr || buffer_lr == nullptr || frames == 0) {
        return;
    }

    uint32_t rendered = 0;
    xSemaphoreTakeRecursive(s_sf2_mutex, portMAX_DELAY);
    while (rendered < frames) {
        uint32_t chunk = (frames - rendered) > DMA_BUFFER_LEN ? DMA_BUFFER_LEN : (frames - rendered);

        memset(s_render_l, 0, sizeof(s_render_l));
        memset(s_render_r, 0, sizeof(s_render_r));

        /* 控制率推进（LFO/portamento/voice 打分），上游在 1ms 控制任务中调用，
         * 此处对齐渲染块率（1.45ms） */
        s_synth->updateScores();

        uint32_t c0 = esp_cpu_get_cycle_count();
        s_synth->renderLRBlock(s_render_l, s_render_r);
        uint32_t cycles = esp_cpu_get_cycle_count() - c0;
        s_render_cycles_sum += cycles;
        if (cycles > s_render_cycles_max) {
            s_render_cycles_max = cycles;
        }
        s_render_blocks++;

        for (uint32_t i = 0; i < chunk; ++i) {
            float l = s_render_l[i] * SF2_MASTER_GAIN;
            float r = s_render_r[i] * SF2_MASTER_GAIN;
            buffer_lr[(rendered + i) * 2 + 0] = l;
            buffer_lr[(rendered + i) * 2 + 1] = r;
            float al = (l < 0.0f) ? -l : l;
            float ar = (r < 0.0f) ? -r : r;
            if (al > s_render_peak_amp) s_render_peak_amp = al;
            if (ar > s_render_peak_amp) s_render_peak_amp = ar;
        }

        rendered += chunk;
    }
    xSemaphoreGiveRecursive(s_sf2_mutex);
}

bool engine_sf2_is_ready(void)
{
    return s_initialized && s_synth != nullptr;
}

void engine_sf2_set_progress_callback(engine_sf2_progress_cb_t cb, void *user_data)
{
    s_progress_cb = cb;
    s_progress_user_data = user_data;
}

bool engine_sf2_load_file(const char *path)
{
    if (s_synth == nullptr || path == nullptr) {
        return false;
    }

    bool ok;
    xSemaphoreTakeRecursive(s_sf2_mutex, portMAX_DELAY);
    s_parser.setProgressCallback((SF2Parser::SF2ProgressCb)s_progress_cb, s_progress_user_data);
    ok = s_synth->loadSf2File(path);
    xSemaphoreGiveRecursive(s_sf2_mutex);

    ESP_LOGI(TAG, "load %s: %s", path, ok ? "ok" : "FAILED");
    return ok;
}

esp_err_t engine_sf2_register_source(void)
{
    esp_err_t ret = service_audio_register_source(&s_sf2_source);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "register source failed: %d", ret);
    }
    return ret;
}

void engine_sf2_get_debug_info(engine_sf2_debug_info_t *info)
{
    if (info == nullptr) {
        return;
    }

    info->max_voices = MAX_VOICES;
    info->active_voices = (s_synth != nullptr) ? (uint16_t)s_synth->activeVoiceCount() : 0;

    const float cycles_per_us = (float)CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
    info->render_block_us_avg = (s_render_blocks > 0)
        ? (float)s_render_cycles_sum / (float)s_render_blocks / cycles_per_us : 0.0f;
    info->render_block_us_max = (float)s_render_cycles_max / cycles_per_us;
    info->render_peak_amp = s_render_peak_amp;
    s_render_cycles_sum = 0;
    s_render_cycles_max = 0;
    s_render_blocks = 0;
    s_render_peak_amp = 0.0f;
}

static void s_midi_handler(const engine_midi_event_t *evt, void *user_data)
{
    (void)user_data;
    if (s_synth == nullptr || evt == nullptr) {
        return;
    }

    xSemaphoreTakeRecursive(s_sf2_mutex, portMAX_DELAY);
    switch (evt->type) {
        case ENGINE_MIDI_MSG_NOTE_ON:
            if (evt->channel < 16) {
                if (evt->data2 > 0) {
                    s_synth->noteOn(evt->channel, evt->data1, evt->data2);
                } else {
                    s_synth->noteOff(evt->channel, evt->data1);
                }
            }
            break;
        case ENGINE_MIDI_MSG_NOTE_OFF:
            if (evt->channel < 16) {
                s_synth->noteOff(evt->channel, evt->data1);
            }
            break;
        case ENGINE_MIDI_MSG_CONTROL_CHANGE:
            if (evt->channel < 16) {
                s_synth->controlChange(evt->channel, evt->data1, evt->data2);
            }
            break;
        case ENGINE_MIDI_MSG_PROGRAM_CHANGE:
            if (evt->channel < 16) {
                s_synth->programChange(evt->channel, evt->data1);
            }
            break;
        case ENGINE_MIDI_MSG_PITCH_BEND:
            if (evt->channel < 16) {
                s_synth->pitchBend(evt->channel, (int)evt->value - 8192);
            }
            break;
        case ENGINE_MIDI_MSG_SYSEX: {
            /* engine_midi 只存载荷（不含 0xF0/0xF7），handleSysEx 需要完整帧；
             * 内部 4 字节指令帧重建后为 6 字节，不会误判为 GM On（首字节非 0x7E） */
            uint8_t frame[ENGINE_MIDI_SYSEX_BUF_SIZE + 2];
            frame[0] = 0xF0;
            memcpy(&frame[1], evt->sysex_data, evt->sysex_len);
            frame[evt->sysex_len + 1] = 0xF7;
            s_synth->handleSysEx(frame, (size_t)evt->sysex_len + 2);
            break;
        }
        default:
            break;
    }
    xSemaphoreGiveRecursive(s_sf2_mutex);
}

static bool s_file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return false;
    }
    fclose(f);
    return true;
}

/* 默认音色搜索顺序：SD 卡 default.sf2 → SD 卡 soundfonts 目录首个 .sf2 → SPIFFS 内置 */
static bool s_load_default_soundfont(void)
{
    static const char *SD_SF2_DIR = "/sdcard/soundfonts";
    static const char *SD_SF2_DEFAULT = "/sdcard/soundfonts/default.sf2";
    static const char *SPIFFS_SF2_DEFAULT = "/sys_int/soundfonts/default.sf2";
    char path[320];

    if (s_file_exists(SD_SF2_DEFAULT)) {
        return engine_sf2_load_file(SD_SF2_DEFAULT);
    }

    DIR *dir = opendir(SD_SF2_DIR);
    if (dir != NULL) {
        bool found = false;
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type != DT_REG) {
                continue;
            }
            const char *dot = strrchr(entry->d_name, '.');
            if (dot == NULL || strcasecmp(dot, ".sf2") != 0) {
                continue;
            }
            snprintf(path, sizeof(path), "%s/%s", SD_SF2_DIR, entry->d_name);
            found = true;
            break;
        }
        closedir(dir);
        if (found) {
            return engine_sf2_load_file(path);
        }
    }

    if (s_file_exists(SPIFFS_SF2_DEFAULT)) {
        return engine_sf2_load_file(SPIFFS_SF2_DEFAULT);
    }

    return false;
}

static esp_err_t source_init(void)
{
    return engine_sf2_init();
}

static void source_deinit(void)
{
    engine_sf2_deinit();
}

static void source_render_stereo(float *buffer_lr, uint32_t frames)
{
    engine_sf2_render_stereo(buffer_lr, frames);
}

static bool source_is_ready(void)
{
    return engine_sf2_is_ready();
}
