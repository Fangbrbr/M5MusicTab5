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
 */
bool service_page_take_screenshot(void);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_PAGE_SCREENSHOT_H */
