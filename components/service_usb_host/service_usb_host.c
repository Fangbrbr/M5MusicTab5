/**
 * @file service_usb_host.c
 * @brief USB Host 服务实现
 *
 * 基于 BSP 的 USB Host 库，实现一个极简的 USB MIDI 类客户端驱动。
 * 当 USB-A 口接入 MIDI 键盘/控制器时，解析其配置描述符，找到
 * MIDI Streaming 接口的 Bulk IN 端点，持续提交 IN transfer，
 * 收到数据后通过 engine_midi 总线分发。
 *
 * Why: 无 USB-A Host 口的板型由 CONFIG_BOARD_HAS_USB_HOST 编译期门控，
 * 对外符号保留并无害降级（init 返回 NOT_SUPPORTED，查询类 API 返回空态）。
 */

#include "service_usb_host.h"
#include "sdkconfig.h"
#include "esp_log.h"

#define TAG "usb_host"

#if CONFIG_BOARD_HAS_USB_HOST
#include "engine_midi.h"
#include "bsp/m5stack_tab5.h"
#include "usb/usb_host.h"
#include "usb/usb_helpers.h"
#include "usb/usb_types_ch9.h"
#include "usb/usb_types_stack.h"
#include "usb/hid_host.h"
#include "usb/hid_usage_keyboard.h"
#include "usb/hid_usage_mouse.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdio.h"

#define USB_HOST_MIDI_IN_XFER_SIZE 64
#define USB_SUBCLASS_MIDI_STREAMING 0x03
#define USB_HOST_HID_MAX_DEVICES    4
#define USB_HOST_HID_INPUT_BUF_SIZE 64

/* 内部 RAM 门槛：USB host lib 任务栈 + HID 后台任务栈共约 8KB 必须落内部 RAM，
 * 不足时 bsp_usb_host_start 内部 assert 直接整机 abort；提前检查优雅降级 */
#define USB_HOST_MIN_INTERNAL_LARGEST 16384

static usb_host_client_handle_t s_client = NULL;
static usb_device_handle_t s_dev = NULL;
static usb_transfer_t *s_in_xfer = NULL;
static uint8_t s_ep_addr = 0;
static uint8_t s_intf_num = 0xFF;
static bool s_host_started = false;
static bool s_midi_connected = false;
static uint16_t s_vendor_id = 0;
static uint16_t s_product_id = 0;
static char s_vendor_str[32] = {0};

static hid_host_device_handle_t s_hid_devices[USB_HOST_HID_MAX_DEVICES] = {NULL};

static void usb_midi_in_xfer_cb(usb_transfer_t *xfer);
static void usb_host_client_event_cb(const usb_host_client_event_msg_t *event_msg, void *arg);
static void midi_device_open(uint8_t dev_addr);
static void midi_device_close(void);
static void usb_midi_feed_packet(const uint8_t pkt[4]);

static void usb_hid_driver_event_cb(hid_host_device_handle_t handle,
                                     const hid_host_driver_event_t event,
                                     void *arg);
static void usb_hid_interface_event_cb(hid_host_device_handle_t handle,
                                        const hid_host_interface_event_t event,
                                        void *arg);
static void usb_hid_publish_mouse(uint8_t buttons, int16_t dx, int16_t dy);
static void usb_hid_publish_keyboard(uint8_t modifier, const uint8_t keys[6]);
static esp_err_t usb_hid_init(void);

/**
 * @brief 将单个 USB-MIDI Event Packet 解码为标准 MIDI 字节并喂给 engine_midi
 *
 * USB-MIDI 包格式：
 *   Byte 0: [CN:4][CIN:4]
 *   Byte 1: MIDI byte 1
 *   Byte 2: MIDI byte 2
 *   Byte 3: MIDI byte 3
 *
 * 根据 CIN 提取有效字节，按标准 MIDI 1.0 字节流喂入解析器。
 */
static void usb_midi_feed_packet(const uint8_t pkt[4])
{
    uint8_t cin = pkt[0] & 0x0F;
    uint8_t bytes[3];
    uint8_t len = 0;

    switch (cin) {
    case 0x2: /* 2-byte System Common */
        len = 2;
        bytes[0] = pkt[1];
        bytes[1] = pkt[2];
        break;
    case 0x3: /* 3-byte System Common */
    case 0x4: /* SysEx 开始或继续 */
    case 0x7: /* SysEx 结束（3 字节） */
    case 0x8: /* Note Off */
    case 0x9: /* Note On */
    case 0xA: /* Poly Key Pressure */
    case 0xB: /* Control Change */
    case 0xE: /* Pitch Bend */
        len = 3;
        bytes[0] = pkt[1];
        bytes[1] = pkt[2];
        bytes[2] = pkt[3];
        break;
    case 0x5: /* SysEx 结束（1 字节） */
    case 0xF: /* Single byte */
        len = 1;
        bytes[0] = pkt[1];
        break;
    case 0x6: /* SysEx 结束（2 字节） */
    case 0xC: /* Program Change */
    case 0xD: /* Channel Pressure */
        len = 2;
        bytes[0] = pkt[1];
        bytes[1] = pkt[2];
        break;
    case 0x0: /* 保留/杂项 */
    default:
        break;
    }

    for (uint8_t i = 0; i < len; i++) {
        engine_midi_feed_byte_from_port(bytes[i], ENGINE_MIDI_PORT_USB_HOST);
    }
}

static void usb_midi_in_xfer_cb(usb_transfer_t *xfer)
{
    if (xfer->status == USB_TRANSFER_STATUS_COMPLETED && xfer->actual_num_bytes > 0) {
        for (uint32_t i = 0; i + 3 < xfer->actual_num_bytes; i += 4) {
            usb_midi_feed_packet(&xfer->data_buffer[i]);
        }
    }

    if (s_dev != NULL && s_in_xfer != NULL) {
        xfer->num_bytes = USB_HOST_MIDI_IN_XFER_SIZE;
        esp_err_t ret = usb_host_transfer_submit(xfer);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "IN transfer resubmit failed: %s", esp_err_to_name(ret));
        }
    }
}

static void midi_device_close(void)
{
    if (s_in_xfer != NULL) {
        usb_host_transfer_free(s_in_xfer);
        s_in_xfer = NULL;
    }

    if (s_dev != NULL) {
        if (s_intf_num != 0xFF) {
            usb_host_interface_release(s_client, s_dev, s_intf_num);
        }
        usb_host_device_close(s_client, s_dev);
        s_dev = NULL;
    }

    s_ep_addr = 0;
    s_intf_num = 0xFF;
    s_midi_connected = false;
    s_vendor_id = 0;
    s_product_id = 0;
    s_vendor_str[0] = '\0';
}

static void midi_device_open(uint8_t dev_addr)
{
    esp_err_t ret = usb_host_device_open(s_client, dev_addr, &s_dev);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "device open failed: %s", esp_err_to_name(ret));
        return;
    }

    const usb_device_desc_t *dev_desc = NULL;
    ret = usb_host_get_device_descriptor(s_dev, &dev_desc);
    if (ret == ESP_OK && dev_desc != NULL) {
        s_vendor_id = dev_desc->idVendor;
        s_product_id = dev_desc->idProduct;
        snprintf(s_vendor_str, sizeof(s_vendor_str),
                 "VID:0x%04X PID:0x%04X", s_vendor_id, s_product_id);
        ESP_LOGI(TAG, "USB device VID=0x%04X PID=0x%04X", s_vendor_id, s_product_id);
    } else {
        s_vendor_id = 0;
        s_product_id = 0;
        s_vendor_str[0] = '\0';
    }

    const usb_config_desc_t *cfg_desc = NULL;
    ret = usb_host_get_active_config_descriptor(s_dev, &cfg_desc);
    if (ret != ESP_OK || cfg_desc == NULL) {
        ESP_LOGW(TAG, "get config desc failed: %s", esp_err_to_name(ret));
        midi_device_close();
        return;
    }

    uint16_t total_len = cfg_desc->wTotalLength;
    int offset = 0;
    const usb_standard_desc_t *desc = (const usb_standard_desc_t *)cfg_desc;
    uint8_t midi_intf = 0xFF;
    uint8_t ep_in = 0;

    while (desc != NULL) {
        if (desc->bDescriptorType == USB_B_DESCRIPTOR_TYPE_INTERFACE) {
            const usb_intf_desc_t *intf = (const usb_intf_desc_t *)desc;
            if (intf->bInterfaceClass == USB_CLASS_AUDIO &&
                intf->bInterfaceSubClass == USB_SUBCLASS_MIDI_STREAMING) {
                midi_intf = intf->bInterfaceNumber;
                ESP_LOGI(TAG, "Found MIDI Streaming interface %u", midi_intf);
            }
        } else if (desc->bDescriptorType == USB_B_DESCRIPTOR_TYPE_ENDPOINT && midi_intf != 0xFF) {
            const usb_ep_desc_t *ep = (const usb_ep_desc_t *)desc;
            if (USB_EP_DESC_GET_XFERTYPE(ep) == USB_TRANSFER_TYPE_BULK &&
                USB_EP_DESC_GET_EP_DIR(ep) == 1) {
                ep_in = ep->bEndpointAddress;
                break;
            }
        }
        desc = usb_parse_next_descriptor(desc, total_len, &offset);
    }

    if (ep_in == 0) {
        ESP_LOGW(TAG, "No MIDI IN endpoint found");
        midi_device_close();
        return;
    }

    ret = usb_host_interface_claim(s_client, s_dev, midi_intf, 0);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "interface claim failed: %s", esp_err_to_name(ret));
        midi_device_close();
        return;
    }
    s_intf_num = midi_intf;

    ret = usb_host_transfer_alloc(USB_HOST_MIDI_IN_XFER_SIZE, 0, &s_in_xfer);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "transfer alloc failed: %s", esp_err_to_name(ret));
        midi_device_close();
        return;
    }

    s_ep_addr = ep_in;
    s_in_xfer->device_handle = s_dev;
    s_in_xfer->bEndpointAddress = s_ep_addr;
    s_in_xfer->callback = usb_midi_in_xfer_cb;
    s_in_xfer->context = NULL;
    s_in_xfer->num_bytes = USB_HOST_MIDI_IN_XFER_SIZE;

    ret = usb_host_transfer_submit(s_in_xfer);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "transfer submit failed: %s", esp_err_to_name(ret));
        midi_device_close();
        return;
    }

    s_midi_connected = true;
    ESP_LOGI(TAG, "USB MIDI device ready, IN endpoint 0x%02X", ep_in);
}

static void usb_host_client_event_cb(const usb_host_client_event_msg_t *event_msg, void *arg)
{
    (void)arg;

    switch (event_msg->event) {
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
        ESP_LOGI(TAG, "USB device connected, addr=%d", event_msg->new_dev.address);
        midi_device_open(event_msg->new_dev.address);
        break;
    case USB_HOST_CLIENT_EVENT_DEV_GONE:
        ESP_LOGI(TAG, "USB device disconnected");
        midi_device_close();
        break;
    default:
        break;
    }
}

esp_err_t service_usb_host_init(void)
{
    if (s_host_started) {
        return ESP_OK;
    }

    size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    if (largest < USB_HOST_MIN_INTERNAL_LARGEST) {
        ESP_LOGW(TAG, "internal RAM low (largest block %u), skip USB Host",
                 (unsigned)largest);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = bsp_usb_host_start(BSP_USB_HOST_POWER_MODE_USB_DEV, true);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "bsp_usb_host_start failed: %s", esp_err_to_name(ret));
        return ret;
    }
    s_host_started = true;

    const usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = {
            .client_event_callback = usb_host_client_event_cb,
            .callback_arg = NULL,
        },
    };
    ret = usb_host_client_register(&client_config, &s_client);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "usb_host_client_register failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = usb_hid_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "usb_hid_init failed: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "USB Host MIDI service initialized");
    return ESP_OK;
}

void service_usb_host_deinit(void)
{
    for (int i = 0; i < USB_HOST_HID_MAX_DEVICES; i++) {
        if (s_hid_devices[i] != NULL) {
            hid_host_device_close(s_hid_devices[i]);
            s_hid_devices[i] = NULL;
        }
    }
    hid_host_uninstall();

    midi_device_close();

    if (s_client != NULL) {
        usb_host_client_deregister(s_client);
        s_client = NULL;
    }

    if (s_host_started) {
        bsp_usb_host_stop();
        s_host_started = false;
    }
}

void service_usb_host_process(void)
{
    if (s_client == NULL) {
        return;
    }

    /* 单次任务周期内最多处理 4 个事件，避免长时间占用 CPU0 导致 IDLE 饿死 */
    for (int i = 0; i < 4; i++) {
        if (usb_host_client_handle_events(s_client, 0) != ESP_OK) {
            break;
        }
    }
}

bool service_usb_host_is_started(void)
{
    return s_host_started;
}

bool service_usb_host_midi_connected(void)
{
    return s_midi_connected;
}

const char *service_usb_host_midi_vendor(void)
{
    return s_vendor_str;
}

void service_usb_host_get_vid_pid(uint16_t *vid, uint16_t *pid)
{
    if (vid != NULL) {
        *vid = s_vendor_id;
    }
    if (pid != NULL) {
        *pid = s_product_id;
    }
}

/* ==================== USB Host HID (Mouse / Keyboard) ==================== */

static void usb_hid_publish_mouse(uint8_t buttons, int16_t dx, int16_t dy)
{
    engine_midi_event_t evt = {0};
    evt.type = ENGINE_MIDI_MSG_SYSEX;
    evt.sysex_len = 7;
    evt.sysex_data[0] = 3; /* MIDI_CMD_INPUT */
    evt.sysex_data[1] = 2; /* MIDI_FUNC_INPUT_MOUSE */
    evt.sysex_data[2] = buttons;
    evt.sysex_data[3] = (uint8_t)(dx >> 8);
    evt.sysex_data[4] = (uint8_t)(dx & 0xFF);
    evt.sysex_data[5] = (uint8_t)(dy >> 8);
    evt.sysex_data[6] = (uint8_t)(dy & 0xFF);
    evt.source_port = ENGINE_MIDI_PORT_USB_HOST;
    engine_midi_publish(&evt, 0);
}

static void usb_hid_publish_keyboard(uint8_t modifier, const uint8_t keys[6])
{
    engine_midi_event_t evt = {0};
    evt.type = ENGINE_MIDI_MSG_SYSEX;
    evt.sysex_len = 9;
    evt.sysex_data[0] = 3; /* MIDI_CMD_INPUT */
    evt.sysex_data[1] = 3; /* MIDI_FUNC_INPUT_KEYBOARD */
    evt.sysex_data[2] = modifier;
    for (int i = 0; i < 6; i++) {
        evt.sysex_data[3 + i] = keys[i];
    }
    evt.source_port = ENGINE_MIDI_PORT_USB_HOST;
    engine_midi_publish(&evt, 0);
}

static void usb_hid_interface_event_cb(hid_host_device_handle_t handle,
                                       const hid_host_interface_event_t event,
                                       void *arg)
{
    (void)arg;

    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT: {
        uint8_t data[USB_HOST_HID_INPUT_BUF_SIZE];
        size_t data_length = 0;
        esp_err_t ret = hid_host_device_get_raw_input_report_data(handle,
                                                                  data,
                                                                  sizeof(data),
                                                                  &data_length);
        if (ret != ESP_OK || data_length == 0) {
            return;
        }

        hid_host_dev_params_t dev_params;
        ret = hid_host_device_get_params(handle, &dev_params);
        if (ret != ESP_OK) {
            return;
        }

        if (dev_params.sub_class != HID_SUBCLASS_BOOT_INTERFACE) {
            return;
        }

        if (dev_params.proto == HID_PROTOCOL_MOUSE &&
            data_length >= sizeof(hid_mouse_input_report_boot_t)) {
            hid_mouse_input_report_boot_t *report =
                (hid_mouse_input_report_boot_t *)data;
            usb_hid_publish_mouse(report->buttons.val,
                                  (int16_t)report->x_displacement,
                                  (int16_t)report->y_displacement);
        } else if (dev_params.proto == HID_PROTOCOL_KEYBOARD &&
                   data_length >= sizeof(hid_keyboard_input_report_boot_t)) {
            hid_keyboard_input_report_boot_t *report =
                (hid_keyboard_input_report_boot_t *)data;
            usb_hid_publish_keyboard(report->modifier.val, report->key);
        }
        break;
    }
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        hid_host_device_close(handle);
        for (int i = 0; i < USB_HOST_HID_MAX_DEVICES; i++) {
            if (s_hid_devices[i] == handle) {
                s_hid_devices[i] = NULL;
            }
        }
        ESP_LOGI(TAG, "HID device disconnected");
        break;
    default:
        break;
    }
}

static void usb_hid_driver_event_cb(hid_host_device_handle_t handle,
                                    const hid_host_driver_event_t event,
                                    void *arg)
{
    (void)arg;

    if (event != HID_HOST_DRIVER_EVENT_CONNECTED) {
        return;
    }

    for (int i = 0; i < USB_HOST_HID_MAX_DEVICES; i++) {
        if (s_hid_devices[i] != NULL) {
            continue;
        }

        const hid_host_device_config_t dev_config = {
            .callback = usb_hid_interface_event_cb,
            .callback_arg = NULL,
        };
        esp_err_t ret = hid_host_device_open(handle, &dev_config);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "hid_host_device_open failed: %s", esp_err_to_name(ret));
            return;
        }

        s_hid_devices[i] = handle;

        ret = hid_host_device_start(handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "hid_host_device_start failed: %s", esp_err_to_name(ret));
            hid_host_device_close(handle);
            s_hid_devices[i] = NULL;
            return;
        }

        hid_host_dev_params_t params;
        if (hid_host_device_get_params(handle, &params) == ESP_OK) {
            const char *proto_name = "unknown";
            if (params.proto == HID_PROTOCOL_MOUSE) {
                proto_name = "mouse";
            } else if (params.proto == HID_PROTOCOL_KEYBOARD) {
                proto_name = "keyboard";
            }
            ESP_LOGI(TAG, "HID %s connected", proto_name);
        }
        return;
    }

    ESP_LOGW(TAG, "too many HID devices");
}

static esp_err_t usb_hid_init(void)
{
    const hid_host_driver_config_t hid_config = {
        .create_background_task = true,
        .task_priority = 8,
        .stack_size = 4096,
        .core_id = tskNO_AFFINITY,
        .callback = usb_hid_driver_event_cb,
        .callback_arg = NULL,
    };
    return hid_host_install(&hid_config);
}

#else /* !CONFIG_BOARD_HAS_USB_HOST */

/* 无 USB Host 硬件：对外符号无害降级 */
esp_err_t service_usb_host_init(void)
{
    ESP_LOGW(TAG, "board has no USB Host port, service disabled");
    return ESP_ERR_NOT_SUPPORTED;
}

void service_usb_host_deinit(void)
{
}

void service_usb_host_process(void)
{
}

bool service_usb_host_is_started(void)
{
    return false;
}

bool service_usb_host_midi_connected(void)
{
    return false;
}

const char *service_usb_host_midi_vendor(void)
{
    return "";
}

void service_usb_host_get_vid_pid(uint16_t *vid, uint16_t *pid)
{
    if (vid != NULL) {
        *vid = 0;
    }
    if (pid != NULL) {
        *pid = 0;
    }
}

#endif /* CONFIG_BOARD_HAS_USB_HOST */
