/**
 * @file engine_midi.h
 * @brief MIDI 事件总线引擎
 *
 * 系统内部唯一事件分发通道。支持 MIDI 1.0 全部通道消息与系统消息解析，
 * 并通过 SysEx 承载项目内部控制指令。多生产者随时投流，多消费者按类型/通道订阅。
 */

#ifndef ENGINE_MIDI_H
#define ENGINE_MIDI_H

#include "engine_midi_config.h"
#include "esp_err.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MIDI 状态字节高 4 位定义
 */
typedef enum {
    ENGINE_MIDI_STATUS_NOTE_OFF         = 0x80,  /**< 音符关 */
    ENGINE_MIDI_STATUS_NOTE_ON          = 0x90,  /**< 音符开 */
    ENGINE_MIDI_STATUS_POLY_PRESSURE    = 0xA0,  /**< 复音触后 */
    ENGINE_MIDI_STATUS_CONTROL_CHANGE   = 0xB0,  /**< 控制变更 CC */
    ENGINE_MIDI_STATUS_PROGRAM_CHANGE   = 0xC0,  /**< 程序变更 */
    ENGINE_MIDI_STATUS_CHANNEL_PRESSURE = 0xD0,  /**< 通道触后 */
    ENGINE_MIDI_STATUS_PITCH_BEND       = 0xE0,  /**< 弯音轮 */
    ENGINE_MIDI_STATUS_SYSTEM_COMMON    = 0xF0,  /**< 系统消息 */
} engine_midi_status_t;

/**
 * @brief MIDI 消息类型
 */
typedef enum {
    ENGINE_MIDI_MSG_NOTE_OFF         = 0x80,  /**< 音符关 */
    ENGINE_MIDI_MSG_NOTE_ON          = 0x90,  /**< 音符开 */
    ENGINE_MIDI_MSG_POLY_PRESSURE    = 0xA0,  /**< 复音触后 */
    ENGINE_MIDI_MSG_CONTROL_CHANGE   = 0xB0,  /**< 控制变更 CC */
    ENGINE_MIDI_MSG_PROGRAM_CHANGE   = 0xC0,  /**< 程序变更 */
    ENGINE_MIDI_MSG_CHANNEL_PRESSURE = 0xD0,  /**< 通道触后 */
    ENGINE_MIDI_MSG_PITCH_BEND       = 0xE0,  /**< 弯音轮 */
    ENGINE_MIDI_MSG_SYSEX            = 0xF0,  /**< 系统专用消息 */
    ENGINE_MIDI_MSG_TIME_CODE        = 0xF1,  /**< MIDI 时间码 */
    ENGINE_MIDI_MSG_SONG_POSITION    = 0xF2,  /**< 歌曲位置指针 */
    ENGINE_MIDI_MSG_SONG_SELECT      = 0xF3,  /**< 歌曲选择 */
    ENGINE_MIDI_MSG_TUNE_REQUEST     = 0xF6,  /**< 调音请求 */
    ENGINE_MIDI_MSG_TIMING_CLOCK     = 0xF8,  /**< 时钟 */
    ENGINE_MIDI_MSG_START            = 0xFA,  /**< 开始 */
    ENGINE_MIDI_MSG_CONTINUE         = 0xFB,  /**< 继续 */
    ENGINE_MIDI_MSG_STOP             = 0xFC,  /**< 停止 */
    ENGINE_MIDI_MSG_ACTIVE_SENSING   = 0xFE,  /**< 活跃检测 */
    ENGINE_MIDI_MSG_SYSTEM_RESET     = 0xFF,  /**< 系统复位 */
} engine_midi_msg_type_t;

/**
 * @brief 消息类型订阅掩码
 *
 * 用于 engine_midi_subscribe() 的 type_mask 参数，按位对应消息类型。
 */
#define ENGINE_MIDI_MASK_NOTE_OFF         (1UL << 0)    /**< 音符关 */
#define ENGINE_MIDI_MASK_NOTE_ON          (1UL << 1)    /**< 音符开 */
#define ENGINE_MIDI_MASK_POLY_PRESSURE    (1UL << 2)    /**< 复音触后 */
#define ENGINE_MIDI_MASK_CONTROL_CHANGE   (1UL << 3)    /**< 控制变更 CC */
#define ENGINE_MIDI_MASK_PROGRAM_CHANGE   (1UL << 4)    /**< 程序变更 */
#define ENGINE_MIDI_MASK_CHANNEL_PRESSURE (1UL << 5)    /**< 通道触后 */
#define ENGINE_MIDI_MASK_PITCH_BEND       (1UL << 6)    /**< 弯音轮 */
#define ENGINE_MIDI_MASK_SYSEX            (1UL << 7)    /**< 系统专用消息 */
#define ENGINE_MIDI_MASK_TIME_CODE        (1UL << 8)    /**< MIDI 时间码 */
#define ENGINE_MIDI_MASK_SONG_POSITION    (1UL << 9)    /**< 歌曲位置指针 */
#define ENGINE_MIDI_MASK_SONG_SELECT      (1UL << 10)   /**< 歌曲选择 */
#define ENGINE_MIDI_MASK_TUNE_REQUEST     (1UL << 11)   /**< 调音请求 */
#define ENGINE_MIDI_MASK_TIMING_CLOCK     (1UL << 12)   /**< 时钟 */
#define ENGINE_MIDI_MASK_START            (1UL << 13)   /**< 开始 */
#define ENGINE_MIDI_MASK_CONTINUE         (1UL << 14)   /**< 继续 */
#define ENGINE_MIDI_MASK_STOP             (1UL << 15)   /**< 停止 */
#define ENGINE_MIDI_MASK_ACTIVE_SENSING   (1UL << 16)   /**< 活跃检测 */
#define ENGINE_MIDI_MASK_SYSTEM_RESET     (1UL << 17)   /**< 系统复位 */
#define ENGINE_MIDI_MASK_ALL              0xFFFFFFFFUL  /**< 全部消息类型 */

/**
 * @brief 通道订阅掩码辅助宏
 * @param[in] ch 通道号 0-15
 */
#define ENGINE_MIDI_CHANNEL_MASK(ch)      (1U << (ch))

/**
 * @brief 统一 MIDI 事件结构体
 *
 * SysEx 数据内嵌固定 128 字节数组，队列可直接完整拷贝，保证多生产者/多消费者线程安全。
 */
/**
 * @brief MIDI 事件来源端口定义
 *
 * 用于 engine_midi_event_t.source_port，区分事件来源，防止双工输出时回环。
 */
#define ENGINE_MIDI_PORT_INTERNAL   0  /**< 内部/未知来源 */
#define ENGINE_MIDI_PORT_UART       1  /**< UART MIDI */
#define ENGINE_MIDI_PORT_USB_HOST   2  /**< USB Host MIDI（USB-A 接入的 MIDI 键盘） */
#define ENGINE_MIDI_PORT_BLE        3  /**< BLE MIDI */
#define ENGINE_MIDI_PORT_USB_DEVICE 4  /**< USB Device MIDI（Tab5 作为 MIDI 设备输出到 PC） */
#define ENGINE_MIDI_PORT_APP        5  /**< App 主动发布的 SysEx，避免回环 */

typedef struct {
    uint32_t timestamp;                              /**< 接收时间戳（ms） */
    uint8_t  type;                                   /**< 消息类型，见 engine_midi_msg_type_t */
    uint8_t  channel;                                /**< 通道号 0-15，系统消息此字段为 0 */
    uint8_t  data1;                                  /**< 数据字节 1 */
    uint8_t  data2;                                  /**< 数据字节 2 */
    uint16_t value;                                  /**< 扩展值：Pitch Bend 14-bit 等 */
    uint8_t  sysex_len;                              /**< SysEx 有效长度 */
    uint8_t  sysex_data[ENGINE_MIDI_SYSEX_BUF_SIZE]; /**< SysEx 数据缓冲区 */
    uint8_t  source_port;                            /**< 事件来源端口，见 ENGINE_MIDI_PORT_* */
} engine_midi_event_t;

/**
 * @brief 内部 SysEx 指令结构体
 *
 * 内部 SysEx 不使用 vendor id，固定 4 字节有效载荷：
 * [cmd_code] [func_code] [para1] [para2]
 * 对应 MIDI 线路上完整帧为：0xF0 [cmd] [func] [para1] [para2] 0xF7
 */
typedef struct {
    uint8_t vendor_id;                                    /**< 保留，固定为 0 */
    uint8_t cmd_code;                                     /**< 指令码 */
    uint8_t func_code;                                    /**< 功能码 */
    uint8_t payload_len;                                  /**< 消息体长度 */
    uint8_t payload[ENGINE_MIDI_SYSEX_CMD_MAX_LEN];       /**< 消息体，前 2 字节为 para1/para2 */
} engine_midi_sysex_cmd_t;

/**
 * @brief 消费者回调类型
 */
typedef void (*engine_midi_consumer_cb_t)(const engine_midi_event_t *evt, void *user_data);

/**
 * @brief 初始化 MIDI 事件总线
 */
esp_err_t engine_midi_init(void);

/**
 * @brief 向解析器投喂单个 MIDI 字节
 *
 * 供 USB/BLE/UART 等 service 调用，内部线程安全。
 * 来源端口标记为 ENGINE_MIDI_PORT_INTERNAL。
 * @param[in] byte MIDI 字节
 */
esp_err_t engine_midi_feed_byte(uint8_t byte);

/**
 * @brief 向解析器投喂单个 MIDI 字节，并指定来源端口
 *
 * 供具体 transport（UART/USB/BLE）调用，用于双工输出时防止回环。
 * @param[in] byte MIDI 字节
 * @param[in] port 来源端口，见 ENGINE_MIDI_PORT_*
 */
esp_err_t engine_midi_feed_byte_from_port(uint8_t byte, uint8_t port);

/**
 * @brief 向解析器投喂一串 MIDI 字节
 * @param[in] data 数据指针
 * @param[in] len  长度
 */
esp_err_t engine_midi_feed_stream(const uint8_t *data, uint32_t len);

/**
 * @brief 向解析器投喂一串 MIDI 字节，并指定来源端口
 * @param[in] data 数据指针
 * @param[in] len  长度
 * @param[in] port 来源端口，见 ENGINE_MIDI_PORT_*
 */
esp_err_t engine_midi_feed_stream_from_port(const uint8_t *data, uint32_t len, uint8_t port);

/**
 * @brief 直接发布一个已解析的事件
 *
 * 适合 UI、文件解析器等非字节流生产者。
 * @param[in] evt       事件指针
 * @param[in] timeout_ms 队列超时时间（ms）
 */
esp_err_t engine_midi_publish(const engine_midi_event_t *evt, uint32_t timeout_ms);

/**
 * @brief 订阅 MIDI 事件
 *
 * @param[in] type_mask    消息类型掩码，见 ENGINE_MIDI_MASK_* 宏
 * @param[in] channel_mask 通道掩码，位 0-15 对应通道 0-15，系统消息忽略此字段
 * @param[in] cb           回调函数
 * @param[in] user_data    用户数据
 */
esp_err_t engine_midi_subscribe(uint32_t type_mask, uint16_t channel_mask,
                                engine_midi_consumer_cb_t cb, void *user_data);

/**
 * @brief 取消订阅
 * @param[in] cb 回调函数指针
 */
esp_err_t engine_midi_unsubscribe(engine_midi_consumer_cb_t cb);

/**
 * @brief 处理并分发队列中的 MIDI 事件
 *
 * 通常在 task_gui 或 task_comm 中周期性调用。
 */
void engine_midi_process(void);

/**
 * @brief 将 SysEx 事件解析为内部指令结构
 *
 * @param[in]  evt 原始 SysEx MIDI 事件
 * @param[out] cmd 解析后的内部指令
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 参数错误或格式不符
 */
esp_err_t engine_midi_parse_sysex_cmd(const engine_midi_event_t *evt, engine_midi_sysex_cmd_t *cmd);

#ifdef __cplusplus
}
#endif

#endif /* ENGINE_MIDI_H */
