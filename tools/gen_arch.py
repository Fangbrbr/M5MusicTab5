#!/usr/bin/env python3
"""
项目架构骨架生成脚本

支持参数化生成单个模块、分类批量生成或全量生成。
用法:
    python gen_arch.py --list
    python gen_arch.py --module engine_midi
    python gen_arch.py --category engine
    python gen_arch.py --all --force
    python gen_arch.py --dry-run --all
"""

import os
import sys
import argparse

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ---------------------------------------------------------------------------
# 模块定义
# ---------------------------------------------------------------------------

MODULES = {
    # === Engines ===
    "engine_midi": {
        "category": "engine",
        "brief": "MIDI 事件总线引擎",
        "desc": "系统内部唯一事件分发通道。支持 MIDI 1.0 全部通道消息与系统消息解析，并通过 SysEx 承载项目内部控制指令。多生产者随时投流，多消费者按类型/通道订阅。",
        "deps": ["freertos", "log"],
        "extra_include": ["engine_midi_config.h"],
        "types": [
            ("engine_midi_msg_type_t", "enum", """
    ENGINE_MIDI_MSG_NOTE_OFF         = 0x80,
    ENGINE_MIDI_MSG_NOTE_ON          = 0x90,
    ENGINE_MIDI_MSG_POLY_PRESSURE    = 0xA0,
    ENGINE_MIDI_MSG_CONTROL_CHANGE   = 0xB0,
    ENGINE_MIDI_MSG_PROGRAM_CHANGE   = 0xC0,
    ENGINE_MIDI_MSG_CHANNEL_PRESSURE = 0xD0,
    ENGINE_MIDI_MSG_PITCH_BEND       = 0xE0,
    ENGINE_MIDI_MSG_SYSEX            = 0xF0,
    ENGINE_MIDI_MSG_TIME_CODE        = 0xF1,
    ENGINE_MIDI_MSG_SONG_POSITION    = 0xF2,
    ENGINE_MIDI_MSG_SONG_SELECT      = 0xF3,
    ENGINE_MIDI_MSG_TUNE_REQUEST     = 0xF6,
    ENGINE_MIDI_MSG_TIMING_CLOCK     = 0xF8,
    ENGINE_MIDI_MSG_START            = 0xFA,
    ENGINE_MIDI_MSG_CONTINUE         = 0xFB,
    ENGINE_MIDI_MSG_STOP             = 0xFC,
    ENGINE_MIDI_MSG_ACTIVE_SENSING   = 0xFE,
    ENGINE_MIDI_MSG_SYSTEM_RESET     = 0xFF,"""),
            ("engine_midi_event_t", "struct", """
    uint32_t timestamp;
    uint8_t  type;
    uint8_t  channel;
    uint8_t  data1;
    uint8_t  data2;
    uint16_t value;
    uint8_t  sysex_len;
    uint8_t  sysex_data[ENGINE_MIDI_SYSEX_BUF_SIZE];"""),
            ("engine_midi_sysex_cmd_t", "struct", """
    uint8_t vendor_id;
    uint8_t cmd_code;
    uint8_t func_code;
    uint8_t payload_len;
    uint8_t payload[ENGINE_MIDI_SYSEX_CMD_MAX_LEN];"""),
            ("engine_midi_consumer_cb_t", "callback", "void (*engine_midi_consumer_cb_t)(const engine_midi_event_t *evt, void *user_data)"),
        ],
        "funcs": [
            ("engine_midi_init", "esp_err_t", "", "初始化 MIDI 事件总线"),
            ("engine_midi_feed_byte", "esp_err_t", "uint8_t byte", "向解析器投喂单个 MIDI 字节"),
            ("engine_midi_feed_stream", "esp_err_t", "const uint8_t *data, uint32_t len", "向解析器投喂一串 MIDI 字节"),
            ("engine_midi_publish", "esp_err_t", "const engine_midi_event_t *evt, uint32_t timeout_ms", "直接发布已解析事件"),
            ("engine_midi_subscribe", "esp_err_t", "uint32_t type_mask, uint16_t channel_mask, engine_midi_consumer_cb_t cb, void *user_data", "订阅指定类型与通道的事件"),
            ("engine_midi_unsubscribe", "esp_err_t", "engine_midi_consumer_cb_t cb", "取消订阅"),
            ("engine_midi_process", "void", "", "处理并分发队列中的 MIDI 事件"),
            ("engine_midi_parse_sysex_cmd", "esp_err_t", "const engine_midi_event_t *evt, engine_midi_sysex_cmd_t *cmd", "将 SysEx 事件解析为内部指令"),
        ],
        "static_decls": """
#define ENGINE_MIDI_QUEUE_LEN          128
#define ENGINE_MIDI_MAX_CONSUMERS      16
#define ENGINE_MIDI_SYSEX_BUF_SIZE     128
#define ENGINE_MIDI_SYSEX_CMD_MAX_LEN  128
#define ENGINE_MIDI_PARSER_BUF_SIZE    128

typedef enum {
    PARSER_STATE_IDLE,
    PARSER_STATE_DATA1,
    PARSER_STATE_DATA2,
    PARSER_STATE_SYSEX,
} parser_state_t;

typedef struct {
    uint32_t                  type_mask;
    uint16_t                  channel_mask;
    engine_midi_consumer_cb_t cb;
    void                     *user_data;
    bool                      active;
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

static void midi_event_reset(engine_midi_event_t *evt);
static void midi_publish_parsed(const engine_midi_event_t *evt);
static void midi_parser_handle_status(uint8_t byte);
static void midi_parser_handle_data(uint8_t byte);
static uint8_t midi_msg_data_bytes(uint8_t status);
static uint32_t midi_type_to_mask_bit(uint8_t type);
""".strip(),
        "global_impl": """
esp_err_t engine_midi_init(void)
{
    if (s_midi_queue != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_midi_queue = xQueueCreate(ENGINE_MIDI_QUEUE_LEN, sizeof(engine_midi_event_t));
    if (s_midi_queue == NULL) {
        ESP_LOGE(TAG, "queue create failed");
        return ESP_ERR_NO_MEM;
    }

    s_consumer_mutex = xSemaphoreCreateMutex();
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
    if (s_parser_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_parser_mutex, portMAX_DELAY);

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
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (uint32_t i = 0; i < len; i++) {
        engine_midi_feed_byte(data[i]);
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

    xSemaphoreTake(s_consumer_mutex, portMAX_DELAY);

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

    xSemaphoreGive(s_consumer_mutex);
    return ret;
}

esp_err_t engine_midi_unsubscribe(engine_midi_consumer_cb_t cb)
{
    if (cb == NULL || s_consumer_mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_consumer_mutex, portMAX_DELAY);

    esp_err_t ret = ESP_ERR_NOT_FOUND;
    for (int i = 0; i < ENGINE_MIDI_MAX_CONSUMERS; i++) {
        if (s_consumers[i].active && s_consumers[i].cb == cb) {
            memset(&s_consumers[i], 0, sizeof(midi_consumer_t));
            ret = ESP_OK;
        }
    }

    xSemaphoreGive(s_consumer_mutex);
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

    if (evt->sysex_len < 3) {
        return ESP_ERR_INVALID_SIZE;
    }

    cmd->vendor_id = evt->sysex_data[0];
    cmd->cmd_code = evt->sysex_data[1];
    cmd->func_code = evt->sysex_data[2];

    uint8_t payload_len = evt->sysex_len - 3;
    if (payload_len > ENGINE_MIDI_SYSEX_CMD_MAX_LEN) {
        payload_len = ENGINE_MIDI_SYSEX_CMD_MAX_LEN;
    }

    cmd->payload_len = payload_len;
    memcpy(cmd->payload, &evt->sysex_data[3], payload_len);

    return ESP_OK;
}
""".strip(),
        "local_impl": """
static void midi_event_reset(engine_midi_event_t *evt)
{
    memset(evt, 0, sizeof(engine_midi_event_t));
    evt->timestamp = xTaskGetTickCount();
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
    case 0x80:
    case 0x90:
    case 0xA0:
    case 0xB0:
    case 0xE0:
        return 2;
    case 0xC0:
    case 0xD0:
        return 1;
    default:
        break;
    }

    switch (status) {
    case 0xF1:
    case 0xF3:
        return 1;
    case 0xF2:
        return 2;
    case 0xF6:
    case 0xF8:
    case 0xFA:
    case 0xFB:
    case 0xFC:
    case 0xFE:
    case 0xFF:
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
            evt.value = evt.data1;
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
""".strip(),
    },
    "engine_gui": {
        "category": "engine",
        "brief": "图形引擎",
        "desc": "LVGL 后端初始化、EEZ Studio 前后端通信适配。",
        "deps": ["espressif__m5stack_tab5"],
        "funcs": [
            ("engine_gui_init", "esp_err_t", "", "初始化图形引擎"),
        ],
    },
    # === Services ===
    "service_audio": {
        "category": "service",
        "brief": "音频服务",
        "desc": "封装 ES8388 I2S 音频输出，提供播放与录制接口。",
        "deps": ["espressif__m5stack_tab5"],
        "funcs": [
            ("service_audio_init", "esp_err_t", "", "初始化音频服务"),
        ],
    },
    "service_page": {
        "category": "service",
        "brief": "显示服务",
        "desc": "系统屏幕 backend：boot/Launcher/setting/about/onboard 的 LVGL 控件事件处理与导航逻辑。",
        "deps": ["app_manager", "engine_gui", "service_nvs", "service_audio", "service_i18n", "service_wifi", "service_rtc", "espressif__esp_lvgl_port", "espressif__m5stack_tab5"],
        "funcs": [
            ("service_page_init", "void", "", "初始化系统屏幕事件处理器"),
            ("service_page_feed_event", "void", "lv_event_t *e", "分发系统屏幕的 LVGL 控件事件"),
        ],
    },
    "service_input": {
        "category": "service",
        "brief": "输入服务",
        "desc": "触摸、按键、UART MIDI、USB Host MIDI 输入聚合。",
        "deps": ["espressif__m5stack_tab5"],
        "funcs": [
            ("service_input_init", "esp_err_t", "", "初始化输入服务"),
        ],
    },
    "service_sd": {
        "category": "service",
        "brief": "存储服务",
        "desc": "SDMMC / SPIFFS 挂载与文件系统抽象。",
        "deps": ["espressif__m5stack_tab5"],
        "funcs": [
            ("service_sd_init", "esp_err_t", "", "初始化存储服务"),
        ],
    },
    # === App Manager ===
    "app_manager": {
        "category": "app_manager",
        "brief": "App 生命周期管理",
        "desc": "维护 App 注册表，负责启动、挂起、恢复、杀死与资源回收。",
        "deps": [],
        "types": [
            ("app_base_t", "struct_tagged", "app_base", """
    const char *name;
    bool (*on_init)(app_base_t *self);
    void (*on_render)(app_base_t *self, float *L, float *R, uint32_t samples);
    void (*on_update)(app_base_t *self);
    void (*on_pause)(app_base_t *self);
    void (*on_resume)(app_base_t *self);
    void (*on_destroy)(app_base_t *self);
    void *user_data;"""),
        ],
        "funcs": [
            ("app_manager_init", "esp_err_t", "", "初始化 App 管理器"),
            ("app_manager_register", "esp_err_t", "app_base_t *app", "注册一个 App"),
            ("app_manager_launch", "bool", "const char *name", "启动指定名称的 App"),
            ("app_manager_suspend", "bool", "const char *name", "挂起指定名称的 App"),
            ("app_manager_resume", "bool", "const char *name", "恢复指定名称的 App"),
            ("app_manager_kill", "bool", "const char *name", "杀死指定名称的 App 并回收资源"),
            ("app_manager_get_active", "app_base_t *", "", "获取当前激活的 App 实例"),
        ],
        "static_decls": """
#define APP_MANAGER_MAX_APPS 8

static struct {
    app_base_t *apps[APP_MANAGER_MAX_APPS];
    int count;
    app_base_t *active;
} s_mgr;
""".strip(),
        "global_impl": """
esp_err_t app_manager_init(void)
{
    memset(&s_mgr, 0, sizeof(s_mgr));
    return ESP_OK;
}

esp_err_t app_manager_register(app_base_t *app)
{
    if (app == NULL || app->name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_mgr.count >= APP_MANAGER_MAX_APPS) {
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < s_mgr.count; i++) {
        if (s_mgr.apps[i] == app || strcmp(s_mgr.apps[i]->name, app->name) == 0) {
            return ESP_ERR_INVALID_STATE;
        }
    }

    s_mgr.apps[s_mgr.count++] = app;
    return ESP_OK;
}

bool app_manager_launch(const char *name)
{
    for (int i = 0; i < s_mgr.count; i++) {
        if (strcmp(s_mgr.apps[i]->name, name) == 0) {
            if (s_mgr.active != NULL && s_mgr.active->on_pause != NULL) {
                s_mgr.active->on_pause(s_mgr.active);
            }

            s_mgr.active = s_mgr.apps[i];

            if (s_mgr.active->on_init != NULL) {
                s_mgr.active->on_init(s_mgr.active);
            }

            return true;
        }
    }

    return false;
}

bool app_manager_suspend(const char *name)
{
    if (s_mgr.active != NULL && strcmp(s_mgr.active->name, name) == 0) {
        if (s_mgr.active->on_pause != NULL) {
            s_mgr.active->on_pause(s_mgr.active);
        }
        return true;
    }

    return false;
}

bool app_manager_resume(const char *name)
{
    for (int i = 0; i < s_mgr.count; i++) {
        if (strcmp(s_mgr.apps[i]->name, name) == 0) {
            if (s_mgr.active != NULL && s_mgr.active != s_mgr.apps[i] && s_mgr.active->on_pause != NULL) {
                s_mgr.active->on_pause(s_mgr.active);
            }

            s_mgr.active = s_mgr.apps[i];

            if (s_mgr.active->on_resume != NULL) {
                s_mgr.active->on_resume(s_mgr.active);
            }

            return true;
        }
    }

    return false;
}

bool app_manager_kill(const char *name)
{
    for (int i = 0; i < s_mgr.count; i++) {
        if (strcmp(s_mgr.apps[i]->name, name) == 0) {
            if (s_mgr.active == s_mgr.apps[i]) {
                if (s_mgr.active->on_destroy != NULL) {
                    s_mgr.active->on_destroy(s_mgr.active);
                }
                s_mgr.active = NULL;
            }

            for (int j = i; j < s_mgr.count - 1; j++) {
                s_mgr.apps[j] = s_mgr.apps[j + 1];
            }

            s_mgr.count--;
            return true;
        }
    }

    return false;
}

app_base_t *app_manager_get_active(void)
{
    return s_mgr.active;
}
""".strip(),
    },
    # === Apps ===
    "app_piano": {
        "category": "app",
        "brief": "钢琴应用",
        "desc": "钢琴模式应用。",
        "deps": ["app_manager"],
        "funcs": [
            ("app_piano_register", "esp_err_t", "", "注册钢琴应用到 AppManager"),
        ],
    },
    "app_drumpad": {
        "category": "app",
        "brief": "鼓垫应用",
        "desc": "鼓垫模式应用。",
        "deps": ["app_manager"],
        "funcs": [
            ("app_drumpad_register", "esp_err_t", "", "注册鼓垫应用到 AppManager"),
        ],
    },
    "app_musicplayer": {
        "category": "app",
        "brief": "音乐播放器应用",
        "desc": "MIDI 文件音乐播放器应用。",
        "deps": ["app_manager"],
        "funcs": [
            ("app_musicplayer_register", "esp_err_t", "", "注册音乐播放器应用到 AppManager"),
        ],
    },
    # === Tasks ===
    "task_gui": {
        "category": "task",
        "brief": "L3 Task：GUI 任务",
        "desc": "优先级 5 | Core 0 | 周期 10 ms。负责 LVGL tick 与显示刷新。",
        "deps": ["service_page"],
        "task": {"period_ms": 10, "prio": 5, "core": 0, "body": "        /* TODO: LVGL tick + display refresh */"},
    },
    "task_input": {
        "category": "task",
        "brief": "L3 Task：输入任务",
        "desc": "优先级 7 | Core 0 | 周期 10 ms。负责触摸、按键、MIDI 输入轮询。",
        "deps": ["service_input"],
        "task": {"period_ms": 10, "prio": 7, "core": 0, "body": "        /* TODO: poll touch / button / midi input */"},
    },
    "task_comm": {
        "category": "task",
        "brief": "L3 Task：通信任务",
        "desc": "优先级 4 | Core 0 | 周期 10 ms。负责 MIDI 事件总线处理。",
        "deps": ["engine_midi"],
        "task": {"period_ms": 10, "prio": 4, "core": 0, "body": "        engine_midi_process();"},
    },
    "task_app": {
        "category": "task",
        "brief": "L3 Task：App 调度任务",
        "desc": "优先级 4 | Core 0 | 周期 10 ms。负责激活 App 的 on_update 调度。",
        "deps": ["app_manager"],
        "task": {"period_ms": 10, "prio": 4, "core": 0, "body": "        app_base_t *active = app_manager_get_active();\n        if (active != NULL && active->on_update != NULL) {\n            active->on_update(active);\n        }"},
    },
    "task_audio": {
        "category": "task",
        "brief": "L3 Task：音频任务",
        "desc": "优先级 6 | Core 1 | 周期 10 ms。负责音频渲染块处理。",
        "deps": ["service_audio"],
        "task": {"period_ms": 10, "prio": 6, "core": 1, "body": "        /* TODO: audio render block */"},
    },
}


# ---------------------------------------------------------------------------
# 生成辅助函数
# ---------------------------------------------------------------------------

def write_file(path, content, dry_run=False):
    if dry_run:
        print(f"  [DRY] {path}")
        return
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"  [GEN] {path}")


def guard_name(name):
    return name.upper().replace("/", "_").replace("-", "_") + "_H"


def gen_header(module):
    name = module.get("_name", "unknown")
    guard = guard_name(name)
    lines = []
    lines.append(f"/**")
    lines.append(f" * @file {name}.h")
    lines.append(f" * @brief {module['brief']}")
    lines.append(f" *")
    lines.append(f" * {module['desc']}")
    lines.append(f" */")
    lines.append("")
    lines.append(f"#ifndef {guard}")
    lines.append(f"#define {guard}")
    lines.append("")

    includes = set()
    includes.add("esp_err.h")
    for t in module.get("types", []):
        if t[1] in ("struct", "struct_tagged"):
            includes.add("stdint.h")
    if module.get("category") == "app_manager":
        includes.add("stdbool.h")
        includes.add("stdint.h")

    for inc in sorted(includes):
        lines.append(f"#include \"{inc}\"")
    lines.append("")

    lines.append("#ifdef __cplusplus")
    lines.append("extern \"C\" {")
    lines.append("#endif")
    lines.append("")

    for t in module.get("types", []):
        if t[1] == "struct":
            lines.append(f"typedef struct {{")
            lines.append(t[2])
            lines.append(f"}} {t[0]};")
        elif t[1] == "struct_tagged":
            tag = t[2]
            lines.append(f"typedef struct {tag} {t[0]};")
            lines.append("")
            lines.append(f"struct {tag} {{")
            lines.append(t[3])
            lines.append(f"}};")
        elif t[1] == "callback":
            lines.append(f"typedef {t[2]};")
        lines.append("")

    for fn in module.get("funcs", []):
        lines.append(f"/**")
        lines.append(f" * @brief {fn[3]}")
        if fn[2]:
            for p in fn[2].split(", "):
                lines.append(f" * @param[in] {p.split()[-1].strip('*')} 参数")
        lines.append(f" */")
        if fn[2]:
            lines.append(f"{fn[1]} {fn[0]}({fn[2]});")
        else:
            lines.append(f"{fn[1]} {fn[0]}(void);")
        lines.append("")

    if "task" in module:
        lines.append("/**")
        lines.append(" * @brief 启动任务")
        lines.append(" */")
        lines.append(f"void {name}_start(void);")
        lines.append("")

    lines.append("#ifdef __cplusplus")
    lines.append("}")
    lines.append("#endif")
    lines.append("")
    lines.append(f"#endif /* {guard} */")
    lines.append("")
    return "\n".join(lines)


def gen_source(module):
    name = module.get("_name", "unknown")
    lines = []
    lines.append(f"/**")
    lines.append(f" * @file {name}.c")
    lines.append(f" * @brief {module['brief']}")
    lines.append(f" */")
    lines.append("")
    lines.append(f"#include \"{name}.h\"")

    extra_incs = set()
    if "QueueHandle_t" in module.get("static_decls", ""):
        extra_incs.add("freertos/FreeRTOS.h")
        extra_incs.add("freertos/queue.h")
    if "memset" in module.get("global_impl", ""):
        extra_incs.add("string.h")
    if module.get("category") == "task":
        extra_incs.add("freertos/FreeRTOS.h")
        extra_incs.add("freertos/task.h")
    if module.get("category") == "app":
        extra_incs.add("app_manager.h")
    for dep in module.get("deps", []):
        if dep.startswith("service_") or dep.startswith("engine_") or dep.startswith("app_manager"):
            extra_incs.add(f"{dep}.h")
    for inc in sorted(extra_incs):
        lines.append(f"#include \"{inc}\"")

    lines.append("#include \"esp_log.h\"")
    lines.append("")
    lines.append(f"static const char *TAG = \"{name}\";")
    lines.append("")

    if "static_decls" in module and module["static_decls"]:
        lines.append(module["static_decls"])
        lines.append("")

    if "static_fwd" in module and module["static_fwd"]:
        lines.append("/* 本地函数前部声明 */")
        lines.append(module["static_fwd"])
        lines.append("")

    if "global_impl" in module and module["global_impl"]:
        lines.append(module["global_impl"])
    elif "task" in module:
        task = module["task"]
        n = name
        n_upper = n.upper()
        lines.append(f"#define {n_upper}_PERIOD_MS  {task['period_ms']}")
        lines.append(f"#define {n_upper}_STACK_SIZE 8192")
        lines.append(f"#define {n_upper}_PRIORITY   {task['prio']}")
        lines.append(f"#define {n_upper}_CORE       {task['core']}")
        lines.append("")
        lines.append(f"static void {n}_entry(void *arg);")
        lines.append("")
        lines.append(f"void {n}_start(void)")
        lines.append("{")
        lines.append(f"    xTaskCreatePinnedToCore({n}_entry, \"{n}\",")
        lines.append(f"                            {n_upper}_STACK_SIZE, NULL,")
        lines.append(f"                            {n_upper}_PRIORITY, NULL,")
        lines.append(f"                            {n_upper}_CORE);")
        lines.append("}")
        lines.append("")
        lines.append(f"static void {n}_entry(void *arg)")
        lines.append("{")
        lines.append("    (void)arg;")
        lines.append("    TickType_t last_wake = xTaskGetTickCount();")
        lines.append("")
        lines.append("    while (1) {")
        lines.append("        (void)TAG;")
        for line in task["body"].split("\n"):
            lines.append(line)
        lines.append("        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS({}_PERIOD_MS));".format(n_upper))
        lines.append("    }")
        lines.append("}")
    else:
        for fn in module.get("funcs", []):
            if fn[2]:
                lines.append(f"{fn[1]} {fn[0]}({fn[2]})")
                for p in fn[2].split(", "):
                    lines.append(f"    (void){p.split()[-1].strip('*')};")
            else:
                lines.append(f"{fn[1]} {fn[0]}(void)")
            lines.append("{")
            if fn[1] == "esp_err_t":
                lines.append(f"    ESP_LOGI(TAG, \"{fn[0]}\");")
                lines.append("    return ESP_OK;")
            elif fn[1] == "bool":
                lines.append("    return false;")
            elif fn[1] != "void":
                lines.append("    return 0;")
            lines.append("}")
            lines.append("")

    if "static_impl" in module and module["static_impl"]:
        lines.append("")
        lines.append("/* 本地函数体 */")
        lines.append(module["static_impl"])

    result = "\n".join(lines)
    if not result.endswith("\n"):
        result += "\n"
    return result


def gen_cmake(module):
    name = module.get("_name", "unknown")
    srcs = f'"{name}.c"'
    deps = " ".join(module.get("deps", []))
    if not deps:
        deps = ""
    return f"""idf_component_register(SRCS {srcs}
                       INCLUDE_DIRS "include"
                       REQUIRES {deps})
"""


def gen_root_cmake():
    paths = []
    for name, mod in MODULES.items():
        paths.append(f'    "components/{name}"')
    paths_str = "\n".join(paths)
    return f"""cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
{paths_str}
)

include($ENV{{IDF_PATH}}/tools/cmake/project.cmake)
project(TAB5_Music_Pad)
"""


def gen_main_cmake():
    task_names = sorted([n for n, m in MODULES.items() if m.get("category") == "task"])
    core_names = ["engine_midi", "app_manager", "service_audio", "service_page", "service_input", "service_sd"]
    all_reqs = core_names + task_names + ["espressif__m5stack_tab5"]
    req_lines = "\n                                ".join(all_reqs)
    return f"""idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES {req_lines})
"""


def gen_main_c():
    lines = []
    lines.append("/**")
    lines.append(" * @file main.c")
    lines.append(" * @brief 应用入口")
    lines.append(" *")
    lines.append(" * L3 Task 层胶水：初始化 L1 BSP、L2 Service / Engine / AppManager 后创建")
    lines.append(" * 5 个固定任务，自身进入永久阻塞。非必要不新增任何任务。")
    lines.append(" */")
    lines.append("")
    lines.append("#include \"bsp/m5stack_tab5.h\"")
    lines.append("#include \"engine_midi.h\"")
    lines.append("#include \"app_manager.h\"")
    lines.append("#include \"service_audio.h\"")
    lines.append("#include \"service_page.h\"")
    lines.append("#include \"service_input.h\"")
    lines.append("#include \"service_sd.h\"")

    task_names = [n for n, m in MODULES.items() if m.get("category") == "task"]
    for t in task_names:
        lines.append(f"#include \"{t}.h\"")

    lines.append("#include \"esp_log.h\"")
    lines.append("")
    lines.append("void app_main(void)")
    lines.append("{")
    lines.append("    esp_err_t ret;")
    lines.append("")
    lines.append("    ret = bsp_i2c_init();")
    lines.append("    if (ret != ESP_OK) {")
    lines.append("        ESP_LOGE(\"main\", \"bsp_i2c_init failed: %d\", ret);")
    lines.append("        return;")
    lines.append("    }")
    lines.append("")
    lines.append("    ret = engine_midi_init();")
    lines.append("    if (ret != ESP_OK) {")
    lines.append("        ESP_LOGE(\"main\", \"engine_midi_init failed: %d\", ret);")
    lines.append("        return;")
    lines.append("    }")
    lines.append("")
    lines.append("    ret = app_manager_init();")
    lines.append("    if (ret != ESP_OK) {")
    lines.append("        ESP_LOGE(\"main\", \"app_manager_init failed: %d\", ret);")
    lines.append("        return;")
    lines.append("    }")
    lines.append("")
    lines.append("    service_sd_init();")
    lines.append("    service_audio_init();")
    lines.append("    service_page_init();")
    lines.append("    service_input_init();")
    lines.append("")

    for t in task_names:
        lines.append(f"    {t}_start();")

    lines.append("")
    lines.append("    while (1) {")
    lines.append("        vTaskDelay(portMAX_DELAY);")
    lines.append("    }")
    lines.append("}")
    lines.append("")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# 主流程
# ---------------------------------------------------------------------------

def generate_module(name, dry_run=False):
    if name not in MODULES:
        print(f"[ERR] 未知模块: {name}")
        return False
    mod = MODULES[name].copy()
    mod["_name"] = name
    base = os.path.join(PROJECT_ROOT, "components", name)
    write_file(os.path.join(base, "include", f"{name}.h"), gen_header(mod), dry_run)
    write_file(os.path.join(base, f"{name}.c"), gen_source(mod), dry_run)
    write_file(os.path.join(base, "CMakeLists.txt"), gen_cmake(mod), dry_run)
    return True


def generate_project_files(dry_run=False):
    write_file(os.path.join(PROJECT_ROOT, "CMakeLists.txt"), gen_root_cmake(), dry_run)
    write_file(os.path.join(PROJECT_ROOT, "main", "CMakeLists.txt"), gen_main_cmake(), dry_run)
    write_file(os.path.join(PROJECT_ROOT, "main", "main.c"), gen_main_c(), dry_run)


def list_modules():
    print("可用模块列表：")
    cats = {}
    for name, mod in MODULES.items():
        cat = mod.get("category", "unknown")
        cats.setdefault(cat, []).append(name)
    for cat in sorted(cats.keys()):
        print(f"  [{cat}]")
        for name in sorted(cats[cat]):
            print(f"    - {name:20s} {MODULES[name]['brief']}")


def main():
    parser = argparse.ArgumentParser(description="TAB5_MUSIC_PAD 项目骨架生成脚本")
    parser.add_argument("--module", help="生成指定名称的模块")
    parser.add_argument("--category", help="生成指定分类下的所有模块 (engine/service/app_manager/app/task)")
    parser.add_argument("--all", action="store_true", help="生成所有模块及项目骨架")
    parser.add_argument("--force", action="store_true", help="全量生成时跳过确认")
    parser.add_argument("--dry-run", action="store_true", help="仅预览，不实际写入文件")
    parser.add_argument("--list", action="store_true", help="列出所有可用模块")
    args = parser.parse_args()

    if args.list:
        list_modules()
        return

    if args.module:
        print(f"[INFO] 生成模块: {args.module}")
        generate_module(args.module, dry_run=args.dry_run)
        return

    if args.category:
        names = [n for n, m in MODULES.items() if m.get("category") == args.category]
        if not names:
            print(f"[ERR] 分类不存在或无模块: {args.category}")
            return
        print(f"[INFO] 生成分类 '{args.category}' 下的 {len(names)} 个模块")
        for name in names:
            generate_module(name, dry_run=args.dry_run)
        return

    if args.all:
        if not args.force and not args.dry_run:
            print("[WARN] --all 将覆盖所有模块文件及 main.c / CMakeLists.txt")
            print("       若确认请追加 --force，或改用 --dry-run 预览")
            return
        print("[INFO] 全量生成所有模块及项目骨架")
        for name in MODULES:
            generate_module(name, dry_run=args.dry_run)
        generate_project_files(dry_run=args.dry_run)
        return

    parser.print_help()


if __name__ == "__main__":
    main()
