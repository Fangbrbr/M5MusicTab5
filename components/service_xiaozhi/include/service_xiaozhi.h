/**
 * @file service_xiaozhi.h
 * @brief XiaoZhi voice assistant core protocol stack - state machine and event queue only
 */

#ifndef SERVICE_XIAOZHI_H
#define SERVICE_XIAOZHI_H

#include "esp_err.h"
#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Protocol state machine states
 */
typedef enum {
    SERVICE_XIAOZHI_STATE_IDLE = 0,
    SERVICE_XIAOZHI_STATE_ACTIVATING,
    SERVICE_XIAOZHI_STATE_READY,
    SERVICE_XIAOZHI_STATE_CONNECTING,
    SERVICE_XIAOZHI_STATE_LISTENING,
    SERVICE_XIAOZHI_STATE_SPEAKING,
    SERVICE_XIAOZHI_STATE_ERROR
} service_xiaozhi_state_t;

/**
 * @brief App-side event types
 */
typedef enum {
    SERVICE_XIAOZHI_EVT_CONNECTED = 0,
    SERVICE_XIAOZHI_EVT_DISCONNECTED,
    SERVICE_XIAOZHI_EVT_HELLO,
    SERVICE_XIAOZHI_EVT_TTS_START,
    SERVICE_XIAOZHI_EVT_TTS_STOP,
    SERVICE_XIAOZHI_EVT_TTS_SENTENCE,
    SERVICE_XIAOZHI_EVT_STT,
    SERVICE_XIAOZHI_EVT_ALERT,
    SERVICE_XIAOZHI_EVT_SYSTEM_REBOOT,
    SERVICE_XIAOZHI_EVT_MCP,
    SERVICE_XIAOZHI_EVT_STATE,          /* 状态变更 */
    SERVICE_XIAOZHI_EVT_ERROR,          /* 错误/告警 */
    SERVICE_XIAOZHI_EVT_ACTIVATION_CODE,/* 激活码显示 */
    SERVICE_XIAOZHI_EVT_VAD             /* VAD 边沿（废弃）*/
} service_xiaozhi_event_type_t;

/**
 * @brief Event structure for app notification
 *
 * Trap: text 为值语义（队列内嵌缓冲），禁止改成指针——事件经队列跨任务
 * 传递，指针会在生产者下次复用栈槽后悬垂，导致多条事件串成同一文本。
 */
typedef struct {
    service_xiaozhi_event_type_t type;
    service_xiaozhi_state_t state;
    char text[256];             /* 无文本的事件为零串 */
} service_xiaozhi_event_t;

/**
 * @brief Initialize service (open AFE/Opus decoder, etc.)
 * @return ESP_OK on success
 */
esp_err_t service_xiaozhi_init(void);

/**
 * @brief Close service and release resources
 */
void service_xiaozhi_close(void);

/**
 * @brief Start service session
 * @return ESP_OK on success
 */
esp_err_t service_xiaozhi_start(void);

/**
 * @brief Stop service session asynchronously
 * @return ESP_OK on success
 */
esp_err_t service_xiaozhi_stop(void);

/**
 * @brief 重置绑定时重新生成设备 MAC 身份（服务器按 MAC 认设备，换新才能真正重新激活）
 */
void service_xiaozhi_reset_device_identity(void);

/**
 * @brief 请求激活（用于用户重置绑定后重新触发激活流程）
 * 
 * 清除当前激活状态，重新生成设备身份，并在 AI UI active 时立即触发激活流程。
 * 调用后需要确保 AI App 在前台，激活码将通过事件队列发送给 AI App 显示气泡。
 */
void service_xiaozhi_request_activation(void);

/**
 * @brief Start talking mode (manual listening)
 */
void service_xiaozhi_talk_start(void);

/**
 * @brief Stop talking mode
 */
void service_xiaozhi_talk_stop(void);

/**
 * @brief 请求退出对话回待机（MCP self.standby / 用户道别）
 *
 * 不立即关通道：等告别 TTS 播完（tts stop）再收尾，避免告别语被掉；
 * 3s 内无播报到来则兜底直接收尾。
 */
void service_xiaozhi_request_standby(void);

/**
 * @brief Process xiaozhi state machine (called from task_ai every cycle)
 */
void service_xiaozhi_process(void);

/**
 * @brief Submit event to application layer
 * @param type event type
 * @param text optional text
 * @return ESP_OK on success
 */
esp_err_t service_xiaozhi_post_event(service_xiaozhi_event_type_t type, const char *text);

/**
 * @brief Set WebSocket callback (called by service_ws)
 */
void service_xiaozhi_set_ws_callbacks(void *cbs);

/**
 * @brief Get current state
 * @return current state
 */
service_xiaozhi_state_t service_xiaozhi_get_state(void);

/**
 * @brief Check if activated (has token in NVS)
 * @return true if activated
 */
bool service_xiaozhi_is_activated(void);

/**
 * @brief 设置全局唤醒开关（任意界面唤醒后台对话），由 AI App 开关持久化后同步
 */
void service_xiaozhi_set_wake_anywhere(bool enable);

/** @brief 查询全局唤醒开关 */
bool service_xiaozhi_get_wake_anywhere(void);

/**
 * @brief 同步 AI App UI 是否在前台（由 app_ai_agent 生命周期调用）
 *
 * 全局唤醒关闭时，仅 AI 屏前台才响应唤醒。
 */
void service_xiaozhi_set_ai_ui_active(bool active);

/**
 * @brief Check if using auto mode (no AEC enabled)
 * @return true if auto mode, false if realtime mode
 */
bool service_xiaozhi_is_auto_mode(void);

/**
 * @brief Non-blocking event poll
 * @param out output buffer for event
 * @return 1 if event retrieved, 0 if no event
 */
int service_xiaozhi_poll_event(service_xiaozhi_event_t *out);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_XIAOZHI_H */
