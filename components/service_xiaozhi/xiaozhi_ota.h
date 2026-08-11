/**
 * @file xiaozhi_ota.h
 * @brief 小智 OTA 设备激活
 *
 * 通过 service_http_client 发起 HTTPS 请求，使用多个自定义请求头
 * （Activation-Version/Device-Id/Client-Id 等）。
 */

#ifndef XIAOZHI_OTA_H
#define XIAOZHI_OTA_H

#include "esp_err.h"
#include "stdbool.h"
#include "stdint.h"

#include "service_nvs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 激活码最大长度（含结尾 '\0'，服务器下发 6 位数字） */
#define XIAOZHI_OTA_CODE_MAX_LEN 16

/** @brief 激活提示语最大长度（含结尾 '\0'） */
#define XIAOZHI_OTA_MESSAGE_MAX_LEN 128

/** @brief OTA 检查结果 */
typedef struct {
    bool has_websocket;                                  /*!< 响应含有效 websocket 配置 */
    char ws_url[SERVICE_NVS_XZ_WS_URL_MAX_LEN];          /*!< WebSocket 地址 */
    char ws_token[SERVICE_NVS_XZ_WS_TOKEN_MAX_LEN];      /*!< WebSocket 鉴权 token */
    bool has_activation;                                 /*!< 响应含 activation 段（待用户绑定） */
    char activation_code[XIAOZHI_OTA_CODE_MAX_LEN];      /*!< 6 位激活码 */
    char activation_message[XIAOZHI_OTA_MESSAGE_MAX_LEN];/*!< 激活提示语 */
    uint32_t activation_timeout_ms;                      /*!< 激活码有效期 */
} xiaozhi_ota_result_t;

/**
 * @brief 获取设备 Device-Id（STA MAC，小写冒号格式）
 *
 * @param[out] out 输出缓冲（至少 18 字节）
 * @param[in]  len 缓冲长度
 */
void xiaozhi_ota_get_device_id(char *out, size_t len);

/**
 * @brief 重置绑定时重新生成设备 MAC 身份并失效 device_id 缓存
 */
void xiaozhi_ota_regenerate_device_id(void);

/**
 * @brief 确保 Client-Id 存在：NVS xz_uuid 为空则生成 UUID v4 并落盘
 *
 * @param[out] out 输出缓冲（至少 SERVICE_NVS_XZ_UUID_MAX_LEN 字节）
 * @param[in]  len 缓冲长度
 * @return ESP_OK 成功
 */
esp_err_t xiaozhi_ota_ensure_client_id(char *out, size_t len);

/**
 * @brief POST OTA 地址上报设备信息并解析响应（激活检查）
 *
 * 阻塞式 HTTP（最长 SERVICE_XIAOZHI_HTTP_TIMEOUT_MS），仅在 xz_task 调用。
 *
 * @param[out] result 解析结果
 * @return ESP_OK 成功（HTTP 200 且 JSON 解析成功）
 */
esp_err_t xiaozhi_ota_check(xiaozhi_ota_result_t *result);

/**
 * @brief POST {ota_url}/activate 轮询激活状态
 *
 * v1 设备（无序列号）请求体为空 JSON "{}"；HTTP 202 表示等待用户绑定。
 *
 * @return ESP_OK 激活成功（HTTP 200），ESP_ERR_TIMEOUT 等待绑定中（HTTP 202），其他失败
 */
esp_err_t xiaozhi_ota_activate(void);

#ifdef __cplusplus
}
#endif

#endif /* XIAOZHI_OTA_H */
