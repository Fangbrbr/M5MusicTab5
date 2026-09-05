# M5MusicTab5 — 音乐探索终端

**简体中文 | [English](README_EN.md)**

[![i18n](https://img.shields.io/badge/i18n-中文%20%2F%20English-important)]()
[![build](https://img.shields.io/badge/ESP--IDF-v5.5.5-blue)]()
[![CI](https://img.shields.io/github/actions/workflow/status/Fangbrbr/M5MusicTab5/release.yml?label=CI)]()
[![license](https://img.shields.io/badge/license-MIT-green)]()
[![platform](https://img.shields.io/badge/platform-ESP32--P4-orange)]()
[![ui](https://img.shields.io/badge/ui-LVGL%209-9cf)]()
[![audio](https://img.shields.io/badge/audio-SF2%20Synthesizer-ff69b4)]()

🔥 **免安装在线烧录：[fangbrbr.github.io/M5MusicTab5](https://fangbrbr.github.io/M5MusicTab5)**

基于 M5Stack Tab5 (ESP32-P4) 的音乐探索终端，完整固件从零开发。

📺 **[项目演示视频](https://www.bilibili.com/video/BV1A7ua6WEH8)**

[![项目演示视频](doc/misc/cover.png)](https://www.bilibili.com/video/BV1A7ua6WEH8)

---

## 特性

- **🇨🇳 中英双语界面** — 设置页一键切换，重启记忆
- **13 个内置 App** — 禅模式 / 练耳 / 和弦练习 / 五度圈 / 小钢琴 / 组鼓 / 播放器 / XY Pad / 节拍器 / AI 语音助手 / 时钟日历 / 趣味抽卡 / 录音机（开发中）
- **SF2 采样合成器** — 基于 SoundFont 2，支持 SD 卡加载自定义音色
- **MP3 音乐播放** — 直接播放 SD 卡 .mp3，支持 ID3 歌名与中文文件名
- **标准 MIDI 录制与回放** — 录直出 .mid / SMF，拷进电脑任何 DAW 直接打开
- **全局 MIDI 总线** — USB Host、蓝牙、UART 多路输入，生产者/消费者解耦
- **FTP 无线文件管理** — 局域网内免拔卡管理 SD 卡全部文件
- **EEZ Studio 可视化 UI** — 前后端隔离，所见即所得
- **小智 AI 语音 + MCP 设备控制** — 语音对话 + 设备控制
- **实时时钟 + 联网功能** — 天气 / 新闻 / 农历 / 定时器

---

## 硬件

| 项目 | 规格 |
|---|---|
| 主控 | ESP32-P4，双核 RISC-V @ 360 MHz |
| 无线 | ESP32-C6（Wi-Fi + Bluetooth 5，SDIO 透传） |
| 内存 | 32 MB PSRAM |
| 存储 | 16 MB Flash + microSD 卡槽 |
| 显示 | 5 寸 ST7123 电容触摸屏，1280×720（MIPI-DSI） |
| 音频 | ES8388 DAC + ES7210 双麦 AEC，44.1 kHz |
| 接口 | USB-C OTG、USB-A Host、3.5mm 耳机、RS485 |
| 传感器 | BMI270 六轴 IMU |

---

## App 使用说明

👉 **[12 个 App 完整用法 →](doc/apps.md)**

---

## SF2 音源管理

固件内置一套 GM 音色（Florestan Basic GM GS），开箱即用；同时支持从 SD 卡加载自定义 SoundFont：

1. 将 `.sf2` 文件放入 SD 卡 `/soundfonts/` 目录
2. 进入 设置 → 音源，下拉列表选择目标音源（「内部预设」或 SD 卡中的文件）
3. 加载过程通知栏显示实时进度；大音源（10 MB+）加载需数秒到十余秒
4. 切换成功自动持久化，开机恢复上次选择；加载失败（文件损坏、内存不足）自动回退内部预设

**约束：**
- 音源加载到 PSRAM，可用空间不足时会拒绝加载并保持当前音源不变
- 加载大容量音源（多分区 / 每音多 zone）时复音与渲染已做性能优化，但极端密集连击下仍建议优先使用精简音源

---

## FTP 无线文件管理

设备内置 FTP 服务器，SD 卡整卡内容可通过局域网无线管理：给播放器传曲、取回演奏录音（.mid）、上传/替换 SoundFont 音源，全程免拔卡、免接线。

**使用方法：**
1. 确保设备已连接 Wi-Fi（状态栏出现 📶）
2. 进入 设置 → 高级设置 → FTP 文件传输，页面显示 `ftp://<设备IP>` 与账号密码（均为 `musicpad`）
3. 电脑与设备在同一局域网，用 FileZilla / WinSCP / Windows 资源管理器地址栏输入该地址即可连接
   - Windows 资源管理器若不弹账密输入框，地址栏改用完整凭据格式：`ftp://musicpad:musicpad@<设备IP>`（如 `ftp://musicpad:musicpad@192.168.1.187`）
4. 传输中页面实时显示状态、文件名与进度；页面独占系统（不熄屏、语音助手暂停、无法切走）
5. 点「退出」按钮随时主动断开并返回设置页

**说明：**
- 仅支持被动模式（PASV/EPSV），FileZilla / WinSCP / 资源管理器默认即可连
- 仅面向路由器下的本地局域网；FTP 为明文协议、固定凭据，请勿暴露公网
- 支持断点续传（REST）、文件与目录的上传 / 下载 / 删除 / 重命名 / 新建
- 上传进度按已传字节数与活跃指示显示（协议无法预知上传文件大小）；下载按百分比显示
- 仅在 FTP 页面打开期间监听 21 端口，退出即关闭

---

## SD 卡目录规范

设备各功能在 SD 卡上有约定目录。**开机时不会自动创建这些目录**，需要用户自行建立（可通过 FTP 新建），或由对应功能在首次使用时创建。

| 目录 / 文件 | 用途 | 创建时机 |
|:---|:---|:---|
| `/midi/` | 播放器「Midi文件播放」优先扫描的 `.mid` 曲目；目录缺失时回退到 SD 卡根目录扫描 | 手动创建（FTP），非必需 |
| `/music/` | 播放器「音乐播放」优先扫描的 `.mp3` 曲目；目录缺失时回退到 SD 卡根目录扫描 | 手动创建（FTP），非必需 |
| `/record/` | 演奏录制 `.mid` 文件（标准 SMF） | 录音服务初始化时创建 |
| `/soundfonts/` | 自定义 SF2 音源（设置页「音源」读取） | 手动创建（FTP） |
| `/ai_chat.txt` | AI 对话落盘（设置中开启「对话落盘」后追加写入） | 首次开启该功能时创建 |
| `/sys/src/` | 覆盖固件内嵌 UI 资源（字体/图片，同名文件优先读 SD 卡） | 手动创建（FTP） |

> 说明：`/sys/src` 仅为可选覆盖层，缺省时使用固件内置资源，无需建目录。

---

## 状态栏图标

屏幕右上角的状态栏实时指示设备状态，图标仅在对应功能就绪 / 已连接时出现，未出现时即为未连接。按从左到右的排列顺序：

| 图标 | 含义 | 出现条件 |
|:---:|:---|:---|
| 💾 | SD 卡 | microSD 卡已挂载（录制、播放器、对话落盘等功能依赖 SD 卡） |
| 📶 | Wi-Fi | 已成功连接 Wi-Fi 网络 |
| 🤖 | 小智 AI | AI 助手设置中已开启「全局唤醒」（开启前需先在 xiaozhi.me 完成绑定激活） |
| 🎧 | 耳机 | 3.5mm 耳机已插入 |
| 🔌 | USB MIDI | USB-A 口已接入 MIDI 设备（键盘 / 控制器），接入后可直接演奏所有 App |
| 🔋 | 电池 | 5 档电量指示；充电中（USB 供电）显示满格；未装电池时显示空电池 |

> 设备上实际显示的是 Font Awesome 字形图标，上表用相近 emoji 示意。

---

## 构建与发布

基于 VS Code + ESP-IDF 插件开发，ESP-IDF 版本 5.5.5。

本项目使用 **GitHub Actions** 自动构建。推送到 `v*` 标签时自动触发：

1. 使用 `espressif/idf:v5.5.5` 容器构建固件
2. 输出两个镜像：
   - `0x0_full_*.bin` — 整片 16 MB，首次烧录 / 救砖用（会清空设置）
   - `0x10000_app_*.bin` — 仅应用固件，日常升级用（保留 NVS 设置）
3. 自动创建 GitHub Release 并上传固件

手动触发：进入 GitHub 仓库 → Actions → Build and Release → Run workflow

### 烧录说明

#### 第一次拿到板子 / 全新烧录
下载 `xxx_full_16MB.bin`，用 [ESP Flash Download Tool](https://www.espressif.com/zh-hans/support/download/other-tools)
从 **0x0** 整片烧录即可。也可以命令行：

```bash
esptool.py --chip esp32p4 -p COMx write_flash 0x0 xxx_full_16MB.bin
```

> ⚠️ 整片烧录会清空所有设置（Wi-Fi 配置、校准参数等），恢复出厂状态。

#### 已经烧过旧版本，只想升级固件（推荐）
下载 `xxx_app.bin`，只刷应用区，**你的所有设置都会保留**：

```bash
esptool.py --chip esp32p4 -p COMx write_flash 0x10000 xxx_app.bin
```

#### 救砖
烧不进去、起不来 → 回到第一种方法，整片重烧。

---

## 架构概览

```
┌─────────────────────────────────────┐
│  UI Frontend (EEZ Studio + LVGL)    │
├─────────────────────────────────────┤
│  App Layer（12 个 App）             │
├─────────────────────────────────────┤
│  App Manager（生命周期、路由）       │
├─────────────────────────────────────┤
│  Task Layer（调度、胶水）            │
├─────────────────────────────────────┤
│  Services（音频、Wi-Fi、USB 等）     │
├─────────────────────────────────────┤
│  Engines（MIDI、SF2、GUI 等）        │
├─────────────────────────────────────┤
│  BSP（M5Stack Tab5）                │
└─────────────────────────────────────┘
```

**核心设计：**
- **MIDI 事件总线**：所有 App 的发声、外部 MIDI 输入统一走总线分发
- **UI 前后端隔离**：EEZ Studio 做视觉，C 代码做业务，通过控件名绑定
- **分层架构**：严格的层间依赖规则，保证代码可维护性

---

## 目录结构

```
├── components/
│   ├── app_*/              # 12 个 App
│   ├── app_manager/        # App 管理器
│   ├── engine_*/           # 引擎层（gui、midi、sf2）
│   ├── service_*/          # 服务层（audio、wifi、usb、xiaozhi 等）
│   └── task_*/             # 任务层
├── main/                   # 入口
├── tools/                  # 辅助脚本
├── doc/                    # 文档
└── CMakeLists.txt
```

---

## 致谢 / 参考项目

### UI 设计
- [EEZ Studio](https://github.com/eez-open/studio) — 开源可视化 LVGL UI 编辑器

### 音频合成
- [ESP32_SF2_Sampler_Synthesizer](https://github.com/copych/ESP32_SF2_Sampler_Synthesizer) — ESP32 SF2 采样合成器参考实现

### MP3 解码
- [micro-mp3](https://github.com/pschatzmann/micro-mp3) — 轻量 MP3 解码库（经 ESP 组件仓库 `esphome/micro-mp3` 集成）

### AI 语音
- [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) — 小智 AI 语音助手 ESP32 参考实现

### 农历计算
- [cnlunar](https://github.com/OPN48/cnlunar) — 农历/黄历离线计算库，用于时钟日历 App

### 灵感来源
- [CYD-MIDI-Controller](https://github.com/NickCulbertson/CYD-MIDI-Controller) — 禅模式（弹珠/雨滴）的灵感来源

### 字体
- [阿里妈妈方圆体](https://fonts.alibabagroup.com) — 中文界面字库（免费商用授权）
- [Font Awesome 6 Free](https://fontawesome.com) — 界面图标字库（CC BY 4.0 / SIL OFL 1.1）

### 内置音色
- **Florestan Basic GM GS** — Public Domain
  - 官网：<http://go.to/florestan>
  - 作者：<http://nandoflorestan.cjb.net>

### 数据接口
- [一言 Hitokoto](https://hitokoto.cn) — 每日一言 API
- [uAPI 天气](https://uapis.cn) — 实时天气 API

---

## License

MIT License
