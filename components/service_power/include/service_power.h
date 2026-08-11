/**
 * @file service_power.h
 * @brief 电源管理服务
 *
 * 为 M5Stack Tab5 提供电池监测（INA226）、充电/5V 电源控制、外设电源控制、
 * 软件关机与待机唤醒能力。接口设计参考 M5Stack Tab5 UserDemo 的 HAL。
 */

#ifndef SERVICE_POWER_H
#define SERVICE_POWER_H

#include "esp_err.h"
#include "stdbool.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 电源管理事件类型
 */
typedef enum {
    SERVICE_POWER_EVT_NONE = 0,
    SERVICE_POWER_EVT_CHARGING_START,    /*!< 开始充电 */
    SERVICE_POWER_EVT_CHARGING_STOP,     /*!< 停止充电 */
    SERVICE_POWER_EVT_BATTERY_LOW,       /*!< 电池低电量 */
    SERVICE_POWER_EVT_HEADPHONE_INSERT,  /*!< 耳机插入 */
    SERVICE_POWER_EVT_HEADPHONE_REMOVE,  /*!< 耳机拔出 */
} service_power_event_t;

/**
 * @brief 电源事件回调
 */
typedef void (*service_power_event_cb_t)(service_power_event_t evt, void *user_data);

/**
 * @brief 电池信息
 */
typedef struct {
    float bus_voltage_v;    /*!< 总线电压，单位 V */
    float shunt_voltage_v;  /*!< 分流电阻电压，单位 V */
    float current_a;        /*!< 电流，充电为正，放电为负 */
    float power_w;          /*!< 功率，单位 W */
    bool is_charging;       /*!< 是否正在充电 */
} service_power_battery_info_t;

/**
 * @brief 初始化板级电源管理与前置板级服务
 *
 * 包括 I2C 总线、全局 I2C 锁、NVS、IO 扩展器、INA226 及 ESP32-C6 电源使能。
 *
 * @return ESP_OK 成功
 */
esp_err_t service_power_init(void);

/**
 * @brief 电源管理轮询处理
 *
 * 由 task_app 周期性调用，更新电池信息与 IO 扩展器输入状态。
 */
void service_power_process(void);

/**
 * @brief 注册电源事件回调
 *
 * @param cb 回调函数
 * @param user_data 用户数据
 * @return ESP_OK 成功
 */
esp_err_t service_power_register_event_callback(service_power_event_cb_t cb, void *user_data);

/**
 * @brief 更新电池信息（内部定时器也会自动调用）
 *
 * @return ESP_OK 成功
 */
esp_err_t service_power_update_battery_info(void);

/**
 * @brief 获取最近一次更新的电池信息
 *
 * @param info 输出参数
 * @return ESP_OK 成功
 */
esp_err_t service_power_get_battery_info(service_power_battery_info_t *info);

/**
 * @brief 设置充电使能
 */
esp_err_t service_power_set_charge_enable(bool enable);

/**
 * @brief 获取当前充电使能状态
 */
bool service_power_get_charge_enable(void);

/**
 * @brief 设置快充使能
 */
esp_err_t service_power_set_charge_qc_enable(bool enable);

/**
 * @brief 获取当前快充使能状态
 */
bool service_power_get_charge_qc_enable(void);

/**
 * @brief 设置 USB-A 5V 输出使能
 */
esp_err_t service_power_set_usb_5v_enable(bool enable);

/**
 * @brief 获取 USB-A 5V 输出使能状态
 */
bool service_power_get_usb_5v_enable(void);

/**
 * @brief 设置外部 5V 端口使能
 */
esp_err_t service_power_set_ext_5v_enable(bool enable);

/**
 * @brief 获取外部 5V 端口使能状态
 */
bool service_power_get_ext_5v_enable(void);

/**
 * @brief 设置 Wi-Fi 电源使能
 */
esp_err_t service_power_set_wifi_power_enable(bool enable);

/**
 * @brief 获取 Wi-Fi 电源使能状态
 */
bool service_power_get_wifi_power_enable(void);

/**
 * @brief 设置外接天线使能
 */
esp_err_t service_power_set_ext_antenna_enable(bool enable);

/**
 * @brief 获取外接天线使能状态
 */
bool service_power_get_ext_antenna_enable(void);

/**
 * @brief 耳机是否插入
 */
bool service_power_is_headphone_connected(void);

/**
 * @brief 关闭背光并等待触摸唤醒，唤醒后恢复背光
 *
 * 此函数会阻塞调用任务，直到检测到屏幕触摸。
 */
void service_power_standby_touch_wakeup(void);

/**
 * @brief 设置 RTC 闹钟并在 seconds 秒后通过 PMS150G 唤醒
 *
 * @param seconds 从现在起经过的秒数
 */
void service_power_standby_rtc_wakeup(uint32_t seconds);

/**
 * @brief 软件关机
 *
 * 关闭背光并向 PMS150G 发送关机脉冲。此函数不返回。
 */
void service_power_power_off(void) __attribute__((noreturn));

/**
 * @brief 初始化自动熄屏/自动休眠状态机
 */
void service_power_idle_init(void);

/**
 * @brief 使能或禁止自动熄屏/自动休眠计时
 *
 * 启动阶段先禁止，系统完全就绪后再打开，避免 boot 期间误熄屏。
 *
 * @param enable true 启用后台计时
 */
void service_power_idle_set_enabled(bool enable);

/**
 * @brief 重置空闲计时器（用户活动时调用）
 */
void service_power_idle_reset(void);

/**
 * @brief 后台轮询自动熄屏/自动休眠状态机
 *
 * 由 task_app 周期性调用，驱动熄屏与休眠计时。
 */
void service_power_idle_process(void);

/**
 * @brief 当前是否处于熄屏状态
 */
bool service_power_is_screen_off(void);

/**
 * @brief 点亮屏幕并恢复亮度
 */
void service_power_wake_screen(void);

/**
 * @brief 设置自动熄屏时间选项索引
 *
 * 0:15s, 1:30s, 2:1m, 3:3m, 4:5m, 5:10m
 */
void service_power_idle_set_timeout_index(uint8_t index);

/**
 * @brief 获取自动熄屏时间选项索引
 */
uint8_t service_power_idle_get_timeout_index(void);

/**
 * @brief 设置自动休眠开关
 */
void service_power_idle_set_auto_sleep_enabled(bool enable);

/**
 * @brief 获取自动休眠开关
 */
bool service_power_idle_get_auto_sleep_enabled(void);

/**
 * @brief 立即进入自动休眠（切断主电源，不返回）
 */
void service_power_enter_auto_sleep(void) __attribute__((noreturn));

/**
 * @brief 设置保持屏幕常亮（对话进行中调用）
 *
 * @param hold true 保持常亮，false 恢复自动熄屏
 */
void service_power_hold_screen_on(bool hold);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_POWER_H */
