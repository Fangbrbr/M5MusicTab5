/**
 * @file engine_midi_rec.c
 * @brief Hammy MIDI Recording（.hmr）格式解析器实现
 */

#include "engine_midi_rec.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "engine_midi_rec";

typedef struct {
    uint32_t start_time_epoch;
    uint32_t duration_ms;
    uint32_t event_count;
    uint32_t channels_used;
    uint32_t data_crc32;
    uint32_t header_crc32;
    char     source_tag[16];
    uint8_t  channel_init[16][4];
} hmr_header_fields_t;

static const uint32_t s_crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x4c6c5452, 0x4764524e, 0x50d5b5e9,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af,
    0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
    0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1, 0xf00f9344, 0x8708a3d2,
    0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9,
    0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,
    0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c,
    0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703,
    0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,
    0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226,
    0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d,
    0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
    0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777, 0x88085ae6, 0xff0f6a70,
    0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7,
    0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
    0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9, 0xbdbdf21c, 0xcabac28a,
    0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1,
    0x5a05df1b, 0x2d02ef8d
};

static uint32_t read_u32_le(const uint8_t *p);
static uint16_t read_u16_le(const uint8_t *p);
static bool hmr_header_unpack(const uint8_t buf[ENGINE_MIDI_REC_HEADER_SIZE], hmr_header_fields_t *fields);
static bool hmr_event_unpack(const uint8_t *buf, size_t buf_len, engine_midi_rec_event_t *ev, size_t *consumed);

uint32_t engine_midi_rec_crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ s_crc32_table[(crc ^ data[i]) & 0xFF];
    }
    return crc;
}

esp_err_t engine_midi_rec_parse_file(const char *path, engine_midi_rec_t *out)
{
    memset(out, 0, sizeof(*out));

    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "open failed: %s", path);
        return ESP_FAIL;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    esp_err_t ret = ESP_FAIL;
    uint8_t hdr_buf[ENGINE_MIDI_REC_HEADER_SIZE];
    hmr_header_fields_t hdr;

    do {
        if (fsize < (long)(ENGINE_MIDI_REC_HEADER_SIZE + 4)) {
            break;
        }
        if (fread(hdr_buf, 1, ENGINE_MIDI_REC_HEADER_SIZE, f) != ENGINE_MIDI_REC_HEADER_SIZE) {
            break;
        }
        if (!hmr_header_unpack(hdr_buf, &hdr)) {
            ESP_LOGE(TAG, "invalid header: %s", path);
            break;
        }

        long event_data_size = fsize - ENGINE_MIDI_REC_HEADER_SIZE - 4;
        if (event_data_size < 0) {
            break;
        }

        size_t cap = (size_t)event_data_size / 12 + 16;
        out->events = heap_caps_malloc(cap * sizeof(engine_midi_rec_event_t), MALLOC_CAP_SPIRAM);
        if (out->events == NULL) {
            ESP_LOGE(TAG, "no mem for %u events", (unsigned)cap);
            ret = ESP_ERR_NO_MEM;
            break;
        }

        uint8_t ev_buf[12 + ENGINE_MIDI_SYSEX_BUF_SIZE];
        uint32_t computed_crc = ENGINE_MIDI_REC_CRC32_INIT;
        size_t pos = 0;
        bool parse_ok = true;

        while (pos < (size_t)event_data_size) {
            size_t need = 12;
            if (fread(ev_buf, 1, need, f) != need) {
                parse_ok = false;
                break;
            }
            uint8_t sysex_len = ev_buf[10];
            if (sysex_len > ENGINE_MIDI_SYSEX_BUF_SIZE) {
                parse_ok = false;
                break;
            }
            if (fread(&ev_buf[12], 1, sysex_len, f) != sysex_len) {
                parse_ok = false;
                break;
            }
            size_t ev_size = 12 + sysex_len;
            computed_crc = engine_midi_rec_crc32_update(computed_crc, ev_buf, ev_size);

            if (out->event_count >= cap) {
                parse_ok = false;
                break;
            }
            engine_midi_rec_event_t *ev = &out->events[out->event_count++];
            size_t consumed = 0;
            if (!hmr_event_unpack(ev_buf, ev_size, ev, &consumed)) {
                parse_ok = false;
                break;
            }
            (void)consumed;
            pos += ev_size;
        }

        if (!parse_ok || pos != (size_t)event_data_size) {
            break;
        }

        uint8_t footer[4];
        if (fread(footer, 1, 4, f) != 4 || memcmp(footer, ENGINE_MIDI_REC_END_MAGIC, 4) != 0) {
            break;
        }

        if (engine_midi_rec_crc32_final(computed_crc) != hdr.data_crc32) {
            ESP_LOGE(TAG, "data crc mismatch: %s", path);
            break;
        }

        uint64_t header_dur_us = (uint64_t)hdr.duration_ms * 1000;
        uint64_t event_dur_us = 0;
        if (out->event_count > 0) {
            event_dur_us = out->events[out->event_count - 1].time_us;
        }
        /* 旧版本 bug 录制的文件头部时长字段可能错误（如溢出/欠载巨大值）；
         * 头部值与事件时间轴偏离超过 2 倍时以事件时间轴为准 */
        if (event_dur_us > 0 &&
            (header_dur_us < event_dur_us / 2 || header_dur_us > event_dur_us * 2)) {
            ESP_LOGW(TAG, "header dur %llu us deviates from events %llu us, use events",
                     (unsigned long long)header_dur_us, (unsigned long long)event_dur_us);
            out->total_us = event_dur_us;
        } else {
            out->total_us = header_dur_us;
        }
        out->channels_used = hdr.channels_used;
        memcpy(out->source_tag, hdr.source_tag, sizeof(out->source_tag));
        out->source_tag[sizeof(out->source_tag) - 1] = '\0';
        memcpy(out->channel_init, hdr.channel_init, sizeof(out->channel_init));
        snprintf(out->song_name, sizeof(out->song_name), "%s", out->source_tag);

        ESP_LOGI(TAG, "parsed %s: events=%u dur=%us ch=%u",
                 path, (unsigned)out->event_count,
                 (unsigned)(out->total_us / 1000000), (unsigned)out->channels_used);
        ret = ESP_OK;
    } while (0);

    fclose(f);

    if (ret != ESP_OK) {
        if (out->events != NULL) {
            heap_caps_free(out->events);
        }
        memset(out, 0, sizeof(*out));
    }
    return ret;
}

void engine_midi_rec_free(engine_midi_rec_t *rec)
{
    if (rec == NULL) {
        return;
    }
    if (rec->events != NULL) {
        heap_caps_free(rec->events);
    }
    memset(rec, 0, sizeof(*rec));
}

static uint32_t read_u32_le(const uint8_t *p)
{
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_u16_le(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0]) | ((uint16_t)p[1] << 8));
}

static bool hmr_header_unpack(const uint8_t buf[ENGINE_MIDI_REC_HEADER_SIZE], hmr_header_fields_t *fields)
{
    if (memcmp(buf, ENGINE_MIDI_REC_MAGIC, 4) != 0) {
        return false;
    }
    if (buf[4] != ENGINE_MIDI_REC_VERSION) {
        return false;
    }
    if (read_u32_le(&buf[8]) != ENGINE_MIDI_REC_HEADER_SIZE) {
        return false;
    }

    uint8_t tmp[ENGINE_MIDI_REC_HEADER_SIZE];
    memcpy(tmp, buf, ENGINE_MIDI_REC_HEADER_SIZE);
    tmp[32] = 0;
    tmp[33] = 0;
    tmp[34] = 0;
    tmp[35] = 0;
    uint32_t computed_crc = engine_midi_rec_crc32(tmp, ENGINE_MIDI_REC_HEADER_SIZE);
    if (computed_crc != read_u32_le(&buf[32])) {
        return false;
    }

    fields->start_time_epoch = read_u32_le(&buf[12]);
    fields->duration_ms = read_u32_le(&buf[16]);
    fields->event_count = read_u32_le(&buf[20]);
    fields->channels_used = read_u32_le(&buf[24]);
    fields->data_crc32 = read_u32_le(&buf[28]);
    fields->header_crc32 = read_u32_le(&buf[32]);
    memcpy(fields->source_tag, &buf[36], 16);
    fields->source_tag[15] = '\0';
    memcpy(fields->channel_init, &buf[64], sizeof(fields->channel_init));
    return true;
}

static bool hmr_event_unpack(const uint8_t *buf, size_t buf_len, engine_midi_rec_event_t *ev, size_t *consumed)
{
    if (buf_len < 12) {
        return false;
    }
    uint32_t rel_ms = read_u32_le(&buf[0]);
    ev->time_us = (uint64_t)rel_ms * 1000;
    ev->type = buf[4];
    ev->channel = buf[5];
    ev->data1 = buf[6];
    ev->data2 = buf[7];
    ev->value = read_u16_le(&buf[8]);
    ev->sysex_len = buf[10];
    size_t need = 12 + ev->sysex_len;
    if (buf_len < need || ev->sysex_len > ENGINE_MIDI_SYSEX_BUF_SIZE) {
        return false;
    }
    if (ev->sysex_len > 0) {
        memcpy(ev->sysex_data, &buf[12], ev->sysex_len);
    }
    *consumed = need;
    return true;
}
