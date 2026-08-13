/**
 * @file app_manager.c
 * @brief App 生命周期管理
 */

#include "app_manager.h"
#include "engine_midi.h"
#include "service_recorder.h"
#include "service_rtc.h"
#include "string.h"

#include "app_ai_agent.h"
#include "app_chord_trainer.h"
#include "app_circle_of_fifths.h"
#include "app_clock_calendar.h"
#include "app_drum_pad.h"
#include "app_ear_trainer.h"
#include "app_midi_player.h"
#include "app_metronome.h"
#include "app_fun.h"
#include "app_tiny_piano.h"
#include "app_xy_pad.h"
#include "app_zen.h"
#include "stdio.h"
#include "stdarg.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"

static const char *TAG = "app_manager";

/** @brief 系统 tick 计数（由 main 循环累加），供 App 内部超时/动画使用 */
uint32_t sys_monitor_tick = 0;

#define APP_MANAGER_MAX_APPS 32

/* 堆取证：App 切换是内存压力最集中的路径，内部 RAM 仅应剩 DMA/驱动用量 */
static void app_manager_log_heap(const char *where)
{
    ESP_LOGI(TAG, "heap[%s] internal=%u psram=%u", where,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

/* 与 engine_gui / EEZ vars.h 保持一致的内部 SysEx 协议（无 vendor id） */
#define MIDI_CMD_APP             1
#define MIDI_CMD_INPUT           3
#define MIDI_CMD_APP_CONTROL     7
#define MIDI_FUNC_INPUT_TOUCH    0
#define MIDI_FUNC_INPUT_MOUSE    2
#define MIDI_FUNC_INPUT_KEYBOARD 3

static void app_manager_handle_touch(const engine_midi_event_t *evt)
{
    if (evt->sysex_len < 8) {
        return;
    }

    uint8_t state = evt->sysex_data[2];
    int16_t x = (int16_t)((evt->sysex_data[3] << 8) | evt->sysex_data[4]);
    int16_t y = (int16_t)((evt->sysex_data[5] << 8) | evt->sysex_data[6]);
    uint8_t finger = evt->sysex_data[7];

    app_input_event_t input = {
        .type = (app_input_type_t)state, /* engine_gui: 0=down,1=move,2=up */
        .x = x,
        .y = y,
        .finger_id = finger,
        .flags = 0,
    };

    app_manager_feed_input(&input);
}

static void app_manager_handle_mouse(const engine_midi_event_t *evt)
{
    if (evt->sysex_len < 7) {
        return;
    }

    uint8_t buttons = evt->sysex_data[2];
    int16_t dx = (int16_t)((evt->sysex_data[3] << 8) | evt->sysex_data[4]);
    int16_t dy = (int16_t)((evt->sysex_data[5] << 8) | evt->sysex_data[6]);

    if (dx != 0 || dy != 0) {
        app_input_event_t move = {
            .type = APP_INPUT_MOUSE_MOVE,
            .x = dx,
            .y = dy,
            .finger_id = 0,
            .flags = buttons,
        };
        app_manager_feed_input(&move);
    }

    static uint8_t s_prev_buttons = 0;
    uint8_t changed = buttons ^ s_prev_buttons;
    for (int i = 0; i < 3; i++) {
        if (changed & (1 << i)) {
            app_input_event_t btn = {
                .type = APP_INPUT_MOUSE_BUTTON,
                .x = 0,
                .y = 0,
                .finger_id = (uint8_t)i,
                .flags = (buttons & (1 << i)) ? 1 : 0,
            };
            app_manager_feed_input(&btn);
        }
    }
    s_prev_buttons = buttons;
}

static void app_manager_handle_keyboard(const engine_midi_event_t *evt)
{
    if (evt->sysex_len < 9) {
        return;
    }

    uint8_t modifier = evt->sysex_data[2];
    uint8_t keys[6];
    for (int i = 0; i < 6; i++) {
        keys[i] = evt->sysex_data[3 + i];
    }

    static uint8_t s_prev_keys[6] = {0};

    for (int i = 0; i < 6; i++) {
        if (keys[i] == 0) {
            continue;
        }
        bool found = false;
        for (int j = 0; j < 6; j++) {
            if (s_prev_keys[j] == keys[i]) {
                found = true;
                break;
            }
        }
        if (!found) {
            app_input_event_t key_evt = {
                .type = APP_INPUT_KEYBOARD,
                .x = 0,
                .y = 0,
                .finger_id = keys[i],
                .flags = modifier,
            };
            app_manager_feed_input(&key_evt);
        }
    }

    memcpy(s_prev_keys, keys, sizeof(keys));
}

static struct {
    app_base_t *apps[APP_MANAGER_MAX_APPS];
    int count;
    app_base_t *active;
} s_mgr;

/* 异步生命周期请求队列，避免 task_comm 等任务直接调用生命周期函数 */
typedef enum {
    APP_CMD_NONE = 0,
    APP_CMD_LAUNCH,
    APP_CMD_KILL_ACTIVE,
} app_cmd_t;

static volatile app_cmd_t s_pending_cmd = APP_CMD_NONE;
static char s_pending_name[32] = {0};

/* pending 请求槽专用自旋锁：request 路径在 LVGL 任务（持 lvgl_mux）上下文执行，
 * 绝不可使用 s_lifecycle_mutex——task_app 持 lifecycle 锁执行 App 回调时会
 * 申请 lvgl 锁，LVGL 任务持 lvgl 锁申请 lifecycle 锁即构成 ABBA 死锁。
 * 因此 request 路径必须 wait-free，仅用最短临界区写请求槽。 */
static portMUX_TYPE s_pending_mux = portMUX_INITIALIZER_UNLOCKED;

/* GUI 后端回调，由 engine_gui 注册，避免 app_manager 直接依赖 engine_gui */
static app_manager_switch_screen_cb_t s_switch_screen_cb = NULL;
static app_manager_find_widget_cb_t s_find_widget_cb = NULL;

/* 递归锁：保护 active 指针读写以及 on_init/on_pause/on_resume/on_destroy/on_input/on_sysex
 * 等生命周期回调的调用，防止 task_comm（分发 SysEx）与 task_app（on_update）并发
 * 修改/访问同一个 App 实例导致数据竞争或 Use-After-Free。 */
static SemaphoreHandle_t s_lifecycle_mutex = NULL;

static bool lifecycle_take(void)
{
    if (s_lifecycle_mutex == NULL) {
        return false;
    }
    return xSemaphoreTakeRecursive(s_lifecycle_mutex, portMAX_DELAY) == pdTRUE;
}

static void lifecycle_give(void)
{
    if (s_lifecycle_mutex != NULL) {
        xSemaphoreGiveRecursive(s_lifecycle_mutex);
    }
}

static void app_manager_midi_cb(const engine_midi_event_t *evt, void *user_data)
{
    (void)user_data;

    if (evt == NULL || evt->type != ENGINE_MIDI_MSG_SYSEX) {
        return;
    }

    if (evt->sysex_len < 4) {
        return;
    }

    uint8_t cmd  = evt->sysex_data[0];
    uint8_t func = evt->sysex_data[1];

    if (cmd == MIDI_CMD_INPUT) {
        switch (func) {
        case MIDI_FUNC_INPUT_TOUCH:
            app_manager_handle_touch(evt);
            break;
        case MIDI_FUNC_INPUT_MOUSE:
            app_manager_handle_mouse(evt);
            break;
        case MIDI_FUNC_INPUT_KEYBOARD:
            app_manager_handle_keyboard(evt);
            break;
        default:
            break;
        }
        return;
    }

    if (cmd == MIDI_CMD_APP_CONTROL) {
        /* App 自己发布的 SysEx 不再回灌给当前 App，避免回环 */
        if (evt->source_port == ENGINE_MIDI_PORT_APP) {
            return;
        }

        if (!lifecycle_take()) {
            return;
        }

        app_base_t *active = s_mgr.active;
        if (active != NULL && active->on_sysex != NULL) {
            active->on_sysex(active, evt);
        }

        lifecycle_give();
        return;
    }
}

#define APP_MANAGER_NOTIFICATION_MAX_LEN 128

/* 当前可见通知（可能是 base 或插入式通知） */
static char s_notification[APP_MANAGER_NOTIFICATION_MAX_LEN];
static uint32_t s_notification_timeout_ms = 0;
static TickType_t s_notification_show_tick = 0;

/* 插入式通知背后的 base 内容及计时器 */
static char s_notification_base[APP_MANAGER_NOTIFICATION_MAX_LEN];
static uint32_t s_notification_base_timeout_ms = 0;
static TickType_t s_notification_base_show_tick = 0;
static bool s_notification_insert_active = false;

static SemaphoreHandle_t s_notification_mutex = NULL;

static void notification_set_base(const char *text, uint32_t timeout_ms)
{
    if (s_notification_mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(s_notification_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }

    if (text == NULL) {
        text = "";
    }

    strncpy(s_notification, text, sizeof(s_notification) - 1);
    s_notification[sizeof(s_notification) - 1] = '\0';

    if (timeout_ms > 0) {
        s_notification_timeout_ms = timeout_ms;
        s_notification_show_tick = xTaskGetTickCount();
    } else {
        s_notification_timeout_ms = 0;
        s_notification_show_tick = 0;
    }

    /* 抢占式通知会覆盖并结束任何插入式通知 */
    s_notification_insert_active = false;
    s_notification_base[0] = '\0';
    s_notification_base_timeout_ms = 0;
    s_notification_base_show_tick = 0;

    xSemaphoreGive(s_notification_mutex);
}

static void notification_set_insert(const char *text, uint32_t timeout_ms)
{
    if (s_notification_mutex == NULL || timeout_ms == 0) {
        return;
    }

    if (xSemaphoreTake(s_notification_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }

    if (text == NULL) {
        text = "";
    }

    /* 首次插入时备份 base，后续嵌套插入只刷新插入层，base 保持不变 */
    if (!s_notification_insert_active) {
        strncpy(s_notification_base, s_notification, sizeof(s_notification_base) - 1);
        s_notification_base[sizeof(s_notification_base) - 1] = '\0';
        s_notification_base_timeout_ms = s_notification_timeout_ms;
        s_notification_base_show_tick = s_notification_show_tick;
        s_notification_insert_active = true;
    }

    strncpy(s_notification, text, sizeof(s_notification) - 1);
    s_notification[sizeof(s_notification) - 1] = '\0';
    s_notification_timeout_ms = timeout_ms;
    s_notification_show_tick = xTaskGetTickCount();

    xSemaphoreGive(s_notification_mutex);
}

static void notification_clear_insert(void)
{
    if (s_notification_mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(s_notification_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }

    if (s_notification_insert_active) {
        strncpy(s_notification, s_notification_base, sizeof(s_notification) - 1);
        s_notification[sizeof(s_notification) - 1] = '\0';
        s_notification_timeout_ms = s_notification_base_timeout_ms;
        s_notification_show_tick = s_notification_base_show_tick;

        s_notification_insert_active = false;
        s_notification_base[0] = '\0';
        s_notification_base_timeout_ms = 0;
        s_notification_base_show_tick = 0;
    }

    xSemaphoreGive(s_notification_mutex);
}

void app_manager_register_gui_callbacks(app_manager_switch_screen_cb_t switch_cb,
                                        app_manager_find_widget_cb_t find_cb)
{
    s_switch_screen_cb = switch_cb;
    s_find_widget_cb = find_cb;
}

bool app_manager_bind_screen(app_base_t *app)
{
    if (app == NULL || s_find_widget_cb == NULL) {
        return false;
    }

    if (app->screen_ctx != NULL && app->screen_ctx_size > 0) {
        memset(app->screen_ctx, 0, app->screen_ctx_size);
    }

    if (app->widget_bindings == NULL) {
        return true;
    }

    const widget_binding_t *b = app->widget_bindings;
    while (b->name != NULL) {
        lv_obj_t **ptr = (lv_obj_t **)((uint8_t *)app->screen_ctx + b->offset);
        *ptr = s_find_widget_cb(b->name);
        if (*ptr == NULL) {
            ESP_LOGW(TAG, "screen '%s' widget not found: %s",
                     app->screen_name ? app->screen_name : "?", b->name);
        }
        b++;
    }

    return true;
}

static void app_manager_switch_screen(const char *screen_name)
{
    if (s_switch_screen_cb != NULL) {
        s_switch_screen_cb(screen_name);
    }
}

esp_err_t app_manager_init(void)
{
    memset(&s_mgr, 0, sizeof(s_mgr));
    memset(s_notification, 0, sizeof(s_notification));
    s_pending_cmd = APP_CMD_NONE;
    memset(s_pending_name, 0, sizeof(s_pending_name));
    sys_monitor_tick = 0;

    s_lifecycle_mutex = xSemaphoreCreateRecursiveMutex();
    if (s_lifecycle_mutex == NULL) {
        ESP_LOGW(TAG, "lifecycle mutex create failed");
    }

    s_notification_mutex = xSemaphoreCreateMutex();
    if (s_notification_mutex == NULL) {
        ESP_LOGW(TAG, "notification mutex create failed");
    }

    esp_err_t ret = engine_midi_subscribe(ENGINE_MIDI_MASK_SYSEX, 0xFFFF,
                                          app_manager_midi_cb, NULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "midi subscribe failed: %d", ret);
    }

    return ESP_OK;
}


void app_manager_show_notification_timeout(const char *text, uint32_t timeout_ms)
{
    notification_set_base(text, timeout_ms);
}


void app_manager_show_notificationf_timeout(uint32_t timeout_ms, const char *fmt, ...)
{
    if (fmt == NULL) {
        app_manager_show_notification_timeout(NULL, 0);
        return;
    }

    char buf[APP_MANAGER_NOTIFICATION_MAX_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    notification_set_base(buf, timeout_ms);
}

void app_manager_show_notification_insert_timeout(const char *text, uint32_t timeout_ms)
{
    if (timeout_ms == 0) {
        notification_clear_insert();
        return;
    }
    notification_set_insert(text, timeout_ms);
}

void app_manager_show_notificationf_insert_timeout(uint32_t timeout_ms, const char *fmt, ...)
{
    if (fmt == NULL || timeout_ms == 0) {
        notification_clear_insert();
        return;
    }

    char buf[APP_MANAGER_NOTIFICATION_MAX_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    notification_set_insert(buf, timeout_ms);
}

void app_manager_clear_notification(void)
{
    notification_set_base(NULL, 0);
}

void app_manager_clear_notification_insert(void)
{
    notification_clear_insert();
}

esp_err_t app_manager_get_notification(char *buf, size_t len)
{
    if (buf == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_notification_mutex == NULL) {
        buf[0] = '\0';
        return ESP_OK;
    }

    if (xSemaphoreTake(s_notification_mutex, portMAX_DELAY) != pdTRUE) {
        buf[0] = '\0';
        return ESP_FAIL;
    }

    strncpy(buf, s_notification, len - 1);
    buf[len - 1] = '\0';
    xSemaphoreGive(s_notification_mutex);
    return ESP_OK;
}

void app_manager_process(void)
{
    if (s_notification_mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(s_notification_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }

    /* 插入式通知超时后恢复 base；若 base 在插入期间也已超时则清空 */
    if (s_notification_insert_active && s_notification_timeout_ms > 0) {
        uint32_t elapsed_ms = (uint32_t)(pdTICKS_TO_MS(xTaskGetTickCount() - s_notification_show_tick));
        if (elapsed_ms >= s_notification_timeout_ms) {
            uint32_t base_elapsed_ms = 0;
            if (s_notification_base_timeout_ms > 0) {
                base_elapsed_ms = (uint32_t)(pdTICKS_TO_MS(xTaskGetTickCount() - s_notification_base_show_tick));
            }
            if (s_notification_base_timeout_ms > 0 && base_elapsed_ms >= s_notification_base_timeout_ms) {
                s_notification[0] = '\0';
                s_notification_timeout_ms = 0;
                s_notification_show_tick = 0;
            } else {
                strncpy(s_notification, s_notification_base, sizeof(s_notification) - 1);
                s_notification[sizeof(s_notification) - 1] = '\0';
                s_notification_timeout_ms = s_notification_base_timeout_ms;
                s_notification_show_tick = s_notification_base_show_tick;
            }
            s_notification_insert_active = false;
            s_notification_base[0] = '\0';
            s_notification_base_timeout_ms = 0;
            s_notification_base_show_tick = 0;
        }
    }

    /* base 自身超时 */
    if (!s_notification_insert_active && s_notification_timeout_ms > 0) {
        uint32_t elapsed_ms = (uint32_t)(pdTICKS_TO_MS(xTaskGetTickCount() - s_notification_show_tick));
        if (elapsed_ms >= s_notification_timeout_ms) {
            s_notification[0] = '\0';
            s_notification_timeout_ms = 0;
            s_notification_show_tick = 0;
        }
    }

    xSemaphoreGive(s_notification_mutex);
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

static void app_manager_all_sound_off(void)
{
    engine_midi_event_t midi = {0};
    midi.type = ENGINE_MIDI_MSG_CONTROL_CHANGE;
    midi.channel = 0;
    midi.data2 = 0;
    midi.source_port = ENGINE_MIDI_PORT_APP;

    midi.data1 = 120; /* All Sound Off */
    engine_midi_publish(&midi, 0);
    midi.data1 = 123; /* All Notes Off */
    engine_midi_publish(&midi, 0);
}

static void app_manager_reset_environment(void)
{
    app_manager_clear_notification();
    app_manager_all_sound_off();

    /* 通道音量/表情归位：防止上一个 App 残留的 CC7/CC11 污染下一个 App */
    engine_midi_event_t midi = {0};
    midi.type = ENGINE_MIDI_MSG_CONTROL_CHANGE;
    midi.source_port = ENGINE_MIDI_PORT_APP;
    for (int ch = 0; ch < 16; ch++) {
        midi.channel = (uint8_t)ch;
        midi.data1 = 7;
        midi.data2 = 127;
        engine_midi_publish(&midi, 0);
        midi.data1 = 11;
        engine_midi_publish(&midi, 0);
    }
}

void app_manager_request_launch(const char *name)
{
    if (name == NULL) {
        return;
    }

    /* wait-free：仅临界区写请求槽，不取 lifecycle 锁（防 ABBA 死锁） */
    taskENTER_CRITICAL(&s_pending_mux);
    s_pending_cmd = APP_CMD_LAUNCH;
    strncpy(s_pending_name, name, sizeof(s_pending_name) - 1);
    s_pending_name[sizeof(s_pending_name) - 1] = '\0';
    taskEXIT_CRITICAL(&s_pending_mux);
}

void app_manager_request_kill_active(void)
{
    /* wait-free：仅临界区写请求槽，不取 lifecycle 锁（防 ABBA 死锁） */
    taskENTER_CRITICAL(&s_pending_mux);
    s_pending_cmd = APP_CMD_KILL_ACTIVE;
    s_pending_name[0] = '\0';
    taskEXIT_CRITICAL(&s_pending_mux);
}

void app_manager_request_launch_by_screen(const char *screen_name)
{
    if (screen_name == NULL) {
        return;
    }

    /* 注册表仅启动期写入、s_mgr.active 为单指针原子读，无锁扫描安全（契约） */
    for (int i = 0; i < s_mgr.count; i++) {
        app_base_t *app = s_mgr.apps[i];
        if (app->screen_name != NULL && strcmp(app->screen_name, screen_name) == 0) {
            if (s_mgr.active != app) {
                taskENTER_CRITICAL(&s_pending_mux);
                s_pending_cmd = APP_CMD_LAUNCH;
                strncpy(s_pending_name, app->name, sizeof(s_pending_name) - 1);
                s_pending_name[sizeof(s_pending_name) - 1] = '\0';
                taskEXIT_CRITICAL(&s_pending_mux);
            }
            return;
        }
    }
}

void app_manager_process_requests(void)
{
    app_cmd_t cmd;
    char name[sizeof(s_pending_name)];

    taskENTER_CRITICAL(&s_pending_mux);
    cmd = s_pending_cmd;
    strncpy(name, s_pending_name, sizeof(name));
    name[sizeof(name) - 1] = '\0';
    s_pending_cmd = APP_CMD_NONE;
    taskEXIT_CRITICAL(&s_pending_mux);

    switch (cmd) {
    case APP_CMD_LAUNCH:
        app_manager_launch(name);
        break;
    case APP_CMD_KILL_ACTIVE:
        app_manager_kill_active();
        break;
    default:
        break;
    }
}

bool app_manager_launch(const char *name)
{
    if (!lifecycle_take()) {
        return false;
    }

    for (int i = 0; i < s_mgr.count; i++) {
        if (strcmp(s_mgr.apps[i]->name, name) == 0) {
            app_base_t *target = s_mgr.apps[i];

            if (s_mgr.active != NULL && s_mgr.active != target && s_mgr.active->on_pause != NULL) {
                s_mgr.active->on_pause(s_mgr.active);
            }

            app_manager_reset_environment();

            s_mgr.active = target;

            app_manager_switch_screen(target->screen_name);
            app_manager_bind_screen(target);

            if (target->on_init != NULL) {
                if (!target->on_init(target, target->screen_ctx)) {
                    /* on_init 失败（如 LVGL 锁超时）：旧逻辑无视返回值，App
                     * 带病激活成僵尸（无回调无 IMU 无 UI，on_update 空转，
                     * 真机死胡同）。回滚到 launcher 保持系统可用，用户可重点。 */
                    ESP_LOGE(TAG, "on_init failed, abort launch: %s", target->name);
                    s_mgr.active = NULL;
                    app_manager_reset_environment();
                    app_manager_switch_screen(NULL);
                    lifecycle_give();
                    return false;
                }
            }

            app_manager_log_heap(target->name);
            lifecycle_give();
            return true;
        }
    }

    lifecycle_give();
    return false;
}

bool app_manager_suspend(const char *name)
{
    bool ret = false;

    if (!lifecycle_take()) {
        return false;
    }

    if (s_mgr.active != NULL && strcmp(s_mgr.active->name, name) == 0) {
        if (s_mgr.active->on_pause != NULL) {
            s_mgr.active->on_pause(s_mgr.active);
        }
        ret = true;
    }

    lifecycle_give();
    return ret;
}

bool app_manager_resume(const char *name)
{
    bool ret = false;

    if (!lifecycle_take()) {
        return false;
    }

    for (int i = 0; i < s_mgr.count; i++) {
        if (strcmp(s_mgr.apps[i]->name, name) == 0) {
            app_base_t *target = s_mgr.apps[i];

            if (s_mgr.active != NULL && s_mgr.active != target && s_mgr.active->on_pause != NULL) {
                s_mgr.active->on_pause(s_mgr.active);
            }

            s_mgr.active = target;

            app_manager_switch_screen(target->screen_name);
            app_manager_bind_screen(target);

            if (target->on_resume != NULL) {
                target->on_resume(target);
            }

            ret = true;
            break;
        }
    }

    lifecycle_give();
    return ret;
}

bool app_manager_kill(const char *name)
{
    bool ret = false;

    if (!lifecycle_take()) {
        return false;
    }

    for (int i = 0; i < s_mgr.count; i++) {
        if (strcmp(s_mgr.apps[i]->name, name) == 0) {
            if (s_mgr.active == s_mgr.apps[i]) {
                if (s_mgr.active->on_destroy != NULL) {
                    s_mgr.active->on_destroy(s_mgr.active);
                }
                s_mgr.active = NULL;
                app_manager_reset_environment();
                app_manager_switch_screen(NULL);
            }

            for (int j = i; j < s_mgr.count - 1; j++) {
                s_mgr.apps[j] = s_mgr.apps[j + 1];
            }

            s_mgr.count--;
            ret = true;
            break;
        }
    }

    lifecycle_give();
    return ret;
}

void app_manager_kill_active(void)
{
    if (!lifecycle_take()) {
        return;
    }

    if (s_mgr.active != NULL) {
        ESP_LOGI(TAG, "kill active app: %s", s_mgr.active->name);
        if (s_mgr.active->on_destroy != NULL) {
            s_mgr.active->on_destroy(s_mgr.active);
        }
        s_mgr.active = NULL;
        app_manager_reset_environment();
        app_manager_switch_screen(NULL);
        app_manager_log_heap("kill");
    }

    lifecycle_give();
}

app_base_t *app_manager_get_active(void)
{
    return s_mgr.active;
}

void app_manager_process_active(void)
{
    if (!lifecycle_take()) {
        return;
    }

    app_base_t *active = s_mgr.active;
    if (active != NULL && active->on_update != NULL) {
        active->on_update(active);
    }

    lifecycle_give();
}

void app_manager_feed_input(const app_input_event_t *evt)
{
    if (evt == NULL || s_lifecycle_mutex == NULL) {
        return;
    }

    /* Trap: 输入路径不得无界等生命周期锁。on_update 持锁期间若 task_input
     * 在此死等，触摸队列会积压溢出（DOWN/UP 被挤出）→ 坐标类触摸命中率
     * 陡降。改有界等待：超 50ms 仍未得锁说明持锁方异常，丢弃本事件并限流
     * 告警，保证 task_input 继续消费队列不卡死。 */
    if (xSemaphoreTakeRecursive(s_lifecycle_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        static TickType_t s_last_drop_log = 0;
        TickType_t now = xTaskGetTickCount();
        if ((now - s_last_drop_log) > pdMS_TO_TICKS(2000)) {
            s_last_drop_log = now;
            ESP_LOGW(TAG, "feed_input: lifecycle lock held >50ms, input dropped");
        }
        return;
    }

    app_base_t *active = s_mgr.active;
    if (active != NULL && active->on_input != NULL) {
        active->on_input(active, evt);
    }

    xSemaphoreGiveRecursive(s_lifecycle_mutex);
}

esp_err_t app_manager_get_time(struct tm *tm)
{
    return service_rtc_get_time(tm);
}

esp_err_t app_manager_publish_sysex(uint8_t cmd, uint8_t func, uint8_t p1, uint8_t p2)
{
    engine_midi_event_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.type        = ENGINE_MIDI_MSG_SYSEX;
    evt.sysex_len   = 4;
    evt.sysex_data[0] = cmd;
    evt.sysex_data[1] = func;
    evt.sysex_data[2] = p1;
    evt.sysex_data[3] = p2;
    evt.source_port = ENGINE_MIDI_PORT_APP;
    return engine_midi_publish(&evt, 0);
}

service_recorder_result_t app_manager_record_start(const char *tag)
{
    return service_recorder_start(tag);
}

service_recorder_result_t app_manager_record_stop(void)
{
    return service_recorder_stop();
}

bool app_manager_record_is_recording(void)
{
    return service_recorder_is_recording();
}

bool app_manager_record_get_last_path(char *buf, size_t len)
{
    return service_recorder_get_last_path(buf, len);
}

esp_err_t app_manager_register_all(void)
{
    app_zen_register();
    app_ear_trainer_register();
    app_circle_of_fifths_register();
    app_chord_trainer_register();
    app_xy_pad_register();
    app_drum_pad_register();
    app_tiny_piano_register();
    app_clock_calendar_register();
    app_ai_agent_register();
    app_midi_player_register();
    app_metronome_register();
    app_fun_register();

    return ESP_OK;
}
