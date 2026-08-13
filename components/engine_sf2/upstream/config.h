/**
 * @file config.h
 * @brief SF2 合成器核心配置（替换上游 config.h）
 *
 * 上游 config.h 含 Arduino 引脚/GUI/分区等项目配置，属移植目标工程
 * 职责，整体替换（见 UPSTREAM_PATCHES.md）。采样率与渲染帧长从
 * service_audio_config.h 取唯一来源；此处只保留合成器自身参数。
 */

#pragma once

#include "service_audio_config.h"

// ===================== AUDIO =====================
#define   DMA_BUFFER_NUM        2     // number of internal DMA buffers
#define   DMA_BUFFER_LEN        SERVICE_AUDIO_FRAMES_PER_PERIOD
#define   CHANNEL_SAMPLE_BYTES  2     // can be 1, 2, 3 or 4 (2 and 4 only supported yet)
#define   SAMPLE_RATE           SERVICE_AUDIO_SAMPLE_RATE

// ===================== MIDI =====================
#define   NUM_MIDI_CHANNELS     16

// ===================== SYNTHESIZER ===============
/* P4@360MHz 的预算远高于上游 S3@240MHz（19 voices 是上游带 FX 的实测墙）。
 * 32 声部在弦乐密集的 MIDI 文件中频繁抢音：持续长音被评分较低的
 * 新音符挤掉，听感为“长音只剩一声短促”（PC 播放器通常 64~256 声部）。
 * 上调至 48；若遥测 render_block_us 仍有余量可继续上调。 */
#define   MAX_VOICES            48
#define   MAX_VOICES_PER_NOTE   4
#define   PITCH_BEND_CENTER     0

/* FX / filter 开关：默认全部关闭，待渲染遥测确认负载余量后逐项启用。
 * 爆音治理期间保持最简渲染链：dry voice 混音 → 出口软限幅。 */
// #define ENABLE_REVERB
// #define ENABLE_CHORUS
// #define ENABLE_DELAY
// #define ENABLE_IN_VOICE_FILTERS
// #define ENABLE_CH_FILTER_M
// #define ENABLE_OVERDRIVE
// #define ENABLE_CH_FILTER

#define   CH_FILTER_MAX_FREQ    12000.0f
#define   CH_FILTER_MIN_FREQ    50.0f
#define   FILTER_MAX_Q          7.0f

#define   BOARD_HAS_PSRAM

/* 引擎适配层使用绝对路径加载（/sdcard/...、/sys_int/...）；
 * 相对路径仍拼到 SF2_PATH 下，兼容 Synth::scanSf2Files 产出的文件名。 */
#define   SF2_PATH              "/sdcard/"
