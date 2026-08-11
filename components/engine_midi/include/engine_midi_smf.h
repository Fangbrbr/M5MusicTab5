/**
 * @file engine_midi_smf.h
 * @brief 标准 MIDI 文件（SMF）解析器
 *
 * 最小 SMF 实现：MThd/MTrk、VLQ、running status、tempo map、轨道名 meta。
 * 仅支持 format 0/1、最多 64 轨；触后（0xA0/0xD0）与 SysEx 事件不入队。
 * 事件缓冲一次性分配在 PSRAM，解析完成后零文件 I/O。
 */

#ifndef ENGINE_MIDI_SMF_H
#define ENGINE_MIDI_SMF_H

#include "engine_midi_config.h"
#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SMF 事件（tempo map 换算后的绝对时间）
 */
typedef struct {
    uint32_t time_us;   /**< 绝对时间（tempo map 换算后） */
    uint32_t tick;      /**< 绝对 tick */
    uint8_t  type;      /**< 0x8n/0x9n/0xBn/0xCn/0xEn */
    uint8_t  channel;
    uint8_t  data1;
    uint8_t  data2;
} engine_midi_smf_event_t;

/**
 * @brief SMF 解析结果
 */
typedef struct {
    engine_midi_smf_event_t *events;   /**< PSRAM 分配，由 engine_midi_smf_free() 释放 */
    uint32_t event_count;
    uint32_t total_us;                 /**< 总时长（最后一事件绝对时间） */
    uint16_t bpm;                      /**< 依首个 tempo 换算 */
    uint8_t  channels_used;            /**< 实际使用的通道数 */
    char song_name[ENGINE_MIDI_SMF_SONG_NAME_MAX];  /**< 首个轨道名 meta，无则空串 */
} engine_midi_smf_t;

/**
 * @brief 解析 SMF 文件到 out（事件缓冲分配在 PSRAM）
 *
 * @param path  文件路径
 * @param out   解析结果输出；成功时 events 已分配，失败时 out 整体清零
 * @return ESP_OK 解析成功；ESP_ERR_NO_MEM 事件缓冲分配失败；ESP_FAIL 打开/格式错误
 * @note 重复调用前调用方须先 engine_midi_smf_free()，否则旧缓冲泄漏
 */
esp_err_t engine_midi_smf_parse_file(const char *path, engine_midi_smf_t *out);

/**
 * @brief 释放解析结果的事件缓冲并清零结构体
 *
 * @param smf  解析结果；NULL 或 events 为 NULL 时安全
 */
void engine_midi_smf_free(engine_midi_smf_t *smf);

#ifdef __cplusplus
}
#endif

#endif /* ENGINE_MIDI_SMF_H */
