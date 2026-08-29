/**
 * @file service_nvs.c
 * @brief 下电参数持久化管理服务实现
 */

#include "service_nvs.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_system.h"  /* esp_restart */

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "string.h"

static const char *TAG = "service_nvs";

#define SERVICE_NVS_NAMESPACE   "tab5_cfg"
#define SERVICE_NVS_KEY_INIT    "init"
#define SERVICE_NVS_KEY_BOOTCNT "boot_cnt"
#define SERVICE_NVS_KEY_BRIGHT  "brightness"
#define SERVICE_NVS_KEY_VOLUME  "volume"
#define SERVICE_NVS_KEY_WIFI_SSID   "wifi_ssid"
#define SERVICE_NVS_KEY_WIFI_PASS       "wifi_pass"
#define SERVICE_NVS_KEY_FEATURES        "features"
#define SERVICE_NVS_KEY_LANGUAGE        "language"
#define SERVICE_NVS_KEY_THEME           "theme"
#define SERVICE_NVS_KEY_EAR_BEST        "ear_best"
#define SERVICE_NVS_KEY_EAR_MODE        "ear_mode"
#define SERVICE_NVS_KEY_EAR_DIFF        "ear_diff"
#define SERVICE_NVS_KEY_EAR_PRAC        "ear_prac"
#define SERVICE_NVS_KEY_METRONOME       "metronome"
#define SERVICE_NVS_KEY_XIAOZHI         "xiaozhi"
#define SERVICE_NVS_KEY_CLOCK_12H       "clock_12h"
#define SERVICE_NVS_KEY_CLOCK_TIMER     "clock_timer"
#define SERVICE_NVS_KEY_CUSTOM_MAC      "custom_mac"
#define SERVICE_NVS_KEY_APP_FUN_MANA    "app_fun_mana"
#define SERVICE_NVS_KEY_MIDI_PLAYER     "midi_player"
#define SERVICE_NVS_KEY_IDLE_TIMEOUT    "idle_timeout"
#define SERVICE_NVS_KEY_AUTO_SLEEP      "auto_sleep"
#define SERVICE_NVS_KEY_BOOT_SCREEN     "boot_screen"
#define SERVICE_NVS_KEY_PIANO           "piano"
#define SERVICE_NVS_KEY_DRUM            "drum_pad"
#define SERVICE_NVS_KEY_ZEN             "zen"
#define SERVICE_NVS_KEY_SF2_SOURCE      "sf2_src"

#define SERVICE_NVS_DIRTY_INIT      (1U << 0)
#define SERVICE_NVS_DIRTY_BOOTCNT   (1U << 1)
#define SERVICE_NVS_DIRTY_BRIGHT    (1U << 2)
#define SERVICE_NVS_DIRTY_VOLUME    (1U << 3)
#define SERVICE_NVS_DIRTY_WIFI_SSID (1U << 4)
#define SERVICE_NVS_DIRTY_WIFI_PASS     (1U << 5)
#define SERVICE_NVS_DIRTY_FEATURES      (1U << 9)
#define SERVICE_NVS_DIRTY_LANGUAGE      (1U << 10)
#define SERVICE_NVS_DIRTY_THEME         (1U << 11)
#define SERVICE_NVS_DIRTY_EAR_BEST      (1U << 12)
#define SERVICE_NVS_DIRTY_METRONOME     (1U << 13)
#define SERVICE_NVS_DIRTY_XIAOZHI       (1U << 14)
#define SERVICE_NVS_DIRTY_CLOCK_12H     (1U << 17)
#define SERVICE_NVS_DIRTY_CLOCK_TIMER   (1U << 18)
#define SERVICE_NVS_DIRTY_CUSTOM_MAC    (1U << 19)
#define SERVICE_NVS_DIRTY_IDLE_TIMEOUT  (1U << 20)
#define SERVICE_NVS_DIRTY_AUTO_SLEEP    (1U << 21)
#define SERVICE_NVS_DIRTY_BOOT_SCREEN   (1U << 22)
#define SERVICE_NVS_DIRTY_PIANO         (1U << 23)
#define SERVICE_NVS_DIRTY_DRUM          (1U << 24)
/* APP_FUN_MANA / MIDI_PLAYER 未走 s_dirty 延迟提交流程（setter 内部直写
 * nvs_set_blob+nvs_commit），对应 dirty bit 仅用作 factory reset 的语义位
 * 记录；真正的擦除在 reset_to_defaults 末尾通过显式 erase_key 完成。 */
#define SERVICE_NVS_DIRTY_APP_FUN_MANA  (1U << 25)
#define SERVICE_NVS_DIRTY_MIDI_PLAYER   (1U << 26)
#define SERVICE_NVS_DIRTY_EAR_MODE      (1U << 27)
#define SERVICE_NVS_DIRTY_EAR_DIFF      (1U << 28)
#define SERVICE_NVS_DIRTY_EAR_PRAC      (1U << 29)
#define SERVICE_NVS_DIRTY_ZEN           (1U << 30)

static nvs_handle_t s_handle = 0;
static SemaphoreHandle_t s_mutex = NULL;

/** @brief 全局系统参数实例，供各模块直接读取；写入请使用 service_nvs_set_* API。 */
struct s_system_parameters system_parameters = {0};

static uint32_t s_dirty = 0;
static bool s_loaded = false;

static void service_nvs_set_defaults(void)
{
    system_parameters.initialized = false;
    /* boot_count 属于内部调试统计：factory reset 不复位，由调用方在需要时
     * 显式清零。set_defaults 被 service_nvs_init 首次上电路径调用时，
     * 这里 boot_count 会被随后的 load_internal / increment_boot_count 覆盖，
     * 所以仅在这里留 0 作为首次开机默认值即可。 */
    system_parameters.boot_count = 0;
    system_parameters.brightness = 80;
    system_parameters.volume = 70;  /* 与 service_audio 默认音量 / engine_gui 默认值 / NVS fallback 对齐 */
    system_parameters.wifi_ssid[0] = '\0';
    system_parameters.wifi_password[0] = '\0';
    system_parameters.feature_flags = SERVICE_NVS_FLAG_WIFI_ENABLED |
                                      SERVICE_NVS_FLAG_USB_HOST_ENABLED;
    strncpy(system_parameters.language, "zh-CN", sizeof(system_parameters.language) - 1);
    system_parameters.language[sizeof(system_parameters.language) - 1] = '\0';

    strncpy(system_parameters.theme_name, "hammyorange", sizeof(system_parameters.theme_name) - 1);
    system_parameters.theme_name[sizeof(system_parameters.theme_name) - 1] = '\0';

    memset(system_parameters.ear_best, 0, sizeof(system_parameters.ear_best));
    system_parameters.ear_mode = 0;        /* 默认绝对音感 */
    system_parameters.ear_difficulty = 0;  /* 默认初级 */
    system_parameters.ear_practice_mode = true; /* 默认练习模式 */

    system_parameters.metronome.bpm = 120;
    system_parameters.metronome.sig_top = 4;
    system_parameters.metronome.sig_bot = 0;
    system_parameters.metronome.sound = 0;
    system_parameters.metronome.reserved = 0;

    memset(&system_parameters.xiaozhi, 0, sizeof(system_parameters.xiaozhi));
    /* custom_mac 与 boot_count 同等处理：首次开机写零，factory reset
     * 不主动擦除（P4 eFuse 未烧录的设备自定义了退路 MAC 的场景下，
     * 擦除会导致再次配网时 MAC 变化）。 */
    memset(system_parameters.custom_mac, 0, sizeof(system_parameters.custom_mac));
    system_parameters.idle_timeout_index = 2;  /* 默认 1 分钟 */
    system_parameters.auto_sleep_enabled = false;
    system_parameters.boot_screen_index = 0;   /* 默认 launcher */
    memset(&system_parameters.piano, 0, sizeof(system_parameters.piano));
    memset(&system_parameters.drum, 0, sizeof(system_parameters.drum));
    memset(&system_parameters.zen, 0, sizeof(system_parameters.zen));
    system_parameters.zen.speed_sel = 2;  /* 默认 3 档，与旧版禅模式默认一致 */
    system_parameters.clock_12h = false;
    system_parameters.clock_timer_s = 0;
    memset(&system_parameters.app_fun_mana, 0, sizeof(system_parameters.app_fun_mana));
    memset(&system_parameters.midi_player, 0, sizeof(system_parameters.midi_player));
}

static bool service_nvs_take(void)
{
    if (s_mutex == NULL) {
        return false;
    }
    return xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY) == pdTRUE;
}

static void service_nvs_give(void)
{
    if (s_mutex != NULL) {
        xSemaphoreGiveRecursive(s_mutex);
    }
}

static esp_err_t service_nvs_read_bool(const char *key, bool *out, bool default_val)
{
    uint8_t val = 0;
    esp_err_t ret = nvs_get_u8(s_handle, key, &val);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        *out = default_val;
        return ESP_OK;
    }
    if (ret != ESP_OK) {
        return ret;
    }
    *out = (val != 0);
    return ESP_OK;
}

static esp_err_t service_nvs_write_bool(const char *key, bool val)
{
    return nvs_set_u8(s_handle, key, val ? 1 : 0);
}

static esp_err_t service_nvs_read_u32(const char *key, uint32_t *out, uint32_t default_val)
{
    esp_err_t ret = nvs_get_u32(s_handle, key, out);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        *out = default_val;
        return ESP_OK;
    }
    return ret;
}

static esp_err_t service_nvs_write_u32(const char *key, uint32_t val)
{
    return nvs_set_u32(s_handle, key, val);
}

static esp_err_t service_nvs_read_u8(const char *key, uint8_t *out, uint8_t default_val)
{
    esp_err_t ret = nvs_get_u8(s_handle, key, out);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        *out = default_val;
        return ESP_OK;
    }
    return ret;
}

static esp_err_t service_nvs_write_u8(const char *key, uint8_t val)
{
    return nvs_set_u8(s_handle, key, val);
}

static esp_err_t service_nvs_read_i16(const char *key, int16_t *out, int16_t default_val)
{
    int16_t val = 0;
    esp_err_t ret = nvs_get_i16(s_handle, key, &val);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        *out = default_val;
        return ESP_OK;
    }
    if (ret != ESP_OK) {
        return ret;
    }
    *out = val;
    return ESP_OK;
}

static esp_err_t service_nvs_write_i16(const char *key, int16_t val)
{
    return nvs_set_i16(s_handle, key, val);
}

static esp_err_t service_nvs_read_str(const char *key, char *out, size_t max_len, const char *default_val)
{
    size_t len = max_len;
    esp_err_t ret = nvs_get_str(s_handle, key, out, &len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        if (default_val != NULL && out != default_val) {
            strncpy(out, default_val, max_len - 1);
            out[max_len - 1] = '\0';
        } else {
            out[0] = '\0';
        }
        return ESP_OK;
    }
    return ret;
}

static esp_err_t service_nvs_write_str(const char *key, const char *val)
{
    return nvs_set_str(s_handle, key, val);
}

static esp_err_t service_nvs_read_blob(const char *key, void *out, size_t len)
{
    size_t actual = len;
    esp_err_t ret = nvs_get_blob(s_handle, key, out, &actual);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    return ret;
}

static esp_err_t service_nvs_write_blob(const char *key, const void *val, size_t len)
{
    return nvs_set_blob(s_handle, key, val, len);
}

static esp_err_t service_nvs_load_internal(void)
{
    esp_err_t ret;

    service_nvs_set_defaults();

    ret = service_nvs_read_bool(SERVICE_NVS_KEY_INIT, &system_parameters.initialized, system_parameters.initialized);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read init failed: %d", ret);
        return ret;
    }

    ret = service_nvs_read_u32(SERVICE_NVS_KEY_BOOTCNT, &system_parameters.boot_count, system_parameters.boot_count);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read boot_cnt failed: %d", ret);
        return ret;
    }

    ret = service_nvs_read_u8(SERVICE_NVS_KEY_BRIGHT, &system_parameters.brightness, system_parameters.brightness);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read brightness failed: %d", ret);
        return ret;
    }

    ret = service_nvs_read_i16(SERVICE_NVS_KEY_VOLUME, &system_parameters.volume, system_parameters.volume);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read volume failed: %d", ret);
        return ret;
    }

    ret = service_nvs_read_str(SERVICE_NVS_KEY_WIFI_SSID, system_parameters.wifi_ssid,
                               sizeof(system_parameters.wifi_ssid), system_parameters.wifi_ssid);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read wifi_ssid failed: %d", ret);
        return ret;
    }

    ret = service_nvs_read_str(SERVICE_NVS_KEY_WIFI_PASS, system_parameters.wifi_password,
                               sizeof(system_parameters.wifi_password), system_parameters.wifi_password);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read wifi_pass failed: %d", ret);
        return ret;
    }

    ret = service_nvs_read_u32(SERVICE_NVS_KEY_FEATURES, &system_parameters.feature_flags, system_parameters.feature_flags);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read features failed: %d", ret);
        return ret;
    }

    ret = service_nvs_read_str(SERVICE_NVS_KEY_LANGUAGE, system_parameters.language,
                               sizeof(system_parameters.language), system_parameters.language);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read language failed: %d", ret);
        return ret;
    }

    ret = service_nvs_read_str(SERVICE_NVS_KEY_THEME, system_parameters.theme_name,
                               sizeof(system_parameters.theme_name), system_parameters.theme_name);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read theme failed: %d", ret);
        return ret;
    }

    ret = service_nvs_read_blob(SERVICE_NVS_KEY_EAR_BEST, system_parameters.ear_best,
                                sizeof(system_parameters.ear_best));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read ear_best failed: %d", ret);
        return ret;
    }

    ret = service_nvs_read_u8(SERVICE_NVS_KEY_EAR_MODE, &system_parameters.ear_mode,
                              system_parameters.ear_mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read ear_mode failed: %d", ret);
        return ret;
    }

    ret = service_nvs_read_u8(SERVICE_NVS_KEY_EAR_DIFF, &system_parameters.ear_difficulty,
                              system_parameters.ear_difficulty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read ear_diff failed: %d", ret);
        return ret;
    }

    ret = service_nvs_read_bool(SERVICE_NVS_KEY_EAR_PRAC, &system_parameters.ear_practice_mode,
                                system_parameters.ear_practice_mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read ear_prac failed: %d", ret);
        return ret;
    }

    ret = service_nvs_read_blob(SERVICE_NVS_KEY_METRONOME, &system_parameters.metronome,
                                sizeof(system_parameters.metronome));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read metronome failed: %d", ret);
        return ret;
    }

    ret = service_nvs_read_bool(SERVICE_NVS_KEY_CLOCK_12H, &system_parameters.clock_12h, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read clock_12h failed: %d", ret);
        return ret;
    }

    ret = service_nvs_read_u32(SERVICE_NVS_KEY_CLOCK_TIMER, &system_parameters.clock_timer_s, 300);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read clock_timer failed: %d", ret);
        return ret;
    }

    /* xiaozhi blob 兼容读：旧版 blob 无 wake_anywhere 字段，比新结构短；
     * nvs_get_blob 缓冲区大于存储长度时按存储长度拷入、新字段保持
     * 默认值，凭据不丢；仅缓冲区小于存储长度才报错 */
    ret = service_nvs_read_blob(SERVICE_NVS_KEY_XIAOZHI, &system_parameters.xiaozhi,
                                sizeof(system_parameters.xiaozhi));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "read xiaozhi failed: %d, use defaults", ret);
    }

    ret = service_nvs_read_blob(SERVICE_NVS_KEY_CUSTOM_MAC, system_parameters.custom_mac,
                                sizeof(system_parameters.custom_mac));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "read custom_mac not found, will use default zeros");
    }

    ret = service_nvs_read_u8(SERVICE_NVS_KEY_IDLE_TIMEOUT, &system_parameters.idle_timeout_index,
                              system_parameters.idle_timeout_index);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read idle_timeout failed: %d", ret);
        return ret;
    }

    ret = service_nvs_read_bool(SERVICE_NVS_KEY_AUTO_SLEEP, &system_parameters.auto_sleep_enabled,
                                system_parameters.auto_sleep_enabled);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read auto_sleep failed: %d", ret);
        return ret;
    }

    ret = service_nvs_read_u8(SERVICE_NVS_KEY_BOOT_SCREEN, &system_parameters.boot_screen_index,
                              system_parameters.boot_screen_index);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read boot_screen failed: %d", ret);
        return ret;
    }

    /* piano/drum 参数为后加 key，缺失时保持默认值（read_blob 对 NOT_FOUND 返回 OK） */
    ret = service_nvs_read_blob(SERVICE_NVS_KEY_PIANO, &system_parameters.piano,
                                sizeof(system_parameters.piano));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "read piano failed: %d, use defaults", ret);
    }

    ret = service_nvs_read_blob(SERVICE_NVS_KEY_DRUM, &system_parameters.drum,
                                sizeof(system_parameters.drum));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "read drum failed: %d, use defaults", ret);
    }

    s_dirty = 0;
    /* zen 参数为后加 key，缺失时保持默认值（read_blob 对 NOT_FOUND 返回 OK） */
    ret = service_nvs_read_blob(SERVICE_NVS_KEY_ZEN, &system_parameters.zen,
                                sizeof(system_parameters.zen));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "read zen failed: %d, use defaults", ret);
    }

    s_loaded = true;

    ESP_LOGI(TAG, "loaded: init=%d, boot=%lu, bri=%u, vol=%d, flags=0x%08lx, theme=%s",
             system_parameters.initialized, (unsigned long)system_parameters.boot_count,
             system_parameters.brightness, system_parameters.volume,
             (unsigned long)system_parameters.feature_flags,
             system_parameters.theme_name);
    return ESP_OK;
}

esp_err_t service_nvs_init(void)
{
    if (s_handle != 0) {
        return ESP_OK;
    }

    s_mutex = xSemaphoreCreateRecursiveMutex();
    if (s_mutex == NULL) {
        ESP_LOGE(TAG, "mutex create failed");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = nvs_open(SERVICE_NVS_NAMESPACE, NVS_READWRITE, &s_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %d", ret);
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return ret;
    }

    if (!service_nvs_take()) {
        nvs_close(s_handle);
        s_handle = 0;
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return ESP_ERR_INVALID_STATE;
    }

    ret = service_nvs_load_internal();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "load failed, use defaults");
        service_nvs_set_defaults();
    }

    system_parameters.boot_count++;
    s_dirty |= SERVICE_NVS_DIRTY_BOOTCNT;

    ret = service_nvs_commit();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "initial commit failed: %d", ret);
    }

    service_nvs_give();
    return ESP_OK;
}

esp_err_t service_nvs_commit(void)
{
    if (s_handle == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    /* 无脏数据直接返回：避免空提交占用互斥锁与触发 NVS 页维护 */
    if (s_dirty == 0) {
        return ESP_OK;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_OK;

    if (s_dirty & SERVICE_NVS_DIRTY_INIT) {
        ret = service_nvs_write_bool(SERVICE_NVS_KEY_INIT, system_parameters.initialized);
        if (ret != ESP_OK) {
            goto commit_exit;
        }
    }

    if (s_dirty & SERVICE_NVS_DIRTY_BOOTCNT) {
        ret = service_nvs_write_u32(SERVICE_NVS_KEY_BOOTCNT, system_parameters.boot_count);
        if (ret != ESP_OK) {
            goto commit_exit;
        }
    }

    if (s_dirty & SERVICE_NVS_DIRTY_BRIGHT) {
        ret = service_nvs_write_u8(SERVICE_NVS_KEY_BRIGHT, system_parameters.brightness);
        if (ret != ESP_OK) {
            goto commit_exit;
        }
    }

    if (s_dirty & SERVICE_NVS_DIRTY_VOLUME) {
        ret = service_nvs_write_i16(SERVICE_NVS_KEY_VOLUME, system_parameters.volume);
        if (ret != ESP_OK) {
            goto commit_exit;
        }
    }

    if (s_dirty & SERVICE_NVS_DIRTY_WIFI_SSID) {
        ret = service_nvs_write_str(SERVICE_NVS_KEY_WIFI_SSID, system_parameters.wifi_ssid);
        if (ret != ESP_OK) {
            goto commit_exit;
        }
    }

    if (s_dirty & SERVICE_NVS_DIRTY_WIFI_PASS) {
        ret = service_nvs_write_str(SERVICE_NVS_KEY_WIFI_PASS, system_parameters.wifi_password);
        if (ret != ESP_OK) {
            goto commit_exit;
        }
    }

    if (s_dirty & SERVICE_NVS_DIRTY_FEATURES) {
        ret = service_nvs_write_u32(SERVICE_NVS_KEY_FEATURES, system_parameters.feature_flags);
        if (ret != ESP_OK) {
            goto commit_exit;
        }
    }

    if (s_dirty & SERVICE_NVS_DIRTY_LANGUAGE) {
        ret = service_nvs_write_str(SERVICE_NVS_KEY_LANGUAGE, system_parameters.language);
        if (ret != ESP_OK) {
            goto commit_exit;
        }
    }

    if (s_dirty & SERVICE_NVS_DIRTY_THEME) {
        ret = service_nvs_write_str(SERVICE_NVS_KEY_THEME, system_parameters.theme_name);
        if (ret != ESP_OK) {
            goto commit_exit;
        }
    }

    if (s_dirty & SERVICE_NVS_DIRTY_EAR_BEST) {
        ret = service_nvs_write_blob(SERVICE_NVS_KEY_EAR_BEST, system_parameters.ear_best,
                                     sizeof(system_parameters.ear_best));
        if (ret != ESP_OK) {
            goto commit_exit;
        }
    }

    if (s_dirty & SERVICE_NVS_DIRTY_EAR_MODE) {
        ret = service_nvs_write_u8(SERVICE_NVS_KEY_EAR_MODE, system_parameters.ear_mode);
        if (ret != ESP_OK) {
            goto commit_exit;
        }
    }

    if (s_dirty & SERVICE_NVS_DIRTY_EAR_DIFF) {
        ret = service_nvs_write_u8(SERVICE_NVS_KEY_EAR_DIFF, system_parameters.ear_difficulty);
        if (ret != ESP_OK) {
            goto commit_exit;
        }
    }

    if (s_dirty & SERVICE_NVS_DIRTY_EAR_PRAC) {
        ret = service_nvs_write_bool(SERVICE_NVS_KEY_EAR_PRAC, system_parameters.ear_practice_mode);
        if (ret != ESP_OK) {
            goto commit_exit;
        }
    }

    if (s_dirty & SERVICE_NVS_DIRTY_METRONOME) {
        ret = service_nvs_write_blob(SERVICE_NVS_KEY_METRONOME, &system_parameters.metronome,
                                     sizeof(system_parameters.metronome));
        if (ret != ESP_OK) {
            goto commit_exit;
        }
    }

    if (s_dirty & SERVICE_NVS_DIRTY_CLOCK_12H) {
        ret = service_nvs_write_bool(SERVICE_NVS_KEY_CLOCK_12H, system_parameters.clock_12h);
        if (ret != ESP_OK) {
            goto commit_exit;
        }
    }

    if (s_dirty & SERVICE_NVS_DIRTY_CLOCK_TIMER) {
        ret = service_nvs_write_u32(SERVICE_NVS_KEY_CLOCK_TIMER, system_parameters.clock_timer_s);
        if (ret != ESP_OK) {
            goto commit_exit;
        }
    }

    if (s_dirty & SERVICE_NVS_DIRTY_XIAOZHI) {
        ret = service_nvs_write_blob(SERVICE_NVS_KEY_XIAOZHI, &system_parameters.xiaozhi,
                                     sizeof(system_parameters.xiaozhi));
        if (ret != ESP_OK) {
            goto commit_exit;
        }
    }

    if (s_dirty & SERVICE_NVS_DIRTY_CUSTOM_MAC) {
        ret = service_nvs_write_blob(SERVICE_NVS_KEY_CUSTOM_MAC,
                                     system_parameters.custom_mac,
                                     sizeof(system_parameters.custom_mac));
        if (ret != ESP_OK) {
            goto commit_exit;
        }
    }

    if (s_dirty & SERVICE_NVS_DIRTY_IDLE_TIMEOUT) {
        ret = service_nvs_write_u8(SERVICE_NVS_KEY_IDLE_TIMEOUT, system_parameters.idle_timeout_index);
        if (ret != ESP_OK) {
            goto commit_exit;
        }
    }

    if (s_dirty & SERVICE_NVS_DIRTY_AUTO_SLEEP) {
        ret = service_nvs_write_bool(SERVICE_NVS_KEY_AUTO_SLEEP, system_parameters.auto_sleep_enabled);
        if (ret != ESP_OK) {
            goto commit_exit;
        }
    }

    if (s_dirty & SERVICE_NVS_DIRTY_BOOT_SCREEN) {
        ret = service_nvs_write_u8(SERVICE_NVS_KEY_BOOT_SCREEN, system_parameters.boot_screen_index);
        if (ret != ESP_OK) {
            goto commit_exit;
        }
    }

    if (s_dirty & SERVICE_NVS_DIRTY_PIANO) {
        ret = service_nvs_write_blob(SERVICE_NVS_KEY_PIANO, &system_parameters.piano,
                                     sizeof(system_parameters.piano));
        if (ret != ESP_OK) {
            goto commit_exit;
        }
    }

    if (s_dirty & SERVICE_NVS_DIRTY_ZEN) {
        ret = service_nvs_write_blob(SERVICE_NVS_KEY_ZEN, &system_parameters.zen,
                                     sizeof(system_parameters.zen));
        if (ret != ESP_OK) {
            goto commit_exit;
        }
    }

    if (s_dirty & SERVICE_NVS_DIRTY_DRUM) {
        ret = service_nvs_write_blob(SERVICE_NVS_KEY_DRUM, &system_parameters.drum,
                                     sizeof(system_parameters.drum));
        if (ret != ESP_OK) {
            goto commit_exit;
        }
    }

    ret = nvs_commit(s_handle);
    if (ret == ESP_OK) {
        s_dirty = 0;
    }

commit_exit:
    service_nvs_give();
    return ret;
}

esp_err_t service_nvs_load(void)
{
    if (s_handle == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = service_nvs_load_internal();
    service_nvs_give();
    return ret;
}

esp_err_t service_nvs_reset_to_defaults(void)
{
    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    /* 把要保留的内部调试参数先 snapshot，再整体 set_defaults，最后回写。
     * Why 不直接改 set_defaults：首次上电初始化路径需要这些值从零开始，
     * factory reset 则是增量保留。 */
    uint32_t keep_boot_count = system_parameters.boot_count;
    uint8_t  keep_custom_mac[sizeof(system_parameters.custom_mac)];
    memcpy(keep_custom_mac, system_parameters.custom_mac, sizeof(keep_custom_mac));

    service_nvs_set_defaults();

    system_parameters.boot_count = keep_boot_count;
    memcpy(system_parameters.custom_mac, keep_custom_mac, sizeof(system_parameters.custom_mac));

    /* initialized=false 由 set_defaults 写入；重启后 get_var_sys_onboard_flag()
     * 返回 false，engine_gui boot_progress(100) 会自动跳 onboard_step 屏。 */

    /* 全量脏位：包含 set_defaults 改动的全部存储字段。
     * SERVICE_NVS_DIRTY_BOOTCNT / SERVICE_NVS_DIRTY_CUSTOM_MAC 故意不标：
     *   这两个内部调试参数是"保留"的，不应该在 reset_to_defaults 时
     *   用 snapshot 的值强制覆盖 NVS 中已存的相同值（同值写无副作用，
     *   但漏标和"实际没变"比多标更安全，避免引入与期望不一致的写回路径）。 */
    s_dirty = SERVICE_NVS_DIRTY_INIT |
              SERVICE_NVS_DIRTY_BRIGHT |
              SERVICE_NVS_DIRTY_VOLUME |
              SERVICE_NVS_DIRTY_WIFI_SSID |
              SERVICE_NVS_DIRTY_WIFI_PASS |
              SERVICE_NVS_DIRTY_FEATURES |
              SERVICE_NVS_DIRTY_LANGUAGE |
              SERVICE_NVS_DIRTY_THEME |
              SERVICE_NVS_DIRTY_EAR_BEST |
              SERVICE_NVS_DIRTY_EAR_MODE |
              SERVICE_NVS_DIRTY_EAR_DIFF |
              SERVICE_NVS_DIRTY_EAR_PRAC |
              SERVICE_NVS_DIRTY_METRONOME |
              SERVICE_NVS_DIRTY_XIAOZHI |
              SERVICE_NVS_DIRTY_CLOCK_12H |
              SERVICE_NVS_DIRTY_CLOCK_TIMER |
              SERVICE_NVS_DIRTY_APP_FUN_MANA |
              SERVICE_NVS_DIRTY_MIDI_PLAYER |
              SERVICE_NVS_DIRTY_IDLE_TIMEOUT |
              SERVICE_NVS_DIRTY_AUTO_SLEEP |
              SERVICE_NVS_DIRTY_BOOT_SCREEN |
              SERVICE_NVS_DIRTY_PIANO |
              SERVICE_NVS_DIRTY_DRUM |
              SERVICE_NVS_DIRTY_ZEN;

    esp_err_t ret = service_nvs_commit();

    /* 不走 s_dirty 延迟提交的 blob（setter 内部直写 nvs_commit）：
     * service_nvs_commit 内部无对应分支写入，所以 commit 之后显式
     * erase_key 再单独 nvs_commit 一次，保证下次读取时返回默认值。 */
    if (ret == ESP_OK) {
        esp_err_t r1 = nvs_erase_key(s_handle, SERVICE_NVS_KEY_APP_FUN_MANA);
        esp_err_t r2 = nvs_erase_key(s_handle, SERVICE_NVS_KEY_MIDI_PLAYER);
        if (r1 == ESP_OK || r2 == ESP_OK) {
            esp_err_t rc = nvs_commit(s_handle);
            if (ret == ESP_OK && rc != ESP_OK) {
                ret = rc;
            }
        } else {
            /* ESP_ERR_NVS_NOT_FOUND 不算失败（本来就没存过） */
            if (r1 != ESP_ERR_NVS_NOT_FOUND && ret == ESP_OK) ret = r1;
            if (r2 != ESP_ERR_NVS_NOT_FOUND && ret == ESP_OK) ret = r2;
        }
    }

    service_nvs_give();
    return ret;
}

esp_err_t service_nvs_factory_reset(void)
{
    ESP_LOGW(TAG, "factory reset triggered: reset all user settings + keep boot_count/custom_mac + force onboard after reboot");

    esp_err_t ret = service_nvs_reset_to_defaults();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "factory reset: reset_to_defaults failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 保证所有缓存落盘后再重启（WiFi/TTS/Audio 等后台线程仍在跑，
     * 延迟 100ms 给 on-going 打印缓冲一下）。 */
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();

    return ESP_OK;  /* 不会到这里；仅为编译器 */
}

bool service_nvs_is_initialized(void)
{
    bool val = false;

    if (!service_nvs_take()) {
        return false;
    }

    if (s_loaded) {
        val = system_parameters.initialized;
    }

    service_nvs_give();
    return val;
}

esp_err_t service_nvs_set_initialized(bool initialized)
{
    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    system_parameters.initialized = initialized;
    s_dirty |= SERVICE_NVS_DIRTY_INIT;

    service_nvs_give();
    return ESP_OK;
}

uint32_t service_nvs_get_boot_count(void)
{
    uint32_t val = 0;

    if (!service_nvs_take()) {
        return 0;
    }

    if (s_loaded) {
        val = system_parameters.boot_count;
    }

    service_nvs_give();
    return val;
}

esp_err_t service_nvs_increment_boot_count(void)
{
    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    system_parameters.boot_count++;
    s_dirty |= SERVICE_NVS_DIRTY_BOOTCNT;

    service_nvs_give();
    return service_nvs_commit();
}

uint8_t service_nvs_get_brightness(void)
{
    uint8_t val = 80;

    if (!service_nvs_take()) {
        return val;
    }

    if (s_loaded) {
        val = system_parameters.brightness;
    }

    service_nvs_give();
    return val;
}

esp_err_t service_nvs_set_brightness(uint8_t brightness)
{
    if (brightness > 100) {
        brightness = 100;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    system_parameters.brightness = brightness;
    s_dirty |= SERVICE_NVS_DIRTY_BRIGHT;

    service_nvs_give();
    return ESP_OK;
}

int16_t service_nvs_get_volume(void)
{
    int16_t val = 70;  /* perceptual 曲线下 70 ≈ -10 dB，匹配 service_audio / engine_gui 默认 */

    if (!service_nvs_take()) {
        return val;
    }

    if (s_loaded) {
        val = system_parameters.volume;
    }

    service_nvs_give();
    return val;
}

esp_err_t service_nvs_set_volume(int16_t volume)
{
    if (volume < 0) {
        volume = 0;
    } else if (volume > 100) {
        volume = 100;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    system_parameters.volume = volume;
    s_dirty |= SERVICE_NVS_DIRTY_VOLUME;

    service_nvs_give();
    return ESP_OK;
}

esp_err_t service_nvs_get_wifi_ssid(char *ssid, size_t len)
{
    if (ssid == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    strncpy(ssid, system_parameters.wifi_ssid, len - 1);
    ssid[len - 1] = '\0';

    service_nvs_give();
    return ESP_OK;
}

esp_err_t service_nvs_set_wifi_ssid(const char *ssid)
{
    if (ssid == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    strncpy(system_parameters.wifi_ssid, ssid, sizeof(system_parameters.wifi_ssid) - 1);
    system_parameters.wifi_ssid[sizeof(system_parameters.wifi_ssid) - 1] = '\0';
    s_dirty |= SERVICE_NVS_DIRTY_WIFI_SSID;

    service_nvs_give();
    return ESP_OK;
}

esp_err_t service_nvs_get_wifi_password(char *password, size_t len)
{
    if (password == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    strncpy(password, system_parameters.wifi_password, len - 1);
    password[len - 1] = '\0';

    service_nvs_give();
    return ESP_OK;
}

esp_err_t service_nvs_set_wifi_password(const char *password)
{
    if (password == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    strncpy(system_parameters.wifi_password, password, sizeof(system_parameters.wifi_password) - 1);
    system_parameters.wifi_password[sizeof(system_parameters.wifi_password) - 1] = '\0';
    s_dirty |= SERVICE_NVS_DIRTY_WIFI_PASS;

    service_nvs_give();
    return ESP_OK;
}

uint32_t service_nvs_get_feature_flags(void)
{
    uint32_t val = 0;

    if (!service_nvs_take()) {
        return 0;
    }

    if (s_loaded) {
        val = system_parameters.feature_flags;
    }

    service_nvs_give();
    return val;
}

esp_err_t service_nvs_set_feature_flags(uint32_t flags)
{
    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    system_parameters.feature_flags = flags;
    s_dirty |= SERVICE_NVS_DIRTY_FEATURES;

    service_nvs_give();
    return ESP_OK;
}

bool service_nvs_get_feature_flag(uint32_t mask)
{
    bool val = false;

    if (!service_nvs_take()) {
        return false;
    }

    if (s_loaded) {
        val = (system_parameters.feature_flags & mask) != 0;
    }

    service_nvs_give();
    return val;
}

esp_err_t service_nvs_set_feature_flag(uint32_t mask, bool enable)
{
    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    if (enable) {
        system_parameters.feature_flags |= mask;
    } else {
        system_parameters.feature_flags &= ~mask;
    }
    s_dirty |= SERVICE_NVS_DIRTY_FEATURES;

    service_nvs_give();
    return ESP_OK;
}

esp_err_t service_nvs_get_language(char *lang, size_t len)
{
    if (lang == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    strncpy(lang, system_parameters.language, len - 1);
    lang[len - 1] = '\0';

    service_nvs_give();
    return ESP_OK;
}

esp_err_t service_nvs_set_language(const char *lang)
{
    if (lang == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    strncpy(system_parameters.language, lang, sizeof(system_parameters.language) - 1);
    system_parameters.language[sizeof(system_parameters.language) - 1] = '\0';
    s_dirty |= SERVICE_NVS_DIRTY_LANGUAGE;

    service_nvs_give();
    return ESP_OK;
}

esp_err_t service_nvs_get_theme(char *theme, size_t len)
{
    if (theme == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    strncpy(theme, system_parameters.theme_name, len - 1);
    theme[len - 1] = '\0';

    service_nvs_give();
    return ESP_OK;
}

esp_err_t service_nvs_set_theme(const char *theme)
{
    if (theme == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    strncpy(system_parameters.theme_name, theme, sizeof(system_parameters.theme_name) - 1);
    system_parameters.theme_name[sizeof(system_parameters.theme_name) - 1] = '\0';
    s_dirty |= SERVICE_NVS_DIRTY_THEME;

    service_nvs_give();
    return ESP_OK;
}

uint32_t service_nvs_get_ear_best(uint8_t index)
{
    if (index >= 6) {
        return 0;
    }

    if (!service_nvs_take()) {
        return 0;
    }

    uint32_t val = s_loaded ? system_parameters.ear_best[index] : 0;

    service_nvs_give();
    return val;
}

esp_err_t service_nvs_set_ear_best(uint8_t index, uint32_t score)
{
    if (index >= 6) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    system_parameters.ear_best[index] = score;
    s_dirty |= SERVICE_NVS_DIRTY_EAR_BEST;

    service_nvs_give();
    return ESP_OK;
}

esp_err_t service_nvs_set_ear_cfg(uint8_t mode, uint8_t difficulty, bool practice_mode)
{
    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    system_parameters.ear_mode = mode;
    system_parameters.ear_difficulty = difficulty;
    system_parameters.ear_practice_mode = practice_mode;
    s_dirty |= SERVICE_NVS_DIRTY_EAR_MODE |
               SERVICE_NVS_DIRTY_EAR_DIFF |
               SERVICE_NVS_DIRTY_EAR_PRAC;

    service_nvs_give();
    return ESP_OK;
}

void service_nvs_get_metronome(service_nvs_metronome_t *out)
{
    if (out == NULL) {
        return;
    }

    if (!service_nvs_take()) {
        out->bpm = 120;
        out->sig_top = 4;
        out->sig_bot = 0;
        out->sound = 0;
        out->reserved = 0;
        return;
    }

    if (s_loaded) {
        *out = system_parameters.metronome;
    } else {
        out->bpm = 120;
        out->sig_top = 4;
        out->sig_bot = 0;
        out->sound = 0;
        out->reserved = 0;
    }

    service_nvs_give();
}

esp_err_t service_nvs_set_metronome(const service_nvs_metronome_t *params)
{
    if (params == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    system_parameters.metronome = *params;
    s_dirty |= SERVICE_NVS_DIRTY_METRONOME;

    service_nvs_give();
    return ESP_OK;
}

esp_err_t service_nvs_get_xz_uuid(char *uuid, size_t len)
{
    if (uuid == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    strncpy(uuid, system_parameters.xiaozhi.uuid, len - 1);
    uuid[len - 1] = '\0';

    service_nvs_give();
    return ESP_OK;
}

esp_err_t service_nvs_set_xz_uuid(const char *uuid)
{
    if (uuid == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    strncpy(system_parameters.xiaozhi.uuid, uuid, sizeof(system_parameters.xiaozhi.uuid) - 1);
    system_parameters.xiaozhi.uuid[sizeof(system_parameters.xiaozhi.uuid) - 1] = '\0';
    s_dirty |= SERVICE_NVS_DIRTY_XIAOZHI;

    service_nvs_give();
    return ESP_OK;
}

esp_err_t service_nvs_get_xz_ws_url(char *url, size_t len)
{
    if (url == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    strncpy(url, system_parameters.xiaozhi.ws_url, len - 1);
    url[len - 1] = '\0';

    service_nvs_give();
    return ESP_OK;
}

esp_err_t service_nvs_set_xz_ws_url(const char *url)
{
    if (url == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    strncpy(system_parameters.xiaozhi.ws_url, url, sizeof(system_parameters.xiaozhi.ws_url) - 1);
    system_parameters.xiaozhi.ws_url[sizeof(system_parameters.xiaozhi.ws_url) - 1] = '\0';
    s_dirty |= SERVICE_NVS_DIRTY_XIAOZHI;

    service_nvs_give();
    return ESP_OK;
}

esp_err_t service_nvs_get_xz_ws_token(char *token, size_t len)
{
    if (token == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    strncpy(token, system_parameters.xiaozhi.ws_token, len - 1);
    token[len - 1] = '\0';

    service_nvs_give();
    return ESP_OK;
}

esp_err_t service_nvs_set_xz_ws_token(const char *token)
{
    if (token == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    strncpy(system_parameters.xiaozhi.ws_token, token, sizeof(system_parameters.xiaozhi.ws_token) - 1);
    system_parameters.xiaozhi.ws_token[sizeof(system_parameters.xiaozhi.ws_token) - 1] = '\0';
    s_dirty |= SERVICE_NVS_DIRTY_XIAOZHI;

    service_nvs_give();
    return ESP_OK;
}

bool service_nvs_get_xz_wake_anywhere(void)
{
    bool val = false;
    if (!service_nvs_take()) {
        return val;
    }
    val = system_parameters.xiaozhi.wake_anywhere;
    service_nvs_give();
    return val;
}

esp_err_t service_nvs_set_xz_wake_anywhere(bool enable)
{
    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }
    system_parameters.xiaozhi.wake_anywhere = enable;
    s_dirty |= SERVICE_NVS_DIRTY_XIAOZHI;
    service_nvs_give();
    return ESP_OK;
}

bool service_nvs_get_clock_12h(void)
{
    bool val = false;

    if (!service_nvs_take()) {
        return val;
    }

    if (s_loaded) {
        val = system_parameters.clock_12h;
    }

    service_nvs_give();
    return val;
}

esp_err_t service_nvs_set_clock_12h(bool use_12h)
{
    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    system_parameters.clock_12h = use_12h;
    s_dirty |= SERVICE_NVS_DIRTY_CLOCK_12H;

    service_nvs_give();
    return ESP_OK;
}

uint32_t service_nvs_get_clock_timer_s(void)
{
    uint32_t val = 300;

    if (!service_nvs_take()) {
        return val;
    }

    if (s_loaded) {
        val = system_parameters.clock_timer_s;
    }

    service_nvs_give();
    return val;
}

esp_err_t service_nvs_set_clock_timer_s(uint32_t seconds)
{
    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    system_parameters.clock_timer_s = seconds;
    s_dirty |= SERVICE_NVS_DIRTY_CLOCK_TIMER;

    service_nvs_give();
    return ESP_OK;
}

esp_err_t service_nvs_get_custom_mac(uint8_t *mac, size_t len)
{
    if (mac == NULL || len < 6) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    /* 如果未设置过，返回全零 */
    bool has_data = false;
    for (int i = 0; i < 6; i++) {
        if (system_parameters.custom_mac[i] != 0) {
            has_data = true;
            break;
        }
    }

    memcpy(mac, system_parameters.custom_mac, 6);
    service_nvs_give();

    /* 若全为零则视为未设置 */
    if (!has_data) {
        memset(mac, 0, 6);
    }

    return ESP_OK;
}

esp_err_t service_nvs_set_custom_mac(const uint8_t *mac)
{
    if (mac == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    memcpy(system_parameters.custom_mac, mac, 6);
    s_dirty |= SERVICE_NVS_DIRTY_CUSTOM_MAC;

    service_nvs_give();
    return service_nvs_commit();
}

esp_err_t service_nvs_get_app_fun_mana(service_nvs_app_fun_mana_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_handle == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t len = sizeof(*out);
    esp_err_t ret = nvs_get_blob(s_handle, SERVICE_NVS_KEY_APP_FUN_MANA, out, &len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        out->mana = 100;
        out->year = 0;
        out->month = 0;
        out->day = 0;
        out->reserved = 0;
        ret = ESP_OK;
    }

    service_nvs_give();
    return ret;
}

esp_err_t service_nvs_set_app_fun_mana(const service_nvs_app_fun_mana_t *in)
{
    if (in == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_handle == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = nvs_set_blob(s_handle, SERVICE_NVS_KEY_APP_FUN_MANA, in, sizeof(*in));
    if (ret == ESP_OK) {
        ret = nvs_commit(s_handle);
    }

    service_nvs_give();
    return ret;
}

esp_err_t service_nvs_get_midi_player(service_nvs_midi_player_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_handle == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t len = sizeof(*out);
    esp_err_t ret = nvs_get_blob(s_handle, SERVICE_NVS_KEY_MIDI_PLAYER, out, &len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        out->play_type = 0;
        out->filename[0] = '\0';
        memset(out->reserved, 0, sizeof(out->reserved));
        ret = ESP_OK;
    }

    service_nvs_give();
    return ret;
}

esp_err_t service_nvs_set_midi_player(const service_nvs_midi_player_t *in)
{
    if (in == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_handle == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = nvs_set_blob(s_handle, SERVICE_NVS_KEY_MIDI_PLAYER, in, sizeof(*in));
    if (ret == ESP_OK) {
        ret = nvs_commit(s_handle);
    }

    service_nvs_give();
    return ret;
}

/* SF2 音源选择：直写直提（同 midi_player），空串 = 内部预设 */
esp_err_t service_nvs_get_sf2_source(char *out, size_t len)
{
    if (out == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_handle == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t actual = len;
    esp_err_t ret = nvs_get_str(s_handle, SERVICE_NVS_KEY_SF2_SOURCE, out, &actual);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        out[0] = '\0';
        ret = ESP_OK;
    }

    service_nvs_give();
    return ret;
}

esp_err_t service_nvs_set_sf2_source(const char *name)
{
    if (name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_handle == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = nvs_set_str(s_handle, SERVICE_NVS_KEY_SF2_SOURCE, name);
    if (ret == ESP_OK) {
        ret = nvs_commit(s_handle);
    }

    service_nvs_give();
    return ret;
}

uint8_t service_nvs_get_idle_timeout_index(void)
{
    uint8_t val = 2;  /* 默认 1 分钟 */

    if (!service_nvs_take()) {
        return val;
    }

    if (s_loaded) {
        val = system_parameters.idle_timeout_index;
    }

    service_nvs_give();
    return val;
}

esp_err_t service_nvs_set_idle_timeout_index(uint8_t index)
{
    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    system_parameters.idle_timeout_index = index;
    s_dirty |= SERVICE_NVS_DIRTY_IDLE_TIMEOUT;

    service_nvs_give();
    return ESP_OK;
}

bool service_nvs_get_auto_sleep_enabled(void)
{
    bool val = false;

    if (!service_nvs_take()) {
        return val;
    }

    if (s_loaded) {
        val = system_parameters.auto_sleep_enabled;
    }

    service_nvs_give();
    return val;
}

esp_err_t service_nvs_set_auto_sleep_enabled(bool enable)
{
    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    system_parameters.auto_sleep_enabled = enable;
    s_dirty |= SERVICE_NVS_DIRTY_AUTO_SLEEP;

    service_nvs_give();
    return ESP_OK;
}

uint8_t service_nvs_get_boot_screen_index(void)
{
    uint8_t val = 0;  /* 默认 launcher */

    if (!service_nvs_take()) {
        return val;
    }

    if (s_loaded) {
        val = system_parameters.boot_screen_index;
    }

    service_nvs_give();
    return val;
}

esp_err_t service_nvs_set_boot_screen_index(uint8_t index)
{
    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    system_parameters.boot_screen_index = index;
    s_dirty |= SERVICE_NVS_DIRTY_BOOT_SCREEN;

    service_nvs_give();
    return ESP_OK;
}

void service_nvs_get_piano(service_nvs_piano_t *out)
{
    if (out == NULL) {
        return;
    }

    if (!service_nvs_take()) {
        memset(out, 0, sizeof(service_nvs_piano_t));
        return;
    }

    memcpy(out, &system_parameters.piano, sizeof(service_nvs_piano_t));
    service_nvs_give();
}

esp_err_t service_nvs_set_piano(const service_nvs_piano_t *params)
{
    if (params == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    memcpy(&system_parameters.piano, params, sizeof(service_nvs_piano_t));
    s_dirty |= SERVICE_NVS_DIRTY_PIANO;

    service_nvs_give();
    return ESP_OK;
}

void service_nvs_get_drum(service_nvs_drum_t *out)
{
    if (out == NULL) {
        return;
    }

    if (!service_nvs_take()) {
        memset(out, 0, sizeof(service_nvs_drum_t));
        return;
    }

    memcpy(out, &system_parameters.drum, sizeof(service_nvs_drum_t));
    service_nvs_give();
}

esp_err_t service_nvs_set_drum(const service_nvs_drum_t *params)
{
    if (params == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    memcpy(&system_parameters.drum, params, sizeof(service_nvs_drum_t));
    s_dirty |= SERVICE_NVS_DIRTY_DRUM;

    service_nvs_give();
    return ESP_OK;
}

void service_nvs_get_zen(service_nvs_zen_t *out)
{
    if (out == NULL) {
        return;
    }

    if (!service_nvs_take()) {
        memset(out, 0, sizeof(service_nvs_zen_t));
        return;
    }

    memcpy(out, &system_parameters.zen, sizeof(service_nvs_zen_t));
    service_nvs_give();
}

esp_err_t service_nvs_set_zen(const service_nvs_zen_t *params)
{
    if (params == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!service_nvs_take()) {
        return ESP_ERR_INVALID_STATE;
    }

    memcpy(&system_parameters.zen, params, sizeof(service_nvs_zen_t));
    s_dirty |= SERVICE_NVS_DIRTY_ZEN;

    service_nvs_give();
    return ESP_OK;
}
