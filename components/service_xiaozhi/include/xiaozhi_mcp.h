/**
 * @file xiaozhi_mcp.h
 * @brief 小智 MCP（JSON-RPC 2.0）设备工具
 *
 * 经 xiaozhi WebSocket 文本通道收发：入站 {"type":"mcp","payload":{...}}，
 * 出站 {"session_id":...,"type":"mcp","payload":{...}}。
 * 内置工具：self.audio_speaker.set_volume、self.screen.set_brightness、
 * self.screen.set_theme、self.app.launch、self.app.exit、
 * self.get_device_status、self.get_system_info、self.reboot。
 */

#ifndef XIAOZHI_MCP_H
#define XIAOZHI_MCP_H

#include "cJSON.h"
#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief MCP 产品回调函数表（由上层实现，如 app_ai_agent） */
typedef struct {
    void *user_data;
    _Bool (*set_volume)(int v, void *user_data);
    _Bool (*set_brightness)(int v, void *user_data);
    _Bool (*set_theme)(const char *theme_str, void *user_data);
    _Bool (*app_launch)(const char *name, void *user_data);
    _Bool (*app_exit)(void *user_data);   /*!< 退出当前 App，返回主界面/Launcher */
    esp_err_t (*get_device_status)(char *out, size_t out_len, void *user_data);
    esp_err_t (*get_system_info)(char *out, size_t out_len, void *user_data);
    void (*reboot)(void *user_data);
    void (*standby)(void *user_data);   /*!< 用户道别/要求退下：收尾当前对话回待机 */
} xiaozhi_mcp_callbacks_t;

/**
 * @brief 注册 MCP 产品回调函数表
 *
 * 由 app_ai_agent 在初始化时注册，使服务器 MCP 工具调用能控制设备。
 * 传入 NULL 清空已注册回调。
 *
 * @param[in] cbs 回调函数表
 */
void xiaozhi_mcp_register_callbacks(const xiaozhi_mcp_callbacks_t *cbs);

/**
 * @brief 处理一条 MCP JSON-RPC 请求并回复
 *
 * Contract: 仅 xz_task 上下文调用；回复经 xiaozhi_ws_send_text 发出。
 *
 * @param[in] payload    mcp 消息的 payload 对象（{"jsonrpc":"2.0","id":N,"method":...}）
 * @param[in] session_id 当前会话 ID（hello 取得），用于回复封装
 */
void xiaozhi_mcp_handle(const cJSON *payload, const char *session_id);

#ifdef __cplusplus
}
#endif

#endif /* XIAOZHI_MCP_H */
