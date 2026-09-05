/**
 * @file service_voice.h
 * @brief Voice frontend service: mic/AEC/AFE/WakeNet/Opus codec
 *
 * Drove by task_audio in highest priority audio loop, feeding 44.1kHz full-duplex mic/ref
 * to AFE, generating wake/VAD/uplink Opus packets; service_xiaozhi only keeps protocol
 * state machine and consumes events from queue.
 */

#ifndef SERVICE_VOICE_H
#define SERVICE_VOICE_H

#include "esp_err.h"
#include "stdbool.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Configuration constants
 */
#define SERVICE_VOICE_OPUS_ENC_SAMPLE_RATE        16000     /* Encoder sample rate (Hz) */
#define SERVICE_VOICE_OPUS_ENC_FRAME_SAMPLES      960       /* Encoder frame size (samples, 16k/60ms) */
#define SERVICE_VOICE_OPUS_ENC_OUT_SIZE           1024      /* Encoder output buffer size */
#define SERVICE_VOICE_OPUS_DEC_DEFAULT_SAMPLE_RATE 24000    /* Decoder default sample rate (Hz) */
#define SERVICE_VOICE_DEC_PCM_MAX_SAMPLES         3072      /* Max decoded PCM samples */
#define SERVICE_VOICE_AFE_MAX_CHUNK               512       /* AFE 单块采样上限（16k/32ms）：
                                                             * AEC SR_LOW_COST 档 feed/fetch 块为 512，
                                                             * 旧值 256 会把该档 AFE 整个拒开（真机：
                                                             * 唤醒前端全失效退化为按住说话） */
#define SERVICE_VOICE_LISTEN_WARMUP_MS            120       /* Listen warmup duration (ms) */
#define SERVICE_VOICE_OPUS_ENC_FRAME_DURATION_MS  60        /* Encoder frame duration (ms) */

typedef enum {
    SERVICE_VOICE_EVT_WAKE = 0,    /* Wake word detected; text field is wake word */
    SERVICE_VOICE_EVT_VAD,          /* VAD edge; is_speech field */
    SERVICE_VOICE_EVT_PACKET,       /* Uplink Opus packet ready; data/len valid during callback */
    SERVICE_VOICE_EVT_ERROR         /* Error; text field is description */
} service_voice_event_type_t;

typedef struct {
    service_voice_event_type_t type;
    const uint8_t *data;         /* PACKET: Opus packet pointer (PSRAM allocated, caller must heap_caps_free) */
    uint32_t len;                /* PACKET: bytes */
    const char *text;            /* WAKE/ERROR: string (static or constant, no release needed) */
    bool is_speech;              /* VAD: true=speech, false=silence */
} service_voice_event_t;

/**
 * @brief Initialize voice frontend (create queues, allocate PSRAM buffers)
 *
 * AFE is opened separately by service_voice_wake_open_deferred(); main.c calls it
 * before WiFi init to ensure the wake word frontend is always available. App code
 * may call it again (idempotent) when starting a xiaozhi session.
 * Re-entry is idempotent.
 *
 * @return ESP_OK basic buffers ready; ESP_FAIL on alloc failure.
 */
esp_err_t service_voice_init(void);

/**
 * @brief Open AFE wake frontend on demand (call before WiFi init)
 *
 * Why: AFE needs ~110KB internal RAM. Boot has ~126KB, but after WiFi/TLS/WS
 * only ~32KB remains. Must open AFE before WiFi initialization.
 * @return ESP_OK AFE ready (auto mode); ESP_FAIL fallback to manual
 */
esp_err_t service_voice_wake_open_deferred(void);

/**
 * @brief Close AFE wake frontend (when xiaozhi App exits)
 */
void service_voice_wake_close_deferred(void);

/**
 * @brief Voice frontend period processing (called by task_audio every cycle)
 *
 * Contract: Single call only processes bounded slices, does not block I2S rendering; internally
 * processes 44.1k mic/ref -> 16k MR -> AFE feed/fetch -> encoding/events pipeline.
 */
void service_voice_process(void);

/**
 * @brief Start/stop listening (encoding uplink)
 *
 * Commands delivered via queue; service_voice_process actually switches in audio task.
 */
esp_err_t service_voice_start_listen(void);
void service_voice_stop_listen(void);

/**
 * @brief Enable/disable wake word detection
 */
void service_voice_enable_wake(bool enable);

/**
 * @brief Current listening state
 */
bool service_voice_is_listening(void);

/**
 * @brief Non-blocking event poll (called by service_xiaozhi in task_ai)
 * @return true if event retrieved
 */
bool service_voice_poll_event(service_voice_event_t *out);

/**
 * @brief Open downlink Opus decoder (called by service_xiaozhi after receiving hello)
 */
esp_err_t service_voice_decoder_open(uint32_t sample_rate);

/**
 * @brief Close downlink decoder
 */
void service_voice_decoder_close(void);

/**
 * @brief 复位下行重采样跨帧相位（句末调用，防下一句首帧从尾采样插值出 click）
 */
void service_voice_decoder_reset_phase(void);

/**
 * @brief Decode one downlink Opus packet and write to service_audio aux mix stream
 *
 * Contract: Can be called from any task context (internally uses service_audio_aux_write).
 */
esp_err_t service_voice_decode_packet(const uint8_t *data, uint32_t len);

/**
 * @brief Get uplink Opus frame samples (16k/60ms = 960)
 */
uint32_t service_voice_get_frame_samples(void);

/**
 * @brief Get current wake word text (when AFE available)
 * @return wake word string; fallback to NULL when not available
 */
const char *service_voice_get_wake_word(void);

/**
 * @brief mic 录音 tap 回调（16kHz mono int16）
 *
 * Contract: 回调运行在 task_audio（Core 1 最高优先级）上下文，必须零阻塞、
 * 零分配，只允许 memcpy 级操作（如写 SPSC 环形缓冲）。
 */
typedef void (*service_voice_mic_tap_cb_t)(const int16_t *pcm, uint32_t samples, void *ctx);

/**
 * @brief 注册/注销 mic 录音 tap
 *
 * @param cb             回调；NULL = 注销
 * @param ctx            回调上下文
 * @param aec_processed  false=tap 原始 mic（重采样后、AFE 前，含扬声器串音）；
 *                       true=tap AFE 输出（AEC 后，抑制扬声器串音）
 * Why: AFE 常驻持有 mic 且配置锁死 44100/1ch，录音机语音/环境模式只能
 * 从 AFE 管线旁路取 16k mono；两种 tap 点共用同一注册，同一时刻仅一路。
 */
void service_voice_set_mic_tap(service_voice_mic_tap_cb_t cb, void *ctx, bool aec_processed);

/**
 * @brief AFE 是否已激活（tap 数据仅在激活后持续产出）
 */
bool service_voice_is_afe_active(void);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_VOICE_H */
