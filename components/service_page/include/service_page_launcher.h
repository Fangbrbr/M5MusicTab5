/**
 * @file service_page_launcher.h
 * @brief Launcher 屏幕后端事件处理
 */

#ifndef SERVICE_PAGE_LAUNCHER_H
#define SERVICE_PAGE_LAUNCHER_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void service_page_launcher_init(void);
void service_page_launcher_event(lv_event_t *e);

/**
 * @brief 刷新 launcher 背景图：按当前主题选基础图，存在编号扩展图时随机选一张
 */
void service_page_launcher_refresh_bg(void);

/**
 * @brief launcher 屏加载回调：每次回到主菜单随机换一张背景图
 */
void service_page_launcher_on_screen_loaded(void);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_PAGE_LAUNCHER_H */
