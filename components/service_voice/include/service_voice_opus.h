/**
 * @file service_voice_opus.h
 * @brief 服务语音通道 Opus 编解码与重采样
 *
 * 编码固定 16kHz 单声道 60ms 帧（上行）；解码采样率随服务器 hello
 * 动态确定，解码后线性插值重采样到 44.1kHz 立体声写入 service_audio
 * 辅助混音流。
 */

#ifndef SERVICE_VOICE_OPUS_H
#define SERVICE_VOICE_OPUS_H

#include "esp_err.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 打开上行 Opus 编码器（16kHz 单声道 60ms 帧）
 * @return ESP_OK 成功
 */
esp_err_t service_voice_opus_encoder_open(void);

/**
 * @brief 编码一帧 60ms PCM（960 采样，16kHz 单声道 int16）
 *
 * @param[in]  pcm     PCM 采样（960 个 int16）
 * @param[out] out     Opus 输出缓冲
 * @param[in]  out_max 输出缓冲大小（建议 >= SERVICE_VOICE_OPUS_ENC_OUT_SIZE）
 * @return 编码字节数，<0 表示编码失败
 */
int service_voice_opus_encode(const int16_t *pcm, uint8_t *out, uint32_t out_max);

/**
 * @brief 关闭上行 Opus 编码器
 */
void service_voice_opus_encoder_close(void);

/**
 * @brief 打开下行 Opus 解码器
 *
 * @param[in] sample_rate 服务器 hello 下发的采样率（如 24000）
 * @return ESP_OK 成功
 */
esp_err_t service_voice_opus_decoder_open(uint32_t sample_rate);

/**
 * @brief 复位重采样跨帧相位（句末调用）
 *
 * 同会话相邻回复间解码器不重开，末采样残留会让下一句首帧插值出 click。
 */
void service_voice_opus_reset_phase(void);

/**
 * @brief 解码一个 Opus 裸帧并重采样写入辅助混音流
 *
 * 解码（sample_rate 单声道）→ 线性插值重采样到 44.1kHz → 复制双声道 →
 * service_audio_aux_write()。aux 环形缓冲满时截断丢弃，不阻塞调用方。
 * Contract: 仅允许单一调用方（WebSocket 事件回调上下文）。
 *
 * @param[in] data Opus 裸帧
 * @param[in] len  帧字节数
 * @return ESP_OK 成功，ESP_FAIL 解码失败
 */
esp_err_t service_voice_opus_decode_to_aux(const uint8_t *data, uint32_t len);

/**
 * @brief 关闭下行 Opus 解码器
 */
void service_voice_opus_decoder_close(void);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_VOICE_OPUS_H */
