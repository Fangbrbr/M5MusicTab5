/**
 * @file service_usb_host.h
 * @brief USB Host 服务
 *
 * 负责管理 USB-A Host 端口，当前主要用于接入 USB MIDI 键盘，
 * 将收到的 MIDI 字节流通过 engine_midi 总线分发。
 */

#ifndef SERVICE_USB_HOST_H
#define SERVICE_USB_HOST_H

#include "esp_err.h"
#include "stdbool.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 USB Host 服务
 *
 * 启动 BSP USB Host 库并注册 MIDI 类客户端，开始监听 USB-A 端口。
 *
 * @return ESP_OK 成功；其他表示初始化失败，失败不会阻塞系统启动。
 */
esp_err_t service_usb_host_init(void);

/**
 * @brief 反初始化 USB Host 服务
 */
void service_usb_host_deinit(void);

/**
 * @brief 处理 USB Host 事件
 *
 * 由 task_comm 周期调用，非阻塞处理 USB Host 客户端事件。
 */
void service_usb_host_process(void);

/**
 * @brief 查询 USB Host 服务是否已启动
 * @return true 已启动
 */
bool service_usb_host_is_started(void);

/**
 * @brief 查询当前是否有 USB MIDI 设备连接并就绪
 * @return true 已连接；false 未连接
 */
bool service_usb_host_midi_connected(void);

/**
 * @brief 获取当前 USB MIDI 设备厂商/产品描述字符串
 * @return 字符串指针，未连接时返回空字符串
 */
const char *service_usb_host_midi_vendor(void);

/**
 * @brief 获取当前 USB MIDI 设备的 VID/PID
 * @param[out] vid Vendor ID，可为 NULL
 * @param[out] pid Product ID，可为 NULL
 */
void service_usb_host_get_vid_pid(uint16_t *vid, uint16_t *pid);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_USB_HOST_H */
