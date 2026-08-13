/**
 * @file service_page.c
 * @brief 系统屏幕事件分发入口
 */

#include "service_page.h"
#include "service_page_launcher.h"
#include "service_page_setting.h"
#include "service_page_onboard.h"
#include "service_page_about.h"
#include "screens.h"
#include "eez-flow.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"

static const char *TAG = "service_page";

/* 每秒周期定时器：在 LVGL 任务上下文（持锁）内按当前激活屏分发 tick，
 * 驱动需要持续刷新的系统屏（about 运行时长 / setting WiFi 状态文案） */
static lv_timer_t *s_tick_timer = NULL;

static void service_page_tick_cb(lv_timer_t *timer)
{
    (void)timer;
    /* 用 LVGL 当前激活屏判定（比 eez_flow 状态更可靠，避免 EEZ 导航后
     * flow 当前屏不同步导致 tick 永不命中） */
    lv_obj_t *scr = lv_screen_active();
    if (scr == objects.setting) {
        service_page_setting_tick();
    } else if (scr == objects.about) {
        service_page_about_tick();
    }
}

static lv_obj_t *service_page_current_obj(void)
{
    lv_obj_t *scr = lv_scr_act();
    if (scr == NULL) {
        return NULL;
    }

    /* EEZ 创建屏幕时根对象会设置 parent 为 NULL，因此 lv_scr_act() 返回的是当前屏根 */
    return scr;
}

void service_page_init(void)
{
    ESP_LOGI(TAG, "init");

    service_page_launcher_init();
    service_page_setting_init();
    service_page_onboard_init();
    service_page_about_init();

    lvgl_port_lock(portMAX_DELAY);
    if (s_tick_timer == NULL) {
        s_tick_timer = lv_timer_create(service_page_tick_cb, 1000, NULL);
    }
    lvgl_port_unlock();
}

void service_page_feed_event(lv_event_t *e)
{
    lv_obj_t *scr = service_page_current_obj();
    if (scr == NULL) {
        return;
    }

    if (scr == objects.launcher) {
        service_page_launcher_event(e);
    } else if (scr == objects.setting) {
        service_page_setting_event(e);
    } else if (scr == objects.onboard_step) {
        service_page_onboard_event(e);
    } else if (scr == objects.about) {
        service_page_about_event(e);
    }
}
