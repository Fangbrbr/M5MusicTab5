/**
 * @file service_page_about.c
 * @brief About 屏幕后端事件处理
 */

#include "service_page_about.h"
#include "screens.h"
#include "engine_gui.h"
#include "app_manager.h"
#include "service_i18n.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "stdio.h"

static const char *TAG = "service_page_about";

static void about_return_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "return to setting");
    engine_gui_switch_screen("setting");
}

void service_page_about_init(void)
{
    ESP_LOGI(TAG, "init");

    lvgl_port_lock(portMAX_DELAY);
    if (objects.about_btn_return != NULL) {
        lv_obj_add_event_cb(objects.about_btn_return, about_return_cb, LV_EVENT_CLICKED, NULL);
    }
    lvgl_port_unlock();
}

void service_page_about_event(lv_event_t *e)
{
    (void)e;
}

/* 格式化运行时长：自动选择秒/分/时/天单位 */
static void format_uptime(uint32_t seconds, char *buf, size_t len)
{
    if (seconds < 60) {
        snprintf(buf, len, _("系统运行时长: %lu秒"), (unsigned long)seconds);
    } else if (seconds < 3600) {
        uint32_t m = seconds / 60;
        uint32_t s = seconds % 60;
        snprintf(buf, len, _("系统运行时长: %lu分%lu秒"), (unsigned long)m, (unsigned long)s);
    } else if (seconds < 86400) {
        uint32_t h = seconds / 3600;
        uint32_t rem = seconds % 3600;
        uint32_t m = rem / 60;
        uint32_t s = rem % 60;
        snprintf(buf, len, _("系统运行时长: %lu时%lu分%lu秒"), (unsigned long)h, (unsigned long)m, (unsigned long)s);
    } else {
        uint32_t d = seconds / 86400;
        uint32_t rem = seconds % 86400;
        uint32_t h = rem / 3600;
        rem = rem % 3600;
        uint32_t m = rem / 60;
        uint32_t s = rem % 60;
        snprintf(buf, len, _("系统运行时长: %lu天%lu时%lu分%lu秒"), (unsigned long)d, (unsigned long)h, (unsigned long)m, (unsigned long)s);
    }
}

void service_page_about_tick(void)
{
    /* 每秒周期（LVGL 锁内）：about 屏激活时刷新系统运行时长 */
    if (objects.about_system_monitor_tick == NULL) {
        return;
    }
    char buf[48];
    format_uptime(sys_monitor_tick, buf, sizeof(buf));
    lv_label_set_text(objects.about_system_monitor_tick, buf);
}

void service_page_about_on_screen_loaded(void)
{
    /* 进入即刷，不等定时器下一秒 */
    service_page_about_tick();
}
