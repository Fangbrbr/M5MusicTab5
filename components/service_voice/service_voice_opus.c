/**
 * @file service_voice_opus.c
 * @brief 服务语音 Opus 编解码与重采样实现
 */

#include "service_voice_opus.h"

#include "service_audio.h"
#include "service_voice.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_opus_enc.h"

#include "string.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "service_voice_opus";

/** @brief 重采样输出采样率（与 service_audio aux 入口一致） */
#define SERVICE_VOICE_OPUS_OUT_SAMPLE_RATE 44100

/** @brief 44.1kHz 立体声输出缓冲上限：按 120ms 预留（常规包 60ms≈2646
 *  帧，加倍冗余防个别超长包在重采样器内被截断） */
#define SERVICE_VOICE_OPUS_RS_OUT_MAX_SAMPLES (SERVICE_VOICE_OPUS_OUT_SAMPLE_RATE * 120 / 1000 + 16)

/* --------------------------------------------------------------------------
 * 下行解码直调裸 libopus：预编译库导出标准 libopus 符号（nm 验证在档），
 * 头文件未随组件发布，按 opus-1.x 公共 API 原型手工声明。
 * Why 不用 esp wrapper：调试期直调裸 API 以排除 wrapper 变量，后证实
 * "只出首子帧"假象实为 BinaryProtocol3 封包头污染 TOC 所致（已在
 * service_xiaozhi 下行回调剥离）；libopus 整包 decode 内部完整处理
 * Code 0~3 多子帧捆绑/VBR/填充，无需自行拆帧。
 * Trap: 曾引入 opus_packet_parse 逐帧拆包循环，真机解码产出异常且随
 * 栈溢出崩溃，已回退为整包单解（与上游 xiaozhi 一致）。
 * ------------------------------------------------------------------------ */
typedef struct OpusDecoder OpusDecoder;
extern OpusDecoder *opus_decoder_create(int32_t Fs, int channels, int *error);
extern int opus_decode(OpusDecoder *st, const unsigned char *data, int32_t len,
                       int16_t *pcm, int frame_size, int decode_fec);
extern void opus_decoder_destroy(OpusDecoder *st);
/* 包结构只读探针（诊断日志用）：读 TOC 即得子帧数/总采样数，不解码 */
extern int opus_packet_get_nb_frames(const unsigned char data[], int32_t len);
extern int opus_packet_get_nb_samples(const unsigned char data[], int32_t len, int32_t Fs);

static void *s_enc = NULL;
static OpusDecoder *s_dec = NULL;
static uint32_t s_dec_sample_rate = 0;
static uint32_t s_dec_dbg_cnt = 0;    /* 解码会话帧计数：链路诊断用，open 时清零 */

/* 解码器跨任务互斥：open/close 在 task_ai 上下文（xiaozhi 开关通道），
 * decode 在 task_comm 上下文（ws 事件分发）；task_ai 优先级更高可抢占
 * task_comm，无锁时 close 释放 s_dec 会与进行中的 decode 形成释放后使用。
 * 编码器无需此锁（open/close/encode 同在 task_audio 单上下文）。
 *  mutex 在首次 decoder_open 时创建；decode 只可能发生在 open 成功之后，
 * 故 decode/close 路径下 mutex 必已存在。 */
static SemaphoreHandle_t s_dec_mutex = NULL;

/* 解码/重采样缓冲：PSRAM 懒分配（decoder_open 时，常驻不释放），仅
 * s_dec_mutex 保护段内使用（decode 单上下文）。原静态 .bss 常驻内部 RAM
 * 约 16.5KB。 */
static int16_t *s_dec_pcm = NULL;
static int16_t *s_rs_pcm = NULL;

/* 重采样跨帧状态：保留上一帧末采样与小数相位，避免帧间爆音 */
static int16_t s_rs_prev = 0;
static uint32_t s_rs_frac = 0;

esp_err_t service_voice_opus_encoder_open(void)
{
    if (s_enc != NULL) {
        return ESP_OK;
    }

    /* DTX 生效条件为 VOIP 模式，但上行协议按上游 xiaozhi 使用 AUDIO 应用模式 */
    esp_opus_enc_config_t cfg = {
        .sample_rate = SERVICE_VOICE_OPUS_ENC_SAMPLE_RATE,
        .channel = ESP_AUDIO_MONO,
        .bits_per_sample = ESP_AUDIO_BIT16,
        .bitrate = ESP_OPUS_BITRATE_AUTO,
        .frame_duration = ESP_OPUS_ENC_FRAME_DURATION_60_MS,
        .application_mode = ESP_OPUS_ENC_APPLICATION_AUDIO,
        .complexity = 0,
        .enable_fec = false,
        .enable_dtx = true,
        .enable_vbr = true,
    };

    esp_audio_err_t ret = esp_opus_enc_open(&cfg, sizeof(cfg), &s_enc);
    if (ret != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "encoder open failed: %d", ret);
        s_enc = NULL;
        return ESP_FAIL;
    }

    int in_size = 0;
    int out_size = 0;
    if (esp_opus_enc_get_frame_size(s_enc, &in_size, &out_size) == ESP_AUDIO_ERR_OK) {
        ESP_LOGI(TAG, "encoder opened: in=%d out=%d", in_size, out_size);
    }
    return ESP_OK;
}

int service_voice_opus_encode(const int16_t *pcm, uint8_t *out, uint32_t out_max)
{
    if (s_enc == NULL || pcm == NULL || out == NULL) {
        return -1;
    }

    esp_audio_enc_in_frame_t in_frame = {
        .buffer = (uint8_t *)pcm,
        .len = SERVICE_VOICE_OPUS_ENC_FRAME_SAMPLES * sizeof(int16_t),
    };
    esp_audio_enc_out_frame_t out_frame = {
        .buffer = out,
        .len = out_max,
        .encoded_bytes = 0,
    };

    esp_audio_err_t ret = esp_opus_enc_process(s_enc, &in_frame, &out_frame);
    if (ret != ESP_AUDIO_ERR_OK) {
        ESP_LOGW(TAG, "encode failed: %d", ret);
        return -1;
    }
    return (int)out_frame.encoded_bytes;
}

void service_voice_opus_encoder_close(void)
{
    if (s_enc != NULL) {
        esp_opus_enc_close(s_enc);
        s_enc = NULL;
    }
}

esp_err_t service_voice_opus_decoder_open(uint32_t sample_rate)
{
    if (s_dec_mutex == NULL) {
        s_dec_mutex = xSemaphoreCreateMutex();
        if (s_dec_mutex == NULL) {
            ESP_LOGE(TAG, "decoder mutex create failed");
            return ESP_ERR_NO_MEM;
        }
    }

    xSemaphoreTake(s_dec_mutex, portMAX_DELAY);

    /* 解码/重采样缓冲 PSRAM 懒分配（常驻不释放，避免开关通道反复抖动堆） */
    if (s_dec_pcm == NULL) {
        s_dec_pcm = heap_caps_malloc(SERVICE_VOICE_DEC_PCM_MAX_SAMPLES * sizeof(int16_t),
                                     MALLOC_CAP_SPIRAM);
    }
    if (s_rs_pcm == NULL) {
        s_rs_pcm = heap_caps_malloc(SERVICE_VOICE_OPUS_RS_OUT_MAX_SAMPLES * 2 * sizeof(int16_t),
                                    MALLOC_CAP_SPIRAM);
    }
    if (s_dec_pcm == NULL || s_rs_pcm == NULL) {
        ESP_LOGE(TAG, "decoder pcm buf alloc failed");
        xSemaphoreGive(s_dec_mutex);
        return ESP_ERR_NO_MEM;
    }

    if (s_dec != NULL) {
        if (s_dec_sample_rate == sample_rate) {
            xSemaphoreGive(s_dec_mutex);
            return ESP_OK;
        }
        /* 采样率变更：已持锁，直接内联销毁（不可调 close 外壳，递归死锁） */
        opus_decoder_destroy(s_dec);
        s_dec = NULL;
        s_dec_sample_rate = 0;
    }

    if (sample_rate == 0) {
        sample_rate = SERVICE_VOICE_OPUS_DEC_DEFAULT_SAMPLE_RATE;
    }

    /* libopus 仅接受 8/12/16/24/48k 采样率，服务器 hello 下发其他值时报错 */
    int err = 0;
    s_dec = opus_decoder_create((int32_t)sample_rate, 1, &err);
    if (s_dec == NULL || err != 0) {
        ESP_LOGE(TAG, "decoder create failed: %d (rate %lu)", err, (unsigned long)sample_rate);
        s_dec = NULL;
        xSemaphoreGive(s_dec_mutex);
        return ESP_FAIL;
    }

    s_dec_sample_rate = sample_rate;
    s_rs_prev = 0;
    s_rs_frac = 0;
    s_dec_dbg_cnt = 0;
    ESP_LOGI(TAG, "decoder opened: %lu Hz", (unsigned long)sample_rate);
    xSemaphoreGive(s_dec_mutex);
    return ESP_OK;
}

void service_voice_opus_reset_phase(void)
{
    /* 句末复位：下一句首帧不再从本句尾采样插值（帧间电平跳变即 click 爆音）。
     * 同会话相邻回复间解码器不重开，此状态必须由句边界显式清理。
     * 调用点（xz_task 句末）与解码（task_comm）天然时序错开，无需锁 */
    s_rs_prev = 0;
    s_rs_frac = 0;
}

/**
 * @brief 单声道线性插值重采样并复制为立体声
 *
 * 步长 16.16 定点；s_rs_prev/s_rs_frac 跨帧保持相位连续。
 *
 * @return 输出帧数（每帧 2 个 int16）
 */
static uint32_t service_voice_opus_resample_to_stereo(const int16_t *in, uint32_t in_frames, uint32_t in_rate)
{
    if (in_frames == 0 || in_rate == 0) {
        return 0;
    }

    /* 输入采样率与输出一致时直接复制双声道，跳过插值 */
    if (in_rate == SERVICE_VOICE_OPUS_OUT_SAMPLE_RATE) {
        for (uint32_t i = 0; i < in_frames; i++) {
            s_rs_pcm[i * 2] = in[i];
            s_rs_pcm[i * 2 + 1] = in[i];
        }
        s_rs_prev = in[in_frames - 1];
        return in_frames;
    }

    const uint32_t step = (uint32_t)(((uint64_t)in_rate << 16) / SERVICE_VOICE_OPUS_OUT_SAMPLE_RATE);
    uint32_t pos = s_rs_frac;
    uint32_t out_frames = 0;

    while ((pos >> 16) < in_frames) {
        uint32_t idx = pos >> 16;
        uint32_t frac = pos & 0xFFFF;
        int32_t cur = (idx == 0) ? s_rs_prev : in[idx - 1];
        int32_t next = in[idx];
        int32_t sample = cur + (int32_t)(((int64_t)(next - cur) * frac) >> 16);
        s_rs_pcm[out_frames * 2] = (int16_t)sample;
        s_rs_pcm[out_frames * 2 + 1] = (int16_t)sample;
        out_frames++;
        pos += step;
        if (out_frames >= SERVICE_VOICE_OPUS_RS_OUT_MAX_SAMPLES) {
            break;
        }
    }

    /* 截断提前退出时 pos 未消费完整输入，直接 pos-(in_frames<<16) 会下溢
     * 成巨值——下一帧 (pos>>16)>=in_frames 立即成立，输出永久为 0
     * （真机：TTS 偶尔刺啦后永久无声）。截断路径重置相位，丢本帧剩余。 */
    uint32_t consumed = in_frames << 16;
    s_rs_frac = (pos >= consumed) ? (pos - consumed) : 0;
    s_rs_prev = in[in_frames - 1];
    return out_frames;
}

esp_err_t service_voice_opus_decode_to_aux(const uint8_t *data, uint32_t len)
{
    if (data == NULL || len == 0 || s_dec_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_dec_mutex, portMAX_DELAY);
    esp_err_t result = ESP_OK;

    if (s_dec == NULL || s_dec_pcm == NULL || s_rs_pcm == NULL) {
        result = ESP_ERR_INVALID_STATE;
        goto out;
    }

    {
        /* 整包单次解码：libopus 内部完整处理 Code 0~3 多子帧捆绑/填充，
         * 60ms@24k 包解出 1440 采样（与上游 xiaozhi 解码路径一致）。
         * frame_size 为输出缓冲上限（3072 ≈ 128ms@24k），超出 libopus 报错 */
        int decoded = opus_decode(s_dec, data, (int32_t)len, s_dec_pcm,
                                  SERVICE_VOICE_DEC_PCM_MAX_SAMPLES, 0);
        if (decoded < 0) {
            /* 临时诊断：解码失败前 3 次升 WARN（常态化后应为 0 次） */
            if (s_dec_dbg_cnt < 3) {
                ESP_LOGW(TAG, "[dbg] tts decode FAIL: err=%d pkt=%lu",
                         decoded, (unsigned long)len);
            }
            result = ESP_FAIL;
            goto out;
        }

        uint32_t out_frames = 0;
        if (decoded > 0) {
            /* 重采样率固定用打开配置率：opus 解码输出采样率结构上等于该率 */
            out_frames = service_voice_opus_resample_to_stereo(s_dec_pcm, (uint32_t)decoded,
                                                               s_dec_sample_rate);
            if (out_frames > 0) {
                service_audio_aux_write(s_rs_pcm, out_frames);
            }
        }
        /* 临时诊断：每会话前 3 包打印包结构与解码采样，实锤整包解码
         * （预期 60ms 包 decoded≈1440@24k out≈2646；v3 封包病灶期仅 240） */
        if (s_dec_dbg_cnt < 3) {
            ESP_LOGD(TAG, "[dbg] tts chain #%lu: pkt=%lu toc=%02x nfr=%d pktsmp=%d decoded=%d out=%lu",
                     (unsigned long)s_dec_dbg_cnt, (unsigned long)len,
                     (unsigned)data[0],
                     opus_packet_get_nb_frames(data, (int32_t)len),
                     opus_packet_get_nb_samples(data, (int32_t)len, (int32_t)s_dec_sample_rate),
                     decoded, (unsigned long)out_frames);
        }
        s_dec_dbg_cnt++;
    }

out:
    xSemaphoreGive(s_dec_mutex);
    return result;
}

void service_voice_opus_decoder_close(void)
{
    if (s_dec_mutex == NULL) {
        return;
    }
    xSemaphoreTake(s_dec_mutex, portMAX_DELAY);
    if (s_dec != NULL) {
        opus_decoder_destroy(s_dec);
        s_dec = NULL;
        s_dec_sample_rate = 0;
    }
    xSemaphoreGive(s_dec_mutex);
}
