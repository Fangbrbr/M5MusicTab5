/**
 * @file service_page_launcher.c
 * @brief Launcher 屏幕后端事件处理
 *
 * 负责主菜单 12 个 App 图标与设置按钮的事件响应。
 */

#include "service_page_launcher.h"
#include "screens.h"
#include "app_manager.h"
#include "engine_gui.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "esp_random.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "service_page_launcher";

/* 图标索引与 App 屏幕名的映射表； 12 个 App 容器 */
typedef struct
{
    lv_obj_t **widget;
    const char *screen_name;
} launcher_app_map_t;

static const launcher_app_map_t s_app_map[] = {
    {&objects.launcher_zen, "app_zen_mode"},
    {&objects.launcher_ear, "app_ear_train"},
    {&objects.launcher_recorder, "app_recorder"},
    {&objects.launcher_chord, "app_chord_memory"},
    {&objects.launcher_midi, "app_midi_player"},
    {&objects.launcher_xy, "app_xy_mode"},
    {&objects.launcher_sequencer, "app_sequencer"},
    {&objects.launcher_piano, "app_tiny_piano"},
    {&objects.launcher_clock, "app_clock"},
    {&objects.launcher_ai, "app_ai_agent"},
    {&objects.launcher_metron, "app_metronome"},
    {&objects.launcher_fun, "app_fun"},
};

static void launcher_app_cb(lv_event_t *e)
{
    const char *screen_name = (const char *)lv_event_get_user_data(e);
    if (screen_name == NULL) {
        return;
    }
    ESP_LOGI(TAG, "launch app: %s", screen_name);
    app_manager_request_launch_by_screen(screen_name);
}

static void launcher_setting_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "open setting");
    engine_gui_switch_screen("setting");
}

void service_page_launcher_init(void)
{
    ESP_LOGI(TAG, "init");

    lvgl_port_lock(portMAX_DELAY);

    for (size_t i = 0; i < sizeof(s_app_map) / sizeof(s_app_map[0]); i++) {
        lv_obj_t *obj = *(s_app_map[i].widget);
        if (obj != NULL) {
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(obj, launcher_app_cb, LV_EVENT_CLICKED, (void *)s_app_map[i].screen_name);
        }
    }

    if (objects.launcher_btn_setting != NULL) {
        lv_obj_add_event_cb(objects.launcher_btn_setting, launcher_setting_cb, LV_EVENT_CLICKED, NULL);
    }

    lvgl_port_unlock();
}

void service_page_launcher_event(lv_event_t *e)
{
    (void)e;
}

/* launcher 背景图随机切换（自 engine_gui 迁入，系统屏后端统一维护）：
 * 优先使用编号扩展图 ui_image_bg_<主题>_<n>.bin（n=1..8，连续存在时随机
 * 选一张，可放 SD 或 SPIFFS），无扩展图时回退主题默认图。
 * hammyorange → bgday，starrynight → bgnight。
 * LVGL bg_image_src 只存指针，动态路径用 static 缓冲（背景图同一时刻仅一张）。 */
void service_page_launcher_refresh_bg(void)
{
    if (objects.launcher == NULL) {
        return;
    }

    static char s_launcher_bg_path[64];
    const char *theme = engine_gui_get_theme_name();
    const char *base = (theme != NULL && strcmp(theme, "starrynight") == 0)
                           ? "night" : "day";

    int count = 0;
    for (int n = 1; n <= 8; n++) {
        struct stat st;
        snprintf(s_launcher_bg_path, sizeof(s_launcher_bg_path),
                 "/sys/src/ui_image_bg_%s_%d.bin", base, n);
        if (stat(s_launcher_bg_path, &st) == 0) {
            count = n;
        } else {
            break;
        }
    }

    if (count > 0) {
        int pick = 1 + (int)(esp_random() % (uint32_t)count);
        snprintf(s_launcher_bg_path, sizeof(s_launcher_bg_path),
                 "/sys/src/ui_image_bg_%s_%d.bin", base, pick);
    } else {
        snprintf(s_launcher_bg_path, sizeof(s_launcher_bg_path),
                 "/sys/src/ui_image_bg_%s.bin", base);
    }

    lvgl_port_lock(portMAX_DELAY);
    lv_obj_set_style_bg_image_src(objects.launcher, s_launcher_bg_path,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_invalidate(objects.launcher);
    lvgl_port_unlock();

    ESP_LOGI(TAG, "launcher bg → %s", s_launcher_bg_path);
}

void service_page_launcher_on_screen_loaded(void)
{
    service_page_launcher_refresh_bg();
}
