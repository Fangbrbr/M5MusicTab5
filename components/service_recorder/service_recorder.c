/**
 * @file service_recorder.c
 * @brief MIDI 总线录音服务实现（直录标准 SMF .mid）
 */

#include "service_recorder.h"
#include "engine_midi.h"
#include "engine_midi_smf_write.h"
#include "service_rtc.h"
#include "service_sd.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

static const char *TAG = "service_recorder";

#define RECORDER_DIR                    "record"
#define RECORDER_QUEUE_LEN              256
#define RECORDER_MAX_EVENTS_PER_CYCLE   32
#define RECORDER_PATH_MAX               512

/**
 * @brief 内部状态机
 */
typedef enum {
    RECORDER_STATE_IDLE = 0,      /**< 未录制 */
    RECORDER_STATE_RECORDING,     /**< 录制中 */
    RECORDER_STATE_STOPPING,      /**< 停止请求，正在落盘 */
} recorder_state_t;

#define RECORDER_MIN_DURATION_MS        5000

/* 只录通道消息：总线上的内部 SysEx 帧（触摸/App 控制）不是音乐内容，
 * 且触摸流速率极高，订阅即过滤可显著降低队列压力 */
#define RECORDER_SUBSCRIBE_MASK  (ENGINE_MIDI_MASK_NOTE_OFF | ENGINE_MIDI_MASK_NOTE_ON | \
                                  ENGINE_MIDI_MASK_POLY_PRESSURE | ENGINE_MIDI_MASK_CONTROL_CHANGE | \
                                  ENGINE_MIDI_MASK_PROGRAM_CHANGE | ENGINE_MIDI_MASK_CHANNEL_PRESSURE | \
                                  ENGINE_MIDI_MASK_PITCH_BEND)

static uint8_t s_shadow_bank_msb[16];
static uint8_t s_shadow_bank_lsb[16];
static uint8_t s_shadow_pc[16];

static struct {
    volatile recorder_state_t state;
    SemaphoreHandle_t mutex;
    QueueHandle_t queue;
    FILE *file;
    engine_midi_smf_writer_t writer;
    TickType_t start_tick;
    uint32_t event_count;
    uint32_t duration_ms;
    uint32_t channels_used;
    char last_path[RECORDER_PATH_MAX];
} s_rec = { 0 };

static void recorder_midi_cb(const engine_midi_event_t *evt, void *user_data);
static service_recorder_result_t recorder_write_events(int max_events);
static service_recorder_result_t recorder_finalize(void);
static bool recorder_write_snapshot(void);

service_recorder_result_t service_recorder_init(void)
{
    if (s_rec.mutex != NULL) {
        return RECORDER_OK;
    }

    memset((void *)&s_rec, 0, sizeof(s_rec));

    memset(s_shadow_bank_msb, 0xFF, sizeof(s_shadow_bank_msb));
    memset(s_shadow_bank_lsb, 0xFF, sizeof(s_shadow_bank_lsb));
    memset(s_shadow_pc, 0xFF, sizeof(s_shadow_pc));

    s_rec.mutex = xSemaphoreCreateMutex();
    if (s_rec.mutex == NULL) {
        return RECORDER_ERR_NO_MEM;
    }

    /* 录音队列 256 项×约148B≈38KB，落 PSRAM 省内部 RAM（仅任务上下文） */
    s_rec.queue = xQueueCreateWithCaps(RECORDER_QUEUE_LEN, sizeof(engine_midi_event_t),
                                       MALLOC_CAP_SPIRAM);
    if (s_rec.queue == NULL) {
        vSemaphoreDelete(s_rec.mutex);
        s_rec.mutex = NULL;
        return RECORDER_ERR_NO_MEM;
    }

    esp_err_t sub = engine_midi_subscribe(RECORDER_SUBSCRIBE_MASK, 0xFFFF, recorder_midi_cb, NULL);
    if (sub != ESP_OK) {
        vQueueDelete(s_rec.queue);
        s_rec.queue = NULL;
        vSemaphoreDelete(s_rec.mutex);
        s_rec.mutex = NULL;
        return RECORDER_ERR_INTERNAL;
    }

    if (service_sd_is_mounted()) {
        const char *mount = service_sd_get_mount_point();
        char dir_path[256];
        snprintf(dir_path, sizeof(dir_path), "%s/%s", mount, RECORDER_DIR);
        struct stat st;
        if (stat(dir_path, &st) != 0 && mkdir(dir_path, 0755) != 0) {
            ESP_LOGW(TAG, "mkdir %s failed", dir_path);
        }
    }

    return RECORDER_OK;
}

service_recorder_result_t service_recorder_start(const char *source_tag)
{
    if (s_rec.mutex == NULL) {
        return RECORDER_ERR_INTERNAL;
    }

    if (xSemaphoreTake(s_rec.mutex, portMAX_DELAY) != pdTRUE) {
        return RECORDER_ERR_INTERNAL;
    }

    service_recorder_result_t ret = RECORDER_OK;

    do {
        if (s_rec.state != RECORDER_STATE_IDLE) {
            ret = RECORDER_ERR_BUSY;
            break;
        }

        if (!service_sd_is_mounted()) {
            ret = RECORDER_ERR_NO_SD;
            break;
        }

        if (source_tag == NULL || source_tag[0] == '\0') {
            ret = RECORDER_ERR_INTERNAL;
            break;
        }

        struct tm tm_now;
        time_t now = time(NULL);
        if (service_rtc_get_time_cached(&tm_now) != ESP_OK) {
            struct tm *tmp = localtime(&now);
            if (tmp != NULL) {
                tm_now = *tmp;
            } else {
                memset(&tm_now, 0, sizeof(tm_now));
            }
        }

        char fname[64];
        snprintf(fname, sizeof(fname), "%s_%04d%02d%02d%02d%02d%02d.mid",
                 source_tag,
                 tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                 tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);

        const char *mount = service_sd_get_mount_point();
        char dir_path[256];
        snprintf(dir_path, sizeof(dir_path), "%s/%s", mount, RECORDER_DIR);
        struct stat st;
        if (stat(dir_path, &st) != 0 && mkdir(dir_path, 0755) != 0) {
            ret = RECORDER_ERR_FILE_CREATE;
            break;
        }

        char rel_path[128];
        snprintf(rel_path, sizeof(rel_path), "%s/%s", RECORDER_DIR, fname);
        snprintf(s_rec.last_path, sizeof(s_rec.last_path), "%s/%s", mount, rel_path);

        s_rec.file = service_sd_fopen(rel_path, "wb");
        if (s_rec.file == NULL) {
            ret = RECORDER_ERR_FILE_CREATE;
            break;
        }

        if (engine_midi_smf_writer_begin(&s_rec.writer, s_rec.file, source_tag) != ESP_OK ||
            !recorder_write_snapshot()) {
            fclose(s_rec.file);
            s_rec.file = NULL;
            remove(s_rec.last_path);
            ret = RECORDER_ERR_WRITE;
            break;
        }

        s_rec.event_count = 0;
        s_rec.duration_ms = 0;
        s_rec.channels_used = 0;
        s_rec.start_tick = xTaskGetTickCount();
        s_rec.state = RECORDER_STATE_RECORDING;
    } while (0);

    xSemaphoreGive(s_rec.mutex);

    if (ret == RECORDER_OK) {
        ESP_LOGI(TAG, "recording started: %s", s_rec.last_path);
    }
    return ret;
}

service_recorder_result_t service_recorder_stop(void)
{
    if (s_rec.mutex == NULL) {
        return RECORDER_ERR_INTERNAL;
    }

    if (xSemaphoreTake(s_rec.mutex, portMAX_DELAY) != pdTRUE) {
        return RECORDER_ERR_INTERNAL;
    }

    service_recorder_result_t ret = RECORDER_OK;
    if (s_rec.state != RECORDER_STATE_RECORDING) {
        ret = RECORDER_ERR_NOT_RECORDING;
    } else {
        s_rec.state = RECORDER_STATE_STOPPING;
    }

    xSemaphoreGive(s_rec.mutex);
    return ret;
}

bool service_recorder_is_recording(void)
{
    return s_rec.state == RECORDER_STATE_RECORDING;
}

bool service_recorder_get_last_path(char *buf, size_t len)
{
    if (buf == NULL || len == 0) {
        return false;
    }

    if (s_rec.last_path[0] == '\0') {
        buf[0] = '\0';
        return false;
    }

    strncpy(buf, s_rec.last_path, len - 1);
    buf[len - 1] = '\0';
    return true;
}

void service_recorder_process(void)
{
    if (s_rec.mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(s_rec.mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }

    if (s_rec.state == RECORDER_STATE_RECORDING) {
        /* 墙钟推进时长：事件时间戳只反映有 MIDI 活动的时刻，静音间隔（或纯
         * 等待）时不推进；若仅靠事件时间戳，无事件/少事件的录制 stop 时
         * duration_ms 偏小甚至为 0，会被 finalize 按最短时长丢弃。
         * 两者取大，保证时长语义与真实录制时间一致。 */
        uint32_t elapsed_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount() - s_rec.start_tick);
        if (elapsed_ms > s_rec.duration_ms) {
            s_rec.duration_ms = elapsed_ms;
        }

        service_recorder_result_t r = recorder_write_events(RECORDER_MAX_EVENTS_PER_CYCLE);
        if (r != RECORDER_OK) {
            ESP_LOGE(TAG, "write error, stop recording");
            s_rec.state = RECORDER_STATE_STOPPING;
        }
    } else if (s_rec.state == RECORDER_STATE_STOPPING) {
        (void)recorder_write_events(RECORDER_QUEUE_LEN);
        service_recorder_result_t r = recorder_finalize();
        if (r != RECORDER_OK) {
            ESP_LOGE(TAG, "finalize failed: %d", (int)r);
        }
        s_rec.state = RECORDER_STATE_IDLE;
    }

    xSemaphoreGive(s_rec.mutex);
}

static void recorder_midi_cb(const engine_midi_event_t *evt, void *user_data)
{
    (void)user_data;
    if (evt == NULL) {
        return;
    }

    uint8_t ch = evt->channel & 0x0F;
    if (evt->type == ENGINE_MIDI_MSG_PROGRAM_CHANGE) {
        s_shadow_pc[ch] = evt->data1;
    } else if (evt->type == ENGINE_MIDI_MSG_CONTROL_CHANGE) {
        if (evt->data1 == 0) {
            s_shadow_bank_msb[ch] = evt->data2;
        } else if (evt->data1 == 32) {
            s_shadow_bank_lsb[ch] = evt->data2;
        }
    }

    if (s_rec.state != RECORDER_STATE_RECORDING) {
        return;
    }
    if (s_rec.queue == NULL) {
        return;
    }

    /* 时间戳以到达时刻为准：App 发布的 MIDI 事件结构体零初始化，timestamp
     * 恒为 0（engine_midi_publish 原样拷贝不补戳），若直接信任会被
     * 积压保护误杀全部事件。回调在 engine_midi_process 分发链内执行，
     * 到达时刻即事件时刻（±一个分发周期）。 */
    engine_midi_event_t stamped = *evt;
    stamped.timestamp = xTaskGetTickCount();

    /* 丢弃异常早于录制起点的事件（到达时刻戳下理论不触发，兑底保留） */
    if (stamped.timestamp < s_rec.start_tick) {
        ESP_LOGW(TAG, "drop pre-recording event type=0x%02X ts=%u start=%u",
                 stamped.type, (unsigned)stamped.timestamp, (unsigned)s_rec.start_tick);
        return;
    }

    if (xQueueSend(s_rec.queue, &stamped, 0) != pdTRUE) {
        ESP_LOGW(TAG, "queue full, dropped event type=0x%02X", stamped.type);
    }
}

static service_recorder_result_t recorder_write_events(int max_events)
{
    if (s_rec.file == NULL || s_rec.state != RECORDER_STATE_RECORDING) {
        return RECORDER_OK;
    }

    int written = 0;

    while (written < max_events) {
        engine_midi_event_t evt;
        if (xQueueReceive(s_rec.queue, &evt, 0) != pdTRUE) {
            break;
        }

        uint32_t rel_ms;
        if (evt.timestamp >= s_rec.start_tick) {
            rel_ms = pdTICKS_TO_MS(evt.timestamp - s_rec.start_tick);
        } else {
            rel_ms = 0; /* 防御性兜底，理论上 callback 已过滤 */
        }
        if (rel_ms > s_rec.duration_ms) {
            s_rec.duration_ms = rel_ms;
        }

        if (engine_midi_smf_writer_event(&s_rec.writer, rel_ms, evt.type, evt.channel,
                                         evt.data1, evt.data2, evt.value) != ESP_OK) {
            ESP_LOGE(TAG, "write event failed");
            return RECORDER_ERR_WRITE;
        }

        s_rec.event_count++;
        s_rec.channels_used |= (1U << (evt.channel & 0x0F));
        written++;
    }

    return RECORDER_OK;
}

static service_recorder_result_t recorder_finalize(void)
{
    if (s_rec.file == NULL) {
        return RECORDER_ERR_INTERNAL;
    }

    if (s_rec.duration_ms < RECORDER_MIN_DURATION_MS) {
        char path_to_remove[RECORDER_PATH_MAX];
        strncpy(path_to_remove, s_rec.last_path, sizeof(path_to_remove) - 1);
        path_to_remove[sizeof(path_to_remove) - 1] = '\0';
        fclose(s_rec.file);
        s_rec.file = NULL;
        if (path_to_remove[0] != '\0' && remove(path_to_remove) != 0) {
            ESP_LOGW(TAG, "remove short recording failed: %s", path_to_remove);
        }
        s_rec.last_path[0] = '\0';
        ESP_LOGW(TAG, "recording discarded, too short: %u ms", (unsigned)s_rec.duration_ms);
        return RECORDER_ERR_TOO_SHORT;
    }

    if (engine_midi_smf_writer_end(&s_rec.writer) != ESP_OK) {
        fclose(s_rec.file);
        s_rec.file = NULL;
        return RECORDER_ERR_WRITE;
    }

    if (fclose(s_rec.file) != 0) {
        s_rec.file = NULL;
        return RECORDER_ERR_WRITE;
    }
    s_rec.file = NULL;

    ESP_LOGI(TAG, "saved %s: events=%u dur=%ums",
             s_rec.last_path, (unsigned)s_rec.event_count, (unsigned)s_rec.duration_ms);
    return RECORDER_OK;
}

/* 初始音色快照落成 tick 0 的真实 CC/PC 事件：SMF 没有 hmr 式的头快照位，
 * 任何标准播放器（含本机 engine_midi_smf）都能自然恢复录制时音色 */
static bool recorder_write_snapshot(void)
{
    for (int ch = 0; ch < 16; ch++) {
        if (s_shadow_bank_msb[ch] != 0xFF &&
            engine_midi_smf_writer_event(&s_rec.writer, 0, ENGINE_MIDI_MSG_CONTROL_CHANGE,
                                         (uint8_t)ch, 0, s_shadow_bank_msb[ch], 0) != ESP_OK) {
            return false;
        }
        if (s_shadow_bank_lsb[ch] != 0xFF &&
            engine_midi_smf_writer_event(&s_rec.writer, 0, ENGINE_MIDI_MSG_CONTROL_CHANGE,
                                         (uint8_t)ch, 32, s_shadow_bank_lsb[ch], 0) != ESP_OK) {
            return false;
        }
        if (s_shadow_pc[ch] != 0xFF &&
            engine_midi_smf_writer_event(&s_rec.writer, 0, ENGINE_MIDI_MSG_PROGRAM_CHANGE,
                                         (uint8_t)ch, s_shadow_pc[ch], 0, 0) != ESP_OK) {
            return false;
        }
    }
    return true;
}
