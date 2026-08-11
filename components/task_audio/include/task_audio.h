/**
 * @file task_audio.h
 * @brief L3 Task：音频任务
 *
 * 优先级 6 | Core 1 | 周期 10 ms。负责音频渲染块处理。
 */

#ifndef TASK_AUDIO_H
#define TASK_AUDIO_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动任务
 */
void task_audio_start(void);

#ifdef __cplusplus
}
#endif

#endif /* TASK_AUDIO_H */
