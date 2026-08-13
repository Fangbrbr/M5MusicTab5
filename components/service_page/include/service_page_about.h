/**
 * @file service_page_about.h
 * @brief About 屏幕后端事件处理
 */

#ifndef SERVICE_PAGE_ABOUT_H
#define SERVICE_PAGE_ABOUT_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void service_page_about_init(void);
void service_page_about_event(lv_event_t *e);

/**
 * @brief 每秒周期钩子（LVGL 定时上下文）：刷新系统运行时长
 */
void service_page_about_tick(void);

/**
 * @brief about 屏加载时立即刷新一次运行时长
 */
void service_page_about_on_screen_loaded(void);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_PAGE_ABOUT_H */
