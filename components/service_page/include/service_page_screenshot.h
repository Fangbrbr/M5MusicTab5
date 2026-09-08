/**
 * @file service_page_screenshot.h
 * @brief 截图功能（自 engine_gui 解耦）
 */

#ifndef SERVICE_PAGE_SCREENSHOT_H
#define SERVICE_PAGE_SCREENSHOT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 截取当前屏幕保存为 BMP 到 SD 卡 /screenshot 目录
 * @return true 保存成功
 * @note 内部含整屏重绘 + SD 写（百毫秒~秒级），仅可在 task_app 等
 *       非 task_gui 上下文调用；中断/LVGL 事件回调请用 request 接口
 */
bool service_page_take_screenshot(void);

/**
 * @brief 登记一次截图请求（任意上下文安全，幂等合并）
 *
 * 实际截图由 service_page_screenshot_process 执行。
 */
void service_page_screenshot_request(void);

/**
 * @brief 截图请求消化，挂 task_app 周期循环
 */
void service_page_screenshot_process(void);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_PAGE_SCREENSHOT_H */
