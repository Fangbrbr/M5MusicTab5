/**
 * @file app_chord_trainer.c
 * @brief 和弦练习 App：根音 × 和弦类型的构成音可视化 + 内嵌五度圈 panel
 *
 * 根音由 12 键矩阵单选（CHECKED 互斥），和弦类型由点选按钮选择；
 * chord_name 显示组合和弦名，chord_definition 显示音程堆叠定义；
 * canvas 绘制两个八度琴键并标记和弦内音（根音着重），纯展示不发声。
 * 配色全部取自 EEZ 主题数组：白键 bg_secondary(1)，黑键 shadow(16)。
 *
 * chord_btn_circle 打开 chord_panel_fifth（五度圈）：左圈 canvas 点选调号、
 * 右侧信息面板显示调性、小卷帘点按试听音阶。Why 缓冲共享：panel 打开时
 * 释放和弦卷帘 buffer 供五度圈两个 canvas 复用，关闭后重建重绘，
 * PSRAM 占用不叠加。
 */

#include "app_chord_trainer.h"
#include "app_manager.h"
#include "engine_gui.h"
#include "fonts.h"
#include "engine_midi.h"
#include "service_i18n.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lvgl_port.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

static const char *TAG = "app_chord_trainer";

/* 画布尺寸运行期实测控件获得（EEZ: pos 32,390，size 95%×300），
 * 布局收敛后才分配 buffer，不再硬编码（EEZ 改版漂移曾致 buffer 与控件不符） */

#define CHORD_WHITE_COUNT   14
#define CHORD_BLACK_COUNT   10
#define CHORD_DOT_RADIUS    16

/* 主题配色槽位（语义见 EEZ 主题定义） */
#define CHORD_C_BG          COLOR_BG_PRIMARY
#define CHORD_C_WHITE_KEY   COLOR_BG_SECONDARY
#define CHORD_C_TEXT        COLOR_TEXT_PRIMARY
#define CHORD_C_DIM         COLOR_TEXT_SECONDARY
#define CHORD_C_TONE        COLOR_SUCCESS
#define CHORD_C_ROOT        COLOR_ERROR
#define CHORD_C_BLACK_KEY   COLOR_SHADOW

typedef struct {
    const char *cn_name;      /* 中文名称，如 "大小七" */
    const char *suffix;       /* 和弦符号，如 "m7b5" */
    const int8_t *tones;      /* 相对根音的半音数，含根音 0 */
    uint8_t tone_count;
    const char *definition;   /* 音程堆叠定义文本 */
} chord_type_t;

/* 和弦类型按钮管理结构体 */
typedef struct {
    lv_obj_t *btn;
    uint8_t type_index; /* 对应 s_chord_types[] 索引 */
} chord_type_btn_t;

/* 15 种和弦类型与按钮索引顺序严格对齐 */
static const int8_t s_tones_maj[]    = {0, 4, 7};
static const int8_t s_tones_min[]    = {0, 3, 7};
static const int8_t s_tones_aug[]    = {0, 4, 8};
static const int8_t s_tones_dim[]    = {0, 3, 6};
static const int8_t s_tones_sus2[]   = {0, 2, 7};
static const int8_t s_tones_sus4[]   = {0, 5, 7};
static const int8_t s_tones_maj7[]   = {0, 4, 7, 11};
static const int8_t s_tones_7[]      = {0, 4, 7, 10};
static const int8_t s_tones_m7[]     = {0, 3, 7, 10};
static const int8_t s_tones_m7b5[]   = {0, 3, 6, 10};
static const int8_t s_tones_dim7[]   = {0, 3, 6, 9};
static const int8_t s_tones_7sus2[]  = {0, 2, 7, 10};
static const int8_t s_tones_7sus4[]  = {0, 5, 7, 10};
static const int8_t s_tones_add9[]   = {0, 4, 7, 14};
static const int8_t s_tones_9[]      = {0, 4, 7, 10, 14};

static const chord_type_t s_chord_types[] = {
    { "大三",   "",      s_tones_maj,   3, "大三度+小三度" },
    { "小三",   "m",     s_tones_min,   3, "小三度+大三度" },
    { "增三",   "aug",   s_tones_aug,   3, "大三度+大三度" },
    { "减三",   "dim",   s_tones_dim,   3, "小三度+小三度" },
    { "挂二",   "sus2",  s_tones_sus2,  3, "大二度+纯四度" },
    { "挂四",   "sus4",  s_tones_sus4,  3, "纯四度+大二度" },
    { "大七",   "maj7",  s_tones_maj7,  4, "大三度+小三度+大三度" },
    { "大小七", "7",     s_tones_7,     4, "大三度+小三度+小三度" },
    { "小七",   "m7",    s_tones_m7,    4, "小三度+大三度+小三度" },
    { "半减七", "m7b5",  s_tones_m7b5,  4, "小三度+小三度+大三度" },
    { "减七",   "dim7",  s_tones_dim7,  4, "小三度+小三度+小三度" },
    { "挂二七", "7sus2", s_tones_7sus2, 4, "大二度+纯四度+小三度" },
    { "挂四七", "7sus4", s_tones_7sus4, 4, "纯四度+大二度+小三度" },
    { "加九",   "add9",  s_tones_add9,  4, "大三度+小三度+大二度" },
    { "九和弦", "9",     s_tones_9,     5, "大三度+小三度+小三度+大二度" },
};
#define CHORD_TYPE_COUNT  (sizeof(s_chord_types) / sizeof(s_chord_types[0]))

/* 和弦类型按钮数组：索引与 s_chord_types[] 严格对齐 */
static chord_type_btn_t s_chord_type_buttons[CHORD_TYPE_COUNT] = {0};

/* 与 chord_key_key 按键矩阵顺序一致的根音名 */
static const char * const s_root_names[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"
};

/* 白键/黑键的音名映射（单八度内） */
static const int8_t s_white_pc[7] = {0, 2, 4, 5, 7, 9, 11};
static const int8_t s_black_pc[5] = {1, 3, 6, 8, 10};
/* 黑键在八度内的位置：紧跟其左侧白键的索引（C D F G A） */
static const int8_t s_black_pos[5] = {0, 1, 3, 4, 5};

#define CHORD_PLAY_MAX_NOTES  8
#define CHORD_BLOCK_DUR_MS    800
#define CHORD_ARPEGGIO_STEP_MS 200

typedef struct {
    uint8_t note;
    int64_t on_at_us;
    int64_t off_at_us;
    bool started;
} chord_note_slot_t;

typedef struct {
    uint8_t root;      /* 0~11，0=C */
    uint8_t type;      /* 0~CHORD_TYPE_COUNT-1 */
    bool arpeggio;     /* 点按播放模式：false 柱式 true 分解，每次点按轮换 */
    chord_note_slot_t notes[CHORD_PLAY_MAX_NOTES];
    int note_count;
    void *canvas_buf;
    int canvas_w;
    int canvas_h;
} chord_state_t;

typedef struct {
    lv_obj_t *canvas;
    lv_obj_t *chord_name;
    lv_obj_t *chord_definition;
    lv_obj_t *chord_key_key;
    lv_obj_t *chord_panel_type_poll; /* 和弦类型 Panel 父容器 */
    lv_obj_t *btn_home;
    lv_obj_t *btn_circle;            /* 打开五度圈 panel */
    lv_obj_t *panel_fifth;           /* 五度圈 panel 容器 */
    lv_obj_t *fifth_btn_return;
    lv_obj_t *fifth_canvas_circle;
    lv_obj_t *fifth_name;
    lv_obj_t *fifth_key_sig;
    lv_obj_t *fifth_scale;
    lv_obj_t *fifth_canvas_piano;
    lv_obj_t *fifth_dominant;
    lv_obj_t *fifth_parallel;
    lv_obj_t *fifth_subdominant;
} ui_screen_chord_t;

static ui_screen_chord_t s_chord_ui = {0};

static const widget_binding_t s_chord_bindings[] = {
    WIDGET_BIND(ui_screen_chord_t, canvas,           "chord_canvas_piano", WIDGET_KIND_CANVAS),
    WIDGET_BIND(ui_screen_chord_t, chord_name,       "chord_name",         WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_chord_t, chord_definition, "chord_definition",   WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_chord_t, chord_key_key,    "chord_key_key",      WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_chord_t, chord_panel_type_poll, "chord_panel_type_poll", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_chord_t, btn_home,         "chord_btn_home",     WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_chord_t, btn_circle,       "chord_btn_circle",   WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_chord_t, panel_fifth,      "chord_panel_fifth",  WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_chord_t, fifth_btn_return, "fifth_btn_return",   WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_chord_t, fifth_canvas_circle, "fifth_canvas_circle_1", WIDGET_KIND_CANVAS),
    WIDGET_BIND(ui_screen_chord_t, fifth_name,       "fifth_name_1",       WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_chord_t, fifth_key_sig,    "fifth_key_sig_1",    WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_chord_t, fifth_scale,      "fifth_scale_1",      WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_chord_t, fifth_canvas_piano,  "fifth_canvas_piano_1",  WIDGET_KIND_CANVAS),
    WIDGET_BIND(ui_screen_chord_t, fifth_dominant,   "fifth_dominant_1",   WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_chord_t, fifth_parallel,   "fifth_parallel_1",   WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_chord_t, fifth_subdominant, "fifth_subdominant_1", WIDGET_KIND_LABEL),
    WIDGET_BINDING_END,
};

static chord_state_t s_chord = {0};

/* -------------------- 五度圈数据（移植自原独立 app_circle_of_fifths） -------------------- */

#define FIFTH_PLAY_DELAY_MS   100
#define FIFTH_SCALE_NOTES     8

typedef struct {
    const char *name;      /* 调名，如 "C"、"Bb" */
    uint8_t root_pc;       /* 主音音名 */
    const char *sig;       /* 调号文本，如 "0"、"1#"、"3b" */
    const char *rel_root;  /* 关系小调根音名，如 "A" */
    const char *par_minor; /* 平行小调名，如 "Cm" */
    const char *scale;     /* 音阶拼写，如 "C D E F G A B" */
} fifth_key_t;

/* 顺时针从顶部 C 开始的 12 个调号 */
static const fifth_key_t s_fifth_keys[] = {
    {"C", 0, "0", "A", "Cm", "C D E F G A B"},
    {"G", 7, "1#", "E", "Gm", "G A B C D E F#"},
    {"D", 2, "2#", "B", "Dm", "D E F# G A B C#"},
    {"A", 9, "3#", "F#", "Am", "A B C# D E F# G#"},
    {"E", 4, "4#", "C#", "Em", "E F# G# A B C# D#"},
    {"B", 11, "5#", "G#", "Bm", "B C# D# E F# G# A#"},
    {"F#", 6, "6#", "D#", "F#m", "F# G# A# B C# D# E#"},
    {"Db", 1, "5b", "Bb", "C#m", "Db Eb F Gb Ab Bb C"},
    {"Ab", 8, "4b", "F", "G#m", "Ab Bb C Db Eb F G"},
    {"Eb", 3, "3b", "C", "Ebm", "Eb F G Ab Bb C D"},
    {"Bb", 10, "2b", "G", "Bbm", "Bb C D Eb F G A"},
    {"F", 5, "1b", "D", "Fm", "F G A Bb C D E"},
};
#define FIFTH_KEY_COUNT (sizeof(s_fifth_keys) / sizeof(s_fifth_keys[0]))

/* 大调音阶半音步进（小卷帘标记与试听共用） */
static const int8_t s_major_steps[7] = {0, 2, 4, 5, 7, 9, 11};

/* LVGL 回调（task_gui，锁外）只登记开关请求，on_update（task_app，锁内）执行 */
typedef enum {
    FIFTH_REQ_NONE = 0,
    FIFTH_REQ_OPEN,
    FIFTH_REQ_CLOSE,
} fifth_req_t;

typedef struct {
    bool open;             /* panel 打开中 */
    uint8_t index;         /* 当前调号索引 0~11 */
    void *circle_buf;
    int circle_w;
    int circle_h;
    void *piano_buf;
    int piano_w;
    int piano_h;
} fifth_state_t;

static fifth_state_t s_fifth = {0};
static volatile fifth_req_t s_fifth_req = FIFTH_REQ_NONE;

static void fifth_refresh_labels(void);
static void fifth_play_scale(void);
static bool fifth_circle_ensure_buffer(void);
static bool fifth_piano_ensure_buffer(void);
static void fifth_circle_redraw(void);
static void fifth_piano_redraw(void);
static void fifth_panel_open(void);
static void fifth_panel_close(void);

/* -------------------- 和弦播放 -------------------- */

/* 前向声明 */
static void chord_populate_type_buttons(void);
static void chord_sync_type_buttons(void);
static void chord_select_type(uint8_t type_idx);
static void chord_refresh_labels(void);
static void chord_canvas_redraw(void);
static void app_chord_trainer_type_button_cb(lv_event_t *e);

static void chord_midi_note(uint8_t note, uint8_t velocity)
{
    engine_midi_event_t midi = {0};
    midi.type = (velocity > 0) ? ENGINE_MIDI_MSG_NOTE_ON : ENGINE_MIDI_MSG_NOTE_OFF;
    midi.channel = 0;
    midi.data1 = note;
    midi.data2 = velocity;
    midi.source_port = ENGINE_MIDI_PORT_APP;
    engine_midi_publish(&midi, 0);
}

/* Why: 本 App 不设音色，channel 0 会继承上一个 App 的 Program（如 XY Pad
 * 退出后试听全变弦乐）；初始化显式回 GM 默认（Bank MSB/LSB=0 + PC=0） */
static void chord_reset_timbre(void)
{
    engine_midi_event_t evt = {0};
    evt.type = ENGINE_MIDI_MSG_CONTROL_CHANGE;
    evt.channel = 0;
    evt.data1 = 0;   /* CC0 Bank MSB */
    evt.source_port = ENGINE_MIDI_PORT_APP;
    engine_midi_publish(&evt, 0);
    evt.data1 = 32;  /* CC32 Bank LSB */
    engine_midi_publish(&evt, 0);
    evt.type = ENGINE_MIDI_MSG_PROGRAM_CHANGE;
    evt.data1 = 0;
    engine_midi_publish(&evt, 0);
}

static void chord_play_note(uint8_t note, uint32_t duration_ms, uint32_t delay_ms)
{
    if (s_chord.note_count >= CHORD_PLAY_MAX_NOTES) {
        return;
    }
    int64_t now = esp_timer_get_time();
    chord_note_slot_t *slot = &s_chord.notes[s_chord.note_count];
    slot->note = note;
    slot->on_at_us = now + (int64_t)delay_ms * 1000;
    slot->off_at_us = slot->on_at_us + (int64_t)duration_ms * 1000;
    slot->started = false;
    s_chord.note_count++;
}

/* -------------------- 和弦类型按钮管理 -------------------- */

/* 动态创建所有和弦类型按钮到 Panel，并设置互斥逻辑 */
static void chord_populate_type_buttons(void)
{
    if (s_chord_ui.chord_panel_type_poll == NULL) {
        ESP_LOGE(TAG, "chord_panel_type_poll not bound");
        return;
    }

    lvgl_port_lock(portMAX_DELAY);

    /* 清空 Panel 现有内容 */
    lv_obj_clean(s_chord_ui.chord_panel_type_poll);

    for (uint8_t i = 0; i < CHORD_TYPE_COUNT; i++) {
        lv_obj_t *btn = lv_button_create(s_chord_ui.chord_panel_type_poll);
        lv_obj_set_size(btn, 120, LV_PCT(100)); /* 固定宽度 120px 高度撑满父容器 */
        lv_obj_set_style_bg_color(btn, engine_gui_theme_color(COLOR_BG_PRIMARY), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(btn, engine_gui_theme_color(COLOR_TEXT_PRIMARY), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(btn, engine_gui_theme_color(COLOR_SHADOW), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn, engine_gui_theme_color(COLOR_CARD), LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(btn, engine_gui_theme_color(COLOR_TEXT_PRIMARY), LV_PART_MAIN | LV_STATE_CHECKED);

        /* 创建标签，显示 "中文名\n符号" 两行 */
        lv_obj_t *label = lv_label_create(btn);
        char label_text[32];
        snprintf(label_text, sizeof(label_text), "%s\n%s", 
                 _(s_chord_types[i].cn_name), 
                 s_chord_types[i].suffix[0] ? s_chord_types[i].suffix : "");
        lv_label_set_text(label, label_text);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(label);

        /* 绑定点击事件 */
        lv_obj_add_event_cb(btn, app_chord_trainer_type_button_cb,
                            LV_EVENT_CLICKED, (void *)(intptr_t)i);

        /* 保存按钮指针 */
        s_chord_type_buttons[i].btn = btn;
        s_chord_type_buttons[i].type_index = i;
    }

    lvgl_port_unlock();
}

/* 设置指定按钮的检查状态（互斥逻辑） */
static void chord_type_button_set_checked(uint8_t idx, bool checked)
{
    if (idx >= CHORD_TYPE_COUNT || s_chord_type_buttons[idx].btn == NULL) {
        return;
    }
    lvgl_port_lock(portMAX_DELAY);
    if (checked) {
        lv_obj_add_state(s_chord_type_buttons[idx].btn, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(s_chord_type_buttons[idx].btn, LV_STATE_CHECKED);
    }
    lvgl_port_unlock();
}

/* 选择和弦类型（内部执行互斥更新 + 刷新 UI） */
static void chord_select_type(uint8_t type_idx)
{
    if (type_idx >= CHORD_TYPE_COUNT) {
        return;
    }
    /* 若已选中则无需重复操作 */
    if (type_idx == s_chord.type) {
        return;
    }
    s_chord.type = type_idx;
    
    /* 更新所有按钮视觉状态（互斥） */
    for (uint8_t i = 0; i < CHORD_TYPE_COUNT; i++) {
        chord_type_button_set_checked(i, (i == type_idx));
    }
    
    ESP_LOGD(TAG, "type=%d (%s)", type_idx, s_chord_types[type_idx].suffix);
    chord_refresh_labels();
    chord_canvas_redraw();
}

/* 和弦类型按钮点击回调 */
static void app_chord_trainer_type_button_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    /* 遍历查找按钮对应的索引 */
    for (uint8_t i = 0; i < CHORD_TYPE_COUNT; i++) {
        if (s_chord_type_buttons[i].btn == btn) {
            chord_select_type(i);
            break;
        }
    }
}

/* 同步刷新和弦类型按钮的视觉状态（用于初始化/恢复） */
static void chord_sync_type_buttons(void)
{
    for (uint8_t i = 0; i < CHORD_TYPE_COUNT; i++) {
        chord_type_button_set_checked(i, (i == s_chord.type));
    }
}

static void chord_stop_all_notes(void)
{
    for (int i = 0; i < s_chord.note_count; i++) {
        if (s_chord.notes[i].started) {
            chord_midi_note(s_chord.notes[i].note, 0);
        }
    }
    s_chord.note_count = 0;
}

/* 点按 canvas 播放当前和弦：柱式与分解每次轮换 */
static void chord_play_current(void)
{
    const chord_type_t *ct = &s_chord_types[s_chord.type];

    chord_stop_all_notes();
    for (int i = 0; i < ct->tone_count; i++) {
        uint8_t note = (uint8_t)(60 + s_chord.root + ct->tones[i]);
        uint32_t delay = s_chord.arpeggio ? (uint32_t)i * CHORD_ARPEGGIO_STEP_MS : 0;
        chord_play_note(note, CHORD_BLOCK_DUR_MS, delay);
    }
    s_chord.arpeggio = !s_chord.arpeggio;
}

static void chord_process_notes(void)
{
    if (s_chord.note_count == 0) {
        return;
    }

    int64_t now = esp_timer_get_time();
    int write = 0;
    for (int i = 0; i < s_chord.note_count; i++) {
        chord_note_slot_t *n = &s_chord.notes[i];
        if (!n->started && now >= n->on_at_us) {
            chord_midi_note(n->note, 100);
            n->started = true;
        }
        if (n->started && now >= n->off_at_us) {
            chord_midi_note(n->note, 0);
        } else {
            if (write != i) {
                s_chord.notes[write] = s_chord.notes[i];
            }
            write++;
        }
    }
    s_chord.note_count = write;
}

/* -------------------- 画布绘制 -------------------- */

static float chord_white_w(void)
{
    return (float)s_chord.canvas_w / CHORD_WHITE_COUNT;
}

/* 判断音名是否为白键，返回单八度白键索引；黑键返回 -1 */
static int chord_pc_to_white(int pc)
{
    for (int i = 0; i < 7; i++) {
        if (s_white_pc[i] == pc) {
            return i;
        }
    }
    return -1;
}

static int chord_pc_to_black(int pc)
{
    for (int i = 0; i < 5; i++) {
        if (s_black_pc[i] == pc) {
            return i;
        }
    }
    return -1;
}

static void chord_draw_marker(lv_layer_t *layer, int32_t cx, int32_t cy, bool is_root)
{
    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.color = engine_gui_theme_color(is_root ? CHORD_C_ROOT : CHORD_C_TONE);
    dsc.width = (lv_value_precise_t)CHORD_DOT_RADIUS;
    dsc.opa = LV_OPA_COVER;
    dsc.center.x = (lv_value_precise_t)cx;
    dsc.center.y = (lv_value_precise_t)cy;
    dsc.radius = (lv_value_precise_t)CHORD_DOT_RADIUS;
    dsc.start_angle = 0;
    dsc.end_angle = 360;
    lv_draw_arc(layer, &dsc);
}

static void chord_draw_canvas(lv_obj_t *canvas)
{
    int w = s_chord.canvas_w;
    int h = s_chord.canvas_h;
    if (w <= 0 || h <= 0) {
        return;
    }
    const chord_type_t *ct = &s_chord_types[s_chord.type];
    float white_w = chord_white_w();
    float black_w = white_w * 0.58f;
    float black_h = h * 0.62f;

    lv_canvas_fill_bg(canvas, engine_gui_theme_color(CHORD_C_BG), LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    static const char white_names[7][2] = {"C", "D", "E", "F", "G", "A", "B"};

    /* 白键 */
    for (int i = 0; i < CHORD_WHITE_COUNT; i++) {
        lv_draw_rect_dsc_t key_dsc;
        lv_draw_rect_dsc_init(&key_dsc);
        key_dsc.bg_color = engine_gui_theme_color(CHORD_C_WHITE_KEY);
        key_dsc.bg_opa = LV_OPA_COVER;
        key_dsc.border_color = engine_gui_theme_color(CHORD_C_DIM);
        key_dsc.border_width = 2;
        key_dsc.border_opa = LV_OPA_COVER;
        key_dsc.radius = 6;

        lv_area_t area = {
            (int32_t)(i * white_w) + 1, 1,
            (int32_t)((i + 1) * white_w) - 2, h - 2
        };
        lv_draw_rect(&layer, &key_dsc, &area);

        lv_draw_label_dsc_t name_dsc;
        lv_draw_label_dsc_init(&name_dsc);
        name_dsc.font = &lv_font_montserrat_14;
        name_dsc.color = engine_gui_theme_color(CHORD_C_DIM);
        name_dsc.text = white_names[i % 7];
        name_dsc.align = LV_TEXT_ALIGN_CENTER;
        lv_area_t name_area = {
            (int32_t)(i * white_w), h - 22,
            (int32_t)((i + 1) * white_w), h - 4
        };
        lv_draw_label(&layer, &name_dsc, &name_area);
    }

    /* 黑键 */
    for (int j = 0; j < CHORD_BLACK_COUNT; j++) {
        int octave = j / 5;
        int pos = s_black_pos[j % 5];
        float cx = (octave * 7 + pos + 1) * white_w;

        lv_draw_rect_dsc_t key_dsc;
        lv_draw_rect_dsc_init(&key_dsc);
        key_dsc.bg_color = engine_gui_theme_color(CHORD_C_BLACK_KEY);
        key_dsc.bg_opa = LV_OPA_COVER;
        key_dsc.border_color = engine_gui_theme_color(CHORD_C_DIM);
        key_dsc.border_width = 1;
        key_dsc.border_opa = LV_OPA_COVER;
        key_dsc.radius = 4;

        lv_area_t area = {
            (int32_t)(cx - black_w / 2), 1,
            (int32_t)(cx + black_w / 2), (int32_t)black_h
        };
        lv_draw_rect(&layer, &key_dsc, &area);
    }

    /* 和弦内音标记：跨两个八度全部标出，根音着重 */
    for (int t = 0; t < ct->tone_count; t++) {
        int pc = (s_chord.root + ct->tones[t]) % 12;
        bool is_root = (ct->tones[t] == 0);

        int wi = chord_pc_to_white(pc);
        if (wi >= 0) {
            for (int o = 0; o < 2; o++) {
                int32_t cx = (int32_t)((o * 7 + wi + 0.5f) * white_w);
                chord_draw_marker(&layer, cx, h - 40, is_root);
            }
            continue;
        }

        int bi = chord_pc_to_black(pc);
        if (bi >= 0) {
            for (int o = 0; o < 2; o++) {
                int32_t cx = (int32_t)((o * 7 + s_black_pos[bi] + 1) * white_w);
                chord_draw_marker(&layer, cx, (int32_t)black_h - 30, is_root);
            }
        }
    }

    lv_canvas_finish_layer(canvas, &layer);
}

/* 确保卷帘 canvas buffer 与控件当前尺寸一致；返回 true 表示新建/重建需重绘。
 * Trap: PCT 尺寸在布局收敛前读到的是设计占位值，先强制布局再测量；
 * LVGL 9 canvas 将 buffer 居中绘制，buffer 偏小会缩成中间一小块。 */
static bool chord_canvas_ensure_buffer(void)
{
    if (s_chord_ui.canvas == NULL) {
        return false;
    }

    lvgl_port_lock(portMAX_DELAY);
    lv_obj_update_layout(s_chord_ui.canvas);
    int w = lv_obj_get_width(s_chord_ui.canvas);
    int h = lv_obj_get_height(s_chord_ui.canvas);
    lvgl_port_unlock();
    if (w <= 0 || h <= 0) {
        return false;
    }

    if (s_chord.canvas_buf != NULL && s_chord.canvas_w == w && s_chord.canvas_h == h) {
        return false;
    }

    void *buf = heap_caps_calloc(1, (size_t)w * h * 2, MALLOC_CAP_SPIRAM);
    if (buf == NULL) {
        ESP_LOGE(TAG, "canvas buf alloc failed");
        return false;
    }

    void *old = s_chord.canvas_buf;
    lvgl_port_lock(portMAX_DELAY);
    lv_canvas_set_buffer(s_chord_ui.canvas, buf, w, h, LV_COLOR_FORMAT_RGB565);
    if (old != NULL) {
        heap_caps_free(old);
    }
    lvgl_port_unlock();
    s_chord.canvas_buf = buf;
    s_chord.canvas_w = w;
    s_chord.canvas_h = h;
    ESP_LOGI(TAG, "canvas %dx%d", w, h);
    return true;
}

/* App 逻辑运行在 task_gui / task_app 上下文，绘制与 invalidate 必须持 LVGL 锁 */
static void chord_canvas_redraw(void)
{
    if (s_chord_ui.canvas == NULL || s_chord.canvas_buf == NULL) {
        return;
    }
    lvgl_port_lock(portMAX_DELAY);
    chord_draw_canvas(s_chord_ui.canvas);
    lv_obj_clear_flag(s_chord_ui.canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(s_chord_ui.canvas);
    lvgl_port_unlock();
}

/* -------------------- UI 刷新 -------------------- */

static void chord_refresh_labels(void)
{
    char name[16];
    snprintf(name, sizeof(name), "%s%s",
             s_root_names[s_chord.root], s_chord_types[s_chord.type].suffix);

    lvgl_port_lock(portMAX_DELAY);
    lv_label_set_text(s_chord_ui.chord_name, name);
    lv_label_set_text(s_chord_ui.chord_definition, _(s_chord_types[s_chord.type].definition));
    lvgl_port_unlock();
}

/* 根音 12 键 CHECKED 互斥 */
static void chord_key_check(uint32_t btn)
{
    lvgl_port_lock(portMAX_DELAY);
    for (uint32_t i = 0; i < 12; i++) {
        if (i == btn) {
            lv_buttonmatrix_set_button_ctrl(s_chord_ui.chord_key_key, i,
                                            LV_BUTTONMATRIX_CTRL_CHECKED);
        } else {
            lv_buttonmatrix_clear_button_ctrl(s_chord_ui.chord_key_key, i,
                                              LV_BUTTONMATRIX_CTRL_CHECKED);
        }
    }
    lvgl_port_unlock();
}

static void chord_select_root(uint32_t btn)
{
    if (btn >= 12 || btn == s_chord.root) {
        return;
    }
    s_chord.root = (uint8_t)btn;
    chord_key_check(btn);
    ESP_LOGD(TAG, "root=%s", s_root_names[s_chord.root]);
    chord_refresh_labels();
    chord_canvas_redraw();
}

/* -------------------- 五度圈 panel -------------------- */

static void fifth_refresh_labels(void)
{
    const fifth_key_t *k = &s_fifth_keys[s_fifth.index];
    char buf[64];
    lvgl_port_lock(portMAX_DELAY);

    snprintf(buf, sizeof(buf), _("%s大调 / %s小调"), k->name, k->rel_root);
    lv_label_set_text(s_chord_ui.fifth_name, buf);

    snprintf(buf, sizeof(buf), _("调号: %s"), k->sig);
    lv_label_set_text(s_chord_ui.fifth_key_sig, buf);

    lv_label_set_text(s_chord_ui.fifth_scale, k->scale);

    snprintf(buf, sizeof(buf), "%s(V)", s_fifth_keys[(s_fifth.index + 1) % FIFTH_KEY_COUNT].name);
    lv_label_set_text(s_chord_ui.fifth_dominant, buf);

    snprintf(buf, sizeof(buf), "%s(IV)", s_fifth_keys[(s_fifth.index + FIFTH_KEY_COUNT - 1) % FIFTH_KEY_COUNT].name);
    lv_label_set_text(s_chord_ui.fifth_subdominant, buf);

    lv_label_set_text(s_chord_ui.fifth_parallel, k->par_minor);

    lvgl_port_unlock();
}

/* 点按小卷帘试听当前调上行音阶：复用和弦音符队列（task_app 周期调度） */
static void fifth_play_scale(void)
{
    static const int8_t steps[FIFTH_SCALE_NOTES] = {0, 2, 4, 5, 7, 9, 11, 12};
    chord_stop_all_notes();
    for (int i = 0; i < FIFTH_SCALE_NOTES; i++) {
        uint8_t note = (uint8_t)(60 + s_fifth_keys[s_fifth.index].root_pc + steps[i]);
        chord_play_note(note, CHORD_ARPEGGIO_STEP_MS,
                        FIFTH_PLAY_DELAY_MS + (uint32_t)i * CHORD_ARPEGGIO_STEP_MS);
    }
}

static void fifth_draw_marker(lv_layer_t *layer, int32_t cx, int32_t cy, bool is_root)
{
    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.color = engine_gui_theme_color(is_root ? CHORD_C_ROOT : CHORD_C_TONE);
    dsc.width = 8;
    dsc.opa = LV_OPA_COVER;
    dsc.center.x = cx;
    dsc.center.y = cy;
    dsc.radius = 8;
    dsc.start_angle = 0;
    dsc.end_angle = 360;
    lv_draw_arc(layer, &dsc);
}

static void fifth_draw_piano_canvas(lv_obj_t *canvas)
{
    int w = s_fifth.piano_w, h = s_fifth.piano_h;
    if (w <= 0 || h <= 0) {
        return;
    }

    float white_w = (float)w / 7.0f;
    float black_w = white_w * 0.58f;
    float black_h = h * 0.62f;

    lv_canvas_fill_bg(canvas, engine_gui_theme_color(CHORD_C_BG), LV_OPA_COVER);
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    static const char white_names[7][2] = {"C", "D", "E", "F", "G", "A", "B"};

    for (int i = 0; i < 7; i++) {
        lv_draw_rect_dsc_t key_dsc;
        lv_draw_rect_dsc_init(&key_dsc);
        key_dsc.bg_color = engine_gui_theme_color(CHORD_C_WHITE_KEY);
        key_dsc.bg_opa = LV_OPA_COVER;
        key_dsc.border_color = engine_gui_theme_color(CHORD_C_DIM);
        key_dsc.border_width = 1;
        key_dsc.border_opa = LV_OPA_COVER;
        key_dsc.radius = 4;
        lv_area_t area = {
            (int32_t)(i * white_w) + 1, 1,
            (int32_t)((i + 1) * white_w) - 2, h - 2
        };
        lv_draw_rect(&layer, &key_dsc, &area);

        lv_draw_label_dsc_t name_dsc;
        lv_draw_label_dsc_init(&name_dsc);
        name_dsc.font = &lv_font_montserrat_14;
        name_dsc.color = engine_gui_theme_color(CHORD_C_DIM);
        name_dsc.text = white_names[i];
        name_dsc.align = LV_TEXT_ALIGN_CENTER;
        lv_area_t name_area = {
            (int32_t)(i * white_w), h - 20,
            (int32_t)((i + 1) * white_w), h - 3
        };
        lv_draw_label(&layer, &name_dsc, &name_area);
    }

    for (int j = 0; j < 5; j++) {
        float cx = (s_black_pos[j] + 1) * white_w;
        lv_draw_rect_dsc_t key_dsc;
        lv_draw_rect_dsc_init(&key_dsc);
        key_dsc.bg_color = engine_gui_theme_color(CHORD_C_BLACK_KEY);
        key_dsc.bg_opa = LV_OPA_COVER;
        key_dsc.border_color = engine_gui_theme_color(CHORD_C_DIM);
        key_dsc.border_width = 1;
        key_dsc.border_opa = LV_OPA_COVER;
        key_dsc.radius = 3;
        lv_area_t area = {
            (int32_t)(cx - black_w / 2), 1,
            (int32_t)(cx + black_w / 2), (int32_t)black_h
        };
        lv_draw_rect(&layer, &key_dsc, &area);
    }

    /* 标记当前调自然大调音级，根音着重 */
    uint8_t root_pc = s_fifth_keys[s_fifth.index].root_pc;
    for (int t = 0; t < 7; t++) {
        int pc = (root_pc + s_major_steps[t]) % 12;
        bool is_root = (t == 0);
        int wi = chord_pc_to_white(pc);
        if (wi >= 0) {
            int32_t cx = (int32_t)((wi + 0.5f) * white_w);
            fifth_draw_marker(&layer, cx, h - 34, is_root);
            continue;
        }
        int bi = chord_pc_to_black(pc);
        if (bi >= 0) {
            int32_t cx = (int32_t)((s_black_pos[bi] + 1) * white_w);
            fifth_draw_marker(&layer, cx, (int32_t)black_h - 14, is_root);
        }
    }

    lv_canvas_finish_layer(canvas, &layer);
}

static void fifth_piano_redraw(void)
{
    if (s_chord_ui.fifth_canvas_piano == NULL || s_fifth.piano_buf == NULL) {
        return;
    }
    lvgl_port_lock(portMAX_DELAY);
    fifth_draw_piano_canvas(s_chord_ui.fifth_canvas_piano);
    lv_obj_clear_flag(s_chord_ui.fifth_canvas_piano, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(s_chord_ui.fifth_canvas_piano);
    lvgl_port_unlock();
}

/* ensure 模式同 chord_canvas_ensure_buffer：强制布局后实测，尺寸变化才重建 */
static bool fifth_piano_ensure_buffer(void)
{
    if (s_chord_ui.fifth_canvas_piano == NULL) {
        return false;
    }

    lvgl_port_lock(portMAX_DELAY);
    lv_obj_update_layout(s_chord_ui.fifth_canvas_piano);
    int w = lv_obj_get_width(s_chord_ui.fifth_canvas_piano);
    int h = lv_obj_get_height(s_chord_ui.fifth_canvas_piano);
    lvgl_port_unlock();
    if (w <= 0 || h <= 0) {
        return false;
    }

    if (s_fifth.piano_buf != NULL && s_fifth.piano_w == w && s_fifth.piano_h == h) {
        return false;
    }

    void *buf = heap_caps_calloc(1, (size_t)w * h * 2, MALLOC_CAP_SPIRAM);
    if (buf == NULL) {
        ESP_LOGE(TAG, "fifth piano canvas alloc failed");
        return false;
    }

    void *old = s_fifth.piano_buf;
    lvgl_port_lock(portMAX_DELAY);
    lv_canvas_set_buffer(s_chord_ui.fifth_canvas_piano, buf, w, h, LV_COLOR_FORMAT_RGB565);
    if (old != NULL) {
        heap_caps_free(old);
    }
    lvgl_port_unlock();
    s_fifth.piano_buf = buf;
    s_fifth.piano_w = w;
    s_fifth.piano_h = h;
    ESP_LOGI(TAG, "fifth piano canvas %dx%d", w, h);
    return true;
}

static void fifth_draw_circle_canvas(lv_obj_t *canvas)
{
    int w = s_fifth.circle_w, h = s_fifth.circle_h;
    if (w <= 0 || h <= 0) {
        return;
    }

    lv_canvas_fill_bg(canvas, engine_gui_theme_color(CHORD_C_BG), LV_OPA_COVER);
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    int cx = w / 2, cy = h / 2;
    int outer_radius = (w < h ? w : h) / 2 - 50;
    int inner_radius = outer_radius - 80;
    if (outer_radius < 70) outer_radius = 70;
    if (inner_radius < 30) inner_radius = 30;

    lv_color_t color_dim      = engine_gui_theme_color(CHORD_C_DIM);
    lv_color_t color_selected = engine_gui_theme_color(COLOR_PRIMARY);
    lv_color_t color_neighbor = engine_gui_theme_color(COLOR_SECONDARY);
    lv_color_t color_default  = engine_gui_theme_color(COLOR_CARD);

    /* 12 扇区（中心 ±15°），选中/邻接/默认三色 */
    for (int i = 0; i < 12; i++) {
        int center_deg = (270 + i * 30) % 360;
        int start_deg  = (center_deg > 15 ? center_deg - 15 : center_deg + 360 - 15);
        int end_deg    = center_deg + 15;

        lv_color_t color;
        if (i == s_fifth.index) {
            color = color_selected;
        } else if (i == (s_fifth.index + 1) % 12 || i == (s_fifth.index + 11) % 12) {
            color = color_neighbor;
        } else {
            color = color_default;
        }

        lv_draw_arc_dsc_t arc_dsc;
        lv_draw_arc_dsc_init(&arc_dsc);
        arc_dsc.color = color;
        arc_dsc.width = outer_radius - inner_radius;
        arc_dsc.opa = LV_OPA_COVER;
        arc_dsc.center.x = cx;
        arc_dsc.center.y = cy;
        arc_dsc.radius = (outer_radius + inner_radius) / 2;
        arc_dsc.start_angle = start_deg;
        arc_dsc.end_angle   = end_deg;
        arc_dsc.rounded = 0;
        lv_draw_arc(&layer, &arc_dsc);
    }

    /* 外圈分隔线 */
    lv_draw_arc_dsc_t line_dsc;
    lv_draw_arc_dsc_init(&line_dsc);
    line_dsc.color = color_dim;
    line_dsc.width = 2;
    line_dsc.opa = LV_OPA_COVER;
    line_dsc.center.x = cx;
    line_dsc.center.y = cy;
    line_dsc.radius = outer_radius;
    line_dsc.start_angle = 0;
    line_dsc.end_angle = 3600;
    line_dsc.rounded = 0;
    lv_draw_arc(&layer, &line_dsc);

    /* 外圈大调名 + 内圈关系小调名 */
    static char minor_names[12][8];
    for (int i = 0; i < 12; i++) {
        double angle_rad = (-90.0 + i * 30.0) * M_PI / 180.0;
        int x_out = cx + (int)((outer_radius + 30) * cos(angle_rad));
        int y_out = cy + (int)((outer_radius + 30) * sin(angle_rad));
        int x_in  = cx + (int)((outer_radius - 40) * cos(angle_rad));
        int y_in  = cy + (int)((outer_radius - 40) * sin(angle_rad));

        bool is_highlight = (i == s_fifth.index);

        lv_draw_label_dsc_t label_dsc;
        lv_draw_label_dsc_init(&label_dsc);

        label_dsc.color = engine_gui_theme_color(is_highlight ? CHORD_C_ROOT : CHORD_C_DIM);
        label_dsc.font = ui_font_chinese_40;
        label_dsc.text = s_fifth_keys[i].name;
        label_dsc.align = LV_TEXT_ALIGN_CENTER;
        lv_area_t area_out = { x_out - 40, y_out - 25, x_out + 40, y_out + 25 };
        lv_draw_label(&layer, &label_dsc, &area_out);

        snprintf(minor_names[i], sizeof(minor_names[i]), "%sm", s_fifth_keys[i].rel_root);
        label_dsc.color = engine_gui_theme_color(is_highlight ? CHORD_C_ROOT : CHORD_C_DIM);
        label_dsc.font = ui_font_chinese_30;
        label_dsc.text = minor_names[i];
        label_dsc.align = LV_TEXT_ALIGN_CENTER;
        lv_area_t area_in = { x_in - 40, y_in - 20, x_in + 40, y_in + 20 };
        lv_draw_label(&layer, &label_dsc, &area_in);
    }

    /* 中心装饰圆 */
    lv_draw_arc_dsc_t center_dsc;
    lv_draw_arc_dsc_init(&center_dsc);
    center_dsc.color = color_dim;
    center_dsc.width = 2;
    center_dsc.opa = LV_OPA_COVER;
    center_dsc.center.x = cx;
    center_dsc.center.y = cy;
    center_dsc.radius = 40;
    center_dsc.start_angle = 0;
    center_dsc.end_angle = 3600;
    center_dsc.rounded = 0;
    lv_draw_arc(&layer, &center_dsc);

    lv_canvas_finish_layer(canvas, &layer);
}

static void fifth_circle_redraw(void)
{
    if (s_chord_ui.fifth_canvas_circle == NULL || s_fifth.circle_buf == NULL) {
        return;
    }
    lvgl_port_lock(portMAX_DELAY);
    fifth_draw_circle_canvas(s_chord_ui.fifth_canvas_circle);
    lv_obj_clear_flag(s_chord_ui.fifth_canvas_circle, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(s_chord_ui.fifth_canvas_circle);
    lvgl_port_unlock();
}

static bool fifth_circle_ensure_buffer(void)
{
    if (s_chord_ui.fifth_canvas_circle == NULL) {
        return false;
    }

    lvgl_port_lock(portMAX_DELAY);
    lv_obj_update_layout(s_chord_ui.fifth_canvas_circle);
    int w = lv_obj_get_width(s_chord_ui.fifth_canvas_circle);
    int h = lv_obj_get_height(s_chord_ui.fifth_canvas_circle);
    lvgl_port_unlock();
    if (w <= 0 || h <= 0) {
        return false;
    }

    if (s_fifth.circle_buf != NULL && s_fifth.circle_w == w && s_fifth.circle_h == h) {
        return false;
    }

    void *buf = heap_caps_calloc(1, (size_t)w * h * 2, MALLOC_CAP_SPIRAM);
    if (buf == NULL) {
        ESP_LOGE(TAG, "fifth circle canvas alloc failed");
        return false;
    }

    void *old = s_fifth.circle_buf;
    lvgl_port_lock(portMAX_DELAY);
    lv_canvas_set_buffer(s_chord_ui.fifth_canvas_circle, buf, w, h, LV_COLOR_FORMAT_RGB565);
    if (old != NULL) {
        heap_caps_free(old);
    }
    lvgl_port_unlock();
    s_fifth.circle_buf = buf;
    s_fifth.circle_w = w;
    s_fifth.circle_h = h;
    ESP_LOGI(TAG, "fifth circle canvas %dx%d", w, h);
    return true;
}

/* Why 缓冲共享：panel 打开时释放和弦卷帘 buffer 供五度圈两个 canvas 复用，
 * PSRAM 占用不叠加；panel 置 clickable 吞掉下层控件（根音矩阵等）的穿透点击。
 * 在 on_update（task_app，App 锁内）执行，与卷帘重绘无并发。 */
static void fifth_panel_open(void)
{
    if (s_fifth.open || s_chord_ui.panel_fifth == NULL) {
        return;
    }
    s_fifth.open = true;

    /* 先释放卷帘 buffer 再分配五度圈 buffer，峰值不叠加 */
    if (s_chord.canvas_buf != NULL) {
        heap_caps_free(s_chord.canvas_buf);
        s_chord.canvas_buf = NULL;
        s_chord.canvas_w = 0;
        s_chord.canvas_h = 0;
    }

    lvgl_port_lock(portMAX_DELAY);
    if (s_chord_ui.canvas != NULL) {
        lv_obj_add_flag(s_chord_ui.canvas, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_flag(s_chord_ui.panel_fifth, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_chord_ui.panel_fifth, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_update_layout(s_chord_ui.panel_fifth);
    lvgl_port_unlock();

    fifth_refresh_labels();
    if (fifth_circle_ensure_buffer()) {
        fifth_circle_redraw();
    }
    if (fifth_piano_ensure_buffer()) {
        fifth_piano_redraw();
    }
}

/* 关闭 panel：释放五度圈 buffer，重建并重绘和弦卷帘 */
static void fifth_panel_close(void)
{
    if (!s_fifth.open) {
        return;
    }
    s_fifth.open = false;
    chord_stop_all_notes();

    lvgl_port_lock(portMAX_DELAY);
    lv_obj_add_flag(s_chord_ui.panel_fifth, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_chord_ui.panel_fifth, LV_OBJ_FLAG_CLICKABLE);
    lvgl_port_unlock();

    if (s_fifth.circle_buf != NULL) {
        heap_caps_free(s_fifth.circle_buf);
        s_fifth.circle_buf = NULL;
        s_fifth.circle_w = 0;
        s_fifth.circle_h = 0;
    }
    if (s_fifth.piano_buf != NULL) {
        heap_caps_free(s_fifth.piano_buf);
        s_fifth.piano_buf = NULL;
        s_fifth.piano_w = 0;
        s_fifth.piano_h = 0;
    }

    chord_canvas_ensure_buffer();
    chord_canvas_redraw();
}

static void chord_btn_circle_cb(lv_event_t *e)
{
    (void)e;
    s_fifth_req = FIFTH_REQ_OPEN;
}

static void fifth_btn_return_cb(lv_event_t *e)
{
    (void)e;
    s_fifth_req = FIFTH_REQ_CLOSE;
}

static void fifth_piano_play_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
        fifth_play_scale();
    }
}

/* -------------------- 生命周期与事件 -------------------- */

static void app_chord_trainer_home_cb(lv_event_t *e);
static void app_chord_trainer_event_cb(lv_event_t *e);

/* 首帧拆分倒计时（on_update 周期数），见 on_update 顶部 */
static uint8_t s_canvas_defer = 0;

static bool app_chord_trainer_on_init(app_base_t *self, void *screen_ctx)
{
    (void)self;
    ESP_LOGI(TAG, "init");

    memset(&s_chord, 0, sizeof(s_chord));
    memset(&s_fifth, 0, sizeof(s_fifth));
    s_fifth_req = FIFTH_REQ_NONE;

    chord_reset_timbre();

    ui_screen_chord_t *ui = (ui_screen_chord_t *)screen_ctx;

    if (ui->canvas == NULL || lv_obj_get_class(ui->canvas) != &lv_canvas_class) {
        ESP_LOGE(TAG, "canvas object invalid");
        return false;
    }

    /* 不启用 LVGL 的 CHECKABLE 自动翻转，CHECKED 由后端互斥独占管理 */
    chord_key_check(0);
    
    lvgl_port_lock(portMAX_DELAY);
    if (ui->btn_home != NULL) {
        lv_obj_add_event_cb(ui->btn_home, app_chord_trainer_home_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui->canvas != NULL) {
        lv_obj_add_event_cb(ui->canvas, app_chord_trainer_event_cb, LV_EVENT_PRESSED, self);
    }
    if (ui->chord_key_key != NULL) {
        lv_obj_add_event_cb(ui->chord_key_key, app_chord_trainer_event_cb, LV_EVENT_VALUE_CHANGED, self);
    }
    if (ui->btn_circle != NULL) {
        lv_obj_add_event_cb(ui->btn_circle, chord_btn_circle_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui->fifth_btn_return != NULL) {
        lv_obj_add_event_cb(ui->fifth_btn_return, fifth_btn_return_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui->fifth_canvas_piano != NULL) {
        lv_obj_add_event_cb(ui->fifth_canvas_piano, fifth_piano_play_cb, LV_EVENT_PRESSED, NULL);
    }
    /* 五度圈 panel 默认隐藏（EEZ 里为可见设计态），chord_btn_circle 打开 */
    if (ui->panel_fifth != NULL) {
        lv_obj_add_flag(ui->panel_fifth, LV_OBJ_FLAG_HIDDEN);
    }
    /* 动态填充和弦类型按钮到 Panel */
    chord_populate_type_buttons();
    lvgl_port_unlock();

    chord_refresh_labels();
    /* 首帧拆分：canvas 首绘推迟到 on_update 倒计时结束，背景先上屏 */
    s_canvas_defer = 3;
    chord_sync_type_buttons();

    return true;
}

static void app_chord_trainer_on_ui_event(app_base_t *self, lv_event_t *e)
{
    (void)self;
    ui_screen_chord_t *ui = &s_chord_ui;
    lv_obj_t *target = lv_event_get_target_obj(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (target == ui->canvas && code == LV_EVENT_PRESSED) {
        chord_play_current();
        return;
    }

    if (code != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    if (target == ui->chord_key_key) {
        uint32_t btn = lv_buttonmatrix_get_selected_button(ui->chord_key_key);
        if (btn != LV_BUTTONMATRIX_BUTTON_NONE) {
            chord_select_root(btn);
        }
        return;
    }
}

static void app_chord_trainer_event_cb(lv_event_t *e)
{
    app_base_t *self = (app_base_t *)lv_event_get_user_data(e);
    app_chord_trainer_on_ui_event(self, e);
}

static void app_chord_trainer_home_cb(lv_event_t *e)
{
    (void)e;
    app_manager_request_kill_active();
}

static void app_chord_trainer_on_update(app_base_t *self)
{
    (void)self;

    chord_process_notes();

    /* 五度圈 panel 开关请求：LVGL 回调只登记，buffer 收放在锁内串行执行 */
    if (s_fifth_req != FIFTH_REQ_NONE) {
        fifth_req_t req = s_fifth_req;
        s_fifth_req = FIFTH_REQ_NONE;
        if (req == FIFTH_REQ_OPEN) {
            fifth_panel_open();
        } else {
            fifth_panel_close();
        }
    }

    if (s_fifth.open) {
        /* panel 打开期：卷帘 buffer 已让出（跳过卷帘 ensure 防重复占用 PSRAM）；
         * 五度圈 canvas 每拍 ensure，覆盖 grid STRETCH 布局收敛与分配失败重试 */
        if (fifth_circle_ensure_buffer()) {
            fifth_circle_redraw();
        }
        if (fifth_piano_ensure_buffer()) {
            fifth_piano_redraw();
        }
    } else if (s_canvas_defer > 0) {
        /* 首帧拆分：canvas 分配/首绘推迟 ~30ms 让背景先上屏（防 DPI underrun 闪屏） */
        s_canvas_defer--;
    } else if (chord_canvas_ensure_buffer()) {
        /* ensure 返回 true 表示新建/重建了 buffer（含尺寸修正与失败重试），需重绘 */
        chord_canvas_redraw();
    }

    /* 轮询根音键选中态收敛：EEZ 事件接线缺失时也能正常工作 */
    if (s_chord_ui.chord_key_key == NULL) {
        return;
    }
    lvgl_port_lock(portMAX_DELAY);
    uint32_t btn = lv_buttonmatrix_get_selected_button(s_chord_ui.chord_key_key);
    lvgl_port_unlock();
    if (btn != LV_BUTTONMATRIX_BUTTON_NONE) {
        chord_select_root(btn);
    }
}

/* 五度圈扇区触摸：panel 打开时按角度命中 12 调号；
 * 屏幕坐标与 lv_obj_get_coords 同一坐标系（触摸已经逻辑旋转换算） */
static void app_chord_trainer_on_input(app_base_t *self, const app_input_event_t *evt)
{
    (void)self;
    if (!s_fifth.open || evt->type != APP_INPUT_TOUCH_DOWN) {
        return;
    }
    if (s_chord_ui.fifth_canvas_circle == NULL) {
        return;
    }

    lvgl_port_lock(portMAX_DELAY);
    lv_area_t area;
    lv_obj_get_coords(s_chord_ui.fifth_canvas_circle, &area);
    lvgl_port_unlock();
    int cx = (area.x1 + area.x2) / 2;
    int cy = (area.y1 + area.y2) / 2;
    int radius = (area.x2 - area.x1) / 2 - 10; /* 留边距 */

    int dx = (int)evt->x - cx;
    int dy = (int)evt->y - cy;
    if (dx * dx + dy * dy > radius * radius) {
        return;
    }

    double ang = atan2((double)dx, (double)(-dy)) * 180.0 / M_PI;
    if (ang < 0) {
        ang += 360.0;
    }
    int idx = ((int)(ang + 15.0)) / 30 % (int)FIFTH_KEY_COUNT;

    if (idx == s_fifth.index) {
        return;
    }
    s_fifth.index = (uint8_t)idx;
    ESP_LOGI(TAG, "fifth key=%s", s_fifth_keys[idx].name);

    fifth_refresh_labels();
    fifth_circle_redraw();
    fifth_piano_redraw();
}

static void app_chord_trainer_on_pause(app_base_t *self)
{
    (void)self;
    chord_stop_all_notes();
    ESP_LOGI(TAG, "pause");
}

static void app_chord_trainer_on_resume(app_base_t *self)
{
    (void)self;
    chord_sync_type_buttons();
    chord_canvas_redraw();
    if (s_fifth.open) {
        fifth_circle_redraw();
        fifth_piano_redraw();
    }
    ESP_LOGI(TAG, "resume");
}

static void app_chord_trainer_on_destroy(app_base_t *self)
{
    (void)self;
    ESP_LOGI(TAG, "destroy");
    chord_stop_all_notes();

    /* 移除 on_init 注册的事件回调：EEZ 屏幕对象持久存在，
     * 不移除会在再次进入时重复注册，一次点击多次触发（柱式被分解覆盖） */
    lvgl_port_lock(portMAX_DELAY);
    if (s_chord_ui.canvas != NULL) {
        lv_obj_remove_event_cb(s_chord_ui.canvas, app_chord_trainer_event_cb);
    }
    if (s_chord_ui.chord_key_key != NULL) {
        lv_obj_remove_event_cb(s_chord_ui.chord_key_key, app_chord_trainer_event_cb);
    }
    /* 移除动态创建的按钮事件回调 */
    for (uint8_t i = 0; i < CHORD_TYPE_COUNT; i++) {
        if (s_chord_type_buttons[i].btn != NULL) {
            lv_obj_remove_event_cb(s_chord_type_buttons[i].btn, app_chord_trainer_type_button_cb);
        }
    }
    if (s_chord_ui.btn_home != NULL) {
        lv_obj_remove_event_cb(s_chord_ui.btn_home, app_chord_trainer_home_cb);
    }
    if (s_chord_ui.btn_circle != NULL) {
        lv_obj_remove_event_cb(s_chord_ui.btn_circle, chord_btn_circle_cb);
    }
    if (s_chord_ui.fifth_btn_return != NULL) {
        lv_obj_remove_event_cb(s_chord_ui.fifth_btn_return, fifth_btn_return_cb);
    }
    if (s_chord_ui.fifth_canvas_piano != NULL) {
        lv_obj_remove_event_cb(s_chord_ui.fifth_canvas_piano, fifth_piano_play_cb);
    }
    lvgl_port_unlock();

    if (s_chord.canvas_buf != NULL) {
        heap_caps_free(s_chord.canvas_buf);
        s_chord.canvas_buf = NULL;
    }
    if (s_fifth.circle_buf != NULL) {
        heap_caps_free(s_fifth.circle_buf);
        s_fifth.circle_buf = NULL;
    }
    if (s_fifth.piano_buf != NULL) {
        heap_caps_free(s_fifth.piano_buf);
        s_fifth.piano_buf = NULL;
    }
}

esp_err_t app_chord_trainer_register(void)
{
    static app_base_t app = {
        .name = "Chord Trainer",
        .screen_name = "app_chord_memory",
        .screen_ctx = &s_chord_ui,
        .screen_ctx_size = sizeof(s_chord_ui),
        .widget_bindings = s_chord_bindings,
        .on_init = app_chord_trainer_on_init,
        .on_update = app_chord_trainer_on_update,
        .on_pause = app_chord_trainer_on_pause,
        .on_resume = app_chord_trainer_on_resume,
        .on_destroy = app_chord_trainer_on_destroy,
        .on_input = app_chord_trainer_on_input,
        .on_ui_event = app_chord_trainer_on_ui_event,
    };
    return app_manager_register(&app);
}