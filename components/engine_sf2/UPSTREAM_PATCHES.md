# UPSTREAM_PATCHES.md — engine_sf2 vendor 补丁清单

> **上游来源**：`tools/ESP32_SF2_Sampler_Synthesizer`（Copych / Evgeny Aslovskiy，仓库快照 commit `f4db00e`）
> **管理原则**：`upstream/` 为 vendor 拷贝，除本文件登记的补丁外**禁止就地修改**；Arduino 依赖一律由 `shim/` 经 include 路径阴影解决。上游更新时拖入覆盖后，按本清单逐条重打补丁（每处补丁在代码中以 `PATCH(Pn)` 注释标出）。

## 整体替换

| 项 | 文件 | 说明 |
|:---|:---|:---|
| R1 | `upstream/config.h` | **整体替换**。上游 config.h 含 Arduino 引脚/GUI/分区等项目配置，属目标工程职责。本版只保留合成器参数；`SAMPLE_RATE`/`DMA_BUFFER_LEN` 从 `service_audio_config.h` 取唯一来源；`MAX_VOICES 64`、`MAX_VOICES_PER_NOTE 6`、`SF2_VOICE_GAIN_REF 48`（volume_scaler 增益基准独立于 MAX_VOICES，调声部数不降全局响度）；全部 `ENABLE_*` FX/滤波开关默认关闭（遥测有余量后逐项启用）。声部数 96→64 回退理由见 P21（同音连击余音 voice 累积导致复音耗尽，扩容只是推迟崩溃点；根治后 64 声部够用且省内存） |

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
| P8 | `SF2Parser.cpp` | `clear()` | 修复双重释放：多 header 共享同一 data 指针时只释放唯一指针（P20 后共享指针为静音缓冲，见 P20 一并跳过） |
| P9 | `voice.cpp` | `nextSample()` 包络段 | 修复幅度量双重叠加：`velocityVolume`/`modVolume`/`modExpression` 在 `renderLRBlock` 的 `volL/volR` 已按块应用，此处不再每样本重复乘（上游为平方化响应，CC7/CC11 手感与响度均错误） |
| P10 | `synth.h` | `activeVoiceCount()` | 新增活跃 voice 计数，引擎遥测快照用 |
| P11 | `voice.cpp` | `prepareStart()` | `chFilter.setCoeffs/resetState` 补 `#ifdef ENABLE_CH_FILTER_M` 守卫。上游默认开启该宏故未加守卫，关闭后编译失败 |
| P12 | `voice.cpp` | `prepareStart()` 末尾 ESP_LOGD | 上游笔误 `ch->delaySend`（ch 是 uint8_t 通道号）改为 `chan->delaySend`。Arduino 高日志级别下该行被编译剔除未暴露 |
| P13 | `voice.cpp` | `updateScore()` 末尾 | 按住中音符（noteHeld）评分 ×1.5：抢音优先牺牲已释放/低电平 voice，保护弦乐等持续长音不被新音符挤掉（与 R1 声部数上调配套治理 MIDI 播放长音截短） |
| P14 | `adsr.h` / `biquad2.h` / `channel.h` / `voice.h` / `synth.h` | 类/struct 类型定义行 | 移除类型上的 `DRAM_ATTR`（section 属性不适用于类型定义，仅对变量实例有效）。GCC 发出 `-Wattributes` 告警；变量声明上的 `DRAM_ATTR` 保留不动 |
| P15 | `SF2Parser.cpp` | `parsePDTA()` 循环首部 | 移除未使用的 `chunkStart`（-Wunused-variable） |
| P16 | `SF2Parser.cpp` | `loadSampleDataToMemory()` 分配、`clear()` 释放 | IDF 5.4 弃用 `heap_caps_aligned_alloc/free`，改用 `heap_caps_malloc/heap_caps_free`（采样仅需 2 字节对齐，默认对齐已满足） |
| P17 | `adsr.cpp` | 文件顶部 | 移除未使用的 `TAG`（shim 后本文件无日志调用，-Wunused-variable） |
| P18 | `synth.cpp` | 构造函数 | `volume_scaler` 改用 `SF2_VOICE_GAIN_REF`（固定 48 声部基准）而非 `MAX_VOICES`：扩容 96 后按 1/sqrt(48) 归一，避免全局响度下掉 ~3dB |
| P19 | `synth.cpp` | `findWorstVoice()` / `findWeakestVoiceOnNote()` | 复音耗尽诊断日志：所有 voice 活跃仍分配 → 打 WARN「polyphony exhausted」；同 note 超 `MAX_VOICES_PER_NOTE` 复用 → 打 WARN「voice reuse same-note」。用于确认大型音源连击轻音是否复音池耗尽导致 |
| P20 | `SF2Parser.cpp` | `loadSampleDataToMemory()` / `clear()` | 采样分配失败时绑定共享静音缓冲（静态 4 字节零样本），而非上游 fallback 别名让大量 zone 共享同一波形 → 复音相位重叠产生刺耳噪音。静音绑定后该 zone 静音发声、2 帧即止 |
| P21 | `synth.cpp` | `noteOn()` poly 分支 | 同音 re-trigger 清理：同 channel 同 note 且已松开（noteHeld==false、release 余音）的旧 voice 先 kill 再启动新声。**必须 kill()（END_NOW+active=false）而非 die()**：die() 只进快释放、active 仍 true，尾音 voice 持续占槽位；大型音源一个键映射多个 zone（力度分层），快速连击时余音 voice 不释放会把 64 复音池塞满，新 noteOn 被迫 steal score=0 的 voice 而被掐音（只剩尾音）。kill 立即释放槽位根治。配合 R1 声部数回退 64 |

## 性能优化补丁

| 编号 | 文件 | 位置 | 说明 |
|:---|:---|:---|:---|
| P22 | `voice.cpp` | `nextSample()` 开头 | 移除每样本 `updatePitch()`，改为块级更新。热路径实测单 voice ≈20µs（复音 48 时 ren avg/max 1003/2463µs，超 1450µs 预算即 I2S 断流杂音）。安全性：弯音/通道状态在 `s_sf2_mutex` 锁内变更，渲染块持锁期间不可能变化；LFO/portamento 的 `pitchMod` 本身就按块推进（`updatePitchFactors()` 690Hz），块内恒定，逐样本重算无值变化。`effectivePhaseIncrement` 由渲染循环前的 `updateScores()` 统一刷新；新发声 voice 由 `prepareStart()` 末尾的 `updatePitch()` 保证首块有效 |

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
