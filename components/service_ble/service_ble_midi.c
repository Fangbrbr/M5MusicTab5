/**
 * @file service_ble_midi.c
 * @brief BLE MIDI peripheral：通过 ESP32-C6 + esp_hosted + NimBLE 发送 MIDI 事件
 *
 * Why: BLE 控制器挂在 C6 协处理器上，板型无 C6 时 CONFIG_ESP_HOSTED_ENABLED=n，
 * 整个 NimBLE 实现编译期剔除，对外符号保留并无害降级
 * （probe 返回 false、init 返回 NOT_SUPPORTED、状态查询返回空态）。
 */

#include "service_ble_midi.h"
#include "sdkconfig.h"
#include "esp_log.h"

static const char *TAG = "service_ble_midi";

#if CONFIG_ESP_HOSTED_ENABLED
#include "service_ble_midi_config.h"
#include "engine_midi.h"

#include "esp_timer.h"
#include "esp_hosted_misc.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/ble_gatt.h"
#include "host/ble_hs_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "os/os_mbuf.h"

#include "freertos/FreeRTOS.h"

#include <string.h>
#include <stdio.h>

/* MIDI BLE service / characteristic UUIDs (reversed byte order for NimBLE) */
static const ble_uuid128_t s_midi_svc_uuid = BLE_UUID128_INIT(
    0x00, 0xC7, 0xC4, 0x4E, 0xE3, 0x6C, 0x51, 0xA7,
    0x33, 0x4B, 0xE8, 0xED, 0x5A, 0x0E, 0xB8, 0x03);

static const ble_uuid128_t s_midi_chr_uuid = BLE_UUID128_INIT(
    0xF3, 0x6B, 0x10, 0x9D, 0x66, 0xF2, 0xA9, 0xA1,
    0x12, 0x41, 0x68, 0x38, 0xDB, 0xE5, 0x72, 0x77);

static uint16_t s_midi_chr_val_handle = 0;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool s_enabled = true;
static bool s_initialized = false;

static void ble_midi_host_task(void *param);
static void ble_midi_on_sync(void);
static void ble_midi_on_reset(int reason);
static int  ble_midi_gap_event(struct ble_gap_event *event, void *arg);
static int  ble_midi_access(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg);
static void ble_midi_advertise(void);
static void ble_midi_send_event(const engine_midi_event_t *evt);
static void ble_midi_consumer(const engine_midi_event_t *evt, void *user_data);

static const struct ble_gatt_svc_def s_gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_midi_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = &s_midi_chr_uuid.u,
                .access_cb = ble_midi_access,
                .arg = NULL,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .val_handle = &s_midi_chr_val_handle,
            },
            { 0 },
        },
    },
    { 0 },
};

static uint8_t ble_midi_now_ts(void)
{
    return (uint8_t)(0x80 | ((esp_timer_get_time() / 1000ULL) & 0x3F));
}

static void ble_midi_host_task(void *param)
{
    (void)param;
    ESP_LOGI(TAG, "nimble host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void ble_midi_on_sync(void)
{
    ESP_LOGI(TAG, "nimble synced");
    ble_midi_advertise();
}

static void ble_midi_on_reset(int reason)
{
    ESP_LOGW(TAG, "nimble reset: %d", reason);
}

static int ble_midi_gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "connected handle=%d", s_conn_handle);
        } else {
            ESP_LOGW(TAG, "connect failed status=%d", event->connect.status);
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            if (s_initialized && s_enabled) {
                ble_midi_advertise();
            }
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnected reason=%d", event->disconnect.reason);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        if (s_initialized && s_enabled) {
            ble_midi_advertise();
        }
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGW(TAG, "adv complete");
        if (s_initialized && s_enabled) {
            ble_midi_advertise();
        }
        break;

    default:
        break;
    }

    return 0;
}

static void ble_midi_advertise(void)
{
    struct ble_gap_adv_params adv_params = {0};
    struct ble_hs_adv_fields fields = {0};
    const char *name = ble_svc_gap_device_name();
    int rc;

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_set_fields failed: %d", rc);
        return;
    }

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_midi_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_start failed: %d", rc);
    } else {
        ESP_LOGI(TAG, "advertising as %s", name);
    }
}

static int ble_midi_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    static const uint8_t zero = 0;

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        return os_mbuf_append(ctxt->om, &zero, 1) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        /* 可扩展：将主机发来的 BLE MIDI 数据喂回 engine_midi */
        return 0;

    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}

static void ble_midi_send_event(const engine_midi_event_t *evt)
{
    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE || s_midi_chr_val_handle == 0) {
        return;
    }

    uint8_t buf[8];
    uint8_t len = 0;

    buf[len++] = ble_midi_now_ts();

    switch (evt->type) {
    case ENGINE_MIDI_MSG_NOTE_OFF:
        buf[len++] = (uint8_t)(0x80 | (evt->channel & 0x0F));
        buf[len++] = evt->data1 & 0x7F;
        buf[len++] = evt->data2 & 0x7F;
        break;

    case ENGINE_MIDI_MSG_NOTE_ON:
        buf[len++] = (uint8_t)(0x90 | (evt->channel & 0x0F));
        buf[len++] = evt->data1 & 0x7F;
        buf[len++] = evt->data2 & 0x7F;
        break;

    case ENGINE_MIDI_MSG_POLY_PRESSURE:
        buf[len++] = (uint8_t)(0xA0 | (evt->channel & 0x0F));
        buf[len++] = evt->data1 & 0x7F;
        buf[len++] = evt->data2 & 0x7F;
        break;

    case ENGINE_MIDI_MSG_CONTROL_CHANGE:
        buf[len++] = (uint8_t)(0xB0 | (evt->channel & 0x0F));
        buf[len++] = evt->data1 & 0x7F;
        buf[len++] = evt->data2 & 0x7F;
        break;

    case ENGINE_MIDI_MSG_PROGRAM_CHANGE:
        buf[len++] = (uint8_t)(0xC0 | (evt->channel & 0x0F));
        buf[len++] = evt->data1 & 0x7F;
        break;

    case ENGINE_MIDI_MSG_CHANNEL_PRESSURE:
        buf[len++] = (uint8_t)(0xD0 | (evt->channel & 0x0F));
        buf[len++] = evt->data1 & 0x7F;
        break;

    case ENGINE_MIDI_MSG_PITCH_BEND: {
        uint16_t bend = evt->value & 0x3FFF;
        buf[len++] = (uint8_t)(0xE0 | (evt->channel & 0x0F));
        buf[len++] = (uint8_t)(bend & 0x7F);
        buf[len++] = (uint8_t)((bend >> 7) & 0x7F);
        break;
    }

    default:
        return;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(buf, len);
    if (om == NULL) {
        ESP_LOGW(TAG, "mbuf alloc failed");
        return;
    }

    int rc = ble_gatts_notify_custom(s_conn_handle, s_midi_chr_val_handle, om);
    if (rc != 0) {
        ESP_LOGW(TAG, "notify failed: %d", rc);
    }
}

static void ble_midi_consumer(const engine_midi_event_t *evt, void *user_data)
{
    (void)user_data;

    if (!s_enabled) {
        return;
    }

    /* 不转发来自 BLE 的事件，避免回环 */
    if (evt->source_port == ENGINE_MIDI_PORT_BLE) {
        return;
    }

    ble_midi_send_event(evt);
}

bool service_ble_midi_probe_c6_support(void)
{
    esp_hosted_app_desc_t desc = {0};

    if (esp_hosted_get_coprocessor_app_desc(&desc) != ESP_OK) {
        return false;
    }

    unsigned major = 0;
    unsigned minor = 0;
    unsigned patch = 0;
    if (sscanf(desc.version, "%u.%u.%u", &major, &minor, &patch) < 1) {
        return false;
    }

    if (major == 0) {
        return false;
    }

    return true;
}

esp_err_t service_ble_midi_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "init");

    esp_err_t ret = esp_hosted_bt_controller_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bt controller init failed: %d", ret);
        return ret;
    }

    ret = esp_hosted_bt_controller_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bt controller enable failed: %d", ret);
        return ret;
    }

    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %d", ret);
        return ret;
    }

    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_gatts_count_cfg(s_gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "gatts_count_cfg failed: %d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(s_gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "gatts_add_svcs failed: %d", rc);
        return ESP_FAIL;
    }

    rc = ble_svc_gap_device_name_set(SERVICE_BLE_MIDI_DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGW(TAG, "set device name failed: %d", rc);
    }

    ble_hs_cfg.sync_cb = ble_midi_on_sync;
    ble_hs_cfg.reset_cb = ble_midi_on_reset;

    ret = engine_midi_subscribe(
        ENGINE_MIDI_MASK_NOTE_OFF |
        ENGINE_MIDI_MASK_NOTE_ON |
        ENGINE_MIDI_MASK_POLY_PRESSURE |
        ENGINE_MIDI_MASK_CONTROL_CHANGE |
        ENGINE_MIDI_MASK_PROGRAM_CHANGE |
        ENGINE_MIDI_MASK_CHANNEL_PRESSURE |
        ENGINE_MIDI_MASK_PITCH_BEND,
        0xFFFF,
        ble_midi_consumer,
        NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "midi subscribe failed: %d", ret);
        return ret;
    }

    nimble_port_freertos_init(ble_midi_host_task);

    s_initialized = true;
    ESP_LOGI(TAG, "init ok");
    return ESP_OK;
}

void service_ble_midi_deinit(void)
{
    if (!s_initialized) {
        return;
    }
    /* NimBLE 反初始化较复杂，当前仅停止广播 */
    ble_gap_adv_stop();
    s_initialized = false;
}

esp_err_t service_ble_midi_enable(bool enable)
{
    if (!s_initialized) {
        ESP_LOGW(TAG, "not initialized, ignore enable=%d", enable);
        return ESP_ERR_INVALID_STATE;
    }

    if (s_enabled == enable) {
        return ESP_OK;
    }
    s_enabled = enable;

    if (enable) {
        if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
            ble_midi_advertise();
        }
    } else {
        ble_gap_adv_stop();
    }

    return ESP_OK;
}

bool service_ble_midi_is_enabled(void)
{
    return s_enabled;
}

bool service_ble_midi_is_connected(void)
{
    return s_conn_handle != BLE_HS_CONN_HANDLE_NONE;
}

#else /* !CONFIG_ESP_HOSTED_ENABLED */

/* 无 esp_hosted（C6 协处理器）的板型：BLE MIDI 不可用，对外符号无害降级 */
bool service_ble_midi_probe_c6_support(void)
{
    return false;
}

esp_err_t service_ble_midi_init(void)
{
    ESP_LOGW(TAG, "esp_hosted disabled on this board, BLE MIDI not supported");
    return ESP_ERR_NOT_SUPPORTED;
}

void service_ble_midi_deinit(void)
{
}

esp_err_t service_ble_midi_enable(bool enable)
{
    (void)enable;
    return ESP_ERR_NOT_SUPPORTED;
}

bool service_ble_midi_is_enabled(void)
{
    return false;
}

bool service_ble_midi_is_connected(void)
{
    return false;
}

#endif /* CONFIG_ESP_HOSTED_ENABLED */
