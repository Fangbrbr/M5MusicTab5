/**
 * @file engine_midi.c
 * @brief MIDI 事件总线引擎实现
 */

#include "engine_midi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "string.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "engine_midi";

/**
 * @brief 解析器状态
 */
typedef enum {
    PARSER_STATE_IDLE,      /**< 等待状态字节 */
    PARSER_STATE_DATA1,     /**< 等待第一个数据字节 */
    PARSER_STATE_DATA2,     /**< 等待第二个数据字节 */
    PARSER_STATE_SYSEX,     /**< 接收 SysEx 数据中 */
} parser_state_t;

/**
 * @brief 消费者条目
 */
typedef struct {
    uint32_t                  type_mask;    /**< 消息类型掩码 */
    uint16_t                  channel_mask; /**< 通道掩码 */
    engine_midi_consumer_cb_t cb;           /**< 回调函数 */
    void                     *user_data;    /**< 用户数据 */
    bool                      active;       /**< 是否有效 */
} midi_consumer_t;

static QueueHandle_t  s_midi_queue = NULL;
static midi_consumer_t s_consumers[ENGINE_MIDI_MAX_CONSUMERS];
static SemaphoreHandle_t s_consumer_mutex = NULL;
static SemaphoreHandle_t s_parser_mutex = NULL;

static parser_state_t s_parser_state = PARSER_STATE_IDLE;
static uint8_t        s_running_status = 0;
static uint8_t        s_data_count = 0;
static uint8_t        s_data_buf[2];
static uint8_t        s_sysex_buf[ENGINE_MIDI_SYSEX_BUF_SIZE];
static uint8_t        s_sysex_len = 0;
static uint8_t        s_source_port = ENGINE_MIDI_PORT_INTERNAL;

static void midi_event_reset(engine_midi_event_t *evt);
static void midi_publish_parsed(const engine_midi_event_t *evt);
static void midi_parser_handle_status(uint8_t byte);
static void midi_parser_handle_data(uint8_t byte);
static uint8_t midi_msg_data_bytes(uint8_t status);
static uint32_t midi_type_to_mask_bit(uint8_t type);

esp_err_t engine_midi_init(void)
{
    if (s_midi_queue != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* MIDI 事件含 128B SysEx 缓冲，单项约 148B，128 项约 19KB；落 PSRAM
     * 省内部 RAM（总线仅任务上下文读写，无 ISR）。 */
    s_midi_queue = xQueueCreateWithCaps(ENGINE_MIDI_QUEUE_LEN, sizeof(engine_midi_event_t),
                                        MALLOC_CAP_SPIRAM);
    if (s_midi_queue == NULL) {
        ESP_LOGE(TAG, "queue create failed");
        return ESP_ERR_NO_MEM;
    }

    /* 递归锁：engine_midi_process 迭代消费者表时，可能回调内部再次 subscribe/unsubscribe */
    s_consumer_mutex = xSemaphoreCreateRecursiveMutex();
    if (s_consumer_mutex == NULL) {
        vQueueDelete(s_midi_queue);
        s_midi_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_parser_mutex = xSemaphoreCreateMutex();
    if (s_parser_mutex == NULL) {
        vSemaphoreDelete(s_consumer_mutex);
        s_consumer_mutex = NULL;
        vQueueDelete(s_midi_queue);
        s_midi_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    memset(s_consumers, 0, sizeof(s_consumers));
    s_parser_state = PARSER_STATE_IDLE;
    s_running_status = 0;
    s_data_count = 0;
    s_sysex_len = 0;

    return ESP_OK;
}

esp_err_t engine_midi_feed_byte(uint8_t byte)
{
    return engine_midi_feed_byte_from_port(byte, ENGINE_MIDI_PORT_INTERNAL);
}

esp_err_t engine_midi_feed_byte_from_port(uint8_t byte, uint8_t port)
{
    if (s_parser_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_parser_mutex, portMAX_DELAY);

    s_source_port = port;

    if (byte >= 0x80) {
        midi_parser_handle_status(byte);
    } else {
        midi_parser_handle_data(byte);
    }

    xSemaphoreGive(s_parser_mutex);
    return ESP_OK;
}

esp_err_t engine_midi_feed_stream(const uint8_t *data, uint32_t len)
{
    return engine_midi_feed_stream_from_port(data, len, ENGINE_MIDI_PORT_INTERNAL);
}

esp_err_t engine_midi_feed_stream_from_port(const uint8_t *data, uint32_t len, uint8_t port)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (uint32_t i = 0; i < len; i++) {
        engine_midi_feed_byte_from_port(data[i], port);
    }

    return ESP_OK;
}

esp_err_t engine_midi_publish(const engine_midi_event_t *evt, uint32_t timeout_ms)
{
    if (s_midi_queue == NULL || evt == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    BaseType_t ret = xQueueSend(s_midi_queue, evt, pdMS_TO_TICKS(timeout_ms));
    if (ret != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t engine_midi_subscribe(uint32_t type_mask, uint16_t channel_mask,
                                engine_midi_consumer_cb_t cb, void *user_data)
{
    if (cb == NULL || s_consumer_mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTakeRecursive(s_consumer_mutex, portMAX_DELAY);

    esp_err_t ret = ESP_ERR_NO_MEM;
    for (int i = 0; i < ENGINE_MIDI_MAX_CONSUMERS; i++) {
        if (!s_consumers[i].active) {
            s_consumers[i].type_mask = type_mask;
            s_consumers[i].channel_mask = channel_mask;
            s_consumers[i].cb = cb;
            s_consumers[i].user_data = user_data;
            s_consumers[i].active = true;
            ret = ESP_OK;
            break;
        }
    }

    xSemaphoreGiveRecursive(s_consumer_mutex);
    return ret;
}

esp_err_t engine_midi_unsubscribe(engine_midi_consumer_cb_t cb)
{
    if (cb == NULL || s_consumer_mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTakeRecursive(s_consumer_mutex, portMAX_DELAY);

    esp_err_t ret = ESP_ERR_NOT_FOUND;
    for (int i = 0; i < ENGINE_MIDI_MAX_CONSUMERS; i++) {
        if (s_consumers[i].active && s_consumers[i].cb == cb) {
            memset(&s_consumers[i], 0, sizeof(midi_consumer_t));
            ret = ESP_OK;
        }
    }

    xSemaphoreGiveRecursive(s_consumer_mutex);
    return ret;
}

void engine_midi_process(void)
{
    engine_midi_event_t evt;

    while (xQueueReceive(s_midi_queue, &evt, 0) == pdTRUE) {
        uint32_t type_bit = midi_type_to_mask_bit(evt.type);
        if (type_bit == 0) {
            continue;
        }

        /* 遍历消费者表期间持有递归锁，避免其他任务动态 subscribe/unsubscribe
         * 导致读到半初始化条目或回调指针。 */
        xSemaphoreTakeRecursive(s_consumer_mutex, portMAX_DELAY);

        for (int i = 0; i < ENGINE_MIDI_MAX_CONSUMERS; i++) {
            if (!s_consumers[i].active) {
                continue;
            }

            if ((s_consumers[i].type_mask & type_bit) == 0) {
                continue;
            }

            if (evt.type < 0xF0) {
                uint16_t ch_bit = (uint16_t)(1U << (evt.channel & 0x0F));
                if ((s_consumers[i].channel_mask & ch_bit) == 0) {
                    continue;
                }
            }

            s_consumers[i].cb(&evt, s_consumers[i].user_data);
        }

        xSemaphoreGiveRecursive(s_consumer_mutex);
    }
}

esp_err_t engine_midi_parse_sysex_cmd(const engine_midi_event_t *evt, engine_midi_sysex_cmd_t *cmd)
{
    if (evt == NULL || cmd == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (evt->type != ENGINE_MIDI_MSG_SYSEX) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 内部 SysEx 固定 4 字节：cmd + func + para1 + para2，无 vendor id */
    if (evt->sysex_len < 4) {
        return ESP_ERR_INVALID_SIZE;
    }

    cmd->vendor_id   = 0;
    cmd->cmd_code    = evt->sysex_data[0];
    cmd->func_code   = evt->sysex_data[1];

    uint8_t payload_len = evt->sysex_len - 2;
    if (payload_len > ENGINE_MIDI_SYSEX_CMD_MAX_LEN) {
        payload_len = ENGINE_MIDI_SYSEX_CMD_MAX_LEN;
    }

    cmd->payload_len = payload_len;
    memcpy(cmd->payload, &evt->sysex_data[2], payload_len);

    return ESP_OK;
}

static void midi_event_reset(engine_midi_event_t *evt)
{
    memset(evt, 0, sizeof(engine_midi_event_t));
    evt->timestamp = xTaskGetTickCount();
    evt->source_port = s_source_port;
}

static void midi_publish_parsed(const engine_midi_event_t *evt)
{
    if (s_midi_queue == NULL) {
        return;
    }

    BaseType_t ret = xQueueSend(s_midi_queue, evt, 0);
    if (ret != pdTRUE) {
        ESP_LOGW(TAG, "event queue full, dropped type=0x%02X", evt->type);
    }
}

static uint8_t midi_msg_data_bytes(uint8_t status)
{
    uint8_t type = status & 0xF0;

    switch (type) {
    case 0x80:  /* Note Off */
    case 0x90:  /* Note On */
    case 0xA0:  /* Poly Pressure */
    case 0xB0:  /* Control Change */
    case 0xE0:  /* Pitch Bend */
        return 2;
    case 0xC0:  /* Program Change */
    case 0xD0:  /* Channel Pressure */
        return 1;
    default:
        break;
    }

    switch (status) {
    case 0xF1:  /* Time Code */
    case 0xF3:  /* Song Select */
        return 1;
    case 0xF2:  /* Song Position */
        return 2;
    case 0xF6:  /* Tune Request */
    case 0xF8:  /* Timing Clock */
    case 0xFA:  /* Start */
    case 0xFB:  /* Continue */
    case 0xFC:  /* Stop */
    case 0xFE:  /* Active Sensing */
    case 0xFF:  /* System Reset */
        return 0;
    default:
        break;
    }

    return 0;
}

static uint32_t midi_type_to_mask_bit(uint8_t type)
{
    switch (type) {
    case ENGINE_MIDI_MSG_NOTE_OFF:         return ENGINE_MIDI_MASK_NOTE_OFF;
    case ENGINE_MIDI_MSG_NOTE_ON:          return ENGINE_MIDI_MASK_NOTE_ON;
    case ENGINE_MIDI_MSG_POLY_PRESSURE:    return ENGINE_MIDI_MASK_POLY_PRESSURE;
    case ENGINE_MIDI_MSG_CONTROL_CHANGE:   return ENGINE_MIDI_MASK_CONTROL_CHANGE;
    case ENGINE_MIDI_MSG_PROGRAM_CHANGE:   return ENGINE_MIDI_MASK_PROGRAM_CHANGE;
    case ENGINE_MIDI_MSG_CHANNEL_PRESSURE: return ENGINE_MIDI_MASK_CHANNEL_PRESSURE;
    case ENGINE_MIDI_MSG_PITCH_BEND:       return ENGINE_MIDI_MASK_PITCH_BEND;
    case ENGINE_MIDI_MSG_SYSEX:            return ENGINE_MIDI_MASK_SYSEX;
    case ENGINE_MIDI_MSG_TIME_CODE:        return ENGINE_MIDI_MASK_TIME_CODE;
    case ENGINE_MIDI_MSG_SONG_POSITION:    return ENGINE_MIDI_MASK_SONG_POSITION;
    case ENGINE_MIDI_MSG_SONG_SELECT:      return ENGINE_MIDI_MASK_SONG_SELECT;
    case ENGINE_MIDI_MSG_TUNE_REQUEST:     return ENGINE_MIDI_MASK_TUNE_REQUEST;
    case ENGINE_MIDI_MSG_TIMING_CLOCK:     return ENGINE_MIDI_MASK_TIMING_CLOCK;
    case ENGINE_MIDI_MSG_START:            return ENGINE_MIDI_MASK_START;
    case ENGINE_MIDI_MSG_CONTINUE:         return ENGINE_MIDI_MASK_CONTINUE;
    case ENGINE_MIDI_MSG_STOP:             return ENGINE_MIDI_MASK_STOP;
    case ENGINE_MIDI_MSG_ACTIVE_SENSING:   return ENGINE_MIDI_MASK_ACTIVE_SENSING;
    case ENGINE_MIDI_MSG_SYSTEM_RESET:     return ENGINE_MIDI_MASK_SYSTEM_RESET;
    default:                               return 0;
    }
}

static void midi_parser_handle_status(uint8_t byte)
{
    if (byte == 0xF0) {
        s_parser_state = PARSER_STATE_SYSEX;
        s_sysex_len = 0;
        return;
    }

    if (byte == 0xF7) {
        if (s_parser_state == PARSER_STATE_SYSEX) {
            engine_midi_event_t evt;
            midi_event_reset(&evt);
            evt.type = ENGINE_MIDI_MSG_SYSEX;
            evt.sysex_len = s_sysex_len;
            memcpy(evt.sysex_data, s_sysex_buf, s_sysex_len);
            midi_publish_parsed(&evt);
        }
        s_parser_state = PARSER_STATE_IDLE;
        return;
    }

    uint8_t data_bytes = midi_msg_data_bytes(byte);

    if (byte >= 0xF0) {
        if (data_bytes == 0) {
            engine_midi_event_t evt;
            midi_event_reset(&evt);
            evt.type = byte;
            midi_publish_parsed(&evt);
            s_parser_state = PARSER_STATE_IDLE;
        } else {
            s_running_status = byte;
            s_data_count = 0;
            s_parser_state = PARSER_STATE_DATA1;
        }
        return;
    }

    /* 通道消息 */
    s_running_status = byte;
    s_data_count = 0;

    if (data_bytes == 0) {
        s_parser_state = PARSER_STATE_IDLE;
        return;
    }

    s_parser_state = PARSER_STATE_DATA1;
}

static void midi_parser_handle_data(uint8_t byte)
{
    if (s_parser_state == PARSER_STATE_IDLE) {
        if (s_running_status == 0) {
            return;
        }

        uint8_t data_bytes = midi_msg_data_bytes(s_running_status);
        if (data_bytes == 0) {
            return;
        }

        s_data_count = 0;
        s_parser_state = PARSER_STATE_DATA1;
    }

    if (s_parser_state == PARSER_STATE_SYSEX) {
        if (s_sysex_len < ENGINE_MIDI_SYSEX_BUF_SIZE) {
            s_sysex_buf[s_sysex_len++] = byte;
        } else {
            ESP_LOGW(TAG, "sysex overflow, truncating");
        }
        return;
    }

    if (s_parser_state == PARSER_STATE_DATA1) {
        s_data_buf[0] = byte;
        s_data_count = 1;

        uint8_t need = midi_msg_data_bytes(s_running_status);
        if (need == 1) {
            engine_midi_event_t evt;
            midi_event_reset(&evt);
            evt.type = s_running_status & 0xF0;
            evt.channel = s_running_status & 0x0F;
            evt.data1 = s_data_buf[0];

            if (evt.type == ENGINE_MIDI_MSG_PROGRAM_CHANGE ||
                evt.type == ENGINE_MIDI_MSG_CHANNEL_PRESSURE) {
                evt.value = evt.data1;
            }

            midi_publish_parsed(&evt);
            s_parser_state = PARSER_STATE_DATA1;
        } else {
            s_parser_state = PARSER_STATE_DATA2;
        }
        return;
    }

    if (s_parser_state == PARSER_STATE_DATA2) {
        s_data_buf[1] = byte;
        s_data_count = 2;

        engine_midi_event_t evt;
        midi_event_reset(&evt);
        evt.type = s_running_status & 0xF0;
        evt.channel = s_running_status & 0x0F;
        evt.data1 = s_data_buf[0];
        evt.data2 = s_data_buf[1];

        /* Note On velocity = 0 等价 Note Off */
        if (evt.type == ENGINE_MIDI_MSG_NOTE_ON && evt.data2 == 0) {
            evt.type = ENGINE_MIDI_MSG_NOTE_OFF;
        }

        if (evt.type == ENGINE_MIDI_MSG_PITCH_BEND) {
            evt.value = (uint16_t)(evt.data1 | (evt.data2 << 7));
        } else if (evt.type == ENGINE_MIDI_MSG_CONTROL_CHANGE) {
            evt.value = evt.data2;
        } else {
            evt.value = ((uint16_t)evt.data1 << 8) | evt.data2;
        }

        midi_publish_parsed(&evt);
        s_parser_state = PARSER_STATE_DATA1;
    }
}
