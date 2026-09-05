/**
 * @file app_midi_player.c
 * @brief 通用播放器：SD 卡音乐(.mp3) / MIDI(.mid) / 录音(.mid) 三模式扫描与播放
 *
 * 左侧文件清单按三个互斥按钮（音乐/录音/MIDI）分三面板显示；
 * 右侧播放控制视图显示曲名/路径/通道数/BPM/进度/时间。
 * - MIDI 与录音统一为标准 SMF，解析由 engine_midi_smf，合成统一交给 engine_sf2；
 * - MP3 由 service_player（micro-mp3 桥接）解码重采样后写入 aux 混音流。
 *
 * SF2 隔离策略：MP3 进入播放（PLAYING）时停用 SF2 主源并挂起 AI（TTS 同走 aux 通道，
 * 避免与 MP3 竞争生产者），直到停止/切模式/退出播放器才恢复 MIDI 链路。
 */

#include "app_midi_player.h"
#include "app_manager.h"
#include "engine_gui.h"
#include "engine_midi.h"
#include "engine_midi_smf.h"
#include "service_audio.h"
#include "service_i18n.h"
#include "service_player.h"
#include "service_nvs.h"
#include "service_timer.h"
#include "service_xiaozhi.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

static const char *TAG = "app_midi_player";

#define MIDI_SCAN_DIR_MUSIC     "/sdcard/music"
#define MIDI_SCAN_DIR_MIDI      "/sdcard/midi"
#define MIDI_SCAN_DIR_RECORD    "/sdcard/record"
#define MIDI_SCAN_DIR_FALLBACK  "/sdcard"
#define MIDI_MAX_FILES          64
#define MIDI_FILE_ICON          "\xEF\x87\x87 "   /* 与 EEZ 示例条目一致的图标前缀 */
#define MIDI_UI_REFRESH_MS      200

/* -------------------- UI -------------------- */

typedef struct {
    lv_obj_t *btn_home;
    lv_obj_t *btn_set;
    lv_obj_t *panel_music_list;
    lv_obj_t *list_music;
    lv_obj_t *panel_mid_list;
    lv_obj_t *list_mid;
    lv_obj_t *file_example;
    lv_obj_t *panel_record_list;
    lv_obj_t *list_rec;
    lv_obj_t *del_msgbox;
    lv_obj_t *name_label;
    lv_obj_t *path_label;
    lv_obj_t *track_count;
    lv_obj_t *bpm_num;
    lv_obj_t *prev;
    lv_obj_t *play_stop;
    lv_obj_t *play_stop_label;
    lv_obj_t *next;
    lv_obj_t *progress;
    lv_obj_t *time_now;
    lv_obj_t *time_total;
    lv_obj_t *panel_set;
    lv_obj_t *set_btn_return;
    lv_obj_t *btn_music;
    lv_obj_t *btn_mid;
    lv_obj_t *btn_record;
} ui_screen_midi_t;

static ui_screen_midi_t s_midi_ui = {0};

static const widget_binding_t s_midi_bindings[] = {
    WIDGET_BIND(ui_screen_midi_t, btn_home,        "midi_btn_home",          WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_midi_t, btn_set,         "midi_btn_set",           WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_midi_t, panel_music_list,"midi_panel_music_list",  WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_midi_t, list_music,      "midi_list_music_file",   WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_midi_t, panel_mid_list,  "midi_panel_mid_list",    WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_midi_t, list_mid,        "midi_list_mid_file",     WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_midi_t, file_example,    "midi_file_example",      WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_midi_t, panel_record_list,  "midi_panel_record_list",    WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_midi_t, list_rec,        "midi_list_record_file",  WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_midi_t, del_msgbox,      "midi_del_msgbox",        WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_midi_t, name_label,      "midi_music_name_label",  WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_midi_t, path_label,      "midi_music_path_label",  WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_midi_t, track_count,     "midi_music_track_count", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_midi_t, bpm_num,         "midi_music_bpm_num",     WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_midi_t, prev,            "midi_prev",              WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_midi_t, play_stop,       "midi_play_stop",         WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_midi_t, play_stop_label, "midi_play_stop_label",   WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_midi_t, next,            "midi_next",              WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_midi_t, progress,        "midi_progress",           WIDGET_KIND_SLIDER),
    WIDGET_BIND(ui_screen_midi_t, time_now,        "midi_play_time_now",     WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_midi_t, time_total,      "midi_play_time_total",   WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_midi_t, panel_set,       "midi_set",               WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_midi_t, set_btn_return,  "midi_set_btn_return",    WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_midi_t, btn_music,       "midi_btn_music",         WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_midi_t, btn_mid,         "midi_btn_mid",           WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_midi_t, btn_record,         "midi_btn_record",           WIDGET_KIND_ANY),
    WIDGET_BINDING_END,
};

/* -------------------- 文件扫描缓存 -------------------- */

typedef struct {
    char name[96];    /* 文件名（含后缀） */
    char path[192];   /* 完整路径 */
    time_t mtime;     /* 文件修改时间，用于录音按时间排序 */
} midi_file_item_t;

typedef enum {
    PLAY_TYPE_MIDI = 0,   /* 与 NVS 旧存档兼容 */
    PLAY_TYPE_REC,        /* 1（录音，.mid） */
    PLAY_TYPE_MUSIC,      /* 2 */
} play_type_t;

/**
 * @brief 播放器状态
 */
typedef enum {
    PLAYER_STATE_IDLE,      /* 未加载文件 */
    PLAYER_STATE_READY,     /* 已加载文件，停止 */
    PLAYER_STATE_PLAYING,   /* 播放中 */
    PLAYER_STATE_FINISHED,  /* 已播放到末尾 */
    PLAYER_STATE_SEEKING,   /* 跳转中（临时状态，屏蔽滑块 VALUE_CHANGED） */
    PLAYER_STATE_COUNT,
} player_state_t;

typedef struct {
    midi_file_item_t music_files[MIDI_MAX_FILES];
    midi_file_item_t midi_files[MIDI_MAX_FILES];
    midi_file_item_t rec_files[MIDI_MAX_FILES];
    int music_file_count;
    int midi_file_count;
    int rec_file_count;
    bool scanned;
    int cur_music_index;                 /* 当前音乐选中，-1 无 */
    int cur_midi_index;                  /* 当前 MIDI 选中，-1 无 */
    int cur_rec_index;                   /* 当前录音选中，-1 无 */
    play_type_t play_type;               /* 当前显示/操作类别 */

    engine_midi_smf_t smf;               /* 当前曲目（MIDI/录音均为 SMF）解析结果 */
    bool is_music;                       /* 当前曲目为 .mp3 音乐 */
    bool midi_suspended;                 /* SF2 主源已被隔离（MP3 播放中） */

    player_state_t state;                /* 当前播放状态 */
    player_state_t state_before_seek;    /* 进入 SEEKING 前的状态 */
    uint32_t play_idx;
    int64_t play_time_us;
    int64_t last_pump_us;
    uint32_t last_ui_ms;
    service_timer_handle_t midi_timer;   /* MIDI 时基周期 hook（C6） */
    bool play_finished_flag;             /* hook 置位，on_update 消费（跨任务） */
} midi_player_state_t;

static midi_player_state_t s_mp = {0};

/* 仅用于遮罩 UI 刷新对 progress 滑块赋值引发的 VALUE_CHANGED，
 * 不得借道 SEEKING 状态机（leave(PLAYING) 会 all_notes_off，
 * 曾导致播放中每 200ms 全场断音：长音变短促、XY 弯音流被掐断） */
static bool s_ui_progress_mask = false;

/* -------------------- MIDI 总线 -------------------- */

static void midi_send(uint8_t type, uint8_t ch, uint8_t d1, uint8_t d2)
{
    engine_midi_event_t midi = {0};
    midi.type = type;
    midi.channel = ch;
    midi.data1 = d1;
    midi.data2 = d2;
    midi.source_port = ENGINE_MIDI_PORT_APP;
    engine_midi_publish(&midi, 0);
}

static void midi_send_bend(uint8_t ch, uint16_t value14)
{
    engine_midi_event_t midi = {0};
    midi.type = ENGINE_MIDI_MSG_PITCH_BEND;
    midi.channel = ch;
    midi.value = value14;
    midi.source_port = ENGINE_MIDI_PORT_APP;
    engine_midi_publish(&midi, 0);
}

static void midi_all_notes_off(void)
{
    for (int ch = 0; ch < 16; ch++) {
        midi_send(ENGINE_MIDI_MSG_CONTROL_CHANGE, (uint8_t)ch, 123, 0);
    }
}

/* -------------------- MIDI 时基（C6：service_timer hook） -------------------- */

#define MIDI_TICK_US  2000   /* 2ms 周期：SMF 事件时基为 us 级，500Hz 足够精确 */

/* MIDI/录音 时基 hook：只推进解析与发事件（非阻塞，不碰 LVGL）。
 * Trap: 运行在 esp_timer 任务；加载/停止/换曲期间先注销本 hook（状态切换
 * 均在 task_app），数组 smf.events 在 load 后不再变化，此处只读+推进 play_idx。 */
static void midi_tick_hook(void *arg)
{
    (void)arg;
    if (s_mp.is_music) {
        return;   /* MP3 解码泵留 on_update（2 秒 aux 缓冲天然抗卡） */
    }
    if (s_mp.state != PLAYER_STATE_PLAYING || s_mp.smf.events == NULL ||
        s_mp.smf.event_count == 0) {
        return;
    }

    int64_t now = esp_timer_get_time();
    s_mp.play_time_us += now - s_mp.last_pump_us;
    s_mp.last_pump_us = now;

    while (s_mp.play_idx < s_mp.smf.event_count &&
           s_mp.smf.events[s_mp.play_idx].time_us <= s_mp.play_time_us) {
        engine_midi_smf_event_t *ev = &s_mp.smf.events[s_mp.play_idx];
        if (ev->type == 0xE0) {
            midi_send_bend(ev->channel, (uint16_t)(ev->data1 | ((uint16_t)ev->data2 << 7)));
        } else {
            midi_send(ev->type, ev->channel, ev->data1, ev->data2);
        }
        s_mp.play_idx++;
    }

    if (s_mp.play_idx >= s_mp.smf.event_count) {
        /* 完播：置标志，由 on_update（task_app）消费改状态，防跨任务竞态 */
        s_mp.last_pump_us = now;
        __sync_synchronize();
        s_mp.play_finished_flag = true;
    }
}

static void midi_timer_start(void)
{
    if (s_mp.midi_timer != NULL || s_mp.is_music) {
        return;
    }
    service_timer_periodic_register(MIDI_TICK_US, midi_tick_hook, NULL, &s_mp.midi_timer);
    s_mp.last_pump_us = esp_timer_get_time();
}

static void midi_timer_stop(void)
{
    if (s_mp.midi_timer != NULL) {
        service_timer_unregister(s_mp.midi_timer);
        s_mp.midi_timer = NULL;
    }
}

/* -------------------- 状态机 -------------------- */

static const char *player_state_name(player_state_t state)
{
    switch (state) {
        case PLAYER_STATE_IDLE:     return "IDLE";
        case PLAYER_STATE_READY:    return "READY";
        case PLAYER_STATE_PLAYING:  return "PLAYING";
        case PLAYER_STATE_FINISHED: return "FINISHED";
        case PLAYER_STATE_SEEKING:  return "SEEKING";
        default:                    return "UNKNOWN";
    }
}

static void player_ui_set_icon(bool playing)
{
    lvgl_port_lock(portMAX_DELAY);
    if (s_midi_ui.play_stop_label != NULL) {
        lv_label_set_text(s_midi_ui.play_stop_label, playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    }
    lvgl_port_unlock();
}

/* MP3 播放期间隔离 SF2 主源并挂起 AI（TTS 同走 aux，避免生产者竞争） */
static void music_suspend_midi(void)
{
    if (s_mp.midi_suspended) {
        return;
    }
    service_audio_deactivate_sf2();
    service_xiaozhi_set_suspended(true);
    s_mp.midi_suspended = true;
    ESP_LOGI(TAG, "midi link suspended (mp3 playing)");
}

/* 停止 MP3 后恢复 SF2 主源与 AI */
static void music_restore_midi(void)
{
    if (!s_mp.midi_suspended) {
        return;
    }
    s_mp.midi_suspended = false;
    service_audio_activate_sf2();
    service_xiaozhi_set_suspended(false);
    ESP_LOGI(TAG, "midi link restored");
}

static void player_enter_state(player_state_t prev_state)
{
    switch (s_mp.state) {
        case PLAYER_STATE_IDLE:
        case PLAYER_STATE_READY:
        case PLAYER_STATE_FINISHED:
            player_ui_set_icon(false);
            break;
        case PLAYER_STATE_PLAYING:
            if (s_mp.is_music) {
                /* MP3 进入播放：启动解码（幂等），并隔离 MIDI 链路 */
                service_player_play();
                music_suspend_midi();
                midi_timer_stop();   /* 防残留 MIDI hook */
            } else {
                s_mp.last_pump_us = esp_timer_get_time();
                s_mp.play_finished_flag = false;
                midi_timer_start();   /* MIDI/录音时基挪入周期 hook（C6） */
            }
            player_ui_set_icon(true);
            break;
        case PLAYER_STATE_SEEKING:
            s_mp.state_before_seek = prev_state;
            break;
        default:
            break;
    }
    ESP_LOGD(TAG, "state %s -> %s", player_state_name(prev_state), player_state_name(s_mp.state));
}

static void player_leave_state(player_state_t prev_state)
{
    switch (prev_state) {
        case PLAYER_STATE_PLAYING:
            if (!s_mp.is_music) {
                /* 离开 MIDI/录音 播放态前强制静音 + 注销时基 hook，避免暂停/跳转/切出时
                 * 音符悬挂或 hook 继续推进解析 */
                midi_timer_stop();
                midi_all_notes_off();
            }
            break;
        default:
            break;
    }
}

static void player_set_state(player_state_t new_state)
{
    if (s_mp.state == new_state) {
        return;
    }
    player_state_t prev = s_mp.state;
    player_leave_state(prev);
    s_mp.state = new_state;
    player_enter_state(prev);
}

static void player_begin_seek(void)
{
    player_set_state(PLAYER_STATE_SEEKING);
}

static void player_end_seek(void)
{
    player_set_state(s_mp.state_before_seek);
}

/* -------------------- 播放抽象（SMF 统一：MIDI 文件与录音） -------------------- */

static int64_t player_total_us(void)
{
    return (int64_t)s_mp.smf.total_us;
}

static uint32_t player_event_count(void)
{
    return s_mp.smf.event_count;
}

static uint32_t player_find_index_at_time(int64_t target_us)
{
    uint32_t idx = 0;
    while (idx < s_mp.smf.event_count && (int64_t)s_mp.smf.events[idx].time_us <= target_us) {
        idx++;
    }
    return idx;
}

static void player_collect_state_at(uint32_t idx, uint8_t *last_pc, uint8_t *last_cc0, uint8_t *last_cc32)
{
    for (uint32_t i = 0; i < idx; i++) {
        const engine_midi_smf_event_t *ev = &s_mp.smf.events[i];
        if (ev->type == 0xC0) {
            last_pc[ev->channel] = ev->data1;
        } else if (ev->type == 0xB0) {
            if (ev->data1 == 0) last_cc0[ev->channel] = ev->data2;
            if (ev->data1 == 32) last_cc32[ev->channel] = ev->data2;
        }
    }
}

static void player_set_stopped_state(void)
{
    player_set_state((player_event_count() > 0) ? PLAYER_STATE_READY : PLAYER_STATE_IDLE);
}

static void player_stop(void)
{
    player_set_stopped_state();
}

/* -------------------- 文件扫描 -------------------- */

static bool midi_name_is_mid(const char *name)
{
    const char *dot = strrchr(name, '.');
    return dot != NULL && (strcasecmp(dot, ".mid") == 0 || strcasecmp(dot, ".midi") == 0);
}

static bool midi_name_is_mp3(const char *name)
{
    const char *dot = strrchr(name, '.');
    return dot != NULL && strcasecmp(dot, ".mp3") == 0;
}

static int midi_file_cmp_alpha(const void *a, const void *b)
{
    return strcasecmp(((const midi_file_item_t *)a)->name, ((const midi_file_item_t *)b)->name);
}

static int midi_file_cmp_mtime_desc(const void *a, const void *b)
{
    time_t ta = ((const midi_file_item_t *)a)->mtime;
    time_t tb = ((const midi_file_item_t *)b)->mtime;
    if (ta > tb) return -1;
    if (ta < tb) return 1;
    return strcasecmp(((const midi_file_item_t *)a)->name, ((const midi_file_item_t *)b)->name);
}

static void midi_scan_dir(const char *dir_path, midi_file_item_t *out, int *out_count,
                          bool accept_mid, bool accept_mp3)
{
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        return;
    }

    struct dirent *entry;
    while (*out_count < MIDI_MAX_FILES && (entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG) {
            continue;
        }
        bool ok = false;
        if (accept_mid && midi_name_is_mid(entry->d_name)) ok = true;
        if (accept_mp3 && midi_name_is_mp3(entry->d_name)) ok = true;
        if (!ok) {
            continue;
        }

        midi_file_item_t *it = &out[*out_count];
        size_t name_len = strnlen(entry->d_name, sizeof(it->name) - 1);
        memcpy(it->name, entry->d_name, name_len);
        it->name[name_len] = '\0';
        size_t dir_len = strlen(dir_path);
        memcpy(it->path, dir_path, dir_len);
        it->path[dir_len] = '/';
        memcpy(it->path + dir_len + 1, it->name, name_len + 1);

        struct stat st;
        it->mtime = (stat(it->path, &st) == 0) ? st.st_mtime : 0;

        (*out_count)++;
    }
    closedir(dir);
}

static void midi_scan_files(void)
{
    s_mp.music_file_count = 0;
    s_mp.midi_file_count = 0;
    s_mp.rec_file_count = 0;

    /* 音乐优先扫描 /sdcard/music，缺失则回退 /sdcard 根目录 */
    DIR *music_probe = opendir(MIDI_SCAN_DIR_MUSIC);
    bool music_dir_exists = (music_probe != NULL);
    if (music_probe != NULL) {
        closedir(music_probe);
    }
    if (music_dir_exists) {
        midi_scan_dir(MIDI_SCAN_DIR_MUSIC, s_mp.music_files, &s_mp.music_file_count,
                      false, true);
    }
    if (s_mp.music_file_count == 0) {
        midi_scan_dir(MIDI_SCAN_DIR_FALLBACK, s_mp.music_files, &s_mp.music_file_count,
                      false, true);
    }

    /* MIDI 优先扫描 /sdcard/midi，缺失则回退 /sdcard 根目录 */
    DIR *probe = opendir(MIDI_SCAN_DIR_MIDI);
    bool midi_dir_exists = (probe != NULL);
    if (probe != NULL) {
        closedir(probe);   /* 存在性探测后立即释放，防 fd 泄漏 */
    }
    if (midi_dir_exists) {
        midi_scan_dir(MIDI_SCAN_DIR_MIDI, s_mp.midi_files, &s_mp.midi_file_count,
                      true, false);
    }
    if (s_mp.midi_file_count == 0) {
        midi_scan_dir(MIDI_SCAN_DIR_FALLBACK, s_mp.midi_files, &s_mp.midi_file_count,
                      true, false);
    }

    /* 录音固定扫描 /sdcard/record（录音自 hmr 废弃后直录标准 .mid） */
    midi_scan_dir(MIDI_SCAN_DIR_RECORD, s_mp.rec_files, &s_mp.rec_file_count,
                  true, false);

    if (s_mp.music_file_count > 0) {
        qsort(s_mp.music_files, s_mp.music_file_count, sizeof(midi_file_item_t), midi_file_cmp_alpha);
    }
    if (s_mp.midi_file_count > 0) {
        qsort(s_mp.midi_files, s_mp.midi_file_count, sizeof(midi_file_item_t), midi_file_cmp_alpha);
    }
    if (s_mp.rec_file_count > 0) {
        qsort(s_mp.rec_files, s_mp.rec_file_count, sizeof(midi_file_item_t), midi_file_cmp_mtime_desc);
    }

    s_mp.scanned = true;
    ESP_LOGI(TAG, "scan music=%d midi=%d rec=%d",
             s_mp.music_file_count, s_mp.midi_file_count, s_mp.rec_file_count);
}

/* -------------------- UI 辅助 -------------------- */

static void midi_format_time(uint32_t us, char *out, size_t len)
{
    uint32_t sec = us / 1000000;
    snprintf(out, len, "%02lu:%02lu", (unsigned long)(sec / 60), (unsigned long)(sec % 60));
}

static void player_ui_refresh_progress(void)
{
    char buf[16];
    int64_t pos_us = 0;
    int64_t total_us = 0;

    if (s_mp.is_music) {
        pos_us = service_player_get_position_us();
        total_us = service_player_get_duration_us();
    } else {
        pos_us = s_mp.play_time_us;
        total_us = player_total_us();
    }

    int percent = (total_us > 0)
        ? (int)(pos_us * 100 / total_us) : 0;
    if (percent > 100) {
        percent = 100;
    }

    lvgl_port_lock(portMAX_DELAY);
    s_ui_progress_mask = true;
    if (s_midi_ui.progress != NULL) {
        lv_slider_set_value(s_midi_ui.progress, percent, LV_ANIM_OFF);
    }
    s_ui_progress_mask = false;
    midi_format_time((uint32_t)pos_us, buf, sizeof(buf));
    if (s_midi_ui.time_now != NULL) {
        lv_label_set_text(s_midi_ui.time_now, buf);
    }
    midi_format_time((uint32_t)total_us, buf, sizeof(buf));
    if (s_midi_ui.time_total != NULL) {
        lv_label_set_text(s_midi_ui.time_total, buf);
    }
    lvgl_port_unlock();
}

/* -------------------- 播放控制 -------------------- */

static void player_midi_save_state(void);
static void midi_switch_type(play_type_t new_type);

static void player_seek_percent(int percent)
{
    int64_t total = player_total_us();
    if (total <= 0 || player_event_count() == 0) {
        return;
    }
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    player_begin_seek();
    midi_all_notes_off();

    int64_t target = total * percent / 100;

    uint32_t idx = player_find_index_at_time(target);

    uint8_t last_pc[16], last_cc0[16], last_cc32[16];
    memset(last_pc, 0xFF, sizeof(last_pc));
    memset(last_cc0, 0xFF, sizeof(last_cc0));
    memset(last_cc32, 0xFF, sizeof(last_cc32));
    player_collect_state_at(idx, last_pc, last_cc0, last_cc32);
    for (int ch = 0; ch < 16; ch++) {
        if (last_cc0[ch] != 0xFF) midi_send(ENGINE_MIDI_MSG_CONTROL_CHANGE, (uint8_t)ch, 0, last_cc0[ch]);
        if (last_cc32[ch] != 0xFF) midi_send(ENGINE_MIDI_MSG_CONTROL_CHANGE, (uint8_t)ch, 32, last_cc32[ch]);
        if (last_pc[ch] != 0xFF) midi_send(ENGINE_MIDI_MSG_PROGRAM_CHANGE, (uint8_t)ch, last_pc[ch], 0);
    }

    s_mp.play_idx = idx;
    s_mp.play_time_us = target;
    s_mp.last_pump_us = esp_timer_get_time();
    player_ui_refresh_progress();

    player_end_seek();
}

/* 播放前准备：停止旧曲目，复位状态 */
static void player_prepare_load(void)
{
    player_stop();
    if (s_mp.is_music) {
        service_player_unload();
        music_restore_midi();
        s_mp.is_music = false;
    }
    engine_midi_smf_free(&s_mp.smf);
    player_set_state(PLAYER_STATE_IDLE);
    s_mp.play_time_us = 0;
    s_mp.play_idx = 0;
}

static void player_load_file(play_type_t type, int index)
{
    midi_file_item_t *files = NULL;
    int count = 0;
    int *cur_index = NULL;
    switch (type) {
    case PLAY_TYPE_MUSIC: files = s_mp.music_files; count = s_mp.music_file_count; cur_index = &s_mp.cur_music_index; break;
    case PLAY_TYPE_MIDI:  files = s_mp.midi_files;  count = s_mp.midi_file_count;  cur_index = &s_mp.cur_midi_index;  break;
    case PLAY_TYPE_REC:   files = s_mp.rec_files;   count = s_mp.rec_file_count;   cur_index = &s_mp.cur_rec_index;   break;
    default: return;
    }

    if (index < 0 || index >= count) {
        return;
    }

    player_prepare_load();

    char fallback[96];
    snprintf(fallback, sizeof(fallback), "%s", files[index].name);
    char *dot = strrchr(fallback, '.');
    if (dot != NULL) {
        *dot = '\0';
    }
    const char *title = fallback;

    if (type == PLAY_TYPE_MUSIC) {
        if (service_player_load(files[index].path) != ESP_OK) {
            app_manager_show_notification_timeout(_("音乐文件打开失败"), 2000);
            return;
        }
        s_mp.is_music = true;
        /* 真实歌名优先取 ID3v2 标题，缺失回退文件名 */
        const char *real_title = service_player_get_title();
        if (real_title != NULL && real_title[0] != '\0') {
            title = real_title;
        }
    } else if (type == PLAY_TYPE_REC) {
        /* 录音与 MIDI 同为标准 SMF，统一解析；录音保留独立错误文案 */
        if (engine_midi_smf_parse_file(files[index].path, &s_mp.smf) != ESP_OK) {
            app_manager_show_notification_timeout(_("录音文件解析失败"), 2000);
            return;
        }
    } else {
        if (engine_midi_smf_parse_file(files[index].path, &s_mp.smf) != ESP_OK) {
            app_manager_show_notification_timeout(_("MIDI 文件解析失败"), 2000);
            return;
        }
    }

    *cur_index = index;
    s_mp.play_type = type;

    char buf[192];
    lvgl_port_lock(portMAX_DELAY);
    if (s_midi_ui.name_label != NULL) {
        lv_label_set_text(s_midi_ui.name_label, title);
    }
    if (s_midi_ui.path_label != NULL) {
        snprintf(buf, sizeof(buf), "%s", files[index].path);
        lv_label_set_text(s_midi_ui.path_label, buf);
    }
    if (s_midi_ui.track_count != NULL) {
        if (s_mp.is_music) {
            lv_label_set_text(s_midi_ui.track_count, "--");
        } else {
            snprintf(buf, sizeof(buf), "%u", (unsigned)s_mp.smf.channels_used);
            lv_label_set_text(s_midi_ui.track_count, buf);
        }
    }
    if (s_midi_ui.bpm_num != NULL) {
        if (s_mp.is_music) {
            lv_label_set_text(s_midi_ui.bpm_num, "--");
        } else {
            snprintf(buf, sizeof(buf), "%u", (unsigned)s_mp.smf.bpm);
            lv_label_set_text(s_midi_ui.bpm_num, buf);
        }
    }
    lvgl_port_unlock();

    player_midi_save_state();

    player_set_state(PLAYER_STATE_READY);
    if (!s_mp.is_music) {
        player_seek_percent(0);
    }
}

static void player_load_and_play(play_type_t type, int index)
{
    player_load_file(type, index);
    if (s_mp.state == PLAYER_STATE_READY) {
        player_set_state(PLAYER_STATE_PLAYING);
    }
}

static void player_midi_save_state(void)
{
    const midi_file_item_t *files = NULL;
    int index = -1;
    switch (s_mp.play_type) {
    case PLAY_TYPE_MUSIC: files = s_mp.music_files; index = s_mp.cur_music_index; break;
    case PLAY_TYPE_MIDI:  files = s_mp.midi_files;  index = s_mp.cur_midi_index;  break;
    case PLAY_TYPE_REC:   files = s_mp.rec_files;   index = s_mp.cur_rec_index;   break;
    default: return;
    }

    service_nvs_midi_player_t state = {0};
    state.play_type = (uint8_t)s_mp.play_type;
    if (index >= 0 && index < ((s_mp.play_type == PLAY_TYPE_MUSIC) ? s_mp.music_file_count :
                              (s_mp.play_type == PLAY_TYPE_MIDI) ? s_mp.midi_file_count :
                              s_mp.rec_file_count)) {
        snprintf(state.filename, sizeof(state.filename), "%s", files[index].name);
    }

    esp_err_t ret = service_nvs_set_midi_player(&state);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "save midi player state failed: %d", ret);
    }
}

static int midi_find_index_by_name(const midi_file_item_t *files, int count, const char *name)
{
    for (int i = 0; i < count; i++) {
        if (strcmp(files[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void midi_restore_last_state(void)
{
    service_nvs_midi_player_t saved = {0};
    if (service_nvs_get_midi_player(&saved) != ESP_OK) {
        return;
    }
    if (saved.filename[0] == '\0') {
        return;
    }

    play_type_t type = (play_type_t)saved.play_type;
    if (type < PLAY_TYPE_MIDI || type > PLAY_TYPE_MUSIC) {
        type = PLAY_TYPE_MIDI;
    }

    const midi_file_item_t *files = NULL;
    int count = 0;
    switch (type) {
    case PLAY_TYPE_MUSIC: files = s_mp.music_files; count = s_mp.music_file_count; break;
    case PLAY_TYPE_MIDI:  files = s_mp.midi_files;  count = s_mp.midi_file_count;  break;
    case PLAY_TYPE_REC:   files = s_mp.rec_files;   count = s_mp.rec_file_count;   break;
    default: type = PLAY_TYPE_MIDI; files = s_mp.midi_files; count = s_mp.midi_file_count; break;
    }

    int idx = midi_find_index_by_name(files, count, saved.filename);
    if (idx < 0) {
        /* 保存的曲目已删除，回退到同类型第一首 */
        if (count > 0) {
            idx = 0;
        } else {
            /* 同类型为空，回退到 MIDI 类型 */
            type = PLAY_TYPE_MIDI;
            files = s_mp.midi_files;
            count = s_mp.midi_file_count;
            if (count > 0) {
                idx = 0;
            } else {
                return;
            }
        }
    }

    s_mp.play_type = type;
    player_load_file(type, idx);
    ESP_LOGI(TAG, "restore last: type=%d file=%s",
             (int)type, files[idx].name);
}

static int player_current_count(void)
{
    switch (s_mp.play_type) {
    case PLAY_TYPE_MUSIC: return s_mp.music_file_count;
    case PLAY_TYPE_MIDI:  return s_mp.midi_file_count;
    case PLAY_TYPE_REC:   return s_mp.rec_file_count;
    default: return 0;
    }
}

static int player_current_index(void)
{
    switch (s_mp.play_type) {
    case PLAY_TYPE_MUSIC: return s_mp.cur_music_index;
    case PLAY_TYPE_MIDI:  return s_mp.cur_midi_index;
    case PLAY_TYPE_REC:   return s_mp.cur_rec_index;
    default: return -1;
    }
}

static void player_play_toggle(void)
{
    /* 音乐播放无事件计数概念，走 service_player */
    if (s_mp.is_music) {
        if (!service_player_is_loaded()) {
            int count = player_current_count();
            if (count > 0) {
                int idx = (player_current_index() >= 0) ? player_current_index() : 0;
                player_load_and_play(s_mp.play_type, idx);
            } else {
                app_manager_show_notification_timeout(_("未找到音乐文件"), 0);
            }
            return;
        }

        switch (s_mp.state) {
        case PLAYER_STATE_PLAYING:
            service_player_pause();
            player_set_state(PLAYER_STATE_READY);
            break;
        case PLAYER_STATE_READY:
            player_set_state(PLAYER_STATE_PLAYING);   /* enter_state 内 service_player_play() */
            break;
        case PLAYER_STATE_FINISHED:
            /* 重播当前曲：先卸载再重新加载（load 要求未加载态），enter_state 内启动 */
            service_player_stop();
            service_player_unload();
            service_player_load(s_mp.music_files[s_mp.cur_music_index].path);
            player_set_state(PLAYER_STATE_PLAYING);
            break;
        case PLAYER_STATE_SEEKING:
            break;
        default:
            break;
        }
        return;
    }

    if (player_event_count() == 0) {
        int count = player_current_count();
        if (count > 0) {
            int idx = (player_current_index() >= 0) ? player_current_index() : 0;
            player_load_and_play(s_mp.play_type, idx);
        } else {
            const char *msg = (s_mp.play_type == PLAY_TYPE_REC)
                ? _("未找到录音文件") : _("未找到 MIDI 文件");
            app_manager_show_notification_timeout(msg, 0);
        }
        return;
    }

    switch (s_mp.state) {
        case PLAYER_STATE_PLAYING:
            player_set_state(PLAYER_STATE_READY);
            break;
        case PLAYER_STATE_READY:
            player_set_state(PLAYER_STATE_PLAYING);
            break;
        case PLAYER_STATE_FINISHED:
            player_seek_percent(0);
            player_set_state(PLAYER_STATE_PLAYING);
            break;
        case PLAYER_STATE_SEEKING:
            /* 跳转中不响应播放/暂停 */
            break;
        default:
            break;
    }
}

static void player_play_step(int delta)
{
    int count = player_current_count();
    if (count == 0) {
        return;
    }
    int next = player_current_index() + delta;
    if (next < 0 || next >= count) {
        app_manager_show_notification_timeout(delta < 0 ? _("已经是第一首") : _("已经是最后一首"), 2000);
        return;
    }
    player_load_and_play(s_mp.play_type, next);
}

/* -------------------- LVGL 事件请求环 -------------------- */

/* Trap: LVGL 事件回调跑在 task_gui，不得直接触碰 s_mp.smf/rec 与播放状态。
 * on_init/restore 的整文件解析（task_app，lifecycle 锁内，可达数秒）与点击
 * 回调并发，曾把 events 指针中途腾空（Core0 Store fault, MTVAL=0x4）。
 * 统一登记到 SPSC 请求环，由 on_update（task_app，lifecycle 锁内）串行消化。 */
typedef enum {
    MP_REQ_NONE = 0,
    MP_REQ_LOAD_PLAY,   /* player_load_and_play(type, index) */
    MP_REQ_PLAY_TOGGLE,
    MP_REQ_PLAY_STEP,   /* player_play_step(value=delta) */
    MP_REQ_SEEK,        /* player_seek_percent(value=percent) */
    MP_REQ_SWITCH_TYPE, /* 切换显示类型 */
    MP_REQ_DEL_ASK,     /* 长按条目：弹删除确认 */
    MP_REQ_DEL_CONFIRM, /* 确认删除 */
    MP_REQ_DEL_CANCEL,  /* 取消删除 */
} mp_req_action_t;

typedef struct {
    mp_req_action_t action;
    play_type_t type;
    int index;
    int value;
} mp_req_t;

static void app_midi_player_del_ask_cb(lv_event_t *e);
static void midi_del_ask(play_type_t type, int index);
static void midi_del_confirm(void);
static void midi_del_msgbox_hide(void);

#define MP_REQ_RING_CAP 8
static mp_req_t s_req_ring[MP_REQ_RING_CAP];
static volatile uint8_t s_req_head = 0;   /* 仅消费者 on_update（task_app）写 */
static volatile uint8_t s_req_tail = 0;   /* 仅生产者 LVGL 回调（task_gui）写 */

static void player_post_request(mp_req_action_t action, play_type_t type,
                                int index, int value)
{
    uint8_t tail = s_req_tail;
    uint8_t next = (uint8_t)((tail + 1) % MP_REQ_RING_CAP);
    if (next == s_req_head) {
        /* 满则丢弃本次（连点只丢一拍意图，优于并发踩内存） */
        ESP_LOGW(TAG, "request ring full, drop action=%d", (int)action);
        return;
    }
    s_req_ring[tail].action = action;
    s_req_ring[tail].type = type;
    s_req_ring[tail].index = index;
    s_req_ring[tail].value = value;
    __sync_synchronize();   /* 数据先行落笔，再发布尾指针（SPSC 无锁协议） */
    s_req_tail = next;
}

static void player_drain_requests(void)
{
    while (s_req_head != s_req_tail) {
        mp_req_t req = s_req_ring[s_req_head];
        __sync_synchronize();
        s_req_head = (uint8_t)((s_req_head + 1) % MP_REQ_RING_CAP);
        switch (req.action) {
        case MP_REQ_LOAD_PLAY:
            player_load_and_play(req.type, req.index);
            break;
        case MP_REQ_PLAY_TOGGLE:
            player_play_toggle();
            break;
        case MP_REQ_PLAY_STEP:
            player_play_step(req.value);
            break;
        case MP_REQ_SEEK:
            player_seek_percent(req.value);
            break;
        case MP_REQ_SWITCH_TYPE:
            midi_switch_type(req.type);
            break;
        case MP_REQ_DEL_ASK:
            midi_del_ask(req.type, req.index);
            break;
        case MP_REQ_DEL_CONFIRM:
            midi_del_confirm();
            break;
        case MP_REQ_DEL_CANCEL:
            midi_del_msgbox_hide();
            break;
        default:
            break;
        }
    }
}

/* -------------------- 三模式切换 -------------------- */

static void midi_update_panel_visibility(void)
{
    lvgl_port_lock(portMAX_DELAY);
    if (s_midi_ui.panel_music_list != NULL) {
        if (s_mp.play_type == PLAY_TYPE_MUSIC) {
            lv_obj_clear_flag(s_midi_ui.panel_music_list, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_midi_ui.panel_music_list, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (s_midi_ui.panel_mid_list != NULL) {
        if (s_mp.play_type == PLAY_TYPE_MIDI) {
            lv_obj_clear_flag(s_midi_ui.panel_mid_list, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_midi_ui.panel_mid_list, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (s_midi_ui.panel_record_list != NULL) {
        if (s_mp.play_type == PLAY_TYPE_REC) {
            lv_obj_clear_flag(s_midi_ui.panel_record_list, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_midi_ui.panel_record_list, LV_OBJ_FLAG_HIDDEN);
        }
    }
    lvgl_port_unlock();
}

/* 三个按钮 pressed 状态互斥，高亮当前模式 */
static void midi_update_mode_buttons(void)
{
    lvgl_port_lock(portMAX_DELAY);
    lv_obj_t *btns[] = {s_midi_ui.btn_music, s_midi_ui.btn_mid, s_midi_ui.btn_record};
    for (int i = 0; i < 3; i++) {
        if (btns[i] == NULL) {
            continue;
        }
        if ((s_mp.play_type == PLAY_TYPE_MUSIC && i == 0) ||
            (s_mp.play_type == PLAY_TYPE_MIDI && i == 1) ||
            (s_mp.play_type == PLAY_TYPE_REC && i == 2)) {
            lv_obj_add_state(btns[i], LV_STATE_PRESSED);
        } else {
            lv_obj_clear_state(btns[i], LV_STATE_PRESSED);
        }
    }
    lvgl_port_unlock();
}

static void midi_switch_type(play_type_t new_type)
{
    if (new_type == s_mp.play_type) {
        /* 同类型点击：仍重同步 UI（restore 等路径可能已改 play_type 但面板未同步） */
        midi_update_panel_visibility();
        midi_update_mode_buttons();
        return;
    }

    /* 切走时若正在播放音乐，停止解码并恢复 MIDI 链路 */
    if (s_mp.is_music && service_player_is_loaded()) {
        service_player_stop();
        service_player_unload();
        music_restore_midi();
        s_mp.is_music = false;
        player_set_state(PLAYER_STATE_IDLE);
    }

    s_mp.play_type = new_type;
    midi_update_panel_visibility();
    midi_update_mode_buttons();
    player_midi_save_state();
    ESP_LOGI(TAG, "play_type=%d", (int)new_type);
}

/* -------------------- 事件回调 -------------------- */

static void app_midi_player_home_cb(lv_event_t *e)
{
    (void)e;
    app_manager_request_kill_active();
}

static void app_midi_player_music_cb(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    player_post_request(MP_REQ_LOAD_PLAY, PLAY_TYPE_MUSIC, index, 0);
}

static void app_midi_player_file_cb(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    player_post_request(MP_REQ_LOAD_PLAY, PLAY_TYPE_MIDI, index, 0);
}

static void app_midi_player_rec_cb(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    player_post_request(MP_REQ_LOAD_PLAY, PLAY_TYPE_REC, index, 0);
}

static void app_midi_player_control_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target_obj(e);
    if (target == s_midi_ui.prev) {
        player_post_request(MP_REQ_PLAY_STEP, s_mp.play_type, 0, -1);
    } else if (target == s_midi_ui.next) {
        player_post_request(MP_REQ_PLAY_STEP, s_mp.play_type, 0, 1);
    } else if (target == s_midi_ui.play_stop) {
        player_post_request(MP_REQ_PLAY_TOGGLE, s_mp.play_type, 0, 0);
    }
}

static void app_midi_player_mode_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target_obj(e);
    play_type_t new_type;
    if (target == s_midi_ui.btn_music) {
        new_type = PLAY_TYPE_MUSIC;
    } else if (target == s_midi_ui.btn_mid) {
        new_type = PLAY_TYPE_MIDI;
    } else {
        new_type = PLAY_TYPE_REC;
    }
    player_post_request(MP_REQ_SWITCH_TYPE, new_type, 0, 0);
}

static void app_midi_player_seek_cb(lv_event_t *e)
{
    if (s_ui_progress_mask || s_mp.state == PLAYER_STATE_SEEKING ||
        lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    /* MP3 暂不支持跳转，忽略滑块调整 */
    if (s_mp.is_music) {
        return;
    }
    player_post_request(MP_REQ_SEEK, s_mp.play_type, 0,
                        lv_slider_get_value(s_midi_ui.progress));
}

static void app_midi_player_set_open_cb(lv_event_t *e)
{
    (void)e;
    lvgl_port_lock(portMAX_DELAY);
    if (s_midi_ui.panel_set != NULL) {
        lv_obj_clear_flag(s_midi_ui.panel_set, LV_OBJ_FLAG_HIDDEN);
        /* 设置面板吸收点击，防止穿透触发主界面控件 */
        lv_obj_add_flag(s_midi_ui.panel_set, LV_OBJ_FLAG_CLICKABLE);
    }
    lvgl_port_unlock();
}

static void app_midi_player_set_close_cb(lv_event_t *e)
{
    (void)e;
    lvgl_port_lock(portMAX_DELAY);
    if (s_midi_ui.panel_set != NULL) {
        lv_obj_add_flag(s_midi_ui.panel_set, LV_OBJ_FLAG_HIDDEN);
    }
    lvgl_port_unlock();
}

/* -------------------- 文件清单 -------------------- */

static void midi_populate_list(lv_obj_t *list)
{
    if (list == NULL) {
        return;
    }

    lvgl_port_lock(portMAX_DELAY);

    lv_color_t btn_bg = engine_gui_theme_color(COLOR_CARD);
    lv_color_t btn_text = engine_gui_theme_color(COLOR_TEXT_PRIMARY);
    int32_t btn_shadow = 0;
    if (s_midi_ui.file_example != NULL) {
        btn_bg = lv_obj_get_style_bg_color(s_midi_ui.file_example, LV_PART_MAIN | LV_STATE_DEFAULT);
        btn_text = lv_obj_get_style_text_color(s_midi_ui.file_example, LV_PART_MAIN | LV_STATE_DEFAULT);
        btn_shadow = lv_obj_get_style_shadow_width(s_midi_ui.file_example, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    /* Trap: file_example 是 EEZ 隐藏样式模板，也是 MIDI 列表的子控件；lv_obj_clean 会
     * 连它一起删，后续读其样式即 UAF，改选择性删除保留模板。其余列表无模板可直接清空。 */
    if (list == s_midi_ui.list_mid && s_midi_ui.file_example != NULL) {
        uint32_t child_cnt = lv_obj_get_child_count(list);
        for (int32_t i = (int32_t)child_cnt - 1; i >= 0; i--) {
            lv_obj_t *ch = lv_obj_get_child(list, i);
            if (ch != NULL && ch != s_midi_ui.file_example) {
                lv_obj_delete(ch);
            }
        }
    } else {
        lv_obj_clean(list);
    }

    midi_file_item_t *files = NULL;
    int count = 0;
    void (*cb)(lv_event_t *) = NULL;
    if (list == s_midi_ui.list_music) {
        files = s_mp.music_files;
        count = s_mp.music_file_count;
        cb = app_midi_player_music_cb;
    } else if (list == s_midi_ui.list_mid) {
        files = s_mp.midi_files;
        count = s_mp.midi_file_count;
        cb = app_midi_player_file_cb;
    } else {
        files = s_mp.rec_files;
        count = s_mp.rec_file_count;
        cb = app_midi_player_rec_cb;
    }

    for (int i = 0; i < count; i++) {
        lv_obj_t *btn = lv_button_create(list);
        lv_obj_set_size(btn, LV_PCT(100), 50);
        lv_obj_set_style_bg_color(btn, btn_bg, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(btn, btn_text, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_width(btn, btn_shadow, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *label = lv_label_create(btn);
        lv_obj_set_size(label, LV_PCT(99), 34);
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        lv_obj_set_style_align(label, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text_fmt(label, MIDI_FILE_ICON "%s", files[i].name);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        /* .mid 类列表（MIDI/录音）长按弹删除确认；type 编码进高 16 位 */
        if (list == s_midi_ui.list_mid || list == s_midi_ui.list_rec) {
            play_type_t del_type = (list == s_midi_ui.list_mid) ? PLAY_TYPE_MIDI : PLAY_TYPE_REC;
            lv_obj_add_event_cb(btn, app_midi_player_del_ask_cb, LV_EVENT_LONG_PRESSED,
                                (void *)(intptr_t)(i | ((int)del_type << 16)));
        }
    }
    lvgl_port_unlock();
}

static void midi_populate_all_lists(void)
{
    midi_populate_list(s_midi_ui.list_music);
    midi_populate_list(s_midi_ui.list_mid);
    midi_populate_list(s_midi_ui.list_rec);
}

/* -------------------- 长按删除确认（midi_del_msgbox） -------------------- */

static struct {
    play_type_t type;
    int index;
} s_del = { PLAY_TYPE_MIDI, -1 };

static void app_midi_player_del_ok_cb(lv_event_t *e)
{
    (void)e;
    player_post_request(MP_REQ_DEL_CONFIRM, PLAY_TYPE_MIDI, 0, 0);
}

static void app_midi_player_del_cancel_cb(lv_event_t *e)
{
    (void)e;
    player_post_request(MP_REQ_DEL_CANCEL, PLAY_TYPE_MIDI, 0, 0);
}

static void app_midi_player_del_ask_cb(lv_event_t *e)
{
    int v = (int)(intptr_t)lv_event_get_user_data(e);
    player_post_request(MP_REQ_DEL_ASK, (play_type_t)((v >> 16) & 0xFF),
                        v & 0xFFFF, 0);
}

static void midi_del_msgbox_hide(void)
{
    lvgl_port_lock(portMAX_DELAY);
    if (s_midi_ui.del_msgbox != NULL) {
        lv_obj_add_flag(s_midi_ui.del_msgbox, LV_OBJ_FLAG_HIDDEN);
    }
    lvgl_port_unlock();
    s_del.index = -1;
}

static void midi_del_ask(play_type_t type, int index)
{
    if (type != PLAY_TYPE_MIDI && type != PLAY_TYPE_REC) {
        return;  /* 仅 .mid 类文件开放删除 */
    }
    midi_file_item_t *files = (type == PLAY_TYPE_MIDI) ? s_mp.midi_files : s_mp.rec_files;
    int count = (type == PLAY_TYPE_MIDI) ? s_mp.midi_file_count : s_mp.rec_file_count;
    if (index < 0 || index >= count || s_midi_ui.del_msgbox == NULL) {
        return;
    }
    s_del.type = type;
    s_del.index = index;

    lvgl_port_lock(portMAX_DELAY);
    lv_obj_t *box = s_midi_ui.del_msgbox;
    /* 重建内容：msgbox add_* 为追加式，重弹前清 content/footer。
     * Trap: footer 惰性创建（首个 add_footer_button 才建），新弹窗
     * get_footer==NULL，lv_obj_clean(NULL) 触发 LV_ASSERT_NULL 死循环
     * （LV_ASSERT_HANDLER=while(1)，真机 WDT 重启）——必须判空 */
    lv_obj_clean(lv_msgbox_get_content(box));
    lv_obj_t *footer = lv_msgbox_get_footer(box);
    if (footer != NULL) {
        lv_obj_clean(footer);
    }
    /* EEZ 固定高 256 装不下 100px 按钮的 footer（底部截断），高度改按内容自适应 */
    lv_obj_set_height(box, LV_SIZE_CONTENT);

    lv_obj_t *name_lbl = lv_msgbox_add_text(box, files[index].name);
    lv_obj_set_style_text_align(name_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_t *ask_lbl = lv_msgbox_add_text(box, _("确认删除该文件？"));
    lv_obj_set_style_text_align(ask_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    /* EEZ 未暴露按钮配色/尺寸/圆角接口，显式按主题上色并固定 100x60(宽x高)、圆角 20
     *（项目按钮惯例值，msgbox 内部按钮不吃 EEZ 逐控件样式）：OK=删除（错误色），LEFT=返回 */
    lv_obj_t *btn_ok = lv_msgbox_add_footer_button(box, LV_SYMBOL_OK);
    lv_obj_set_size(btn_ok, 100, 60);
    lv_obj_set_style_radius(btn_ok, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn_ok, engine_gui_theme_color(COLOR_ERROR),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(btn_ok, engine_gui_theme_color(COLOR_TEXT_PRIMARY),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn_ok, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(btn_ok, app_midi_player_del_ok_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_back = lv_msgbox_add_footer_button(box, LV_SYMBOL_LEFT);
    lv_obj_set_size(btn_back, 100, 60);
    lv_obj_set_style_radius(btn_back, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn_back, engine_gui_theme_color(COLOR_PRIMARY),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(btn_back, engine_gui_theme_color(COLOR_TEXT_PRIMARY),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn_back, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(btn_back, app_midi_player_del_cancel_cb, LV_EVENT_CLICKED, NULL);

    /* Trap: footer class 默认高度固定 LV_DPI_DEF/3(~43px)，100px 按钮被上下裁切
     *（圆角恰在裁剪区外，可见部分就是直角矩形横带）；footer 高度改内容自适应 */
    footer = lv_msgbox_get_footer(box);
    if (footer != NULL) {
        lv_obj_set_height(footer, LV_SIZE_CONTENT);
    }

    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);  /* 吸收点击防穿透列表 */
    lv_obj_clear_flag(box, LV_OBJ_FLAG_HIDDEN);
    lvgl_port_unlock();
}

static void midi_del_confirm(void)
{
    play_type_t type = s_del.type;
    int index = s_del.index;
    midi_del_msgbox_hide();
    if (type != PLAY_TYPE_MIDI && type != PLAY_TYPE_REC) {
        return;
    }
    midi_file_item_t *files = (type == PLAY_TYPE_MIDI) ? s_mp.midi_files : s_mp.rec_files;
    int count = (type == PLAY_TYPE_MIDI) ? s_mp.midi_file_count : s_mp.rec_file_count;
    int cur = (type == PLAY_TYPE_MIDI) ? s_mp.cur_midi_index : s_mp.cur_rec_index;
    if (index < 0 || index >= count) {
        return;
    }

    char name[96];
    snprintf(name, sizeof(name), "%s", files[index].name);

    /* 删除当前加载曲目：先卸载释放 SMF/复位状态机（FATFS 下删打开文件有风险） */
    if (s_mp.play_type == type && cur == index && s_mp.state != PLAYER_STATE_IDLE) {
        player_prepare_load();
    }

    if (remove(files[index].path) == 0) {
        app_manager_show_notificationf_timeout(2000, _("Deleted: %s"), name);
    } else {
        app_manager_show_notification_timeout(_("Delete failed"), 2000);
    }
    midi_scan_files();
    midi_populate_all_lists();
    /* 被删曲目可能正是 NVS 保存的曲目：重写一次状态（restore 侧本就有回退） */
    player_midi_save_state();
}

/* -------------------- 生命周期 -------------------- */

static bool app_midi_player_on_init(app_base_t *self, void *screen_ctx)
{
    (void)self;
    ESP_LOGI(TAG, "init");

    /* 清空上次会话遗留的请求：此时控件回调尚未注册（on_destroy 已移除），
     * 无生产者，直接复位指针 */
    s_req_head = 0;
    s_req_tail = 0;
    s_del.index = -1;

    /* 每次进入重新扫描：录音随时可能在其他 App 产生，缓存扫描结果会导致
     * 新录音不显示（目录列表开销小，仅遍历文件名）；当前曲目索引保留 */
    s_mp.cur_music_index = -1;
    s_mp.cur_midi_index = -1;
    s_mp.cur_rec_index = -1;
    s_mp.play_idx = 0;
    s_mp.play_time_us = 0;
    s_mp.last_ui_ms = 0;
    s_mp.is_music = false;
    s_mp.play_finished_flag = false;
    player_set_state(PLAYER_STATE_IDLE);

    midi_scan_files();
    midi_populate_all_lists();
    /* restore 会把 play_type 改为上次会话类型（可能非 MIDI），必须在其后
     * 同步面板显隐与按钮高亮，否则 UI 停在旧类型、点当前类型按钮不响应 */
    midi_restore_last_state();
    midi_update_panel_visibility();
    midi_update_mode_buttons();

    lvgl_port_lock(portMAX_DELAY);

    if (s_midi_ui.btn_home != NULL) {
        lv_obj_add_event_cb(s_midi_ui.btn_home, app_midi_player_home_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_midi_ui.btn_set != NULL) {
        lv_obj_add_event_cb(s_midi_ui.btn_set, app_midi_player_set_open_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_midi_ui.set_btn_return != NULL) {
        lv_obj_add_event_cb(s_midi_ui.set_btn_return, app_midi_player_set_close_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_midi_ui.btn_music != NULL) {
        lv_obj_add_event_cb(s_midi_ui.btn_music, app_midi_player_mode_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_midi_ui.btn_mid != NULL) {
        lv_obj_add_event_cb(s_midi_ui.btn_mid, app_midi_player_mode_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_midi_ui.btn_record != NULL) {
        lv_obj_add_event_cb(s_midi_ui.btn_record, app_midi_player_mode_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_midi_ui.prev != NULL) {
        lv_obj_add_event_cb(s_midi_ui.prev, app_midi_player_control_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_midi_ui.play_stop != NULL) {
        lv_obj_add_event_cb(s_midi_ui.play_stop, app_midi_player_control_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_midi_ui.next != NULL) {
        lv_obj_add_event_cb(s_midi_ui.next, app_midi_player_control_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_midi_ui.progress != NULL) {
        lv_obj_add_event_cb(s_midi_ui.progress, app_midi_player_seek_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
    lvgl_port_unlock();

    player_ui_set_icon(false);
    player_ui_refresh_progress();

    return true;
}

static void app_midi_player_on_update(app_base_t *self)
{
    (void)self;

    /* 先消化 LVGL 事件请求（task_app 串行执行解析/控制，与生命周期同锁） */
    player_drain_requests();

    if (s_mp.state != PLAYER_STATE_PLAYING) {
        return;
    }

    if (s_mp.is_music) {
        /* MP3：推进解码，完播后自动切下一首（连续播放，循环到首） */
        service_player_poll();
        if (!service_player_is_playing()) {
            if (s_mp.music_file_count > 0) {
                int next = s_mp.cur_music_index + 1;
                if (next >= s_mp.music_file_count) {
                    next = 0;
                }
                /* 不调 player_load_file：其 prepare_load 会恢复 MIDI 链路，
                 * 连续播放应保持 MIDI 隔离，直接卸载旧曲、加载下一首继续播 */
                service_player_stop();
                service_player_unload();
                service_player_load(s_mp.music_files[next].path);
                s_mp.cur_music_index = next;
                s_mp.play_type = PLAY_TYPE_MUSIC;
                player_set_state(PLAYER_STATE_PLAYING);   /* enter_state 内 play + suspend（已挂起，幂等） */
                player_midi_save_state();

                const char *real_title = service_player_get_title();
                const char *title = (real_title != NULL && real_title[0] != '\0')
                    ? real_title : s_mp.music_files[next].name;
                lvgl_port_lock(portMAX_DELAY);
                if (s_midi_ui.name_label != NULL) {
                    lv_label_set_text(s_midi_ui.name_label, title);
                }
                if (s_midi_ui.path_label != NULL) {
                    lv_label_set_text(s_midi_ui.path_label, s_mp.music_files[next].path);
                }
                lvgl_port_unlock();
                ESP_LOGI(TAG, "music auto-next -> %s", s_mp.music_files[next].name);
            } else {
                player_set_state(PLAYER_STATE_FINISHED);
                music_restore_midi();
            }
        }
        player_ui_refresh_progress();
        return;
    }

    int64_t now = esp_timer_get_time();

    /* 完播标志由时基 hook 置位，此处（task_app）消费改状态，防跨任务竞态 */
    if (s_mp.play_finished_flag) {
        s_mp.play_finished_flag = false;
        player_set_state(PLAYER_STATE_FINISHED);
        ESP_LOGI(TAG, "playback finished");
        return;
    }

    /* MIDI 事件派发已移入 service_timer hook；此处仅刷新进度 UI */
    uint32_t now_ms = (uint32_t)(now / 1000);
    if (now_ms - s_mp.last_ui_ms >= MIDI_UI_REFRESH_MS) {
        s_mp.last_ui_ms = now_ms;
        player_ui_refresh_progress();
    }
}

static void app_midi_player_on_pause(app_base_t *self)
{
    (void)self;
    /* 切出即暂停：MP3 停止解码并恢复 MIDI 链路，MIDI/录音静音但保留进度 */
    if (s_mp.is_music && service_player_is_loaded()) {
        service_player_stop();
        music_restore_midi();
        player_set_state(PLAYER_STATE_READY);
    } else {
        player_stop();
    }
    ESP_LOGI(TAG, "pause");
}

static void app_midi_player_on_resume(app_base_t *self)
{
    (void)self;
    player_ui_refresh_progress();
    ESP_LOGI(TAG, "resume");
}

static void app_midi_player_on_destroy(app_base_t *self)
{
    (void)self;
    midi_timer_stop();   /* 注销时基 hook：防 destroy 后 hook 触碰已释放 smf */
    player_set_state(PLAYER_STATE_IDLE);
    midi_all_notes_off();

    /* 若 MP3 正在加载/播放，停止并恢复 MIDI 链路 */
    if (s_mp.is_music && service_player_is_loaded()) {
        service_player_stop();
        service_player_unload();
        music_restore_midi();
        s_mp.is_music = false;
    }

    /* 移除 on_init 注册的事件回调：EEZ 屏幕对象持久存在，
     * 不移除会在再次进入时重复注册导致一次事件多次触发 */
    lvgl_port_lock(portMAX_DELAY);
    if (s_midi_ui.btn_home != NULL) {
        lv_obj_remove_event_cb(s_midi_ui.btn_home, app_midi_player_home_cb);
    }
    if (s_midi_ui.btn_set != NULL) {
        lv_obj_remove_event_cb(s_midi_ui.btn_set, app_midi_player_set_open_cb);
    }
    if (s_midi_ui.set_btn_return != NULL) {
        lv_obj_remove_event_cb(s_midi_ui.set_btn_return, app_midi_player_set_close_cb);
    }
    if (s_midi_ui.btn_music != NULL) {
        lv_obj_remove_event_cb(s_midi_ui.btn_music, app_midi_player_mode_cb);
    }
    if (s_midi_ui.btn_mid != NULL) {
        lv_obj_remove_event_cb(s_midi_ui.btn_mid, app_midi_player_mode_cb);
    }
    if (s_midi_ui.btn_record != NULL) {
        lv_obj_remove_event_cb(s_midi_ui.btn_record, app_midi_player_mode_cb);
    }
    if (s_midi_ui.prev != NULL) {
        lv_obj_remove_event_cb(s_midi_ui.prev, app_midi_player_control_cb);
    }
    if (s_midi_ui.play_stop != NULL) {
        lv_obj_remove_event_cb(s_midi_ui.play_stop, app_midi_player_control_cb);
    }
    if (s_midi_ui.next != NULL) {
        lv_obj_remove_event_cb(s_midi_ui.next, app_midi_player_control_cb);
    }
    if (s_midi_ui.progress != NULL) {
        lv_obj_remove_event_cb(s_midi_ui.progress, app_midi_player_seek_cb);
    }
    lvgl_port_unlock();

    /* 事件缓冲随曲目释放；扫描缓存按设计保留到下次开机 */
    engine_midi_smf_free(&s_mp.smf);
    s_mp.cur_music_index = -1;
    s_mp.cur_midi_index = -1;
    s_mp.cur_rec_index = -1;

    ESP_LOGI(TAG, "destroy");
}

esp_err_t app_midi_player_register(void)
{
    static app_base_t app = {
        .name = "MIDI Player",
        .screen_name = "app_midi_player",
        .screen_ctx = &s_midi_ui,
        .screen_ctx_size = sizeof(s_midi_ui),
        .widget_bindings = s_midi_bindings,
        .on_init = app_midi_player_on_init,
        .on_update = app_midi_player_on_update,
        .on_pause = app_midi_player_on_pause,
        .on_resume = app_midi_player_on_resume,
        .on_destroy = app_midi_player_on_destroy,
    };
    return app_manager_register(&app);
}