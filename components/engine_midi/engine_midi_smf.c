/**
 * @file engine_midi_smf.c
 * @brief 标准 MIDI 文件（SMF）解析器实现
 */

#include "engine_midi_smf.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "engine_midi_smf";

typedef struct
{
    uint32_t tick;
    uint32_t us_per_quarter;
} smf_tempo_point_t;

static uint32_t smf_read_u32(FILE *f);
static uint16_t smf_read_u16(FILE *f);
static uint32_t smf_read_vlq(FILE *f);
static int smf_event_cmp(const void *a, const void *b);
static int smf_tempo_cmp(const void *a, const void *b);
static bool smf_parse_track(FILE *f, engine_midi_smf_t *out, size_t events_cap,
                            smf_tempo_point_t *tmap, size_t *tmap_count, size_t tmap_cap,
                            uint16_t *ch_mask);

esp_err_t engine_midi_smf_parse_file(const char *path, engine_midi_smf_t *out)
{
    memset(out, 0, sizeof(*out));

    FILE *f = fopen(path, "rb");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "open failed: %s", path);
        return ESP_FAIL;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize < 14)
    {
        fclose(f);
        return ESP_FAIL;
    }

    /* 事件容量按文件大小上限估算（每事件至少 3 字节），避免二次扫描 */
    size_t events_cap = (size_t)fsize / 3 + 16;
    out->events = heap_caps_malloc(events_cap * sizeof(engine_midi_smf_event_t), MALLOC_CAP_SPIRAM);
    if (out->events == NULL)
    {
        ESP_LOGE(TAG, "no mem for %u events", (unsigned)events_cap);
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    smf_tempo_point_t tmap[64];
    size_t tmap_count = 0;
    uint16_t ch_mask = 0;
    uint16_t ntracks = 0;
    uint16_t division = 0;
    bool ok = false;

    do
    {
        char id[4];
        if (fread(id, 1, 4, f) != 4 || memcmp(id, "MThd", 4) != 0 || smf_read_u32(f) != 6)
        {
            break;
        }
        uint16_t format = smf_read_u16(f);
        ntracks = smf_read_u16(f);
        division = smf_read_u16(f);
        if (format > 1 || ntracks == 0 || ntracks > 64 || division == 0)
        {
            break;
        }

        ok = true;
        for (uint16_t t = 0; t < ntracks && ok; t++)
        {
            ok = smf_parse_track(f, out, events_cap, tmap, &tmap_count,
                                 sizeof(tmap) / sizeof(tmap[0]), &ch_mask);
        }
    } while (0);
    fclose(f);

    if (!ok || out->event_count == 0)
    {
        ESP_LOGE(TAG, "parse failed: %s", path);
        heap_caps_free(out->events);
        memset(out, 0, sizeof(*out));
        return ESP_FAIL;
    }

    /* 按绝对 tick 归并排序后，用 tempo map 换算绝对时间 */
    qsort(out->events, out->event_count, sizeof(engine_midi_smf_event_t), smf_event_cmp);
    qsort(tmap, tmap_count, sizeof(smf_tempo_point_t), smf_tempo_cmp);

    uint32_t first_uspq = (tmap_count > 0) ? tmap[0].us_per_quarter : 500000;
    out->bpm = (uint16_t)((60000000UL + first_uspq / 2) / first_uspq);

    if (division & 0x8000)
    {
        /* SMPTE：高字节负帧率，低字节每帧 tick */
        int fps = 256 - (int)(division >> 8);
        int tpf = division & 0xFF;
        uint32_t us_per_tick = (fps > 0 && tpf > 0) ? (1000000UL / (uint32_t)(fps * tpf)) : 0;
        for (uint32_t i = 0; i < out->event_count; i++)
        {
            out->events[i].time_us = out->events[i].tick * us_per_tick;
        }
    }
    else
    {
        uint32_t uspq = 500000;
        uint32_t last_tick = 0;
        uint64_t cur_us = 0;
        size_t ti = 0;
        for (uint32_t i = 0; i < out->event_count; i++)
        {
            uint32_t tick = out->events[i].tick;
            while (ti < tmap_count && tmap[ti].tick <= tick)
            {
                cur_us += (uint64_t)(tmap[ti].tick - last_tick) * uspq / division;
                last_tick = tmap[ti].tick;
                uspq = tmap[ti].us_per_quarter;
                ti++;
            }
            out->events[i].time_us = (uint32_t)(cur_us + (uint64_t)(tick - last_tick) * uspq / division);
        }
    }

    out->total_us = out->events[out->event_count - 1].time_us;
    for (int ch = 0; ch < 16; ch++)
    {
        if (ch_mask & (1U << ch))
        {
            out->channels_used++;
        }
    }

    ESP_LOGI(TAG, "parsed %s: events=%u dur=%us bpm=%u ch=%u",
             path, (unsigned)out->event_count, (unsigned)(out->total_us / 1000000),
             out->bpm, out->channels_used);
    return ESP_OK;
}

void engine_midi_smf_free(engine_midi_smf_t *smf)
{
    if (smf == NULL)
    {
        return;
    }
    if (smf->events != NULL)
    {
        heap_caps_free(smf->events);
    }
    memset(smf, 0, sizeof(*smf));
}

static uint32_t smf_read_u32(FILE *f)
{
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4)
    {
        return 0;
    }
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
}

static uint16_t smf_read_u16(FILE *f)
{
    uint8_t b[2];
    if (fread(b, 1, 2, f) != 2)
    {
        return 0;
    }
    return (uint16_t)(((uint16_t)b[0] << 8) | b[1]);
}

static uint32_t smf_read_vlq(FILE *f)
{
    uint32_t v = 0;
    for (int i = 0; i < 4; i++)
    {
        int c = fgetc(f);
        if (c < 0)
        {
            break;
        }
        v = (v << 7) | (uint32_t)(c & 0x7F);
        if (!(c & 0x80))
        {
            break;
        }
    }
    return v;
}

static int smf_event_cmp(const void *a, const void *b)
{
    uint32_t ta = ((const engine_midi_smf_event_t *)a)->tick;
    uint32_t tb = ((const engine_midi_smf_event_t *)b)->tick;
    return (ta > tb) - (ta < tb);
}

static int smf_tempo_cmp(const void *a, const void *b)
{
    uint32_t ta = ((const smf_tempo_point_t *)a)->tick;
    uint32_t tb = ((const smf_tempo_point_t *)b)->tick;
    return (ta > tb) - (ta < tb);
}

/* 解析单个 MTrk，事件追加到 out->events（容量 events_cap），返回 false 表示格式错误 */
static bool smf_parse_track(FILE *f, engine_midi_smf_t *out, size_t events_cap,
                            smf_tempo_point_t *tmap, size_t *tmap_count, size_t tmap_cap,
                            uint16_t *ch_mask)
{
    char id[4];
    if (fread(id, 1, 4, f) != 4 || memcmp(id, "MTrk", 4) != 0)
    {
        return false;
    }
    uint32_t len = smf_read_u32(f);
    long track_end = ftell(f) + (long)len;

    uint32_t tick = 0;
    uint8_t status = 0;
    bool eot = false;

    while (!eot && ftell(f) < track_end)
    {
        tick += smf_read_vlq(f);

        int c = fgetc(f);
        if (c < 0)
        {
            break;
        }

        uint8_t type;
        if (c < 0x80)
        {
            /* running status：c 是第一个数据字节 */
            if (status == 0)
            {
                return false;
            }
            type = status;
            ungetc(c, f);
        }
        else
        {
            type = (uint8_t)c;
            if (type < 0xF0)
            {
                status = type;
            }
        }

        if (type == 0xFF)
        {
            int meta = fgetc(f);
            uint32_t mlen = smf_read_vlq(f);
            long next = ftell(f) + (long)mlen;
            if (meta == 0x2F)
            {
                eot = true;
            }
            else if (meta == 0x51 && mlen == 3)
            {
                uint8_t tb[3];
                if (fread(tb, 1, 3, f) != 3)
                {
                    return false;
                }
                if (*tmap_count < tmap_cap)
                {
                    tmap[*tmap_count].tick = tick;
                    tmap[*tmap_count].us_per_quarter =
                        ((uint32_t)tb[0] << 16) | ((uint32_t)tb[1] << 8) | tb[2];
                    (*tmap_count)++;
                }
            }
            else if (meta == 0x03 && out->song_name[0] == '\0' && mlen > 0)
            {
                // Sequence/Track Name - 回退
                size_t n = mlen < (ENGINE_MIDI_SMF_SONG_NAME_MAX - 1) ? mlen : (ENGINE_MIDI_SMF_SONG_NAME_MAX - 1);
                if (fread(out->song_name, 1, n, f) != n)
                {
                    return false;
                }
                out->song_name[n] = '\0';
            }
            else if (meta == 0x01 && out->song_name[0] == '\0' && mlen > 0)
            {
                // Text Event - 优先作为歌曲名
                size_t n = mlen < (ENGINE_MIDI_SMF_SONG_NAME_MAX - 1) ? mlen : (ENGINE_MIDI_SMF_SONG_NAME_MAX - 1);
                if (fread(out->song_name, 1, n, f) != n)
                {
                    return false;
                }
                out->song_name[n] = '\0';
            }
            fseek(f, next, SEEK_SET);
        }
        else if (type == 0xF0 || type == 0xF7)
        {
            uint32_t slen = smf_read_vlq(f);
            fseek(f, (long)slen, SEEK_CUR);
        }
        else
        {
            uint8_t hi = type & 0xF0;
            uint8_t ch = type & 0x0F;
            int need = (hi == 0xC0 || hi == 0xD0) ? 1 : 2;
            int d1 = fgetc(f);
            int d2 = (need == 2) ? fgetc(f) : 0;
            if (d1 < 0 || d2 < 0)
            {
                return false;
            }

            if ((hi == 0x80 || hi == 0x90 || hi == 0xB0 || hi == 0xC0 || hi == 0xE0) &&
                out->event_count < events_cap)
            {
                engine_midi_smf_event_t *ev = &out->events[out->event_count++];
                ev->tick = tick;
                ev->time_us = 0;
                ev->type = hi;
                ev->channel = ch;
                ev->data1 = (uint8_t)d1;
                ev->data2 = (uint8_t)d2;
                *ch_mask |= (uint16_t)(1U << ch);
            }
        }
    }

    fseek(f, track_end, SEEK_SET);
    return true;
}
