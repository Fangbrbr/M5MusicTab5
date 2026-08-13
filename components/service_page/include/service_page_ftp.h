/**
 * @file service_page_ftp.h
 * @brief FTP 屏幕后端事件处理
 */

#ifndef SERVICE_PAGE_FTP_H
#define SERVICE_PAGE_FTP_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void service_page_ftp_init(void);
void service_page_ftp_event(lv_event_t *e);

/**
 * @brief 每秒周期钩子（service_page 定时器在 FTP 屏激活时调用）
 *
 * 刷新 IP/凭据、连接状态、文件名与进度条。
 */
void service_page_ftp_tick(void);

/**
 * @brief FTP 屏加载入口：禁熄屏、暂停小智、启动 FTP 服务、锁切屏与 App 启动
 */
void service_page_ftp_on_screen_loaded(void);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_PAGE_FTP_H */
