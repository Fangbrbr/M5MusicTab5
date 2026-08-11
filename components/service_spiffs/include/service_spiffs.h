/**
 * @file service_spiffs.h
 * @brief SPIFFS 资源分区挂载服务
 */

#ifndef SERVICE_SPIFFS_H
#define SERVICE_SPIFFS_H

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 挂载 SPIFFS 资源分区
 *
 * 挂载点为 `/sys_int`，由 engine_gui 的自定义 VFS 将 `/sys/src`
 * 透明重定向到 SD 卡（优先）或 SPIFFS。
 * 若挂载失败，会自动格式化后重试一次。
 *
 * @return ESP_OK 成功
 */
esp_err_t service_spiffs_init(void);

/**
 * @brief 获取 SPIFFS 分区使用信息
 */
esp_err_t service_spiffs_info(size_t *total_bytes, size_t *used_bytes);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_SPIFFS_H */
