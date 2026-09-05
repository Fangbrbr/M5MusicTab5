/**
 * @file service_wavrec.c
 * @brief WAV 录音/回放服务实现
 */

#include "service_wavrec.h"

#include "sdkconfig.h"

#include "service_audio.h"
#include "service_rtc.h"
#include "service_sd.h"
#include "service_voice.h"
#include "service_xiaozhi.h"

#include "esp_heap_caps.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static const char *TAG = "service_wavrec";

/* --------------------------------------------------------------------------
 * 配置常量
 * ------------------------------------------------------------------------ */

#define WAVREC_DIR              "wav"

/** @brief 录音环形缓冲（PSRAM，2 的幂）；乐器立体声 176KB/s 时约 5.9s 深度 */
#define WAVREC_RING_SIZE        (1U << 20)

/** @brief 采集任务单次 mic_read 帧数：读空后环内仍剩 192 帧(约4.3ms)调度余量 */
#define WAVREC_CAP_CHUNK_FRAMES 64

/** @brief 乐器模式开录：等 AFE 关闭+mic 实际释放的条件超时 */
#define WAVREC_ARM_TIMEOUT_MS   1000

/** @brief 停止时等采集任务退出读循环的时限（单次阻塞读 64 帧约 1.45ms） */
#define WAVREC_STOP_WAIT_MS     100

/** @brief 落盘泵单周期写入上限（避免 task_app 单次写 SD 卡 stall 过久） */
#define WAVREC_DRAIN_MAX_BYTES  (16 * 1024)

/** @brief 回放源 PCM 缓冲帧数（PSRAM，立体声时 8KB） */
#define WAVREC_PLAY_SRC_FRAMES  2048

/** @brief 回放单次重采样输出帧数（44.1k stereo） */
#define WAVREC_PLAY_OUT_FRAMES  512

/** @brief 播完等 aux 尾音排空的超时 */
#define WAVREC_PLAY_TAIL_MS     3000

/** @brief tap 模式 AGC：目标峰值（≈43% FS）。
 * Why: 固定增益两度实测失效（30dB PGA 远场语音基础电平仅百分之几 FS），
 * 改自动增益：块峰值低于目标慢放、近削顶快攻，把人声拉平到可闻响度。
 * 基准：官方出厂 demo 录音顶满 PGA 37.5dB + 裸放 ≈ 本链 30dB + AGC 后
 * 典型 +12~18dB − 6dB（AUX_MIX_GAIN=0.5 回放衰减） */
#define WAVREC_AGC_TARGET_PEAK  14000
#define WAVREC_AGC_MIN_GAIN_Q8  (1 << 8)    /* ×1 */
#define WAVREC_AGC_MAX_GAIN_Q8  (16 << 8)   /* ×16（+24dB） */
#define WAVREC_AGC_INIT_GAIN_Q8 (4 << 8)    /* 起始 ×4，对齐官方基准链 */
#define WAVREC_AGC_SIGNAL_FLOOR 150         /* 静音底噪声门限：低于此值不释放增益 */

/** @brief 乐器模式 mic 增益（dB）。
 * 依据：官方录音基准 37.5dB 是远场语音调校；乐器近场大声压，回退 13.5dB
 * 防削波（get_db 3dB 步进，24dB 精确落档）。实测后可调 */
#define WAVREC_INST_GAIN_DB     24.0f

/** @brief 采集任务参数（Core 0，栈 PSRAM） */
#define WAVREC_CAP_TASK_STACK   3072
#define WAVREC_CAP_TASK_PRIO    8
#define WAVREC_CAP_TASK_CORE    0

/* --------------------------------------------------------------------------
 * 静态状态
 * ------------------------------------------------------------------------ */

typedef enum {
    WAVREC_ST_IDLE = 0,
    WAVREC_ST_ARMING,       /**< 乐器：等 service_voice 释放 mic */
    WAVREC_ST_RECORDING,
    WAVREC_ST_STOPPING,     /**< 乐器：等采集任务退出读循环 */
    WAVREC_ST_PLAYING,
    WAVREC_ST_PLAY_TAIL,    /**< 等 aux 尾音排空后恢复 AI */
} wavrec_state_t;

static wavrec_state_t s_state = WAVREC_ST_IDLE;
static service_wavrec_err_t s_last_err = SERVICE_WAVREC_OK;
static volatile uint8_t s_level = 0;   /* 瞬时电平 0-100（数据面写，UI 轮询读） */

/** @brief 块峰值 → 0-100 电平（dB 域：-42dBFS..0dBFS 映射 0..100）。
 * Trap: 线性映射下语音典型峰值 -30dBFS 只得 3/100，电平柱"只有吹气才动"；
 * dB 域下底噪 ~17（可见不抢戏）、语音 60+、AGC 收敛后 80+、削顶 100。 */
static uint8_t wavrec_calc_level(const int16_t *pcm, uint32_t samples)
{
    int16_t peak = 0;
    for (uint32_t i = 0; i < samples; i++) {
        int16_t a = (pcm[i] < 0) ? (int16_t)(-(int32_t)pcm[i]) : pcm[i];
        if (a > peak) {
            peak = a;
        }
    }
    if (peak < 260) {   /* -42dBFS 以下直接归零：省一次 log，且保证下式 v 非负 */
        return 0;
    }
    float db = 20.0f * log10f((float)peak * (1.0f / 32767.0f));
    int v = (int)((db + 42.0f) * (100.0f / 42.0f));
    if (v > 100) {
        v = 100;
    }
    return (uint8_t)v;
}

/* 录音上下文 */
static FILE *s_rec_file = NULL;
static char s_rec_path[160] = {0};
static service_wavrec_mode_t s_rec_mode = SERVICE_WAVREC_MODE_VOICE;
static uint32_t s_rec_rate = 0;
static uint8_t s_rec_ch = 0;
static uint32_t s_rec_data_bytes = 0;
static TickType_t s_arm_tick = 0;
static TickType_t s_stop_tick = 0;

/* SPSC 环形缓冲（生产者=tap/采集任务，消费者=process/task_app） */
static uint8_t *s_ring = NULL;
static volatile uint32_t s_ring_w = 0;
static volatile uint32_t s_ring_r = 0;
static uint32_t s_ring_drop_bytes = 0;

/* 采集任务（仅乐器模式运行） */
static TaskHandle_t s_cap_task = NULL;
static volatile bool s_cap_run = false;
static volatile bool s_cap_idle = true;
static int16_t s_cap_buf[WAVREC_CAP_CHUNK_FRAMES * 2];

/* 回放上下文 */
static FILE *s_play_file = NULL;
static uint32_t s_play_rate = 0;
static uint16_t s_play_ch = 0;
static uint32_t s_play_data_left = 0;
static uint32_t s_play_total_ms = 0;
static uint32_t s_play_frames_base = 0;   /* 已从源缓冲移出的累计源帧数 */
static int16_t *s_play_src = NULL;
static uint32_t s_play_src_len = 0;
static float s_play_src_pos = 0.0f;
static float s_play_step = 1.0f;
static TickType_t s_play_tail_tick = 0;
static int16_t s_play_out[WAVREC_PLAY_OUT_FRAMES * 2];

/* --------------------------------------------------------------------------
 * 前向声明
 * ------------------------------------------------------------------------ */

static void wavrec_capture_task(void *arg);
static void wavrec_tap_cb(const int16_t *pcm, uint32_t samples, void *ctx);
static uint32_t wavrec_ring_avail(void);
static void wavrec_ring_write(const void *data, uint32_t bytes);
static bool wavrec_rec_create_file(void);
static void wavrec_rec_finalize(bool keep);
static void wavrec_rec_abort(void);
static void wavrec_set_error(service_wavrec_err_t err);

/* --------------------------------------------------------------------------
 * 环形缓冲（SPSC）
 * ------------------------------------------------------------------------ */

static uint32_t wavrec_ring_avail(void)
{
    return s_ring_w - s_ring_r;
}

static void wavrec_ring_write(const void *data, uint32_t bytes)
{
    uint32_t free_bytes = WAVREC_RING_SIZE - (s_ring_w - s_ring_r);
    if (bytes > free_bytes) {
        /* 满即丢弃（丢尾部新数据），由消费侧限流上报 */
        s_ring_drop_bytes += bytes - free_bytes;
        bytes = free_bytes;
        if (bytes == 0) {
            return;
        }
    }
    uint32_t off = s_ring_w & (WAVREC_RING_SIZE - 1);
    uint32_t first = WAVREC_RING_SIZE - off;
    if (first > bytes) {
        first = bytes;
    }
    memcpy(s_ring + off, data, first);
    memcpy(s_ring, (const uint8_t *)data + first, bytes - first);
    __sync_synchronize();
    s_ring_w += bytes;
}

/* --------------------------------------------------------------------------
 * WAV 容器
 * ------------------------------------------------------------------------ */

static void wavrec_put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

static void wavrec_put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static void wavrec_write_header(FILE *f, uint32_t rate, uint16_t ch, uint32_t data_bytes)
{
    uint8_t h[44];
    memset(h, 0, sizeof(h));
    memcpy(h + 0, "RIFF", 4);
    wavrec_put32(h + 4, 36 + data_bytes);
    memcpy(h + 8, "WAVE", 4);
    memcpy(h + 12, "fmt ", 4);
    wavrec_put32(h + 16, 16);              /* fmt 块长度 */
    wavrec_put16(h + 20, 1);               /* PCM */
    wavrec_put16(h + 22, ch);
    wavrec_put32(h + 24, rate);
    wavrec_put32(h + 28, rate * ch * 2);   /* 字节率 */
    wavrec_put16(h + 32, ch * 2);          /* 块对齐 */
    wavrec_put16(h + 34, 16);              /* 位深 */
    memcpy(h + 36, "data", 4);
    wavrec_put32(h + 40, data_bytes);
    fseek(f, 0, SEEK_SET);
    fwrite(h, 1, sizeof(h), f);
}

static uint32_t wavrec_get16(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}

static uint32_t wavrec_get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * @brief 解析 RIFF/WAVE 头，定位 data 块（fmt 可为扩展布局，逐块跳过）
 */
static bool wavrec_parse_header(FILE *f, uint32_t *rate, uint16_t *ch, uint32_t *data_len)
{
    uint8_t hdr[12];
    if (fread(hdr, 1, 12, f) != 12 || memcmp(hdr, "RIFF", 4) != 0 ||
        memcmp(hdr + 8, "WAVE", 4) != 0) {
        return false;
    }

    bool got_fmt = false;
    uint16_t fmt_tag = 0, bits = 0;
    while (1) {
        uint8_t chdr[8];
        if (fread(chdr, 1, 8, f) != 8) {
            return false;
        }
        uint32_t sz = wavrec_get32(chdr + 4);
        if (memcmp(chdr, "fmt ", 4) == 0) {
            uint8_t fmt[16];
            if (sz < 16 || fread(fmt, 1, 16, f) != 16) {
                return false;
            }
            fmt_tag = (uint16_t)wavrec_get16(fmt + 0);
            *ch = (uint16_t)wavrec_get16(fmt + 2);
            *rate = wavrec_get32(fmt + 4);
            bits = (uint16_t)wavrec_get16(fmt + 14);
            got_fmt = true;
            if (sz > 16) {
                fseek(f, sz - 16, SEEK_CUR);
            }
        } else if (memcmp(chdr, "data", 4) == 0) {
            *data_len = sz;
            break;
        } else {
            fseek(f, sz, SEEK_CUR);
        }
        if (sz & 1) {
            fseek(f, 1, SEEK_CUR);   /* RIFF 块按偶数对齐 */
        }
    }

    /* 仅接受 PCM16、mono/stereo、8-48kHz */
    return got_fmt && fmt_tag == 1 && bits == 16 &&
           (*ch == 1 || *ch == 2) && *rate >= 8000 && *rate <= 48000;
}

/* --------------------------------------------------------------------------
 * 录音：tap 回调与采集任务（数据面）
 * ------------------------------------------------------------------------ */

static int16_t s_tap_buf[SERVICE_VOICE_AFE_MAX_CHUNK];
static int32_t s_agc_gain_q8 = WAVREC_AGC_INIT_GAIN_Q8;   /* AGC 当前增益 Q8.8 */

static void wavrec_tap_cb(const int16_t *pcm, uint32_t samples, void *ctx)
{
    (void)ctx;
    if (s_state != WAVREC_ST_RECORDING ||
        s_rec_mode == SERVICE_WAVREC_MODE_INSTRUMENT) {
        return;
    }
    if (samples > SERVICE_VOICE_AFE_MAX_CHUNK) {
        samples = SERVICE_VOICE_AFE_MAX_CHUNK;
    }

    /* AGC 判决用原始块峰值 */
    int16_t peak = 0;
    for (uint32_t i = 0; i < samples; i++) {
        int16_t a = (pcm[i] < 0) ? (int16_t)(-(int32_t)pcm[i]) : pcm[i];
        if (a > peak) {
            peak = a;
        }
    }
    /* 快攻慢放（每块 ~32ms 一步）：近削顶立即 ×3/4；有信号且低于目标
     * 则 ×1.031 慢放（约 +0.27dB/块）；静音期不释放防底噪爬升 */
    int32_t scaled = ((int32_t)peak * s_agc_gain_q8) >> 8;
    if (scaled > 30000) {
        s_agc_gain_q8 = (s_agc_gain_q8 * 3) / 4;
        if (s_agc_gain_q8 < WAVREC_AGC_MIN_GAIN_Q8) {
            s_agc_gain_q8 = WAVREC_AGC_MIN_GAIN_Q8;
        }
    } else if (peak > WAVREC_AGC_SIGNAL_FLOOR && scaled < WAVREC_AGC_TARGET_PEAK) {
        s_agc_gain_q8 += s_agc_gain_q8 >> 5;
        if (s_agc_gain_q8 > WAVREC_AGC_MAX_GAIN_Q8) {
            s_agc_gain_q8 = WAVREC_AGC_MAX_GAIN_Q8;
        }
    }

    /* 应用增益 + 限幅（tap 契约：task_audio 上下文仅允许轻量数据处理） */
    for (uint32_t i = 0; i < samples; i++) {
        int32_t v = ((int32_t)pcm[i] * s_agc_gain_q8) >> 8;
        if (v > 32767) {
            v = 32767;
        } else if (v < -32768) {
            v = -32768;
        }
        s_tap_buf[i] = (int16_t)v;
    }
    s_level = wavrec_calc_level(s_tap_buf, samples);
    wavrec_ring_write(s_tap_buf, samples * sizeof(int16_t));
}

static void wavrec_capture_task(void *arg)
{
    (void)arg;
    while (1) {
        if (!s_cap_run) {
            s_cap_idle = true;
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            continue;
        }
        s_cap_idle = false;
        int32_t n = service_audio_mic_read(s_cap_buf, WAVREC_CAP_CHUNK_FRAMES);
        if (n < 0) {
            wavrec_set_error(SERVICE_WAVREC_ERR_MIC);
            s_cap_run = false;
            continue;
        }
        s_level = wavrec_calc_level(s_cap_buf, (uint32_t)n * s_rec_ch);
        wavrec_ring_write(s_cap_buf, (uint32_t)n * s_rec_ch * sizeof(int16_t));
    }
}

/* --------------------------------------------------------------------------
 * 录音：控制面（task_app 上下文）
 * ------------------------------------------------------------------------ */

static void wavrec_set_error(service_wavrec_err_t err)
{
    s_last_err = err;
}

/** @brief 写盘失败等致命错误的模式感知收尾：保留已录部分并走正常恢复链 */
static void wavrec_rec_abort(void)
{
    wavrec_set_error(SERVICE_WAVREC_ERR_FILE);
    if (s_rec_mode == SERVICE_WAVREC_MODE_INSTRUMENT) {
        /* 复用 STOPPING 分支完成 关mic/落盘/重开AFE/恢复AI；重复进入幂等 */
        if (s_state != WAVREC_ST_STOPPING) {
            s_cap_run = false;
            s_stop_tick = xTaskGetTickCount();
            s_state = WAVREC_ST_STOPPING;
        }
    } else {
        service_voice_set_mic_tap(NULL, NULL, false);
        wavrec_rec_finalize(true);
        s_state = WAVREC_ST_IDLE;
    }
}

static bool wavrec_rec_create_file(void)
{
    if (!service_sd_is_mounted()) {
        return false;
    }

    char dir_path[64];
    snprintf(dir_path, sizeof(dir_path), "%s/%s", service_sd_get_mount_point(), WAVREC_DIR);
    struct stat st;
    if (stat(dir_path, &st) != 0 && mkdir(dir_path, 0755) != 0) {
        return false;
    }

    struct tm tm_now;
    memset(&tm_now, 0, sizeof(tm_now));
    if (service_rtc_get_time_cached(&tm_now) != ESP_OK) {
        time_t now = time(NULL);
        struct tm *tmp = localtime(&now);
        if (tmp != NULL) {
            tm_now = *tmp;
        }
    }

    /* 重名追加 _1/_2…；RTC 无效（1970）时同秒重复即触发该兜底 */
    char rel_path[128];
    for (int i = 0; i < 100; i++) {
        if (i == 0) {
            snprintf(rel_path, sizeof(rel_path), "%s/%04d%02d%02d_%02d%02d%02d.wav",
                     WAVREC_DIR, tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                     tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
        } else {
            snprintf(rel_path, sizeof(rel_path), "%s/%04d%02d%02d_%02d%02d%02d_%d.wav",
                     WAVREC_DIR, tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                     tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec, i);
        }
        snprintf(s_rec_path, sizeof(s_rec_path), "%s/%s", service_sd_get_mount_point(), rel_path);
        if (stat(s_rec_path, &st) != 0) {
            break;
        }
    }

    s_rec_file = service_sd_fopen(rel_path, "wb");
    if (s_rec_file == NULL) {
        return false;
    }
    /* 先写占位头，停止时回填真实长度 */
    wavrec_write_header(s_rec_file, s_rec_rate, s_rec_ch, 0);
    return true;
}

static void wavrec_rec_finalize(bool keep)
{
    if (s_rec_file != NULL) {
        if (keep) {
            wavrec_write_header(s_rec_file, s_rec_rate, s_rec_ch, s_rec_data_bytes);
            fclose(s_rec_file);
            ESP_LOGI(TAG, "saved: %s (%lu bytes)", s_rec_path,
                     (unsigned long)(s_rec_data_bytes + 44));
        } else {
            fclose(s_rec_file);
            remove(s_rec_path);
            ESP_LOGW(TAG, "discarded: %s", s_rec_path);
        }
        s_rec_file = NULL;
    }
}

/** @brief 落盘泵：环形缓冲 → SD（task_app 上下文，单周期限量） */
static void wavrec_rec_drain(uint32_t max_bytes)
{
    uint32_t avail = wavrec_ring_avail();
    if (avail == 0 || s_rec_file == NULL) {
        return;
    }
    uint32_t todo = (avail > max_bytes) ? max_bytes : avail;

    while (todo > 0) {
        uint32_t off = s_ring_r & (WAVREC_RING_SIZE - 1);
        uint32_t seg = WAVREC_RING_SIZE - off;
        if (seg > todo) {
            seg = todo;
        }
        size_t wr = fwrite(s_ring + off, 1, seg, s_rec_file);
        s_ring_r += wr;
        s_rec_data_bytes += wr;
        if (wr != seg) {
            /* 写入失败（SD 满/掉卡）：保留已录部分，走模式感知恢复链 */
            ESP_LOGE(TAG, "write failed, salvaging %lu bytes",
                     (unsigned long)s_rec_data_bytes);
            wavrec_rec_abort();
            return;
        }
        todo -= seg;
    }
}

service_wavrec_err_t service_wavrec_start(service_wavrec_mode_t mode)
{
#if !CONFIG_BOARD_HAS_SD || !CONFIG_BOARD_HAS_MIC
    return SERVICE_WAVREC_ERR_NO_SD;
#endif
    if (s_state != WAVREC_ST_IDLE) {
        return SERVICE_WAVREC_ERR_BUSY;
    }
    if (!service_sd_is_mounted()) {
        return SERVICE_WAVREC_ERR_NO_SD;
    }
    if (s_ring == NULL) {
        return SERVICE_WAVREC_ERR_STATE;
    }

    s_rec_mode = mode;
    s_rec_data_bytes = 0;
    s_ring_w = 0;
    s_ring_r = 0;
    s_ring_drop_bytes = 0;

    if (mode == SERVICE_WAVREC_MODE_INSTRUMENT) {
        /* 独占 mic：挂起 AI（停会话+关唤醒），关 AFE 释放 mic，
         * ARMING 态等 manage_mic 在 task_audio 下一拍实际关闭 */
        service_xiaozhi_set_suspended(true);
        service_voice_wake_close_deferred();
        s_rec_rate = 44100;
        s_rec_ch = 2;
        s_arm_tick = xTaskGetTickCount();
        s_state = WAVREC_ST_ARMING;
    } else {
        if (!service_voice_is_afe_active()) {
            return SERVICE_WAVREC_ERR_NO_VOICE_FE;
        }
        s_rec_rate = 16000;
        s_rec_ch = 1;
        if (!wavrec_rec_create_file()) {
            return SERVICE_WAVREC_ERR_FILE;
        }
        s_agc_gain_q8 = WAVREC_AGC_INIT_GAIN_Q8;   /* AGC 从基准增益起步自适应 */
        service_voice_set_mic_tap(wavrec_tap_cb, NULL,
                                  mode == SERVICE_WAVREC_MODE_AMBIENT);
        s_state = WAVREC_ST_RECORDING;
        ESP_LOGI(TAG, "rec start: mode=%d %s", (int)mode, s_rec_path);
    }
    return SERVICE_WAVREC_OK;
}

void service_wavrec_stop(void)
{
    if (s_state == WAVREC_ST_RECORDING) {
        if (s_rec_mode == SERVICE_WAVREC_MODE_INSTRUMENT) {
            s_cap_run = false;
            s_stop_tick = xTaskGetTickCount();
            s_state = WAVREC_ST_STOPPING;
        } else {
            service_voice_set_mic_tap(NULL, NULL, false);
            wavrec_rec_drain(UINT32_MAX);
            wavrec_rec_finalize(true);
            s_state = WAVREC_ST_IDLE;
        }
    } else if (s_state == WAVREC_ST_ARMING) {
        /* 未开录即取消：直接恢复 AI/AFE */
        service_voice_wake_open_deferred();
        service_xiaozhi_set_suspended(false);
        s_state = WAVREC_ST_IDLE;
    }
    /* STOPPING/PLAYING 中重复调用忽略 */
}

bool service_wavrec_is_recording(void)
{
    return s_state == WAVREC_ST_RECORDING;
}

bool service_wavrec_is_rec_busy(void)
{
    return s_state == WAVREC_ST_ARMING || s_state == WAVREC_ST_RECORDING ||
           s_state == WAVREC_ST_STOPPING;
}

uint32_t service_wavrec_get_elapsed_ms(void)
{
    if (s_rec_rate == 0 || s_rec_ch == 0) {
        return 0;
    }
    return (uint32_t)((uint64_t)s_rec_data_bytes * 1000 / (s_rec_rate * s_rec_ch * 2));
}

bool service_wavrec_get_last_path(char *buf, size_t len)
{
    if (buf == NULL || len == 0 || s_rec_path[0] == '\0') {
        return false;
    }
    snprintf(buf, len, "%s", s_rec_path);
    return true;
}

/* --------------------------------------------------------------------------
 * 回放（task_app 上下文控制 + 泵）
 * ------------------------------------------------------------------------ */

/** @brief 源缓冲补料：移出已消费帧，从文件读满剩余空间 */
static bool wavrec_play_fill(void)
{
    uint32_t used = (uint32_t)s_play_src_pos;
    if (used > 0) {
        uint32_t keep = s_play_src_len - used;
        memmove(s_play_src, s_play_src + used * s_play_ch,
                keep * s_play_ch * sizeof(int16_t));
        s_play_src_len = keep;
        s_play_src_pos -= (float)used;
        s_play_frames_base += used;
    }
    if (s_play_data_left == 0) {
        return s_play_src_len > 0;
    }
    uint32_t want = (WAVREC_PLAY_SRC_FRAMES - s_play_src_len) * s_play_ch * 2;
    if (want > s_play_data_left) {
        want = s_play_data_left;
    }
    size_t got = fread(s_play_src + s_play_src_len * s_play_ch, 1, want, s_play_file);
    s_play_data_left -= (uint32_t)got;
    s_play_src_len += (uint32_t)got / (s_play_ch * 2);
    return s_play_src_len > 0;
}

/** @brief 线性插值重采样到 44.1k stereo，返回产出帧数（0=源耗尽） */
static uint32_t wavrec_play_render(int16_t *out_lr, uint32_t out_frames)
{
    uint32_t produced = 0;
    while (produced < out_frames) {
        uint32_t idx = (uint32_t)s_play_src_pos;
        if (idx + 1 >= s_play_src_len) {
            if (!wavrec_play_fill()) {
                break;
            }
            idx = (uint32_t)s_play_src_pos;
            if (idx + 1 >= s_play_src_len) {
                if (idx >= s_play_src_len) {
                    break;
                }
                /* 源最后一帧无插值对，直接输出 */
                const int16_t *f0 = s_play_src + idx * s_play_ch;
                out_lr[produced * 2] = f0[0];
                out_lr[produced * 2 + 1] = (s_play_ch == 2) ? f0[1] : f0[0];
                produced++;
                s_play_src_pos += s_play_step;
                break;
            }
        }
        float frac = s_play_src_pos - (float)idx;
        const int16_t *f0 = s_play_src + idx * s_play_ch;
        const int16_t *f1 = f0 + s_play_ch;
        int32_t l = f0[0] + (int32_t)((f1[0] - f0[0]) * frac);
        int32_t r = (s_play_ch == 2) ? f0[1] + (int32_t)((f1[1] - f0[1]) * frac) : l;
        out_lr[produced * 2] = (int16_t)l;
        out_lr[produced * 2 + 1] = (int16_t)r;
        produced++;
        s_play_src_pos += s_play_step;
    }
    return produced;
}

service_wavrec_err_t service_wavrec_play(const char *path)
{
    if (s_state != WAVREC_ST_IDLE) {
        return SERVICE_WAVREC_ERR_BUSY;
    }
    if (path == NULL || s_play_src == NULL) {
        return SERVICE_WAVREC_ERR_STATE;
    }

    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return SERVICE_WAVREC_ERR_FILE;
    }
    uint32_t rate = 0, data_len = 0;
    uint16_t ch = 0;
    if (!wavrec_parse_header(f, &rate, &ch, &data_len) || data_len == 0) {
        fclose(f);
        return SERVICE_WAVREC_ERR_BAD_FILE;
    }

    s_play_file = f;
    s_play_rate = rate;
    s_play_ch = ch;
    s_play_data_left = data_len;
    s_play_src_len = 0;
    s_play_src_pos = 0.0f;
    s_play_frames_base = 0;
    s_play_step = (float)rate / 44100.0f;
    s_play_total_ms = (uint32_t)((uint64_t)data_len * 1000 / ((uint32_t)ch * 2 * rate));

    /* 挂起 AI：aux 为 SPSC 单生产者，TTS 下行会撞车；清空残留重新预充 */
    service_xiaozhi_set_suspended(true);
    service_audio_aux_clear();

    s_state = WAVREC_ST_PLAYING;
    ESP_LOGI(TAG, "play: %s rate=%lu ch=%u total=%lums", path,
             (unsigned long)rate, ch, (unsigned long)s_play_total_ms);
    return SERVICE_WAVREC_OK;
}

void service_wavrec_play_stop(void)
{
    if (s_state != WAVREC_ST_PLAYING && s_state != WAVREC_ST_PLAY_TAIL) {
        return;
    }
    if (s_play_file != NULL) {
        fclose(s_play_file);
        s_play_file = NULL;
    }
    service_audio_aux_clear();
    service_xiaozhi_set_suspended(false);
    s_state = WAVREC_ST_IDLE;
}

bool service_wavrec_is_playing(void)
{
    return s_state == WAVREC_ST_PLAYING || s_state == WAVREC_ST_PLAY_TAIL;
}

uint32_t service_wavrec_play_get_pos_ms(void)
{
    if (s_play_rate == 0) {
        return 0;
    }
    uint32_t frames = s_play_frames_base + (uint32_t)s_play_src_pos;
    return (uint32_t)((uint64_t)frames * 1000 / s_play_rate);
}

uint32_t service_wavrec_play_get_total_ms(void)
{
    return s_play_total_ms;
}

service_wavrec_err_t service_wavrec_take_error(void)
{
    service_wavrec_err_t e = s_last_err;
    s_last_err = SERVICE_WAVREC_OK;
    return e;
}

uint8_t service_wavrec_get_level(void)
{
    return s_level;
}

/* --------------------------------------------------------------------------
 * 周期处理（task_app 10ms）
 * ------------------------------------------------------------------------ */

void service_wavrec_process(void)
{
#if CONFIG_BOARD_HAS_SD && CONFIG_BOARD_HAS_MIC
    switch (s_state) {
    case WAVREC_ST_ARMING:
        /* 条件等待：AFE 关闭且 mic 真正释放后才可重配 44.1k/2ch
         *（mic_open 对已开状态幂等不重配，早开只会拿到错误的 1ch 配置） */
        if (!service_voice_is_afe_active() && !service_audio_mic_is_open()) {
            if (service_audio_mic_open(44100, 2) != ESP_OK) {
                wavrec_set_error(SERVICE_WAVREC_ERR_MIC);
                service_voice_wake_open_deferred();
                service_xiaozhi_set_suspended(false);
                s_state = WAVREC_ST_IDLE;
                break;
            }
            service_audio_mic_set_gain(WAVREC_INST_GAIN_DB);
            if (!wavrec_rec_create_file()) {
                wavrec_set_error(SERVICE_WAVREC_ERR_FILE);
                service_audio_mic_close();
                service_voice_wake_open_deferred();
                service_xiaozhi_set_suspended(false);
                s_state = WAVREC_ST_IDLE;
                break;
            }
            s_cap_idle = false;
            s_cap_run = true;
            xTaskNotifyGive(s_cap_task);
            s_state = WAVREC_ST_RECORDING;
            ESP_LOGI(TAG, "rec start: mode=instrument %s", s_rec_path);
        } else if ((xTaskGetTickCount() - s_arm_tick) >= pdMS_TO_TICKS(WAVREC_ARM_TIMEOUT_MS)) {
            ESP_LOGE(TAG, "arm timeout: afe=%d mic=%d",
                     (int)service_voice_is_afe_active(),
                     (int)service_audio_mic_is_open());
            wavrec_set_error(SERVICE_WAVREC_ERR_MIC);
            service_voice_wake_open_deferred();
            service_xiaozhi_set_suspended(false);
            s_state = WAVREC_ST_IDLE;
        }
        break;

    case WAVREC_ST_RECORDING:
        wavrec_rec_drain(WAVREC_DRAIN_MAX_BYTES);
        if (s_ring_drop_bytes > 0) {
            /* 落盘跟不上导致环形缓冲溢出丢段：仅告警计数，录制继续 */
            ESP_LOGW(TAG, "ring overrun, dropped %lu bytes",
                     (unsigned long)s_ring_drop_bytes);
            s_ring_drop_bytes = 0;
        }
        break;

    case WAVREC_ST_STOPPING:
        if (s_cap_idle ||
            (xTaskGetTickCount() - s_stop_tick) >= pdMS_TO_TICKS(WAVREC_STOP_WAIT_MS)) {
            service_audio_mic_close();
            wavrec_rec_drain(UINT32_MAX);
            wavrec_rec_finalize(true);
            /* AFE 重开尽力而为：内部 RAM 不足时唤醒退化为按住说话直至重启 */
            if (service_voice_wake_open_deferred() != ESP_OK) {
                ESP_LOGW(TAG, "AFE reopen failed, wake word degraded until reboot");
            }
            service_xiaozhi_set_suspended(false);
            s_state = WAVREC_ST_IDLE;
        }
        break;

    case WAVREC_ST_PLAYING: {
        uint32_t guard = 4;
        while (guard-- > 0) {
            /* 按 aux 实际余量定本次产出，保证 aux_write 不截断（SPSC 单生产者） */
            uint32_t free_f = service_audio_aux_free_frames();
            if (free_f < 160) {
                break;
            }
            uint32_t want = (free_f < WAVREC_PLAY_OUT_FRAMES) ? free_f : WAVREC_PLAY_OUT_FRAMES;
            uint32_t n = wavrec_play_render(s_play_out, want);
            if (n == 0) {
                break;
            }
            s_level = wavrec_calc_level(s_play_out, n * 2);
            service_audio_aux_write(s_play_out, n);
        }
        if (s_play_data_left == 0 && s_play_src_len <= (uint32_t)s_play_src_pos) {
            service_audio_aux_end_of_stream();
            s_play_tail_tick = xTaskGetTickCount();
            s_state = WAVREC_ST_PLAY_TAIL;
        }
        break;
    }

    case WAVREC_ST_PLAY_TAIL:
        if (service_audio_aux_is_idle() ||
            (xTaskGetTickCount() - s_play_tail_tick) >= pdMS_TO_TICKS(WAVREC_PLAY_TAIL_MS)) {
            if (s_play_file != NULL) {
                fclose(s_play_file);
                s_play_file = NULL;
            }
            service_xiaozhi_set_suspended(false);
            s_state = WAVREC_ST_IDLE;
        }
        break;

    default:
        if (s_level != 0) {
            s_level = 0;   /* 空闲归零，电平柱靠窗自然下落 */
        }
        break;
    }
#endif
}

/* --------------------------------------------------------------------------
 * 初始化
 * ------------------------------------------------------------------------ */

esp_err_t service_wavrec_init(void)
{
#if !CONFIG_BOARD_HAS_SD || !CONFIG_BOARD_HAS_MIC
    ESP_LOGW(TAG, "board has no SD/MIC, wavrec disabled");
    return ESP_ERR_NOT_SUPPORTED;
#else
    s_ring = heap_caps_malloc(WAVREC_RING_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_play_src = heap_caps_malloc(WAVREC_PLAY_SRC_FRAMES * 2 * sizeof(int16_t),
                                  MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_ring == NULL || s_play_src == NULL) {
        ESP_LOGE(TAG, "psram alloc failed");
        return ESP_ERR_NO_MEM;
    }

    static StackType_t *s_cap_stack = NULL;
    static StaticTask_t s_cap_tcb;
    if (s_cap_stack == NULL) {
        s_cap_stack = heap_caps_malloc(WAVREC_CAP_TASK_STACK,
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (s_cap_stack == NULL) {
        ESP_LOGE(TAG, "cap task stack alloc failed");
        return ESP_ERR_NO_MEM;
    }
    s_cap_task = xTaskCreateStaticPinnedToCore(
        wavrec_capture_task, "wavrec_cap",
        WAVREC_CAP_TASK_STACK, NULL,
        WAVREC_CAP_TASK_PRIO, s_cap_stack, &s_cap_tcb,
        WAVREC_CAP_TASK_CORE);
    if (s_cap_task == NULL) {
        ESP_LOGE(TAG, "cap task create failed");
        return ESP_ERR_NO_MEM;
    }

    if (service_sd_is_mounted()) {
        char dir_path[64];
        snprintf(dir_path, sizeof(dir_path), "%s/%s", service_sd_get_mount_point(), WAVREC_DIR);
        struct stat st;
        if (stat(dir_path, &st) != 0) {
            mkdir(dir_path, 0755);
        }
    }

    ESP_LOGI(TAG, "init ok (ring=%uKB)", (unsigned)(WAVREC_RING_SIZE / 1024));
    return ESP_OK;
#endif
}
