#include "service_rtc.h"
#include "bsp/m5stack_tab5.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "rx8130ce.hpp"
#include "service_i2c.h"

#include <stdlib.h>
#include <system_error>
#include <sys/time.h>

static const char *TAG = "service_rtc";

#define RX8130CE_ADDR       0x32
#define RX8130CE_I2C_TIMEOUT_MS 100
#define RX8130CE_I2C_FREQ_HZ    100000
#define RX8130CE_READ_RETRY     3
#define RX8130CE_READ_RETRY_MS  5

/* RTC 同步到系统时钟的常规间隔：1 分钟。
 * 过短会增加 I2C 总线负载，过长会放大系统时钟漂移导致的时间差。 */
#define RTC_SYNC_INTERVAL_MS    (60 * 1000)

static i2c_master_dev_handle_t s_dev_handle = NULL;
static espp::Rx8130ce<true> *s_rtc = NULL;
static SemaphoreHandle_t s_rtc_mutex = NULL;

static struct tm s_cached_tm;
static int64_t s_last_update_ms = 0;
static bool s_cache_valid = false;
static uint32_t s_rtc_err_cnt = 0;
static volatile bool s_sync_requested = false;

extern "C" esp_err_t service_rtc_init(void)
{
    if (s_rtc != NULL) {
        return ESP_OK;
    }

    /* 设置为中国标准时间 CST (UTC+8) */
    setenv("TZ", "CST-8", 1);
    tzset();

    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (bus == NULL) {
        ESP_LOGE(TAG, "bsp i2c handle is null");
        return ESP_ERR_INVALID_STATE;
    }

    s_rtc_mutex = xSemaphoreCreateRecursiveMutex();
    if (s_rtc_mutex == NULL) {
        ESP_LOGE(TAG, "rtc mutex create failed");
        return ESP_ERR_NO_MEM;
    }

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = RX8130CE_ADDR;
    dev_cfg.scl_speed_hz = RX8130CE_I2C_FREQ_HZ;

    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "add i2c device failed: %d", ret);
        return ret;
    }

    using Rtc = espp::Rx8130ce<true>;

    Rtc::Config config;
    config.device_address = RX8130CE_ADDR;
    config.auto_init = false;
    config.log_level = espp::Logger::Verbosity::WARN;
    config.write = [](uint8_t addr, const uint8_t *data, size_t len) -> bool {
        (void)addr;
        if (s_dev_handle == NULL) {
            return false;
        }
        return i2c_master_transmit(s_dev_handle, data, len,
                                   pdMS_TO_TICKS(RX8130CE_I2C_TIMEOUT_MS)) == ESP_OK;
    };
    config.read = [](uint8_t addr, uint8_t *data, size_t len) -> bool {
        (void)addr;
        if (s_dev_handle == NULL) {
            return false;
        }
        return i2c_master_receive(s_dev_handle, data, len,
                                  pdMS_TO_TICKS(RX8130CE_I2C_TIMEOUT_MS)) == ESP_OK;
    };

    s_rtc = new Rtc(config);
    if (s_rtc == NULL) {
        ESP_LOGE(TAG, "alloc rtc object failed");
        return ESP_ERR_NO_MEM;
    }

    s_rtc->set_write_then_read(
        [](uint8_t addr, const uint8_t *wdata, size_t wlen,
           uint8_t *rdata, size_t rlen) -> bool {
            (void)addr;
            if (s_dev_handle == NULL) {
                return false;
            }
            return i2c_master_transmit_receive(s_dev_handle, wdata, wlen, rdata, rlen,
                                               pdMS_TO_TICKS(RX8130CE_I2C_TIMEOUT_MS)) == ESP_OK;
        });

    std::error_code ec;
    if (!s_rtc->init(ec)) {
        ESP_LOGE(TAG, "rtc init failed: %s", ec.message().c_str());
        return ESP_FAIL;
    }

    struct tm tm_now;
    ret = service_rtc_get_time(&tm_now);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "RTC ready: %04d-%02d-%02d %02d:%02d:%02d",
                 tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                 tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
        /* 立即同步到系统时钟并预热缓存：UI/App 统一用 time(NULL)，若等到
         * 首次 service_rtc_process 才同步，开机窗口期状态栏与各 App 时间不一致 */
        s_cached_tm = tm_now;
        s_cache_valid = true;
        s_last_update_ms = (int64_t)(esp_timer_get_time() / 1000ULL);
        service_rtc_sync_to_system();
    }

    return ret;
}

static bool rtc_take(void)
{
    if (s_rtc_mutex == NULL) {
        return false;
    }
    return xSemaphoreTakeRecursive(s_rtc_mutex, portMAX_DELAY) == pdTRUE;
}

static void rtc_give(void)
{
    if (s_rtc_mutex != NULL) {
        xSemaphoreGiveRecursive(s_rtc_mutex);
    }
}

extern "C" esp_err_t service_rtc_get_time(struct tm *tm)
{
    if (tm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_rtc == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!rtc_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    service_i2c_take();
    std::error_code ec;
    std::tm rtc_tm;
    bool ok = false;

    for (int i = 0; i < RX8130CE_READ_RETRY; i++) {
        ec.clear();
        rtc_tm = s_rtc->get_time(ec);
        if (!ec) {
            ok = true;
            break;
        }
        if (i < RX8130CE_READ_RETRY - 1) {
            vTaskDelay(pdMS_TO_TICKS(RX8130CE_READ_RETRY_MS));
        }
    }

    service_i2c_give();
    rtc_give();

    if (!ok) {
        s_rtc_err_cnt++;
        if (s_rtc_err_cnt <= 3 || (s_rtc_err_cnt % 20) == 0) {
            ESP_LOGW(TAG, "get time failed: %s", ec.message().c_str());
        }
        return ESP_FAIL;
    }

    if (s_rtc_err_cnt > 0) {
        ESP_LOGI(TAG, "RTC read recovered");
        s_rtc_err_cnt = 0;
    }

    *tm = rtc_tm;
    return ESP_OK;
}

extern "C" esp_err_t service_rtc_sync_to_system(void)
{
    struct tm tm_now;
    esp_err_t ret = service_rtc_get_time(&tm_now);
    if (ret != ESP_OK) {
        return ret;
    }

    time_t t = mktime(&tm_now);
    if (t == (time_t)-1) {
        return ESP_FAIL;
    }

    struct timeval tv = {
        .tv_sec = t,
        .tv_usec = 0,
    };
    if (settimeofday(&tv, NULL) != 0) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

extern "C" void service_rtc_request_sync(void)
{
    s_sync_requested = true;
}

extern "C" esp_err_t service_rtc_process(void)
{
    if (s_rtc == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* 常规每 1 分钟把 RTC 硬件时间同步到系统时间，避免频繁 I2C 读取。
     * App 可通过 service_rtc_request_sync() 在切到前台时立即校准一次。
     * UI 和应用层直接使用 time(NULL) 获取系统时间。 */
    int64_t now = (int64_t)(esp_timer_get_time() / 1000ULL);
    bool need_sync = s_sync_requested ||
                     !s_cache_valid ||
                     (now - s_last_update_ms) >= RTC_SYNC_INTERVAL_MS;
    if (!need_sync) {
        return ESP_OK;
    }
    s_sync_requested = false;

    struct tm tm_now;
    esp_err_t ret = service_rtc_get_time(&tm_now);
    if (ret == ESP_OK) {
        s_cached_tm = tm_now;
        s_cache_valid = true;
        s_last_update_ms = now;
        service_rtc_sync_to_system();
    }
    return ret;
}

extern "C" esp_err_t service_rtc_get_time_cached(struct tm *tm)
{
    if (tm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_cache_valid) {
        esp_err_t ret = service_rtc_process();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    if (!s_cache_valid) {
        return ESP_FAIL;
    }

    *tm = s_cached_tm;
    return ESP_OK;
}

extern "C" esp_err_t service_rtc_set_time(const struct tm *tm)
{
    if (tm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_rtc == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!rtc_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    service_i2c_take();
    std::error_code ec;
    bool ok = s_rtc->set_time(*tm, ec);
    service_i2c_give();
    rtc_give();

    if (!ok) {
        ESP_LOGW(TAG, "set time failed: %s", ec.message().c_str());
        return ESP_FAIL;
    }

    return ESP_OK;
}

extern "C" esp_err_t service_rtc_set_alarm_relative(uint32_t seconds)
{
    if (s_rtc == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!rtc_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    service_i2c_take();
    std::error_code ec;
    std::tm now = s_rtc->get_time(ec);
    if (ec) {
        service_i2c_give();
        rtc_give();
        ESP_LOGW(TAG, "get time for alarm failed: %s", ec.message().c_str());
        return ESP_FAIL;
    }

    time_t t = mktime(&now);
    t += seconds;
    std::tm alarm = *localtime(&t);

    bool ok = s_rtc->set_alarm(alarm, false, ec);
    if (!ok) {
        service_i2c_give();
        rtc_give();
        ESP_LOGW(TAG, "set alarm failed: %s", ec.message().c_str());
        return ESP_FAIL;
    }

    ok = s_rtc->set_alarm_interrupt_enabled(true, ec);
    service_i2c_give();
    rtc_give();

    if (!ok) {
        ESP_LOGW(TAG, "enable alarm interrupt failed: %s", ec.message().c_str());
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "alarm set to %04d-%02d-%02d %02d:%02d:%02d",
             alarm.tm_year + 1900, alarm.tm_mon + 1, alarm.tm_mday,
             alarm.tm_hour, alarm.tm_min, alarm.tm_sec);
    return ESP_OK;
}

extern "C" esp_err_t service_rtc_clear_alarm(void)
{
    if (s_rtc == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    std::error_code ec;
    service_i2c_take();
    bool ok = s_rtc->disable_alarm(ec);
    if (!ok) {
        service_i2c_give();
        ESP_LOGE(TAG, "disable alarm failed: %s", ec.message().c_str());
        return ESP_FAIL;
    }

    ok = s_rtc->clear_alarm_flag(ec);
    service_i2c_give();
    if (!ok) {
        ESP_LOGE(TAG, "clear alarm flag failed: %s", ec.message().c_str());
        return ESP_FAIL;
    }

    return ESP_OK;
}
