# App User Guide

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
- Tap the "Circle of Fifths" button to open the circle panel (see next section)

---

### 4. Circle of Fifths

A music-theory visualization: keys, chords, and modulation relationships at a glance. Integrated into Chord Trainer as a panel.

**Usage:**
- Entry: tap the "Circle of Fifths" button in Chord Trainer
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

### 6. Sequencer

An 8-track drum step sequencer with a 16/32-step grid, one-tap Euclidean rhythm generation, and pattern file management.

**Usage:**
- **Grid editing**: short-press a cell to toggle the step; long-press a cell to open per-step settings (velocity / trigger probability / CC lock)
- **Tracks**: tap one of the 8 track rows on the left to select it; the parameter panel offers 23 GM percussion sounds, mute / solo, velocity, trigger probability, and timing randomization (humanize)
- **Steps & pages**: long-press the page button to switch 16/32-step mode; short-press flips between pages A/B in 32-step mode; the clear button clears the current page
- **Playback**: BPM slider (20~300), play/stop button, live playhead cursor in the grid
- **Randomize**: one-tap rhythm generation (Euclidean algorithm) styled per each track's selected sound
- **Pattern slots (A~F)**: 6 in-RAM slots; tap to select the file target, long-press to switch the playing pattern (quantized to the beat while playing; the pending slot blinks)
- **Save / Load**: save the current pattern as a `.m5p` file under `/sdcard/sequencer/` (auto-named by date & time); loading opens the file list — pick a file to write into the target slot, files can be deleted in the list
- Recording supported: the record button captures the sequencer output as standard .mid (`/sdcard/record/`)

---

### 7. Player

Three-mode player: SD music (.mp3) / MIDI files (.mid) / performance recordings (.mid).

**Usage:**
- Three mutually exclusive mode buttons:
  - **Music**: scans `/sdcard/music` for .mp3 (falls back to SD root if missing)
  - **MIDI files**: scans `/sdcard/midi` for .mid (falls back to SD root if missing)
  - **Recordings**: scans `/sdcard/record` for .mid recordings
- Tap a file in the left list; MIDI/recording lists support **long-press a file for delete confirmation**
- Right side controls: previous / play-stop / next
- Seekable progress bar
- Shows title and path; MIDI/recording modes also show channel count and BPM
- Non-ASCII filenames supported; MP3 titles prefer ID3v2 tags (UTF-8 / UTF-16 auto-detected), falling back to filename
- MP3 auto-advances to the next track, looping from the end back to the first
- During MP3 playback the synth chain is muted and the voice assistant pauses; both resume when playback stops or you leave the app
- All recordings from record-capable apps (Tiny Piano, Sequencer, XY Pad, Zen Mode, Ear Trainer) appear in recording mode and can be played back

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

### 13. Recorder

A WAV recorder with three scene modes; files are saved to `/sdcard/wav/`.

**Usage:**
- **Scene mode** dropdown (locked while recording):
  - **Voice**: mono 16 kHz, compact size, for speech notes
  - **Instrument**: stereo 44.1 kHz, high quality, for instrument pickup (the voice assistant auto-pauses during recording and resumes afterwards)
  - **Ambient**: dual-mic AEC (acoustic echo cancellation) enabled
- Big round button to start/stop recording; elapsed time and a live level meter are shown while recording
- Live SD-card storage usage indicator
- File list: tap to audition (tap again to stop); long-press a file for delete confirmation
- Files are auto-named by date & time (.wav); same-second collisions get a numeric suffix
