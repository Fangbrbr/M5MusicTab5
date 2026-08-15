/**
 * @file service_player.cpp
 * @brief 通用音频播放服务实现：micro-mp3 解码 + 重采样 44.1kHz 立体声 + aux 输出
 *
 * 本文件为 C++ 实现，对外暴露纯 C API（service_player.h）。
 * 调用方（App 层）应通过 service_player_poll() 驱动解码推进。
 */

#include "service_player.h"
#include "micro_mp3/mp3_decoder.h"
#include "service_audio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <new>

static const char *TAG = "service_player";

/* -------------------- 常量 -------------------- */

/** ID3v2 头部固定大小 */
#define ID3V2_HEADER_SIZE 10

/** 文件读取块大小 */
#define READ_CHUNK 2048

/** 每次 poll 最多解码帧数（防长时间阻塞） */
#define MAX_DECODE_FRAMES_PER_POLL 30

/** aux 通道剩余帧数门限，低于此值时暂停解码 */
#define AUX_FREE_THRESHOLD 512

/** MPEG1 最大帧样本数（1152 帧/声道） */
#define MAX_PCM_FRAMES 1152
#define MAX_PCM_SAMPLES (MAX_PCM_FRAMES * 2)

/** 重采样输出缓冲帧数（最大输入帧 1152 × 44100/8000 ≈ 6352，取 8192） */
#define RESAMPLE_BUF_FRAMES 8192

/* -------------------- 内部状态 -------------------- */

typedef struct {
    /* 文件 */
    FILE *fp;
    size_t file_size;       /* 文件总大小（字节） */
    size_t mp3_data_offset; /* MP3 音频数据起始偏移（跳过 ID3v2 标签后） */

    /* 读取缓冲（SPSC 单生产者） */
    uint8_t read_buf[READ_CHUNK];
    const uint8_t *read_ptr;    /* 当前未解码数据指针 */
    size_t read_remaining;      /* 未解码数据字节数 */
    bool eof;                   /* 文件已读完 */

    /* ID3v2 标题 */
    char title[256];
    bool title_parsed;

    /* 解码器 */
    micro_mp3::Mp3Decoder *decoder;
    int16_t *pcm_buf;           /* 解码 PCM 缓冲（堆分配，PSRAM） */
    int16_t *resample_buf;      /* 重采样输出缓冲（堆分配，PSRAM） */

    /* 流信息（首帧解码后确定） */
    uint32_t sample_rate;
    uint8_t channels;
    uint32_t bitrate_kbps;

    /* 重采样累积相位（输入样本域分数位置，跨帧保持，避免帧边界跳变） */
    double resample_phase;

    /* 待写输出帧状态（分片写入，防 aux 满时整帧截断丢帧导致播放加速） */
    const int16_t *out_ptr;    /* 当前解码帧输出缓冲（pcm_buf 或 resample_buf） */
    uint32_t out_total;        /* 当前解码帧总输出帧数 */
    uint32_t out_done;         /* 已成功写入 aux 的帧数 */

    /* 播放状态 */
    bool loaded;
    bool playing;
    bool paused;

    /* 位置追踪（以 44.1kHz 输出帧数累加） */
    int64_t total_output_frames; /* 输出到 aux 的立体声帧数 */
    bool eos_sent;               /* 是否已发送 end_of_stream */
} player_state_t;

static player_state_t s_state = {0};

/* -------------------- 辅助函数 -------------------- */

/* ID3v2 同步安全整数（7 位/字节） */
static uint32_t id3_syncsafe_to_uint(const uint8_t buf[4])
{
    return ((uint32_t)buf[0] << 21) | ((uint32_t)buf[1] << 14) |
           ((uint32_t)buf[2] << 7)  | (uint32_t)buf[3];
}

/* UTF-16 码元追加为 UTF-8 序列（支持 BMP 与代理对，size 上限内） */
static void utf16_append_utf8(char *out, size_t out_cap, size_t *out_len, uint32_t cp)
{
    if (cp > 0x10FFFF) {
        cp = 0xFFFD;
    }
    size_t n = 0;
    char seq[4];
    if (cp < 0x80) {
        seq[n++] = (char)cp;
    } else if (cp < 0x800) {
        seq[n++] = (char)(0xC0 | (cp >> 6));
        seq[n++] = (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        seq[n++] = (char)(0xE0 | (cp >> 12));
        seq[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        seq[n++] = (char)(0x80 | (cp & 0x3F));
    } else {
        seq[n++] = (char)(0xF0 | (cp >> 18));
        seq[n++] = (char)(0x80 | ((cp >> 12) & 0x3F));
        seq[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        seq[n++] = (char)(0x80 | (cp & 0x3F));
    }
    if (*out_len + n < out_cap) {
        memcpy(out + *out_len, seq, n);
        *out_len += n;
    }
}

/* 读 UTF-16 码元（LE/BE），返回码元值并推进 pos */
static bool utf16_read_code_unit(const uint8_t *buf, size_t buf_len, size_t *pos,
                                 bool big_endian, uint32_t *out)
{
    if (*pos + 2 > buf_len) {
        return false;
    }
    if (big_endian) {
        *out = ((uint32_t)buf[*pos] << 8) | buf[*pos + 1];
    } else {
        *out = ((uint32_t)buf[*pos + 1] << 8) | buf[*pos];
    }
    *pos += 2;
    return true;
}

/* UTF-16 → UTF-8：跳过 BOM 判字节序，支持代理对，遇 0x00 视为串尾 */
static void utf16_to_utf8(const uint8_t *text, size_t text_len,
                          char *out, size_t out_cap)
{
    size_t pos = 0;
    bool big_endian = false;
    if (text_len >= 2) {
        if (text[0] == 0xFF && text[1] == 0xFE) {
            big_endian = false;
            pos = 2;
        } else if (text[0] == 0xFE && text[1] == 0xFF) {
            big_endian = true;
            pos = 2;
        }
    }

    size_t out_len = 0;
    while (pos + 2 <= text_len) {
        uint32_t unit;
        if (!utf16_read_code_unit(text, text_len, &pos, big_endian, &unit)) {
            break;
        }
        if (unit == 0x0000) {
            break;   /* 串尾 */
        }
        if (unit >= 0xD800 && unit <= 0xDBFF) {
            /* 高代理：需跟一个低代理 */
            uint32_t low;
            if (!utf16_read_code_unit(text, text_len, &pos, big_endian, &low)) {
                utf16_append_utf8(out, out_cap, &out_len, 0xFFFD);
                break;
            }
            if (low >= 0xDC00 && low <= 0xDFFF) {
                uint32_t cp = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
                utf16_append_utf8(out, out_cap, &out_len, cp);
            } else {
                utf16_append_utf8(out, out_cap, &out_len, 0xFFFD);
                /* 低代理被消费，回退一步重读 */
                pos -= 2;
            }
        } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
            utf16_append_utf8(out, out_cap, &out_len, 0xFFFD);
        } else {
            utf16_append_utf8(out, out_cap, &out_len, unit);
        }
    }
    out[out_len] = '\0';
}

/**
 * @brief 解析 ID3v2 标签中的 TIT2（标题）帧
 *
 * 支持 ID3v2.2（TT2）、v2.3+（TIT2）；
 * 编码支持 ISO-8859-1（0x00）、UTF-16（0x01/0x02）、UTF-8（0x03）。
 * UTF-16 完整转换（BOM 判序 + 代理对），中文歌名不再只取低字节而乱码。
 */
static void parse_id3v2_title(const uint8_t *data, size_t data_len,
                              char *title, size_t title_max)
{
    if (data_len < ID3V2_HEADER_SIZE ||
        data[0] != 'I' || data[1] != 'D' || data[2] != '3') {
        return;
    }

    /* ID3v2.2 用 3 字节帧 ID，v2.3+ 用 4 字节；主版本号在头第 4 字节 */
    bool is_v22 = (data[3] == 2);

    uint32_t tag_size = id3_syncsafe_to_uint(data + 6);
    size_t total_size = ID3V2_HEADER_SIZE + tag_size;
    if (total_size > data_len) {
        total_size = data_len;
    }

    const uint8_t *tag_data = data + ID3V2_HEADER_SIZE;
    size_t tag_data_len = total_size - ID3V2_HEADER_SIZE;

    size_t pos = 0;
    while (pos + (is_v22 ? 6 : 10) <= tag_data_len) {
        uint32_t frame_size;
        char frame_id[5] = {0};

        const uint8_t *hdr = tag_data + pos;

        if (is_v22) {
            /* V2.2：3 字节帧 ID + 3 字节大小 */
            memcpy(frame_id, hdr, 3);
            frame_size = ((uint32_t)hdr[3] << 16) | ((uint32_t)hdr[4] << 8) | (uint32_t)hdr[5];
        } else {
            /* V2.3+：4 字节帧 ID + 4 字节大小（大端） */
            memcpy(frame_id, hdr, 4);
            frame_size = ((uint32_t)hdr[4] << 24) | ((uint32_t)hdr[5] << 16) |
                         ((uint32_t)hdr[6] << 8)  | (uint32_t)hdr[7];
        }

        if (frame_size == 0) {
            pos += is_v22 ? 6 : 10;
            continue;
        }
        if (pos + (is_v22 ? 6 : 10) + frame_size > tag_data_len) {
            break;
        }

        const uint8_t *frame_data = tag_data + pos + (is_v22 ? 6 : 10);

        /* 匹配标题帧 */
        bool is_title = (!is_v22 && strcmp(frame_id, "TIT2") == 0) ||
                        (is_v22 && strcmp(frame_id, "TT2") == 0);

        if (is_title && frame_size >= 2) {
            uint8_t encoding = frame_data[0];
            const uint8_t *text = frame_data + 1;
            size_t text_len = frame_size - 1;

            if (encoding == 0x00) {
                /* ISO-8859-1（Latin-1）：直接拷贝 */
                size_t copy_len = std::min(text_len, title_max - 1);
                memcpy(title, text, copy_len);
                title[copy_len] = '\0';
                /* 去掉末尾 null */
                while (copy_len > 0 && title[copy_len - 1] == '\0') {
                    title[--copy_len] = '\0';
                }
                return;
            } else if (encoding == 0x03) {
                /* UTF-8：直接拷贝 */
                size_t copy_len = std::min(text_len, title_max - 1);
                memcpy(title, text, copy_len);
                title[copy_len] = '\0';
                return;
            } else if (encoding == 0x01 || encoding == 0x02) {
                /* UTF-16：完整转换，BOM 判序，支持中文 */
                utf16_to_utf8(text, text_len, title, title_max);
                return;
            }
        }

        pos += (is_v22 ? 6 : 10) + frame_size;
    }
}

/**
 * @brief 线性插值重采样：从任意采样率到 44.1kHz 立体声
 *
 * @param src         输入 PCM（int16，交错，src_channels 声道）
 * @param src_frames  输入帧数（每声道）
 * @param src_channels 输入声道数（1=mono，2=stereo）
 * @param dst         输出缓冲（int16，立体声交错）
 * @param dst_capacity 输出帧容量
 * @param[inout] phase 分数相位（输入帧空间中的位置，跨次调用累积）
 * @param ratio        44100.0 / src_rate
 * @return 实际输出立体声帧数
 */
static uint32_t resample_linear(const int16_t *src, uint32_t src_frames,
                                uint8_t src_channels, int16_t *dst,
                                uint32_t dst_capacity, double *phase, double ratio)
{
    uint32_t out_frames = 0;

    if (src_channels == 2) {
        while (*phase < (double)src_frames && out_frames < dst_capacity) {
            uint32_t idx = (uint32_t)*phase;
            double frac = *phase - (double)idx;
            if (idx + 1 < src_frames) {
                dst[out_frames * 2]     = (int16_t)((double)src[idx * 2]     * (1.0 - frac) +
                                                     (double)src[(idx + 1) * 2]     * frac);
                dst[out_frames * 2 + 1] = (int16_t)((double)src[idx * 2 + 1] * (1.0 - frac) +
                                                     (double)src[(idx + 1) * 2 + 1] * frac);
            } else {
                dst[out_frames * 2]     = src[idx * 2];
                dst[out_frames * 2 + 1] = src[idx * 2 + 1];
            }
            out_frames++;
            *phase += ratio;
        }
    } else {
        /* 单声道 → 立体声（复制到双声道） */
        while (*phase < (double)src_frames && out_frames < dst_capacity) {
            uint32_t idx = (uint32_t)*phase;
            double frac = *phase - (double)idx;
            int16_t sample;
            if (idx + 1 < src_frames) {
                sample = (int16_t)((double)src[idx] * (1.0 - frac) +
                                    (double)src[idx + 1] * frac);
            } else {
                sample = src[idx];
            }
            dst[out_frames * 2]     = sample;
            dst[out_frames * 2 + 1] = sample;
            out_frames++;
            *phase += ratio;
        }
    }

    *phase -= (double)src_frames;
    return out_frames;
}

/* -------------------- 公共 API -------------------- */

esp_err_t service_player_init(void)
{
    if (s_state.pcm_buf != NULL) {
        return ESP_OK;
    }

    s_state.pcm_buf = (int16_t *)heap_caps_malloc(
        MAX_PCM_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (s_state.pcm_buf == NULL) {
        ESP_LOGE(TAG, "pcm buf alloc failed");
        return ESP_ERR_NO_MEM;
    }

    s_state.resample_buf = (int16_t *)heap_caps_malloc(
        RESAMPLE_BUF_FRAMES * 2 * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (s_state.resample_buf == NULL) {
        heap_caps_free(s_state.pcm_buf);
        s_state.pcm_buf = NULL;
        ESP_LOGE(TAG, "resample buf alloc failed");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "init ok");
    return ESP_OK;
}

/* 分片写入相关辅助（定义见后） */
static void player_clear_pending_out(void);

esp_err_t service_player_load(const char *path)
{
    if (s_state.loaded) {
        return ESP_ERR_INVALID_STATE;
    }
    if (path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 确保 PCM 缓冲已分配（init 幂等，首用即分配） */
    esp_err_t init_ret = service_player_init();
    if (init_ret != ESP_OK) {
        return init_ret;
    }

    player_clear_pending_out();

    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "open %s failed", path);
        return ESP_ERR_NOT_FOUND;
    }

    /* 获取文件大小 */
    fseek(fp, 0, SEEK_END);
    s_state.file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* 读取文件头（ID3v2 标签 + 首帧头，最多 64 KB） */
    size_t header_size = std::min(s_state.file_size, (size_t)65536);
    uint8_t *header_buf = (uint8_t *)malloc(header_size);
    if (header_buf == NULL) {
        fclose(fp);
        return ESP_ERR_NO_MEM;
    }
    size_t read = fread(header_buf, 1, header_size, fp);
    if (read < ID3V2_HEADER_SIZE) {
        free(header_buf);
        fclose(fp);
        return ESP_ERR_INVALID_SIZE;
    }

    /* 解析 ID3v2 标题 */
    memset(s_state.title, 0, sizeof(s_state.title));
    parse_id3v2_title(header_buf, read, s_state.title, sizeof(s_state.title));
    s_state.title_parsed = true;

    /* 计算 ID3v2 标签大小，跳过 */
    size_t id3_skip = 0;
    if (header_buf[0] == 'I' && header_buf[1] == 'D' && header_buf[2] == '3') {
        id3_skip = ID3V2_HEADER_SIZE + id3_syncsafe_to_uint(header_buf + 6);
    }
    free(header_buf);

    /* 创建解码器（lazy init，构造器不分配） */
    s_state.decoder = new (std::nothrow) micro_mp3::Mp3Decoder();
    if (s_state.decoder == NULL) {
        fclose(fp);
        return ESP_ERR_NO_MEM;
    }

    /* 定位到音频数据起始 */
    fseek(fp, (long)id3_skip, SEEK_SET);

    /* 初始化状态 */
    s_state.fp = fp;
    s_state.mp3_data_offset = id3_skip;
    s_state.read_ptr = s_state.read_buf;
    s_state.read_remaining = 0;
    s_state.eof = false;
    s_state.loaded = true;
    s_state.playing = false;
    s_state.paused = false;
    s_state.sample_rate = 0;
    s_state.channels = 0;
    s_state.bitrate_kbps = 0;
    s_state.total_output_frames = 0;
    s_state.resample_phase = 0.0;
    s_state.eos_sent = false;

    ESP_LOGI(TAG, "loaded: %s, title=%s", path, s_state.title);
    return ESP_OK;
}

void service_player_unload(void)
{
    if (s_state.decoder) {
        delete s_state.decoder;
        s_state.decoder = NULL;
    }
    if (s_state.fp) {
        fclose(s_state.fp);
        s_state.fp = NULL;
    }

    /* 清空 aux 缓冲 */
    service_audio_aux_clear();
    service_audio_aux_end_of_stream();

    memset(&s_state, 0, sizeof(s_state));
    /* 保留 PCM 缓冲（init 分配，unload 不释放） */
    ESP_LOGI(TAG, "unloaded");
}

esp_err_t service_player_play(void)
{
    if (!s_state.loaded) {
        return ESP_ERR_INVALID_STATE;
    }
    s_state.playing = true;
    s_state.paused = false;
    s_state.eos_sent = false;
    player_clear_pending_out();

    /* 如果已到文件末尾，从头开始 */
    if (s_state.eof && s_state.fp) {
        fseek(s_state.fp, (long)s_state.mp3_data_offset, SEEK_SET);
        s_state.eof = false;
        s_state.read_ptr = s_state.read_buf;
        s_state.read_remaining = 0;
        s_state.total_output_frames = 0;
        s_state.resample_phase = 0.0;
        /* 重新创建解码器（旧解码器已耗尽） */
        delete s_state.decoder;
        s_state.decoder = new (std::nothrow) micro_mp3::Mp3Decoder();
        if (s_state.decoder == NULL) {
            s_state.playing = false;
            return ESP_ERR_NO_MEM;
        }
    }

    ESP_LOGI(TAG, "play");
    return ESP_OK;
}

esp_err_t service_player_pause(void)
{
    if (!s_state.playing || s_state.paused) {
        return ESP_ERR_INVALID_STATE;
    }
    s_state.paused = true;
    s_state.playing = false;
    ESP_LOGI(TAG, "pause");
    return ESP_OK;
}

void service_player_stop(void)
{
    s_state.playing = false;
    s_state.paused = false;
    s_state.total_output_frames = 0;
    service_audio_aux_clear();
    service_audio_aux_end_of_stream();
    s_state.eos_sent = true;
    ESP_LOGI(TAG, "stop");
}

/* 填充文件读取缓冲 */
static void player_fill_read_buf(void)
{
    if (s_state.eof || s_state.fp == NULL) {
        return;
    }

    /* 有未读数据留在缓冲中，先移到头部 */
    if (s_state.read_remaining > 0 && s_state.read_ptr != s_state.read_buf) {
        memmove(s_state.read_buf, s_state.read_ptr, s_state.read_remaining);
    } else if (s_state.read_remaining > 0) {
        /* 缓冲已满（read_ptr == read_buf），无法再读 */
        return;
    }
    s_state.read_ptr = s_state.read_buf;

    size_t space = sizeof(s_state.read_buf) - s_state.read_remaining;
    size_t got = fread(s_state.read_buf + s_state.read_remaining, 1, space, s_state.fp);
    s_state.read_remaining += got;
    if (got == 0) {
        s_state.eof = true;
    }
}

/* 清除待写输出状态（load/play/stop/unload 时调用） */
static void player_clear_pending_out(void)
{
    s_state.out_ptr = NULL;
    s_state.out_total = 0;
    s_state.out_done = 0;
}

/* 分片写入待写输出帧：按 aux 剩余空间切片，返回是否已全部写完 */
static bool player_write_pending_slice(void)
{
    if (s_state.out_ptr == NULL || s_state.out_total == 0 ||
        s_state.out_done >= s_state.out_total) {
        return true;
    }
    uint32_t remain = s_state.out_total - s_state.out_done;
    uint32_t free_frames = service_audio_aux_free_frames();
    if (free_frames == 0) {
        return false;
    }
    uint32_t chunk = (remain < free_frames) ? remain : free_frames;
    uint32_t w = service_audio_aux_write(s_state.out_ptr + s_state.out_done * 2, chunk);
    s_state.out_done += w;
    s_state.total_output_frames += (int64_t)w;
    return s_state.out_done >= s_state.out_total;
}

esp_err_t service_player_poll(void)
{
    if (!s_state.playing || s_state.paused) {
        return ESP_ERR_INVALID_STATE;
    }

    /* 先排水上一解码帧未写完的部分（分片写入，保证不丢帧） */
    if (!player_write_pending_slice()) {
        return ESP_OK;   /* aux 满，等下一轮 */
    }

    for (int frame_i = 0; frame_i < MAX_DECODE_FRAMES_PER_POLL; frame_i++) {
        /* 确保读取缓冲有数据 */
        if (s_state.read_remaining == 0 && !s_state.eof) {
            player_fill_read_buf();
        }

        /* 尝试解码 */
        size_t consumed = 0;
        size_t samples = 0;

        micro_mp3::Mp3Result result = s_state.decoder->decode(
            s_state.read_ptr, s_state.read_remaining,
            reinterpret_cast<uint8_t *>(s_state.pcm_buf),
            MAX_PCM_SAMPLES * sizeof(int16_t),
            consumed, samples
        );

        s_state.read_ptr += consumed;
        s_state.read_remaining -= consumed;

        switch (result) {
        case micro_mp3::MP3_STREAM_INFO_READY:
            /* 首帧格式探测，无 PCM */
            s_state.sample_rate = s_state.decoder->get_sample_rate();
            s_state.channels = s_state.decoder->get_channels();
            s_state.bitrate_kbps = s_state.decoder->get_bitrate();
            s_state.resample_phase = 0.0;
            ESP_LOGI(TAG, "stream: %luHz %uch %lukbps",
                     (unsigned long)s_state.sample_rate, (unsigned)s_state.channels,
                     (unsigned long)s_state.bitrate_kbps);
            continue;

        case micro_mp3::MP3_STREAM_INFO_CHANGED:
            /* 流格式变更 */
            s_state.sample_rate = s_state.decoder->get_sample_rate();
            s_state.channels = s_state.decoder->get_channels();
            s_state.bitrate_kbps = s_state.decoder->get_bitrate();
            s_state.resample_phase = 0.0;
            ESP_LOGW(TAG, "stream changed: %luHz %uch %lukbps",
                     (unsigned long)s_state.sample_rate, (unsigned)s_state.channels,
                     (unsigned long)s_state.bitrate_kbps);
            continue;

        case micro_mp3::MP3_NEED_MORE_DATA:
            /* 需要更多数据 */
            if (s_state.eof) {
                goto playback_done;
            }
            /* 读更多数据 */
            player_fill_read_buf();
            continue;

        case micro_mp3::MP3_DECODE_ERROR:
            /* 可恢复：跳过坏帧，继续 */
            ESP_LOGW(TAG, "decode error frame skipped");
            continue;

        default:
            if (result < 0) {
                /* 致命错误，停止播放 */
                ESP_LOGE(TAG, "decode fatal error: %d", (int)result);
                s_state.playing = false;
                return ESP_FAIL;
            }
            break;
        }

        /* MP3_OK：有解码数据，产出一帧输出并分片写入 */
        if (samples > 0 && s_state.sample_rate > 0) {
            if (s_state.sample_rate == 44100 && s_state.channels == 2) {
                /* 直接写 aux（无需重采样） */
                s_state.out_ptr = s_state.pcm_buf;
                s_state.out_total = (uint32_t)samples;
            } else {
                /* 重采样到 44.1kHz 立体声：相位步进 = 输入率/输出率 */
                double ratio = (double)s_state.sample_rate / 44100.0;
                s_state.out_total = resample_linear(
                    s_state.pcm_buf, (uint32_t)samples, s_state.channels,
                    s_state.resample_buf, RESAMPLE_BUF_FRAMES,
                    &s_state.resample_phase, ratio);
                s_state.out_ptr = s_state.resample_buf;
            }
            s_state.out_done = 0;

            if (!player_write_pending_slice()) {
                break;   /* aux 满，等下一轮再写 */
            }
        }
    }

    return ESP_OK;

playback_done:
    ESP_LOGI(TAG, "playback finished");
    s_state.playing = false;
    if (!s_state.eos_sent) {
        service_audio_aux_end_of_stream();
        s_state.eos_sent = true;
    }
    return ESP_OK;
}

bool service_player_is_playing(void)
{
    return s_state.playing && !s_state.paused;
}

bool service_player_is_loaded(void)
{
    return s_state.loaded;
}

const char *service_player_get_title(void)
{
    if (!s_state.loaded) return NULL;
    /* 优先返回 ID3v2 标题 */
    if (s_state.title[0] != '\0') {
        return s_state.title;
    }
    return NULL;
}

uint32_t service_player_get_sample_rate(void)
{
    return s_state.sample_rate;
}

uint8_t service_player_get_channels(void)
{
    return s_state.channels;
}

int64_t service_player_get_position_us(void)
{
    if (s_state.total_output_frames <= 0) return 0;
    return s_state.total_output_frames * 1000000LL / 44100;
}

int64_t service_player_get_duration_us(void)
{
    if (!s_state.loaded || s_state.bitrate_kbps == 0) {
        return 0;
    }
    /* 估算：MP3 音频数据大小 / 码率 × 8 × 1000000 */
    size_t mp3_data_size = s_state.file_size - s_state.mp3_data_offset;
    if (mp3_data_size == 0) return 0;
    return (int64_t)mp3_data_size * 8 * 1000000LL / (int64_t)(s_state.bitrate_kbps * 1000);
}