# USB Host MIDI 键盘接入原理与实现指南

> 本文档说明 TAB5_MUSIC_PAD 如何通过 ESP-IDF USB Host 接口接入标准 USB MIDI 键盘，并将键盘事件转换为合成器可识别的 MIDI 消息。

---

## 1. 整体架构

```
USB MIDI Keyboard
       │
       │ USB Cable
       ▼
┌──────────────┐     ┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│ ESP-IDF      │     │ UsbHostMidi  │     │ MidiParser   │     │ Synth/App    │
│ USB Host     │────►│ Class Driver │────►│ State Machine│────►│ noteOn/Off/… │
│ Library      │     │              │     │              │     │              │
└──────────────┘     └──────────────┘     └──────────────┘     └──────────────┘
```

三个关键层级：

1. **ESP-IDF USB Host Library**：底层 USB 枚举、传输、事件分发
2. **UsbHostMidi 类驱动**：识别 USB MIDI 设备，建立 Bulk IN 传输，解析 USB-MIDI 包
3. **MidiParser**：把标准 MIDI 字节流解析为回调事件（Note On/Off 等）

---

## 2. USB MIDI 设备的类别识别

USB MIDI 键盘在 USB 描述符中不是单一 class，而是属于 **USB Audio Class (0x01)** 下的 **MIDI Streaming Subclass (0x03)**。

关键字段：

| 字段 | 值 | 含义 |
|------|-----|------|
| bInterfaceClass | 0x01 | Audio |
| bInterfaceSubClass | 0x03 | MIDI Streaming |
| bInterfaceProtocol | 0x00 | 无特定协议 |

代码中的识别逻辑：

```cpp
#define USB_AUDIO_CLASS         0x01
#define USB_MIDI_STREAMING_SUB  0x03

if (intf_class == USB_AUDIO_CLASS && intf_subclass == USB_MIDI_STREAMING_SUB) {
    // 找到 MIDI Streaming 接口
}
```

> 注意：很多键盘还有一个 **Audio Control (0x01)** 接口，它不是 MIDI Streaming，需要跳过。

---

## 3. 配置描述符解析

USB Host 拿到 `config descriptor` 后，必须手动遍历其中的 interface 和 endpoint 描述符。

### 3.1 遍历规则

```
Configuration Descriptor
├── Interface Descriptor (Audio Control)      ← 跳过
├── Interface Descriptor (MIDI Streaming)     ← 目标
│   └── Endpoint Descriptor (Bulk IN)         ← 读取端点地址和 MPS
```

解析代码逻辑：

```cpp
const uint8_t* p = config_desc_raw + config_desc_raw[0];  // 跳过 configuration desc
while (p < end) {
    uint8_t len = p[0];
    uint8_t type = p[1];

    if (type == USB_DESC_TYPE_INTERFACE && len >= 9) {
        uint8_t intf_class = p[5];
        uint8_t intf_subclass = p[6];
        if (intf_class == USB_AUDIO_CLASS && intf_subclass == USB_MIDI_STREAMING_SUB) {
            midi_interface = p[2];  // bInterfaceNumber
            found_midi_intf = true;
        }
    } else if (type == USB_DESC_TYPE_ENDPOINT && found_midi_intf) {
        uint8_t ep_addr = p[2];
        uint8_t ep_attr = p[3];
        // Bulk IN: 方向 IN (bit7=1) + 类型 Bulk (attr & 0x03 == 0x02)
        if ((ep_addr & 0x80) && ((ep_attr & 0x03) == 0x02)) {
            bulk_in_ep = ep_addr;
            ep_max_packet_size = p[4] | (p[5] << 8);
        }
    }
    p += len;
}
```

### 3.2 常见端点

大多数 USB MIDI 键盘使用：
- **Bulk IN**：从键盘到 ESP（按键事件）
- **Bulk OUT**：可选，用于向键盘发送数据（本项目暂不实现）

只需要 **Bulk IN** 即可演奏。

---

## 4. USB Host 库初始化

### 4.1 安装 USB Host Library

```cpp
usb_host_config_t host_config = {
    .skip_phy_setup = false,
    .root_port_unpowered = false,
    .intr_flags = ESP_INTR_FLAG_LEVEL1,
};

esp_err_t err = usb_host_install(&host_config);
```

### 4.2 创建 Library Event Task

USB Host Library 需要一个专用任务来处理底层事件（设备连接、断开、传输完成等）：

```cpp
xTaskCreatePinnedToCore(usb_host_lib_task, "usb_host_lib", 4096, nullptr, 10, &lib_task_hdl, 0);
```

```cpp
void usb_host_lib_task(void* arg) {
    while (true) {
        uint32_t event_flags = 0;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
    }
}
```

### 4.3 注册 Client

```cpp
usb_host_client_config_t client_config = {};
client_config.is_synchronous = false;
client_config.max_num_event_msg = 5;
client_config.async.client_event_callback = client_event_cb_static;
client_config.async.callback_arg = this;

usb_host_client_register(&client_config, &client_hdl);
```

Client 会收到两类事件：
- `USB_HOST_CLIENT_EVENT_NEW_DEV`：新设备连接
- `USB_HOST_CLIENT_EVENT_DEV_GONE`：设备断开

---

## 5. 设备打开与接口声明

### 5.1 打开设备

收到 `NEW_DEV` 事件后：

```cpp
usb_host_device_open(client_hdl, dev_addr, &dev_hdl);
usb_host_get_device_descriptor(dev_hdl, &dev_desc);
usb_host_get_active_config_descriptor(dev_hdl, &config_desc);
```

### 5.2 声明接口

找到 MIDI Streaming 接口和 Bulk IN 端点后：

```cpp
usb_host_interface_claim(client_hdl, dev_hdl, midi_interface, 0);
```

然后启动 Bulk IN 传输。

---

## 6. Bulk IN 传输与数据接收

### 6.1 分配传输对象

```cpp
size_t buf_size = ep_max_packet_size * 4;  // 64 * 4 = 256 bytes
if (buf_size < 64) buf_size = 64;

usb_host_transfer_alloc(buf_size, 0, &in_xfer);

in_xfer->device_handle = dev_hdl;
in_xfer->bEndpointAddress = bulk_in_ep;
in_xfer->callback = transfer_cb_static;
in_xfer->context = this;
in_xfer->num_bytes = buf_size;
```

### 6.2 提交传输

```cpp
usb_host_transfer_submit(in_xfer);
```

这是一个异步传输。当键盘发送数据时，ESP-IDF 会调用 `transfer_cb_static`。

### 6.3 回调处理

```cpp
void handleTransferComplete(usb_transfer_t* transfer) {
    if (transfer->status == USB_TRANSFER_STATUS_COMPLETED) {
        uint8_t* data = transfer->data_buffer;
        int len = transfer->actual_num_bytes;

        // 每 4 字节为一个 USB-MIDI Event Packet
        for (int i = 0; i + 3 < len; i += 4) {
            parseUsbMidiPacket(&data[i]);
        }

        // 处理完后必须重新提交，才能继续接收
        transfer->num_bytes = transfer->data_buffer_size;
        usb_host_transfer_submit(transfer);
    }
}
```

> **关键**：每次传输完成后必须 `usb_host_transfer_submit()`，否则只会收到一次数据。

---

## 7. USB-MIDI Event Packet 解析

### 7.1 包格式

USB-MIDI 协议规定，每个 MIDI 事件封装为 4 字节：

```
Byte 0: [CN:4][CIN:4]
Byte 1: MIDI byte 1
Byte 2: MIDI byte 2
Byte 3: MIDI byte 3
```

- **CN (Cable Number)**：多 cable 设备使用，单键盘通常为 0
- **CIN (Code Index Number)**：标识 MIDI 事件类型和长度

### 7.2 CIN 对照表

| CIN | 含义 | 有效字节 |
|-----|------|---------|
| 0x0 | 保留/杂项 | - |
| 0x2 | 2-byte System Common | pkt[1], pkt[2] |
| 0x3 | 3-byte System Common | pkt[1], pkt[2], pkt[3] |
| 0x4 | SysEx 开始或继续 | pkt[1], pkt[2], pkt[3] |
| 0x5 | SysEx 结束（1 字节） | pkt[1] |
| 0x6 | SysEx 结束（2 字节） | pkt[1], pkt[2] |
| 0x7 | SysEx 结束（3 字节） | pkt[1], pkt[2], pkt[3] |
| 0x8 | Note Off | pkt[1], pkt[2], pkt[3] |
| 0x9 | Note On | pkt[1], pkt[2], pkt[3] |
| 0xA | Poly Key Pressure | pkt[1], pkt[2], pkt[3] |
| 0xB | Control Change | pkt[1], pkt[2], pkt[3] |
| 0xC | Program Change | pkt[1], pkt[2] |
| 0xD | Channel Pressure | pkt[1], pkt[2] |
| 0xE | Pitch Bend | pkt[1], pkt[2], pkt[3] |
| 0xF | Single byte | pkt[1] |

### 7.3 解析代码

```cpp
void parseUsbMidiPacket(const uint8_t* pkt) {
    uint8_t cin = pkt[0] & 0x0F;

    switch (cin) {
        case 0x9:  // Note On
            parser.feed(pkt[1]);  // status
            parser.feed(pkt[2]);  // note
            parser.feed(pkt[3]);  // velocity
            break;
        case 0x8:  // Note Off
            parser.feed(pkt[1]);
            parser.feed(pkt[2]);
            parser.feed(pkt[3]);
            break;
        // ... 其他 case
    }
}
```

> 本项目把提取出的标准 MIDI 字节喂给 `MidiParser`，由它维护状态机。

---

## 8. MidiParser 状态机

### 8.1 状态

```cpp
enum State {
    STATE_IDLE,
    STATE_WAIT_DATA1,
    STATE_WAIT_DATA2,
    STATE_SYSEX
};
```

### 8.2 处理流程

1. 收到状态字节（`0x80~0xEF`）：保存 `status`，进入 `WAIT_DATA1`
2. 收到数据字节 1：保存到 `data1`
   - 如果是 Program Change (0xC) 或 Channel Pressure (0xD)，直接触发回调
   - 否则进入 `WAIT_DATA2`
3. 收到数据字节 2：触发回调
4. 支持 Running Status：如果在 IDLE 状态收到数据字节，复用上一次的 `status`

### 8.3 Note On 特殊处理

MIDI 标准中，Note On velocity = 0 等价于 Note Off：

```cpp
case 0x09:  // Note On
    if (d2 == 0) {
        onNoteOff(channel, d1, 0);
    } else {
        onNoteOn(channel, d1, d2);
    }
    break;
```

### 8.4 回调注册

```cpp
MidiParser& parser = usbHostMidi.getParser();
parser.setNoteOnCallback([](uint8_t ch, uint8_t note, uint8_t vel) {
    synth->noteOn(ch - 1, note, vel);  // 外部通道 1-16 → 内部 0-15
});
parser.setNoteOffCallback([](uint8_t ch, uint8_t note, uint8_t vel) {
    synth->noteOff(ch - 1, note);
});
```

---

## 9. 与 App/Synth 的对接

在 `ServiceInput` 中，USB Host MIDI 被封装为输入源之一：

```cpp
class ServiceInput {
public:
    bool init();
    void poll();
    MidiParser& getParser();

private:
    MidiParser parser;
    UsbHostMidi usbHostMidi;
};
```

`poll()` 在 System Task 中每 1ms 调用一次：

```cpp
void ServiceInput::poll() {
    usbHostMidi.process();
}
```

`process()` 调用 `usb_host_client_handle_events()` 处理客户端事件，包括新设备连接、传输完成等。

---

## 10. 关键配置

### 10.1 选择 MIDI 输入源

在 `service_input/midi_config.h` 中：

```cpp
#define USE_USB_HOST_MIDI     3
#define MIDI_IN_DEV           USE_USB_HOST_MIDI
```

### 10.2 sdkconfig 要求

USB Host 需要 ESP-IDF 的 USB 组件支持。确保：

```
CONFIG_USB_OTG_SUPPORTED=y
```

并依赖 `espressif__usb` 组件。

### 10.3 GPIO

ESP32-P4 的 USB 使用固定 GPIO：
- D-：GPIO 19
- D+：GPIO 20

由 BSP 自动配置，无需手动设置。

---

## 11. 常见问题与调试

### 11.1 键盘插入无反应

| 可能原因 | 排查方法 |
|---------|---------|
| USB Host Library 未安装 | 检查 `usb_host_install()` 返回值 |
| Library Task 未创建 | 确认 `usb_host_lib_task` 在运行 |
| Client 未注册 | 确认 `usb_host_client_register()` 成功 |
| 描述符解析失败 | 打印 `VID/PID` 和 interface class/subclass |
| 不是 MIDI Streaming 设备 | 检查 interface subclass 是否为 0x03 |

### 11.2 收到一次数据后不再接收

原因：传输完成后没有重新 `submit`。必须在回调中重新提交：

```cpp
usb_host_transfer_submit(transfer);
```

### 11.3 按键错乱或漏键

原因：
1. **Running Status 处理错误**：同一个状态字节下的连续 Note On/Off 不应重复发送状态字节
2. **CIN 解析错误**：SysEx 和普通通道消息混淆
3. **缓冲区太小**：`buf_size` 应至少为 `MPS * 4`

### 11.4 延迟高

原因：
1. `process()` 调用频率太低（应 1ms 一次）
2. Bulk IN 传输缓冲区太大，键盘需要攒够数据才发送
3. 音频任务优先级不够，MIDI 事件无法及时处理

### 11.5 断开后重连失败

原因：
1. 没有正确处理 `DEV_GONE` 事件释放资源
2. `device_connected` 状态未清空
3. 接口未 `usb_host_interface_release()`

正确关闭顺序：
```cpp
stopBulkInTransfer();
usb_host_interface_release(client_hdl, dev_hdl, midi_interface);
usb_host_device_close(client_hdl, dev_hdl);
```

---

## 12. 日志输出参考

正常连接时应看到：

```
I UsbHostMidi: USB Host MIDI initialized
I UsbHostMidi: New USB device connected, addr=1
I UsbHostMidi: Device: VID=0xXXXX PID=0xYYYY
I UsbHostMidi: Found MIDI Streaming interface: 0
I UsbHostMidi: Found Bulk IN endpoint: 0x81, MPS=64
I UsbHostMidi: Bulk IN transfer started, buf=256 bytes
I UsbHostMidi: USB MIDI device ready
```

---

## 13. 总结

USB Host MIDI 接入的核心流程：

1. **初始化 USB Host Library**：`usb_host_install()` + Library Task
2. **注册 Client**：接收设备连接/断开事件
3. **打开设备**：获取设备描述符和配置描述符
4. **解析描述符**：找到 Audio Class (0x01) + MIDI Streaming (0x03) + Bulk IN 端点
5. **声明接口**：`usb_host_interface_claim()`
6. **启动 Bulk IN 传输**：`usb_host_transfer_alloc()` + `submit()`
7. **解析 USB-MIDI 包**：4 字节一组，提取标准 MIDI 字节
8. **喂给 MidiParser**：触发 Note On/Off 等回调
9. **回调到 Synth**：调用 `synth->noteOn(ch-1, note, vel)`

只要描述符解析正确、传输能持续 re-submit、MIDI 解析器状态机正确，USB MIDI 键盘就能稳定演奏。
