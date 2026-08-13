/**
 * @file service_wifi.c
 * @brief Wi-Fi STA + SNTP 对时服务
 */

#include "service_wifi.h"
#include "sdkconfig.h"
#include "service_wifi_config.h"
#include "service_nvs.h"
#include "service_rtc.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_sntp.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <time.h>
#include <sys/time.h>

static const char *TAG = "service_wifi";

static bool s_initialized = false;
static bool s_connected = false;
static bool s_ap_mode = false;
/* 是否已向驱动写入 STA 凭证（无凭证开机时跳过 set_config，
 * 配网后由 reconnect_now 从 NVS 装载） */
static bool s_sta_configured = false;
static bool s_sync_pending = false;
static bool s_sntp_started = false;
/* 用户在设置中关闭 WiFi 总开关：抑制重连风暴，重启前不再联网 */
static bool s_user_disabled = false;
static int64_t s_last_sync_ms = 0;
static int64_t s_sync_wait_start_ms = 0;
static int64_t s_next_reconnect_ms = 0;
static uint32_t s_reconnect_delay_ms = SERVICE_WIFI_RECONNECT_MIN_MS;
static esp_netif_t *s_sta_netif = NULL;
static int s_last_disconnect_reason = 0;
static service_wifi_sta_state_t s_sta_state = SERVICE_WIFI_STA_IDLE;
static service_wifi_sta_cb_t s_sta_cb = NULL;

extern void service_wifi_ap_process(void);

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);
static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data);
static esp_err_t wifi_sta_start(void);
static void wifi_schedule_reconnect(void);
static esp_err_t wifi_do_sync_rtc(void);

static int64_t wifi_now_ms(void)
{
    return (int64_t)(esp_timer_get_time() / 1000ULL);
}

esp_err_t service_wifi_boot(void)
{
#if !CONFIG_BOARD_HAS_WIFI
    /* 板型无 WiFi 协处理器：跳过协议栈与配网 AP，网络功能整体走既有离线降级 */
    ESP_LOGW(TAG, "board has no WiFi, skip wifi boot");
    return ESP_ERR_NOT_SUPPORTED;
#else
    /* 旧版 C6 固件会打印大量版本不匹配、RPC 超时等 warning/info，统一降噪 */
    esp_log_level_set("transport", ESP_LOG_ERROR);
    esp_log_level_set("rpc_core", ESP_LOG_ERROR);
    esp_log_level_set("vhci_drv", ESP_LOG_ERROR);
    esp_log_level_set("H_SDIO_DRV", ESP_LOG_ERROR);

    /* 按 NVS 保存的开关决定是否初始化 Wi-Fi 协议栈。关闭时仅跳过协议栈
     * 初始化（C6 电源由 service_power_init 开机统一供给，不在此控制）。 */
    if (!service_nvs_get_feature_flag(SERVICE_NVS_FLAG_WIFI_ENABLED)) {
        ESP_LOGI(TAG, "WiFi disabled by user setting, skipping initialization");
        return ESP_OK;
    }

    esp_err_t ret;
    if (!service_nvs_is_initialized()) {
        ESP_LOGI(TAG, "first boot, start config AP");
        ret = service_wifi_start_ap();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "config AP start failed: %d", ret);
        }
    } else {
        ret = service_wifi_init();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "service_wifi_init failed: %d", ret);
        }
    }
    return ret;
#endif /* CONFIG_BOARD_HAS_WIFI */
}

esp_err_t service_wifi_init(void)
{
#if !CONFIG_BOARD_HAS_WIFI
    /* 无 WiFi 硬件：init 是 start_ap/onboard 等路径的共同入口，统一在此拦截 */
    ESP_LOGW(TAG, "board has no WiFi, wifi init not supported");
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (s_initialized) {
        return ESP_OK;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NO_FREE_PAGES &&
        ret != ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGE(TAG, "nvs_flash_init failed: %d", ret);
        return ret;
    }

    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_init failed: %d", ret);
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_event_loop_create_default failed: %d", ret);
        return ret;
    }

    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (s_sta_netif == NULL) {
        ESP_LOGE(TAG, "create_default_wifi_sta failed");
        return ESP_FAIL;
    }

    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              &wifi_event_handler, NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "register wifi event handler failed: %d", ret);
        return ret;
    }

    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              &ip_event_handler, NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "register ip event handler failed: %d", ret);
        return ret;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %d", ret);
        return ret;
    }

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode failed: %d", ret);
        return ret;
    }

    ret = wifi_sta_start();
    if (ret != ESP_OK) {
        return ret;
    }

    s_initialized = true;

    /* HTTP 配置服务器在 STA/AP 模式下都保持运行，方便随时通过 IP 访问修改配置 */
    service_wifi_http_server_start();

    ESP_LOGI(TAG, "wifi init ok");
    return ESP_OK;
#endif /* CONFIG_BOARD_HAS_WIFI */
}

esp_err_t service_wifi_connect(const char *ssid, const char *password)
{
    if (ssid == NULL || password == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_config_t wifi_config = {0};
    snprintf((char *)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), "%s", ssid);
    snprintf((char *)wifi_config.sta.password, sizeof(wifi_config.sta.password), "%s", password);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "set_config failed: %d", ret);
        return ret;
    }
    s_sta_configured = true;

    return esp_wifi_connect();
}

esp_err_t service_wifi_disconnect(void)
{
    if (!s_initialized) {
        return ESP_OK;   /* 未初始化（如开机被开关跳过），无需断开 */
    }
    return esp_wifi_disconnect();
}

bool service_wifi_is_initialized(void)
{
    return s_initialized;
}

void service_wifi_reconnect_now(void)
{
    /* 开关重新打开时立即恢复连接：重置退避并主动发起 connect，
     * 避免等待已翻倍的退避定时器造成“打开后久久不连”。 */
    if (!s_initialized || s_user_disabled) {
        return;
    }

    /* 无凭证开机时跳过了 set_config：配网写入 NVS 后首次连接前需装载进驱动 */
    if (!s_sta_configured) {
        char ssid[64] = {0};
        char password[64] = {0};
        service_nvs_get_wifi_ssid(ssid, sizeof(ssid));
        service_nvs_get_wifi_password(password, sizeof(password));
        if (ssid[0] == '\0') {
            return;
        }
        wifi_config_t wifi_config = {0};
        snprintf((char *)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), "%s", ssid);
        snprintf((char *)wifi_config.sta.password, sizeof(wifi_config.sta.password), "%s", password);
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "set_config from nvs failed: %d", ret);
            return;
        }
        s_sta_configured = true;
        ESP_LOGI(TAG, "credentials loaded from NVS");
    }

    s_reconnect_delay_ms = SERVICE_WIFI_RECONNECT_MIN_MS;
    s_next_reconnect_ms = 0;
    if (!s_ap_mode && !s_connected) {
        esp_err_t ret = esp_wifi_connect();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "reconnect_now failed: %d", ret);
            wifi_schedule_reconnect();
        }
    }
}

bool service_wifi_is_connected(void)
{
    return s_connected;
}

esp_err_t service_wifi_sync_rtc(void)
{
    s_sync_pending = true;
    return ESP_OK;
}

int64_t service_wifi_get_last_sync_date_ms(void)
{
    return s_last_sync_ms;
}

esp_err_t service_wifi_load_config_from_sd(void)
{
    /* 预留接口：后续从 /sdcard/system/config.ini 读取 SSID/密码/SNTP 服务器 */
    return ESP_ERR_NOT_SUPPORTED;
}

void service_wifi_mark_ap_mode(bool ap_mode)
{
    s_ap_mode = ap_mode;
    if (ap_mode) {
        s_sta_state = SERVICE_WIFI_STA_IDLE;
    } else if (s_initialized && !s_user_disabled) {
        s_sta_state = SERVICE_WIFI_STA_CONNECTING;
    }
}

void service_wifi_process(void)
{
    if (s_user_disabled) {
        return;
    }
    if (s_ap_mode) {
        service_wifi_ap_process();
        return;
    }

    int64_t now = wifi_now_ms();

    if (!s_connected) {
        if (s_next_reconnect_ms != 0 && now >= s_next_reconnect_ms) {
            s_next_reconnect_ms = 0;
            esp_err_t ret = esp_wifi_connect();
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "reconnect attempt failed: %d", ret);
                wifi_schedule_reconnect();
            }
        }
        return;
    }

    /* 连接成功后首次同步已由 ip_event_handler 置位 s_sync_pending；
     * 成功后开始 30 分钟间隔计时，间隔到达再次置位触发对时。 */
    if (!s_sync_pending) {
        if (s_last_sync_ms != 0 && (now - s_last_sync_ms) < SERVICE_WIFI_SNTP_INTERVAL_MS) {
            return;
        }
        s_sync_pending = true;
    }

    if (wifi_do_sync_rtc() == ESP_OK) {
        s_sync_pending = false;
    }
}

static esp_err_t wifi_sta_start(void)
{
    char ssid[64] = {0};
    char password[64] = {0};
    service_nvs_get_wifi_ssid(ssid, sizeof(ssid));
    service_nvs_get_wifi_password(password, sizeof(password));

    /* Contract: 无已配网凭证时不自动连接（禁止内置默认凭证），但驱动照常启动；
     * 配网写入 NVS 后由 service_wifi_reconnect_now 装载凭证并发起连接 */
    if (ssid[0] != '\0') {
        wifi_config_t wifi_config = {0};
        snprintf((char *)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), "%s", ssid);
        snprintf((char *)wifi_config.sta.password, sizeof(wifi_config.sta.password), "%s", password);
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

        esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "set_config failed: %d", ret);
            return ret;
        }
        s_sta_configured = true;
    } else {
        ESP_LOGW(TAG, "no wifi credentials in NVS, skip auto-connect");
    }

    esp_err_t ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %d", ret);
        return ret;
    }

    return ESP_OK;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;

    if (event_id == WIFI_EVENT_STA_START) {
        /* 无凭证时空转：等待配网后 reconnect_now 触发连接 */
        if (!s_ap_mode && s_sta_configured) {
            ESP_LOGI(TAG, "sta start, connecting...");
            s_sta_state = SERVICE_WIFI_STA_CONNECTING;
            s_last_disconnect_reason = 0;
            if (s_sta_cb) s_sta_cb(s_sta_state, 0);
            esp_wifi_connect();
        }
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *dis = (wifi_event_sta_disconnected_t *)event_data;
        s_last_disconnect_reason = (dis != NULL) ? (int)dis->reason : 0;
        ESP_LOGW(TAG, "sta disconnected, reason=%d", s_last_disconnect_reason);
        s_connected = false;
        s_sta_state = SERVICE_WIFI_STA_FAILED;
        if (s_sta_cb) s_sta_cb(s_sta_state, s_last_disconnect_reason);
        wifi_schedule_reconnect();
    } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "sta connected");
        s_sta_state = SERVICE_WIFI_STA_CONNECTED;
        s_last_disconnect_reason = 0;
        if (s_sta_cb) s_sta_cb(s_sta_state, 0);
    }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;

    if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&event->ip_info.ip));
        s_connected = true;
        s_reconnect_delay_ms = SERVICE_WIFI_RECONNECT_MIN_MS;
        s_next_reconnect_ms = 0;
        s_sync_pending = true;
        s_sta_state = SERVICE_WIFI_STA_GOT_IP;
        s_last_disconnect_reason = 0;
        if (s_sta_cb) s_sta_cb(s_sta_state, 0);
    }
}

static void wifi_schedule_reconnect(void)
{
    s_next_reconnect_ms = wifi_now_ms() + (int64_t)s_reconnect_delay_ms;
    s_reconnect_delay_ms *= 2;
    if (s_reconnect_delay_ms > SERVICE_WIFI_RECONNECT_MAX_MS) {
        s_reconnect_delay_ms = SERVICE_WIFI_RECONNECT_MAX_MS;
    }
    ESP_LOGI(TAG, "reconnect in %u ms", (unsigned)s_reconnect_delay_ms);
}

static esp_err_t wifi_do_sync_rtc(void)
{
    if (!s_connected) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_sntp_started) {
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, SERVICE_WIFI_SNTP_SERVER);
        esp_sntp_init();
        s_sntp_started = true;
    }

    /* 非阻塞轮询：本函数在 task_app 循环中每周期调用一次，绝不 vTaskDelay。
     * 阻塞等待会把 task_app 整体停摆（最长 30s），导致开机后 launcher 无响应 */
    if (s_sync_wait_start_ms == 0) {
        sntp_restart();
        s_sync_wait_start_ms = wifi_now_ms();
    }

    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
        time_t now = time(NULL);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);

        /* 先设置系统时间，再同步到 RTC 硬件 */
        struct timeval tv = {.tv_sec = now, .tv_usec = 0};
        settimeofday(&tv, NULL);

        esp_err_t ret = service_rtc_set_time(&timeinfo);
        if (ret == ESP_OK) {
            s_last_sync_ms = (int64_t)((now + (8 * 3600)) * 1000LL);
            ESP_LOGI(TAG, "RTC synced: %04d-%02d-%02d %02d:%02d:%02d",
                     timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        } else {
            ESP_LOGW(TAG, "set rtc failed: %d", ret);
        }
        s_sync_wait_start_ms = 0;
        return ret;
    }

    /* 首次同步给更长窗口，避免刚联网时 NTP 往返延迟导致误报超时 */
    int64_t timeout_ms = (s_last_sync_ms == 0) ? 30000 : SERVICE_WIFI_SNTP_TIMEOUT_MS;
    if ((wifi_now_ms() - s_sync_wait_start_ms) >= timeout_ms) {
        s_sync_wait_start_ms = 0;
        ESP_LOGI(TAG, "sntp sync timeout, retry later");
    }

    /* 同步进行中或超时：保持 pending，下周期继续 */
    return ESP_ERR_TIMEOUT;
}

bool service_wifi_is_available(void)
{
    /* 检查 NVS 标记和初始化状态 */
    bool enabled = service_nvs_get_feature_flag(SERVICE_NVS_FLAG_WIFI_ENABLED);
    if (!enabled || !s_initialized) {
        return false;
    }
    
    /* 物理上电且正在连接中或已连接才算可用 */
    return s_connected;
}

void service_wifi_set_user_disabled(bool disabled)
{
    s_user_disabled = disabled;
    if (disabled) {
        /* 取消待执行的重连，避免关闭后仍尝试 esp_wifi_connect */
        s_next_reconnect_ms = 0;
    }
    ESP_LOGI(TAG, "user disabled: %d", (int)disabled);
}

bool service_wifi_get_user_disabled(void)
{
    return s_user_disabled;
}

service_wifi_sta_state_t service_wifi_get_sta_state(int *reason)
{
    if (reason != NULL) {
        *reason = s_last_disconnect_reason;
    }
    /* 连接中（没启动也没失败）仍有下一次重连计划 → 汇报 CONNECTING */
    if (!s_ap_mode && s_initialized && !s_user_disabled &&
        s_sta_state == SERVICE_WIFI_STA_FAILED &&
        s_next_reconnect_ms != 0) {
        return SERVICE_WIFI_STA_CONNECTING;
    }
    if (s_ap_mode) {
        return SERVICE_WIFI_STA_IDLE;
    }
    return s_sta_state;
}

void service_wifi_set_sta_callback(service_wifi_sta_cb_t cb)
{
    s_sta_cb = cb;
}
