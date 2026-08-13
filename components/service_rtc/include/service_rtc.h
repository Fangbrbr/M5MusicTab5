#ifndef SERVICE_RTC_H
#define SERVICE_RTC_H

#include "esp_err.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t service_rtc_init(void);
esp_err_t service_rtc_get_time(struct tm *tm);
esp_err_t service_rtc_get_time_cached(struct tm *tm);
esp_err_t service_rtc_set_time(const struct tm *tm);
esp_err_t service_rtc_process(void);

/**
 * @brief 请求下一次 service_rtc_process 立即执行 RTC 同步
 *
 * 用于 App 切到前台等需要立即校准系统时钟的场景，
 * 触发一次 I2C 读取后仍由系统时钟供 UI/应用使用。
 */
void service_rtc_request_sync(void);

/**
 * @brief 将 RTC 时间同步到系统时间
 *
 * 读取 RTC 硬件时间并通过 settimeofday 写入系统时钟，
 * 后续 UI/应用可直接使用 time(NULL) 而无须频繁访问 RTC I2C。
 *
 * @return ESP_OK 成功
 */
esp_err_t service_rtc_sync_to_system(void);

/**
 * @brief 设置相对闹钟，在 seconds 秒后触发。
 *
 * @param seconds 从现在起经过的秒数
 * @return ESP_OK 成功
 */
esp_err_t service_rtc_set_alarm_relative(uint32_t seconds);

/**
 * @brief 禁用并清除 RTC 闹钟标志。
 *
 * @return ESP_OK 成功
 */
esp_err_t service_rtc_clear_alarm(void);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_RTC_H */
