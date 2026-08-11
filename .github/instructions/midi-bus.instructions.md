---
applyTo: ["components/engine_midi/**", "components/app_*/**", "components/service_sf2/**"]
description: "MIDI event bus usage patterns for inter-module communication"
---

# MIDI 事件总线使用规范

## 概述

`engine_midi` 实现发布/订阅事件总线，是模块间通信的核心机制。所有演奏类 App 的发声统一走总线，由 `engine_sf2` 订阅消费。

## 关键参数

| 参数 | 值 |
|:---|:---|
| 事件队列长度 | 128 (`ENGINE_MIDI_QUEUE_LEN`) |
| 最大消费者数 | 16 (`ENGINE_MIDI_MAX_CONSUMERS`) |
| SysEx 缓冲区 | 128 字节 |
| 来源端口 | `INTERNAL`(0)、`UART`(1)、`USB_HOST`(2)、`BLE`(3)、`USB_DEVICE`(4)、`APP`(5) |

## 发布事件

```c
// 发布音符开事件
engine_midi_publish_note_on(port, channel, note, velocity);

// 发布控制变更
engine_midi_publish_cc(port, channel, cc_num, value);

// 发布 SysEx
engine_midi_publish_sysex(port, data, length);
```

**重要**：App 侧发声必须使用 `ENGINE_MIDI_PORT_APP` 标记来源端口，防止回环。

## 订阅事件

```c
// 订阅 NOTE_ON / NOTE_OFF
engine_midi_subscribe(ENGINE_MIDI_EVENT_NOTE_ON, consumer_callback);
engine_midi_subscribe(ENGINE_MIDI_EVENT_NOTE_OFF, consumer_callback);

// 订阅 SysEx
engine_midi_subscribe(ENGINE_MIDI_EVENT_SYSEX, consumer_callback);
```

## 内部 SysEx 协议

4 字节帧格式：`[cmd][func][p1][p2]`，无 vendor id。

| cmd | 常量名 | 说明 |
|:---:|:---|:---|
| 1 | `MIDI_CMD_APP` | App 启动/返回 |
| 3 | `MIDI_CMD_INPUT` | 输入事件（func: 0=触摸，2=鼠标，3=键盘） |
| 7 | `MIDI_CMD_APP_CONTROL` | App 控制，转发给当前 App 的 `on_sysex()` |

## 防回环规则

- `app_manager` 收到 `MIDI_CMD_APP_CONTROL` 时，若来源端口为 `ENGINE_MIDI_PORT_APP` 则忽略
- 所有从 App 发出的消息必须标记 `ENGINE_MIDI_PORT_APP`

## 消费者回调

```c
static void my_consumer_callback(const engine_midi_event_t *event, void *user_data)
{
    switch (event->type) {
        case ENGINE_MIDI_EVENT_NOTE_ON: {
            uint8_t ch = event->channel;
            uint8_t note = event->data.note_on.note;
            uint8_t vel = event->data.note_on.velocity;
            // 处理音符开
            break;
        }
        // ...
    }
}
```
