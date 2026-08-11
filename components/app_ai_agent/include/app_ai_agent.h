/**
 * @file app_ai_agent.h
 * @brief 小智（xiaozhi）语音助手 App
 *
 * 纯 UI 壳：协议栈全部在 service_xiaozhi，本 App 只做气泡渲染与按键交互。
 */

#ifndef APP_AI_AGENT_H
#define APP_AI_AGENT_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_ai_agent_register(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_AI_AGENT_H */
