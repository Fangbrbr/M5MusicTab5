/**
 * @file engine_gui.c
 * @brief 图形引擎
 *
 * LVGL 后端初始化，10 点触摸输入设备注册。
 * EEZ Studio 生成的前端代码位于 src/ui/，UserAction 回调实现在 eez_backend.cpp 中。
 */

#include "engine_gui.h"
#include "engine_gui_keyboard.h"
#include "engine_gui_res_vfs.h"
#include "app_manager.h"
#include "screens.h"
#include "fonts.h"
#include "images.h"
#include "engine_midi.h"
#include "service_usb_host.h"
#include "service_wifi.h"
#include "service_page_onboard.h"
#include "service_page_setting.h"
#include "service_page_launcher.h"
#include "service_page_boot.h"
#include "service_page_screenshot.h"
#include "service_page_about.h"
#include "service_page_ftp.h"
#include "service_wifi.h"
#include "service_i2c.h"
#include "service_power.h"
#include "service_sd.h"
#include "service_nvs.h"
#include "service_i18n.h"
#include "service_xiaozhi.h"
#include "esp_system.h"
#include "esp_core_dump.h"
#include "esp_random.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include <sys/stat.h>
#include <math.h>
#include "service_ble_midi.h"
#include "service_rtc.h"
#include "service_audio.h"
#include "service_spiffs.h"
#include "service_sd.h"
#include "vars.h"
#include <time.h>
#include <stdlib.h>
#include "board_config.h"
#include "board_hal.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_cache.h"
#include "esp_lvgl_port.h"
#include "ui.h"
#include "src/widgets/canvas/lv_canvas.h"
#include "esp_log.h"
#include "string.h"
#include <stdint.h>

static const char *TAG = "engine_gui";

/* LVGL 堆收口到 PSRAM（sdkconfig: CONFIG_LV_USE_CUSTOM_MALLOC=y）：
 * 全部屏幕对象/样式/字形缓存经此分配，内部 RAM 仅保留 DMA 与驱动用 */
void lv_mem_init(void)
{
}

void lv_mem_deinit(void)
{
}

void lv_mem_monitor_core(lv_mem_monitor_t *mon_p)
{
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    mon_p->total_size = info.total_free_bytes + info.total_allocated_bytes;
    mon_p->free_cnt = 0;
    mon_p->free_size = info.total_free_bytes;
    mon_p->free_biggest_size = info.largest_free_block;
    mon_p->used_cnt = 0;
    mon_p->max_used = info.total_allocated_bytes;
    mon_p->used_pct = mon_p->total_size > 0
                          ? (uint8_t)((info.total_allocated_bytes * 100) / mon_p->total_size) : 0;
    mon_p->frag_pct = info.total_free_bytes > 0
                          ? (uint8_t)(100 - (info.largest_free_block * 100) / info.total_free_bytes) : 0;
}

void *lv_malloc_core(size_t size)
{
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void *lv_realloc_core(void *p, size_t new_size)
{
    return heap_caps_realloc(p, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void lv_free_core(void *p)
{
    heap_caps_free(p);
}

static bool s_usb_dev_msc_enable = true;
static int64_t s_sys_date_ms_prev = 0;
static int32_t s_sys_main_volume = 70;   /* perceptual 曲线下 70 ≈ -10 dB，安静环境合适 */
static int32_t s_sys_main_volume_prev = -1;
static int32_t s_sys_main_brightness = 30;
static int32_t s_sys_main_brightness_prev = -1;
static int32_t s_sys_synth_engine_active = 0;
static int32_t s_sys_synth_engine_active_prev = -1;
static int32_t s_boot_percent = 0;
static bool s_engine_gui_inited = false;
static char s_sys_wifi_list[256] = {0};

/* 切屏时短暂关闭背光以隐藏 LVGL 全屏逐块刷新（从上到下推进）。
 * 仅对 launcher 和 App 屏幕生效；setting/about 保持原样。
 * SCREEN_LOAD_START 关闭背光；LV_EVENT_REFR_READY（首帧渲染完成）后恢复亮度。
 * 不用 SCREEN_LOADED，它在实际渲染开始前就触发了，起不到隐藏作用。 */
static uint8_t s_backlight_saved_brightness = 0;
static bool s_backlight_load_dimmed = false;

/* 切屏锁：FTP 等独占系统屏期间为 true，engine_gui_switch_screen 直接丢弃。
 * 独占页退出路径先解锁再切屏，不受影响。 */
static bool s_screen_locked = false;

void engine_gui_translate_obj_tree(lv_obj_t *obj);

/* app_manager GUI 后端回调前向声明 */
static void engine_gui_app_switch_screen_cb(const char *screen_name);
static lv_obj_t *engine_gui_app_find_widget_cb(const char *name);

/* 切屏背光控制注册前向声明 */
static void engine_gui_register_backlight_load_hooks(void);

/* 触摸事件队列：供 task_input 消费 */
#define ENGINE_GUI_TOUCH_QUEUE_LEN 32
static QueueHandle_t s_touch_event_queue = NULL;



/* 逻辑分辨率 1280×720，flush 时旋转 90° 写入物理 720×1280 帧缓冲；板级数值见 board_config.h */
#define UI_LOGICAL_H_RES    BOARD_UI_H_RES
#define UI_LOGICAL_V_RES    BOARD_UI_V_RES

static esp_lcd_panel_handle_t s_panel = NULL;
static lv_display_t *s_disp = NULL;

/* 显示反向开关（true=270° 反向横向）；开机由 NVS 恢复，设置页硬开关即时切换 */
static bool s_disp_inverted = false;

/* DSI 物理帧缓冲：仅用于上电清零防花屏；
 * 刷新走 lvgl_port 官方路径（部分缓冲 + 软件旋转 + draw_bitmap 中转） */
static uint16_t *s_dsi_fb = NULL;
#define DSI_FB_SIZE         ((size_t)BOARD_LCD_H_RES * BOARD_LCD_V_RES * sizeof(uint16_t))

/* ui_init 加载期间临时放宽 task_wdt 超时，避免 SPIFFS 慢读取触发 idle task WDT */
static uint32_t s_wdt_orig_timeout_ms = 0;

/* 状态栏图标（FontAwesome，需与 ui_font_chinese_30 的 range 对应） */
#define STATUS_ICON_SD              "\xEF\x83\x87"  /* U+F0C7 save */
#define STATUS_ICON_WIFI            "\xEF\x87\xAB"  /* U+F1EB wifi */
#define STATUS_ICON_AI_KEY          ""  /* U+f558 book-atlas */
#define STATUS_ICON_BATTERY_FULL    "\xEF\x89\x80"  /* U+F240 */
#define STATUS_ICON_BATTERY_3QUART  "\xEF\x89\x81"  /* U+F241 */
#define STATUS_ICON_BATTERY_HALF    "\xEF\x89\x82"  /* U+F242 */
#define STATUS_ICON_BATTERY_QUART   "\xEF\x89\x83"  /* U+F243 */
#define STATUS_ICON_BATTERY_EMPTY   "\xEF\x89\x84"  /* U+F244 */
#define STATUS_ICON_HEADPHONE       "\xEF\x80\xA5"  /* U+F025 headphone */
#define STATUS_ICON_USB             "\xEF\x8A\x87"  /* U+F287 usb */

#if CONFIG_BOARD_HAS_BATTERY
/* 电池显示参数：M5Stack Tab5 为 2S 7.2V 锂电系统，INA226 总线电压即电池电压
 * 5 档电量对应的电压范围（2S 电池）：
 *   FULL:    >= 8.0V
 *   3/4:     >= 7.5V
 *   HALF:    >= 7.0V
 *   1/4:     >= 6.5V
 *   EMPTY:   <  6.5V
 * 滞回窗：避免临界电压在两个电量等级间抖动
 *   - 电压滞回：切换时额外 ±0.1V 裕量
 *   - 时间滞回：两次切换间隔至少 2 秒 */
#define BAT_VOLTAGE_FULL            8.0f
#define BAT_VOLTAGE_3QUART          7.5f
#define BAT_VOLTAGE_HALF            7.0f
#define BAT_VOLTAGE_QUART           6.5f
#define BAT_VOLTAGE_EMPTY_THRESHOLD 5.5f  /* 低于此值认为无电池 */
#define BAT_HYSTERESIS_VOLTAGE      0.1f  /* 电压滞回窗（±0.1V） */
#define BAT_HYSTERESIS_TIME_MS      20000  /* 时间滞回窗（20秒） */

/* 电池图标滞回状态 */
static int s_bat_level = -1;         /* 当前电量等级：0-4（4=满电，0=空电） */
static uint32_t s_bat_last_change_ms = 0;  /* 上次变更时间 */
#endif /* CONFIG_BOARD_HAS_BATTERY */

static char s_sys_time_display_prev[64] = {0};
static char s_sys_status_bar_prev[64] = {0};
static char s_sys_notification_bar_prev[128] = {0};


bool get_var_sys_usb_dev_msc_enable(void)
{
    return s_usb_dev_msc_enable;
}

void set_var_sys_usb_dev_msc_enable(bool value)
{
    s_usb_dev_msc_enable = value;
}

bool get_var_sys_onboard_flag(void)
{
    return service_nvs_is_initialized();
}

void set_var_sys_onboard_flag(bool value)
{
    service_nvs_set_initialized(value);
    service_nvs_commit();
}

static bool s_sys_online_status = false;

bool get_var_sys_online_status(void)
{
    return s_sys_online_status;
}

void set_var_sys_online_status(bool value)
{
    s_sys_online_status = value;
}

int32_t get_var_sys_main_volume(void)
{
    return s_sys_main_volume;
}

void set_var_sys_main_volume(int32_t value)
{
    if (value < 0) {
        value = 0;
    } else if (value > 100) {
        value = 100;
    }
    s_sys_main_volume = value;
    s_sys_main_volume_prev = value;
    service_audio_set_volume(value);
    service_nvs_set_volume((int16_t)value);
}

int32_t get_var_sys_main_brightness(void)
{
    return s_sys_main_brightness;
}

/* 亮度松手下发：setter 只记账，值稳定 300ms 后一次性下发背光+落盘，
 * 避免拖动期间打印刷屏与 NVS 频繁置脏。-1 = 无待下发值 */
static volatile int32_t s_brightness_pending = -1;
static volatile int64_t s_brightness_pending_at_us = 0;

void set_var_sys_main_brightness(int32_t value)
{
    if (value < 0) {
        value = 0;
    } else if (value > 100) {
        value = 100;
    }
    s_sys_main_brightness = value;
    s_brightness_pending = value;
    s_brightness_pending_at_us = esp_timer_get_time();
}

/* 亮度待下发收敛：值稳定超 300ms 视为松手，一次性下发并清零 */
static void engine_gui_brightness_settle(void)
{
    int32_t pending = s_brightness_pending;
    if (pending < 0) {
        return;
    }
    if (esp_timer_get_time() - s_brightness_pending_at_us < 300000) {
        return;
    }
    s_brightness_pending = -1;
    s_sys_main_brightness_prev = pending;
    board_display_brightness_set((uint8_t)pending);
    service_nvs_set_brightness((uint8_t)pending);
}

/* 外部路径（如 MCP）已自行设置硬件+NVS，此处仅回填 native 变量供设置界面绑定读取。
 * Trap: s_sys_main_brightness 为 int32，Core 0 UI 读 / task_ai 写单字长天然原子，无需加锁。 */
void engine_gui_sync_brightness(int32_t value)
{
    if (value < 0) {
        value = 0;
    } else if (value > 100) {
        value = 100;
    }
    s_sys_main_brightness = value;
    s_sys_main_brightness_prev = value;
    /* 清除 pending，防止 300ms 后 settle 用旧值覆盖外部刚设置的亮度 */
    s_brightness_pending = -1;
}

void engine_gui_sync_volume(int32_t value)
{
    if (value < 0) {
        value = 0;
    } else if (value > 100) {
        value = 100;
    }
    s_sys_main_volume = value;
    s_sys_main_volume_prev = value;
}

/* EEZ native 变量桩：前端将系统变量改为 native 后，由后端维护状态供 UI 读取。
 * 当前部分变量仍同时通过 flow 变量推送，native 取值与 flow 取值保持一致。 */
static int32_t s_sys_random = 0;
static char s_sys_version_str[32] = {0};
static char s_sys_build_str[32] = {0};
static char s_ai_agent_prompt[256] = {0};
static lv_scr_load_anim_t s_sys_anmination = LV_SCR_LOAD_ANIM_FADE_IN;

int32_t get_var_sys_boot_percent(void)
{
    return s_boot_percent;
}

void set_var_sys_boot_percent(int32_t value)
{
    s_boot_percent = value;
}

int32_t get_var_sys_random(void)
{
    return s_sys_random;
}

void set_var_sys_random(int32_t value)
{
    s_sys_random = value;
}

const char *get_var_sys_version_str(void)
{
    return s_sys_version_str;
}

void set_var_sys_version_str(const char *value)
{
    if (value == NULL) {
        s_sys_version_str[0] = '\0';
        return;
    }
    strncpy(s_sys_version_str, value, sizeof(s_sys_version_str) - 1);
    s_sys_version_str[sizeof(s_sys_version_str) - 1] = '\0';
}

const char *get_var_sys_build_str(void)
{
    return s_sys_build_str;
}

void set_var_sys_build_str(const char *value)
{
    if (value == NULL) {
        s_sys_build_str[0] = '\0';
        return;
    }
    strncpy(s_sys_build_str, value, sizeof(s_sys_build_str) - 1);
    s_sys_build_str[sizeof(s_sys_build_str) - 1] = '\0';
}

int64_t get_var_sys_date(void)
{
    return s_sys_date_ms_prev;
}

void set_var_sys_date(int64_t value)
{
    s_sys_date_ms_prev = value;
}

const char *get_var_sys_status_bar(void)
{
    return s_sys_status_bar_prev;
}

void set_var_sys_status_bar(const char *value)
{
    if (value == NULL) {
        s_sys_status_bar_prev[0] = '\0';
        return;
    }
    if (value == s_sys_status_bar_prev) {
        return;
    }
    strncpy(s_sys_status_bar_prev, value, sizeof(s_sys_status_bar_prev) - 1);
    s_sys_status_bar_prev[sizeof(s_sys_status_bar_prev) - 1] = '\0';
}

const char *get_var_sys_notification_bar(void)
{
    return s_sys_notification_bar_prev;
}

void set_var_sys_notification_bar(const char *value)
{
    if (value == NULL) {
        s_sys_notification_bar_prev[0] = '\0';
        return;
    }
    if (value == s_sys_notification_bar_prev) {
        return;
    }
    strncpy(s_sys_notification_bar_prev, value, sizeof(s_sys_notification_bar_prev) - 1);
    s_sys_notification_bar_prev[sizeof(s_sys_notification_bar_prev) - 1] = '\0';
}

const char *get_var_ai_agent_prompt(void)
{
    return s_ai_agent_prompt;
}

void set_var_ai_agent_prompt(const char *value)
{
    if (value == NULL) {
        s_ai_agent_prompt[0] = '\0';
        return;
    }
    strncpy(s_ai_agent_prompt, value, sizeof(s_ai_agent_prompt) - 1);
    s_ai_agent_prompt[sizeof(s_ai_agent_prompt) - 1] = '\0';
}

lv_scr_load_anim_t get_var_sys_anmination(void)
{
    return s_sys_anmination;
}

void set_var_sys_anmination(lv_scr_load_anim_t value)
{
    s_sys_anmination = value;
}

int32_t get_var_sys_synth_engine_active(void)
{
    return s_sys_synth_engine_active;
}

void set_var_sys_synth_engine_active(int32_t value)
{
    /* 当前仅支持 SF2 一个音频源，忽略任何切换请求。 */
    (void)value;
    s_sys_synth_engine_active = 0;
    s_sys_synth_engine_active_prev = 0;
}

const char *get_var_sys_wifi_list(void)
{
    return s_sys_wifi_list;
}

void set_var_sys_wifi_list(const char *value)
{
    if (value == NULL) {
        s_sys_wifi_list[0] = '\0';
        return;
    }
    strncpy(s_sys_wifi_list, value, sizeof(s_sys_wifi_list) - 1);
    s_sys_wifi_list[sizeof(s_sys_wifi_list) - 1] = '\0';
}

void engine_gui_set_boot_percent(int32_t percent)
{
    if (percent < 0) {
        percent = 0;
    } else if (percent > 100) {
        percent = 100;
    }
    /* 启动顺序调整后网络阶段在 READY 之后继续，其较低进度值不得回退进度条 */
    if (percent < s_boot_percent) {
        return;
    }
    s_boot_percent = percent;
    if (!s_engine_gui_inited) {
        return;
    }

    engine_gui_set_flow_var_int(FLOW_GLOBAL_VARIABLE_SYS_BOOT_PERCENT, percent);

    service_page_boot_progress_text(percent);

    if (percent >= 100) {
        bool onboard_done = get_var_sys_onboard_flag();
        if (onboard_done) {
            engine_gui_switch_to_boot_screen();
        } else {
            engine_gui_switch_screen("onboard_step");
        }
    }
}

/* EEZ 工程中的主题名顺序，需与 theme_names 及设置页下拉框选项保持一致 */
static const char * const s_theme_name_map[] = {
    "hammyorange",
    "starrynight",
};

const char *engine_gui_theme_name_by_index(uint8_t index)
{
    if (index >= sizeof(s_theme_name_map) / sizeof(s_theme_name_map[0])) {
        return s_theme_name_map[0];
    }
    return s_theme_name_map[index];
}

uint8_t engine_gui_theme_count(void)
{
    return (uint8_t)(sizeof(s_theme_name_map) / sizeof(s_theme_name_map[0]));
}

/* 前向声明 */
static uint32_t engine_gui_saved_theme_index(void);

void engine_gui_set_theme(const char *name)
{
    if (name == NULL) {
        return;
    }

    /* 获取 LVGL 锁，防止与 taskLVGL 竞争导致系统崩溃
     * AI MCP 在 task_ai 中调用此函数，必须在持锁状态下操作 LVGL */
    lvgl_port_lock(portMAX_DELAY);

    eez_flow_set_theme(name);
    service_nvs_set_theme(name);
    service_page_launcher_refresh_bg();
    /* 设置页 tab 按钮是手动注入样式，切主题后需重新应用（已持锁版本） */
    service_page_setting_refresh_tab_styles_locked();
    ESP_LOGI(TAG, "theme set: %s", name);

    lvgl_port_unlock();
}

const char *engine_gui_get_theme_name(void)
{
    static char s_theme_buf[16];

    if (service_nvs_get_theme(s_theme_buf, sizeof(s_theme_buf)) != ESP_OK ||
        s_theme_buf[0] == '\0') {
        return s_theme_name_map[0];
    }

    return s_theme_buf;
}

lv_color_t engine_gui_theme_color(uint8_t index)
{
    if (index >= ENGINE_GUI_THEME_COLOR_COUNT) {
        index = ENGINE_GUI_THEME_COLOR_COUNT - 1;
    }
    return lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][index]);
}

/* 读取持久化主题索引（未保存时回退 0） */
static uint32_t engine_gui_saved_theme_index(void)
{
    const char *saved = engine_gui_get_theme_name();
    size_t theme_count = sizeof(s_theme_name_map) / sizeof(s_theme_name_map[0]);

    for (size_t i = 0; i < theme_count; i++) {
        if (strcmp(saved, s_theme_name_map[i]) == 0) {
            return (uint32_t)i;
        }
    }
    return 0;
}

/* 极简启动画面：纯手工 LVGL 屏（不依赖 EEZ flow 与字库），用于 EEZ 全量
 * 建屏（约 2.7s）期间保持屏幕有内容。配色直接取 theme_colors 持久化索引，
 * 与 EEZ boot 页一致；ui_init 加载 EEZ boot 后由调用方回收。 */
static lv_obj_t *engine_gui_splash_show(uint32_t theme_idx)
{
    lv_obj_t *splash = lv_obj_create(NULL);
    lv_obj_set_pos(splash, 0, 0);
    lv_obj_set_size(splash, UI_LOGICAL_H_RES, UI_LOGICAL_V_RES);
    lv_obj_set_style_bg_color(splash, lv_color_hex(theme_colors[theme_idx][0]),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_flag(splash, LV_OBJ_FLAG_SCROLLABLE);

    /* 与 EEZ boot 页同款图标与位置，用户感知为同一画面 */
    lv_obj_t *img = lv_image_create(splash);
    lv_obj_set_pos(img, 540, 201);
    lv_image_set_src(img, "/sys/src/ui_image_sleepy.bin");
    lv_image_set_scale(img, 400);

    lv_screen_load(splash);
    return splash;
}

#define MULTI_TOUCH_MAX_POINTS 5
#define LVGL_TASK_PRIORITY     8
#define LVGL_BUFFER_LINES      120

static esp_lcd_touch_handle_t s_tp = NULL;
static lv_indev_t *s_indevs[MULTI_TOUCH_MAX_POINTS] = {NULL};

#define TOUCH_TRACK_DIST_MAX 120
#define TOUCH_TRACK_DIST_MAX_SQ (TOUCH_TRACK_DIST_MAX * TOUCH_TRACK_DIST_MAX)

static struct {
    int64_t last_read_us;
    uint8_t count;
    esp_lcd_touch_point_data_t points[CONFIG_ESP_LCD_TOUCH_MAX_POINTS];
} s_touch_cache = {
    .last_read_us = 0,
};

typedef struct {
    bool active;
    bool prev_active;
    uint16_t x;
    uint16_t y;
    uint16_t prev_x;
    uint16_t prev_y;
    uint16_t strength;  /* 触点强度原始值（ST7123: 8bit area；GT911: 16bit pressure） */
} touch_slot_t;

static touch_slot_t s_slots[MULTI_TOUCH_MAX_POINTS];

/* 熄屏唤醒拦截：首触仅点亮背光。手势标志吞掉当前整段手势（任意时长，
 * 全抬指才解除）；时间窗口兜底唤醒后的快速连点。 */
#define TOUCH_WAKE_INTERCEPT_MS 500
static TickType_t s_touch_swallow_until = 0;
static bool s_touch_swallow_gesture = false;

#define GUI_SYSEX_CMD_INPUT    ENUM_SYS_COMMAND_CMD_INPUT
#define GUI_SYSEX_FUNC_TOUCH   0

static void gui_publish_touch(app_input_type_t type, int16_t x, int16_t y, uint8_t finger,
                              uint8_t pressure)
{
    if (s_touch_event_queue == NULL) {
        return;
    }

    app_input_event_t evt = {
        .type = type,
        .x = x,
        .y = y,
        .finger_id = finger,
        .flags = 0,
        .pressure = pressure,
    };

    BaseType_t ret = xQueueSend(s_touch_event_queue, &evt, 0);
    if (ret != pdTRUE) {
        /* MOVE 高频可丢弃；DOWN/UP 丢弃会造成放置/释放失灵，挤出最老事件腾位 */
        if (type == APP_INPUT_TOUCH_MOVE) {
            return;
        }
        app_input_event_t old;
        if (xQueueReceive(s_touch_event_queue, &old, 0) == pdTRUE) {
            xQueueSend(s_touch_event_queue, &evt, 0);
        }
    }
}

bool engine_gui_get_touch_event(app_input_event_t *out)
{
    if (s_touch_event_queue == NULL || out == NULL) {
        return false;
    }

    return xQueueReceive(s_touch_event_queue, out, 0) == pdTRUE;
}

typedef struct {
    int slot;
    int point;
    int32_t dist;
} touch_candidate_t;

static void touch_assign_slots(void)
{
    touch_slot_t next[MULTI_TOUCH_MAX_POINTS] = {0};
    bool point_used[CONFIG_ESP_LCD_TOUCH_MAX_POINTS] = {false};

    /*
     * 1. 为每个上一帧已激活的手指，在阈值内找最近的当前点。
     *    为了避免“远手指抢走近手指的点”，按距离升序排序后再分配。
     */
    touch_candidate_t cand[MULTI_TOUCH_MAX_POINTS];
    int cand_count = 0;

    for (int i = 0; i < MULTI_TOUCH_MAX_POINTS; i++) {
        if (!s_slots[i].active) {
            continue;
        }

        int best = -1;
        int32_t best_dist = INT32_MAX;

        for (int j = 0; j < s_touch_cache.count && j < CONFIG_ESP_LCD_TOUCH_MAX_POINTS; j++) {
            int dx = (int)s_touch_cache.points[j].x - (int)s_slots[i].x;
            int dy = (int)s_touch_cache.points[j].y - (int)s_slots[i].y;
            int32_t dist = (int32_t)dx * dx + (int32_t)dy * dy;

            if (dist < best_dist) {
                best_dist = dist;
                best = j;
            }
        }

        if (best >= 0 && best_dist <= TOUCH_TRACK_DIST_MAX_SQ) {
            cand[cand_count].slot = i;
            cand[cand_count].point = best;
            cand[cand_count].dist = best_dist;
            cand_count++;
        }
    }

    /* 按距离升序排序（冒泡，最多 5 个元素） */
    for (int a = 0; a < cand_count - 1; a++) {
        for (int b = a + 1; b < cand_count; b++) {
            if (cand[b].dist < cand[a].dist) {
                touch_candidate_t tmp = cand[a];
                cand[a] = cand[b];
                cand[b] = tmp;
            }
        }
    }

    /* 优先把点分配给离它最近的手指 */
    for (int k = 0; k < cand_count; k++) {
        int i = cand[k].slot;
        int j = cand[k].point;

        if (point_used[j] || next[i].active) {
            continue;
        }

        next[i].active = true;
        next[i].x = s_touch_cache.points[j].x;
        next[i].y = s_touch_cache.points[j].y;
        next[i].strength = s_touch_cache.points[j].strength;
        point_used[j] = true;
    }

    /* 2. 剩余未分配的点占用空闲 slot */
    for (int j = 0; j < s_touch_cache.count && j < CONFIG_ESP_LCD_TOUCH_MAX_POINTS; j++) {
        if (point_used[j]) {
            continue;
        }

        int free_slot = -1;
        for (int i = 0; i < MULTI_TOUCH_MAX_POINTS; i++) {
            if (!next[i].active) {
                free_slot = i;
                break;
            }
        }

        if (free_slot < 0) {
            break;
        }

        next[free_slot].active = true;
        next[free_slot].x = s_touch_cache.points[j].x;
        next[free_slot].y = s_touch_cache.points[j].y;
        next[free_slot].strength = s_touch_cache.points[j].strength;
        point_used[j] = true;
    }

    /* 3. 保存旧状态用于事件检测，然后提交新状态 */
    for (int i = 0; i < MULTI_TOUCH_MAX_POINTS; i++) {
        next[i].prev_active = s_slots[i].active;
        next[i].prev_x = s_slots[i].x;
        next[i].prev_y = s_slots[i].y;
    }
    memcpy(s_slots, next, sizeof(s_slots));
}

/* 触摸 I2C 读缓存去重窗口（微秒）：固定 10ms，与 FREERTOS_HZ 无关 */
#define TOUCH_CACHE_WINDOW_US 10000

static void touch_cache_refresh(void)
{
    /* Trap: 去重绝不能用 tick 相等判断——窗口随 FREERTOS_HZ 缩放，
     * 用微秒时钟固定窗口，tick 无关 */
    int64_t now_us = esp_timer_get_time();
    if (now_us - s_touch_cache.last_read_us < TOUCH_CACHE_WINDOW_US) {
        return;
    }
    s_touch_cache.last_read_us = now_us;

    if (s_tp == NULL) {
        s_touch_cache.count = 0;
        touch_assign_slots();
        return;
    }

    service_i2c_take();
    esp_err_t ret = esp_lcd_touch_read_data(s_tp);
    if (ret != ESP_OK) {
        service_i2c_give();
        s_touch_cache.count = 0;
        touch_assign_slots();
        return;
    }

    ret = esp_lcd_touch_get_data(s_tp, s_touch_cache.points,
                                 &s_touch_cache.count,
                                 CONFIG_ESP_LCD_TOUCH_MAX_POINTS);
    service_i2c_give();
    if (ret != ESP_OK) {
        s_touch_cache.count = 0;
    }

    touch_assign_slots();

    /* 熄屏首触仅点亮背光：吞掉当前整段手势（全抬指解除），并在唤醒后
     * TOUCH_WAKE_INTERCEPT_MS 窗口内继续拦截后续连点/拖动，防黑屏盲点
     * 穿透到控件。
     * Trap: 手势解除必须按吞并前的原始点数判断——置零后再判会在同周期
     * 清掉标志，按住的手指超过时间窗后被重新识别为新 PRESS 穿透控件。 */
    uint8_t raw_count = s_touch_cache.count;
    /* 吞并窗口用 tick 比较但经 pdMS_TO_TICKS 换算，tick 频率无关 */
    uint32_t now = xTaskGetTickCount();
    if (service_power_is_screen_off() && raw_count > 0) {
        service_power_wake_screen();
        s_touch_swallow_until = now + pdMS_TO_TICKS(TOUCH_WAKE_INTERCEPT_MS);
        s_touch_swallow_gesture = true;
    }
    bool intercept = s_touch_swallow_gesture ||
                     ((int32_t)(now - s_touch_swallow_until) < 0);
    if (intercept && raw_count > 0) {
        s_touch_cache.count = 0;
        touch_assign_slots();
        /* 同时把 prev_active 清零，避免 multi_touch_read_cb 把“释放”误报为 UP 事件 */
        for (int i = 0; i < MULTI_TOUCH_MAX_POINTS; i++) {
            s_slots[i].prev_active = false;
        }
    }
    if (raw_count == 0) {
        s_touch_swallow_gesture = false;
    }
}

/* 将触摸控制器的原生 720×1280 坐标转换为 1280×720 逻辑坐标（随显示旋转方向） */
static inline void touch_to_logical(uint16_t raw_x, uint16_t raw_y,
                                    int16_t *log_x, int16_t *log_y)
{
    if (s_disp_inverted) {
        /* 270°：与 LVGL lv_display_rotate_point 的 ROTATION_270 同式 */
        *log_x = (int16_t)raw_y;
        *log_y = (int16_t)(BOARD_LCD_H_RES - 1 - (int)raw_x);
    } else {
        /* 90°：与 ROTATION_90 同式 */
        *log_x = (int16_t)((int)BOARD_LCD_V_RES - 1 - (int)raw_y);
        *log_y = (int16_t)raw_x;
    }
}

static uint8_t touch_strength_to_pressure(uint16_t strength)
{
    static uint16_t s_str_max = 1;  /* 自适应峰值，单调递增（reset 时复位即可） */

    if (strength > s_str_max) {
        s_str_max = (uint16_t)(((uint32_t)s_str_max * 3 + strength) / 4);
    }

    uint32_t p = ((uint32_t)strength * 127) / s_str_max;
    if (p < 1) {
        p = 1;   /* MIDI velocity 0 = note off，至少给 1 表“按下” */
    }
    if (p > 127) {
        p = 127;
    }
    return (uint8_t)p;
}

static void multi_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    int point_idx = (int)(intptr_t)lv_indev_get_driver_data(indev);

    touch_cache_refresh();

    if (point_idx < 0 || point_idx >= MULTI_TOUCH_MAX_POINTS) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    touch_slot_t *slot = &s_slots[point_idx];
    int16_t log_x = 0, log_y = 0;

    if (slot->active) {
        /* LVGL 按 display rotation 自动把物理坐标换算成逻辑坐标，上报原生值即可 */
        data->point.x = slot->x;
        data->point.y = slot->y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }

    if (slot->active != slot->prev_active) {
        slot->prev_active = slot->active;
        if (slot->active) {
            slot->prev_x = slot->x;
            slot->prev_y = slot->y;
            touch_to_logical(slot->x, slot->y, &log_x, &log_y);
            gui_publish_touch(APP_INPUT_TOUCH_DOWN, log_x, log_y, (uint8_t)point_idx,
                              touch_strength_to_pressure(slot->strength));
        } else {
            touch_to_logical(slot->prev_x, slot->prev_y, &log_x, &log_y);
            gui_publish_touch(APP_INPUT_TOUCH_UP, log_x, log_y, (uint8_t)point_idx, 0);
        }
    } else if (slot->active &&
               (slot->x != slot->prev_x || slot->y != slot->prev_y)) {
        touch_to_logical(slot->x, slot->y, &log_x, &log_y);
        gui_publish_touch(APP_INPUT_TOUCH_MOVE, log_x, log_y, (uint8_t)point_idx,
                          touch_strength_to_pressure(slot->strength));
        slot->prev_x = slot->x;
        slot->prev_y = slot->y;
    }
}

static const char *gui_app_id_to_name(int app_id)
{
    switch (app_id) {
        case ENUM_APP_APP_ZEN_MODE:         return "Zen Mode";
        case ENUM_APP_APP_XY_PAD:           return "XY Pad";
        case ENUM_APP_APP_EAR_TRAINER:      return "Ear Trainer";
        case ENUM_APP_APP_CIRCLE_OF_FIFTHS: return "Circle Of Fifths";
        case ENUM_APP_APP_CHORD_TRAIN:      return "Chord Trainer";
        case ENUM_APP_APP_MIDI_PLAYER:      return "MIDI Player";
        case ENUM_APP_APP_DRUM_PAD:         return "Drum Pad";
        case ENUM_APP_APP_TINY_PIANO:       return "Tiny Piano";
        case ENUM_APP_APP_CLOCK_CALENDAR:   return "Clock Calendar";
        case ENUM_APP_APP_AI_AGENT:         return "AI Agent";
        case ENUM_APP_APP_METRONOME:        return "Metronome";
        default:                            return NULL;
    }
}

static void gui_sysex_consumer(const engine_midi_event_t *evt, void *user_data)
{
    (void)user_data;

    if (evt->type != ENGINE_MIDI_MSG_SYSEX || evt->sysex_len < 4) {
        return;
    }

    uint8_t cmd  = evt->sysex_data[0];
    uint8_t func = evt->sysex_data[1];
    uint8_t p1   = evt->sysex_data[2];
    (void)evt->sysex_data[3];

    switch (cmd) {
        case ENUM_SYS_COMMAND_CMD_APP:
            if (func == ENUM_FUNC_APP_FUNC_APP_LAUNCH) {
                const char *name = gui_app_id_to_name((int)p1);
                ESP_LOGI(TAG, "launch app_id=%d name=%s", p1, name ? name : "?");
                if (name != NULL) {
                    app_manager_request_launch(name);
                }
            } else if (func == ENUM_FUNC_APP_FUNC_APP_KILL) {
                app_base_t *active = app_manager_get_active();
                const char *target_name = gui_app_id_to_name((int)p1);
                if (active != NULL && target_name != NULL &&
                    strcmp(active->name, target_name) == 0) {
                    ESP_LOGI(TAG, "kill app_id=%d name=%s", p1, target_name);
                    app_manager_request_kill_active();
                }
            } else if (func == ENUM_FUNC_APP_FUNC_APP_BACK) {
                app_base_t *active = app_manager_get_active();
                if (active != NULL) {
                    ESP_LOGI(TAG, "back to launcher, kill active: %s", active->name);
                    app_manager_request_kill_active();
                }
            } else if (func == ENUM_FUNC_APP_FUNC_APP_KILL_ALL) {
                app_base_t *active = app_manager_get_active();
                if (active != NULL) {
                    ESP_LOGI(TAG, "kill all requested, active: %s", active->name);
                    app_manager_request_kill_active();
                }
            }
            break;

        case ENUM_SYS_COMMAND_CMD_SYSTEM:
            switch (func) {
                case ENUM_FUNC_SYSTEM_FUNC_SYSTEM_REBOOT:
                    ESP_LOGI(TAG, "system reboot requested");
                    vTaskDelay(pdMS_TO_TICKS(100));
                    esp_restart();
                    break;

                case ENUM_FUNC_SYSTEM_FUNC_SYSTEM_SLEEP:
                    ESP_LOGI(TAG, "system sleep requested");
                    service_power_standby_touch_wakeup();
                    break;

                case ENUM_FUNC_SYSTEM_FUNC_SYSTEM_RTC_SYNC_LOCAL: {
                    int64_t date_ms = get_var_sys_date();
                    time_t utc_sec  = (time_t)((date_ms / 1000LL) - (8 * 3600));
                    struct tm tm_set;
                    localtime_r(&utc_sec, &tm_set);
                    esp_err_t ret = service_rtc_set_time(&tm_set);
                    if (ret == ESP_OK) {
                        service_rtc_sync_to_system();
                        s_sys_date_ms_prev = 0; /* 下次刷新立即把新时间推回前端 */
                        ESP_LOGI(TAG, "rtc synced from UI: %04d-%02d-%02d %02d:%02d:%02d",
                                 tm_set.tm_year + 1900, tm_set.tm_mon + 1, tm_set.tm_mday,
                                 tm_set.tm_hour, tm_set.tm_min, tm_set.tm_sec);
                    } else {
                        ESP_LOGW(TAG, "rtc sync from UI failed: %d", ret);
                    }
                    break;
                }

                case ENUM_FUNC_SYSTEM_FUNC_SYSTEM_WIFI_RESET:
                    ESP_LOGI(TAG, "wifi reset requested");
                    service_nvs_set_wifi_ssid("");
                    service_nvs_set_wifi_password("");
                    service_nvs_set_initialized(false);
                    service_nvs_commit();
                    set_var_sys_online_status(false);
                    service_wifi_start_ap();
                    break;

                case ENUM_FUNC_SYSTEM_FUNC_SYSTEM_FACTORY_RESET:
                    ESP_LOGI(TAG, "factory reset requested");
                    service_nvs_reset_to_defaults();
                    vTaskDelay(pdMS_TO_TICKS(100));
                    esp_restart();
                    break;

                case ENUM_FUNC_SYSTEM_FUNC_SYSTEM_SCREENSHOT:
                    service_page_take_screenshot();
                    break;

                case ENUM_FUNC_SYSTEM_FUNC_SYSTEM_CHANGE_LANGUAGE: {
                    static const char * const s_lang_ids[] = {"zh-CN", "en"};
                    int lang_idx = (int)p1;

                    if (lang_idx < 0 || lang_idx >= I18N_LANG_COUNT) {
                        ESP_LOGW(TAG, "change_language: invalid lang_idx=%d", lang_idx);
                        break;
                    }

                    const char *lang_id = s_lang_ids[lang_idx];
                    if (!service_i18n_set_language_by_id(lang_id)) {
                        ESP_LOGW(TAG, "change_language: unsupported lang_id=%s", lang_id);
                        break;
                    }

                    ESP_LOGI(TAG, "change_language: switched to %s", lang_id);

                    if (service_nvs_set_language(lang_id) != ESP_OK) {
                        ESP_LOGW(TAG, "change_language: save language failed");
                    }
                    if (service_nvs_commit() != ESP_OK) {
                        ESP_LOGW(TAG, "change_language: commit failed");
                    }

                    /* 切回 launcher 重新加载所有 UI 文本 */
                    lvgl_port_lock(0);
                    eez_flow_set_screen(SCREEN_ID_LAUNCHER, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0);
                    lvgl_port_unlock();
                    break;
                }

                default:
                    break;
            }
            break;

        default:
            break;
    }
}

static void engine_gui_refresh_status_bar(void)
{
    char buf[48] = {0};
    bool has_icon = false;

    if (service_sd_is_mounted()) {
        strncat(buf, STATUS_ICON_SD, sizeof(buf) - strlen(buf) - 1);
        has_icon = true;
    }

    if (service_wifi_is_connected()) {
        if (has_icon) {
            strncat(buf, " ", sizeof(buf) - strlen(buf) - 1);
        }
        strncat(buf, STATUS_ICON_WIFI, sizeof(buf) - strlen(buf) - 1);
        has_icon = true;
    }

    /* AI 图标：全局唤醒已开启（AI 助手设置项；开启前要求已完成绑定激活，
     * 由 app_ai_agent 门控保证，此处只读开关态） */
    if (service_nvs_get_xz_wake_anywhere()) {
        if (has_icon) {
            strncat(buf, " ", sizeof(buf) - strlen(buf) - 1);
        }
        strncat(buf, STATUS_ICON_AI_KEY, sizeof(buf) - strlen(buf) - 1);
        has_icon = true;
    }

    /* 耳机连接检测 */
    if (service_power_is_headphone_connected()) {
        if (has_icon) {
            strncat(buf, " ", sizeof(buf) - strlen(buf) - 1);
        }
        strncat(buf, STATUS_ICON_HEADPHONE, sizeof(buf) - strlen(buf) - 1);
        has_icon = true;
    }

    /* USB MIDI 设备连接检测 */
    if (service_usb_host_midi_connected()) {
        if (has_icon) {
            strncat(buf, " ", sizeof(buf) - strlen(buf) - 1);
        }
        strncat(buf, STATUS_ICON_USB, sizeof(buf) - strlen(buf) - 1);
        has_icon = true;
    }

#if CONFIG_BOARD_HAS_BATTERY
    /* 电池信息永远放在状态栏最后
     * 逻辑：
     *   1. 充电中（USB 供电）→ 显示满电图标
     *   2. 有电池 → 根据电压显示对应电量等级（5 档）
     *   3. 无电池 → 显示空图标
     *   滞回：电压 ±0.1V + 时间 2 秒，防止临界抖动 */
    service_power_battery_info_t bat = {0};
    if (service_power_get_battery_info(&bat) == ESP_OK) {
        if (has_icon) {
            strncat(buf, " ", sizeof(buf) - strlen(buf) - 1);
        }

        int new_level = -1;  /* -1 表示无电池 */
        bool charging = bat.is_charging;
        float v = bat.bus_voltage_v;

        if (v < BAT_VOLTAGE_EMPTY_THRESHOLD && !charging) {
            /* 无电池且未充电：显示空图标 */
            new_level = -1;
        } else if (charging) {
            /* 充电中：显示满电图标 */
            new_level = 4;
        } else {
            /* 有电池：根据电压计算电量等级 */
            if (v >= BAT_VOLTAGE_FULL) new_level = 4;
            else if (v >= BAT_VOLTAGE_3QUART) new_level = 3;
            else if (v >= BAT_VOLTAGE_HALF) new_level = 2;
            else if (v >= BAT_VOLTAGE_QUART) new_level = 1;
            else new_level = 0;
        }

        /* 滞回处理：避免临界电压在两个电量等级间抖动 */
        uint32_t now_ms = lv_tick_get();
        if (new_level != s_bat_level) {
            bool allow_change = true;

            /* 时间滞回：两次切换间隔至少 2 秒 */
            if ((now_ms - s_bat_last_change_ms) < BAT_HYSTERESIS_TIME_MS && s_bat_level >= 0) {
                allow_change = false;
            }

            /* 电压滞回：升档需要多 0.1V，降档需要少 0.1V */
            if (allow_change && s_bat_level >= 0 && new_level >= 0) {
                float threshold = 0.0f;
                int check_level = (new_level > s_bat_level) ? new_level : s_bat_level;
                switch (check_level) {
                    case 4: threshold = BAT_VOLTAGE_FULL; break;
                    case 3: threshold = BAT_VOLTAGE_3QUART; break;
                    case 2: threshold = BAT_VOLTAGE_HALF; break;
                    case 1: threshold = BAT_VOLTAGE_QUART; break;
                }
                if (new_level > s_bat_level) {
                    /* 升档：需要电压 >= 阈值 + 0.1V */
                    if (v < threshold + BAT_HYSTERESIS_VOLTAGE) {
                        allow_change = false;
                    }
                } else {
                    /* 降档：需要电压 < 阈值 - 0.1V */
                    if (v >= threshold - BAT_HYSTERESIS_VOLTAGE) {
                        allow_change = false;
                    }
                }
            }

            if (!allow_change) {
                new_level = s_bat_level;
            } else {
                /* 允许切换 */
                s_bat_level = new_level;
                s_bat_last_change_ms = now_ms;
            }
        }

        /* 选择图标 */
        const char *bat_icon = STATUS_ICON_BATTERY_EMPTY;
        switch (s_bat_level) {
            case 4: bat_icon = STATUS_ICON_BATTERY_FULL; break;
            case 3: bat_icon = STATUS_ICON_BATTERY_3QUART; break;
            case 2: bat_icon = STATUS_ICON_BATTERY_HALF; break;
            case 1: bat_icon = STATUS_ICON_BATTERY_QUART; break;
            default: bat_icon = STATUS_ICON_BATTERY_EMPTY; break;
        }
        strncat(buf, bat_icon, sizeof(buf) - strlen(buf) - 1);
        has_icon = true;
    }
#else
    /* JC4880P443 无电量计：状态栏不含电池图标，flow 变量该段保持为空 */
#endif /* CONFIG_BOARD_HAS_BATTERY */

    if (has_icon) {
        strncat(buf, "  ", sizeof(buf) - strlen(buf) - 1);
    }

    if (strcmp(buf, s_sys_status_bar_prev) == 0) {
        return;
    }

    strncpy(s_sys_status_bar_prev, buf, sizeof(s_sys_status_bar_prev) - 1);
    s_sys_status_bar_prev[sizeof(s_sys_status_bar_prev) - 1] = '\0';
    engine_gui_set_flow_var_string(FLOW_GLOBAL_VARIABLE_SYS_STATUS_BAR,
                                   s_sys_status_bar_prev);
}

/* 刷新管线：官方 lvgl_port 部分缓冲路径（无自定义 flush，无 PPA 直写）。
 * Why 放弃 PPA 直写帧缓冲：BLOCKING PPA 传输长时间独占 PSRAM 总线，与 DSI
 * DMA 持续读取帧缓冲竞争，大面积刷新必现 DPI underrun 闪屏（真机验证）。
 * 官方路径小块轮换 + memcpy 中转，总线仲裁天然留出 DMA 窗口。 */


esp_err_t engine_gui_init(void)
{
    ESP_LOGI(TAG, "init");

    /* 时区必须在 GUI 启动前就绪：状态栏时间经 localtime 渲染，若等
     * service_rtc_init（RTC 阶段）才 setenv，开机窗口期状态栏按 UTC 显示，
     * 与 App 的 CST 时间不一致。service_rtc_init 会幂等地再次设置。
     * GUI 自己渲染时间，由自己保证前置条件，不再由 main 代劳。 */
    setenv("TZ", "CST-8", 1);
    tzset();

    s_touch_event_queue = xQueueCreate(ENGINE_GUI_TOUCH_QUEUE_LEN, sizeof(app_input_event_t));
    if (s_touch_event_queue == NULL) {
        ESP_LOGE(TAG, "touch event queue create failed");
        return ESP_FAIL;
    }

    esp_err_t ret = ESP_OK;

    esp_reset_reason_t reset_reason = esp_reset_reason();
    ESP_LOGI(TAG, "reset reason=%d", (int)reset_reason);
    if (reset_reason != ESP_RST_POWERON) {
        ESP_LOGW(TAG, "warm reset: power-cycle LCD panel for real POR");
        board_display_warm_reset_power_cycle();
    }

    board_lcd_handles_t lcd_handles;
    if (board_display_create(&lcd_handles) != ESP_OK) {
        ESP_LOGE(TAG, "board_display_create failed");
        return ESP_FAIL;
    }
    s_panel = lcd_handles.panel;
    esp_lcd_panel_disp_on_off(s_panel, true);

    /* 禁止在此重跑 esp_lcd_panel_init：热复位时 GDMA 脏，重建链表触发
     * Store access fault 并陷入崩溃-重启循环；GRAM 残留靠上方电源级重启 */
    if (reset_reason != ESP_RST_POWERON && esp_core_dump_image_check() == ESP_OK) {
        /* 上次以崩溃结束：清除 coredump 避免分区残留 */
        esp_core_dump_image_erase();
    }

    lvgl_port_cfg_t lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_cfg.task_priority = 10;
    lvgl_port_cfg.task_affinity = 0;    // 任务核心0
    lvgl_port_cfg.task_stack = 12288;
    if (lvgl_port_init(&lvgl_port_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "lvgl_port_init failed");
        return ESP_FAIL;
    }

    /* 取 DSI 物理帧缓冲指针：仅上电清零用；刷新由 lvgl_port draw_bitmap 中转 */
    ret = esp_lcd_dpi_panel_get_frame_buffer(s_panel, 1, (void **)&s_dsi_fb);
    if (ret != ESP_OK || s_dsi_fb == NULL) {
        ESP_LOGE(TAG, "get dsi framebuffer failed: %d", ret);
        return ESP_FAIL;
    }
    memset(s_dsi_fb, 0, DSI_FB_SIZE);
    /* 清零后 cache 写回，确保 DSI DMA 读到全零帧而非旧数据。
     * 起始地址向下取整、结束向上取整，满足 esp_cache_msync 的对齐要求。 */
    uintptr_t fb_addr = (uintptr_t)s_dsi_fb;
    uintptr_t aligned_start = fb_addr & ~(uintptr_t)0x3F;
    uintptr_t aligned_end = (fb_addr + DSI_FB_SIZE + 0x3F) & ~(uintptr_t)0x3F;
    esp_cache_msync((void *)aligned_start, aligned_end - aligned_start,
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M);

    /* 官方配方：小块部分缓冲 + LVGL 侧软件旋转 + draw_bitmap 拷进 DSI 帧缓冲。
     * Why 缓冲放 PSRAM 而非官方的内部 DMA RAM：DSI 部分缓冲不直接交给外设
     * DMA（draw_bitmap 内部 memcpy 中转），无需 DMA 能力；内部 RAM 预算已被
     * AFE(~110KB)/WS TLS(~50KB) 占满，放 PSRAM 保命。
     * Why 不再 DIRECT + PPA：见上方刷新管线注释。 */
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = lcd_handles.io,
        .panel_handle = lcd_handles.panel,
        .control_handle = NULL,   /* BSP 未设置 control 字段，留 NULL 让 lvgl_port 回退到 panel_handle */
        .buffer_size = BOARD_LCD_H_RES * BOARD_LCD_DRAW_BUF_HEIGHT,
        .double_buffer = true,
        .hres = BOARD_LCD_H_RES,
        .vres = BOARD_LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
            .sw_rotate = true,
            .direct_mode = false,
        },
    };
    const lvgl_port_display_dsi_cfg_t dsi_cfg = {
        .flags = {
            .avoid_tearing = false,
        }
    };

    lv_display_t *disp = lvgl_port_add_disp_dsi(&disp_cfg, &dsi_cfg);
    if (disp == NULL) {
        ESP_LOGE(TAG, "lvgl_port_add_disp_dsi failed");
        return ESP_FAIL;
    }
    s_disp = disp;

    /* LVGL 管理旋转：正向横向 90° / 反向横向 270°（NVS 持久化，设置页硬开关）。
     * 逻辑 1280×720 渲染，flush 由 port 软件旋转后 draw_bitmap 进物理帧缓冲；
     * 触摸坐标由 LVGL 按旋转自动换算。
     * Trap: set_rotation 触发 RESOLUTION_CHANGED，port 据此缓存 rotation 并在
     * flush 执行旋转，因此必须在 add_disp 之后调用。 */
    s_disp_inverted = service_nvs_get_feature_flag(SERVICE_NVS_FLAG_INVERT_DISPLAY);
    lvgl_port_lock(0);
    lv_display_set_rotation(disp, s_disp_inverted ? LV_DISPLAY_ROTATION_270
                                                  : LV_DISPLAY_ROTATION_90);
    lvgl_port_unlock();

    /* 先关闭背光；等启动画面创建并 flush 后再点亮，避免显示白屏/花屏。 */
    board_display_brightness_set(0);

    int32_t initial_brightness = (int32_t)service_nvs_get_brightness();
    int32_t initial_volume = (int32_t)service_nvs_get_volume();
    s_sys_main_brightness = initial_brightness;
    s_sys_main_brightness_prev = initial_brightness;
    s_sys_main_volume = initial_volume;
    s_sys_main_volume_prev = initial_volume;

    /* 当前仅支持 SF2 一个音频源 */
    s_sys_synth_engine_active = 0;
    s_sys_synth_engine_active_prev = 0;

    /* service_audio_set_volume 只缓存音量，实际写入 codec 在 Core1 audio 任务中完成 */
    service_audio_set_volume(initial_volume);

    /* 注册 /sys/src 资源重定向 VFS，必须在 ui_init 加载字体之前。 */
    ret = engine_gui_res_vfs_register();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "res vfs register failed: %d", ret);
    }

    lvgl_port_lock(0);

    /* 替换 EEZ Flow 默认的 stopScript 钩子，避免前端表达式错误导致 assert 重启。 */
    engine_gui_install_eez_hooks();

    /* 移除 BSP 默认的单点输入设备 */
    lv_indev_t *default_indev = board_display_get_default_indev();
    if (default_indev != NULL) {
        lvgl_port_remove_touch(default_indev);
    }

    /* 创建独立触摸句柄，供 10 个输入设备共享 */
    ret = board_touch_create(&s_tp);
    if (ret != ESP_OK || s_tp == NULL) {
        lvgl_port_unlock();
        ESP_LOGE(TAG, "board_touch_create failed: %d", ret);
        return ESP_FAIL;
    }

    /* 为每个触摸点注册独立 LVGL 输入设备 */
    for (int i = 0; i < MULTI_TOUCH_MAX_POINTS; i++) {
        s_indevs[i] = lv_indev_create();
        if (s_indevs[i] == NULL) {
            continue;
        }
        lv_indev_set_type(s_indevs[i], LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(s_indevs[i], multi_touch_read_cb);
        lv_indev_set_disp(s_indevs[i], disp);
        /* indev 读周期默认绑 LV_DEF_REFR_PERIOD（随降帧到 33ms），短触连击会
         * 整个落在采样间隙被漏检；单独把读定时器拉回 10ms，与显示刷新解耦 */
        lv_timer_t *read_timer = lv_indev_get_read_timer(s_indevs[i]);
        if (read_timer != NULL) {
            lv_timer_set_period(read_timer, 10);
        }
        lv_indev_set_driver_data(s_indevs[i], (void *)(intptr_t)i);
    }

    lvgl_port_unlock();

    engine_gui_set_boot_percent(22);

    /* 记录 ui_init 耗时，用于诊断启动阶段白屏/长时间无日志问题 */
    int64_t ui_init_start_us = esp_timer_get_time();

    /* ui_init 加载字体/图片可能耗时数秒，临时放宽 task_wdt 超时阈值，
     * 避免 main 任务阻塞时触发 idle task WDT。 */
    engine_gui_wdt_relax();

    /* ui_init 与初始 Flow 变量更新需要在 LVGL 锁保护下进行，避免与 LVGL task 并发访问 */
    lvgl_port_lock(0);

    /* 注册 EEZ Flow USER1~USER4 自定义键盘地图。
     * 必须在 ui_init() 之前完成：EEZ 会在 ui_init() 里创建键盘并设置 mode=USER1，
     * 此时 LVGL 会从全局 kb_map[] 读取地图；若自定义地图尚未写入，就会显示默认全键盘。 */
    engine_gui_keyboard_init();

    /* 极简启动画面先行点亮背光：EEZ 全量建屏约 2.7s 期间保持有内容。
     * 配色取持久化主题索引（白屏闪病根因治理：首刷即为主题色，无跳变）。
     * 背光直接用 NVS 配置值（全局亮度一致，无 boot 专属下限；duty 底线由
     * board 层查找表保证 1% 可见）。NVS=0 属异常值（UI 滑条最小 1%），回退 50。 */
    uint32_t theme_idx = engine_gui_saved_theme_index();
    lv_obj_t *splash = engine_gui_splash_show(theme_idx);
    uint8_t boot_brightness = (initial_brightness > 0) ? (uint8_t)initial_brightness : 50;
    board_display_brightness_set(boot_brightness);
    lv_timer_handler();

    /* 字体经 VFS 从固件内嵌区读取；EEZ 全量创建 17 屏并加载 boot 页 */
    ui_init();

    /* 强制 boot 屏完整重绘：防止 splash 删除后 boot 屏未覆盖区域残留旧数据。 */
    lv_obj_invalidate(lv_screen_active());

    /* 字体缺失兜底：未加载成功的 binfont 回退内置字体，避免空指针渲染崩溃 */
    for (int i = 0; i < 16 && fonts[i].name != NULL; i++) {
        if (strncmp(fonts[i].name, "MONTSERRAT", 10) == 0) {
            break;
        }
        if (fonts[i].font_ptr == NULL) {
            ESP_LOGW(TAG, "font '%s' load failed, fallback to montserrat_14", fonts[i].name);
            fonts[i].font_ptr = &lv_font_montserrat_14;
        }
    }

    /* 首刷前完成，EEZ boot 上屏即为主题色 */
    service_page_setting_apply_saved_theme();

    service_page_launcher_refresh_bg();

    /* 依赖中文字库的两个 label 默认隐藏，字体加载完成后再显示；
     * 版本 label 也等到版本信息写入后再显示。 */
    service_page_boot_reveal_labels();
    service_page_boot_translate();

    /* 再刷新一次，让新显示的 label 和版本号上屏。 */
    lv_timer_handler();

    /* 回收启动画面（当前活动屏已是 EEZ boot） */
    if (splash != NULL) {
        lv_obj_delete(splash);
    }

    /* SPIFFS 挂载（字库已全量内嵌，无需赶在 ui_init 前） */
    esp_err_t spiffs_ret = service_spiffs_init();
    if (spiffs_ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS init failed: %d", spiffs_ret);
    }

    ESP_LOGI(TAG, "ui_init done in %lld ms", (esp_timer_get_time() - ui_init_start_us) / 1000);

    /* ui_init 完成，恢复原来的 WDT 超时阈值 */
    engine_gui_wdt_restore();

    s_engine_gui_inited = true;
    engine_gui_set_flow_var_int(FLOW_GLOBAL_VARIABLE_SYS_BOOT_PERCENT, s_boot_percent);

    /* 把 EEZ 的 Set Color Theme 动作挂接到后端，实现主题切换持久化 */
    engine_gui_install_color_theme_hook();

    /* 对 launcher 和 App 屏幕注册切屏背光控制：LOAD_START 关闭背光隐藏逐块刷新，
     * LOADED 后恢复用户亮度。setting/about 不处理，避免全黑。 */
    engine_gui_register_backlight_load_hooks();

    /* 清空状态栏与通知栏默认值，后续由 engine_gui 刷新 */
    engine_gui_set_flow_var_string(FLOW_GLOBAL_VARIABLE_SYS_STATUS_BAR, "");
    engine_gui_set_flow_var_string(FLOW_GLOBAL_VARIABLE_SYS_NOTIFICATION_BAR, "");

    /* 版本与构建信息：初始化时推送一次，关于页直接显示 */
    engine_gui_set_flow_var_string(FLOW_GLOBAL_VARIABLE_SYS_VERSION_STR, FIRMWARE_VERSION);
    const esp_app_desc_t *app_desc = esp_app_get_description();
    engine_gui_set_flow_var_string(FLOW_GLOBAL_VARIABLE_SYS_BUILD_STR,
                                   (app_desc != NULL) ? app_desc->date : __DATE__);

    lvgl_port_unlock();

    /* 注册 GUI 后端回调到 app_manager，使其能在生命周期切换时切屏并绑定控件 */
    app_manager_register_gui_callbacks(engine_gui_app_switch_screen_cb,
                                       engine_gui_app_find_widget_cb);

    engine_midi_subscribe(ENGINE_MIDI_MASK_SYSEX, 0, gui_sysex_consumer, NULL);

    return ESP_OK;
}

/**
 * @brief 统一刷新 EEZ Flow 全局变量
 *
 * 在 task_gui 周期 tick 中调用，所有需要主动推送到前端的系统状态
 * （连接状态、时间、音量等）均在此集中刷新，避免零散更新。
 */
static void engine_gui_refresh_vars(void)
{
    /* 系统时间：每秒从系统时钟读取一次（SNTP/RTC 已同步到系统时钟） */
    static int s_prev_sec = -1;
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_sec != s_prev_sec) {
        s_prev_sec = timeinfo.tm_sec;
        int64_t date_ms = (int64_t)((now + (8 * 3600)) * 1000LL);
        if (date_ms != s_sys_date_ms_prev) {
            s_sys_date_ms_prev = date_ms;
            engine_gui_set_flow_var_date(FLOW_GLOBAL_VARIABLE_SYS_DATE, date_ms);
        }

        /* 系统随机数：与日期同频刷新，使用硬件随机数发生器 */
        int32_t random_val = (int32_t)(esp_random() & 0x7FFFFFFF);
        s_sys_random = random_val;
        engine_gui_set_flow_var_int(FLOW_GLOBAL_VARIABLE_SYS_RANDOM, random_val);
    }

    /* 系统主音量：后台变化时同步到本地缓存，EEZ 通过 Native getter 刷新显示 */
    int32_t vol = service_audio_get_volume();
    if (vol != s_sys_main_volume_prev) {
        s_sys_main_volume_prev = vol;
        s_sys_main_volume = vol;
    }

    /* 当前仅支持 SF2 一个音频源，无需同步 */
    (void)service_audio_get_active_source();

    /* 状态栏时间字符串*/
    char time_display[10];
    sprintf(time_display, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    if (strcmp(time_display, s_sys_time_display_prev) != 0) {
        strncpy(s_sys_time_display_prev, time_display, sizeof(s_sys_time_display_prev) - 1);
        s_sys_time_display_prev[sizeof(s_sys_time_display_prev) - 1] = '\0';
        engine_gui_set_flow_var_string(FLOW_GLOBAL_VARIABLE_SYS_TIME_STR, time_display);
    }

    /* 运行级通知栏 */
    char notification[128];
    if (app_manager_get_notification(notification, sizeof(notification)) == ESP_OK) {
        if (strcmp(notification, s_sys_notification_bar_prev) != 0) {
            strncpy(s_sys_notification_bar_prev, notification,
                    sizeof(s_sys_notification_bar_prev) - 1);
            s_sys_notification_bar_prev[sizeof(s_sys_notification_bar_prev) - 1] = '\0';
            engine_gui_set_flow_var_string(FLOW_GLOBAL_VARIABLE_SYS_NOTIFICATION_BAR,
                                           s_sys_notification_bar_prev);
        }
    }

    /* 顶部状态栏图标 */
    engine_gui_refresh_status_bar();
}

/* -------------------- AI 对话中 LED 指示器（xx_led_ai 全局联动） -------------------- */

/* EEZ 已为各外部屏放置 xx_led_ai（lv_led，主题 secondary 色由生成代码上色）。
 * 本模块只负责显隐与动画：屏幕加载默认隐藏；xiaozhi 会话激活
 * （CONNECTING/LISTENING/SPEAKING）时从屏幕底部外上滑入场；会话期小幅
 * 漂浮 + 尺寸脉动；会话结束下滑出屏后隐藏。
 * Contract: 全部函数仅在 engine_gui_tick 内、持 LVGL 锁执行。
 * Trap: 坐标/尺寸与 EEZ 工程摆放一致，EEZ 改动后需同步。 */
#define AI_LED_HOME_X       BOARD_AI_LED_HOME_X
#define AI_LED_HOME_Y       BOARD_AI_LED_HOME_Y
#define AI_LED_OFF_Y        BOARD_AI_LED_OFF_Y   /* 屏高 + LED 高，完全屏外 */
#define AI_LED_HOME_W       BOARD_AI_LED_W
#define AI_LED_HOME_H       BOARD_AI_LED_H
#define AI_LED_ENTER_MS     350
#define AI_LED_EXIT_MS      280
#define AI_LED_FLOAT_MS     900
#define AI_LED_FLOAT_DY     6       /* 漂浮幅度（上下） */
#define AI_LED_PULSE_DW     6       /* 脉动宽增量 */
#define AI_LED_PULSE_DH     5       /* 脉动高增量 */

typedef enum {
    AI_LED_HIDDEN = 0,      /* 隐藏静止（默认态） */
    AI_LED_ENTERING,        /* 上滑入场中 */
    AI_LED_FLOATING,        /* 对话中漂浮 */
    AI_LED_EXITING,         /* 下滑退场中 */
} ai_led_phase_t;

static lv_obj_t      *s_ai_led = NULL;          /* 当前屏的 LED（无则 NULL） */
static lv_obj_t      *s_ai_led_screen = NULL;   /* LED 所属屏幕根对象 */
static ai_led_phase_t s_ai_led_phase = AI_LED_HIDDEN;
static bool           s_ai_conv_prev = false;

/* 屏根对象 → LED 控件名映射。
 * Why 按名运行时解析而非直接引用 objects 成员：EEZ 重导出丢过控件（成员消失
 * 即编译错误），按名查找时缺失仅该屏无 LED，构建不随前端导出漂移破裂。
 * Contract: 解析发生在 tick（ui_init 已完成），指针终身有效，首次解析后缓存。 */
typedef struct {
    lv_obj_t * const *screen;
    const char       *led_name;
    lv_obj_t         *led;      /* 解析缓存，NULL=未解析或控件不存在 */
    bool              resolved;
} ai_led_map_t;

static ai_led_map_t s_ai_led_map[] = {
    { &objects.launcher,             "launcher_led_ai", NULL, false },
    { &objects.setting,              "setting_led_ai",  NULL, false },
    { &objects.about,                "about_led_ai",    NULL, false },
    { &objects.app_zen_mode,         "zen_led_ai",      NULL, false },
    { &objects.app_ear_train,        "ear_led_ai",      NULL, false },
    { &objects.app_chord_memory,     "chord_led_ai",    NULL, false },
    { &objects.app_circle_of_fifths, "fifth_led_ai",    NULL, false },
    { &objects.app_tiny_piano,       "piano_led_ai",    NULL, false },
    { &objects.app_drum_pad,         "drum_led_ai",     NULL, false },
    { &objects.app_midi_player,      "midi_led_ai",     NULL, false },
    { &objects.app_xy_mode,          "xy_led_ai",       NULL, false },
    { &objects.app_metronome,        "metron_led_ai",   NULL, false },
    { &objects.app_clock,            "clock_led_ai",    NULL, false },
    { &objects.app_fun,              "fun_led_ai",      NULL, false },
};

static lv_obj_t *ai_led_find_for_screen(lv_obj_t *screen)
{
    for (uint32_t i = 0; i < sizeof(s_ai_led_map) / sizeof(s_ai_led_map[0]); i++) {
        if (*s_ai_led_map[i].screen == screen) {
            if (!s_ai_led_map[i].resolved) {
                s_ai_led_map[i].led = engine_gui_find_widget(s_ai_led_map[i].led_name);
                s_ai_led_map[i].resolved = true;
                if (s_ai_led_map[i].led == NULL) {
                    ESP_LOGW(TAG, "ai led widget missing in EEZ export: %s",
                             s_ai_led_map[i].led_name);
                }
            }
            return s_ai_led_map[i].led;
        }
    }
    return NULL;
}

/* 复位并隐藏（换屏/默认态）：回到 EEZ 默认坐标尺寸 */
static void ai_led_park(lv_obj_t *led)
{
    if (led == NULL) {
        return;
    }
    lv_anim_delete(led, NULL);
    lv_obj_add_flag(led, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(led, AI_LED_HOME_X, AI_LED_HOME_Y);
    lv_obj_set_size(led, AI_LED_HOME_W, AI_LED_HOME_H);
}

/* 尺寸脉动 exec：宽高绕默认值缩放，x 随宽度补偿保持水平居中 */
static void ai_led_size_exec(void *obj, int32_t v)
{
    lv_obj_t *led = (lv_obj_t *)obj;
    lv_coord_t w = AI_LED_HOME_W + (lv_coord_t)(v * AI_LED_PULSE_DW / 100);
    lv_coord_t h = AI_LED_HOME_H + (lv_coord_t)(v * AI_LED_PULSE_DH / 100);
    lv_obj_set_size(led, w, h);
    lv_obj_set_x(led, AI_LED_HOME_X - (w - AI_LED_HOME_W) / 2);
}

/* 对话期漂浮：y 上下缓动 + 尺寸脉动，playback 往复无限循环 */
static void ai_led_start_float(lv_obj_t *led)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, led);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_duration(&a, AI_LED_FLOAT_MS);
    lv_anim_set_values(&a, AI_LED_HOME_Y, AI_LED_HOME_Y - AI_LED_FLOAT_DY);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_playback_duration(&a, AI_LED_FLOAT_MS);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, led);
    lv_anim_set_exec_cb(&a, ai_led_size_exec);
    lv_anim_set_duration(&a, AI_LED_FLOAT_MS);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_playback_duration(&a, AI_LED_FLOAT_MS);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static void ai_led_enter_done(lv_anim_t *a)
{
    /* 入场途中会话可能已结束（阶段被退出接管），只在仍是入场态时接漂浮 */
    if (s_ai_led_phase != AI_LED_ENTERING) {
        return;
    }
    ai_led_start_float((lv_obj_t *)a->var);
    s_ai_led_phase = AI_LED_FLOATING;
}

static void ai_led_exit_done(lv_anim_t *a)
{
    lv_obj_t *led = (lv_obj_t *)a->var;
    if (s_ai_led_phase != AI_LED_EXITING) {
        return;
    }
    ai_led_park(led);
    s_ai_led_phase = AI_LED_HIDDEN;
}

static void ai_led_start_enter(lv_obj_t *led)
{
    lv_anim_delete(led, NULL);
    lv_obj_set_pos(led, AI_LED_HOME_X, AI_LED_OFF_Y);
    lv_obj_set_size(led, AI_LED_HOME_W, AI_LED_HOME_H);
    lv_obj_remove_flag(led, LV_OBJ_FLAG_HIDDEN);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, led);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_duration(&a, AI_LED_ENTER_MS);
    lv_anim_set_values(&a, AI_LED_OFF_Y, AI_LED_HOME_Y);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&a, ai_led_enter_done);
    lv_anim_start(&a);
    s_ai_led_phase = AI_LED_ENTERING;
}

static void ai_led_start_exit(lv_obj_t *led)
{
    lv_anim_delete(led, NULL);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, led);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_duration(&a, AI_LED_EXIT_MS);
    lv_anim_set_values(&a, (int32_t)lv_obj_get_y(led), AI_LED_OFF_Y);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_completed_cb(&a, ai_led_exit_done);
    lv_anim_start(&a);
    s_ai_led_phase = AI_LED_EXITING;
}

/* 每 tick 轮询：换屏检测（新屏默认隐藏，对话中补入场）+ 会话状态边沿 */
static void engine_gui_ai_led_tick(void)
{
    lv_obj_t *screen = lv_screen_active();
    if (screen != s_ai_led_screen) {
        ai_led_park(s_ai_led);
        s_ai_led_screen = screen;
        s_ai_led = ai_led_find_for_screen(screen);
        s_ai_led_phase = AI_LED_HIDDEN;
        if (s_ai_led != NULL) {
            ai_led_park(s_ai_led);
            if (s_ai_conv_prev) {
                ai_led_start_enter(s_ai_led);
            }
        }
    }

    service_xiaozhi_state_t st = service_xiaozhi_get_state();
    bool conv = (st == SERVICE_XIAOZHI_STATE_CONNECTING ||
                 st == SERVICE_XIAOZHI_STATE_LISTENING ||
                 st == SERVICE_XIAOZHI_STATE_SPEAKING);
    if (conv != s_ai_conv_prev) {
        s_ai_conv_prev = conv;
        if (s_ai_led != NULL) {
            if (conv) {
                ai_led_start_enter(s_ai_led);
            } else if (s_ai_led_phase == AI_LED_ENTERING || s_ai_led_phase == AI_LED_FLOATING) {
                ai_led_start_exit(s_ai_led);
            }
        }
    }
}

void engine_gui_set_display_inverted(bool inverted)
{
    if (s_disp == NULL || inverted == s_disp_inverted) {
        return;
    }
    s_disp_inverted = inverted;
    /* 递归锁：设置页回调（LVGL 任务内）与外部任务上下文均可调用 */
    lvgl_port_lock(portMAX_DELAY);
    lv_display_set_rotation(s_disp, inverted ? LV_DISPLAY_ROTATION_270
                                             : LV_DISPLAY_ROTATION_90);
    lvgl_port_unlock();
}

bool engine_gui_get_display_inverted(void)
{
    return s_disp_inverted;
}

void engine_gui_tick(void)
{
    engine_gui_brightness_settle();
    engine_gui_refresh_vars();

    lvgl_port_lock(0);
    ui_tick();
    engine_gui_ai_led_tick();
    lvgl_port_unlock();
}

/* -------------------- 资源加载期间喂狗（SPIFFS 慢路径） -------------------- */

void engine_gui_feed_wdt_during_load(void)
{
    /* 仅当当前任务已加入 task_wdt 时 reset，避免 "task not found" 报错。
     * 调用前通常已通过 engine_gui_wdt_relax()/restore() 调整超时阈值。 */
    if (esp_task_wdt_status(NULL) == ESP_OK) {
        esp_task_wdt_reset();
    }
}

/* ui_init 等大段阻塞操作前临时放宽 WDT，操作后恢复。
 * sdkconfig 中的 timeout 为原始阈值， relax 时改大到 30s。 */
void engine_gui_wdt_relax(void)
{
    s_wdt_orig_timeout_ms = CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000;
    esp_task_wdt_config_t cfg = {
        .timeout_ms = 30000,
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1
        .idle_core_mask = (1 << 0) | (1 << 1),
#else
        .idle_core_mask = (1 << 0),
#endif
        .trigger_panic = false,
    };
    esp_task_wdt_reconfigure(&cfg);
}

void engine_gui_wdt_restore(void)
{
    if (s_wdt_orig_timeout_ms == 0) {
        return;
    }
    esp_task_wdt_config_t cfg = {
        .timeout_ms = s_wdt_orig_timeout_ms,
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1
        .idle_core_mask = (1 << 0) | (1 << 1),
#else
        .idle_core_mask = (1 << 0),
#endif
        .trigger_panic = false,
    };
    esp_task_wdt_reconfigure(&cfg);
    s_wdt_orig_timeout_ms = 0;
}

/* -------------------------------------------------------------------------- */
/* 通用 UI 控件查找（按 Widget Name）                                           */
/* -------------------------------------------------------------------------- */

/* 由 eez_backend.cpp 提供，通过 EEZ Flow 的 name->index->object 映射查找控件 */
extern lv_obj_t *engine_gui_find_widget_by_name(const char *name);

lv_obj_t *engine_gui_find_widget(const char *name)
{
    if (name == NULL) {
        return NULL;
    }

    /* 优先走 EEZ Flow 的 Widget Name 映射，支持所有在 EEZ Studio 中命名的控件 */
    lv_obj_t *obj = engine_gui_find_widget_by_name(name);
    if (obj != NULL) {
        return obj;
    }

    return NULL;
}

/* -------------------------------------------------------------------------- */
/* 屏幕名与 SCREEN_ID 映射                                                     */
/* -------------------------------------------------------------------------- */

int16_t engine_gui_screen_name_to_id(const char *name)
{
    if (name == NULL) {
        return -1;
    }

    if (strcmp(name, "boot") == 0) {
        return SCREEN_ID_BOOT;
    }
    if (strcmp(name, "onboard_step") == 0) {
        return SCREEN_ID_ONBOARD_STEP;
    }
    if (strcmp(name, "launcher") == 0) {
        return SCREEN_ID_LAUNCHER;
    }
    if (strcmp(name, "setting") == 0) {
        return SCREEN_ID_SETTING;
    }
    if (strcmp(name, "about") == 0) {
        return SCREEN_ID_ABOUT;
    }
    if (strcmp(name, "app_zen_mode") == 0) {
        return SCREEN_ID_APP_ZEN_MODE;
    }
    if (strcmp(name, "app_xy_mode") == 0) {
        return SCREEN_ID_APP_XY_MODE;
    }
    if (strcmp(name, "app_ai_agent") == 0) {
        return SCREEN_ID_APP_AI_AGENT;
    }
    if (strcmp(name, "app_drum_pad") == 0) {
        return SCREEN_ID_APP_DRUM_PAD;
    }
    if (strcmp(name, "app_circle_of_fifths") == 0) {
        return SCREEN_ID_APP_CIRCLE_OF_FIFTHS;
    }
    if (strcmp(name, "app_ear_train") == 0) {
        return SCREEN_ID_APP_EAR_TRAIN;
    }
    if (strcmp(name, "app_chord_memory") == 0) {
        return SCREEN_ID_APP_CHORD_MEMORY;
    }
    if (strcmp(name, "app_tiny_piano") == 0) {
        return SCREEN_ID_APP_TINY_PIANO;
    }
    if (strcmp(name, "app_clock") == 0) {
        return SCREEN_ID_APP_CLOCK;
    }
    if (strcmp(name, "app_midi_player") == 0) {
        return SCREEN_ID_APP_MIDI_PLAYER;
    }
    if (strcmp(name, "app_metronome") == 0) {
        return SCREEN_ID_APP_METRONOME;
    }
    if (strcmp(name, "app_fun") == 0) {
        return SCREEN_ID_APP_FUN;
    }
    if (strcmp(name, "ftp") == 0) {
        return SCREEN_ID_FTP;
    }

    return -1;
}

/* 递归隐藏屏幕内所有 canvas：防止 App 还没来得及分配/填充 buffer 时，
 * task_gui 先刷新一帧 uninitialized PSRAM，造成花屏。 */
static void engine_gui_hide_canvas_recursive(lv_obj_t *obj)
{
    if (obj == NULL) {
        return;
    }
    if (lv_obj_get_class(obj) == &lv_canvas_class) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
    uint32_t cnt = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < cnt; i++) {
        engine_gui_hide_canvas_recursive(lv_obj_get_child(obj, (int32_t)i));
    }
}

/* 递归翻译对象树内所有可翻译文案：EEZ 屏终身缓存只建一次（_() 只在创建时求值），
 * 运行时切语言靠本函数就地改写。service_i18n_translate 双向查表（译文可反查词条），
 * 未命中词条表的文本（动态内容/无翻译）原样返回，不重写。 */
void engine_gui_translate_obj_tree(lv_obj_t *obj)
{
    if (obj == NULL) {
        return;
    }
    if (lv_obj_check_type(obj, &lv_label_class)) {
        const char *txt = lv_label_get_text(obj);
        if (txt != NULL && txt[0] != '\0') {
            const char *tr = service_i18n_translate(txt);
            if (tr != txt && strcmp(tr, txt) != 0) {
                lv_label_set_text(obj, tr);
            }
        }
    } else if (lv_obj_check_type(obj, &lv_dropdown_class)) {
        const char *opts = lv_dropdown_get_options(obj);
        if (opts != NULL && opts[0] != '\0') {
            const char *tr = service_i18n_translate(opts);
            if (tr != opts && strcmp(tr, opts) != 0) {
                lv_dropdown_set_options(obj, tr);
            }
        }
    } else if (lv_obj_check_type(obj, &lv_roller_class)) {
        const char *opts = lv_roller_get_options(obj);
        if (opts != NULL && opts[0] != '\0') {
            const char *tr = service_i18n_translate(opts);
            if (tr != opts && strcmp(tr, opts) != 0) {
                lv_roller_set_options(obj, tr, LV_ROLLER_MODE_NORMAL);
            }
        }
    }
    uint32_t cnt = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < cnt; i++) {
        engine_gui_translate_obj_tree(lv_obj_get_child(obj, (int32_t)i));
    }
}

void engine_gui_switch_screen(const char *screen_name)
{
    if (s_screen_locked) {
        ESP_LOGW(TAG, "screen switch locked, drop: %s",
                 screen_name != NULL ? screen_name : "launcher");
        return;
    }

    lvgl_port_lock(0);

    if (screen_name == NULL) {
        eez_flow_set_screen(SCREEN_ID_LAUNCHER, LV_SCR_LOAD_ANIM_NONE, 0, 0);
    } else {
        int16_t id = engine_gui_screen_name_to_id(screen_name);
        if (id >= 0) {
            eez_flow_set_screen(id, LV_SCR_LOAD_ANIM_NONE, 0, 0);
        } else {
            ESP_LOGW(TAG, "unknown screen: %s", screen_name);
        }
    }

    /* 标记整个屏幕为脏区域，强制完整重绘。
     * 防止新屏幕未覆盖区域残留旧帧数据（残影）。 */
    lv_obj_invalidate(lv_screen_active());

    engine_gui_hide_canvas_recursive(lv_screen_active());
    engine_gui_translate_obj_tree(lv_screen_active());

    lvgl_port_unlock();
}

void engine_gui_set_screen_locked(bool locked)
{
    s_screen_locked = locked;
    ESP_LOGI(TAG, "screen locked: %d", (int)locked);
}

bool engine_gui_is_screen_locked(void)
{
    return s_screen_locked;
}

/* 开机默认页面映射：与 EEZ 下拉框顺序一致 */
static const char * const s_boot_screen_map[] = {
    "launcher",      /* 主选单 */
    "app_clock",     /* 时钟 */
    "app_tiny_piano",/* 小钢琴 */
    "app_ai_agent",  /* AI 导师 */
    "app_zen_mode",  /* 禅模式 */
};

void engine_gui_switch_to_boot_screen(void)
{
    uint8_t idx = service_nvs_get_boot_screen_index();
    if (idx >= sizeof(s_boot_screen_map) / sizeof(s_boot_screen_map[0])) {
        idx = 0;
    }
    ESP_LOGI(TAG, "switch to boot screen index %u: %s", (unsigned)idx, s_boot_screen_map[idx]);
    if(idx > 0)
        app_manager_request_launch_by_screen(s_boot_screen_map[idx]);
    else
        engine_gui_switch_screen(s_boot_screen_map[idx]);
}

void engine_gui_on_screen_loaded(lv_obj_t *screen)
{
    if (screen == NULL) {
        return;
    }

    /* 切屏时隐藏 canvas 防未初始化 PSRAM 上屏花屏。注：不在此额外全屏
     * 失效——屏幕切换 LVGL 本就会整屏重绘，额外 invalidate 只会加重 PSRAM
     * 总线压力加剧 DPI underrun；灰白残影属热复位面板重同步问题，非脏区。 */
    lvgl_port_lock(0);
    engine_gui_hide_canvas_recursive(lv_screen_active());
    lvgl_port_unlock();

    const char *screen_name = NULL;
    if (screen == objects.app_zen_mode) screen_name = "app_zen_mode";
    else if (screen == objects.app_ear_train) screen_name = "app_ear_train";
    else if (screen == objects.app_chord_memory) screen_name = "app_chord_memory";
    else if (screen == objects.app_circle_of_fifths) screen_name = "app_circle_of_fifths";
    else if (screen == objects.app_tiny_piano) screen_name = "app_tiny_piano";
    else if (screen == objects.app_drum_pad) screen_name = "app_drum_pad";
    else if (screen == objects.app_midi_player) screen_name = "app_midi_player";
    else if (screen == objects.app_xy_mode) screen_name = "app_xy_mode";
    else if (screen == objects.app_metronome) screen_name = "app_metronome";
    else if (screen == objects.app_ai_agent) screen_name = "app_ai_agent";
    else if (screen == objects.app_clock) screen_name = "app_clock";
    else if (screen == objects.app_fun) screen_name = "app_fun";

    if (screen_name != NULL) {
        ESP_LOGI(TAG, "screen loaded: %s, request launch", screen_name);
        app_manager_request_launch_by_screen(screen_name);
        return;
    }

    /* 非 App 屏幕：退出当前 App，返回系统界面 */
    if (screen == objects.setting) {
        ESP_LOGI(TAG, "setting screen loaded");
        service_page_setting_on_screen_loaded();
        return;
    }

    /* 回到 launcher：交由系统屏后端随机换一张背景图 */
    if (screen == objects.launcher) {
        service_page_launcher_on_screen_loaded();
    }
    if (screen == objects.about) {
        /* 进入即刷一次运行时长，不等下一秒定时器 */
        service_page_about_on_screen_loaded();
    }
    if (screen == objects.onboard_step) {
        ESP_LOGI(TAG, "onboard screen loaded");
        service_page_onboard_on_screen_loaded();
        return;
    }

    /* FTP 独占屏：进入即启动服务并锁系统；不加 return，落到尾部自动
     * kill active App 完成独占（kill 走异步 request，不受 launch 锁影响） */
    if (screen == objects.ftp) {
        ESP_LOGI(TAG, "ftp screen loaded");
        service_page_ftp_on_screen_loaded();
    }

    if (app_manager_get_active() != NULL) {
        ESP_LOGI(TAG, "system screen loaded, kill active app");
        app_manager_request_kill_active();
    }
}

/* app_manager GUI 后端回调 */
static void engine_gui_app_switch_screen_cb(const char *screen_name)
{
    engine_gui_switch_screen(screen_name);
}

static lv_obj_t *engine_gui_app_find_widget_cb(const char *name)
{
    return engine_gui_find_widget(name);
}

/**
 * @brief 切屏背光控制回调。
 *
 * 注册到 launcher/App 屏幕：新屏开始加载时关闭背光。
 * 同时注册到 display：首帧刷新完成（LV_EVENT_REFR_READY）后恢复亮度。
 * setting/about 等系统屏不注册，保持正常刷新。
 * 若当前已处于自动熄屏黑屏状态，则不再重复关闭，也不恢复。
 */
static void engine_gui_backlight_load_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_SCREEN_LOAD_START) {
        if (s_backlight_load_dimmed) {
            return; /* 已有未完成的加载，避免重复记录 */
        }
        /* 已在熄屏状态则保持黑屏，不干预自动熄屏逻辑 */
        if (service_power_is_screen_off()) {
            return;
        }

        uint8_t b = service_nvs_get_brightness();
        if (b == 0) {
            b = 30; /* 避免 0 亮度导致恢复后看似黑屏 */
        }
        s_backlight_saved_brightness = b;
        board_display_brightness_set(0);
        s_backlight_load_dimmed = true;
        ESP_LOGD(TAG, "screen load start: backlight off");
    } else if (code == LV_EVENT_REFR_READY) {
        if (!s_backlight_load_dimmed) {
            return;
        }

        /* 熄屏期间被自动熄屏接管，不主动恢复 */
        if (!service_power_is_screen_off()) {
            board_display_brightness_set(s_backlight_saved_brightness);
        }
        s_backlight_load_dimmed = false;
        ESP_LOGD(TAG, "refr ready: backlight %u", s_backlight_saved_brightness);
    }
}

/* 注册需要切屏背光控制的屏幕：launcher + 12 个 App 屏 */
static void engine_gui_register_backlight_load_hooks(void)
{
    lv_obj_t *screens[] = {
        objects.launcher,
        objects.app_zen_mode,
        objects.app_ear_train,
        objects.app_chord_memory,
        objects.app_circle_of_fifths,
        objects.app_tiny_piano,
        objects.app_drum_pad,
        objects.app_midi_player,
        objects.app_xy_mode,
        objects.app_metronome,
        objects.app_ai_agent,
        objects.app_clock,
        objects.app_fun,
    };

    for (size_t i = 0; i < sizeof(screens) / sizeof(screens[0]); i++) {
        if (screens[i] != NULL) {
            lv_obj_add_event_cb(screens[i], engine_gui_backlight_load_cb,
                                LV_EVENT_SCREEN_LOAD_START, NULL);
        }
    }

    /* 在 display 上监听 REFR_READY，用于在首帧渲染完成后恢复背光 */
    if (s_disp != NULL) {
        lv_display_add_event_cb(s_disp, engine_gui_backlight_load_cb,
                                  LV_EVENT_REFR_READY, NULL);
    }
}


