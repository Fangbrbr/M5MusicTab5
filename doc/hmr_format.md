# Hammy MIDI Recording（.hmr）格式规范

> 本文档面向需要在 PC 上位机中解析 `.hmr` 录音文件、进行回放或转码（MP3/WAV）的开发者。
> 文中所有多字节整数如无特殊说明均为**小端序（LE）**。

---

## 1. 概述

`.hmr` 是 TAB5_Music_Pad 项目自定义的轻量 MIDI 总线录音格式。它记录系统内部 MIDI 事件总线流，而不是真实音频采样。

- 不保存波形，只保存 MIDI 指令与时间戳。
- 回放时必须依赖 MIDI 合成器（设备端使用 `engine_sf2` + SoundFont；PC 端可用 FluidSynth 等软合成器）。
- 文件默认存放于 SD 卡 `/record/` 目录。
- 命名约定：`{source_tag}_{YYYYMMDDhhmmss}.hmr`，例如 `zen_20260729143028.hmr`。

---

## 2. 文件整体结构

| 区域 | 长度 | 说明 |
|------|------|------|
| Header | 128 字节 | 文件头，含校验 |
| Event Data | 变长 | 若干条事件记录 |
| Footer | 4 字节 | 结束标记 `ENDR` |

```
[Header 128B][Event 1][Event 2]...[Event N][Footer 4B]
```

事件总长度由文件大小决定：

```
event_data_size = file_size - 128 - 4
```

---

## 3. Header（128 字节）

| 偏移 | 长度 | 字段 | 说明 |
|------|------|------|------|
| 0 | 4 | magic | 固定 `"HMRE"`（0x48 0x4D 0x52 0x45） |
| 4 | 1 | version | 当前为 `2` |
| 5 | 3 | reserved | 保留，填 0 |
| 8 | 4 | header_size | 固定 `128` |
| 12 | 4 | start_time_epoch | 开始录制时的 UNIX 时间戳（秒） |
| 16 | 4 | duration_ms | 录制总时长（毫秒） |
| 20 | 4 | event_count | 事件条数 |
| 24 | 4 | channels_used | 16 位通道使用位图（bit0=ch1，bit15=ch16） |
| 28 | 4 | data_crc32 | Event Data 区域的 CRC32 |
| 32 | 4 | header_crc32 | 整个 Header（含本字段位置）的 CRC32 |
| 36 | 16 | source_tag | 录制来源标签，如 `"zen"`，剩余字节填 0 |
| 52 | 12 | reserved | 保留，填 0 |
| 64 | 64 | channel_init[16][4] | 初始通道状态快照 |

### 3.1 Header CRC32 计算方式

1. 构造 128 字节 Header 缓冲区。
2. 将偏移 32~35（header_crc32 字段）置为 0。
3. 对整个 128 字节计算 CRC32（Ethernet 多项式）。
4. 将结果写入偏移 32~35。

设备端打包代码示例：

```c
// 伪代码
write_u32_le(&buf[32], 0);              // 先清零 CRC 字段
crc = engine_midi_rec_crc32(buf, 128);  // 标准 CRC32
write_u32_le(&buf[32], crc);
```

### 3.2 初始通道状态 `channel_init`

`channel_init[16][4]` 保存开始录制瞬间每个 MIDI 通道的音色状态，用于回放时先恢复音色，再播放事件。

| 每通道内偏移 | 含义 | 特殊值 |
|------|------|------|
| 0 | Bank MSB（CC#0） | `0xFF` 表示未设置 |
| 1 | Bank LSB（CC#32）| `0xFF` 表示未设置 |
| 2 | Program Number（PC）| `0xFF` 表示未设置 |
| 3 | reserved | `0xFF` |

回放时，对每个通道：

1. 若 Bank MSB ≠ 0xFF，发送 `CC#0 = Bank MSB`。
2. 若 Bank LSB ≠ 0xFF，发送 `CC#32 = Bank LSB`。
3. 若 Program ≠ 0xFF，发送 `Program Change = Program`。

> 为什么需要快照：录制按钮可以随时点击，音色也可能在录制前被改变；快照保证回放时音色与录制时一致。

---

## 4. Event 结构

单条事件最小 12 字节，若携带 SysEx 则追加 `sysex_len` 字节。

| 偏移 | 长度 | 字段 | 说明 |
|------|------|------|------|
| 0 | 4 | rel_ms | 距离录制开始的相对时间（毫秒） |
| 4 | 1 | type | MIDI 消息类型 |
| 5 | 1 | channel | 通道号 0~15 |
| 6 | 1 | data1 | 数据字节 1 |
| 7 | 1 | data2 | 数据字节 2 |
| 8 | 2 | value | 扩展值，Pitch Bend 等使用 |
| 10 | 1 | sysex_len | SysEx 数据长度，0 表示无 |
| 11 | 1 | reserved | 保留，填 0 |
| 12 | N | sysex_data | 仅当 `sysex_len > 0` 时存在，N = sysex_len |

事件总长度：

```
event_size = 12 + sysex_len
```

### 4.1 MIDI 类型映射

`type` 字段直接对应 MIDI 状态字节高 4 位（`0x80~0xEF`），常见值如下：

| type | MIDI 状态 | 含义 |
|------|-----------|------|
| 0x80 | Note Off | `data1`=note, `data2`=velocity |
| 0x90 | Note On | `data1`=note, `data2`=velocity（velocity=0 等效 Note Off） |
| 0xB0 | Control Change | `data1`=CC 编号, `data2`=值 |
| 0xC0 | Program Change | `data1`=program |
| 0xE0 | Pitch Bend | `value` 为 14 位弯音值（0~16383，8192 为中心） |
| 0xF0 | SysEx | `sysex_data` 中保存完整 SysEx 数据（不含 0xF0 起始和 0xF7 结束） |

普通通道消息由 `(type | channel)` 构成完整 MIDI 状态字节。

### 4.2 特殊约定

- `value` 字段对 Pitch Bend 使用全部 14 位：`value = data1 | (data2 << 7)`。
- SysEx 数据中不包含起始 `0xF0` 和结束 `0xF7`；若需要，解析端自行补回。
- 回放时发送 MIDI 事件前需恢复初始音色快照。

---

## 5. Footer

文件最后 4 字节固定为 ASCII `"ENDR"`：

```
0x45 0x4E 0x44 0x52
```

若 Footer 不匹配，应视为文件损坏或截断。

---

## 6. CRC32

使用标准 Ethernet CRC32：

- 多项式：`0xEDB88320`（与 zlib、PNG、Ethernet 一致）
- 初始值：`0xFFFFFFFF`
- 最终异或：`0xFFFFFFFF`

设备端实现示例：

```c
uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ s_crc32_table[(crc ^ data[i]) & 0xFF];
    }
    return crc;
}

uint32_t crc32(const uint8_t *data, size_t len) {
    return (crc32_update(0xFFFFFFFFU, data, len) ^ 0xFFFFFFFFU);
}
```

PC 端 Python 示例：

```python
import zlib

def hmr_crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF
```

---

## 7. 完整解析流程

1. 打开文件，读取 128 字节 Header。
2. 验证 `magic == "HMRE"`、`version == 2`、`header_size == 128`。
3. 将 Header 中 `header_crc32` 位置清零，重新计算 CRC32，与 `header_crc32` 比较。
4. 读取剩余 `file_size - 128` 字节，最后 4 字节应为 `"ENDR"`。
5. 对中间的事件数据计算 CRC32，与 `data_crc32` 比较。
6. 按 `Event 结构` 逐条解析事件。
7. 回放前发送 `channel_init` 中记录的初始音色。

---

## 8. PC 端回放实现要点

### 8.1 推荐技术栈

| 功能 | 推荐库 / 工具 |
|------|--------------|
| 文件解析 | Python struct / 任意语言二进制读取 |
| MIDI 输出 | `mido` + `python-rtmidi`（播放外置 MIDI 设备） |
| 软合成回放 | FluidSynth（`pyfluidsynth`）+ SoundFont |
| 转 WAV | FluidSynth 渲染为 PCM，再用 `wave` 模块写入 |
| 转 MP3 | `pydub` / `lameenc` / ffmpeg |

### 8.2 回放伪代码

```python
import time
import mido

def play_hmr(path, port_name=None):
    rec = parse_hmr(path)            # 按第 7 节解析

    out = mido.open_output(port_name) if port_name else mido.open_output()

    # 1. 恢复初始音色
    for ch in range(16):
        msb, lsb, pc, _ = rec.channel_init[ch]
        if msb != 0xFF:
            out.send(mido.Message('control_change', channel=ch, control=0, value=msb))
        if lsb != 0xFF:
            out.send(mido.Message('control_change', channel=ch, control=32, value=lsb))
        if pc != 0xFF:
            out.send(mido.Message('program_change', channel=ch, program=pc))

    # 2. 按时间播放事件
    start = time.time()
    for ev in rec.events:
        target = start + ev.time_us / 1_000_000.0
        while time.time() < target:
            time.sleep(0.001)

        if ev.type == 0xE0:  # Pitch Bend
            out.send(mido.Message('pitchwheel', channel=ev.channel, pitch=ev.value - 8192))
        elif ev.type == 0xF0:  # SysEx
            out.send(mido.Message('sysex', data=ev.sysex_data))
        else:
            out.send(mido.Message.from_bytes([ev.type | ev.channel, ev.data1, ev.data2]))

    out.close()
```

### 8.3 使用 FluidSynth 转码为 WAV/MP3

```python
import fluidsynth
import numpy as np
from pydub import AudioSegment

SF2_PATH = "your_soundfont.sf2"
SAMPLE_RATE = 44100

def render_hmr_to_wav(hmr_path, wav_path):
    rec = parse_hmr(hmr_path)
    fs = fluidsynth.Synth(samplerate=SAMPLE_RATE)
    fs.start(driver="file")  # 或手动采样
    sfid = fs.sfload(SF2_PATH)
    fs.program_select(0, sfid, 0, 0)

    # 恢复初始音色
    for ch in range(16):
        msb, lsb, pc, _ = rec.channel_init[ch]
        if msb != 0xFF:
            fs.cc(ch, 0, msb)
        if lsb != 0xFF:
            fs.cc(ch, 32, lsb)
        if pc != 0xFF:
            fs.program_change(ch, pc)

    # 按 64 帧（约 1.45ms）粒度渲染
    total_samples = int(rec.total_us / 1_000_000.0 * SAMPLE_RATE) + SAMPLE_RATE  # 留 1 秒尾音
    pcm = np.zeros(total_samples * 2, dtype=np.int16)

    event_idx = 0
    for block_start in range(0, total_samples, 64):
        block_us = block_start / SAMPLE_RATE * 1_000_000
        while event_idx < len(rec.events) and rec.events[event_idx].time_us <= block_us:
            ev = rec.events[event_idx]
            if ev.type == 0x90:
                fs.noteon(ev.channel, ev.data1, ev.data2)
            elif ev.type == 0x80 or (ev.type == 0x90 and ev.data2 == 0):
                fs.noteoff(ev.channel, ev.data1)
            elif ev.type == 0xB0:
                fs.cc(ev.channel, ev.data1, ev.data2)
            elif ev.type == 0xC0:
                fs.program_change(ev.channel, ev.data1)
            elif ev.type == 0xE0:
                fs.pitch_bend(ev.channel, ev.value)
            event_idx += 1

        block = fs.get_samples(64)
        pos = block_start * 2
        pcm[pos:pos + len(block)] = block

    # 写入 WAV
    import wave, struct
    with wave.open(wav_path, "wb") as wf:
        wf.setnchannels(2)
        wf.setsampwidth(2)
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(pcm.tobytes())

    fs.delete()

# WAV -> MP3
def wav_to_mp3(wav_path, mp3_path, bitrate="192k"):
    audio = AudioSegment.from_wav(wav_path)
    audio.export(mp3_path, format="mp3", bitrate=bitrate)
```

> 注意：设备端 `engine_sf2` 合成器对 Bank/Program、鼓组路由、弯音等行为可能与 PC 端软合成器有差异；转码时请选择与设备端相近的 GM/GS/XG SoundFont。

---

## 9. 版本历史

| 版本 | 说明 |
|------|------|
| 1 | 初始 64 字节 Header，无初始音色快照 |
| 2 | 128 字节 Header，新增 `channel_init[16][4]` 初始通道音色快照 |

---

## 10. 参考文件

- 设备端解析实现：`components/engine_midi/engine_midi_rec.c`
- 设备端解析头文件：`components/engine_midi/include/engine_midi_rec.h`
- 设备端录音服务：`components/service_recorder/service_recorder.c`
- 设备端回放实现：`components/app_midi_player/app_midi_player.c`
