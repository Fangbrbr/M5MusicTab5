# UPSTREAM_PATCHES.md — engine_sf2 vendor 补丁清单

> **上游来源**：`tools/ESP32_SF2_Sampler_Synthesizer`（Copych / Evgeny Aslovskiy，仓库快照 commit `f4db00e`）
> **管理原则**：`upstream/` 为 vendor 拷贝，除本文件登记的补丁外**禁止就地修改**；Arduino 依赖一律由 `shim/` 经 include 路径阴影解决。上游更新时拖入覆盖后，按本清单逐条重打补丁（每处补丁在代码中以 `PATCH(Pn)` 注释标出）。

## 整体替换

| 项 | 文件 | 说明 |
|:---|:---|:---|
| R1 | `upstream/config.h` | **整体替换**。上游 config.h 含 Arduino 引脚/GUI/分区等项目配置，属目标工程职责。本版只保留合成器参数；`SAMPLE_RATE`/`DMA_BUFFER_LEN` 从 `service_audio_config.h` 取唯一来源；`MAX_VOICES 48`（32 时弦乐密集 MIDI 抢音导致长音被截短，上调缓解；遥测有余量可续调）、`MAX_VOICES_PER_NOTE 4`；全部 `ENABLE_*` FX/滤波开关默认关闭（遥测有余量后逐项启用） |

## 功能剔除补丁

| 编号 | 文件 | 位置 | 说明 |
|:---|:---|:---|:---|
| P1 | `synth.h` / `synth.cpp` | include 区、`begin()`、文件尾部、`loadSynthState`/`saveSynthState` 声明 | 移除 `#include "TLVStorage.h"` 及 TLV 状态持久化（`begin()`/`loadSynthState`/`saveSynthState`）。项目不需要跨重启的合成器状态保存；适配层直接调 `loadSf2File()` |
| P2 | `voice.cpp` | 文件顶部 | `int Voice::usage = 0;` 定义从 `.ino` 移入（上游定义在应用入口，组件化后链接不到） |

## Bug 修复补丁

| 编号 | 文件 | 位置 | 说明 |
|:---|:---|:---|:---|
| P3 | `SF2Parser.h` / `SF2Parser.cpp` | `setProgressCallback()`、`loadSampleDataToMemory()` 循环 | 新增采样加载进度回调（0-100），供 boot 进度条。上游无此机制 |
| P4 | `synth.cpp` | `loadSf2File()` | 绝对路径（`/` 开头）直接使用，相对路径才拼 `SF2_PATH`。适配层只传绝对路径（SD/SPIFFS 双挂载点） |
| P5 | `SF2Parser.cpp` | `applyGenerators()` switch 尾部 | 补上 SF2 规范的 `InitialAttenuation`(gen 48)（centibels→线性衰减）。上游缺失导致 Zone 衰减永远 1.0，音色响度映射失真 |
| P6 | `SF2Parser.cpp` | `applyGenerators()` 末尾 | 移除「chorusSend/reverbSend 为 0 强制改 1.0」的上游私货。非 SF2 语义，效果发送保持解析值 |
| P7 | `SF2Parser.cpp` | `sf2FixedName()` + preset/instrument 名赋值两处 | SF2 name 字段为 20 字节定长、不保证 NUL 结尾，按定长拷贝防过读 |
| P8 | `SF2Parser.cpp` | `clear()` | 修复双重释放：PSRAM 不足时 fallback 别名会让多个 SampleHeader 共享同一 `data` 指针，只释放唯一指针 |
| P9 | `voice.cpp` | `nextSample()` 包络段 | 修复幅度量双重叠加：`velocityVolume`/`modVolume`/`modExpression` 在 `renderLRBlock` 的 `volL/volR` 已按块应用，此处不再每样本重复乘（上游为平方化响应，CC7/CC11 手感与响度均错误） |
| P10 | `synth.h` | `activeVoiceCount()` | 新增活跃 voice 计数，引擎遥测快照用 |
| P11 | `voice.cpp` | `prepareStart()` | `chFilter.setCoeffs/resetState` 补 `#ifdef ENABLE_CH_FILTER_M` 守卫。上游默认开启该宏故未加守卫，关闭后编译失败 |
| P12 | `voice.cpp` | `prepareStart()` 末尾 ESP_LOGD | 上游笔误 `ch->delaySend`（ch 是 uint8_t 通道号）改为 `chan->delaySend`。Arduino 高日志级别下该行被编译剔除未暴露 |
| P13 | `voice.cpp` | `updateScore()` 末尾 | 按住中音符（noteHeld）评分 ×1.5：抢音优先牺牲已释放/低电平 voice，保护弦乐等持续长音不被新音符挤掉（与 R1 声部数上调配套治理 MIDI 播放长音截短） |
| P14 | `adsr.h` / `biquad2.h` / `channel.h` / `voice.h` / `synth.h` | 类/struct 类型定义行 | 移除类型上的 `DRAM_ATTR`（section 属性不适用于类型定义，仅对变量实例有效）。GCC 发出 `-Wattributes` 告警；变量声明上的 `DRAM_ATTR` 保留不动 |
| P15 | `SF2Parser.cpp` | `parsePDTA()` 循环首部 | 移除未使用的 `chunkStart`（-Wunused-variable） |
| P16 | `SF2Parser.cpp` | `loadSampleDataToMemory()` 分配、`clear()` 释放 | IDF 5.4 弃用 `heap_caps_aligned_alloc/free`，改用 `heap_caps_malloc/heap_caps_free`（采样仅需 2 字节对齐，默认对齐已满足） |
| P17 | `adsr.cpp` | 文件顶部 | 移除未使用的 `TAG`（shim 后本文件无日志调用，-Wunused-variable） |

## 已通过 shim 解决、无需补丁的项

| 上游依赖 | shim 文件 |
|:---|:---|
| `<Arduino.h>`（micros/delay/PI/constrain/min/max） | `shim/Arduino.h`（min/max/constrain 用模板防 STL 冲突） |
| `String` | `shim/WString.h`（std::string 封装） |
| `<FS.h>` / `fs::File` / `fs::FS` / 目录遍历 | `shim/FS.h` + `shim/shim.cpp`（stdio/VFS 后端） |
| `<LittleFS.h>` / `<SD_MMC.h>` 单例 | `shim/LittleFS.h` / `shim/SD_MMC.h` + `shim/shim.cpp` |
| `<esp_dsp.h>`（include 但无调用） | `shim/esp_dsp.h`（空） |

## 已知遗留（未处理，按优先级排序）

1. **`getZonesForNote` 每次 noteOn 线性扫描 + 堆分配 vector**：事件路径堆分配。后续优化为 program change 时预建 128 键路由表
2. **`loadSf2File` 与渲染互斥但加载耗时长**：换音色期间渲染暂停数秒。后续可改「临时 parser 无锁解析 + 锁内交换」
3. **FX（reverb/chorus/delay）与滤波默认关闭**：渲染遥测确认负载余量后逐项启用
4. **`misc.h` 内 Xtensa 汇编 `one_div`/`fdiv`**：RISC-V 不兼容但全工程无调用，未实例化不影响；上游同步时留意勿新增调用
5. **`ChannelState::reset()` 把 pan 复位为 0.0（初始值 0.5）**：上游 quirk，CC121 后声像偏左，待验证后修
