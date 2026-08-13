/**
 * @file service_page.h
 * @brief 系统屏幕（launcher/setting/about/onboard）后端事件处理入口
 *
 * 系统屏幕不是 App，不经过 app_manager 生命周期；本模块集中处理这些屏幕的
 * LVGL 控件事件与导航逻辑，避免把业务逻辑分散到 engine_gui 或 app_manager。
 */

#ifndef SERVICE_PAGE_H
#define SERVICE_PAGE_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化系统屏幕事件处理器
 *
 * 在 engine_gui_init 之后调用，注册系统屏幕相关控件事件。
 */
void service_page_init(void);

/**
 * @brief 分发系统屏幕的 LVGL 控件事件
 *
 * 由 engine_gui 的 action_widget_event 在当前无激活 App 时调用。
 *
 * @param[in] e LVGL 事件
 */
void service_page_feed_event(lv_event_t *e);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_PAGE_H */
