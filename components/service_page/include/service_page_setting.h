/**
 * @file service_page_setting.h
 * @brief Setting 屏幕后端事件处理
 */

#ifndef SERVICE_PAGE_SETTING_H
#define SERVICE_PAGE_SETTING_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void service_page_setting_init(void);
void service_page_setting_event(lv_event_t *e);
void service_page_setting_on_screen_loaded(void);

/**
 * @brief 每秒周期钩子（service_page 定时器在设置屏激活时调用）
 */
void service_page_setting_tick(void);

/**
 * @brief 应用持久化主题：同步设置页主题下拉选中项并注册切换回调，
 * 再刷一次当前主题（开机阶段由 engine_gui_init 调用）
 */
void service_page_setting_apply_saved_theme(void);

/**
 * @brief 按当前主题重新注入设置页 tab 按钮样式（切主题后调用）
 */
void service_page_setting_refresh_tab_styles(void);

/**
 * @brief 已持有锁的版本：供 engine_gui_set_theme 等已持锁的调用者使用
 */
void service_page_setting_refresh_tab_styles_locked(void);

/**
 * @brief 10ms 周期钩子（task_app 循环调用）
 *
 * 消化 SF2 音源切换的挂起请求：LVGL 回调（task_gui）只登记，
 * 实际加载（秒级阻塞）在本钩子内执行，避免卡死 UI 任务。
 */
void service_page_setting_process(void);

/**
 * @brief SD 卡内容可能变化（FTP 会话结束）时的音源重扫入口
 *
 * 仅登记校验请求，由 service_page_setting_process 异步执行：
 * 当前生效的 SD 音源若已被删除，自动回退内部预设并通知。
 */
void service_page_setting_sf2_on_sd_changed(void);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_PAGE_SETTING_H */
