/**
 * @file task_app.h
 * @brief L3 Task：App 调度任务
 *
 * 优先级 4 | Core 0 | 周期 10 ms。负责激活 App 的 on_update 调度。
 */

#ifndef TASK_APP_H
#define TASK_APP_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动任务
 */
void task_app_start(void);

#ifdef __cplusplus
}
#endif

#endif /* TASK_APP_H */
