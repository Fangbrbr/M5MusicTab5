/**
 * @file app_circle_of_fifths.c
 * @brief 五度圈 App：点选调号查看调性信息，点按钢琴卷试听音阶
 *
 * 点击五度圈画布按扇区命中 12 个调号，刷新调性信息；
 * 点按钢琴画布播放当前调上行音阶。五度圈和钢琴均为 Canvas 动态绘制。
 */

#include "app_circle_of_fifths.h"
#include "app_manager.h"
#include "engine_gui.h"
#include "fonts.h"
#include "engine_midi.h"
#include "service_i18n.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "app_circle_of_fifths";

/* 五度圈画布尺寸（由布局决定，此处仅为后备） */
#define FIFTH_CIRCLE_DEFAULT_W 540
#define FIFTH_CIRCLE_DEFAULT_H 540

#define FIFTH_PLAY_DELAY_MS 100
#define FIFTH_NOTE_STEP_MS 200
#define FIFTH_SCALE_NOTES 8

/* 主题配色槽位（语义见 EEZ 主题定义） */
#define FIFTH_C_BG COLOR_BG_PRIMARY
#define FIFTH_C_WHITE_KEY COLOR_BG_SECONDARY
#define FIFTH_C_DIM COLOR_TEXT_SECONDARY
#define FIFTH_C_TONE COLOR_SUCCESS
#define FIFTH_C_ROOT COLOR_ERROR
#define FIFTH_C_BLACK_KEY COLOR_SHADOW
#define FIFTH_C_TEXT COLOR_TEXT_PRIMARY

typedef struct
{
    const char *name;      /* 调名，如 "C"、"Bb" */
    uint8_t root_pc;       /* 主音音名 */
    const char *sig;       /* 调号文本，如 "0"、"1#"、"3b" */
    const char *rel_root;  /* 关系小调根音名，如 "A" */
    const char *par_minor; /* 平行小调名，如 "Cm" */
    const char *scale;     /* 音阶拼写，如 "C D E F G A B" */
} fifth_key_t;

/* 顺时针从顶部 C 开始的 12 个调号 */
static const fifth_key_t s_keys[] = {
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
#define FIFTH_KEY_COUNT (sizeof(s_keys) / sizeof(s_keys[0]))

typedef struct
{
    uint8_t note;
    int64_t on_at_us;
    int64_t off_at_us;
    bool started;
} fifth_note_slot_t;

typedef struct
{
    uint8_t index; /* 当前调号索引 0~11 */
    fifth_note_slot_t notes[FIFTH_SCALE_NOTES];
    int note_count;

    /* 钢琴画布 */
    void *piano_buf;
    int piano_w;
    int piano_h;

    /* 五度圈画布 */
    void *circle_buf;
    int circle_w;
    int circle_h;
} fifth_state_t;

typedef struct
{
    lv_obj_t *canvas_circle;
    lv_obj_t *name;
    lv_obj_t *key_sig;
    lv_obj_t *scale;
    lv_obj_t *dominant;
    lv_obj_t *subdominant;
    lv_obj_t *parallel;
    lv_obj_t *canvas_piano;
    lv_obj_t *btn_home;
} ui_screen_fifth_t;

static ui_screen_fifth_t s_fifth_ui = {0};
static fifth_state_t s_fifth = {0};

static const widget_binding_t s_fifth_bindings[] = {
    WIDGET_BIND(ui_screen_fifth_t, canvas_circle, "fifth_canvas_circle", WIDGET_KIND_CANVAS),
    WIDGET_BIND(ui_screen_fifth_t, name, "fifth_name", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_fifth_t, key_sig, "fifth_key_sig", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_fifth_t, scale, "fifth_scale", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_fifth_t, dominant, "fifth_dominant", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_fifth_t, subdominant, "fifth_subdominant", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_fifth_t, parallel, "fifth_parallel", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_fifth_t, canvas_piano, "fifth_canvas_piano", WIDGET_KIND_CANVAS),
    WIDGET_BIND(ui_screen_fifth_t, btn_home, "fifth_btn_home", WIDGET_KIND_ANY),
    WIDGET_BINDING_END,
};

/* 白键/黑键的音名映射（单八度） */
static const int8_t s_white_pc[7] = {0, 2, 4, 5, 7, 9, 11};
static const int8_t s_black_pc[5] = {1, 3, 6, 8, 10};
static const int8_t s_black_pos[5] = {0, 1, 3, 4, 5};
static const int8_t s_major_steps[7] = {0, 2, 4, 5, 7, 9, 11};

/* -------------------- 音阶播放 -------------------- */
static void fifth_midi_note(uint8_t note, uint8_t velocity)
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
static void fifth_reset_timbre(void)
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

static void fifth_stop_all_notes(void)
{
    for (int i = 0; i < s_fifth.note_count; i++)
    {
        if (s_fifth.notes[i].started)
            fifth_midi_note(s_fifth.notes[i].note, 0);
    }
    s_fifth.note_count = 0;
}

static void fifth_play_scale(void)
{
    static const int8_t steps[FIFTH_SCALE_NOTES] = {0, 2, 4, 5, 7, 9, 11, 12};
    int64_t now = esp_timer_get_time();
    fifth_stop_all_notes();
    for (int i = 0; i < FIFTH_SCALE_NOTES; i++)
    {
        fifth_note_slot_t *slot = &s_fifth.notes[s_fifth.note_count];
        slot->note = (uint8_t)(60 + s_keys[s_fifth.index].root_pc + steps[i]);
        slot->on_at_us = now + ((int64_t)FIFTH_PLAY_DELAY_MS + (int64_t)i * FIFTH_NOTE_STEP_MS) * 1000;
        slot->off_at_us = slot->on_at_us + (int64_t)FIFTH_NOTE_STEP_MS * 1000;
        slot->started = false;
        s_fifth.note_count++;
    }
}

static void fifth_process_notes(void)
{
    if (s_fifth.note_count == 0)
        return;
    int64_t now = esp_timer_get_time();
    int write = 0;
    for (int i = 0; i < s_fifth.note_count; i++)
    {
        fifth_note_slot_t *n = &s_fifth.notes[i];
        if (!n->started && now >= n->on_at_us)
        {
            fifth_midi_note(n->note, 100);
            n->started = true;
        }
        if (n->started && now >= n->off_at_us)
        {
            fifth_midi_note(n->note, 0);
        }
        else
        {
            if (write != i)
                s_fifth.notes[write] = s_fifth.notes[i];
            write++;
        }
    }
    s_fifth.note_count = write;
}

static void fifth_canvas_play_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_PRESSED)
        fifth_play_scale();
}

/* -------------------- UI 标签刷新 -------------------- */
static void fifth_refresh_labels(void)
{
    const fifth_key_t *k = &s_keys[s_fifth.index];
    char buf[64];
    lvgl_port_lock(portMAX_DELAY);

    snprintf(buf, sizeof(buf), _("%s大调 / %s小调"), k->name, k->rel_root);
    lv_label_set_text(s_fifth_ui.name, buf);

    snprintf(buf, sizeof(buf), _("调号: %s"), k->sig);
    lv_label_set_text(s_fifth_ui.key_sig, buf);

    lv_label_set_text(s_fifth_ui.scale, k->scale);

    snprintf(buf, sizeof(buf), "%s(V)", s_keys[(s_fifth.index + 1) % FIFTH_KEY_COUNT].name);
    lv_label_set_text(s_fifth_ui.dominant, buf);

    snprintf(buf, sizeof(buf), "%s(IV)", s_keys[(s_fifth.index + FIFTH_KEY_COUNT - 1) % FIFTH_KEY_COUNT].name);
    lv_label_set_text(s_fifth_ui.subdominant, buf);

    lv_label_set_text(s_fifth_ui.parallel, k->par_minor);

    lvgl_port_unlock();
}

/* -------------------- 钢琴画布绘制 -------------------- */
static int fifth_pc_to_white(int pc)
{
    for (int i = 0; i < 7; i++)
        if (s_white_pc[i] == pc)
            return i;
    return -1;
}
static int fifth_pc_to_black(int pc)
{
    for (int i = 0; i < 5; i++)
        if (s_black_pc[i] == pc)
            return i;
    return -1;
}

static void fifth_draw_marker(lv_layer_t *layer, int32_t cx, int32_t cy, bool is_root)
{
    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.color = engine_gui_theme_color(is_root ? FIFTH_C_ROOT : FIFTH_C_TONE);
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
    if (w <= 0 || h <= 0)
        return;

    float white_w = (float)w / 7.0f;
    float black_w = white_w * 0.58f;
    float black_h = h * 0.62f;

    lv_canvas_fill_bg(canvas, engine_gui_theme_color(FIFTH_C_BG), LV_OPA_COVER);
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    static const char white_names[7][2] = {"C", "D", "E", "F", "G", "A", "B"};

    // 白键
    for (int i = 0; i < 7; i++)
    {
        lv_draw_rect_dsc_t key_dsc;
        lv_draw_rect_dsc_init(&key_dsc);
        key_dsc.bg_color = engine_gui_theme_color(FIFTH_C_WHITE_KEY);
        key_dsc.bg_opa = LV_OPA_COVER;
        key_dsc.border_color = engine_gui_theme_color(FIFTH_C_DIM);
        key_dsc.border_width = 1;
        key_dsc.radius = 4;
        lv_area_t area = {
            (int32_t)(i * white_w) + 1, 1,
            (int32_t)((i + 1) * white_w) - 2, h - 2};
        lv_draw_rect(&layer, &key_dsc, &area);

        lv_draw_label_dsc_t name_dsc;
        lv_draw_label_dsc_init(&name_dsc);
        name_dsc.font = &lv_font_montserrat_14;
        name_dsc.color = engine_gui_theme_color(FIFTH_C_DIM);
        name_dsc.text = white_names[i];
        name_dsc.align = LV_TEXT_ALIGN_CENTER;
        lv_area_t name_area = {
            (int32_t)(i * white_w), h - 20,
            (int32_t)((i + 1) * white_w), h - 3};
        lv_draw_label(&layer, &name_dsc, &name_area);
    }

    // 黑键
    for (int j = 0; j < 5; j++)
    {
        float cx = (s_black_pos[j] + 1) * white_w;
        lv_draw_rect_dsc_t key_dsc;
        lv_draw_rect_dsc_init(&key_dsc);
        key_dsc.bg_color = engine_gui_theme_color(FIFTH_C_BLACK_KEY);
        key_dsc.bg_opa = LV_OPA_COVER;
        key_dsc.border_color = engine_gui_theme_color(FIFTH_C_DIM);
        key_dsc.border_width = 1;
        key_dsc.radius = 3;
        lv_area_t area = {
            (int32_t)(cx - black_w / 2), 1,
            (int32_t)(cx + black_w / 2), (int32_t)black_h};
        lv_draw_rect(&layer, &key_dsc, &area);
    }

    // 标记音级
    uint8_t root_pc = s_keys[s_fifth.index].root_pc;
    for (int t = 0; t < 7; t++)
    {
        int pc = (root_pc + s_major_steps[t]) % 12;
        bool is_root = (t == 0);
        int wi = fifth_pc_to_white(pc);
        if (wi >= 0)
        {
            int32_t cx = (int32_t)((wi + 0.5f) * white_w);
            fifth_draw_marker(&layer, cx, h - 34, is_root);
        }
        else
        {
            int bi = fifth_pc_to_black(pc);
            if (bi >= 0)
            {
                int32_t cx = (int32_t)((s_black_pos[bi] + 1) * white_w);
                fifth_draw_marker(&layer, cx, (int32_t)black_h - 14, is_root);
            }
        }
    }

    lv_canvas_finish_layer(canvas, &layer);
}

static void fifth_piano_redraw(void)
{
    if (!s_fifth_ui.canvas_piano || !s_fifth.piano_buf)
        return;
    lvgl_port_lock(portMAX_DELAY);
    fifth_draw_piano_canvas(s_fifth_ui.canvas_piano);
    lv_obj_clear_flag(s_fifth_ui.canvas_piano, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(s_fifth_ui.canvas_piano);
    lvgl_port_unlock();
}

/* 确保钢琴卷帘 canvas buffer 与控件当前尺寸一致。
 * 返回 true 表示新建/重建了 buffer，调用方需重绘。
 * Trap: 首次进屏时布局可能尚未收敛，直接读尺寸会拿到预布局默认值，
 * 分配出偏小 buffer；LVGL 9 canvas 将 buffer 居中绘制，表现为钢琴被
 * 缩小挤在中间一小块。因此读尺寸前强制布局收敛，尺寸变化时重分配。 */
static bool fifth_piano_ensure_buffer(void)
{
    if (!s_fifth_ui.canvas_piano)
        return false;

    lvgl_port_lock(portMAX_DELAY);
    lv_obj_update_layout(s_fifth_ui.canvas_piano);
    int w = lv_obj_get_width(s_fifth_ui.canvas_piano);
    int h = lv_obj_get_height(s_fifth_ui.canvas_piano);
    lvgl_port_unlock();
    if (w <= 0 || h <= 0)
        return false;

    if (s_fifth.piano_buf && s_fifth.piano_w == w && s_fifth.piano_h == h)
        return false;

    void *buf = heap_caps_calloc(1, (size_t)w * h * 2, MALLOC_CAP_SPIRAM);
    if (!buf)
    {
        ESP_LOGE(TAG, "piano canvas alloc failed");
        return false;
    }

    void *old = s_fifth.piano_buf;
    lvgl_port_lock(portMAX_DELAY);
    lv_canvas_set_buffer(s_fifth_ui.canvas_piano, buf, w, h, LV_COLOR_FORMAT_RGB565);
    if (old)
        heap_caps_free(old);
    lvgl_port_unlock();
    s_fifth.piano_buf = buf;
    s_fifth.piano_w = w;
    s_fifth.piano_h = h;
    ESP_LOGI(TAG, "piano canvas %dx%d", w, h);
    return true;
}

/* -------------------- 五度圈画布绘制 -------------------- */
static void fifth_draw_circle_canvas(lv_obj_t *canvas)
{
    int w = s_fifth.circle_w, h = s_fifth.circle_h;
    if (w <= 0 || h <= 0) return;

    lv_canvas_fill_bg(canvas, engine_gui_theme_color(FIFTH_C_BG), LV_OPA_COVER);
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    int cx = w / 2, cy = h / 2;
    int outer_radius = (w < h ? w : h) / 2 - 50;
    int inner_radius = outer_radius - 80;
    if (outer_radius < 70) outer_radius = 70;
    if (inner_radius < 30) inner_radius = 30;

    lv_color_t color_dim      = engine_gui_theme_color(FIFTH_C_DIM);
    lv_color_t color_selected = engine_gui_theme_color(COLOR_PRIMARY);   // 根据实际主题调整
    lv_color_t color_neighbor = engine_gui_theme_color(COLOR_SECONDARY);
    lv_color_t color_default  = engine_gui_theme_color(COLOR_CARD);

    /* ----- 1. 绘制 12 扇区（中心 ±15°） ----- */
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

    /* ----- 2. 外圈分隔线 ----- */
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

    /* ----- 3. 外圈大调名 + 内圈关系小调名 ----- */
    static char minor_names[12][8];
    for (int i = 0; i < 12; i++) {
        double angle_rad = (-90.0 + i * 30.0) * M_PI / 180.0;
        int x_out = cx + (int)((outer_radius + 30) * cos(angle_rad));
        int y_out = cy + (int)((outer_radius + 30) * sin(angle_rad));
        int x_in  = cx + (int)((outer_radius - 50) * cos(angle_rad));
        int y_in  = cy + (int)((outer_radius - 50) * sin(angle_rad));

        bool is_highlight = (i == s_fifth.index);

        lv_draw_label_dsc_t label_dsc;
        lv_draw_label_dsc_init(&label_dsc);

        /* 外圈大调 */
        label_dsc.color = engine_gui_theme_color(is_highlight ? FIFTH_C_ROOT : FIFTH_C_DIM);
        label_dsc.font = ui_font_chinese_40;
        label_dsc.text = s_keys[i].name;
        label_dsc.align = LV_TEXT_ALIGN_CENTER;
        lv_area_t area_out = { x_out - 40, y_out - 25, x_out + 40, y_out + 25 };
        lv_draw_label(&layer, &label_dsc, &area_out);

        /* 内圈关系小调 */
        snprintf(minor_names[i], sizeof(minor_names[i]), "%sm", s_keys[i].rel_root);
        label_dsc.color = engine_gui_theme_color(is_highlight ? FIFTH_C_ROOT : FIFTH_C_DIM);
        label_dsc.font = ui_font_chinese_30;
        label_dsc.text = minor_names[i];
        label_dsc.align = LV_TEXT_ALIGN_CENTER;
        lv_area_t area_in = { x_in - 40, y_in - 20, x_in + 40, y_in + 20 };
        lv_draw_label(&layer, &label_dsc, &area_in);
    }

    /* ----- 4. 中心装饰圆 ----- */
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
    if (!s_fifth_ui.canvas_circle || !s_fifth.circle_buf)
        return;
    lvgl_port_lock(portMAX_DELAY);
    fifth_draw_circle_canvas(s_fifth_ui.canvas_circle);
    lv_obj_clear_flag(s_fifth_ui.canvas_circle, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(s_fifth_ui.canvas_circle);
    lvgl_port_unlock();
}

/* 五度圈 canvas buffer 确保逻辑，同 fifth_piano_ensure_buffer */
static bool fifth_circle_ensure_buffer(void)
{
    if (!s_fifth_ui.canvas_circle)
        return false;

    lvgl_port_lock(portMAX_DELAY);
    lv_obj_update_layout(s_fifth_ui.canvas_circle);
    int w = lv_obj_get_width(s_fifth_ui.canvas_circle);
    int h = lv_obj_get_height(s_fifth_ui.canvas_circle);
    lvgl_port_unlock();
    if (w <= 0 || h <= 0)
        return false;

    if (s_fifth.circle_buf && s_fifth.circle_w == w && s_fifth.circle_h == h)
        return false;

    void *buf = heap_caps_calloc(1, (size_t)w * h * 2, MALLOC_CAP_SPIRAM);
    if (!buf)
    {
        ESP_LOGE(TAG, "circle canvas alloc failed");
        return false;
    }

    void *old = s_fifth.circle_buf;
    lvgl_port_lock(portMAX_DELAY);
    lv_canvas_set_buffer(s_fifth_ui.canvas_circle, buf, w, h, LV_COLOR_FORMAT_RGB565);
    if (old)
        heap_caps_free(old);
    lvgl_port_unlock();
    s_fifth.circle_buf = buf;
    s_fifth.circle_w = w;
    s_fifth.circle_h = h;
    ESP_LOGI(TAG, "circle canvas %dx%d", w, h);
    return true;
}

/* -------------------- 事件与生命周期 -------------------- */

/* 首帧拆分倒计时（on_update 周期数）：canvas 分配/绘制推迟 ~30ms，让全屏
 * 背景先上屏；同帧叠加 全屏重绘+两个大 canvas+PPA 全帧旋转 会瞬时挤爆
 * PSRAM 总线触发 DPI underrun 闪屏。切屏时 canvas 已被 engine_gui 统一
 * 隐藏，推迟期间不会显示未初始化缓冲。 */
static uint8_t s_canvas_defer = 0;
/* 首绘完成标志：尺寸需连续两拍不变才上屏，防布局未收敛先画小卷帘再闪大 */
static bool s_canvas_ready = false;

static void app_circle_of_fifths_home_cb(lv_event_t *e)
{
    (void)e;
    app_manager_request_kill_active();
}

static bool app_circle_of_fifths_on_init(app_base_t *self, void *screen_ctx)
{
    (void)self;
    (void)screen_ctx;
    ESP_LOGI(TAG, "init");
    memset(&s_fifth, 0, sizeof(s_fifth));

    fifth_reset_timbre();

    lvgl_port_lock(portMAX_DELAY);
    if (s_fifth_ui.canvas_piano)
    {
        lv_obj_add_flag(s_fifth_ui.canvas_piano, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_fifth_ui.canvas_piano, fifth_canvas_play_cb, LV_EVENT_PRESSED, NULL);
    }
    if (s_fifth_ui.btn_home)
    {
        lv_obj_add_event_cb(s_fifth_ui.btn_home, app_circle_of_fifths_home_cb, LV_EVENT_CLICKED, NULL);
    }
    lvgl_port_unlock();

    // 两个画布的首绘推迟到 on_update 倒计时结束（首帧拆分，见 s_canvas_defer）
    s_canvas_defer = 3;
    s_canvas_ready = false;

    fifth_refresh_labels();
    return true;
}

static void app_circle_of_fifths_on_input(app_base_t *self, const app_input_event_t *evt)
{
    (void)self;
    if (evt->type != APP_INPUT_TOUCH_DOWN)
        return;

    // 动态获取五度圈画布的中心和半径
    lvgl_port_lock(portMAX_DELAY);
    lv_area_t area;
    lv_obj_get_coords(s_fifth_ui.canvas_circle, &area);
    lvgl_port_unlock();
    int cx = (area.x1 + area.x2) / 2;
    int cy = (area.y1 + area.y2) / 2;
    int radius = (area.x2 - area.x1) / 2 - 10; // 留边距

    int dx = (int)evt->x - cx;
    int dy = (int)evt->y - cy;
    if (dx * dx + dy * dy > radius * radius)
        return;

    double ang = atan2((double)dx, (double)(-dy)) * 180.0 / M_PI;
    if (ang < 0)
        ang += 360.0;
    int idx = ((int)(ang + 15.0)) / 30 % FIFTH_KEY_COUNT;

    if (idx == s_fifth.index)
        return;
    s_fifth.index = (uint8_t)idx;
    ESP_LOGI(TAG, "key=%s", s_keys[idx].name);

    fifth_refresh_labels();
    fifth_piano_redraw();
    fifth_circle_redraw();
}

static void app_circle_of_fifths_on_update(app_base_t *self)
{
    (void)self;
    fifth_process_notes();

    if (!s_canvas_ready) {
        /* 首帧拆分 + 尺寸两拍收敛：推迟期内只倒计时；之后每拍 ensure，
         * 尺寸刚变化（含首次分配）就再等一拍，连续两拍尺寸不变才绘制
         * 上屏——避免布局未收敛时先画小卷帘再闪成大卷帘 */
        if (s_canvas_defer > 0) {
            s_canvas_defer--;
            return;
        }
        /* EEZ 网格 STRETCH 控件的最终尺寸只有在本屏成为活动屏后由网格布局
         * 解析；未上屏时量到的只是设计占位值（piano 卷帘 180x100→600x219
         * 闪变的根因，screens.c:4542 设计值 + STRETCH）。等本屏上屏再测量。 */
        lvgl_port_lock(portMAX_DELAY);
        bool screen_active = (s_fifth_ui.canvas_piano != NULL) &&
                             (lv_obj_get_screen(s_fifth_ui.canvas_piano) == lv_screen_active());
        lvgl_port_unlock();
        if (!screen_active) {
            return;
        }
        bool piano_new = fifth_piano_ensure_buffer();
        bool circle_new = fifth_circle_ensure_buffer();
        if (piano_new || circle_new) {
            return;
        }
        if (s_fifth.piano_buf != NULL && s_fifth.circle_buf != NULL) {
            fifth_piano_redraw();
            fifth_circle_redraw();
            s_canvas_ready = true;
        }
        return;
    }

    /* ensure 返回 true 表示新建/重建了 buffer（尺寸修正），需重绘；
     * 尺寸未变时不重绘，避免每周期重复绘制 */
    if (fifth_piano_ensure_buffer())
        fifth_piano_redraw();
    if (fifth_circle_ensure_buffer())
        fifth_circle_redraw();
}

static void app_circle_of_fifths_on_pause(app_base_t *self)
{
    (void)self;
    fifth_stop_all_notes();
    ESP_LOGI(TAG, "pause");
}

static void app_circle_of_fifths_on_resume(app_base_t *self)
{
    (void)self;
    ESP_LOGI(TAG, "resume");
    if (s_fifth.piano_buf)
        fifth_piano_redraw();
    if (s_fifth.circle_buf)
        fifth_circle_redraw();
}

static void app_circle_of_fifths_on_destroy(app_base_t *self)
{
    (void)self;
    ESP_LOGI(TAG, "destroy");
    fifth_stop_all_notes();

    lvgl_port_lock(portMAX_DELAY);
    if (s_fifth_ui.canvas_piano)
        lv_obj_remove_event_cb(s_fifth_ui.canvas_piano, fifth_canvas_play_cb);
    if (s_fifth_ui.btn_home)
        lv_obj_remove_event_cb(s_fifth_ui.btn_home, app_circle_of_fifths_home_cb);
    lvgl_port_unlock();

    if (s_fifth.piano_buf)
    {
        heap_caps_free(s_fifth.piano_buf);
        s_fifth.piano_buf = NULL;
    }
    if (s_fifth.circle_buf)
    {
        heap_caps_free(s_fifth.circle_buf);
        s_fifth.circle_buf = NULL;
    }
}

esp_err_t app_circle_of_fifths_register(void)
{
    static app_base_t app = {
        .name = "Circle Of Fifths",
        .screen_name = "app_circle_of_fifths",
        .screen_ctx = &s_fifth_ui,
        .screen_ctx_size = sizeof(s_fifth_ui),
        .widget_bindings = s_fifth_bindings,
        .on_init = app_circle_of_fifths_on_init,
        .on_update = app_circle_of_fifths_on_update,
        .on_pause = app_circle_of_fifths_on_pause,
        .on_resume = app_circle_of_fifths_on_resume,
        .on_destroy = app_circle_of_fifths_on_destroy,
        .on_input = app_circle_of_fifths_on_input,
    };
    return app_manager_register(&app);
}