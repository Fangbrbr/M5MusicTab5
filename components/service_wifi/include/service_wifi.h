/**
 * @file service_wifi.h
 * @brief Wi-Fi 联网与 SNTP 对时服务
 */

#ifndef SERVICE_WIFI_H
#define SERVICE_WIFI_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 开机 WiFi 启动策略：降噪 C6 日志、按 NVS 开关决定使能，
 * 首次启动进配网 AP、否则 STA 连接
 * @return ESP_OK 成功或无需启动
 */
esp_err_t service_wifi_boot(void);

/**
 * @brief 初始化 Wi-Fi STA、事件循环与 SNTP
 * @return ESP_OK 成功
 */
esp_err_t service_wifi_init(void);

/**
 * @brief 使用指定 SSID/密码连接 Wi-Fi
 */
esp_err_t service_wifi_connect(const char *ssid, const char *password);

/**
 * @brief 断开当前 Wi-Fi 连接（未初始化时安全返回）
 */
esp_err_t service_wifi_disconnect(void);

/**
 * @brief Wi-Fi 协议栈是否已初始化
 */
bool service_wifi_is_initialized(void);

/**
 * @brief 重置重连退避并立即发起 STA 连接（开关重新打开时用）
 */
void service_wifi_reconnect_now(void);

/**
 * @brief 是否已获取 IP
 */
bool service_wifi_is_connected(void);

/**
 * @brief 请求立即对时（异步，实际在 service_wifi_process 中执行）
 */
esp_err_t service_wifi_sync_rtc(void);

/**
 * @brief 周期性处理：重连、周期对时
 *
 * 由 task_app 每 10 ms 调用一次。
 */
void service_wifi_process(void);

/**
 * @brief 获取上次成功对时的毫秒级 Unix 时间戳
 * @return 0 表示尚未成功对时
 */
int64_t service_wifi_get_last_sync_date_ms(void);

/**
 * @brief 从 SD 卡加载 Wi-Fi 配置（预留接口）
 */
esp_err_t service_wifi_load_config_from_sd(void);

/**
 * @brief 启动 SoftAP 手动配网
 *
 * 启动 AP "HammySetup"，运行 HTTP 配置服务器与 DNS 服务器。
 * 用户可访问 http://hammy.config/ 或 http://192.168.4.1/ 进行配置。
 * 若 Wi-Fi 尚未初始化，会先调用 service_wifi_init()。
 */
esp_err_t service_wifi_start_ap(void);

/**
 * @brief 停止 SoftAP 并切回 STA 模式
 */
esp_err_t service_wifi_stop_ap(void);

/**
 * @brief 启动 HTTP 配置服务器
 *
 * 服务器绑定在 80 端口、INADDR_ANY，AP 和 STA 模式下均可访问。
 */
esp_err_t service_wifi_http_server_start(void);

/**
 * @brief 检查 WiFi 是否可用（NVS 标记为 enabled 且已物理上电）
 *
 * App 层使用此接口替代直接调用 is_connected()，因为：
 * - NVS flag = false → 物理断电，永远返回 false
 * - NVS flag = true → 正常初始化，可能处于 AP/STA/未连接状态
 *
 * @return true=WiFi 可用，false=不可用
 */
bool service_wifi_is_available(void);

/**
 * @brief 用户在设置中关闭/打开 WiFi 总开关（运行时软禁用，不物理断电）
 *
 * 关闭后抑制重连风暴且 service_wifi_process 不再联网；物理断电仅在开机
 * 依据 NVS 标志生效（运行时断电会使 esp_hosted/lwip 崩溃）。
 */
void service_wifi_set_user_disabled(bool disabled);

/** @brief 查询是否被用户软禁用 */
bool service_wifi_get_user_disabled(void);

/**
 * @brief STA 连接状态（onboard / 设置页等 UI 轮询用）
 */
typedef enum {
    SERVICE_WIFI_STA_IDLE = 0,         /**< 未启动或 AP 模式 */
    SERVICE_WIFI_STA_CONNECTING,       /**< 正在连接 / 重连退避等待中 */
    SERVICE_WIFI_STA_CONNECTED,        /**< 已关联 AP，等待 IP */
    SERVICE_WIFI_STA_GOT_IP,           /**< 已获取 IP，联网可用 */
    SERVICE_WIFI_STA_FAILED,           /**< 关联失败，保存失败原因码 */
} service_wifi_sta_state_t;

/**
 * @brief 获取当前 STA 连接状态
 * @param[out] reason  若不为 NULL，失败时保存 WiFi 断开 reason 码（wifi_err_reason_t）
 * @return  当前状态
 */
service_wifi_sta_state_t service_wifi_get_sta_state(int *reason);

/**
 * @brief 获取 STA 当前 IP 字符串（如 "192.168.1.100"）
 *
 * @param[out] buf 输出缓冲
 * @param[in]  len 缓冲长度（至少 16）
 * @return ESP_OK 成功；ESP_ERR_INVALID_STATE 未连接/未初始化
 */
esp_err_t service_wifi_get_sta_ip_str(char *buf, size_t len);

/**
 * @brief STA 状态变化回调（单次注册，NULL 注销）
 *
 * 回调在 Wi-Fi 事件上下文（高优先级），应仅投递轻量操作，不得阻塞。
 */
typedef void (*service_wifi_sta_cb_t)(service_wifi_sta_state_t state, int reason);
void service_wifi_set_sta_callback(service_wifi_sta_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_WIFI_H */
