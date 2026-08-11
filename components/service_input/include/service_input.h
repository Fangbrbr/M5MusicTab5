/**
 * @file service_input.h
 * @brief 输入服务
 *
 * 统一初始化所有输入 transport：
 * - UART MIDI
 * - USB Host MIDI/HID
 * - USB Device MIDI/MSC（USB Host 失败时 fallback）
 * - BLE MIDI（探测到 C6 支持时初始化）
 *
 * 具体事件处理由 task_input / task_comm / engine_gui 按各自职责完成。
 */

#ifndef SERVICE_INPUT_H
#define SERVICE_INPUT_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化所有输入 transport
 * @return ESP_OK 成功（个别 transport 失败仅记录日志，不阻塞启动）
 */
esp_err_t service_input_init(void);

/**
 * @brief 处理输入事件
 *
 * 由 task_input 周期调用，统一轮询 UART MIDI 等输入源。
 */
void service_input_process(void);

/**
 * @brief 后台探测 C6 BLE MIDI 支持并初始化（非阻塞启动）
 *
 * C6 RPC 探测可能超时约 5s，不在 service_input_init() 同步执行，
 * 由 task_app 在后台周期调用一次，避免阻塞开机进度。
 */
void service_input_late_probe_ble(void);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_INPUT_H */
