/**
 * @file app_chord_trainer.c
 * @brief 和弦练习 App：根音 × 和弦类型的构成音可视化
 *
 * 根音由 12 键矩阵单选（CHECKED 互斥），和弦类型由点选按钮选择；
 * chord_name 显示组合和弦名，chord_definition 显示音程堆叠定义；
 * canvas 绘制两个八度琴键并标记和弦内音（根音着重），纯展示不发声。
 * 配色全部取自 EEZ 主题数组：白键 bg_secondary(1)，黑键 shadow(16)。
 */

#include "app_chord_trainer.h"
#include "app_manager.h"
#include "engine_gui.h"
#include "engine_midi.h"
#include "service_i18n.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lvgl_port.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "app_chord_trainer";

/* 画布在 app_chord_memory 屏幕中的位置与尺寸（与 EEZ 布局一致） */
#define CHORD_CANVAS_X      32
#define CHORD_CANVAS_Y      350
#define CHORD_CANVAS_W      1216
#define CHORD_CANVAS_H      260

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
} chord_state_t;

typedef struct {
    lv_obj_t *canvas;
    lv_obj_t *chord_name;
    lv_obj_t *chord_definition;
    lv_obj_t *chord_key_key;
    lv_obj_t *chord_panel_type_poll; /* 和弦类型 Panel 父容器 */
    lv_obj_t *btn_home;
} ui_screen_chord_t;

static ui_screen_chord_t s_chord_ui = {0};

static const widget_binding_t s_chord_bindings[] = {
    WIDGET_BIND(ui_screen_chord_t, canvas,           "chord_canvas_piano", WIDGET_KIND_CANVAS),
    WIDGET_BIND(ui_screen_chord_t, chord_name,       "chord_name",         WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_chord_t, chord_definition, "chord_definition",   WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_chord_t, chord_key_key,    "chord_key_key",      WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_chord_t, chord_panel_type_poll, "chord_panel_type_poll", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_chord_t, btn_home,         "chord_btn_home",     WIDGET_KIND_ANY),
    WIDGET_BINDING_END,
};

static chord_state_t s_chord = {0};

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
        lv_obj_set_style_pad_left(btn, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(btn, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn, engine_gui_theme_color(COLOR_CARD), LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(btn, engine_gui_theme_color(COLOR_TEXT_PRIMARY), LV_PART_MAIN | LV_STATE_CHECKED);

        /* 创建标签，显示 "中文名\n符号" 两行；long_mode=SCROLL 防英文长文本溢出 */
        lv_obj_t *label = lv_label_create(btn);
        char label_text[48];
        snprintf(label_text, sizeof(label_text), "%s\n%s", 
                 _(s_chord_types[i].cn_name), 
                 s_chord_types[i].suffix[0] ? s_chord_types[i].suffix : "");
        lv_label_set_text(label, label_text);
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL);
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
    return (float)CHORD_CANVAS_W / CHORD_WHITE_COUNT;
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
    const chord_type_t *ct = &s_chord_types[s_chord.type];
    float white_w = chord_white_w();
    float black_w = white_w * 0.58f;
    float black_h = CHORD_CANVAS_H * 0.62f;

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
            (int32_t)((i + 1) * white_w) - 2, CHORD_CANVAS_H - 2
        };
        lv_draw_rect(&layer, &key_dsc, &area);

        lv_draw_label_dsc_t name_dsc;
        lv_draw_label_dsc_init(&name_dsc);
        name_dsc.font = &lv_font_montserrat_14;
        name_dsc.color = engine_gui_theme_color(CHORD_C_DIM);
        name_dsc.text = white_names[i % 7];
        name_dsc.align = LV_TEXT_ALIGN_CENTER;
        lv_area_t name_area = {
            (int32_t)(i * white_w), CHORD_CANVAS_H - 22,
            (int32_t)((i + 1) * white_w), CHORD_CANVAS_H - 4
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
                chord_draw_marker(&layer, cx, CHORD_CANVAS_H - 40, is_root);
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

/* App 逻辑运行在 task_gui / task_app 上下文，绘制与 invalidate 必须持 LVGL 锁 */
static void chord_canvas_redraw(void)
{
    if (s_chord_ui.canvas == NULL) {
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

    chord_reset_timbre();

    ui_screen_chord_t *ui = (ui_screen_chord_t *)screen_ctx;

    if (ui->canvas == NULL || lv_obj_get_class(ui->canvas) != &lv_canvas_class) {
        ESP_LOGE(TAG, "canvas object invalid");
        return false;
    }

    size_t buf_size = (size_t)CHORD_CANVAS_W * CHORD_CANVAS_H * 2;
    s_chord.canvas_buf = heap_caps_calloc(1, buf_size, MALLOC_CAP_SPIRAM);
    if (s_chord.canvas_buf == NULL) {
        ESP_LOGE(TAG, "canvas buf alloc failed");
        return false;
    }

    lvgl_port_lock(portMAX_DELAY);
    lv_canvas_set_buffer(ui->canvas, s_chord.canvas_buf,
                         CHORD_CANVAS_W, CHORD_CANVAS_H,
                         LV_COLOR_FORMAT_RGB565);
    lvgl_port_unlock();

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

    /* 首帧拆分：canvas 首绘推迟 ~30ms 让背景先上屏（防 DPI underrun 闪屏） */
    if (s_canvas_defer > 0 && --s_canvas_defer == 0) {
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
    lvgl_port_unlock();

    if (s_chord.canvas_buf != NULL) {
        heap_caps_free(s_chord.canvas_buf);
        s_chord.canvas_buf = NULL;
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
        .on_ui_event = app_chord_trainer_on_ui_event,
    };
    return app_manager_register(&app);
}