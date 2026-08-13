/**
 * @file service_input.c
 * @brief 输入服务
 *
 * 统一初始化 UART MIDI、USB Host、USB Device、BLE MIDI 等输入 transport。
 * 具体 MIDI 事件分发由 engine_midi 总线负责。
 */

#include "service_input.h"
#include "service_input_uart.h"
#include "service_usb_host.h"
#include "service_usb_device.h"
#include "service_ble_midi.h"
#include "esp_log.h"
#include <stdbool.h>

static const char *TAG = "service_input";

static bool s_ble_probe_done = false;

esp_err_t service_input_init(void)
{
    ESP_LOGI(TAG, "service_input_init");

    /* UART MIDI：实际硬件 transport */
    esp_err_t ret = service_input_uart_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "UART MIDI init failed or not configured: %s", esp_err_to_name(ret));
    }

    /* USB Host MIDI/HID */
    ret = service_usb_host_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "USB Host init failed: %s", esp_err_to_name(ret));
    }

    /* Tab5 仅有一个 USB PHY：Host 未启动时尝试 Device fallback */
    if (!service_usb_host_is_started()) {
        ret = service_usb_device_init();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "USB Device init failed: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGI(TAG, "USB Host started; USB-C Device disabled due to single PHY");
    }

    /* BLE MIDI：C6 固件探测可能因 RPC 超时报 5s，不在启动主线阻塞，
     * 延后到 service_input_late_probe_ble() 由 task_app 后台调用 */
    ESP_LOGI(TAG, "BLE MIDI probe deferred to background (not blocking boot)");

    return ESP_OK;
}

void service_input_late_probe_ble(void)
{
    if (s_ble_probe_done) {
        return;
    }
    s_ble_probe_done = true;

    ESP_LOGI(TAG, "BLE MIDI probe: starting (background)");
    if (service_ble_midi_probe_c6_support()) {
        esp_err_t ret = service_ble_midi_init();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "BLE MIDI init failed: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "BLE MIDI ready");
        }
    } else {
        ESP_LOGI(TAG, "C6 firmware outdated/unsupported, BLE MIDI skipped");
    }
}

void service_input_process(void)
{
    service_input_uart_process();
}
