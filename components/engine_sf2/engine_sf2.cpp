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
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "math.h"
#include "string.h"
#include "stdio.h"
#include "dirent.h"
#include "strings.h"
#include "sys/stat.h"

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
/* SD 目录扫描专用锁：setting rebuild(task_gui) 与 process 校验(task_app) 会并发
 * 重扫，共享 s_sd_names/s_sd_count 静态态，不加锁时两段扫描交织会出现计数翻倍
 * （2026-09 FTP 退出后真机日志 rescan 14→28→14）。独立于 s_sf2_mutex：
 * 加载持锁数秒，扫描只互斥扫描本身，不阻塞/不被加载阻塞 */
static SemaphoreHandle_t   s_sd_list_mutex = nullptr;

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

/* SD 音源选择状态：扫描缓存（静态 3KB，免堆分配）、当前生效音源（""=内部预设）、
 * 开机指定音源（main 按 NVS 在 activate 前写入） */
static char                s_sd_names[ENGINE_SF2_SD_MAX_FILES][ENGINE_SF2_SD_NAME_MAX_LEN];
static int                 s_sd_count = 0;
static char                s_current_source[ENGINE_SF2_SD_NAME_MAX_LEN] = "";
static char                s_boot_source[ENGINE_SF2_SD_NAME_MAX_LEN] = "";

/* 当前已加载音源实际占用的 PSRAM 字节数（0 = 未加载/内部预设）。
 * check_fit 用「绝对空间」判断：切换音源时旧音源会先释放，
 * 可用空间 = 当前剩余 + 当前已占用，否则基准被已占音源低估（2026-08）。 */
static uint32_t            s_loaded_psram_bytes = 0;

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

    if (s_sd_list_mutex == nullptr) {
        s_sd_list_mutex = xSemaphoreCreateMutex();
        if (s_sd_list_mutex == nullptr) {
            ESP_LOGE(TAG, "failed to create sd list mutex");
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

    struct stat st;
    uint64_t file_size = 0;
    if (stat(path, &st) == 0) {
        file_size = (uint64_t)st.st_size;
        if (strncmp(path, ENGINE_SF2_SD_DIR, strlen(ENGINE_SF2_SD_DIR)) == 0) {
            if (file_size > ENGINE_SF2_SD_MAX_BYTES) {
                ESP_LOGW(TAG, "sf2 too large: %s (%llu bytes > %lu)", path,
                         (unsigned long long)file_size, (unsigned long)ENGINE_SF2_SD_MAX_BYTES);
                return false;
            }
            /* PSRAM 预算闸门：加载后需保留安全余量给 App（Zen/Drum canvas 等），
             * 否则大音源加载后这些功能静默崩溃 */
            if (!engine_sf2_check_fit(path)) {
                ESP_LOGW(TAG, "sf2 exceeds PSRAM budget: %s", path);
                return false;
            }
        }
    } else if (strncmp(path, ENGINE_SF2_SD_DIR, strlen(ENGINE_SF2_SD_DIR)) == 0) {
        ESP_LOGW(TAG, "stat failed: %s", path);
        return false;
    }

    /* 记录加载前的 PSRAM 剩余，用于计算新音源实际占用 */
    uint32_t free_before = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    bool ok;
    xSemaphoreTakeRecursive(s_sf2_mutex, portMAX_DELAY);
    s_parser.setProgressCallback((SF2Parser::SF2ProgressCb)s_progress_cb, s_progress_user_data);
    ok = s_synth->loadSf2File(path);
    xSemaphoreGiveRecursive(s_sf2_mutex);

    /* 更新当前音源占用：新占用 = 加载前剩余 + 旧占用 - 加载后剩余。
     * 旧音源在加载过程中被上游 clear 释放，只看前后差会把新占用记成 0
     * （2026-08 真机：8MB→内部切换后 s_loaded=0，12.95MB 音源被 check_fit
     * 按 free+0 误拒）。并发分配/释放由差值自然吸收 */
    if (ok) {
        uint32_t free_after = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        uint64_t used = (uint64_t)free_before + (uint64_t)s_loaded_psram_bytes - (uint64_t)free_after;
        s_loaded_psram_bytes = (used > 0) ? (uint32_t)used : 0;
        /* 每次导入后打印实际大小与 PSRAM 占用，供容量审计；
         * internal 列：定位音源切换路径的内部 RAM 泄漏（2026-09 ws 建栈失败） */
        ESP_LOGI(TAG, "load %s: ok, file=%llu bytes, psram=%u bytes, free_psram=%u, internal=%u",
                 path, (unsigned long long)file_size, (unsigned)s_loaded_psram_bytes,
                 (unsigned)free_after,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL));

        /* 跟踪当前音源：SD 目录下记文件名，其余（内部预设）记空串 */
        if (strncmp(path, ENGINE_SF2_SD_DIR "/", strlen(ENGINE_SF2_SD_DIR) + 1) == 0) {
            strncpy(s_current_source, path + strlen(ENGINE_SF2_SD_DIR) + 1,
                    sizeof(s_current_source) - 1);
            s_current_source[sizeof(s_current_source) - 1] = '\0';
        } else {
            s_current_source[0] = '\0';
        }
    } else {
        s_loaded_psram_bytes = 0;
    }

    ESP_LOGI(TAG, "load %s: %s", path, ok ? "ok" : "FAILED");
    return ok;
}

/* 扫描本体（调用方须已持 s_sd_list_mutex） */
static int s_sd_rescan_locked(void)
{
    s_sd_count = 0;

    DIR *dir = opendir(ENGINE_SF2_SD_DIR);
    if (dir == NULL) {
        return 0;
    }

    struct dirent *entry;
    while (s_sd_count < ENGINE_SF2_SD_MAX_FILES && (entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG) {
            continue;
        }
        const char *dot = strrchr(entry->d_name, '.');
        if (dot == NULL || strcasecmp(dot, ".sf2") != 0) {
            continue;
        }
        /* d_name 最大 256 字节，用 memcpy 固定长度拷贝 + 显式终止（避免
         * -Wstringop-truncation 和 -Wformat-truncation） */
        memcpy(s_sd_names[s_sd_count], entry->d_name, ENGINE_SF2_SD_NAME_MAX_LEN - 1);
        s_sd_names[s_sd_count][ENGINE_SF2_SD_NAME_MAX_LEN - 1] = '\0';
        s_sd_count++;
    }
    closedir(dir);

    /* 选择序稳定：按文件名排序，下拉选项不随目录枚举顺序漂移 */
    for (int i = 1; i < s_sd_count; i++) {
        char key[ENGINE_SF2_SD_NAME_MAX_LEN];
        memcpy(key, s_sd_names[i], sizeof(key));
        int j = i - 1;
        while (j >= 0 && strcasecmp(s_sd_names[j], key) > 0) {
            memcpy(s_sd_names[j + 1], s_sd_names[j], sizeof(key));
            j--;
        }
        memcpy(s_sd_names[j + 1], key, sizeof(key));
    }

    ESP_LOGI(TAG, "sd rescan: %d soundfont(s)", s_sd_count);
    return s_sd_count;
}

int engine_sf2_sd_rescan(void)
{
    if (s_sd_list_mutex != nullptr) {
        xSemaphoreTake(s_sd_list_mutex, portMAX_DELAY);
    }
    int count = s_sd_rescan_locked();
    if (s_sd_list_mutex != nullptr) {
        xSemaphoreGive(s_sd_list_mutex);
    }
    return count;
}

bool engine_sf2_sd_source_exists(void)
{
    if (s_sd_list_mutex != nullptr) {
        xSemaphoreTake(s_sd_list_mutex, portMAX_DELAY);
    }
    bool exists = true;   /* 内部预设（空串）视为存在 */
    if (s_current_source[0] != '\0') {
        exists = false;
        int count = s_sd_rescan_locked();
        for (int i = 0; i < count; i++) {
            if (strcmp(s_sd_names[i], s_current_source) == 0) {
                exists = true;
                break;
            }
        }
    }
    if (s_sd_list_mutex != nullptr) {
        xSemaphoreGive(s_sd_list_mutex);
    }
    return exists;
}

const char *engine_sf2_sd_name_at(int index)
{
    if (index < 0 || index >= s_sd_count) {
        return NULL;
    }
    return s_sd_names[index];
}

const char *engine_sf2_current_source(void)
{
    return s_current_source;
}

void engine_sf2_set_boot_source(const char *sd_name)
{
    if (sd_name == NULL) {
        s_boot_source[0] = '\0';
        return;
    }
    strncpy(s_boot_source, sd_name, sizeof(s_boot_source) - 1);
    s_boot_source[sizeof(s_boot_source) - 1] = '\0';
}

bool engine_sf2_load_internal(void)
{
    static const char *SPIFFS_SF2_DEFAULT = "/sys_int/soundfonts/default.sf2";
    return engine_sf2_load_file(SPIFFS_SF2_DEFAULT);
}

/* 大音源加载后需保留给 App（Zen/Drum canvas 等）的最小 PSRAM 余量 */
#define SF2_SAFE_PSRAM_RESERVE  (3u * 1024u * 1024u)

bool engine_sf2_check_fit(const char *path)
{
    struct stat st;
    if (path == NULL || stat(path, &st) != 0) {
        return false;
    }
    uint32_t free_psram = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    /* 绝对空间：切换音源时旧音源先被上游释放，可用空间 = 当前剩余 + 当前已占用。
     * 若只按当前剩余判断，已被占音源会低估基准，导致加载 12M 左右完全可行的
     * 音源也被误拒（2026-08）。采样数据约等于文件大小（sdta chunk 占大头），
     * 加载后还需覆盖解析结构开销。 */
    uint64_t available = (uint64_t)free_psram + (uint64_t)s_loaded_psram_bytes;
    uint64_t needed = (uint64_t)st.st_size + SF2_SAFE_PSRAM_RESERVE;
    bool fits = (available >= needed);
    if (!fits) {
        ESP_LOGW(TAG, "sf2 %s size=%luMB available=%lluMB (free=%luMB+loaded=%luMB) needed=%lluMB, rejected",
                 path, (unsigned long)(st.st_size / 1024 / 1024),
                 (unsigned long long)(available / 1024 / 1024),
                 (unsigned long)(free_psram / 1024 / 1024),
                 (unsigned long)(s_loaded_psram_bytes / 1024 / 1024),
                 (unsigned long long)(needed / 1024 / 1024));
    }
    return fits;
}

bool engine_sf2_load_sd(const char *sd_name)
{
    if (sd_name == NULL || sd_name[0] == '\0') {
        return false;
    }
    char path[ENGINE_SF2_SD_NAME_MAX_LEN + 32];
    snprintf(path, sizeof(path), "%s/%s", ENGINE_SF2_SD_DIR, sd_name);
    return engine_sf2_load_file(path);
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

/* 开机音色加载顺序：NVS 指定的 SD 音源 → 内部预设 → 遗留 SD 探测兜底。
 * 兜底存在的意义：SPIFFS 镜像损坏时仍允许用 SD 出声，不静默死机 */
static bool s_load_default_soundfont(void)
{
    static const char *SD_SF2_DEFAULT = ENGINE_SF2_SD_DIR "/default.sf2";
    char path[320];

    if (s_boot_source[0] != '\0') {
        if (engine_sf2_load_sd(s_boot_source)) {
            return true;
        }
        ESP_LOGW(TAG, "boot source %s unavailable, fallback to internal", s_boot_source);
    }

    if (engine_sf2_load_internal()) {
        return true;
    }

    if (s_file_exists(SD_SF2_DEFAULT)) {
        return engine_sf2_load_file(SD_SF2_DEFAULT);
    }

    if (engine_sf2_sd_rescan() > 0) {
        snprintf(path, sizeof(path), "%s/%s", ENGINE_SF2_SD_DIR, engine_sf2_sd_name_at(0));
        return engine_sf2_load_file(path);
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
