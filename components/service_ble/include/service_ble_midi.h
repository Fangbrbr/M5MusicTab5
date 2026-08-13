/**
 * @file service_ble_midi.h
 * @brief BLE MIDI peripheral 服务
 */

#ifndef SERVICE_BLE_MIDI_H
#define SERVICE_BLE_MIDI_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 探测 ESP32-C6 协处理器固件是否支持 BLE
 *
 * 在 Wi-Fi 初始化后调用，通过 esp_hosted_get_coprocessor_app_desc 读取协处理器版本。
 * 版本号解析失败或主版本为 0 时视为旧固件，应跳过 BLE MIDI 初始化。
 *
 * @return true C6 固件支持 BLE
 * @return false C6 固件过旧或不可用
 */
bool service_ble_midi_probe_c6_support(void);

/**
 * @brief 初始化 BLE MIDI 外设（NimBLE + Hosted 控制器）
 */
esp_err_t service_ble_midi_init(void);

/**
 * @brief 反初始化 BLE MIDI
 */
void service_ble_midi_deinit(void);

/**
 * @brief 使能/禁用 BLE MIDI 广播与发送
 */
esp_err_t service_ble_midi_enable(bool enable);

/**
 * @brief 当前是否处于使能状态
 */
bool service_ble_midi_is_enabled(void);

/**
 * @brief 是否有主机连接
 */
bool service_ble_midi_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_BLE_MIDI_H */
