/**
 * @file engine_midi_rec.h
 * @brief Hammy MIDI Recording（.hmr）格式解析器
 *
 * 自定义轻量录音格式，记录系统内部 MIDI 总线事件流，支持回放。
 */

#ifndef ENGINE_MIDI_REC_H
#define ENGINE_MIDI_REC_H

#include "engine_midi_config.h"
#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENGINE_MIDI_REC_MAGIC        "HMRE"
#define ENGINE_MIDI_REC_END_MAGIC    "ENDR"
#define ENGINE_MIDI_REC_VERSION      2
#define ENGINE_MIDI_REC_HEADER_SIZE  128

#define ENGINE_MIDI_REC_CH_INIT_BANK_MSB 0
#define ENGINE_MIDI_REC_CH_INIT_BANK_LSB 1
#define ENGINE_MIDI_REC_CH_INIT_PROGRAM  2
#define ENGINE_MIDI_REC_CH_INIT_RESERVED 3

#define ENGINE_MIDI_REC_CH_INIT_NONE     0xFF

/**
 * @brief HMR 录音事件
 */
typedef struct {
    uint64_t time_us;                               /**< 相对录音开始的绝对时间（us） */
    uint8_t  type;                                  /**< MIDI 消息类型 */
    uint8_t  channel;                               /**< 通道号 0-15 */
    uint8_t  data1;                                 /**< 数据字节 1 */
    uint8_t  data2;                                 /**< 数据字节 2 */
    uint16_t value;                                 /**< 扩展值 */
    uint8_t  sysex_len;                             /**< SysEx 有效长度 */
    uint8_t  sysex_data[ENGINE_MIDI_SYSEX_BUF_SIZE]; /**< SysEx 数据 */
} engine_midi_rec_event_t;

/**
 * @brief HMR 解析结果
 */
typedef struct {
    engine_midi_rec_event_t *events;   /**< PSRAM 分配，由 engine_midi_rec_free() 释放 */
    uint32_t event_count;
    uint64_t total_us;                 /**< 总时长 */
    uint32_t channels_used;            /**< 通道使用位图 */
    char     source_tag[16];           /**< 录制来源标签 */
    char     song_name[64];            /**< 显示名称：filename/source_tag */
    uint8_t  channel_init[16][4];      /**< 初始通道状态：0=bank_msb, 1=bank_lsb, 2=program, 3=reserved；0xFF 表示未设置 */
} engine_midi_rec_t;

/**
 * @brief 解析 HMR 文件到 out（事件缓冲分配在 PSRAM）
 *
 * @param path 文件路径
 * @param out  解析结果输出；成功时 events 已分配，失败时 out 整体清零
 * @return ESP_OK 成功；ESP_ERR_NO_MEM 内存不足；ESP_FAIL 格式/校验/CRC 错误
 */
esp_err_t engine_midi_rec_parse_file(const char *path, engine_midi_rec_t *out);

/**
 * @brief 释放解析结果的事件缓冲并清零结构体
 *
 * @param rec 解析结果；NULL 或 events 为 NULL 时安全
 */
void engine_midi_rec_free(engine_midi_rec_t *rec);

/**
 * @brief 初始化 CRC32 状态
 *
 * 与 engine_midi_rec_crc32_update / engine_midi_rec_crc32_final 配合，
 * 支持流式计算录音文件事件数据 CRC。
 */
#define ENGINE_MIDI_REC_CRC32_INIT 0xFFFFFFFFU

/**
 * @brief 更新 CRC32
 */
uint32_t engine_midi_rec_crc32_update(uint32_t crc, const uint8_t *data, size_t len);

/**
 * @brief 结束 CRC32 计算
 */
static inline uint32_t engine_midi_rec_crc32_final(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFU;
}

/**
 * @brief 标准 CRC32（Ethernet 多项式）
 */
static inline uint32_t engine_midi_rec_crc32(const uint8_t *data, size_t len)
{
    return engine_midi_rec_crc32_final(engine_midi_rec_crc32_update(ENGINE_MIDI_REC_CRC32_INIT, data, len));
}

#ifdef __cplusplus
}
#endif

#endif /* ENGINE_MIDI_REC_H */
