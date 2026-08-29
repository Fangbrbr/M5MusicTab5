/**
 * @file engine_midi_smf_write.c
 * @brief 标准 MIDI 文件（SMF）format 0 序列化器实现
 */

#include "engine_midi_smf_write.h"
#include <string.h>

#define SMF_MTHD_SIZE           14      /**< MThd 块总长（8 头 + 6 数据） */
#define SMF_MTRK_LEN_OFFSET     (SMF_MTHD_SIZE + 4)  /**< MTrk length 字段文件偏移 */
#define SMF_TRACK_NAME_MAX      31
#define SMF_EVENT_BUF_SIZE      8       /**< VLQ(<=4B) + 状态 + 2 数据字节 */

static uint32_t smf_ms_to_tick(uint32_t abs_ms);
static size_t smf_put_vlq(uint8_t *out, uint32_t v);
static esp_err_t smf_write(engine_midi_smf_writer_t *w, const void *buf, size_t len);
static esp_err_t smf_write_meta(engine_midi_smf_writer_t *w, uint8_t meta,
                                const uint8_t *data, uint32_t len);
static void smf_put_be32(uint8_t *p, uint32_t v);

esp_err_t engine_midi_smf_writer_begin(engine_midi_smf_writer_t *w, FILE *fp,
                                       const char *track_name)
{
    if (w == NULL || fp == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    w->fp = fp;
    w->last_tick = 0;
    w->track_bytes = 0;
    w->failed = false;

    /* MThd：format 0、1 轨、PPQ 480；MTrk 长度收尾时回填 */
    const uint8_t header[SMF_MTHD_SIZE + 8] = {
        'M', 'T', 'h', 'd', 0, 0, 0, 6,
        0, 0, 0, 1,
        (uint8_t)(ENGINE_MIDI_SMF_WRITE_PPQ >> 8), (uint8_t)(ENGINE_MIDI_SMF_WRITE_PPQ & 0xFF),
        'M', 'T', 'r', 'k', 0, 0, 0, 0,
    };
    if (fwrite(header, 1, sizeof(header), fp) != sizeof(header)) {
        w->failed = true;
        return ESP_FAIL;
    }

    const uint8_t tempo[3] = {
        (uint8_t)(ENGINE_MIDI_SMF_WRITE_TEMPO_USQN >> 16),
        (uint8_t)(ENGINE_MIDI_SMF_WRITE_TEMPO_USQN >> 8),
        (uint8_t)(ENGINE_MIDI_SMF_WRITE_TEMPO_USQN),
    };
    const uint8_t time_sig[4] = { 4, 2, 24, 8 };    /* 4/4，标准默认参数 */
    if (smf_write_meta(w, 0x51, tempo, sizeof(tempo)) != ESP_OK ||
        smf_write_meta(w, 0x58, time_sig, sizeof(time_sig)) != ESP_OK) {
        return ESP_FAIL;
    }
    if (track_name != NULL && track_name[0] != '\0') {
        size_t len = strnlen(track_name, SMF_TRACK_NAME_MAX);
        if (smf_write_meta(w, 0x03, (const uint8_t *)track_name, (uint32_t)len) != ESP_OK) {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

esp_err_t engine_midi_smf_writer_event(engine_midi_smf_writer_t *w, uint32_t abs_ms,
                                       uint8_t type, uint8_t ch, uint8_t d1, uint8_t d2,
                                       uint16_t value14)
{
    if (w == NULL || w->fp == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (w->failed) {
        return ESP_FAIL;
    }

    /* 只落标准通道消息；系统消息/内部 SysEx 不属于音乐内容 */
    if (type < 0x80 || type > 0xE0) {
        return ESP_OK;
    }

    uint32_t tick = smf_ms_to_tick(abs_ms);
    if (tick < w->last_tick) {
        tick = w->last_tick;    /* 时间戳乱序兜底：delta 不得为负 */
    }

    uint8_t buf[SMF_EVENT_BUF_SIZE];
    size_t pos = smf_put_vlq(buf, tick - w->last_tick);
    buf[pos++] = (uint8_t)(type | (ch & 0x0F));
    if (type == 0xE0) {
        if (value14 > 0x3FFF) {
            value14 = 0x3FFF;
        }
        buf[pos++] = (uint8_t)(value14 & 0x7F);
        buf[pos++] = (uint8_t)((value14 >> 7) & 0x7F);
    } else {
        buf[pos++] = (uint8_t)(d1 & 0x7F);
        if (type != 0xC0 && type != 0xD0) {
            buf[pos++] = (uint8_t)(d2 & 0x7F);
        }
    }

    esp_err_t ret = smf_write(w, buf, pos);
    if (ret == ESP_OK) {
        w->last_tick = tick;
    }
    return ret;
}

esp_err_t engine_midi_smf_writer_end(engine_midi_smf_writer_t *w)
{
    if (w == NULL || w->fp == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (w->failed) {
        return ESP_FAIL;
    }

    const uint8_t eot[4] = { 0x00, 0xFF, 0x2F, 0x00 };
    if (smf_write(w, eot, sizeof(eot)) != ESP_OK) {
        return ESP_FAIL;
    }

    uint8_t len_be[4];
    smf_put_be32(len_be, w->track_bytes);
    if (fseek(w->fp, SMF_MTRK_LEN_OFFSET, SEEK_SET) != 0 ||
        fwrite(len_be, 1, 4, w->fp) != 4 ||
        fseek(w->fp, 0, SEEK_END) != 0) {
        w->failed = true;
        return ESP_FAIL;
    }
    return ESP_OK;
}

/* tick = ms * PPQ / 500（120 BPM 下 500ms/拍），绝对换算避免 delta 累积漂移 */
static uint32_t smf_ms_to_tick(uint32_t abs_ms)
{
    return (uint32_t)(((uint64_t)abs_ms * ENGINE_MIDI_SMF_WRITE_PPQ * 1000) /
                      ENGINE_MIDI_SMF_WRITE_TEMPO_USQN);
}

static size_t smf_put_vlq(uint8_t *out, uint32_t v)
{
    uint8_t tmp[5];
    size_t n = 0;
    tmp[n++] = (uint8_t)(v & 0x7F);
    v >>= 7;
    while (v != 0) {
        tmp[n++] = (uint8_t)(0x80 | (v & 0x7F));
        v >>= 7;
    }
    for (size_t i = 0; i < n; i++) {
        out[i] = tmp[n - 1 - i];
    }
    return n;
}

static esp_err_t smf_write(engine_midi_smf_writer_t *w, const void *buf, size_t len)
{
    if (fwrite(buf, 1, len, w->fp) != len) {
        w->failed = true;
        return ESP_FAIL;
    }
    w->track_bytes += (uint32_t)len;
    return ESP_OK;
}

static esp_err_t smf_write_meta(engine_midi_smf_writer_t *w, uint8_t meta,
                                const uint8_t *data, uint32_t len)
{
    uint8_t head[3 + 5];
    size_t pos = 0;
    head[pos++] = 0x00;                 /* delta time */
    head[pos++] = 0xFF;
    head[pos++] = meta;
    pos += smf_put_vlq(&head[pos], len);
    if (smf_write(w, head, pos) != ESP_OK) {
        return ESP_FAIL;
    }
    if (len > 0 && smf_write(w, data, len) != ESP_OK) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void smf_put_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v);
}
