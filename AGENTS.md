# AGENTS.md

This file provides guidance to AI coding agents when working with code in this repository.

本文件为 AI coding agent 提供项目 overview、构建方式、代码规范、运行时架构与安全注意事项。详细编码规范见 `doc/agent_prompt.md`，构建排错见 `doc/build_prompt.md`，组件结构 / MIDI 总线 / EEZ 生成代码禁令见 `.github/instructions/`。另有专项 agent 配置见 `.github/agents/`，常用 prompt 模板见 `.github/prompts/`。

---

## 1. 常用命令

本项目无传统 lint/test 脚本；验证以 **ESP-IDF 构建零错误 + 真机验证** 为主。开发环境为 Windows，ESP-IDF 在 Git Bash / MSYS 中会被拒绝，必须通过 PowerShell 清空 `MSYSTEM`/`MSYS` 并设置 PATH。

### 1.1 环境变量（PowerShell）

构建前必须确保：

```powershell
$env:MSYSTEM = ''
$env:MSYS = ''
$env:ESP_IDF_VERSION = '5.4'
$env:IDF_PATH = 'S:\Espressif\frameworks\esp-idf-v5.4.4'
$env:PATH = 'S:\Espressif\python_env\idf5.4_py3.11_env\Scripts;S:\Espressif\tools\ninja\1.12.1;S:\Espressif\tools\cmake\3.30.2\bin;S:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin;' + $env:PATH
```

### 1.2 构建

**一键构建并烧录（推荐日常开发）：**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/flash_all.ps1
```

- 默认端口 `COM29`，通过 `-Port COMxx` 指定其他端口。
- 加 `-Monitor` 可在烧录后进入日志监控。

**仅构建：**

```powershell
$env:MSYSTEM = ''
$env:MSYS = ''
$env:ESP_IDF_VERSION = '5.4'
$env:IDF_PATH = 'S:\Espressif\frameworks\esp-idf-v5.4.4'
$env:PATH = 'S:\Espressif\python_env\idf5.4_py3.11_env\Scripts;S:\Espressif\tools\ninja\1.12.1;S:\Espressif\tools\cmake\3.30.2\bin;S:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin;' + $env:PATH
& 'S:\Espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe' 'S:\Espressif\frameworks\esp-idf-v5.4.4\tools\idf.py' build
```

**完整重建（修改 `sdkconfig`、`partitions.csv`、增删组件后必须）：**

```powershell
# 同上环境变量
& 'S:\Espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe' 'S:\Espressif\frameworks\esp-idf-v5.4.4\tools\idf.py' fullclean
& 'S:\Espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe' 'S:\Espressif\frameworks\esp-idf-v5.4.4\tools\idf.py' build
```

**烧录 / 日志监控：**

```powershell
# 烧录
& 'S:\Espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe' 'S:\Espressif\frameworks\esp-idf-v5.4.4\tools\idf.py' -p COM29 flash

# 监控
& 'S:\Espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe' 'S:\Espressif\frameworks\esp-idf-v5.4.4\tools\idf.py' -p COM29 monitor
```

**增量构建：** 若 `build/build.ninja` 已存在，可直接用 `ninja -C build` 增量构建（需同样清空 MSYS 变量），详见 `doc/build_prompt.md` 方案二。

### 1.3 测试

本项目未引入单元测试框架，因此不存在 `run a single test` 命令。每次修改后必须执行上述构建命令，确保 **零错误**（可忽略警告除外）。构建期自动运行的静态检查包括：

- `check_widget_bindings.py`：校验 App 控件绑定与生成代码（screens.h 结构体字段）一致性。
- `gen_i18n.py`：由 `service_i18n/translations.tsv` 生成 i18n 代码。

> 注：`components/app_clock_calendar/cnlunar/host_test/` 下有一个独立的主机端参考对比程序，未接入主构建与 CI，需手动编译运行并对比输出。

### 1.4 辅助脚本

`tools/` 目录下：

| 脚本 | 用途 |
|:---|:---|
| `flash_all.ps1` | 一键构建 + 烧录 + 可选监控 |
| `check_widget_bindings.py` | 校验 `WIDGET_BIND` 与生成代码控件字段一致性（构建期自动调用） |
| `gen_i18n.py` | 由 `translations.tsv` 生成 i18n 代码（构建期自动调用） |
| `extract_chinese.py` | 扫描代码/文档提取中文字符，用于字体生成 |
| `sf2_list_presets.py` | 列出 SF2 音色 bank/program/名称 |
| `sync_ui_bins.py` | 同步 UI 二进制资源：维护 `EMBED_FILES`、VFS 嵌入表 |
| `gen_arch.py` | 生成/刷新 `doc/项目架构.md` 骨架 |

---

## 2. 项目概览

**TAB5_Music_Pad** 是基于 **M5Stack Tab5**（ESP32-P4，RISC-V 360 MHz，32 MB PSRAM，16 MB Flash，5″ 1280×720 MIPI-DSI）的嵌入式音乐固件。

| 属性 | 说明 |
|:---|:---|
| 目标芯片 | ESP32-P4（RISC-V，双核，360 MHz） |
| 框架 | ESP-IDF v5.4.4（`dependencies.lock` / `main/idf_component.yml` 锁定） |
| BSP | `espressif/m5stack_tab5` `1.2.0~1`；依赖 `esp_lvgl_port`、`esp_codec_dev`、`esp_video`、`usb`、`bmi270`、`io_expander` 等 |
| 语言 | C 为主；`engine_sf2`、`service_rtc`、`eez_backend.cpp` 等使用 C++，对外暴露纯 C 接口 |
| GUI | LVGL 9.5 + EEZ Studio 生成 UI（`components/engine_gui/TAB_MusicBox.eez-project` 为单一可信源） |
| 音频 | ES8388 播放 + ES7210 双麦 AEC，44.1 kHz，SF2 采样合成 + aux 混音 |
| 连接 | Wi-Fi/BLE 经板载 ESP32-C6（SDIO/EPPP 透传），USB-A Host + USB-C OTG |
| 分区表 | `partitions.csv`，无 OTA，factory 单分区 |

### 2.0 多板卡适配（board_config）

`components/board_config/`（Kconfig + `include/board_hal.h` + `boards/<board>/` 实现）是板级差异统一出口：

- `CONFIG_BOARD_TYPE_TAB5`（默认）/ `CONFIG_BOARD_TYPE_JC4880P443` 二选一，派生 `CONFIG_BOARD_HAS_POWER_MGMT/BATTERY/HEADPHONE/IMU/RTC/SD/MIC/WIFI/USB_HOST/USB_DEVICE/UART_MIDI`（Tab5 全 y，JC 全 n）。
- Tab5 专属硬件代码用 `#if CONFIG_BOARD_HAS_*` 编译期门控（`service_power`/`service_sd`/`service_usb_host`/`service_usb_device`/`service_input` UART MIDI/`service_rtc`/`app_fun` IMU/`service_wifi` 入口），对外符号在 n 时保留并无害降级；BLE MIDI 用 `CONFIG_ESP_HOSTED_ENABLED` 门控（`service_ble_midi.c` 整体实现 + 降级 stub）。
- 使用 `CONFIG_*` 门控前必须显式 `#include "sdkconfig.h"`（目前经 esp_err.h→esp_compiler.h 间接可得，显式包含防顺序依赖）。
- CMake 组件依赖条件化（如 `bmi270`、`espp__rx8130ce`）：IDF 早期组件展开阶段 Kconfig 不可见，用 `if(NOT CONFIG_BOARD_TYPE_JC4880P443)` 反向判断追加（参考 `board_config/CMakeLists.txt`）。
- 换板后必须 `idf.py fullclean` 重建。
- EEZ 工程保持单一（`components/engine_gui/TAB_MusicBox.eez-project` + `src/ui/`），两板共用同一份生成代码；按板拆分的文件级操作由用户手动执行后再接线。
- JC4880P443 构建（独立构建目录，不干扰 Tab5 的 build/）：`idf.py -B build-jc4880p443 -DSDKCONFIG=<绝对路径>/sdkconfig.boards/jc4880p443 build`（该文件是整板 sdkconfig 快照：板卡切换 + `ESP_HOSTED_ENABLED=n`；改根 sdkconfig 公共配置后需同步刷新）。或在 menuconfig → Board Selection 里切换后 fullclean。
- **Trap（P4 无 WiFi 板）**：P4 的 `esp_wifi_*` 符号只能由 esp_wifi_remote 的劫持层提供，不能简单整体关闭；jc 快照保留 `ESP_WIFI_REMOTE_ENABLED+LIBRARY_HOSTED`、仅关 `ESP_HOSTED_ENABLED`，链接由 `esp_wifi_remote_weak.c` 弱符号桩（返回 NOT_SUPPORTED）收口，运行时经 `BOARD_HAS_WIFI=n` 门控，hosted 构造器不链接、SDIO 不占脚。

### 2.1 分区表

`partitions.csv`：

| Name | Type | Subtype | Offset | Size |
|:---|:---|:---:|---:|---:|
| nvs | data | nvs | 0x9000 | 0x6000 |
| phy_init | data | phy | 0xF000 | 0x1000 |
| factory | app | factory | 0x10000 | 0xB60000 |
| model | data | spiffs | 0xB70000 | 0x80000 |
| storage | data | spiffs | 0xBF0000 | 0x400000 |
| coredump | data | coredump | 0xFF0000 | 0x10000 |

- Flash 配置：16 MB、DIO 80 MHz。
- 无 OTA 分区，固件更新需重新烧录整个 factory 分区。
- `storage` 分区 4 MB，用于 SPIFFS 字体/图片资源；新增资源时必须校验可用空间。
- `model` 分区 512 KB，用于语音唤醒/识别模型。

### 2.2 资源配置

UI 字体/图片二进制资源（`components/engine_gui/src/ui/*.bin`）通过 `EMBED_FILES` 直接内嵌进固件，运行时经自定义 VFS 从 Flash XIP 读取，不再依赖 `prepare_spiffs.py` 收集到 SPIFFS（该脚本已移除）。

- `engine_gui` 初始化时注册自定义 VFS 挂载点 `/sys/src`；`service_spiffs_init()` 将 `storage` 分区挂载到 `/sys_int`。
- 打开 `/sys/src/xxx.bin` 时，优先尝试 SD 卡 `/sdcard/sys/src/xxx.bin`。
- 5 个字库 bin（`chinese_30`、`chinese_40`、`icon_70`、`clock_150`、`clock_150_a`）经 `EMBED_FILES` 内嵌进固件，VFS 直接从 Flash XIP 读取。
- 其余文件回退到 SPIFFS `/sys_int/src/xxx.bin`。
- 新增/删除 `.bin` 资源后必须运行 `tools/sync_ui_bins.py` 并执行 `idf.py fullclean` + 重建。

### 2.3 CI / 发布

`.github/workflows/release.yml`：

- 触发条件：`v*` 标签 push 或手动触发；release tag 必须符合 `va.b.c`（CI 校验，不合规直接失败）。
- 使用容器 `espressif/idf:v5.4.4` 执行 `idf.py build`；tag 构建经环境变量 `FW_VERSION_OVERRIDE=<tag>` 显式注入版本号（不用 `-D`，避免残留 CMakeCache 污染后续构建）。
- **红线：容器内必须显式 `git config --global --add safe.directory`**。容器内 run 步骤的 HOME 与 checkout（node 动作）不同，缺失时构建期 git 全部报 dubious ownership，版本号静默退化为 `unknown`（2026-08 教训：v1.0.1 release 固件关于页显示 unknown）。
- 构建成功后合并 16 MB 完整 Flash 镜像并上传 artifact / GitHub Release。
- CI 无测试步骤，仅验证构建。

`.github/workflows/pages.yml`（在线烧录站 `doc/web_flash/`）：

- **红线：GitHub release 资产无 CORS 头，页面不能跨域 fetch**。部署时经 curl 把最新 release 的固定文件名固件（`0x0_full_*.bin` / `0x10000_app_*.bin`）镜像进站点目录，页面同源拉取；`.bin` 已被 `.gitignore` 排除，不入库。
- 触发：烧录页代码 push、`Build and Release` 工作流成功完成（`workflow_run`；action-gh-release 的 GITHUB_TOKEN release 事件不触发其他工作流，不能靠 `release` 触发）、手动。
- 页面为自研 esptool-js@0.6.1 单页（非 esp-web-tools）：默认只读固件区 + 整机/仅 App 下拉自动填充，勾选高级模式后退化为通用烧录器；`manifest_*.json` 保留供 esp-web-tools 兼容使用，路径为同源相对路径。

版本号规则（顶层 `CMakeLists.txt` 唯一计算，经 `FW_VERSION` cache 变量下发组件）：

- HEAD 有 `v*` tag → 版本 = tag（如 `v1.0.1`）；无 tag → 7 位 commit hash；工作区脏追加 `-dirty`（CI 检出必干净，仅本地出现）。
- 关于页显示与 `main.c` 启动日志均使用编译宏 `FIRMWARE_VERSION`；构建日期取 `esp_app_desc_t.date`。
- git 完全不可用（如源码 zip 构建）→ 版本为 `unknown` 并在构建期输出 WARNING。
- `FW_VERSION_OVERRIDE` 仅在 cmake configure 时读取；本地用过 override 后须 `idf.py reconfigure`（或 fullclean）才能回到 git 版本。

---

## 3. 架构与运行时

### 3.1 分层架构

```
┌─────────────────────────────────────────┐
│ UI Frontend（EEZ 生成 + engine_gui）     │
├─────────────────────────────────────────┤
│ App Layer（12 个 P0 App）                │
├─────────────────────────────────────────┤
│ App Manager（生命周期、输入/音频路由）    │
├─────────────────────────────────────────┤
│ Task 层（胶水层，禁止业务逻辑）            │
├─────────────────────────────────────────┤
│ Services（直接调用 BSP/API）              │
├─────────────────────────────────────────┤
│ Engines（纯算法/协议层）                   │
├─────────────────────────────────────────┤
│ BSP（espressif/m5stack_tab5）             │
└─────────────────────────────────────────┘
```

层间依赖规则：

| 调用方向 | 允许 | 禁止 / 例外 |
|:---|:---|:---|
| App → Engine / AppManager | ✅ | App 不得直接调用 Service/BSP |
| Service → BSP / 其他 Service | ✅ | Service 不得调用 App/Engine |
| Engine → 无（纯算法/协议） | ✅ | Engine 不得调用 Service/BSP；`engine_gui` 因显示/触摸需要为例外 |
| Task → Service / Engine / AppManager | ✅ | Task 只作胶水调用，禁止包含业务逻辑 |

12 个已注册 P0 App（`components/app_manager/app_manager.c:914`）：

`app_zen`、`app_ear_trainer`、`app_circle_of_fifths`、`app_chord_trainer`、`app_xy_pad`、`app_drum_pad`、`app_tiny_piano`、`app_clock_calendar`、`app_ai_agent`、`app_midi_player`、`app_metronome`、`app_fun`。

### 3.2 启动顺序

`main/main.c` 中 `app_main()` 按阶段初始化，顺序错误会导致内存或总线问题：

1. `service_power_init()` → 10%
2. `engine_midi_init()` → `app_manager_init()` → `app_manager_register_all()`
3. `service_sd_init()` → `service_i18n_init()` → `service_recorder_init()` → `service_usb_host_init()` → 20%
4. `engine_gui_init()` → `service_page_init()` → 30%
5. 启动 `task_gui` / `task_comm` / `task_app` → 40%
6. `task_input_start()` → `service_audio_init()` → 50%
7. `task_audio_start()` → `service_voice_init()` → `engine_sf2_register_source()` / `service_audio_activate_sf2()` → 60%
8. `service_rtc_init()` → 70%
9. `service_wifi_boot()` → `service_http_client_init()` → `service_ws_init()` → `task_ai_start()` → 80%
10. `service_input_init()` → 90%
11. `service_power_idle_set_enabled(true)` + 版本日志 → 100%

关键约束：

- **音频/语音前端必须在 Wi-Fi 之前就绪**：AFE 需约 110 KB 内部 RAM，Wi-Fi 启动后仅剩约 32 KB。
- **`service_usb_host_init()` 提前到 Wi-Fi 前**：其任务栈需落内部 RAM。
- **`task_audio` 必须在 AFE 打开前创建**：AFE 打开后内部 RAM 碎片化，会导致任务创建失败。
- **SF2 加载可能超过 10 秒**：`main.c` 在加载期间临时放宽看门狗并周期性 `vTaskDelay` 喂狗。
- **网络为可选功能**：离线音乐必须可用，网络初始化失败全部降级。
- **注册完 App 后 `main.c` 不主动启动任何 App**：App 切换全部走 `app_manager_request_*()` 异步请求。

### 3.3 任务与调度

| 任务 | 优先级 | 核心 | 栈大小 | 周期 | 职责 |
|:---|:---:|:---:|:---:|:---:|:---|
| `task_gui` | 10 | 0 | 8192 B | 10 ms | LVGL tick/刷新 |
| `task_input` | 7 | 0 | 8192 B | 10 ms | 输入处理 + 触摸分发 |
| `task_app` | 4 | 0 | 12288 B | 10 ms | App 生命周期、`app_manager_process_requests()`、各路 `service_*_process()` |
| `task_comm` | 4 | 0 | 16384 B | 10 ms | USB Host、`engine_midi_process()`、`service_ws_process()`（含 TTS Opus 解码回调）、`service_ftp_process()`（FTP 状态机轮询） |
| `task_ai` | 5 | 0 | 24576 B | 命令队列 | 小智协议状态机 |
| `task_audio` | 最高 | 1 | 24576 B | 死循环 | 音频渲染 + 语音前端 |

FreeRTOS tick 1000 Hz，双核。`task_comm` 栈扩至 16 KB 以容纳 Opus/CELT 解码链；`task_ai` 栈 24 KB。`task_audio` 必须运行在 Core 1 最高优先级并使用内部 RAM。

### 3.4 MIDI 事件总线

`engine_midi` 是 App、MIDI 输入、合成器之间的核心通信机制。

- 事件队列长度 128，最大消费者数 16。
- 来源端口：`INTERNAL` / `UART` / `USB_HOST` / `BLE` / `USB_DEVICE` / `APP`。
- App 发声统一走总线：`engine_midi_publish_*(..., ENGINE_MIDI_PORT_APP, ...)`，由 `engine_sf2` 订阅消费。
- 内部 SysEx 帧格式：`[cmd][func][p1][p2]`，无 vendor id。
  - `MIDI_CMD_APP` (cmd=1)：App 启动/返回。
  - `MIDI_CMD_INPUT` (cmd=3)：输入事件 —— `func=0` 触摸、`func=2` 鼠标、`func=3` 键盘。
  - `MIDI_CMD_APP_CONTROL` (cmd=7)：App 控制，转发给当前 App 的 `on_sysex()`。

`app_manager` 订阅 SysEx，把 `MIDI_CMD_INPUT` 解析为 `app_input_event_t` 喂给当前 App，把 `MIDI_CMD_APP_CONTROL` 转发给 `on_sysex()`。App 发出的 `MIDI_CMD_APP_CONTROL` 不会被自身消费（防循环）。

### 3.5 App 生命周期

`app_manager` 维护最多 32 个 App 的注册表（`APP_MANAGER_MAX_APPS`）。切换 App 使用异步接口：

- `app_manager_request_launch(name)` / `request_kill_active()` 仅登记命令。
- `task_app` 每周期调用 `app_manager_process_requests()` 执行实际的 `launch`/`kill`。
- `app_manager_request_launch_by_screen(screen_name)` 按注册 App 的 `screen_name` 匹配唤醒。
- 生命周期回调（`on_init`、`on_render`、`on_update`、`on_pause`、`on_resume`、`on_destroy`、`on_input`、`on_sysex` 等）受递归互斥锁保护，防止并发访问。
- **Trap：LVGL 事件回调跑在 task_gui，不在该锁内**，不得直接触碰 App 的解析/播放状态。范式（2026-08 app_midi_player 空指针 panic 修复）：LVGL 回调只登记到 SPSC 请求环（`player_post_request`），由 `on_update`（task_app，锁内）串行消化（`player_drain_requests`）；整文件解析（数秒级）期间点击不再并发踩内存。
- 每个 App 必须实现 `app_base_t base` 作为结构体第一个字段，并通过 `app_manager_register()` 注册。

### 3.6 音频路由

`service_audio` 采用混音架构：

- 主音源：`engine_sf2`（SF2 采样合成器），注册为 `AUDIO_SOURCE_SF2`。
- 第二通道：`service_audio_aux_write()`（44.1 kHz 立体声 int16，SPSC 环形缓冲 @PSRAM），小智 TTS 下行语音等由此混入。
- 主源与 aux 在 float 域混音，限幅统一收口于混音出口。
- `task_audio` 运行在 Core 1 最高优先级，混音后写入 ES8388。
- Aux 缓冲区约 2 秒深度，并设有预充电阈值（约 400 ms）以吸收网络抖动。
- xiaozhi 下行 TTS：ws 二进制帧在 `task_comm`（`service_ws_process()` 分发）上下文经裸 libopus `opus_decode` 整包解码、线性插值重采样到 44.1 kHz 后写 aux。服务器实际按 BinaryProtocol3（4 字节头）封包下发；ws 握手必须带 `Protocol-Version: 1` 头，且接收侧保留 v3 防御性剥离，否则封包头被误当 opus TOC，TTS 只剩噪声碎片。
- 下行反压链（防服务器突发丢音频）：`xz_on_ws_audio` 在 aux 剩余空间不足一个最长包（`XZ_AUX_BACKPRESSURE_FREE_FRAMES`）时等播放端排水（10ms 切片、上限 `XZ_AUX_BACKPRESSURE_MAX_MS`，期间离开 SPEAKING 立即丢帧防残留）；积压倒灌回 ws 事件队列（64 深，编码包 ~200B/60ms 是最廉价缓冲层）；队列满后 ws 任务入队阻塞（`SERVICE_WS_EVT_SEND_BLOCK_MS`）经 TCP 接收窗口反压服务器。队列满丢包告警已按 1s 窗口限流，逐条刷屏会进一步挤占 CPU。
- aux 预充门与流尾（2026-08 尾音残留修复）：欠载即重新预充（攒够 ~400ms 再出声），但"流已结束"时尾音不足门限会永久卡门、跨轮残留成下轮开头插播。`tts_stop` 时 `service_audio_aux_end_of_stream()` 一次性解除预充放尾音（置位式，Core 1 消费侧应用，`s_aux_priming` 保持单写者）；auto 续听 2500ms 兜底命中时 `service_audio_aux_clear()` 清场；按钮打断与唤醒打断均已对齐 aux_clear。

### 3.7 AI 对话 LED 指示器（xx_led_ai）

14 个外部屏（launcher/setting/about/zen/ear/chord/fifth/piano/drum/midi/xy/metron/clock/fun）在 EEZ 工程中各放置了一个 `xx_led_ai`（lv_led，默认坐标 620,677，尺寸 40x35；主题 secondary 色由生成代码上色）。

- 显隐与动画由 `engine_gui.c` 的 `engine_gui_ai_led_tick()` 驱动：挂在 `engine_gui_tick()` 的 LVGL 锁内，每 10 ms 轮询 `service_xiaozhi_get_state()`。
- LED 控件经 `engine_gui_find_widget()` 按名懒解析并缓存；EEZ 重导出若丢控件，仅告警并表现为该屏无 LED，不产生编译错误（2026-08 教训：导出漂移曾致 14 个控件丢失）。
- 换屏以 `lv_screen_active()` 比对检测；新屏 LED 一律先复位隐藏（"页面默认刷新时隐藏"由该路径保证）。
- 会话进入 CONNECTING/LISTENING/SPEAKING：LED 自屏幕底部外上滑入场，会话期小幅漂浮 + 尺寸脉动；回到 READY/IDLE：下滑出屏后隐藏。
- AI 屏（app_ai_agent）无此控件，对话状态由聊天气泡与通知栏表达。
- `AI_LED_HOME_*` 等常量与 EEZ 摆放耦合，EEZ 改动后需同步。

### 3.8 显示刷新管线

面板物理 720×1280（竖），UI 逻辑 1280×720（横），旋转由 LVGL 管理：`lvgl_port_add_disp_dsi` 之后调用 `lv_display_set_rotation()`，port 经 RESOLUTION_CHANGED 缓存 rotation 并在 flush 中执行软件旋转。

- 官方 esp_lvgl_port 配方：部分缓冲（720×50 双缓冲，PSRAM，`CONFIG_BSP_LCD_DRAW_BUF_HEIGHT=50`）+ `sw_rotate` + `esp_lcd_panel_draw_bitmap` 中转进 DSI 单帧缓冲（PSRAM，上电清零防花屏）；`LV_DEF_REFR_PERIOD=33`。部分模式整屏切换是逐块推进的，块越高单块突发越长、推进感越短。
- **红线：块高 50 是实测 underrun 安全上限，禁止调大**。DPI 需持续 ~120 MB/s 读 PSRAM 帧缓冲；本项目三块缓冲（双 draw buf + 旋转中转 buf，port 用同一 buff_caps 分配）全在 PSRAM，单块 PSRAM 流量约为官方内部 RAM 方案的 4~5 倍。块高 ≥100 时 rotate+memcpy+msync 连续突发超过 DPI FIFO 容忍度，大面积刷新必现 underrun 闪屏（2026-08 真机：150 必现、100 必现、50 稳定）。
- **红线：禁止 `CONFIG_BSP_LCD_USE_DMA2D`**（BSP 默认 y，必须显式 not set）。DMA2D 把 draw_bitmap 变成异步 GDMA 2D 突发拷贝，CPU 同时旋转下一块，双主并发挤占 PSRAM；PSRAM 驻留缓冲下真机全屏必闪。官方敢开是因为其中转 buf 在内部 RAM，GDMA 只写不读 PSRAM。
- **红线：禁止回到 DIRECT + PPA 直写帧缓冲**。BLOCKING PPA 传输独占 PSRAM 总线，与 DSI DMA 持续读帧缓冲竞争，大面积刷新必现 DPI underrun 闪屏（2026-08 真机确认，该路径已拆除）。
- 缓冲放 PSRAM 而非官方默认的内部 DMA RAM：官方 `buff_dma=true` 需 3×72KB 内部 RAM，本项目内部 RAM 预算被 AFE(~110KB)/WS TLS(~50KB) 占满（运行时 free ~74KB），只能 PSRAM——这也是必须守上面两条红线的原因。刷新周期调到 16ms 对 underrun 无影响（突发长度才是主因），保持默认 33。
- 触摸：`multi_touch_read_cb` 上报物理原生坐标，LVGL 按 display rotation 自动换算；`gui_publish_touch` 的 App 输入事件经 `touch_to_logical` 手动转换（随反向开关分 90°/270° 两式）。
- 显示反向硬开关：设置项 `setting_invert_display` → NVS `SERVICE_NVS_FLAG_INVERT_DISPLAY`（bit 8）→ `engine_gui_set_display_inverted()` 即时切换 `ROTATION_90/270`；开机由 `engine_gui_init` 读取应用。渲染与触摸均由 LVGL 旋转自动联动，App 无感。

### 3.9 FTP 无线文件管理（service_ftp）

`components/service_ftp/` 是 lwIP 原生单会话 FTP 服务器（协议语义参考 `tools/SimpleFTPServer`，MIT；socket/文件层为本项目非阻塞轮询模式重写），管理 `/sdcard` 整卡（虚拟路径沙箱，`..` 逃逸回 550）。

- **无独立任务**：`service_ftp_process()` 挂 `task_comm` 10 ms 循环，全 O_NONBLOCK socket + 回包 pending 缓冲，数据通道每拍 ≤4 块 × 8 KB（PSRAM 缓冲，start 时 `heap_caps_malloc(MALLOC_CAP_SPIRAM)`），严禁忙等。仅被动模式（PASV/EPSV，ephemeral 端口），固定凭据 `musicpad/musicpad`，仅局域网 STA 场景。
- **独占系统屏**：`ftp` 屏（EEZ）走 service_page 体系（`service_page_ftp.c`），非 App。设置页 `setting_btn_ftp` → `engine_gui_switch_screen("ftp")`；`engine_gui_on_screen_loaded` 的 ftp 分支**不带 return**，落尾部自动 kill active App 完成独占。进入时 `service_power_idle_set_enabled(false)` + `service_xiaozhi_set_suspended(true)` + `service_ftp_start()` + 双锁；退出（`ftp_btn_back2setting`）随时可点：stop（中止传输断客户端）→ 解锁 → 恢复熄屏/语音 → 回 setting。
- **双锁 API**：`app_manager_set_launch_locked()`（guard `app_manager_launch`，kill 不锁——独占屏进入靠异步 kill 清场）与 `engine_gui_set_screen_locked()`（guard `engine_gui_switch_screen`）；退出路径先解锁再切屏。
- **AI 暂停**：`service_xiaozhi_set_suspended(true)` 停止活跃会话（异步收尾回 IDLE，ws 断开释放 socket/TLS）并关唤醒检出；`xz_enter_standby`/`process`/`set_wake_anywhere`/`set_ai_ui_active` 等 re-arm 点全部被 `s_suspended` 门控，唤醒事件直接丢弃。
- **socket 预算**：`CONFIG_LWIP_MAX_SOCKETS=10`；常开 httpd 占 2，FTP 占 4（cmd listen/cmd/pasv listen/data），AI 暂停期 ws 不占。
- **板级门控**：`CONFIG_BOARD_HAS_WIFI=n` 时整体 stub 降级（start 返回 `ESP_ERR_NOT_SUPPORTED`）。
- **已知边界**：中文长文件名需 `CONFIG_FATFS_API_ENCODING_UTF_8=y`（当前 CP437，非 ASCII 名经 POSIX 层读出为乱码，FTP 原样透传）。STOR 上传长度服务端不可预知（status `file_size=0`），进度条锯齿滚动仅指示活跃，真实进度看 label 已传字节；下载（RETR）按百分比正常显示。

---

## 4. 代码规范

### 4.1 红线：禁止修改 EEZ Studio 生成代码

`components/engine_gui/src/ui/` 下所有文件（`vars.h/c`、`ui.h/c`、`screens.h/c`、`styles.h/c`、`eez-flow.h/cpp`、`images.h/c`、`fonts.h/c`、`actions.h/c`、`structs.h` 等）由 EEZ Studio 工程 `components/engine_gui/TAB_MusicBox.eez-project` 生成，是单一可信源。

- **严禁手动修改**，即使是为了修复编译错误。
- 若生成代码导致构建失败，立即停止修改，向用户报告错误、文件路径和原因，等待用户在 EEZ Studio 修复或取得明确书面授权后方可临时 patch。
- 临时 patch 必须在 commit message 中注明授权与修改范围。
- EEZ 工程中 `fileSystemPath` 必须保持 `/sys/src`；App 中的 `WIDGET_BIND` 名称必须与生成代码 `src/ui/screens.h` 的结构体字段完全一致，构建期由 `check_widget_bindings.py` 校验。

### 4.2 组件目录结构

标准布局：

```
components/<module_name>/
├── CMakeLists.txt
├── include/
│   └── <module_name>.h
└── <module_name>.c          # 或 .cpp / <module_name>_extra.c
```

`CMakeLists.txt` 模板：

```cmake
idf_component_register(
    SRCS "<module_name>.c"
    INCLUDE_DIRS "include"
    REQUIRES <deps>
)
```

### 4.3 代码风格

- **注释**：使用中文；只保留 **Why / Trap / Contract** 三类；禁止「过程描述型」注释（如"首先…然后…"）。
- **缩进**：4 空格；**大括号**：K&R 风格，控制语句必须加大括号。
- **命名**：公共 C API 使用 snake_case，模块前缀统一：`engine_` / `service_` / `app_` / `task_` / `app_manager`。
- **头文件保护宏**：全大写，如 `APP_MANAGER_H`。
- **日志**：每个 `.c`/`.cpp` 顶部定义 `static const char *TAG = "module_name"`，使用 `ESP_LOG*`。周期性遥测/逐包诊断（`[dbg]` 前缀、每 3 秒统计、逐帧日志）统一 `ESP_LOGD`，默认 INFO 级不输出；状态迁移、对话内容、告警保留 INFO/WARN。
- **错误处理**：业务代码避免 `ESP_ERROR_CHECK`，应显式检查返回值并返回 `esp_err_t`/`bool`；失败不得静默忽略。
- **C/C++ 混合**：公共头文件含 `extern "C"`；C++ 导出函数显式 `extern "C"`。
- **局部变量**：在函数开头声明。
- **文件内容顺序**：
  1. `#include`
  2. `static const char *TAG = "module_name"`
  3. `#define`
  4. `static` 变量
  5. `static` 函数前向声明
  6. 全局函数
  7. `static` 函数

### 4.4 Git 提交规范

- 构建通过（零错误，可忽略警告除外）后必须立即 `git add -A` + `git commit`。
- 提交信息使用中文，简洁描述变更点与验证结果。
- `components/engine_gui/` 下的 EEZ 工程文件、生成代码、字体/图片资源一旦变更，必须与代码一起提交。
- 不主动 `git push`；禁止 `git reset`/`rebase`/`checkout -f`/`clean -fd` 等危险操作，除非用户明确授权。

---

## 5. 安全与注意事项

| 风险点 | 说明 |
|:---|:---|
| **SoftAP 默认密码** | `components/service_wifi/service_wifi_ap.c:38-39` 硬编码 AP SSID `HammySetup`、密码 `12345678`；`components/service_page/service_page_onboard.c:321` 与 `service_page_setting.c:185` 的二维码直接暴露该密码。 |
| **HTTP 配网明文传输** | `service_wifi_ap.c` 通过 HTTP POST 接收 SSID/密码并写入 NVS，无 HTTPS/TLS。 |
| **NVS 明文存储** | `components/service_nvs/include/service_nvs.h:125-133` 中 Wi-Fi 凭据、xiaozhi uuid/ws_url/ws_token 以明文形式保存在 NVS。 |
| **小智默认服务端** | `components/service_xiaozhi/include/service_xiaozhi_config.h:14` 硬编码 `https://api.tenclass.net/xiaozhi/ota/`。 |
| **单 USB PHY** | `components/service_input/service_input.c:37-45`：Host 与 Device 不可同时工作。 |
| **分区空间** | 无 OTA；`storage` 固定 4 MB，新增资源时需验证空间。 |
| **C6 提前上电** | `components/service_power/service_power.c:219` 中 `service_power_c6_early_enable()` 在调度器启动前操作 GPIO，不能调用 FreeRTOS API；仅 `CONFIG_BOARD_HAS_POWER_MGMT=y` 时编译。 |
| **音频任务独占 Core 1** | `components/task_audio/task_audio.c` 运行在 Core 1 最高优先级。 |
| **MIDI 录音** | `components/service_recorder/service_recorder.c:24` 将 MIDI 流以明文 `.hmr` 写入 SD 卡 `/record/`，路径与内容未经加密。 |
| **FTP 明文服务** | `components/service_ftp/service_ftp.c:43-44` 固定凭据 `musicpad/musicpad`，FTP 协议明文传输，仅面向路由器级局域网（FTP 屏独占期间才监听 21 端口，退出即关）；严禁暴露公网。 |

---

## 6. 参考文档

- `doc/agent_prompt.md`
- `doc/build_prompt.md`
- `.github/instructions/component-pattern.instructions.md`
- `.github/instructions/midi-bus.instructions.md`
- `.github/instructions/eez-generated-code.instructions.md`
- `.github/agents/` 与 `.github/prompts/`

*本文件基于项目实际内容整理，代码变更后应同步更新。*
