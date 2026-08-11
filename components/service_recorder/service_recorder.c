/**
 * @file service_recorder.c
 * @brief MIDI 总线录音服务实现
 */

#include "service_recorder.h"
#include "engine_midi.h"
#include "engine_midi_rec.h"
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
#define RECORDER_TAG_MAX                16

/**
 * @brief 内部状态机
 */
typedef enum {
    RECORDER_STATE_IDLE = 0,      /**< 未录制 */
    RECORDER_STATE_RECORDING,     /**< 录制中 */
    RECORDER_STATE_STOPPING,      /**< 停止请求，正在落盘 */
} recorder_state_t;

#define RECORDER_MIN_DURATION_MS        5000

static uint8_t s_shadow_bank_msb[16];
static uint8_t s_shadow_bank_lsb[16];
static uint8_t s_shadow_pc[16];

static struct {
    volatile recorder_state_t state;
    SemaphoreHandle_t mutex;
    QueueHandle_t queue;
    FILE *file;
    TickType_t start_tick;
    uint32_t start_time_epoch;
    uint32_t event_count;
    uint32_t duration_ms;
    uint32_t channels_used;
    uint32_t data_crc;
    char source_tag[RECORDER_TAG_MAX];
    char last_path[RECORDER_PATH_MAX];
    uint8_t channel_init[16][4];
} s_rec = { 0 };

static void recorder_midi_cb(const engine_midi_event_t *evt, void *user_data);
static void recorder_pack_header(uint8_t *out, uint32_t start_time_epoch, uint32_t duration_ms,
                                 uint32_t event_count, uint32_t channels_used,
                                 uint32_t data_crc, const char *source_tag,
                                 const uint8_t channel_init[16][4]);
static size_t recorder_pack_event(uint8_t *out, const engine_midi_event_t *evt, uint32_t rel_ms);
static service_recorder_result_t recorder_write_events(int max_events);
static service_recorder_result_t recorder_finalize(void);
static void write_u32_le(uint8_t *p, uint32_t v);
static void write_u16_le(uint8_t *p, uint16_t v);

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

    esp_err_t sub = engine_midi_subscribe(ENGINE_MIDI_MASK_ALL, 0xFFFF, recorder_midi_cb, NULL);
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
        } else {
            now = mktime(&tm_now);
        }
        s_rec.start_time_epoch = (uint32_t)now;

        char fname[64];
        snprintf(fname, sizeof(fname), "%s_%04d%02d%02d%02d%02d%02d.hmr",
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

        for (int ch = 0; ch < 16; ch++) {
            s_rec.channel_init[ch][ENGINE_MIDI_REC_CH_INIT_BANK_MSB] = s_shadow_bank_msb[ch];
            s_rec.channel_init[ch][ENGINE_MIDI_REC_CH_INIT_BANK_LSB] = s_shadow_bank_lsb[ch];
            s_rec.channel_init[ch][ENGINE_MIDI_REC_CH_INIT_PROGRAM]  = s_shadow_pc[ch];
            s_rec.channel_init[ch][ENGINE_MIDI_REC_CH_INIT_RESERVED] = ENGINE_MIDI_REC_CH_INIT_NONE;
        }

        uint8_t hdr[ENGINE_MIDI_REC_HEADER_SIZE];
        memset(hdr, 0, sizeof(hdr));
        recorder_pack_header(hdr, s_rec.start_time_epoch, 0, 0, 0, 0, source_tag, s_rec.channel_init);
        if (fwrite(hdr, 1, sizeof(hdr), s_rec.file) != sizeof(hdr)) {
            fclose(s_rec.file);
            s_rec.file = NULL;
            ret = RECORDER_ERR_WRITE;
            break;
        }

        s_rec.event_count = 0;
        s_rec.duration_ms = 0;
        s_rec.channels_used = 0;
        s_rec.data_crc = ENGINE_MIDI_REC_CRC32_INIT;
        strncpy(s_rec.source_tag, source_tag, sizeof(s_rec.source_tag) - 1);
        s_rec.source_tag[sizeof(s_rec.source_tag) - 1] = '\0';
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

    uint8_t ev_buf[12 + ENGINE_MIDI_SYSEX_BUF_SIZE];
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

        size_t ev_size = recorder_pack_event(ev_buf, &evt, rel_ms);
        if (fwrite(ev_buf, 1, ev_size, s_rec.file) != ev_size) {
            ESP_LOGE(TAG, "write event failed");
            return RECORDER_ERR_WRITE;
        }

        s_rec.data_crc = engine_midi_rec_crc32_update(s_rec.data_crc, ev_buf, ev_size);
        s_rec.event_count++;

        if (evt.type < 0xF0) {
            s_rec.channels_used |= (1U << (evt.channel & 0x0F));
        }

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

    if (fseek(s_rec.file, 0, SEEK_SET) != 0) {
        return RECORDER_ERR_WRITE;
    }

    uint8_t hdr[ENGINE_MIDI_REC_HEADER_SIZE];
    memset(hdr, 0, sizeof(hdr));
    recorder_pack_header(hdr, s_rec.start_time_epoch, s_rec.duration_ms,
                         s_rec.event_count, s_rec.channels_used,
                         engine_midi_rec_crc32_final(s_rec.data_crc),
                         s_rec.source_tag, s_rec.channel_init);
    if (fwrite(hdr, 1, sizeof(hdr), s_rec.file) != sizeof(hdr)) {
        return RECORDER_ERR_WRITE;
    }

    /* 头部覆盖后文件指针位于 header 末尾，必须跳到文件末尾再追加 footer，
     * 否则 footer 会覆盖前 4 字节事件数据导致解析失败。 */
    if (fseek(s_rec.file, 0, SEEK_END) != 0) {
        return RECORDER_ERR_WRITE;
    }

    uint8_t footer[4] = { 'E', 'N', 'D', 'R' };
    if (fwrite(footer, 1, 4, s_rec.file) != 4) {
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

static void recorder_pack_header(uint8_t *out, uint32_t start_time_epoch, uint32_t duration_ms,
                                 uint32_t event_count, uint32_t channels_used,
                                 uint32_t data_crc, const char *source_tag,
                                 const uint8_t channel_init[16][4])
{
    memcpy(out, ENGINE_MIDI_REC_MAGIC, 4);
    out[4] = ENGINE_MIDI_REC_VERSION;
    out[5] = 0;
    out[6] = 0;
    out[7] = 0;
    write_u32_le(&out[8], ENGINE_MIDI_REC_HEADER_SIZE);
    write_u32_le(&out[12], start_time_epoch);
    write_u32_le(&out[16], duration_ms);
    write_u32_le(&out[20], event_count);
    write_u32_le(&out[24], channels_used);
    write_u32_le(&out[28], data_crc);
    write_u32_le(&out[32], 0);
    memset(&out[36], 0, 16);
    if (source_tag != NULL) {
        size_t tag_len = strnlen(source_tag, 15);
        memcpy(&out[36], source_tag, tag_len);
    }
    memset(&out[52], 0, 12);
    if (channel_init != NULL) {
        memcpy(&out[64], channel_init, 16 * 4);
    } else {
        memset(&out[64], 0xFF, 16 * 4);
    }

    uint32_t crc = engine_midi_rec_crc32(out, ENGINE_MIDI_REC_HEADER_SIZE);
    write_u32_le(&out[32], crc);
}

static size_t recorder_pack_event(uint8_t *out, const engine_midi_event_t *evt, uint32_t rel_ms)
{
    write_u32_le(&out[0], rel_ms);
    out[4] = evt->type;
    out[5] = evt->channel;
    out[6] = evt->data1;
    out[7] = evt->data2;
    write_u16_le(&out[8], evt->value);
    out[10] = evt->sysex_len;
    out[11] = 0;
    if (evt->sysex_len > 0) {
        memcpy(&out[12], evt->sysex_data, evt->sysex_len);
    }
    return 12 + evt->sysex_len;
}

static void write_u32_le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static void write_u16_le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}
