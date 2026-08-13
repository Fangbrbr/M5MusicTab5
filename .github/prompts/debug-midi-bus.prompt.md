---
description: "Debug MIDI event bus issues: events not firing, wrong routing, or synthesis problems"
---

# 调试 MIDI 事件总线

## 诊断流程

### 1. 确认事件是否发布

在发布点添加日志：

```c
ESP_LOGI(TAG, "Publishing NOTE_ON: ch=%d note=%d vel=%d port=%d",
         channel, note, velocity, ENGINE_MIDI_PORT_APP);
engine_midi_publish_note_on(ENGINE_MIDI_PORT_APP, channel, note, velocity);
```

### 2. 确认消费者是否注册

检查订阅代码：

```c
esp_err_t ret = engine_midi_subscribe(ENGINE_MIDI_EVENT_NOTE_ON, my_callback);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to subscribe: %s", esp_err_to_name(ret));
}
```

### 3. 检查来源端口

- 回环问题：确认 `ENGINE_MIDI_PORT_APP` 标记正确
- `app_manager` 会忽略来源端口为 `ENGINE_MIDI_PORT_APP` 的 `MIDI_CMD_APP_CONTROL`

### 4. 检查队列溢出

```c
engine_midi_stats_t stats;
engine_midi_get_stats(&stats);
ESP_LOGI(TAG, "Queue: used=%d/%d dropped=%d",
         stats.queue_used, stats.queue_size, stats.events_dropped);
```

### 5. 检查 SF2 引擎路由

- CC0/CC32 选择 bank
- PC 选择 program
- ch10（内部索引 9）自动路由鼓组（bank 128）

## 常见问题

| 现象 | 可能原因 | 排查方向 |
|:---|:---|:---|
| 无声音 | 事件未发布 | 检查发布代码与端口 |
| 无声音 | 消费者未注册 | 检查 `engine_midi_subscribe` 返回值 |
| 声音错乱 | bank/program 错误 | 检查 CC0/CC32/PC 消息 |
| 事件丢失 | 队列满 | 检查 `stats.events_dropped` |
| 回环 | 端口标记错误 | 确认使用 `ENGINE_MIDI_PORT_APP` |
