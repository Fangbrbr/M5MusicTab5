/**
 * @file service_usb_device.h
 * @brief USB Device 服务
 *
 * 负责管理 USB-C Device 端口，当前实现两个功能：
 * 1. USB MIDI 设备：订阅 engine_midi 总线的非 SysEx 通道消息，转发到 USB Host（如 PC）。
 * 2. USB Mass Storage 设备：将 SD 卡暴露给 USB Host，无需取卡即可读写文件。
 */

#ifndef SERVICE_USB_DEVICE_H
#define SERVICE_USB_DEVICE_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 USB Device 服务
 *
 * 安装 TinyUSB 协议栈，初始化 MIDI 输出监听与 SD 卡 MSC 存储。
 * 必须在 service_sd_init() 之后调用。
 *
 * @return ESP_OK 成功；其他表示初始化失败，失败不会阻塞系统启动。
 */
esp_err_t service_usb_device_init(void);

/**
 * @brief 反初始化 USB Device 服务
 */
void service_usb_device_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_USB_DEVICE_H */
