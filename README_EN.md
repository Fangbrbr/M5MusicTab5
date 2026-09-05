# M5MusicTab5 — Music Exploration Terminal

**[简体中文](README.md) | English**

[![i18n](https://img.shields.io/badge/i18n-%E4%B8%AD%E6%96%87%20%2F%20English-important)]()
[![build](https://img.shields.io/badge/ESP--IDF-v5.5.5-blue)]()
[![CI](https://img.shields.io/github/actions/workflow/status/Fangbrbr/M5MusicTab5/release.yml?label=CI)]()
[![license](https://img.shields.io/badge/license-MIT-green)]()
[![platform](https://img.shields.io/badge/platform-ESP32--P4-orange)]()
[![ui](https://img.shields.io/badge/ui-LVGL%209-9cf)]()
[![audio](https://img.shields.io/badge/audio-SF2%20Synthesizer-ff69b4)]()

🔥 **No-install web flasher: [fangbrbr.github.io/M5MusicTab5](https://fangbrbr.github.io/M5MusicTab5)**

A music exploration terminal built from scratch on the M5Stack Tab5 (ESP32-P4).

📺 **[Watch the demo video](https://www.bilibili.com/video/BV1A7ua6WEH8)**

[![Demo video](doc/misc/cover.png)](https://www.bilibili.com/video/BV1A7ua6WEH8)

---

## Features

- **🇨🇳 Bilingual UI (中文 / English)** — one-tap switching in Settings, persisted across reboots
- **13 built-in apps** — Zen Mode / Ear Trainer / Chord Trainer / Circle of Fifths / Tiny Piano / Drum Pad / Player / XY Pad / Metronome / AI Voice Assistant / Clock & Calendar / Fun (Book of Answers + Tarot) / Recorder (WIP)
- **SF2 sample synthesizer** — real instrument sampling based on SoundFont 2; load custom soundfonts from the SD card
- **MP3 playback** — .mp3 files straight from SD card, ID3 title display, non-ASCII filename support
- **Standard MIDI recording & playback** — records output directly as .mid / SMF; copy to any DAW
- **Global MIDI bus** — USB Host, Bluetooth, and UART MIDI inputs with decoupled producers/consumers
- **FTP wireless file management** — manage the entire SD card over LAN, no cable needed
- **EEZ Studio visual UI** — fully isolated frontend/backend, WYSIWYG
- **Xiaozhi AI voice + MCP device control** — voice chat + device control
- **RTC + online features** — weather / news / lunar calendar / timer

---

## Hardware

| Item | Spec |
|---|---|
| MCU | ESP32-P4, dual-core RISC-V @ 360 MHz |
| Wireless | ESP32-C6 (Wi-Fi + Bluetooth 5, SDIO passthrough) |
| Memory | 32 MB PSRAM |
| Storage | 16 MB Flash + microSD slot |
| Display | 5" ST7123 capacitive touch, 1280×720 (MIPI-DSI) |
| Audio | ES8388 DAC + ES7210 dual-mic AEC, 44.1 kHz |
| Ports | USB-C OTG, USB-A Host, 3.5mm headphone jack, RS485 |
| Sensors | BMI270 6-axis IMU |

---

## App User Guide

👉 **[Complete guide for all 12 apps →](doc/apps_en.md)**

---

## SF2 Soundfont Management

The firmware ships with a built-in GM soundfont (Florestan Basic GM GS) that works out of the box, and supports loading custom SoundFonts from the SD card:

1. Put `.sf2` files in the SD card `/soundfonts/` directory
2. Go to Settings → Soundfont, pick from the dropdown ("Internal preset" or an SD card file)
3. The notification bar shows live progress; large soundfonts (10 MB+) take a few to a dozen seconds
4. Successful switches persist automatically and are restored at boot; load failures (corrupt file, out of memory) fall back to the internal preset

**Constraints:**
- Soundfonts load into PSRAM; if free space is insufficient the load is rejected and the current soundfont is kept
- Polyphony and rendering are optimized for large soundfonts (multi-preset / multi-zone-per-note), but compact soundfonts are still recommended under extremely dense playing

---

## FTP Wireless File Management

A built-in FTP server exposes the entire SD card over LAN: push songs to the player, pull performance recordings (.mid), upload/replace soundfonts — no card removal, no cables.

**Usage:**
1. Make sure the device is on Wi-Fi (📶 in the status bar)
2. Go to Settings → Advanced → FTP file transfer; the page shows `ftp://<device IP>` with credentials (both `musicpad`)
3. On the same LAN, connect with FileZilla / WinSCP / your OS file manager
   - If Windows Explorer doesn't pop up the credential dialog, enter the full credential URL in the address bar instead: `ftp://musicpad:musicpad@<device IP>` (e.g. `ftp://musicpad:musicpad@192.168.1.187`)
4. During transfer the page shows live status, filename, and progress; the page takes over the system (no sleep, voice assistant paused, no switching away)
5. Tap "Exit" anytime to disconnect and return to Settings

**Notes:**
- Passive mode only (PASV/EPSV); FileZilla / WinSCP / Explorer work by default
- Intended for local LANs behind a router only; FTP is plaintext with fixed credentials — never expose it to the public internet
- Supports resume (REST), upload / download / delete / rename / mkdir for files and directories
- Upload progress shows transferred bytes + activity indicator (the protocol can't know upload size in advance); downloads show percentage
- Port 21 is only listened on while the FTP page is open; it closes when you leave

---

## SD Card Directory Conventions

Each feature uses conventional directories on the SD card. **They are not auto-created at boot** — create them yourself (via FTP) or let the corresponding feature create them on first use.

| Directory / file | Purpose | Created when |
|:---|:---|:---|
| `/midi/` | Preferred scan dir for .mid files in "MIDI files" mode; falls back to SD root if missing | Manually (FTP), optional |
| `/music/` | Preferred scan dir for .mp3 files in "Music" mode; falls back to SD root if missing | Manually (FTP), optional |
| `/record/` | Performance recordings (.mid, standard SMF) | Created by the recorder service on init |
| `/soundfonts/` | Custom SF2 soundfonts (read by Settings → Soundfont) | Manually (FTP) |
| `/ai_chat.txt` | AI chat log (appended when "chat logging" is enabled) | On first enable |
| `/sys/src/` | Overrides firmware-embedded UI assets (fonts/images; same-named files on SD take priority) | Manually (FTP) |

> Note: `/sys/src` is an optional overlay layer; the firmware falls back to built-in assets, no directory needed.

---

## Status Bar Icons

The status bar (top right) shows device state in real time; icons appear only when the corresponding feature is ready/connected. From left to right:

| Icon | Meaning | Appears when |
|:---:|:---|:---|
| 💾 | SD card | microSD card mounted (recording, player, chat logging depend on it) |
| 📶 | Wi-Fi | Wi-Fi connected |
| 🤖 | Xiaozhi AI | "Global wake" enabled in AI settings (requires prior binding at xiaozhi.me) |
| 🎧 | Headphones | 3.5mm headphones plugged in |
| 🔌 | USB MIDI | A MIDI device (keyboard/controller) on the USB-A port — play all apps directly |
| 🔋 | Battery | 5-level gauge; full when charging (USB powered); empty battery icon when no battery installed |

> The device actually renders Font Awesome glyphs; the table uses approximate emoji.

---

## Build & Release

Developed with VS Code + the ESP-IDF extension, ESP-IDF v5.5.5.

Built automatically via **GitHub Actions**. Pushing a `v*` tag triggers:

1. Firmware build in the `espressif/idf:v5.5.5` container
2. Two images:
   - `0x0_full_*.bin` — full 16 MB image for first flash / unbricking (wipes settings)
   - `0x10000_app_*.bin` — app-only image for routine upgrades (keeps NVS settings)
3. Automatic GitHub Release with the firmware attached

Manual trigger: repo → Actions → Build and Release → Run workflow

### Flashing

#### First time / fresh flash
Download `xxx_full_16MB.bin` and flash it at **0x0** with the [ESP Flash Download Tool](https://www.espressif.com/zh-hans/support/download/other-tools), or via CLI:

```bash
esptool.py --chip esp32p4 -p COMx write_flash 0x0 xxx_full_16MB.bin
```

> ⚠️ A full flash wipes all settings (Wi-Fi config, calibration, etc.) back to factory state.

#### Upgrading from an older build (recommended)
Download `xxx_app.bin` and flash only the app partition — **all your settings are kept**:

```bash
esptool.py --chip esp32p4 -p COMx write_flash 0x10000 xxx_app.bin
```

#### Unbricking
Won't flash, won't boot → go back to the full-flash method.

---

## Architecture

```
┌─────────────────────────────────────┐
│  UI Frontend (EEZ Studio + LVGL)    │
├─────────────────────────────────────┤
│  App Layer (12 apps)                │
├─────────────────────────────────────┤
│  App Manager (lifecycle, routing)   │
├─────────────────────────────────────┤
│  Task Layer (scheduling, glue)      │
├─────────────────────────────────────┤
│  Services (audio, Wi-Fi, USB, ...)  │
├─────────────────────────────────────┤
│  Engines (MIDI, SF2, GUI, ...)      │
├─────────────────────────────────────┤
│  BSP (M5Stack Tab5)                 │
└─────────────────────────────────────┘
```

**Core design:**
- **MIDI event bus**: all app sound generation and external MIDI input go through one bus
- **UI frontend/backend isolation**: EEZ Studio for visuals, C for business logic, bound by widget names
- **Layered architecture**: strict inter-layer dependency rules keep the code maintainable

---

## Directory Structure

```
├── components/
│   ├── app_*/              # 12 apps
│   ├── app_manager/        # App manager
│   ├── engine_*/           # Engine layer (gui, midi, sf2)
│   ├── service_*/          # Service layer (audio, wifi, usb, xiaozhi, ...)
│   └── task_*/             # Task layer
├── main/                   # Entry point
├── tools/                  # Helper scripts
├── doc/                    # Documentation
└── CMakeLists.txt
```

---

## Acknowledgements / References

### UI design
- [EEZ Studio](https://github.com/eez-open/studio) — open-source visual LVGL UI editor

### Audio synthesis
- [ESP32_SF2_Sampler_Synthesizer](https://github.com/copych/ESP32_SF2_Sampler_Synthesizer) — reference ESP32 SF2 sampler synthesizer implementation

### MP3 decoding
- [micro-mp3](https://github.com/pschatzmann/micro-mp3) — lightweight MP3 decoder (integrated via the ESP component registry as `esphome/micro-mp3`)

### AI voice
- [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) — Xiaozhi AI voice assistant reference implementation for ESP32

### Lunar calendar
- [cnlunar](https://github.com/OPN48/cnlunar) — offline lunar/almanac calculation library, used by the Clock & Calendar app

### Inspiration
- [CYD-MIDI-Controller](https://github.com/NickCulbertson/CYD-MIDI-Controller) — inspiration for Zen Mode (marbles/raindrops)

### Fonts
- [AlimamaFangYuanTi](https://fonts.alibabagroup.com) — Chinese UI font (free for commercial use)
- [Font Awesome 6 Free](https://fontawesome.com) — UI icon font (CC BY 4.0 / SIL OFL 1.1)

### Built-in soundfont
- **Florestan Basic GM GS** — Public Domain
  - Website: <http://go.to/florestan>
  - Author: <http://nandoflorestan.cjb.net>

### Data APIs
- [Hitokoto](https://hitokoto.cn) — daily quote API
- [uAPI Weather](https://uapis.cn) — real-time weather API

---

## License

MIT License
