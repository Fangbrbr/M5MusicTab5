/**
 * @file service_usb_device.c
 * @brief USB Device 服务实现
 *
 * 基于 TinyUSB 协议栈，实现 USB-C Device 端的两个功能：
 * 1. USB MIDI 设备：订阅 engine_midi 总线的非 SysEx 通道消息，转发到 USB Host。
 * 2. USB Mass Storage 设备：将 SD 卡暴露给 USB Host。
 *
 * 自定义 USB 配置描述符，同时包含 MIDI 与 MSC 接口。
 *
 * Why: 无 USB Device（MSC/MIDI  gadget）能力的板型由 CONFIG_BOARD_HAS_USB_DEVICE
 * 编译期门控，对外符号保留并无害降级（init 返回 NOT_SUPPORTED，deinit 空操作）。
 */

#include "service_usb_device.h"
#include "sdkconfig.h"
#include "esp_log.h"

#define TAG "usb_device"

#if CONFIG_BOARD_HAS_USB_DEVICE
#include "engine_midi.h"
#include "bsp/m5stack_tab5.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_msc.h"
#include "sdmmc_cmd.h"

/* 自定义字符串描述符索引 */
#define STRID_LANGUAGE     0
#define STRID_MANUFACTURER 1
#define STRID_PRODUCT      2
#define STRID_SERIAL       3
#define STRID_MIDI_ITF     4
#define STRID_MSC_ITF      5

static const char *s_str_desc[] = {
    (char[]){0x09, 0x04},     // 0: Language (English 0x0409)
    "M5Stack",                // 1: Manufacturer
    "TAB5 Music Pad",         // 2: Product
    "123456",                 // 3: Serial
    "TAB5 MIDI",              // 4: MIDI interface
    "TAB5 SD Card",           // 5: MSC interface
};

/* 接口与端点编号 */
enum {
    ITF_NUM_MIDI = 0,
    ITF_NUM_MIDI_STREAMING,
    ITF_NUM_MSC,
    ITF_NUM_TOTAL
};

enum {
    EPNUM_MIDI = 1,
    EPNUM_MSC  = 2,
};

#define USB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN + TUD_MSC_DESC_LEN)

/* 全速配置描述符 */
static const uint8_t s_fs_config_desc[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, USB_DESC_TOTAL_LEN, 0, 500),
    TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, STRID_MIDI_ITF, EPNUM_MIDI, (0x80 | EPNUM_MIDI), 64),
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, STRID_MSC_ITF, EPNUM_MSC, (0x80 | EPNUM_MSC), 64),
};

#if (TUD_OPT_HIGH_SPEED)
/* 高速配置描述符 */
static const uint8_t s_hs_config_desc[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, USB_DESC_TOTAL_LEN, 0, 500),
    TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, STRID_MIDI_ITF, EPNUM_MIDI, (0x80 | EPNUM_MIDI), 512),
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, STRID_MSC_ITF, EPNUM_MSC, (0x80 | EPNUM_MSC), 512),
};
#endif

static tinyusb_msc_storage_handle_t s_msc_storage = NULL;
static bool s_tinyusb_installed = false;

static uint8_t encode_midi_event(const engine_midi_event_t *evt, uint8_t *out)
{
    switch (evt->type) {
    case ENGINE_MIDI_MSG_NOTE_OFF:
        out[0] = (uint8_t)(ENGINE_MIDI_MSG_NOTE_OFF | (evt->channel & 0x0F));
        out[1] = evt->data1;
        out[2] = evt->data2;
        return 3;
    case ENGINE_MIDI_MSG_NOTE_ON:
        out[0] = (uint8_t)(ENGINE_MIDI_MSG_NOTE_ON | (evt->channel & 0x0F));
        out[1] = evt->data1;
        out[2] = evt->data2;
        return 3;
    case ENGINE_MIDI_MSG_POLY_PRESSURE:
        out[0] = (uint8_t)(ENGINE_MIDI_MSG_POLY_PRESSURE | (evt->channel & 0x0F));
        out[1] = evt->data1;
        out[2] = evt->data2;
        return 3;
    case ENGINE_MIDI_MSG_CONTROL_CHANGE:
        out[0] = (uint8_t)(ENGINE_MIDI_MSG_CONTROL_CHANGE | (evt->channel & 0x0F));
        out[1] = evt->data1;
        out[2] = evt->data2;
        return 3;
    case ENGINE_MIDI_MSG_PROGRAM_CHANGE:
        out[0] = (uint8_t)(ENGINE_MIDI_MSG_PROGRAM_CHANGE | (evt->channel & 0x0F));
        out[1] = evt->data1;
        return 2;
    case ENGINE_MIDI_MSG_CHANNEL_PRESSURE:
        out[0] = (uint8_t)(ENGINE_MIDI_MSG_CHANNEL_PRESSURE | (evt->channel & 0x0F));
        out[1] = evt->data1;
        return 2;
    case ENGINE_MIDI_MSG_PITCH_BEND:
        out[0] = (uint8_t)(ENGINE_MIDI_MSG_PITCH_BEND | (evt->channel & 0x0F));
        out[1] = evt->value & 0x7F;
        out[2] = (evt->value >> 7) & 0x7F;
        return 3;
    default:
        return 0;
    }
}

static void usb_device_midi_out_cb(const engine_midi_event_t *evt, void *user_data)
{
    (void)user_data;

    if (evt->source_port == ENGINE_MIDI_PORT_USB_DEVICE) {
        return;
    }

    uint8_t msg[4];
    uint8_t len = encode_midi_event(evt, msg);
    if (len > 0 && tud_midi_mounted()) {
        uint32_t written = tud_midi_stream_write(0, msg, len);
        if (written != len) {
            ESP_LOGD(TAG, "MIDI stream write short: %lu/%u", (unsigned long)written, len);
        }
    }
}

static void msc_event_cb(tinyusb_msc_storage_handle_t handle,
                         tinyusb_msc_event_t *event,
                         void *arg)
{
    (void)handle;
    (void)arg;

    const char *name = (event->mount_point == TINYUSB_MSC_STORAGE_MOUNT_USB) ? "USB" : "APP";
    switch (event->id) {
    case TINYUSB_MSC_EVENT_MOUNT_START:
        ESP_LOGI(TAG, "MSC mount to %s start", name);
        break;
    case TINYUSB_MSC_EVENT_MOUNT_COMPLETE:
        ESP_LOGI(TAG, "MSC mount to %s complete", name);
        break;
    case TINYUSB_MSC_EVENT_MOUNT_FAILED:
        ESP_LOGW(TAG, "MSC mount to %s failed", name);
        break;
    default:
        break;
    }
}

esp_err_t service_usb_device_init(void)
{
    if (s_tinyusb_installed) {
        return ESP_OK;
    }

    tinyusb_msc_driver_config_t msc_driver_cfg = {
        .callback = msc_event_cb,
        .callback_arg = NULL,
    };
    esp_err_t ret = tinyusb_msc_install_driver(&msc_driver_cfg);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "tinyusb_msc_install_driver failed: %s", esp_err_to_name(ret));
        return ret;
    }

    sdmmc_card_t *card = bsp_sdcard_get_handle();
    if (card != NULL) {
        tinyusb_msc_storage_config_t storage_cfg = {
            .medium.card = card,
            .mount_point = TINYUSB_MSC_STORAGE_MOUNT_USB,
            .fat_fs = {
                .base_path = NULL,
                .config = {
                    .max_files = 5,
                },
                .format_flags = 0,
            },
        };
        ret = tinyusb_msc_new_storage_sdmmc(&storage_cfg, &s_msc_storage);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "tinyusb_msc_new_storage_sdmmc failed: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGW(TAG, "No SD card handle, MSC disabled");
    }

    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    tusb_cfg.descriptor.string = s_str_desc;
    tusb_cfg.descriptor.string_count = sizeof(s_str_desc) / sizeof(s_str_desc[0]);
    tusb_cfg.descriptor.full_speed_config = s_fs_config_desc;
#if (TUD_OPT_HIGH_SPEED)
    tusb_cfg.descriptor.high_speed_config = s_hs_config_desc;
    tusb_cfg.descriptor.qualifier = NULL;
#endif

    ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "tinyusb_driver_install failed: %s", esp_err_to_name(ret));
        if (s_msc_storage != NULL) {
            tinyusb_msc_delete_storage(s_msc_storage);
            s_msc_storage = NULL;
        }
        tinyusb_msc_uninstall_driver();
        return ret;
    }
    s_tinyusb_installed = true;

    ret = engine_midi_subscribe(
        ENGINE_MIDI_MASK_NOTE_ON |
        ENGINE_MIDI_MASK_NOTE_OFF |
        ENGINE_MIDI_MASK_CONTROL_CHANGE |
        ENGINE_MIDI_MASK_PROGRAM_CHANGE |
        ENGINE_MIDI_MASK_PITCH_BEND |
        ENGINE_MIDI_MASK_CHANNEL_PRESSURE |
        ENGINE_MIDI_MASK_POLY_PRESSURE,
        0xFFFF,
        usb_device_midi_out_cb,
        NULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "engine_midi_subscribe failed: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "USB Device MIDI/MSC service initialized");
    return ESP_OK;
}

void service_usb_device_deinit(void)
{
    engine_midi_unsubscribe(usb_device_midi_out_cb);

    if (s_tinyusb_installed) {
        tinyusb_driver_uninstall();
        s_tinyusb_installed = false;
    }

    if (s_msc_storage != NULL) {
        tinyusb_msc_delete_storage(s_msc_storage);
        s_msc_storage = NULL;
    }

    tinyusb_msc_uninstall_driver();
}

#else /* !CONFIG_BOARD_HAS_USB_DEVICE */

/* 无 USB Device 硬件：对外符号无害降级 */
esp_err_t service_usb_device_init(void)
{
    ESP_LOGW(TAG, "board has no USB Device support, service disabled");
    return ESP_ERR_NOT_SUPPORTED;
}

void service_usb_device_deinit(void)
{
}

#endif /* CONFIG_BOARD_HAS_USB_DEVICE */
