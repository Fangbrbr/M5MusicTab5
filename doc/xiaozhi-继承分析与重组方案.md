# xiaozhi 继承深度分析与重组方案

> 本文档回答三个问题：① 移植版「不能唤醒、不能打断」的根因是什么；② 全局内部/外部 RAM 现状与合理分配；③ 要达到「除升级与原版图片资源外全能力对齐」的目标，架构上怎么重组、分几步走。
> 依据：对 `tools/xiaozhi-esp32`（原版）与 `components/service_xiaozhi`、`components/service_audio`、`components/service_rtc`、`managed_components`（BSP/esp-sr/esp_codec_dev）、`build/TAB5_Music_Pad.map`、`sdkconfig`、IDF v5.4.4 源码的逐一核对。机制细节见姊妹篇 `doc/xiaozhi-esp32-core-analysis.md`。

---

## 1. 问题定性与根因链

**结论先行：「不能唤醒、不能打断」主要不是 RAM 总量不够，而是三个架构取舍叠加的结果；RAM 分配不合理（AFE 挤内部 RAM）是第二根因。32MB PSRAM 确实极度富余（实测余量 >20MB），问题在内部 RAM 侧的错误堆叠。**

### 1.1 「不能唤醒」根因（按可能性排序）

| # | 根因 | 证据 | 性质 |
|:---:|:---|:---|:---|
| 1 | **唤醒只存在于 AI App 前台窗口**：`main.c` 开机只 `task_ai_start()`（建任务，main/main.c:146），`service_xiaozhi_process()` 阻塞等 `XZ_CMD_START`；START 的全部调用方是 app_ai_agent 的 on_init/on_resume/重置（app_ai_agent.c:707/779/557），on_pause/on_destroy 即 stop（:771/:825）并销毁 AFE。**不打开 AI App，唤醒 100% 不存在**。原版是 Idle 态全局常驻唤醒（application.cc:924-925） | 事实 | 架构 |
| 2 | **AFE 打开靠抽签**：`WAKE_MIN_INTERNAL_FREE=90KB` 门槛（xiaozhi_wake.c:49,110-116），注释自证「实测入口空闲仅 ~97KB，96KB 阈值几乎必判失败」——余量仅 ~7KB，且失败后整段会话退化 manual，UI 零提示，问候语仍在宣传「喊 Hi，喵喵」（app_ai_agent.c:49） | 事实 | RAM 分配 |
| 3 | **STANDBY 中 mic 泵一次读错误即永久死亡**：读错误→停泵（service_xiaozhi.c:512-516），纯 STANDBY 无任何路径重启泵（重启泵的 `xz_enter_standby` 只在 TTS_STOP/断线/空闲超时触发，而空闲超时要求通道开着） | 事实 | 健壮性 |
| 4 | **命中后 1~10 秒静默无反馈**：开通道同步阻塞等 hello，无 popup 提示音（原版有，application.cc:831），主观等同「没反应」 | 事实 | 体验缺失 |
| 5 | mic 增益 32dB 经验值 + TTS 合成训练模型（wn9_himiaomiao_tts，sdkconfig:772）召回率；fetch 结果队列深 2 满则丢检出（xiaozhi_wake.c:94,173） | 推断 | 调参 |

### 1.2 「不能打断」根因（硬性依赖链）

```text
无 AEC（xiaozhi_wake.c:136 aec_init=false）
  ⇒ 开 mic 必须关扬声器（service_audio.c:395-399，单 I2S 端口互斥）
  ⇒ 录音期 service_audio_process 整体停摆（:215-218，SF2/aux 全停）
  ⇒ TTS 播报期必须停 mic 泵（service_xiaozhi.c:894-901）
  ⇒ 播报期唤醒检测物理缺席
  ⇒ 打断只剩 AI App 前台的按钮
```

原版每一环的对应物：有硬件回采参考 → AEC 常开（afe_audio_engine.cc:135）→ 播放采集并行 → SPEAKING 保持 AFE 唤醒检测（application.cc:954-958）→ 命中走 `AbortSpeaking(wake_word_detected)`（:822-838）。

### 1.3 连带伤害（用户能感知到的其他异常）

- AI App 前台且唤醒可用时，**整机持续静音**（mic 常开 ⇒ 扬声器关 ⇒ SF2/提示音全哑）；
- auto 模式 TTS_STOP 后立即开 mic 关扬声器，**aux 环形缓冲里最多 ~2 秒的回答尾巴被掐掉**（service_audio.c:31 的 2s 深度 vs service_xiaozhi.c:908-910 的立即重开）；
- 非 SPEAKING 态迟到的下行 TTS 包无条件解码写 aux，manual READY 时**突兀出声**（service_xiaozhi.c:259-272，原版只在 Speaking 消费，application.cc:518-522）；
- 半开 TCP（对端死而无 FIN）下永不触发 DISCONNECTED，20s 空闲看门狗豁免 SPEAKING，可能**永久卡在 SPEAKING 无声**（service_xiaozhi.c:992-1000）；
- WS 单帧 >4096B 直接丢弃（xiaozhi_ws.c:64-67），**大 MCP payload 会丢**；
- server_time 只取 timestamp 忽略时区写 UTC（xiaozhi_ota.c:290-300），与 RTC 服务每 60s 本地语义回写（service_rtc.cpp:193-249）**互相覆盖**，绑定窗口内时钟可差 8 小时。

---

## 2. 全局 RAM 预算（实测）

### 2.1 ESP32-P4 内部 RAM 布局（来源：build/TAB5_Music_Pad.map Memory Configuration 与 size -A）

| 区域 | 大小 | 用途 |
|:---|---:|:---|
| sram_low（0x4ff00000） | 183,504 B | IRAM + .dram0 + 少量堆 |
| sram_high（0x4ff40000） | 393,216 B | .dram1.bss + 主堆 |
| TCM | 8 KB | 特殊用途 |
| **应用可用合计** | **≈576.7 KB（563 KiB）** | |

**链接期静态占用**：`.iram0.text` 97,780 + `.dram0.data` 40,853 + `.dram0.bss` 36,768 + `.dram1.bss` 83,388 = **258,789 B（≈253 KiB）**。sram_low 仅剩 ~8 KB；**开机堆 ≈310 KB**（几乎全在 sram_high）。

**运行时内部 RAM 主要消费者**：

| 消费者 | 大小 | 依据 |
|:---|---:|:---|
| 任务栈（全部内部 RAM）：task_ai 24K + wake_fetch 16K + esp_websocket_client 20K + 5×8K（gui/app/comm/input/audio） | ≈101 KB | task_ai.c:18、xiaozhi_wake.c:32、service_xiaozhi_config.h:73、task_*.c |
| IDF 系统任务栈（main/ipc×2/esp_timer/sys_evt/wifi/tcpip/mDNS/usb 等） | ≈40–60 KB | 估算 |
| WiFi 驱动内部残留（`SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y` 已外移大头） | ≈30–50 KB | 估算 |
| I2S DMA（256 帧×2ch×2B×6 desc） | ≈6 KB | service_audio.c:63-66 |
| 瞬时分配：cJSON/http/TLS 小对象（≤4096 B 优先内部，`SPIRAM_MALLOC_ALWAYSINTERNAL=4096`） | 波动 | sdkconfig |
| **AFE（当前 MORE_INTERNAL）** | **≈90 KB**（门槛值即其需求） | xiaozhi_wake.c:46-49 |
| **合计后余量** | **会话入口实测仅 ~97 KB（代码注释自证）** | |

**症结**：310 KB 堆被任务栈/驱动吃掉一半以上，AFE 的 ~90 KB 塞进去后**余量趋近于零**——既要抽签开门（90KB 门槛），开门后 TLS 握手、MCP 大 JSON 等瞬时峰值又无处安放。而 `MORE_INTERNAL` 是当初为了规避「PSRAM 在 flash 写期间崩溃」选的。

### 2.2 PSRAM（32 MB）使用

| 消费者 | 大小 | 依据 |
|:---|---:|:---|
| SF2 采样 PCM | ≈3.1 MB | SF2Parser.cpp:710（MALLOC_CAP_SPIRAM） |
| LVGL 全堆（对象+绘制缓冲，1280×720） | 数 MB | engine_gui.c:61-97（lv_mem 收口 PSRAM） |
| aux 混音环形缓冲 | 345 KB（88,200 帧×2ch×2B） | service_audio.c:31,128 |
| SF2 效果器/声部缓冲 | <1 MB | fx_reverb.h:18 等 |
| mbedTLS/esp-tls 大缓冲（>4KB 走 PSRAM） | 每连接数十 KB | sdkconfig malloc 策略 |
| **已用合计** | **<10 MB** | |
| **空闲** | **>20 MB** | 用户判断正确：极度富余 |

### 2.3 关键约束核实：P4 上「flash 写期间访问 PSRAM」到底崩不崩？

对 IDF v5.4.4 源码的核查结论（components/spi_flash/）：

1. flash 写/擦除入口固定走 `spi_flash_disable_interrupts_caches_and_other_cpu`（flash_ops.c:83）；
2. **另一核**被 IPC 强制进入 `spi_flash_op_block_func`：关调度、关非 IRAM 中断、在 IRAM 里忙等（cache_utils.c:110-145）——**另一核的任务整体冻结，根本不会在写窗口内运行**；
3. 写操作所在核只有当前任务继续跑（SMP 下 `vTaskPreemptionDisable`），且入口断言**该任务栈必须在 DRAM**（`esp_task_stack_is_sane_cache_disabled`，cache_utils.c:81-89）；
4. cache 注释确认 PSRAM cache 双核共享（cache_utils.c:108-112），但配合 2/3 两点的冻结机制，**写窗口内没有任何代码会访问 PSRAM**。

**结论（2026-07-29 终版，两轮真机崩溃 + 官方资料交叉验证）**：

1. **芯片 revision 是分水岭**：本机为 ESP32-P4 **rev v1.3（eco2）**，属 esp-sr 的 `esp32p4_less_v3` 变体。P4 的 PIE（SIMD）指令集在 **rev v3.x 发生过变更**（饱和/舍入语义更新），esp-sr 自 2.4.1 起按 revision 分发不同预编译库（CHANGELOG："Supports both esp32p4 eco6 and old versions"）。
2. **本芯片上 PIE 核无法访问 PSRAM 数据**：`MORE_PSRAM` 首次推理即 `Store access fault @ dl_esp32p4_sr_pointwise_conv1d_qacc_*`；`MORE_INTERNAL` 在内部不足时**溢出**到 PSRAM 同样 `Load access fault`（同一核族）——`MORE_INTERNAL` 只是「优先」而非「独占」。官方资料「支持/推荐 PSRAM」面向新 revision 芯片与普通 CPU 访问（AFE 框架缓冲/模型存储），与本机不矛盾；上游 xiaozhi 的 MORE_PSRAM 配置亦面向新 revision 板型。
3. **因此 AFE 张量必须「整体」落内部 RAM**：实测 MORE_INTERNAL 全量需求 ~70KB（前任开发者：入口 ~97KB 打开后余 >20KB）。门槛必须按「总量 ≥96KB 且最大连续块 ≥56KB」双条件把关；旧 90KB 门槛在入口 ~85KB 时必失败（唤醒静默消失），72KB 门槛过低（溢出即崩），均已被真机证据否掉。
4. **与 flash 写 cache-off 无关**：IDF 冻结机制（另一核 IPC 冻结、写核仅 IRAM 任务）保证 flash 写窗口内无人访问 PSRAM；esp-sr 模型权重恒为 flash 分区 mmap XIP（model_path.c:522-528），两次崩溃均无 flash 写并发。
5. **版本教训**：esp-sr 2.4.1 才加入 eco6/旧版适配、2.4.5 修过 P4 崩溃——2.3.1 早于全部 rev 适配，在本芯片上严格更差；「下载量最高」反映的是发布时间早（S3 时代存量项目多），不是对本芯片的适配度。本项目锁定 2.4.7（`main/idf_component.yml: ~2.4.6`）。
6. **结构性出路**：上游 P4 默认开 `CONFIG_SPIRAM_XIP_FROM_PSRAM`（代码从 PSRAM 执行）+ `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC`——把 ~98KB IRAM 释放为堆，内部 RAM 才供得起 AFE/AEC。这是本项目 P1（AEC 全双工）开工前应评估的开关（见第 5 章 P0-3）。

**附带结论**：真正需要敬畏 flash 写的是「每次 NVS commit 双核冻结数 ms（擦除更久）」——task_app 每 1s `service_nvs_commit()` 会造成 AFE/音频的微小停顿，当前靠「无脏页时 commit 为快速 no-op」规避，维持现状即可。

### 2.4 RAM 重分配方案

| 项 | 现状 | 目标 | 内部 RAM 变化 | 依据/风险 |
|:---|:---|:---|:---:|:---|
| **esp-sr AFE 内存模式** | `MORE_INTERNAL` + 90KB 门槛抽签 | **保持 `MORE_INTERNAL`**，门槛改双条件（总量 ≥96KB 且最大连续块 ≥56KB）+ 失败投事件告知 UI | 0（但不再抽签、不再溢出即崩） | rev v1.3 PIE 无法访问 PSRAM，见 2.3 |
| 队列存储/音频缓冲 | 内部 RAM（~14KB） | **xQueueCreateStatic + heap 缓冲外迁 PSRAM** | +≈14 KB | 为 AFE 腾出整体内部空间 |
| WS 接收缓冲 | 静态 4096B，大帧丢弃 | PSRAM 动态缓冲（16KB 或按帧长分配） | -4 KB | 修复大 MCP 丢帧 |
| 唤醒词 2s 音频缓存 | 无 | 新增 64KB PSRAM 环形缓冲 | 0 | 恢复唤醒音频上行（原版 64000B，afe_audio_engine.cc:104） |
| 任务栈（task_ai 24K/wake_fetch 16K/ws 20K） | 内部 RAM | **保持内部**（task_ai 写 NVS 是 flash 操作任务，栈必须内部；其余留安全余量） | 0 | cache_utils.c:81-89 断言 |
| I2S DMA / codec 缓冲 | 内部 | 保持内部 | 0 | DMA 硬性要求 |
| LVGL 堆、SF2、aux、TLS 大缓冲 | PSRAM | 保持 PSRAM | 0 | 现状正确 |
| WiFi/LwIP | TRY_ALLOCATE_WIFI_LWIP=y | 保持 | 0 | sdkconfig 已优 |

**重分配后内部 RAM 账（终版）**：AFE 因 rev v1.3 的 PIE 限制必须整体留内部（~70KB），PSRAM 无法为它直接泄压；正确姿势是反过来——**把能去 PSRAM 的全部外迁**（队列/缓冲/TLS/代码），为 AFE 腾出连续内部空间。当前账：会话入口 ~85KB + 外迁释放 ~14KB ≈ 99KB，过 96KB 门槛但余量薄；结构性解法是 P0-3 的 `SPIRAM_XIP_FROM_PSRAM`（再释放 ~98KB IRAM）。

---

## 3. 资源链路分析

### 3.1 音频硬件链路现状（精确版）

```text
ES7210（4 通道 ADC，I2C 7bit 0x40）──DSIN(GPIO28)──┐
                                                   │ I2S1 单一全双工通道对（STD 模式，共享时钟域）
ES8388（DAC，I2C 7bit 0x10）───DOUT(GPIO26)───────┘ MCLK=GPIO30 BCLK=GPIO27 WS=GPIO29
```

- BSP（espressif__m5stack_tab5, bsp_audio.c:50-74）：TX/RX 均 `i2s_channel_init_std_mode`；ES7210 默认 `mic_selected=MIC1|MIC2`（<3 个 → **非 TDM**，只出 2 槽立体声，es7210.c:177-186,453-455）；从机跟随 I2S 时钟。
- 当前 mic 打开：`mic_open(16000,1)` → esp_codec_dev 发现 TX 已关 → **把整口 I2S 时钟重配为 16kHz**（audio_codec_data_i2s.c:420-451），所以开 mic 必须先关扬声器，SF2 渲染同步停摆。16k 是硬件时钟切换，无软件重采样。
- **硬件回采参考当前拿不到**：MIC3（PCB 上硬连扬声器信号的脚）未上电未上槽；且 STD 通道不能原地切 TDM，BSP 的 rx 句柄无公开访问器——不改 BSP 走不了原版的 4 槽 TDM 路线。

### 3.2 AEC 参考信号：方案决策

原版在**同一块 Tab5 硬件**上的做法：RX 改 TDM 4 槽 @24kHz，槽 0=MIC1、槽 1=MIC3=扬声器硬回采，AFE `input_format="MR"`、`aec_init=true`、24k→16k 软件重采样（esp_ae_rate_cvt）。

本项目整机音频是 44.1kHz 且 BSP 已占 I2S1 STD 模式，三个可行方案：

| 方案 | 数据流 | 改动 | 评价 |
|:---|:---|:---|:---|
| **C：软件参考（推荐先行）** | mic 开 **44.1k mono**（与 TX 44.1k 同速率，esp_codec_dev 同参数直通、TX 零扰动，audio_codec_data_i2s.c:437-457）→ 重采样 16k 作 M；**混音出口 PCM**（软限幅后的 s_i2s_buffer，含 SF2+TTS）折 mono → 重采样 16k 作 R → AFE "MR" | 全部在 service_audio + service_xiaozhi 内；新增 esp_audio_effects 依赖（44.1k→16k 非整数比，不宜线性插值）；BSP/I2S 零改动 | 参考是数字精确的混音出口、延迟为零、M/R 同 I2S 时钟域无漂移；音量在 ES8388 寄存器实现（参考抽取点之后），音量变化需 AEC 短暂再收敛（可接受）；随时可回退 |
| A：TDM 硬回采（原版路线） | RX 重建为 TDM 4 槽 @44.1k，槽 1=MIC3 硬回采 | 收编/替换 BSP 的 bsp_audio（managed component 会被覆盖，需 vendor 进 components/）；44.1k 下 TX-STD+RX-TDM 共享时钟在 P4 未验证（原版只验证过 24k）；首次打开可能有一次可闻爆音 | 最贴近原版、参考含模拟路径真实；手术大、验证成本高，作为 C 跑通后的可选升级 |
| B：MIC1\|MIC3 非 TDM 立体声 | 仅改 mic_selected，右槽可能是 MIC3 | 改动最小 | ES7210 非 TDM 槽序 datasheet 未证实，需真机 5 分钟验证右槽是否跟随扬声器；失败则回退 C/A |

**决策建议：先 C 落地全部体验（全局唤醒 + 打断 + 不静音），验证稳定后再评估是否升级 A。**

### 3.3 提示音资源链路

原版：内嵌 OGG → OggDemuxer → 与 TTS 共用 Opus 解码队列混播（audio_service.cc:710-731）。移植版资源与通路双空白。目标项目方案：提示音做成**内嵌 PCM（44.1k int16 数组，EMBED_FILES）直接写 aux 环形缓冲**（免 OGG/Opus 依赖，复用 `service_audio_aux_write`，xiaozhi_opus.c:185-221 的通路已验证），覆盖 popup（进入聆听）、success（绑定成功）、exclamation（错误）三条最小集；激活码数字播报可暂缓（激活码已有气泡 UI）。资源体积：44.1k×16bit×立体声约 176KB/s，每条提示音 0.3~0.5s 裁剪为 mono 复成立体声可控制在 30~50KB 内嵌。

---

## 4. 重组方案：目标架构

```text
开机
 ├─ task_comm_start() 后周期调用 service_ws_process()
 ├─ task_audio_start() 后周期调用 service_voice_process()
 └─ task_ai_start() → service_xiaozhi_init() → 协议状态机运行

task_comm（Core 0，prio 4，10 ms）
 ├─ service_usb_host_process()
 ├─ engine_midi_process()
 └─ service_ws_process()          ← 通用 WebSocket 事件出队/回调

task_audio（Core 1，最高优先级）
 ├─ service_audio_process()        ← SF2 渲染 + aux 混音 + I2S 输出
 └─ service_voice_process()        ← mic/AEC ref → AFE feed/fetch → Opus 编码/事件

task_ai（Core 0，prio 5）
 └─ service_xiaozhi 协议状态机
     ├─ OTA 激活 / 设备绑定
     ├─ service_ws_connect / send / disconnect
     ├─ service_voice_start_listen / stop_listen / enable_wake
     ├─ 消费 service_voice 事件（WAKE/VAD/上行 Opus 包）
     ├─ 消费 service_ws 事件（hello/tts/stt/alert/mcp）
     ├─ hello / listen / abort 协议
     ├─ MCP 工具分发（回调由 app_ai_agent 注册）
     └─ 120s 入站看门狗 / 空闲超时 / 状态转换校验表

app_ai_agent = 纯 UI（聊天气泡/激活码/收音动效）+ MCP 回调实现（音量/亮度/主题/状态/重启）
service_audio = mic/扬声器解耦：mic_open 不再关扬声器、渲染不停摆、混音出口提供 AEC 参考
```

行为变化声明（需产品层面认可）：改造后设备**开机联网即常驻监听唤醒词**（对齐原版），不再需要先打开 AI App；ADC 常开，功耗略升。

---

## 5. 实施路线图

按依赖排序，每步独立可构建、可验证、可回退（遵循 AGENTS.md：每步构建零错误即 commit）。

**P0-1 RAM 与 AFE 地基（已完成，含两轮真机修正）**
- ~~AFE `MORE_INTERNAL`→`MORE_PSRAM`~~：rev v1.3 芯片 PIE 核访问 PSRAM 必崩，回退并保持 `MORE_INTERNAL`；~~72KB 门槛~~：过低导致溢出即崩，最终定稿**双条件门槛**（总量 ≥96KB 且最大连续块 ≥56KB，附前后 largest-block 诊断日志）；fetch 结果队列 2→4 且满时覆盖最旧；打开失败向 App 投「仅按钮可用」事件；队列存储与音频缓冲外迁 PSRAM（+~14KB 内部空间）；esp-sr 锁定 `~2.4.6`（2.3.1 早于 rev 适配，勿用）。
- 验收：构建零错误（esp-sr 2.4.7 解析成功）；真机日志 `AFE 打开前/后内部 RAM` 双指标打印，AFE 稳定就绪、唤醒可用、无崩溃。

**P0-3 结构性 RAM 解压（已实施）**
- 开 `CONFIG_SPIRAM_XIP_FROM_PSRAM`（对齐上游 P4 默认）：代码从 PSRAM 执行，~98KB IRAM 释放为堆，sram_low 区域变成 ~175KB 连续堆空间，「最大连续块不足」问题连根消失。
- 实施中发现的两件事：① `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY=y` **本就已开启**——>4KB 的任务栈（含 wake_fetch 16KB）实际早已在 PSRAM，也从侧面证实 PIE 核只用 128-bit 向量指令访问张量、普通标量访问（栈溢出保护/寄存器保存）走 PSRAM 无恙；② `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y` 本就已开启，TLS 大结构已在 PSRAM。另 IDF Kconfig 明确 P4 的 flash 与 PSRAM 是**两条独立 SPI 总线**，XIP 后 flash 写窗口内代码可正常执行。
- 风险与回退：sdkconfig 变更已 fullclean 重建；boot 增加数 MB 拷贝耗时；取指与显示 DMA 共享 PSRAM 总线，需真机回归（LVGL 流畅度、SF2 播放、全部 App）。回退仅需把 sdkconfig 该行改回 `is not set`。
- 验收：全量构建零错误；真机——会话入口内部空闲与最大连续块均显著增大（预期空闲 >150KB、连续块 >100KB），AFE 稳定打开、唤醒可用。

**P0-2 独立健壮性补丁（互不依赖，可并行做）**
- mic 泵错误自恢复（STANDBY 下定时重开泵）；非 SPEAKING 态下行音频过滤（service_xiaozhi.c:259 加一行）；WS ERROR/DISCONNECTED 投事件给 App；120s 入站看门狗覆盖 SPEAKING；WS 接收缓冲改 PSRAM 动态（修大 MCP 丢帧）；`xz_set_state` 加 6 态转换校验表；auto 模式 TTS_STOP 等 aux 排空再续听（service_audio 加 `service_audio_aux_is_idle()`）；server_time 改经 `service_rtc_set_time` 统一时间源；MCP 补 `self.get_system_info`/`self.reboot`；VAD 边沿透传 UI。
- 验收：构建零错误；真机过一遍激活→对话→断网→恢复全流程。

**P1 AEC 与全双工（方案 C，核心体验）**
- 新增 esp_audio_effects 依赖；`service_audio_mic_open(44100,1)` 且不再关扬声器；`service_audio_process` 移除录音停摆分支；混音出口加参考抽取（44.1k 立体声 → mono → 16k，PSRAM 环形缓冲）；xiaozhi_wake 改 "MR" + `aec_init=true`（VOIP_HIGH_PERF + NLP VERYAGGR，对齐原版 afe_audio_engine.cc:136-137）；SPEAKING 不停 mic 泵；唤醒命中在 SPEAKING → abort + 清 aux + 续听；输入暖机 120ms。
- 验收：真机——音乐播放中喊唤醒词可唤醒；TTS 播报中喊唤醒词可打断；全程 SF2/提示音不中断。

**P2 常驻会话与体验链收尾**
- 会话与 App 解耦（开机联网即 START，app_ai_agent 移除 stop 调用）；提示音最小集（popup/success/exclamation 内嵌 PCM 走 aux）；唤醒词 2s 缓存上行 + listen detect 消息；激活轮询加上限与 message 展示。
- 验收：不打开任何 App，喊「Hi，喵喵」可拉起对话；断句完整、回答结尾不被掐。

**可选 P3**：方案 A（TDM 硬回采）替换方案 C；realtime 全双工模式（需确认服务端配额）；音频电源管理（电池场景唤醒占空比）。

---

## 6. 真机验证清单（日志关键字）

| 现象 | 看这些日志 |
|:---|:---|
| AFE 是否打开成功 | `AFE 打开前内部 RAM 空闲` / `AFE 打开后内部 RAM 空闲` / `internal RAM too low` |
| 唤醒链路是否活着 | `mic opened`（STANDBY 期应存在）/ `唤醒词命中` / WakeNet 检出打印 |
| 泵死亡 | `mic read error, stop pump` 之后是否再有 `mic opened` |
| 扬声器恢复 | `insufficient DMA memory` / `codec open failed` / 有 `下行音频: 第N帧` 但无声 = 扬声器未恢复 |
| AEC 效果 | 播放音乐时 WakeNet 无误触发；喊唤醒词可命中；上行语音无音乐残留 |
| 打断 | SPEAKING 中命中 → `abort` 发送日志 → `listen start` |
| 时钟 | 激活完成后 `date` 与 RTC 一致性 |

---

## 7. 风险清单

| 风险 | 缓解 |
|:---|:---|
| PSRAM 模式 AFE 在本芯片（rev v1.3）不可行（已实测两轮崩溃定案：PIE 核访问 PSRAM 数据 Store/Load access fault；rev v3.x 芯片无此限制） | 已定案 MORE_INTERNAL + 双条件门槛；余量靠 P0-3 XIP_FROM_PSRAM 与 ws 栈瘦身 |
| 44.1k→16k 重采样质量影响唤醒召回 | 用 esp_audio_effects（原版同款），不用手写线性插值；真机标定增益 |
| 音乐强动态下残余回声致误唤醒/漏唤醒 | `aec_nlp_level` 与 WakeNet 阈值调参；原版同机制已量产验证 |
| 会话常驻后 ADC 常开的功耗 | 后续以唤醒监听占空比方案解决（N 项），非本期目标 |
| mic 44.1k 与 TX 共存触发 esp_codec_dev 边界 bug | agent 已核对同参数直通路径（audio_codec_data_i2s.c:437-457）；真机重点验证首次打开瞬态 |
| 行为变化（开机即监听）产品不接受 | P2 才做解耦，可单独评审；P0/P1 不改变会话生命周期 |

---

*文档生成：2026-07-28。分析基线：当前工作区源码 + build/TAB5_Music_Pad.map（2026-07-28 构建）+ IDF v5.4.4 源码。*
