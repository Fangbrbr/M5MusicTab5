/**
 * @file task_ai.h
 * @brief AI 语音任务（胶水层）
 *
 * 开机创建一次 service_xiaozhi 常驻工作循环。任务栈放内部 RAM（NVS
 * flash 写入期间 cache 禁用，PSRAM 不可达）。
 */

#ifndef TASK_AI_H
#define TASK_AI_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 service_xiaozhi 队列并创建常驻 AI 任务
 *
 * 重复调用幂等；任务创建失败仅告警，AI App 进入时状态机会经事件上报错误。
 */
void task_ai_start(void);

#ifdef __cplusplus
}
#endif

#endif /* TASK_AI_H */
