/**
 * @file service_page_onboard.h
 * @brief Onboard 引导屏幕后端事件处理
 */

#ifndef SERVICE_PAGE_ONBOARD_H
#define SERVICE_PAGE_ONBOARD_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void service_page_onboard_init(void);
void service_page_onboard_event(lv_event_t *e);
void service_page_onboard_on_screen_loaded(void);

/**
 * @brief 周期性处理（task_app 每 10ms 调用）
 *
 * - 时间保存 3s 反馈到期后恢复显示实际时间
 * - WiFi 连接状态 tip 文案刷新（状态变化回调 + 轮询去抖）
 */
void service_page_onboard_process(void);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_PAGE_ONBOARD_H */
