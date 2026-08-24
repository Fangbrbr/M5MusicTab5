# M5MusicTab5 — Music Exploration Terminal

**[简体中文](README.md) | English**

[![build](https://img.shields.io/badge/build-ESP--IDF%20v5.4-blue)]()
[![CI](https://img.shields.io/github/actions/workflow/status/Fangbrbr/M5MusicTab5/release.yml?label=CI)]()
[![license](https://img.shields.io/badge/license-MIT-green)]()
[![platform](https://img.shields.io/badge/platform-ESP32--P4-orange)]()
[![ui](https://img.shields.io/badge/ui-LVGL%209-9cf)]()
[![audio](https://img.shields.io/badge/audio-SF2%20Synthesizer-ff69b4)]()

🔥 **No-install web flasher: [fangbrbr.github.io/M5MusicTab5](https://fangbrbr.github.io/M5MusicTab5)**

> This is not a tablet — it's a music exploration terminal with firmware written from scratch.
>
> Built on the M5Stack Tab5 (ESP32-P4): from a bare dev board to a complete firmware with 12 apps, all hand-built.

📺 **[Watch the demo video](https://www.bilibili.com/video/BV1A7ua6WEH8)**

[![Demo video](doc/misc/cover.png)](https://www.bilibili.com/video/BV1A7ua6WEH8)

---

## Features

- **12 built-in apps** — Zen Mode / Ear Trainer / Chord Trainer / Circle of Fifths / Tiny Piano / Drum Pad / Player / XY Pad / Metronome / AI Voice Assistant / Clock & Calendar / Fun (Book of Answers + Tarot)
- **SF2 sample synthesizer** — real instrument sampling based on SoundFont 2; load custom soundfonts from the SD card (one-tap switching in Settings, last selection restored at boot), with polyphony optimizations for large soundfonts
- **MP3 playback** — the Player app plays .mp3 files straight from the SD card (ID3 title display, auto-advance), full support for non-ASCII filenames
- **Global MIDI bus** — USB Host, Bluetooth, and UART MIDI inputs with a decoupled producer/consumer architecture
- **Performance recording & playback** — apps that support recording start/stop with a single tap; performances are saved to the SD card as .hmr (MIDI event streams) and replayed in the Player app
- **FTP wireless file management** — one-tap enable in Settings; manage the entire SD card over LAN with FileZilla / your OS file manager (transfer MIDI songs, pull performance recordings, upload/replace soundfonts)
- **EEZ Studio visual UI** — fully isolated frontend/backend, WYSIWYG interface design
- **Xiaozhi AI voice + MCP device control** — voice chat + device control (query device/firmware info, control brightness/volume/theme, launch apps)
- **RTC + online features** — weather / news / lunar calendar / Chinese almanac

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

## The 12 Apps

### 1. Zen Mode

A focus & relaxation tool where bouncing marbles or falling raindrops trigger notes.

**Usage:**
- Three dropdowns at the top:
  - **Mode**: Marble / Raindrop
  - **Scale**: Major / Chinese pentatonic / Minor / Japanese minor
  - **Parameter**: Marble mode — 5 speed levels / Raindrop mode — 1~5 simultaneous drops
- **Marble mode**: tap the canvas to place balls (up to 4); balls bounce at constant speed in the active area, triggering notes on wall hits
- **Raindrop mode**: drops fall automatically from the top, bounce off walls, and are destroyed when they leave the canvas
- Balls attract each other gravitationally, curving their trajectories
- Wall segments map to different pitches, picked from the selected scale
- Supports recording

---

### 2. Ear Trainer

An ear-training tool with absolute-pitch and relative-pitch modes and difficulty levels.

**Background:**
- **Note names**: the names of pitches — A B C D E F G — each mapping to a fixed frequency
- **Concert A = 440 Hz**: the international tuning standard; A4 vibrates 440 times per second
- **Twelve-tone equal temperament**: dividing an octave into 12 equal semitones, each a frequency ratio of 2^(1/12). A mathematical compromise rather than physical purity — just intonation gives perfect integer ratios per interval but makes modulation awkward; equal temperament sacrifices the purity of every interval to gain free modulation
- **Absolute pitch**: hearing a note and naming it
- **Relative pitch**: given a reference note, identifying the interval of another note

**Usage:**
- Main screen: choose training mode (absolute/relative), difficulty (easy/medium/hard), practice/challenge mode
- Tap "Play" to hear the note
- Absolute mode: answer on the 12-key piano matrix
- Relative mode: answer with interval buttons
- Tap replay to hear it again
- **Practice mode**: shows the answer before each question, no penalties
- **Challenge mode**: 3 lives; wrong answers cost one, 3-second restart after game over
- High scores persisted per mode × difficulty (6 groups)
- Supports recording

---

### 3. Chord Trainer

A chord learning tool: a two-octave piano roll highlights chord tones, great for inversion design.

**What is a chord:**
Three or more notes of different pitches stacked in specific interval relationships. The most common are triads (root + third + fifth) and seventh chords.

**Usage:**
- Tap the 12-key root matrix to switch roots
- 15 chord-type buttons: major / minor / augmented / diminished / sus2 / sus4 / maj7 / dominant 7 / min7 / half-diminished / diminished 7 / sus2(7) / sus4(7) / add9 / 9th
- Canvas draws a two-octave piano roll with chord tones highlighted (root emphasized)
- **Tap the canvas to toggle block / arpeggiated playback**
- Shows chord name and interval-stack definition
- **Two-octave roll display** — very handy for designing chord inversions

---

### 4. Circle of Fifths

A music-theory visualization: keys, chords, and modulation relationships at a glance.

**Usage:**
- Tap a sector on the circle canvas to switch between the 12 key signatures
- Clockwise = sharp keys (one more sharp per step), counter-clockwise = flat keys
- Shows: key name, signature (e.g. 1# / 3b), relative minor, parallel minor, scale spelling
- Tap the piano canvas below to hear the ascending scale of the current key (8 notes, 200 ms steps)

---

### 5. Tiny Piano

3×5 matrix mode + piano-key mode, up to 5-finger multi-touch.

**Usage:**
- Tap pads / keys to play
- Tap the settings button (top right):
  - **Display**: matrix mode / piano mode (two octaves)
  - **Scale**: major / minor / Chinese pentatonic / Egyptian / Dorian / Japanese
  - **Root note**: 13 positions
  - **Instrument**: 16 SF2 sounds
  - **Start octave**: C0 ~ C6 roller
- Tap the record button to record your performance

---

### 6. Drum Pad

Virtual drum kit + drum-pad matrix layouts, up to 5-finger multi-touch.

**Usage:**
- Tap pads to play; everything goes through MIDI channel 10 GM drum mapping
- Settings (top right):
  - **Layout**: virtual kit (canvas-drawn 8 circular pads) / pad matrix (2×4 buttons)
  - **Kit**: 9 GM drum kits
- Supports recording
- Kick, snare, hi-hat (closed/open), toms (high/mid/low), ride — all included

---

### 7. Player

Three-mode player: SD music (.mp3) / MIDI files (.mid) / performance recordings (.hmr).

**Usage:**
- Three mutually exclusive mode buttons:
  - **Music**: scans `/sdcard/music` for .mp3 (falls back to SD root if missing)
  - **MIDI files**: scans `/sdcard/midi` for .mid (falls back to SD root if missing)
  - **Recordings**: scans `/sdcard/record` for .hmr files
- Tap a file in the left list
- Right side controls: previous / play-stop / next
- Seekable progress bar
- Shows title and path; MIDI/recording modes also show channel count and BPM
- Non-ASCII filenames supported; MP3 titles prefer ID3v2 tags (UTF-8 / UTF-16 auto-detected), falling back to filename
- MP3 auto-advances to the next track, looping from the end back to the first
- During MP3 playback the synth chain is muted and the voice assistant pauses; both resume when playback stops or you leave the app
- All recordings from record-capable apps (Tiny Piano, Drum Pad, XY Pad, Zen Mode, Ear Trainer) appear in recording mode and can be played back

---

### 8. XY Pad

Continuous-pitch performance mode, up to 3 simultaneous touches, with pitch-bend glissando.

**Usage:**
- Slide on the pad: X axis = pitch (low left, high right), Y axis = volume (loud top, quiet bottom)
- Up to 3 fingers, each on its own MIDI channel
- Continuous pitch-bend glissando (±24 semitone bend range); sliding within the bend window only bends without re-triggering
- 3 LED widgets indicate touch positions
- Settings (top right):
  - **Instrument**: 28 GM sounds
  - **Pitch curve**: erhu-string-length law / linear
- Supports recording

---

### 9. Metronome

A professional metronome with multiple tempo-input methods and sounds.

**Usage:**
- Tap play/stop to start or stop
- **Tempo control**:
  - +/- buttons for fine adjustment
  - Slider for coarse adjustment
  - TAP tempo (tap in rhythm to compute BPM)
- 16 beat lights, one per beat, accented first beat
- Settings (top right):
  - **BPM range**: 20 ~ 300
  - **Time signature**: numerator 1~16 / denominator 4/6/8/16/32
  - **Sound**: 7 options (standard / tick / woodblock / drum kit / percussion / haptic-style / 2-4 accent)
- BPM / time signature / sound auto-saved

---

### 10. AI Voice Assistant

Voice chat + device control based on the Xiaozhi protocol.

**Usage:**
- Wake-up: say the wake word "Hi Miaomiao" (「Hi 喵喵」), or hold the talk button and release to send
- Chat bubbles stream the conversation
- Device control supported: brightness, volume, theme switching, open/close apps (MCP protocol)
- Up to 20 messages of history
- When unbound, shows a 6-digit activation code and guides you to bind at xiaozhi.me

- Settings (top right):
  - Chat logging toggle (appends to SD card ai_chat.txt)
  - Global wake-word toggle (requires prior binding at xiaozhi.me)
  - Reset device binding

---

### 11. Clock & Calendar

Three panels: weather-news clock / monthly calendar with almanac / timer.

**Usage:**
- Tap the settings button (top right); the three bottom buttons switch panels: clock / calendar / timer

**Weather & news clock:**
- Large-font current time
- Real-time weather fetched online
- Scrolling news + daily quote at the bottom (auto-refresh every 30 min)
- News panel auto show/hide cycle (10s visible / 5s hidden)

**Monthly calendar + Chinese lunar almanac:**
- Tap a date to select it
- Tap the almanac panel to flip pages (4 pages)
- Shows: lunar date, dos & don'ts, ganzhi, solar terms, constellations, clashes, nayin

**Timer:**
- +/- buttons and quick presets (1/3/5/10/20/30/40/50/60 min)
- Start / pause controls
- Alarm on completion

- Settings: 12/24-hour format, clock font

---

### 12. Fun (Book of Answers + Tarot)

Book of Answers and Tarot, triggered by flipping the device via the IMU, with a psychic-energy system.

**Usage:**
1. Settings (top right) → bottom buttons switch mode: Book of Answers or Tarot
2. Silently ask your question
3. Flip the device face-down → flip it back
4. Your answer / cards appear
5. Book of Answers: a one-line answer
6. Tarot: draws 3 cards with upright/reversed orientations; **tap a card to read its meaning**

**Psychic-energy system:**
- A "Psychic Energy" value at the top, refilled to 100% daily
- Each draw randomly consumes 10~50 energy
- Out of energy for the day? Wait for tomorrow's refresh

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

A built-in FTP server exposes the entire SD card over LAN: push songs to the player, pull performance recordings (.hmr), upload/replace soundfonts — no card removal, no cables.

**Usage:**
1. Make sure the device is on Wi-Fi (📶 in the status bar)
2. Go to Settings → Advanced → FTP file transfer; the page shows `ftp://<device IP>` with credentials (both `musicpad`)
3. On the same LAN, connect with FileZilla / WinSCP / your OS file manager
   — (Note: on my Windows 11, Explorer doesn't pop up the credential dialog; use ftp://musicpad:musicpad@<IP shown on screen>, e.g. ftp://musicpad:musicpad@192.168.1.187)
5. During transfer the page shows live status, filename, and progress; the page takes over the system (no sleep, voice assistant paused, no switching away)
6. Tap "Exit" anytime to disconnect and return to Settings

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
| `/record/` | Performance recordings (.hmr) | Created by the recorder service on init |
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

## Development Environment

Developed with VS Code + the ESP-IDF extension, ESP-IDF v5.4.4.

---

## Build & Release

Built automatically via **GitHub Actions**. Pushing a `v*` tag triggers:

1. Firmware build in the `espressif/idf:v5.4.4` container
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
- **MIDI event bus**: all app sound generation and external MIDI input go through one bus with decoupled producers/consumers
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

This project used or learned from the following open-source projects — many thanks:

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

### Built-in soundfont
- **Florestan Basic GM GS** — Public Domain
  - Website: <http://go.to/florestan>
  - Author: <http://nandoflorestan.cjb.net>

### Data APIs
- [Hitokoto](https://hitokoto.cn) — daily quote API
- [uAPI Weather](https://uapis.cn) — real-time weather API

### Large language models

The following LLMs / AI tools assisted design, coding, and debugging during development:

- [Kimi](https://www.kimi.com) — AI assistant by Moonshot AI
- [Doubao Seedance](https://seed.bytedance.com/zh/) — multimodal LLM by ByteDance Seed
- [Qwen 3.8](https://qwen.ai) — Alibaba Tongyi Qianwen LLM
- [Trae](https://www.trae.ai) — AI coding IDE by ByteDance
- [Meituan LongCat](https://longcat.ai) — open-source LLM by Meituan

---

## License

MIT License
