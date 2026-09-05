/**
 * @file engine_midi.h
 * @brief host_test 用的 engine_midi 最小 stub：仅覆盖 engine_sequencer 所需接口
 *
 * 主构建走真实 engine_midi（IDF 组件）；此 stub 仅让纯 gcc 的 host 测试可链接。
 * host_test 目录内重名 shadow 真实头，不影响主构建（主构建 include 路径不含此目录）。
 */
#ifndef HOST_STUB_ENGINE_MIDI_H
#define HOST_STUB_ENGINE_MIDI_H

#include <stdint.h>

#define ENGINE_MIDI_SYSEX_BUF_SIZE 128
#define ENGINE_MIDI_SYSEX_CMD_MAX_LEN 16

#define ENGINE_MIDI_MSG_NOTE_OFF         0x80
#define ENGINE_MIDI_MSG_NOTE_ON          0x90
#define ENGINE_MIDI_MSG_POLY_PRESSURE    0xA0
#define ENGINE_MIDI_MSG_CONTROL_CHANGE   0xB0
#define ENGINE_MIDI_MSG_PROGRAM_CHANGE   0xC0
#define ENGINE_MIDI_MSG_CHANNEL_PRESSURE 0xD0
#define ENGINE_MIDI_MSG_PITCH_BEND       0xE0

#define ENGINE_MIDI_PORT_INTERNAL   0
#define ENGINE_MIDI_PORT_UART       1
#define ENGINE_MIDI_PORT_USB_HOST   2
#define ENGINE_MIDI_PORT_BLE        3
#define ENGINE_MIDI_PORT_USB_DEVICE 4
#define ENGINE_MIDI_PORT_APP        5

typedef struct {
    uint32_t timestamp;
    uint8_t  type;
    uint8_t  channel;
    uint8_t  data1;
    uint8_t  data2;
    uint16_t value;
    uint8_t  sysex_len;
    uint8_t  sysex_data[ENGINE_MIDI_SYSEX_BUF_SIZE];
    uint8_t  source_port;
} engine_midi_event_t;

/* host 端注入：记录事件到全局数组（test_main.c 定义） */
extern void host_midi_record(const engine_midi_event_t *evt);

int engine_midi_publish(const engine_midi_event_t *evt, unsigned int timeout_ms);

#endif /* HOST_STUB_ENGINE_MIDI_H */
