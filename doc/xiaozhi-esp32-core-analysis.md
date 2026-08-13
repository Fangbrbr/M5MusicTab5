# 小智 AI（xiaozhi-esp32）核心流程学习总结

> 本文档基于 `tools/xiaozhi-esp32` 源码逐文件精读整理（2026-07-28 复核版），目标是为将该项目的核心能力（离线语音唤醒 + 基于 WebSocket 的流式语音对话）提取成可插拔的最小模块做知识储备。
> 不涉及的领域：固件 OTA 升级、资源（assets）下载、MQTT 传输后端、多板型适配、显示/LED/相机等纯产品交互层。
> 文中所有结论均标注 `文件:行号`，路径相对 `tools/xiaozhi-esp32/`。提取目标是 ESP32-P4（M5Stack Tab5），因此以 `AfeAudioEngine` 路线为准，`LiteAudioEngine` 仅作对比。

---

## 1. 目标与范围

### 1.1 原项目核心体验

1. **设备声明注册与绑定**：设备联网后向服务器 POST 声明自己；未绑定设备收到 6 位激活码并展示/播报，用户在小程序输入后完成绑定，服务器下发 WebSocket URL 与鉴权 token。
2. **离线语音唤醒**：设备常驻监听麦克风（AFE + WakeNet），命中唤醒词后进入对话流程。
3. **流式语音对话**：唤醒后建立 WebSocket 音频通道，上行 Opus 语音流（16 kHz/60 ms 帧），下行 Opus TTS 流（默认 24 kHz）加状态/文本 JSON 消息。
4. **核心状态机**：有限状态机管理 `Starting → Activating → Idle → Connecting → Listening → Speaking` 等状态，统一驱动音频与协议。
5. **MCP 控制**：设备作为 MCP Server，向云端 AI 暴露工具（音量、亮度、设备状态等），经 WebSocket `type:"mcp"` 的 JSON-RPC 2.0 消息调用。

### 1.2 需要保留的最小能力与源码对应

| 能力 | 对应源码 |
|:---|:---|
| 设备声明/激活/绑定 | `main/ota.cc`、`main/application.cc`（ActivationTask/CheckNewVersion） |
| WebSocket 连接与 hello 握手 | `main/protocols/websocket_protocol.cc`、`main/protocols/protocol.cc` |
| 上行 Opus 音频流 | `main/audio/audio_service.cc`（编码通路） |
| 下行 Opus 解码播放 | `main/audio/audio_service.cc`（解码通路）+ `ogg_demuxer`（提示音） |
| 离线唤醒 + VAD + AEC | `main/audio/engines/afe_audio_engine.cc` |
| 核心状态机 | `main/device_state_machine.cc`、`main/application.cc` |
| MCP 工具注册与调用 | `main/mcp_server.cc` |
| 配置持久化 / 标识 | `main/settings.cc`、`main/system_info.cc`、`main/boards/common/board.cc` |

---

## 2. 整体架构与任务模型

### 2.1 顶层对象关系

```text
app_main()  (main/main.cc:14-29)
 └── Application（单例，main/application.cc）
      ├── DeviceStateMachine        // 状态转换校验 + 观察者广播
      ├── AudioService              // 音频采集/编码/解码/播放总控
      │    ├── AudioCodec 抽象       // ES8388 / ES8311 / dummy ...
      │    ├── AudioEngine
      │    │    ├── AfeAudioEngine  // ESP32-S3/P4：AFE(AEC/VAD) + WakeNet/MultiNet
      │    │    └── LiteAudioEngine // 低端芯片：独立 EspWakeWord
      │    └── esp_opus 编解码器 + esp_ae 重采样器
      ├── Protocol（抽象基类）
      │    └── WebsocketProtocol    // 精简版唯一需要的实现
      ├── McpServer（单例）          // MCP 工具注册与 JSON-RPC 处理
      └── Ota                       // 设备声明/激活（升级部分可裁剪）

Board（单例）提供硬件抽象：Network / AudioCodec / Display / LED / SystemInfo / UUID
——这是与产品层耦合的总入口，提取时是主要替换面。
```

### 2.2 任务与执行上下文全景（精确版）

| 任务/上下文 | 栈（字节） | 优先级 | 核心 | 创建处 | 职责 |
|:---|---:|---:|:---:|:---|:---|
| `app_main`（主事件循环） | IDF 默认 | **10**（`Run()` 开头 `vTaskPrioritySet(nullptr,10)`，application.cc:168-170） | — | IDF | `Application::Run()` 事件循环 |
| `activation` | 8192 | 2 | — | application.cc:287-294 | OTA 检查/激活/初始化协议，完成后自删 |
| `tcp_receive`（每条 TCP 连接一个） | 4096 | 1 | — | esp-ml307 组件 `esp_tcp.cc` | 阻塞 recv，**所有下行 JSON 解析、Opus 包构造、server hello 置位、被动断开回调都跑在这里** |
| `audio_input` | 6144（开 PROCESSOR） | 8 | **Core 0 钉核** | audio_service.cc:131-135 | 10 ms 节拍读 mic、重采样、Feed AFE |
| `audio_output` | 4096 | 4 | — | audio_service.cc:138-142 | 播放队列 → codec |
| `opus_codec` | **24576** | 2 | — | audio_service.cc:160-164 | Opus 编码 + 解码（同一任务两个分支） |
| `audio_afe` | 4096 | 3 | — | afe_audio_engine.cc:173-177 | 阻塞 `fetch_with_delay` 跑 AFE 推理 |
| `encode_wake_word`（按需） | 24576~28672 @PSRAM | 2 | — | afe_audio_engine.cc:479-556 | 唤醒命中后把 2 秒缓存编码为 Opus |
| `clock_timer` | esp_timer 服务任务 | — | — | application.cc:35-45 | 1 秒周期，回调仅置 `MAIN_EVENT_CLOCK_TICK` |
| `audio_power_timer` | esp_timer 服务任务 | — | — | audio_service.cc:109-127 | 1 秒周期，输入/输出空闲超 15 s（`AUDIO_POWER_TIMEOUT_MS`，audio_service.h:47）关 ADC/DAC |

**对「单任务化」提取的启示**：协议层回调天然跑在独立的 `tcp_receive` 任务上下文（优先级仅 1），状态变更全部经 `Schedule()` 投回主任务串行执行——这个「回调只做搬运、主循环串行消费」的模型正是可以压缩成单任务的结构：让网络回调只拷贝入队，把 JSON 解析/音频入队/状态机全部放进唯一的工作任务循环。

### 2.3 主事件循环机制

`Application::Run()`（application.cc:172-273）：

```cpp
xEventGroupWaitBits(event_group_, ALL_EVENTS, pdTRUE /*退出时清位*/,
                    pdFALSE /*任一位即可*/, portMAX_DELAY);   // :180
```

14 个事件位（application.h:22-35，位 0~13）：

| 位 | 值 | 置位者 | 处理要点 |
|:---|---:|:---|:---|
| `MAIN_EVENT_SCHEDULE` | 1 | `Schedule()`（:1006） | 取出 `main_tasks_` 顺序执行（:251-258） |
| `MAIN_EVENT_SEND_AUDIO` | 2 | 音频回调 `on_send_queue_available` | `PopPacketFromSendQueue`→`protocol_->SendAudio`；发送失败清空剩余队列防 Opus 任务死锁（:226-238） |
| `MAIN_EVENT_WAKE_WORD_DETECTED` | 4 | `on_wake_word_detected` | `HandleWakeWordDetectedEvent()` |
| `MAIN_EVENT_VAD_CHANGE` | 8 | `on_vad_change` | 仅刷新 LED（:244-249） |
| `MAIN_EVENT_ERROR` | 16 | 协议 `OnNetworkError` | `SetDeviceState(Idle)` + Alert（:182-186） |
| `MAIN_EVENT_ACTIVATION_DONE` | 32 | ActivationTask 末尾 | `HandleActivationDoneEvent()` → Idle |
| `MAIN_EVENT_CLOCK_TICK` | 64 | clock_timer | 状态栏刷新；每 10 tick 打印堆统计（:260-271） |
| `MAIN_EVENT_NETWORK_CONNECTED` | 128 | Board 网络回调 | `HandleNetworkConnectedEvent()` |
| `MAIN_EVENT_NETWORK_DISCONNECTED` | 256 | Board 网络回调 | 对话状态下 `CloseAudioChannel()` |
| `MAIN_EVENT_TOGGLE_CHAT` | 512 | `ToggleChatState()` | 按键/触摸切换对话 |
| `MAIN_EVENT_START_LISTENING` | 1024 | `StartListening()` | 强制 manual 模式监听 |
| `MAIN_EVENT_STOP_LISTENING` | 2048 | `StopListening()` | |
| `MAIN_EVENT_STATE_CHANGED` | 4096 | 状态机 listener | `HandleStateChangedEvent()` 副作用 |
| `MAIN_EVENT_PLAYBACK_DRAINED` | 8192 | `on_playback_drained` | auto 模式延迟启动监听（:204-212） |

循环体固定处理顺序：**ERROR → NET_CONNECTED → NET_DISCONNECTED → ACTIVATION_DONE → STATE_CHANGED → PLAYBACK_DRAINED → TOGGLE_CHAT → START/STOP_LISTENING → SEND_AUDIO → WAKE_WORD → VAD → SCHEDULE → CLOCK_TICK**。`STATE_CHANGED` 只读取当前态，一轮内多次迁移会被合并。

**`Schedule()` 机制**（application.cc:1001-1007）：`std::deque<std::function<void()>> main_tasks_` + `std::mutex`，**无容量上限**；任意任务上下文可调用，压栈后置 `MAIN_EVENT_SCHEDULE`；主任务持锁整体 move 出队列再逐个执行——所有跨线程动作（UI、状态迁移、MCP 工具回调）借此串行化到主任务。

---

## 3. 设备声明、注册与绑定流程

### 3.1 整体流程

```text
网络连接成功（HandleNetworkConnectedEvent, application.cc:275-300）
  └── 状态 Starting/WifiConfiguring → SetDeviceState(kDeviceStateActivating)
  └── xTaskCreate("activation", 栈 8192, 优先级 2)
       └── ActivationTask()（application.cc:340-355）
            ├── ota_ = make_unique<Ota>()
            ├── CheckAssetsVersion()      // assets 分区资源（可整段裁剪）
            ├── CheckNewVersion()         // 核心：设备声明 + 激活码 + 轮询绑定
            ├── InitializeProtocol()      // 创建 WebsocketProtocol
            └── 置 MAIN_EVENT_ACTIVATION_DONE
  └── HandleActivationDoneEvent()（:316-338）
       └── SetDeviceState(Idle) → ota_.reset() → 就绪提示音
```

可打断性：激活期间用户 ToggleChat 或唤醒词命中都会把状态拉为 Idle，CheckNewVersion 内各等待循环每秒检查 Idle 提前退出（application.cc:445-450、488-490、839-842）。

### 3.2 设备声明：`Ota::CheckVersion()`（ota.cc:77-110）

- **URL**：`Settings("wifi").GetString("ota_url")`，空则回退 `CONFIG_OTA_URL`（默认 `https://api.tenclass.net/xiaozhi/ota/`，Kconfig.projbuild:3-5）。
- **方法**：`board.GetSystemInfoJson()` 恒非空 → 实际永远 **POST**（ota.cc:93-94）。
- **HTTP 客户端**：`Board::GetNetwork()->CreateHttp(0)`，Wi-Fi 板底层是 ESP-IDF `esp_http_client`（esp-ml307 组件封装）。
- **请求头**（`SetupHttp`，ota.cc:55-72）：

```text
Activation-Version: 1 | 2        // 有 eFuse serial number 则为 "2"（ota.cc:60）
Device-Id: <MAC，小写冒号格式>     // SystemInfo::GetMacAddress()
Client-Id: <UUID v4>              // board.GetUuid()
Serial-Number: <eFuse USER_DATA 前 32 字节>   // 仅 version=2
User-Agent: <BOARD_NAME>/<app version>
Accept-Language: <Lang::CODE>
Content-Type: application/json
```

- **POST 体**（`board.cc:70-178` 的 `GetSystemInfoJson()`）：`version / language / flash_size / minimum_free_heap_size / mac_address / uuid / chip_model_name / chip_info{model,cores,revision,features} / application{name,version,compile_time,idf_version,elf_sha256} / partition_table[...] / ota{label} / display{...} / board{...}`。精简版只需保留服务器校验依赖的子集（version、flash_size、mac_address、uuid、chip_model_name、application 即可工作）。
- **结果判定**：非 200 → 返回状态码本身；JSON 解析失败 → `ESP_ERR_INVALID_RESPONSE`。

### 3.3 响应五段解析（内联于 ota.cc:116-244）

| 段 | 解析字段 | 去向 |
|:---|:---|:---|
| `activation` | `message` / `code` / `challenge` / `timeout_ms` | 存成员 + 布尔标志；**`timeout_ms` 解析后无任何消费方（死字段）** |
| `mqtt` | 全部 string/number 键 | 写入 NVS namespace `mqtt`（值变化才写） |
| `websocket` | 全部 string/number 键（实际 `url`/`token`/`version`） | 写入 NVS namespace `websocket`（ota.cc:167-186） |
| `server_time` | `timestamp`（毫秒，必需）、`timezone_offset`（**分钟**，可选） | 偏移直接加进时间戳后 `settimeofday()`（ota.cc:188-211） |
| `firmware` | `version` / `url` / `force` | 版本比较置 `has_new_version_`（升级用，可裁） |

> ⚠️ **server_time 陷阱**：时区偏移被烘进时间戳，系统时钟被设为**本地时间**而非 UTC。若目标项目另有 RTC 芯片（TAB5 的 rx8130ce）或需要 UTC 语义，必须统一约定，避免双重加时区。

### 3.4 激活码与绑定轮询（application.cc:417-493）

`CheckNewVersion()` 精确逻辑：

1. `while(true)` 外层循环：`ota_->CheckVersion()`，失败指数退避（10 s 起步×2），**连续失败 10 次直接 return**——但 ActivationTask 仍会继续 `InitializeProtocol()` + DONE，靠 NVS 缓存的旧配置上线（:430-433）。
2. 成功且有新版本 → 升级分支（裁剪点，见 3.7）。
3. 无激活需求（无 code 且无 challenge）→ `break`，这是已绑定老设备的正常退出点（:466-469）。
4. 有 `activation.code` → `ShowActivationCode()`：Alert 显示 + 逐位播报数字音效 `OGG_0..9`（:655-677）。
5. **Activate 循环**（:477-491）：`for (i=0; i<10; ++i)` 调 `ota_->Activate()`：
   - `ESP_OK`（HTTP 200）→ 绑定成功，break；随后 `continue` 外层循环**重新 CheckVersion** 拉取正式 websocket 配置；
   - `ESP_ERR_TIMEOUT`（HTTP 202，服务器已受理、等待用户输入验证码）→ 等 3 秒重试；
   - 其他错误 → 等 10 秒重试；
   - 10 次未成功 → 回到外层 `while(true)` 重新 CheckVersion（重拉 challenge、重播验证码）。

### 3.5 安全绑定（Activation-Version 2）

`Ota::GetActivationPayload()`（ota.cc:421-456）：

```json
{"algorithm":"hmac-sha256","serial_number":"...","challenge":"...","hmac":"<64 字符小写 hex>"}
```

- HMAC 细节：`esp_hmac_calculate(HMAC_KEY0, challenge, len, out)`（ota.cc:431），eFuse key block `HMAC_KEY0`（密钥须预烧 purpose `HMAC_UP`），`#ifdef SOC_HMAC_SUPPORTED` 保护；输出 32 字节 SHA-256 转小写 hex。
- **注意**：`Activate()` 只要求 `has_activation_challenge_`，**不要求 serial number**——无 serial 设备 payload 为 `"{}"` 仍发 POST（ota.cc:422-424、459-462）。serial 只决定 `Activation-Version` 头与 payload 内容。
- Activate 请求 URL = CheckVersion URL 末尾拼接 `activate`（默认 `https://api.tenclass.net/xiaozhi/ota/activate`，ota.cc:464-469）。

### 3.6 token/URL 持久化与标识体系

- **写**：CheckVersion 成功后把 `websocket` 段整段键值写入 NVS namespace `websocket`（仅值变化才写）。
- **读**：`WebsocketProtocol::OpenAudioChannel()` 以只读方式打开同 namespace 读 `url`/`token`/`version`（websocket_protocol.cc:80-86）。

| 标识 | 生成/来源 | 持久化 |
|:---|:---|:---|
| `Device-Id` | `esp_read_mac(ESP_MAC_WIFI_STA)`，`%02x:` 小写冒号（system_info.cc:35-47） | 无（每次读硬件） |
| `Client-Id` | UUID v4：`esp_fill_random` 16 字节 + version4/variant1 位（board.cc:25-46） | NVS namespace `board`、key `uuid`，首启生成终身持久 |
| `Serial-Number` | eFuse `ESP_EFUSE_USER_DATA` 前 32 字节，量产烧录（ota.cc:29-40） | 无 |

**Settings 类语义与陷阱**（settings.cc）：构造 `nvs_open` 失败不报错只留 handle=0；**析构时才 `nvs_commit`（延迟提交）**，同一作用域多次 Set 只刷一次 flash；只读模式下误调 Set 仅一行警告、配置静默丢弃；写路径用 `ESP_ERROR_CHECK`——NVS 写失败直接 abort。精简版若改用自有 NVS 服务，需保留「延迟提交」或等价批量提交语义。

### 3.7 升级功能裁剪指南

- `CheckNewVersion` 与升级的耦合仅两处：`HasNewVersion()` 分支（application.cc:457-462，唯一调用 `UpgradeFirmware` 处）和 `MarkCurrentVersionValid()`（:465，factory 分区项目本就跳过）。删这两处即摘除升级，**不要动** `SetupHttp/CheckVersion/Activate`——声明/绑定与升级共用同一请求链路。
- `firmware` 段解析保留无害（只置标志），可整段删。
- `CheckAssetsVersion()`（application.cc:357-415）与 Ota 对象零耦合，目标项目无 assets 分区可整段删；但注意 xiaozhi 的提示音 `Lang::Sounds::OGG_*` 实际从 assets 取数据，删前确认提示音另有来源或一并裁剪。

---

## 4. 离线语音唤醒与音频管道

### 4.1 全链路数据流

**上行（mic → 服务器）**：

```text
MIC (I2S) → AudioCodec::InputData()
  → AudioInputTask：10 ms 一帧读 160 采样@16k（audio_service.cc:299-301）
     （codec 输入率 ≠16k 时先读原始率再经 input_resampler_ 重采样，:190-234）
  → AfeAudioEngine::Feed() 追加进 input_buffer_，按 feed chunk 喂 afe_iface_->feed()
     ——feed 在 AudioInputTask 上下文（afe_audio_engine.cc:194-213）
  → ProcessingTask("audio_afe")：fetch_with_delay 阻塞取结果（:348-384）
     ├─ 唤醒分支 HandleWakeWordResult：WAKENET_DETECTED → 回调 wake_word
     │   （检测到后引擎自动自清 kWakeWordEnabled，:407-410）
     ├─ VAD 边沿 → on_vad_change
     └─ 语音帧聚成 960 采样（60 ms）→ PushTaskToEncodeQueue
  → OpusCodecTask 编码分支 → audio_send_queue_（深 40，满丢最旧）
  → on_send_queue_available → Application 主循环 protocol_->SendAudio()
```

**下行（服务器 → 扬声器）**：

```text
WS 二进制帧（tcp_receive 任务）→ on_incoming_audio_
  → Application：仅 Speaking 态 PushPacketToDecodeQueue（application.cc:518-522）
  → OpusCodecTask 解码分支（受 playback 队列 <2 反压闸控，audio_service.cc:369-440）
     → 逐包 SetDecodeSampleRate（采样率变化才重建解码器，:388, :500-518）
     → output_resampler_（esp_ae_rate_cvt）服务器采样率 → codec 输出率
  → audio_playback_queue_（深 2）→ AudioOutputTask → codec->OutputData()
```

三级帧粒度要分清：**10 ms 采集帧 → AFE feed/fetch chunk → 60 ms Opus 编码帧**。

### 4.2 AudioService 队列与参数（精确值）

| 队列 | 深度 | 满时策略 |
|:---|---:|:---|
| `audio_encode_queue_`（PCM→编码） | 2 | 丢最旧（audio_service.cc:560-577） |
| `audio_send_queue_`（Opus→发送） | 40（=2400/60，2.4 s 缓冲） | 丢最旧——实时音频过期无用（:473-476） |
| `audio_decode_queue_`（Opus→解码） | 40 | wait=false 直接丢；wait=true 阻塞（:580-599） |
| `audio_playback_queue_`（PCM→播放） | 2 | 反压闸控解码任务 |
| `audio_testing_queue_` | 166（10 s 自测，可裁） | 满即关测试 |
| `timestamp_queue_` | 3 | 仅服务器 AEC 模式用 |

**Opus 编码器配置**（`AS_OPUS_ENC_CONFIG()`，audio_service.h:65-76）：16 kHz / mono / 16 bit / `BITRATE_AUTO` / 60 ms 帧 / `APPLICATION_AUDIO` / **complexity=0 / FEC=false / DTX=true / VBR=true**。帧长 960 采样由 `esp_opus_enc_get_frame_size` 反查（audio_service.cc:73-77）。`OPUS_FRAME_DURATION_MS=60`（audio_service.h:39）是端云契约（hello 的 `audio_params.frame_duration` 上报同一值）。

**解码器**：mono、非 self-delimited；初始按 codec 输出率配置，之后**每个下行包**按 `packet->sample_rate` 惰性重建（包上的采样率由协议层从服务器 hello 解析后逐包打标，默认 24000）。

### 4.3 AFE 引擎（ESP32-P4 路线）

`AfeAudioEngine::Initialize`（afe_audio_engine.cc:46-192）：

1. **模型加载**：`esp_srmodel_init("model")`——从 label 为 `"model"` 的分区加载；`ESP_WN_PREFIX`/`ESP_MN_PREFIX`/`ESP_VADN_PREFIX` 过滤 WakeNet/MultiNet/VADN 模型。
2. **检测器选择：MultiNet 优先**——有 MN 模型走自定义唤醒词（MultiNet 命令词机制），否则有 WN 模型走 AFE 内嵌 WakeNet（:76-102）。
3. **input_format**：`'M'×(channels−ref) + 'R'×ref`，Tab5（1 mic + 1 回采）→ `"MR"`（:116-123）。
4. **afe_config_t 覆写**（:128-150，`afe_config_init(fmt, models_, AFE_TYPE_VC, AFE_MODE_HIGH_PERF)` 之后）：

| 字段 | 值 | 说明 |
|:---|:---|:---|
| `aec_init` | `codec_->input_reference()` | 有硬件参考通道才开 AEC |
| `aec_mode` | `AEC_MODE_VOIP_HIGH_PERF` | ⚠️ audio/README.md 写 FD_LOW_COST 已过时，以代码为准 |
| `aec_nlp_level` | `AEC_NLP_LEVEL_VERYAGGR` | 最激进残余回声抑制 |
| `ns_init` | `false` | 无 NSNet 模型，降噪关闭 |
| `vad_init` | `CONFIG_USE_AUDIO_PROCESSOR` | |
| `vad_mode` / `vad_min_noise_ms` | `VAD_MODE_0` / `100` | 最保守判决 |
| `wakenet_init` | 仅 WakeNet 路线 | |
| `agc_init` | `false` | |
| `memory_alloc_mode` | `AFE_MEMORY_ALLOC_MORE_PSRAM` | |

5. 创建后**先禁用** WakeNet 与 AEC，实际使能由 ProcessingTask 上下文的 `ApplyAfeControls` 执行（开关与并发 fetch 不安全，:317-335）。**AEC 使能条件**：`kWakeWordEnabled || (device_aec_enabled_ && kVoiceProcessingEnabled)`——唤醒词监听期间 AEC 常开（保证播放中可唤醒），语音上行期间仅当设备侧 AEC 开启。
6. AFE 停用残余处理：清 `kAfeActive` 时 `control_generation_++` 作废在途 fetch，`reset_buffer()` 推迟到 ProcessingTask 执行（与并发 fetch 互斥，:282-308）。

### 4.4 AEC 参考信号通路（重点，决定与目标项目混音器的关系）

- **全代码库不存在软件 `SetReferenceData` 接口**——AFE 的参考只能来自随 mic 数据一起交错送达的 I2S 输入帧（`input_format="MR"` 中的 R）。
- **Tab5 = 硬件回采**：xiaozhi 自带 `boards/m5stack-tab5/` 完整板级实现（与目标硬件同型，是最直接对照样本）。其 RX 为 I2S TDM 4 槽，槽 0 = 麦克风、**槽 1 = 参考**；ES7210 TDM 槽序为 (MIC1, MIC3, MIC2, MIC4)，槽 1 对应 ES7210 的 MIC3 输入脚——**扬声器信号在 PCB 上硬连线回采到该 ADC 通道**（tab5_audio_codec.cc:141-203、box_audio_codec.cc:212-214 注释）。播放数据不做任何软件回灌。
- **软件馈送变体**（无硬回采的板子）：esp-box-lite 在 `Write` 里把播放 PCM 拷进环形 `ref_buffer_`，`Read` 时把「mic + 缓存参考」交错拼帧（box_audio_codec_lite.cc:223-271）——可作为无硬件回采时的参考实现，但要自担 RX/TX 时钟漂移风险（硬回采与 mic 同一 I2S 时钟域，天然同步）。
- **对目标项目的含义**：Tab5 上直接用硬件回采即可，无需把小智播放数据软件回灌；且目标项目混音器（SF2 主音源 + TTS aux）的所有声音都经同一片 ES8388 播出，回采通道天然包含全部播放内容，AFE 会把音乐一并作为回声消除——这正是「放音乐时仍可唤醒词打断」所需特性。
- **但注意**：xiaozhi 的 Tab5 codec 是 24 kHz TDM 独占驱动，与目标项目现有 44.1 kHz 混音通路共存时需做驱动层取舍（伪 codec 转发 vs 独立第二通路）。

### 4.5 三个 Enable 开关与状态组合

| 开关 | 语义 |
|:---|:---|
| `EnableWakeWordDetection(bool)`（audio_service.cc:631-652） | 懒初始化引擎；开时 reset 输入重采样器、置 `kWakeWordEnabled`。**命中后引擎自动关闭检测**，需上层重新打开 |
| `EnableVoiceProcessing(bool)`（:654-677） | 开时 `ResetDecoder()`（清下行）+ 输入先丢 120 ms 暖机 + 置 `AS_EVENT_AUDIO_PROCESSOR_RUNNING` |
| `EnableAudioTesting(bool)`（:679-693） | 配网模式麦克风环回自测，可整段裁 |

Application 状态组合（HandleStateChangedEvent）：**Idle** = 唤醒开 + 语音处理关；**Listening** = 语音处理开（+可选唤醒）；**Speaking（非 realtime）** = 语音处理关 + 唤醒仅 AFE 类保留（`EnableWakeWordDetection(IsAfeWakeWord())`，注释：只有 AFE 唤醒词可在说话中检测，因为有 AEC 参考）+ `ResetDecoder()`。

### 4.6 唤醒词音频回传与提示音

- **唤醒词缓存**：`WakeWordAudioCache` 在 PSRAM 分配 **64000 字节**环形缓冲（16 kHz mono int16 恰 2 秒），唤醒检测开启期间每个 fetch chunk 都写入、满覆最旧（wake_word_audio_cache.cc）。
- **回传流程**（`CONFIG_SEND_WAKE_WORD_DATA`，默认开）：命中 → `Application::BeginWakeWordInvoke` 调 `EncodeWakeWord()` → 引擎建独立任务把缓存按 960 采样帧编码为 Opus 包队列（空包作结束哨兵）→ `ContinueWakeWordInvoke` 先把这些包全部 `SendAudio`，再发 `{"type":"listen","state":"detect","text":唤醒词}`（application.cc:890-897）。服务器借此听到用户完整指令开头。
- **提示音 PlaySound**（audio_service.cc:710-731）：OGG 内嵌资源经 `OggDemuxer` 解出 Opus 包，以 wait=true 压入**同一条** `audio_decode_queue_`——提示音与 TTS 共享解码播放管道、按入队顺序混播；抢占靠 `ResetDecoder()`（清解码器+decode/playback 队列+`playback_generation_++` 作废在途解码）。时序陷阱：进入 Listening 时 `EnableVoiceProcessing(true)` 内部会 ResetDecoder，故 popup 音必须在其后播放（application.cc:984-988 注释）。

### 4.7 AudioCodec 抽象与最小适配面

接口（audio_codec.h:27-69）：纯虚仅 `Read(int16_t*, int)` / `Write(const int16_t*, int)` 一对；虚函数 `SetOutputVolume / SetInputGain / EnableInput / EnableOutput / OutputData / InputData / Start`；getter 组 `duplex() / input_reference() / input_sample_rate() / output_sample_rate() / input_channels() / output_channels()`。音量经 NVS 持久化（`audio.output_volume`），纯 codec 硬件音量、无软件增益。

**最小适配面**：AudioService 只依赖 getter + `Start/InputData/OutputData/EnableInput/EnableOutput`。可写一个**伪 codec**：构造填好采样率/通道/`input_reference_`，`Read` 返回「mic+ref 交错」帧、`Write` 把 PCM 交给目标项目混音器；其余空实现。约束：`Read` 必须按 10 ms 节拍稳定供数；伪 codec 直接报 16 kHz 可省掉 AudioService 的输入重采样段。

### 4.8 采样率换算全表（以 Tab5 板为例）

| 段 | 从 → 到 | 位置 |
|:---|:---|:---|
| ES7210 采集 | TDM 24 kHz 4 槽 | boards/m5stack-tab5/config.h:8 |
| codec → 引擎 | 24 k → **16 k**（2 声道交错 "MR"） | input_resampler_（audio_service.cc:79-86） |
| AFE 工作率 / Opus 上行 | 16 kHz mono | AFE VC 型固定 16k |
| 服务器下行 | hello 决定，默认 **24 kHz** | protocol.h:77 |
| 解码 → codec 输出 | 服务器率 → codec 输出率（Tab5 为 24k，**零重采样**） | output_resampler_ 按需创建（:520-533） |

---

## 5. 流式语音对话：WebSocket 协议

### 5.1 Protocol 抽象层（protocols/protocol.h/.cc）

```cpp
struct AudioStreamPacket { int sample_rate; int frame_duration; uint32_t timestamp;
                           std::vector<uint8_t> payload; };        // protocol.h:10-15

class Protocol {
    // 纯虚
    virtual bool Start() = 0;
    virtual bool OpenAudioChannel() = 0;
    virtual void CloseAudioChannel(bool send_goodbye = true) = 0;
    virtual bool IsAudioChannelOpened() const = 0;
    virtual bool SendAudio(std::unique_ptr<AudioStreamPacket>) = 0;
    // 基类已实现（拼 JSON 走 SendText）
    virtual void SendWakeWordDetected(const std::string&);
    virtual void SendStartListening(ListeningMode);
    virtual void SendStopListening();
    virtual void SendAbortSpeaking(AbortReason);
    virtual void SendMcpMessage(const std::string&);
    // 回调注册：OnIncomingAudio / OnIncomingJson / OnAudioChannelOpened /
    //           OnAudioChannelClosed / OnNetworkError / OnConnected / OnDisconnected
};
```

> 勘误：**不存在 `SendTtsMessage` 接口**——TTS 方向消息由服务器发起，设备端只有上述 5 个语义化发送方法。

各发送方法的精确 JSON（protocol.cc:58-98，`session_id` 总是携带）：

| 方法 | JSON |
|:---|:---|
| `SendAbortSpeaking` | `{"type":"abort"[,"reason":"wake_word_detected"]}`（reason=None 时不带 reason 字段） |
| `SendWakeWordDetected` | `{"type":"listen","state":"detect","text":"<唤醒词>"}` |
| `SendStartListening` | `{"type":"listen","state":"start","mode":"auto\|manual\|realtime"}` |
| `SendStopListening` | `{"type":"listen","state":"stop"}` |
| `SendMcpMessage` | `{"type":"mcp","payload":<JSON-RPC 原文>}` |

ListeningMode 映射：`kListeningModeAutoStop→"auto"`、`ManualStop→"manual"`、`Realtime→"realtime"`。

### 5.2 建立通道 `OpenAudioChannel()`（websocket_protocol.cc:79-196）

1. NVS `websocket` namespace 读 `url`/`token`/`version`（`version` 非 0 才覆盖默认 1）。
2. `network->CreateWebSocket(1)` 创建连接对象。
3. 握手头：`Authorization: Bearer <token>`（token 无空格自动补 `Bearer ` 前缀，为空则不加）、`Protocol-Version`、`Device-Id: <MAC>`、`Client-Id: <UUID>`。
4. `Connect(url)`——底层 esp-ml307 自研 RFC6455 客户端手工拼 HTTP Upgrade 握手（**独立的 10 秒超时**）。
5. `SendText(GetHelloMessage())`。
6. `xEventGroupWaitBits(WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT, 10000ms)` 等服务器 hello——**两个 10 秒超时串联，最坏约 20 秒**。
7. 成功触发 `on_audio_channel_opened_()`（在 OpenAudioChannel 调用者上下文，即主任务）。

### 5.3 Hello 消息

设备发送（websocket_protocol.cc:198-222）：

```json
{
  "type": "hello", "version": 1,
  "features": {
    "mcp": true,                 // 恒 true
    "aec": true,                 // 仅 CONFIG_USE_SERVER_AEC 时存在
    "glyph_push": true           // 仅支持动态字形时附加，同时附 text_font{...}
  },
  "transport": "websocket",
  "audio_params": {"format":"opus","sample_rate":16000,"channels":1,"frame_duration":60}
}
```

服务器返回解析（`ParseServerHello`，:224-250）：校验 `transport=="websocket"`（不匹配不置事件位→退化为超时）；提取 `session_id`、`audio_params.sample_rate`（默认 24000）、`audio_params.frame_duration`（默认 60）；**不解析 format/channels**。

### 5.4 二进制音频帧（protocol.h:17-31）

- **版本 1（默认）**：WebSocket binary 帧 payload 即裸 Opus 数据，靠 WS 帧类型区分文本/二进制。
- **版本 2**（16 字节头，大端）：`u16 version; u16 type(0=OPUS,1=JSON); u32 reserved; u32 timestamp(毫秒); u32 payload_size; u8 payload[]`。timestamp **不是本地墙钟，而是回显的下行帧时间戳**——供服务器端 AEC 对齐参考（audio_service.cc:344-349, 547-553），仅 `CONFIG_USE_SERVER_AEC` 有意义。
- **版本 3**（4 字节头，大端）：`u8 type; u8 reserved; u16 payload_size; u8 payload[]`。
- 接收路径注意：v2/v3 解包时 **type 字段被解析但未使用**——无论 0/1 一律当音频抛给 `on_incoming_audio_`（websocket_protocol.cc:111-132）。

### 5.5 文本消息分发

Protocol 层只拦截 `type=="hello"`（走 ParseServerHello），其余带 type 的 JSON 原样抛给 `on_incoming_json_`；分发在 Application（application.cc:543-650）：

| type | 处理 |
|:---|:---|
| `tts` `state=start` | Schedule：置 `kDeviceStateSpeaking`（无条件尝试，合法性由转换表决定） |
| `tts` `state=stop` | Schedule：当前 Speaking 时——manual→Idle，**auto 和 realtime 都→Listening**（:560-569） |
| `tts` `state=sentence_start` | 解析 text/glyph，显示当前句 |
| `stt` | 用户识别文本上屏 |
| `llm` | `emotion` → 表情（精简版可裁） |
| `mcp` | `McpServer::ParseMessage(payload)`——**直接在 tcp_receive 任务上下文执行，未 Schedule**（:608-612） |
| `system` `command=reboot` | Schedule(Reboot) |
| `alert` | 服务端告警展示 |

### 5.6 连接生命周期与保活

- `IsAudioChannelOpened()` = 对象存在 && 已连接 && 无错误 && **未超时**（:70-72）。
- **超时**：`IsTimeout()` = 120 秒无任何入站数据（protocol.cc:100-109）——**不是定时器**，只在 `IsAudioChannelOpened()` 被查询时惰性判定；`last_incoming_time_` 在任何文本/二进制帧到达时刷新。
- `CloseAudioChannel(send_goodbye)`：**参数被忽略，没有 goodbye 消息**，直接销毁连接对象（:74-77，注释自述 WebSocket 不需要 goodbye）。
- 被动断开：`OnDisconnected` → `on_audio_channel_closed_()`（**tcp_receive 任务上下文**）→ Application 回 Idle。
- **无自动重连**：协议层与 Application 层都没有重连逻辑；断开后回 Idle，等下一次唤醒/按键重新 `OpenAudioChannel()`。`on_connected_` 回调在 WebsocketProtocol 下**永远不会触发**（只有 MqttProtocol 用）。
- **线程安全**：底层 WebSocket `Send` 有帧级互斥锁，文本帧（主任务）与二进制音频帧交错发送不撕帧，可从任意任务调用。

### 5.7 底层组件说明

WebSocket 不是 `esp_websocket_client`，而是 `78/esp-ml307 ~3.6.6` 组件内自研的 RFC6455 客户端（lwIP 裸 socket，手写握手/帧编解码/Ping-Pong）。每条 TCP 连接配一个 `tcp_receive` 任务（栈 4096、优先级 1）。**陷阱**：4096 字节栈跑 `cJSON_Parse` 大 JSON 有溢出风险；精简版宜让该回调只做拷贝入队，解析挪到工作任务。目标项目若改用 `esp_websocket_client`（现有移植版即如此），需注意其默认 8 KB 任务栈同样不够解码+重采样。

---

## 6. 核心状态机

### 6.1 状态与转换表

11 个状态（device_state.h:4-16）：`Unknown / Starting / WifiConfiguring / Idle / Connecting / Listening / Speaking / Upgrading / Activating / AudioTesting / FatalError`。

`DeviceStateMachine::IsValidTransition()`（device_state_machine.cc:34-102，同态转换恒合法）：

| from | 合法 to |
|:---|:---|
| Unknown | Starting |
| Starting | WifiConfiguring, Activating |
| WifiConfiguring | Activating, AudioTesting |
| AudioTesting | WifiConfiguring |
| Activating | Upgrading, Idle, WifiConfiguring |
| Upgrading | Idle, Activating |
| Idle | Connecting, Listening, Speaking, Activating, Upgrading, WifiConfiguring |
| Connecting | Idle, Listening |
| Listening | Speaking, Idle |
| Speaking | Listening, Idle |
| FatalError | 无（不可迁出） |

`TransitionTo`：非法转换仅 `ESP_LOGW` 返回 false，不抛异常不改状态。状态变更经观察者列表广播；回调在 `TransitionTo` 调用者上下文执行——Application 注册的 listener 只置 `MAIN_EVENT_STATE_CHANGED`，副作用全部延迟到主循环。

精简版可裁掉 `Upgrading / AudioTesting / WifiConfiguring / FatalError`（注意：**FatalError 在原代码中本就是死路径**——全工程无任何迁移到该状态的代码）。

### 6.2 状态进入副作用（HandleStateChangedEvent，application.cc:906-969）

| 状态 | 副作用 |
|:---|:---|
| Unknown/Idle | 显示 STANDBY、清聊天消息、emotion=neutral；`EnableVoiceProcessing(false)` + `EnableWakeWordDetection(true)` |
| Connecting | 显示 CONNECTING |
| Listening | 显示 LISTENING；auto 模式且播放未排空 → 置 `pending_listening_start_` 等 PLAYBACK_DRAINED，否则直接 `StartListeningAudio()`（发 listen start + 开语音处理 + 播 popup） |
| Speaking | 显示 SPEAKING；**非 realtime**：`EnableVoiceProcessing(false)` + `EnableWakeWordDetection(IsAfeWakeWord())`；**无条件 `ResetDecoder()`** |
| WifiConfiguring | 两者皆关 |

任何状态变更时 `clock_ticks_` 清零——但该计数器**没有任何超时判断消费它**（死逻辑），Application 层不存在连接保活/空闲超时。

### 6.3 ListeningMode × AecMode

```cpp
enum ListeningMode { kListeningModeAutoStop,    // 无 AEC：说一句，等 tts.stop 自动再听
                     kListeningModeManualStop,  // 手动停止
                     kListeningModeRealtime };  // 有 AEC：全双工连续对话
GetDefaultListeningMode() { return aec_mode_ == kAecOff ? kListeningModeAutoStop
                                                        : kListeningModeRealtime; }
```

- `aec_mode_` 由 Kconfig 构造期固定（`CONFIG_USE_DEVICE_AEC` / `CONFIG_USE_SERVER_AEC` / 皆无），运行期 `SetAecMode()` 改后会关闭通道强制重连（application.cc:1153-1178）。
- **realtime 模式 speaking 时继续上传麦克风**（barge-in 的底层机制）：Speaking 副作用里 `EnableVoiceProcessing(false)` 仅在非 realtime 执行（:954-955），realtime 下语音处理持续运行、编码发送链路不断。
- auto 模式 `tts.stop` 后回 Listening 时若播放队列未排空（网络抖动导致 stop 早到），延迟到 `MAIN_EVENT_PLAYBACK_DRAINED` 才真正 `StartListeningAudio()`——避免把 TTS 尾巴当用户语音上传。

### 6.4 唤醒事件分支（HandleWakeWordDetectedEvent，application.cc:811-843）

| 当前状态 | 动作 |
|:---|:---|
| Idle | `BeginWakeWordInvoke`：先 `EncodeWakeWord()`；**必须经 Connecting 态**（否则后续状态检查静默丢弃）；通道未开 → `Schedule(ContinueWakeWordInvoke)`（OpenAudioChannel 可能阻塞约 1 秒，故异步） |
| Listening | `AbortSpeaking(wake_word_detected)` + 清发送队列 + 重发 `SendStartListening` + ResetDecoder + 播 popup + 重开唤醒检测 |
| Speaking | `AbortSpeaking(...)` + 清队列 + `play_popup_on_listening_=true` + `SetListeningMode(default)`（→Listening，popup 延迟到 StartListeningAudio 播，避免被 ResetDecoder 清掉） |
| Activating | `SetDeviceState(Idle)` 打断激活流程 |

陷阱：唤醒命中后**引擎已自动关闭唤醒检测**，上述每条路径都要负责在合适时机 `EnableWakeWordDetection(true)` 恢复（Idle 副作用兜底）。

### 6.5 公开控制 API

`ToggleChatState() / StartListening() / StopListening()` 本身只置事件位（线程安全），语义在主循环 Handler：`AbortSpeaking(reason)` 只发 abort 消息 + 置 `aborted_`——**注意 `aborted_` 全工程只写不读，是遗留死字段**。错误兜底：任何协议 `OnNetworkError` → 主循环统一 `SetDeviceState(Idle)` + Alert，无重连。

---

## 7. MCP 控制

### 7.1 角色与封装

设备 = MCP Server，云端 AI = MCP Client。外层封装（`Protocol::SendMcpMessage`，protocol.cc:94-98）：

```json
{"session_id":"...","type":"mcp","payload":{ /* JSON-RPC 2.0 */ }}
```

### 7.2 数据模型（mcp_server.h）

- `Property`：三种类型 `boolean/integer/string`（无 float）；**无默认值即 required**；integer 可带 `minimum/maximum`（构造与 `set_value` 时校验，越界抛 `invalid_argument`）。序列化为 JSON Schema 片段。
- `PropertyList` = `std::vector<Property>`；`to_json()` 生成 `{"参数名":{schema},...}`；`GetRequired()` 收集无默认值参数名。
- `McpTool::to_json()` 生成完整工具描述：`{name, description, inputSchema:{type:"object",properties,required?}, annotations?}`——user_only 工具附 `"annotations":{"audience":["user"]}`。
- `ReturnValue = std::variant<bool,int,std::string,cJSON*,ImageContent*>`；统一包装为 `{"content":[{...}],"isError":false}`。注意 **`ImageContent` 全仓库无构造点，是死代码**——相机实际走 HTTP 旁路上传。

### 7.3 工具注册

- 容器 `std::vector<McpTool*> tools_`；**重名拒绝注册且泄漏**（mcp_server.cc:302-305 直接 return 未 delete）。
- `AddUserOnlyTool` 仅多置一个 bool 标记；`tools/list` **默认过滤 user_only 工具**，客户端须传 `params.withUserTools:true` 才列出。
- `McpServer` 构造为空，common/user-only 工具由 `Application` 显式调 `AddCommonTools()/AddUserOnlyTools()` 注册；`AddCommonTools` 会把已有工具挪到尾部，让 common 工具排最前（利于 prompt cache）。
- common 工具（均带注册条件）：`self.get_device_status`、`self.audio_speaker.set_volume`（0-100）、`self.screen.set_brightness`（需背光非空）、`self.screen.set_theme`（需 LVGL 主题）、`self.camera.take_photo`（需相机）。
- user-only 工具：`self.get_system_info`、`self.reboot`、`self.upgrade_firmware`、`self.screen.get_info`、`self.screen.snapshot`、`self.screen.preview_image`、`self.assets.set_download_url`。
- 业务侧注册模式：板级 `InitializeTools()` 在板构造函数末尾调用；回调内可 `throw std::runtime_error("...")` 表达业务错误（会被捕获转为 error 响应）。

### 7.4 消息处理流程

调用链：WS 文本帧（**tcp_receive 任务**）→ Application `OnIncomingJson` 的 `mcp` 分支 → `McpServer::ParseMessage(payload)`（mcp_server.cc:350-433）：

1. `jsonrpc=="2.0"`、method 存在、params 形态、id 为数字——**校验失败只打日志不回任何响应**（与标准 JSON-RPC 不符，属实现缺陷）。
2. `notifications*` 开头一律静默忽略。
3. `initialize` → 解析 `params.capabilities.vision.url/token`（相机用，可裁）；响应 `{"protocolVersion":"2024-11-05","capabilities":{"tools":{}},"serverInfo":{"name":BOARD_NAME,"version":固件版本}}`。
4. `tools/list` → **cursor 语义是工具名**（`nextCursor` 指向下一个未返回的工具名），按 **8000 字节**截断分页；cursor 不匹配返回空数组而非报错。
5. `tools/call` → 参数逐级校验（未知工具 / 缺参 / 类型不匹配按缺参处理 / 超范围），**这些错误在接收线程直接 `ReplyError`**；校验通过则 `Application::Schedule()` 把工具回调切到**主任务**执行，结果/异常在主任务 `ReplyResult/ReplyError`。
6. 其他方法 → `ReplyError("Method not implemented: ...")`。

响应构造：`ReplyResult` = `{"jsonrpc":"2.0","id":N,"result":...}`；`ReplyError` = `{"jsonrpc":"2.0","id":N,"error":{"message":"..."}}`——**无标准 `code` 字段，message 未做 JSON 转义**（工具 throw 的文本含引号会产生非法 JSON）。

发送链路：`ReplyResult/ReplyError` → `Application::SendMcpMessage`（内部再 Schedule 一次）→ `protocol_->SendMcpMessage` 包外层 → `SendText`。

### 7.5 设备主动 MCP 消息

**不存在**。设备只发响应，从无主动 notification；`RegisterMcpBroadcastCallback` 的唯一用途是把 MCP 响应广播给板载本地控制服务器，与云端无关。若产品需要「设备状态变化主动告知 AI」，需自行扩展。

### 7.6 移植最小面

纯协议核心（`Property/PropertyList/McpTool/ParseMessage/GetToolsList/DoToolCall/Reply*`）仅依赖 cJSON + STL，可原样保留。对外仅两处反向依赖 Application 单例，解耦方案：

1. **发送通道**：`ReplyResult/ReplyError` 末尾的 `Application::SendMcpMessage` → 注入 `SetSendCallback`，对接目标项目 WS 发送（外层 `{"session_id","type":"mcp","payload"}` 包装逻辑也要一并移植）。
2. **主线程调度**：`DoToolCall` 的 `Application::Schedule` → 注入 `SetScheduler`，对接任意「切到指定任务执行」机制（事件组+任务队列，参考实现仅 7 行）。若工具回调都是快操作且无 LVGL/音频锁冲突，也可直接在工作任务同步执行——但原设计特意把回调放主线程，建议保留该语义。

`AddCommonTools/AddUserOnlyTools` 整段是对 Board/Display/相机的产品层依赖，提取时裁掉，按目标项目服务重写工具（音量→音频服务、亮度→显示服务、状态→自定义 JSON）。

---

## 8. 最小可插拔模块提取方案

### 8.1 建议保留的源码文件

| 层级 | 保留文件 | 说明 |
|:---|:---|:---|
| 应用框架 | `main/application.h/.cc` | 主事件循环、事件组、Schedule（剥离 UI/LED/升级/assets 后） |
| 状态机 | `main/device_state.h`、`device_state_machine.h/.cc` | 原样保留 |
| 协议抽象 | `main/protocols/protocol.h/.cc` | 删 `AddTextFontCapabilities`（glyph_push） |
| WebSocket | `main/protocols/websocket_protocol.h/.cc` | 可只保留协议版本 1 |
| 配置 | `main/settings.h/.cc`、`system_info.h/.cc` | 或替换为目标项目 NVS 服务 |
| 激活 | `main/ota.h/.cc` | 删 `Upgrade/MarkCurrentVersionValid/StartUpgrade` 与 firmware 段 |
| 音频总控 | `main/audio/audio_service.h/.cc` | 删 AudioTesting/AudioDebugger |
| 音频引擎 | `main/audio/audio_engine.h`、`engines/afe_audio_engine.h/.cc` | P4 路线 |
| 唤醒词 | `main/audio/wake_word.h`、`wake_words/esp_wake_word.*`（Lite 备选）、`custom_wake_word.*`（需 MultiNet 时）、`wake_word_audio_cache.*` | 按唤醒形态取舍 |
| OGG 解封装 | `main/audio/demuxer/ogg_demuxer.h/.cc` | 提示音（可选） |
| Codec | `main/audio/audio_codec.h/.cc` + 一个目标硬件 codec 或伪 codec | 见 4.7 |
| MCP | `main/mcp_server.h/.cc` | 裁 common/user-only 工具表，保留协议核心 |

### 8.2 必须裁剪的部分

`Ota::Upgrade*` 与 assets 下载、`MqttProtocol`、`LiteAudioEngine`（目标固定 P4 时）、AudioTesting、`AudioDebugger`、全部 Display/LED/Camera 实现与 `Lang::Strings/Sounds` 依赖、多板型 boards 目录（只留 codec 参考）、`text_glyph_payload` 与 hello 的 glyph_push/text_font 能力上报、`ImageContent` 返回路径、user-only 工具中升级/截图/预览类。

### 8.3 建议的模块边界与「单任务化」

三层边界：

1. **传输层**：Protocol + WebsocketProtocol + Settings/NVS。
2. **语音对话层**：状态机 + 事件循环 + AudioService + AFE 引擎。
3. **MCP 层**：McpServer 协议核心 + 业务工具注册表。

压缩为单任务的关键手法（原架构已铺好路）：

- 网络回调（tcp_receive 上下文）只拷贝入队，JSON 解析/音频入队全部挪到工作任务循环——摆脱 tcp_receive 4096 栈跑 cJSON 的溢出风险。
- 保留 `Schedule()` 的「队列 + 事件位」模式作为唯一串行化机制；唤醒、VAD、发送就绪、网络事件都归并为事件位或队列消息。
- 音频侧的任务（input/output/opus_codec/afe）若也要合并，需要接受采集节拍与解码在同一循环内分时；更现实的做法是**音频任务保留、控制面单任务**——即现有移植版的取舍（见第 9 章）。
- MCP 的 `Schedule` 注入到同一工作任务即可，无需独立主循环。

### 8.4 依赖清单

| 依赖 | 用途 |
|:---|:---|
| `esp-sr`（esp_afe / esp-srmodel） | AFE / WakeNet / MultiNet / VADN，需 `model` 分区 |
| `espressif/esp_audio_codec`（esp_opus） | Opus 编解码 |
| `esp_audio_effects` | `esp_ae_rate_cvt` 重采样 |
| `esp_codec_dev` | ES8388 等 codec 驱动（或伪 codec 替代） |
| `78/esp-ml307` | Http + 自研 WebSocket/TCP/SSL（或换 esp_http_client + esp_websocket_client） |
| `cJSON` | 协议与 MCP |
| `nvs_flash` | token/URL/UUID/音量持久化 |
| 分区 | `model`（唤醒模型，约 1 MB）、`nvs` |

---

## 9. 对照：现有移植 `components/service_xiaozhi`

目标项目其实已完成一次等价提取（C 语言重写）。以下对照帮助理解「精简版长什么样」，也暴露其与原版的能力差距。

### 9.1 模块对应表

| 原项目（C++） | 现有移植（C） | 备注 |
|:---|:---|:---|
| `application.cc` 主循环 + 状态机 | `service_xiaozhi.c`（xz_task 单循环 + 6 态状态机） | 事件组 → 3 条队列（cmd/msg/evt） |
| `ota.cc` | `xiaozhi_ota.c` | 仅 v1 激活路径 |
| `websocket_protocol.cc` | `xiaozhi_ws.c`（esp_websocket_client） | 固定协议 v1 |
| `audio_service.cc` + `afe_audio_engine.cc` | `xiaozhi_wake.c` + `xiaozhi_opus.c` + service_audio mic/aux | 架构大幅简化 |
| `mcp_server.cc` | `xiaozhi_mcp.c` | 4 个工具，手写校验 |
| `settings.cc` | service_nvs（namespace `tab5_cfg`，key `xz_uuid`/`xz_ws_url`/`xz_ws_token`） | 固定 key |
| 多任务群（6+ 任务） | **1 常驻（task_ai，栈 24576 @内部 RAM，prio 5，Core 0）+ 2 按需**（wake_fetch 栈 16384；esp_websocket_client 内部任务栈显式抬到 20480——默认 8 KB 曾在解码+重采样时溢出崩溃） | 编码在 mic 泵内同步、解码在 ws 回调内同步，零独立编解码任务 |

### 9.2 取舍清单

**保留并等价实现**：OTA v1 激活全流程（check → 激活码 → 202 轮询 → 取 websocket 配置）；Client-Id UUID v4 首启落盘；文本协议核心消息族（hello/listen/abort/tts/stt/alert/system/mcp）；Opus 编码参数逐项相同；AFE feed/fetch 双上下文唤醒架构；MCP 协议骨架（2024-11-05、initialize、tools/list 8000B 分页、tools/call isError）；server_time 校时；6 位激活码绑定交互（改为 App 聊天气泡呈现）。

**保留但简化**：

| 维度 | 原版 | 移植版 |
|:---|:---|:---|
| 状态机 | 11 态 + 转换校验类 | 6 态（IDLE/ACTIVATING/CONNECTING/READY/LISTENING/SPEAKING）单变量 switch，无校验层 |
| 音频管线 | 3 任务 + 5 队列 | xz_task mic 泵 + ws 回调内同步解码 + aux SPSC 环形缓冲（实际 88200 帧≈2 s，AGENTS.md 写「8K 帧」已过时） |
| AEC | 硬件回采 + AFE AEC 常驻 | **关闭 AEC/NS**，以「录音时关扬声器」物理消除回采；代价：READY 待机整机静音（SF2 亦无声）、**SPEAKING 期无法语音打断**（无 barge-in，仅按钮打断） |
| 协议 | v1/v2/v3 | 固定 v1 裸 Opus；hello 无 aec/text_font features |
| 唤醒配套 | 唤醒音频上行 + listen detect + popup 音 | 全部砍掉，命中直接开通道发 listen start |
| 断线策略 | 120 s 惰性超时 | 不自动重连（消除重连风暴）；20 s 空闲主动关通道（`LISTEN_IDLE_MS`） |
| 下行重采样 | esp_ae_rate_cvt + 队列化 | 自写 16.16 定点线性插值（24k→44.1k）→ aux 环形缓冲 |
| MCP 工具 | 12+ 个，反射式 PropertyList | 4 个（设备状态/音量/亮度/主题），手写校验，xz_task 同步执行 |

**完全砍掉**：MQTT 传输、Serial-Number/eFuse HMAC（Activation-Version 2）、固件 OTA 升级、AudioTesting、realtime 监听模式、服务器 AEC、glyph_push 字形推送、llm/emotion 表情、iot 旧协议、MultiNet 命令词、Board/Display/Led 抽象。

### 9.3 音频路由整合（移植版特有，原版无此问题）

- 上行：ES7210 → `service_audio_mic_open(16000,1)`/`mic_read`（≤160 帧/次）→（auto）AFE 喂唤醒且 LISTENING 时取 AFE 输出攒 960 /（manual）直接攒 960 → Opus → WS。
- 下行：WS 帧 → Opus 解码（24k mono）→ 线性插值重采样到 44.1k 立体声 → `service_audio_aux_write` → SPSC 环形缓冲 → task_audio（Core 1）与 SF2 主源 float 域混音 → 统一软限幅 → ES8388。
- 互斥关系：录音期 `service_audio_mic_open` 会临时关闭扬声器 codec（整机静音）；TTS 期 aux 与 SF2 共存混音。即「监听与发声互斥、TTS 与合成可混」。

### 9.4 移植版已知缺陷/遗留（审读发现，代码内无 TODO 标记）

- 死常量 `RECONNECT_BASE_MS/RECONNECT_MAX_MS`；死接口 `xiaozhi_wake_get_fetch_chunk()`；`activation_message/timeout_ms` 解析后无消费方。
- hello 的 `frame_duration` 只存日志，解码器帧长写死 60 ms——服务器下发非 60 ms 帧长会静默失配。
- ws 回调内同步做解码+重采样：阻塞后续帧接收、栈需求被迫抬到 20480；无播放队列削峰，aux 满即截断丢帧。
- WS 单帧 payload >4096 直接丢弃——大 MCP payload 会丢。
- `xiaozhi_wake_poll` 返回指向静态缓冲的指针，仅限单消费者。
- 服务器 alert 一律映射为 ERROR 事件，与真错误共用 UI 前缀。

---

## 10. 移植风险与陷阱汇总

| 风险点 | 说明 | 建议 |
|:---|:---|:---|
| Board 单例强耦合 | Application 大量 `Board::GetInstance()` | 抽象为接口注入，或按现有移植版改为显式服务调用 |
| UI/文案/音效强依赖 | `Lang::Strings/Sounds`、`display->*`、Alert | 收口为回调表或整段删除；提示音改目标项目自有资源 |
| AFE 模型分区 | 需要正确的 `model` 分区与模型匹配 | 确认分区表；唤醒词模型在 sdkconfig 选择（现有移植用 wn9_himiaomiao） |
| 内存分配模式 | 原版 `AFE_MEMORY_ALLOC_MORE_PSRAM`；移植版实测 P4 上 PSRAM cache-off 会崩，改 `MORE_INTERNAL` 且需 ≥90 KB 内部 RAM 余量（在 ws/TLS/opus 吃 RAM 之前先开 AFE） | 直接沿用移植版的校准值与开启时序 |
| tcp_receive 栈 | 4096 栈跑 cJSON/解码有溢出风险 | 回调只搬运；或换 esp_websocket_client 并抬栈（移植版 20480 是实测值） |
| 服务器时钟语义 | server_time 把时区烘进时间戳，系统时钟=本地时间 | 与 RTC 服务统一约定 |
| Schedule 无界 | 队列无容量上限 | 精简版加深度上限与丢弃策略 |
| MCP 响应缺陷 | 无 JSON-RPC code、message 未转义、校验失败静默 | 如需严格兼容 MCP 客户端，补 code 与转义 |
| 协议版本协商 | NVS version 不校验范围；v2/v3 接收不检查 type | 固定 v1 最省心（移植版即如此） |
| 双 10 秒超时 | WS 握手与 server hello 各 10 秒串联 | 用户感知上「连接中」最坏约 20 秒，UI 需有预期 |
| 唤醒检测自停 | 命中后引擎自动关闭检测 | 每条唤醒路径必须负责恢复（Idle 副作用兜底） |

---

## 11. 关键代码入口速查

| 关注点 | 文件 | 函数/行 |
|:---|:---|:---|
| 主入口 | `main/main.cc` | `app_main()` :14 |
| 初始化顺序 | `main/application.cc` | `Initialize()` :58 |
| 主事件循环 | `main/application.cc` | `Run()` :172 |
| 激活任务 | `main/application.cc` | `ActivationTask()` :340 / `CheckNewVersion()` :417 |
| OTA 声明/激活 | `main/ota.cc` | `CheckVersion()` :77 / `Activate()` :458 / `GetActivationPayload()` :421 |
| WS 连接 | `main/protocols/websocket_protocol.cc` | `OpenAudioChannel()` :79 |
| Hello 消息 | `main/protocols/websocket_protocol.cc` | `GetHelloMessage()` :198 / `ParseServerHello()` :224 |
| 二进制帧 | `main/protocols/protocol.h` | :17-31 |
| 状态机 | `main/device_state_machine.cc` | `IsValidTransition()` :34 |
| 状态副作用 | `main/application.cc` | `HandleStateChangedEvent()` :906 |
| 唤醒处理 | `main/application.cc` | `HandleWakeWordDetectedEvent()` :811 / `ContinueWakeWordInvoke()` :869 |
| 音频服务 | `main/audio/audio_service.cc` | `Initialize()` :56 / 上行任务 :280 / 编解码任务 :369 |
| AFE 引擎 | `main/audio/engines/afe_audio_engine.cc` | `Initialize()` :46 / ProcessingTask :348 |
| MCP 解析 | `main/mcp_server.cc` | `ParseMessage()` :350 / `DoToolCall()` :508 |
| MCP 工具注册 | `main/mcp_server.cc` | `AddCommonTools()` :33 / `AddUserOnlyTools()` :128 |
| Tab5 板级参考 | `main/boards/m5stack-tab5/` | codec TDM/回采配置 |

---

*文档生成：2026-07-28（基于六个并行源码精读任务的验证结果全面修订）*
*源码路径：`tools/xiaozhi-esp32/main/`；对照移植：`components/service_xiaozhi/`*
