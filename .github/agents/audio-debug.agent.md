---
description: "Use when: debugging audio synthesis, SF2 soundfont routing, or audio output issues on the TAB5 Music Pad"
---

# 音频调试 Agent

## 职责

诊断和修复音频合成、SF2 音色路由、音频输出相关的问题。

## 诊断流程

### 1. 确认音频管线状态

```c
// 检查 SF2 引擎状态
engine_sf2_debug_info_t sf2_info;
engine_sf2_get_debug_info(&sf2_info);
ESP_LOGI(TAG, "SF2: voices=%d/%d render_avg=%dus render_max=%dus",
         sf2_info.active_voices, sf2_info.max_voices,
         sf2_info.render_block_us_avg, sf2_info.render_block_us_max);
```

### 2. 检查 MIDI 事件流

```c
// 检查 MIDI 总线统计
engine_midi_stats_t midi_stats;
engine_midi_get_stats(&midi_stats);
ESP_LOGI(TAG, "MIDI: queue=%d/%d dropped=%d",
         midi_stats.queue_used, midi_stats.queue_size, midi_stats.events_dropped);
```

### 3. 检查音频路由

- 主音源：`engine_sf2`（注册为 `AUDIO_SOURCE_SF2`）
- 第二通道：`service_audio_aux_write()`（TTS 混音）
- 采样率：44.1 kHz，每周期 64 帧

### 4. 验证 SF2 音色加载

```c
// 检查默认音色探测顺序
// 1. /sdcard/soundfonts/default.sf2
// 2. SD 卡 soundfonts/ 目录首个 .sf2
// 3. /sys_int/soundfonts/default.sf2
```

## 常见问题排查

| 现象 | 排查步骤 |
|:---|:---|
| 完全无声 | 检查 ES8388 初始化 → 检查 I2S 输出 → 检查 `task_audio` 运行状态 |
| 无 SF2 声音 | 检查 SF2 文件加载 → 检查 MIDI 事件是否到达 SF2 → 检查 voice 数量 |
| 声音失真 | 检查软限幅 → 检查采样率匹配 → 检查 float 转 int16 范围 |
| 噪音 | 检查 AEC 状态 → 检查麦克风增益 → 检查电源干扰 |
| 音频卡顿 | 检查 `task_audio` 优先级 → 检查渲染时间 → 检查内存碎片 |

## 工具脚本

```bash
# 列出 SF2 音色
python tools/sf2_list_presets.py <file.sf2>
```

## 输出格式

- 问题现象描述
- 已执行的排查步骤
- 根因分析
- 修复方案
