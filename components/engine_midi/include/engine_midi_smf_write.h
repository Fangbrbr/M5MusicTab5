/**
 * @file engine_midi_smf_write.h
 * @brief 标准 MIDI 文件（SMF）format 0 序列化器
 *
 * 与 engine_midi_smf（解析）互补：把带毫秒时间戳的通道消息流写成单轨 .mid。
 * 纯协议层：只操作调用方传入的 FILE *，不感知 SD 卡/服务，可被宿主机直接单测。
 *
 * 固定参数：format 0、单轨、PPQ=480、tempo=500000us/qn（120 BPM）。
 * 演奏为自由节拍，所有事件按墙钟毫秒换算绝对 tick（tick = ms * 24 / 25，
 * 1 tick ≈ 1.04ms），不建 tempo map。
 */

#ifndef ENGINE_MIDI_SMF_WRITE_H
#define ENGINE_MIDI_SMF_WRITE_H

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENGINE_MIDI_SMF_WRITE_PPQ        480       /**< 每四分音符 tick 数 */
#define ENGINE_MIDI_SMF_WRITE_TEMPO_USQN 500000    /**< 120 BPM，与 PPQ 共同决定 ms->tick */

/**
 * @brief 序列化器状态（调用方持有存储，无需动态分配）
 */
typedef struct {
    FILE *fp;               /**< 目标文件（调用方负责打开/关闭） */
    uint32_t last_tick;     /**< 上一事件绝对 tick，用于 delta 计算与单调兜底 */
    uint32_t track_bytes;   /**< MTrk 负载已写字节数，收尾时回填头部长度 */
    bool failed;            /**< 任一写盘失败置位，后续调用短路报错 */
} engine_midi_smf_writer_t;

/**
 * @brief 写入 MThd/MTrk 头与起始 meta（tempo、拍号、轨道名）
 *
 * @param w          序列化器（内容被整体初始化）
 * @param fp         已以 "wb" 打开的文件
 * @param track_name 轨道名 meta（ASCII，可 NULL 表示不写；最长截断 31 字节）
 */
esp_err_t engine_midi_smf_writer_begin(engine_midi_smf_writer_t *w, FILE *fp,
                                       const char *track_name);

/**
 * @brief 追加一个通道消息事件
 *
 * 仅接受通道消息（0x80~0xE0）；其余类型静默忽略并返回 ESP_OK。
 * 弯音（0xE0）取 value14（0~16383），忽略 d1/d2；0xC0/0xD0 只写 d1。
 *
 * @param abs_ms 距录制起点的绝对毫秒；内部换算 tick 并保证 delta 单调不减
 */
esp_err_t engine_midi_smf_writer_event(engine_midi_smf_writer_t *w, uint32_t abs_ms,
                                       uint8_t type, uint8_t ch, uint8_t d1, uint8_t d2,
                                       uint16_t value14);

/**
 * @brief 写入 End-of-Track 并回填 MTrk 长度
 *
 * 成功后文件可立即由调用方 fclose。失败语义同 fwrite/fseek 错误。
 */
esp_err_t engine_midi_smf_writer_end(engine_midi_smf_writer_t *w);

#ifdef __cplusplus
}
#endif

#endif /* ENGINE_MIDI_SMF_WRITE_H */
