/**
 * @file task_comm.h
 * @brief L3 Task：通信任务
 *
 * 优先级 4 | Core 0 | 周期 10 ms。负责 MIDI 事件总线处理。
 */

#ifndef TASK_COMM_H
#define TASK_COMM_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动任务
 */
void task_comm_start(void);

#ifdef __cplusplus
}
#endif

#endif /* TASK_COMM_H */
