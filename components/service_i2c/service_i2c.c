#include "service_i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "service_i2c";

static SemaphoreHandle_t s_i2c_mutex = NULL;

esp_err_t service_i2c_init(void)
{
    if (s_i2c_mutex != NULL) {
        return ESP_OK;
    }

    s_i2c_mutex = xSemaphoreCreateRecursiveMutex();
    if (s_i2c_mutex == NULL) {
        ESP_LOGE(TAG, "i2c mutex create failed");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "i2c bus lock ready");
    return ESP_OK;
}

bool service_i2c_take(void)
{
    if (s_i2c_mutex == NULL) {
        return false;
    }
    return xSemaphoreTakeRecursive(s_i2c_mutex, portMAX_DELAY) == pdTRUE;
}

void service_i2c_give(void)
{
    if (s_i2c_mutex != NULL) {
        xSemaphoreGiveRecursive(s_i2c_mutex);
    }
}
