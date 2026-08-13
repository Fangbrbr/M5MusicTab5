/**
 * @file engine_midi_config.h
 * @brief MIDI 事件引擎配置参数
 *
 * 所有与引擎容量、缓冲区相关的参数统一在此配置，便于后续根据硬件资源调整。
 */

#ifndef ENGINE_MIDI_CONFIG_H
#define ENGINE_MIDI_CONFIG_H

/** @brief 事件队列长度（同时缓冲的 MIDI 事件数） */
#define ENGINE_MIDI_QUEUE_LEN          128

/** @brief 最大消费者数量 */
#define ENGINE_MIDI_MAX_CONSUMERS      16

/** @brief 单个 SysEx 消息最大长度 */
#define ENGINE_MIDI_SYSEX_BUF_SIZE     128

/** @brief 内部 SysEx 指令消息体最大长度 */
#define ENGINE_MIDI_SYSEX_CMD_MAX_LEN  128

/** @brief 解析器内部临时缓冲区大小 */
#define ENGINE_MIDI_PARSER_BUF_SIZE    128

/** @brief SMF 曲名（轨道名 meta）最大长度，含结尾 '\0' */
#define ENGINE_MIDI_SMF_SONG_NAME_MAX  64

#endif /* ENGINE_MIDI_CONFIG_H */
