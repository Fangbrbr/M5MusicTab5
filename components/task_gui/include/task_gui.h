/**
 * @file task_gui.h
 * @brief L3 Task：GUI 任务
 *
 * 优先级 5 | Core 0 | 周期 10 ms。负责 LVGL tick 与显示刷新。
 */

#ifndef TASK_GUI_H
#define TASK_GUI_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动任务
 */
void task_gui_start(void);

#ifdef __cplusplus
}
#endif

#endif /* TASK_GUI_H */
