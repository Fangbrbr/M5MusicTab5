/**
 * @file task_input.h
 * @brief L3 Task：输入任务
 *
 * 优先级 7 | Core 0 | 周期 10 ms。负责触摸、按键、MIDI 输入轮询。
 */

#ifndef TASK_INPUT_H
#define TASK_INPUT_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动任务
 */
void task_input_start(void);

#ifdef __cplusplus
}
#endif

#endif /* TASK_INPUT_H */
