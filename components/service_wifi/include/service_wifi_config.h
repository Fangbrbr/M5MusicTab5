/**
 * @file service_wifi_config.h
 * @brief Wi-Fi 服务配置头文件
 *
 * STA 凭证仅存 NVS（经设置页/onboarding 配网写入），禁止内置默认凭证。
 */

#ifndef SERVICE_WIFI_CONFIG_H
#define SERVICE_WIFI_CONFIG_H

#define SERVICE_WIFI_SNTP_SERVER        "pool.ntp.org"
#define SERVICE_WIFI_SNTP_INTERVAL_MS   (30 * 60 * 1000)  /* 30 min */
#define SERVICE_WIFI_SNTP_TIMEOUT_MS    10000            /* 10 s */
#define SERVICE_WIFI_RECONNECT_MIN_MS   1000
#define SERVICE_WIFI_RECONNECT_MAX_MS   30000

#endif /* SERVICE_WIFI_CONFIG_H */
