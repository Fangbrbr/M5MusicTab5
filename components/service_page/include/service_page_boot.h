/**
 * @file service_page_boot.h
 * @brief Boot 屏幕后端（自 engine_gui 解耦）
 */

#ifndef SERVICE_PAGE_BOOT_H
#define SERVICE_PAGE_BOOT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 显示/隐藏 boot 屏依赖中文字库的 label（字体加载完成后调用）
 */
void service_page_boot_reveal_labels(void);

/**
 * @brief 按启动进度更新 boot 屏文案（>=50% 起床中，>=80% 起床啦）
 * @param[in] percent 启动进度 0~100
 */
void service_page_boot_progress_text(int percent);

/**
 * @brief 按当前语言翻译 boot 屏静态文案
 */
void service_page_boot_translate(void);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_PAGE_BOOT_H */
