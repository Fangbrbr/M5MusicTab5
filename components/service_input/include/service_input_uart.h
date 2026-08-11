/**
 * @file service_input_uart.h
 * @brief UART MIDI 输入/输出 transport
 *
 * 将 UART 收到的 MIDI 字节流喂给 engine_midi，同时订阅 MIDI 事件总线输出到外接
 * MIDI 设备，实现双工通信。
 */

#ifndef SERVICE_INPUT_UART_H
#define SERVICE_INPUT_UART_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 默认 UART MIDI 引脚与端口配置
 *
 * 注意：请根据实际硬件连接确认这些引脚是否可用。
 * M5Stack Tab5 已占用 GPIO：19/20(USB), 22(LCD_BL), 23(TOUCH_INT),
 * 26-30(I2S), 31/32(I2C), 39-44(SD)。
 */
#ifndef CONFIG_SERVICE_INPUT_UART_NUM
#define CONFIG_SERVICE_INPUT_UART_NUM UART_NUM_1
#endif

#ifndef CONFIG_SERVICE_INPUT_UART_TX_PIN
#define CONFIG_SERVICE_INPUT_UART_TX_PIN GPIO_NUM_17
#endif

#ifndef CONFIG_SERVICE_INPUT_UART_RX_PIN
#define CONFIG_SERVICE_INPUT_UART_RX_PIN GPIO_NUM_18
#endif

#ifndef CONFIG_SERVICE_INPUT_UART_BAUDRATE
#define CONFIG_SERVICE_INPUT_UART_BAUDRATE 31250
#endif

/**
 * @brief 初始化 UART MIDI transport
 *
 * 配置 UART 为 MIDI 标准波特率 31250，安装驱动（使用事件队列）。
 * 同时订阅 engine_midi 事件总线用于输出。
 *
 * @return ESP_OK 成功
 */
esp_err_t service_input_uart_init(void);

/**
 * @brief 反初始化 UART MIDI transport
 */
void service_input_uart_deinit(void);

/**
 * @brief 处理 UART MIDI 输入事件
 *
 * 由 task_input 周期调用，从 UART 事件队列读取数据并喂给 engine_midi。
 */
void service_input_uart_process(void);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_INPUT_UART_H */
