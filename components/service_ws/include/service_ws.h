/**
 * @file service_ws.h
 * @brief 通用 WebSocket 传输服务封装
 *
 * 对 esp_websocket_client 做单例封装，所有事件先入队再由调用方 task 驱动出队。
 * 不感知任何业务协议（如 xiaozhi）。
 */

#ifndef SERVICE_WS_H
#define SERVICE_WS_H

#include "esp_err.h"
#include "stdbool.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief WebSocket 事件类型 */
typedef enum {
    SERVICE_WS_EVT_CONNECTED = 0,   /*!< 连接已建立 */
    SERVICE_WS_EVT_DISCONNECTED,    /*!< 连接已断开 */
    SERVICE_WS_EVT_TEXT,            /*!< 收到完整文本帧 */
    SERVICE_WS_EVT_BINARY,          /*!< 收到完整二进制帧 */
    SERVICE_WS_EVT_ERROR,           /*!< 发生错误 */
} service_ws_event_type_t;

/** @brief WebSocket 事件。
 * Trap: TEXT 帧 data 以 '\0' 结尾；BINARY 帧 data 为裸数据。
 * data 在回调返回后由 service_ws 释放，调用方不可留存指针。 */
typedef struct {
    service_ws_event_type_t type;
    uint8_t *data;      /*!< TEXT 以 '\0' 结尾；BINARY 为裸数据。回调期间有效 */
    uint32_t len;       /*!< data 有效字节数（不含 TEXT 的结尾 NUL） */
    int error_code;     /*!< ERROR 事件携带的传输层 errno，其余事件为 0 */
    int http_status;    /*!< ERROR 事件携带的握手 HTTP 状态码（如 401/403），无则为 0 */
} service_ws_event_t;

/** @brief 事件回调（在 service_ws_process() 调用方上下文执行） */
typedef void (*service_ws_event_cb_t)(const service_ws_event_t *evt, void *user_data);

/**
 * @brief 初始化内部资源（队列、缓冲等）
 * @return ESP_OK 已初始化或初始化成功
 */
esp_err_t service_ws_init(void);

/**
 * @brief 建立 WebSocket 连接（异步）
 *
 * @param[in] uri       wss:// 或 ws:// 地址
 * @param[in] headers   额外请求头字符串，每个 header 以 \r\n 结尾
 * @param[in] cb        事件回调
 * @param[in] user_data 回调上下文
 * @return ESP_OK 连接流程已启动
 */
esp_err_t service_ws_connect(const char *uri, const char *headers,
                             service_ws_event_cb_t cb, void *user_data);

/**
 * @brief 断开并销毁 WebSocket 客户端
 * Trap: 不可在回调上下文调用。
 */
void service_ws_disconnect(void);

/**
 * @brief 是否已连接
 */
bool service_ws_is_connected(void);

/**
 * @brief 发送文本帧
 * @return 实际发送字节数，<0 失败
 */
int service_ws_send_text(const char *data, int len);

/**
 * @brief 发送二进制帧
 * @return 实际发送字节数，<0 失败
 */
int service_ws_send_bin(const uint8_t *data, int len);

/**
 * @brief 处理内部事件队列（非阻塞）
 *
 * 由 task_comm 每周期调用，仅在调用方上下文出队并回调。
 */
void service_ws_process(void);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_WS_H */
