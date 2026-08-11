/**
 * @file service_spiffs.c
 * @brief SPIFFS 资源分区挂载服务实现
 */

#include "service_spiffs.h"

#include <esp_log.h>
#include <esp_spiffs.h>

#define SERVICE_SPIFFS_TAG     "service_spiffs"

/* 与 partitions.csv 中的 `storage` 分区对应。
 * 挂载点改为 /sys_int，由 engine_gui 的自定义 VFS 将 /sys/src 透明重定向到
 * SD 卡（优先）或 SPIFFS，EEZ Studio 工程里的 fileSystemPath 保持 /sys/src 不变。 */
#define SERVICE_SPIFFS_LABEL   "storage"
#define SERVICE_SPIFFS_BASE    "/sys_int"
#define SERVICE_SPIFFS_MAX_FILES 10

esp_err_t service_spiffs_init(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = SERVICE_SPIFFS_BASE,
        .partition_label = SERVICE_SPIFFS_LABEL,
        .max_files = SERVICE_SPIFFS_MAX_FILES,
        .format_if_mount_failed = false,
    };

    ESP_LOGI(SERVICE_SPIFFS_TAG, "mounting SPIFFS partition '%s' at %s",
             SERVICE_SPIFFS_LABEL, SERVICE_SPIFFS_BASE);

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGW(SERVICE_SPIFFS_TAG, "mount failed (%s), formatting partition",
                 esp_err_to_name(ret));

        ret = esp_spiffs_format(SERVICE_SPIFFS_LABEL);
        if (ret != ESP_OK) {
            ESP_LOGE(SERVICE_SPIFFS_TAG, "format failed: %s", esp_err_to_name(ret));
            return ret;
        }

        ret = esp_vfs_spiffs_register(&conf);
        if (ret != ESP_OK) {
            ESP_LOGE(SERVICE_SPIFFS_TAG, "re-mount after format failed: %s",
                     esp_err_to_name(ret));
            return ret;
        }
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(SERVICE_SPIFFS_LABEL, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(SERVICE_SPIFFS_TAG, "mounted: total=%d KB, used=%d KB, free=%d KB",
                 (int)(total / 1024), (int)(used / 1024), (int)((total - used) / 1024));
    }

    return ESP_OK;
}

esp_err_t service_spiffs_info(size_t *total_bytes, size_t *used_bytes)
{
    return esp_spiffs_info(SERVICE_SPIFFS_LABEL, total_bytes, used_bytes);
}
