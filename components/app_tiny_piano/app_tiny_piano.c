/**
 * @file app_tiny_piano.c
 * @brief 小钢琴 App：音阶矩阵垫与虚拟钢琴双布局
 *
 * 矩阵模式：15 个垫按当前音阶规则发音，基准随根音音名移调；
 * 钢琴模式：piano_root_v 选起始八度（C0~C6），canvas 绘制双八度琴键，点按发音。
 * 两种模式均支持多点滑动命中：按住滑过即发音、划走/抬手即 note-off。
 * 布局经 piano_display_type 下拉互斥显示；配色全部取自 EEZ 主题数组。
 */

#include "app_tiny_piano.h"
#include "app_manager.h"
#include "engine_gui.h"
#include "engine_midi.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lvgl_port.h"
#include "service_nvs.h"
#include "service_i18n.h"
#include "lvgl.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "app_tiny_piano";

#define PIANO_PAD_COUNT     15
#define PIANO_TOUCH_MAX_FINGERS 5
#define PIANO_DEFAULT_VELOCITY 100

/* 主题配色槽位（语义见 EEZ 主题定义） */
#define PIANO_C_BG          COLOR_BG_PRIMARY
#define PIANO_C_WHITE_KEY   COLOR_BG_SECONDARY
#define PIANO_C_CARD        COLOR_CARD
#define PIANO_C_DIM         COLOR_SHADOW
#define PIANO_C_BLACK_KEY   COLOR_TEXT_SECONDARY

typedef struct {
    const int8_t *steps;
    uint8_t len;
} piano_scale_t;

/* 大调 / 小调 / 中国五声 / 埃及调式 / 多利亚 / 日本调式
 * 顺序与 EEZ 下拉 piano_scale_type 选项严格一一对应，改动必须同步两侧 */
static const int8_t s_steps_major[]      = {0, 2, 4, 5, 7, 9, 11};
static const int8_t s_steps_minor[]      = {0, 2, 3, 5, 7, 8, 10};
static const int8_t s_steps_china_pent[] = {0, 2, 4, 7, 9};
static const int8_t s_steps_egypt[]      = {0, 2, 5, 7, 10};
static const int8_t s_steps_dorian[]     = {0, 2, 3, 5, 7, 9, 10};
static const int8_t s_steps_japan[]      = {0, 1, 5, 7, 8};

static const piano_scale_t s_scales[] = {
    { s_steps_major,      7 },
    { s_steps_minor,      7 },
    { s_steps_china_pent, 5 },
    { s_steps_egypt,      5 },
    { s_steps_dorian,     7 },
    { s_steps_japan,      5 },
};
#define PIANO_SCALE_COUNT  (sizeof(s_scales) / sizeof(s_scales[0]))

typedef struct {
    lv_obj_t *panel_m;
    lv_obj_t *pads[PIANO_PAD_COUNT];
    lv_obj_t *panel_v;
    lv_obj_t *root_v;
    lv_obj_t *canvas;
    lv_obj_t *display_type;
    lv_obj_t *scale_type;
    lv_obj_t *pitch;     /* 键盘根音音名下拉框（C~B + 高八度 C） */
    lv_obj_t *sound_type;  /* 新增：钢琴音色选择下拉框 */
    lv_obj_t *btn_home;
    lv_obj_t *btn_rec;
    lv_obj_t *btn_set;
    lv_obj_t *set;
    lv_obj_t *set_btn_return;
} ui_screen_piano_t;

static ui_screen_piano_t s_piano_ui = {0};

static const widget_binding_t s_piano_bindings[] = {
    WIDGET_BIND(ui_screen_piano_t, panel_m,      "piano_panel_m",      WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_piano_t, pads[0],      "piano_pad0",         WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_piano_t, pads[1],      "piano_pad1",         WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_piano_t, pads[2],      "piano_pad2",         WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_piano_t, pads[3],      "piano_pad3",         WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_piano_t, pads[4],      "piano_pad4",         WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_piano_t, pads[5],      "piano_pad5",         WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_piano_t, pads[6],      "piano_pad6",         WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_piano_t, pads[7],      "piano_pad7",         WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_piano_t, pads[8],      "piano_pad8",         WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_piano_t, pads[9],      "piano_pad9",         WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_piano_t, pads[10],     "piano_pad10",        WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_piano_t, pads[11],     "piano_pad11",        WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_piano_t, pads[12],     "piano_pad12",        WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_piano_t, pads[13],     "piano_pad13",        WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_piano_t, pads[14],     "piano_pad14",        WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_piano_t, panel_v,      "piano_panel_v",      WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_piano_t, root_v,       "piano_root_v",       WIDGET_KIND_ROLLER),
    WIDGET_BIND(ui_screen_piano_t, canvas,       "piano_canvas_key",   WIDGET_KIND_CANVAS),
    WIDGET_BIND(ui_screen_piano_t, display_type, "piano_display_type", WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_piano_t, scale_type,   "piano_scale_type",   WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_piano_t, pitch,        "piano_pitch",        WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_piano_t, sound_type,   "piano_sound_type",   WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_piano_t, btn_home,     "piano_btn_home",     WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_piano_t, btn_rec,      "piano_btn_rec",      WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_piano_t, btn_set,      "piano_btn_set",      WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_piano_t, set,          "piano_set",          WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_piano_t, set_btn_return, "piano_set_btn_return", WIDGET_KIND_ANY),
    WIDGET_BINDING_END,
};

typedef struct {
    uint8_t display;    /* 0 矩阵 1 钢琴 */
    uint8_t scale;      /* 0~4 */
    uint8_t root_oct;   /* 0~6，默认 3（C3） */
    uint8_t pitch;      /* 键盘根音音名 0~12，默认 0（C），12=高八度 C */
    uint8_t sound_type; /* 0~15，钢琴音色选择 */
    uint8_t finger_note[PIANO_TOUCH_MAX_FINGERS];
    int8_t finger_pad[PIANO_TOUCH_MAX_FINGERS];   /* 各手指当前命中的矩阵垫，-1 无 */
    lv_area_t pad_area[PIANO_PAD_COUNT];          /* 矩阵垫屏幕坐标缓存 */
    bool pad_area_ready;
    uint16_t glide_pad_mask;          /* 滑动命中高亮掩码（on_input 写） */
    uint16_t glide_pad_mask_applied;  /* 已应用到 LVGL 的掩码（on_update 写） */
    void *canvas_buf;
    int canvas_w;
    int canvas_h;
    int canvas_x;
    int canvas_y;
    bool recording_self;        /* 本 App 发起的录制 */
    bool recording_stop_pending;  /* 录制已停止，等待 finalize 后提示 */
} piano_state_t;

static piano_state_t s_piano = {0};

/* 白键/黑键的音名映射（单八度） */
static const int8_t s_white_pc[7] = {0, 2, 4, 5, 7, 9, 11};
static const int8_t s_black_pc[5] = {1, 3, 6, 8, 10};
static const int8_t s_black_pos[5] = {0, 1, 3, 4, 5};

/* -------------------- 音色切换 -------------------- */

/**
 * @brief 切换到指定钢琴音色（通过 MIDI Program Change）
 * 
 * @param sound_type 音色索引 0~15
 */
static void piano_set_sound_type(uint8_t sound_type) {
    if (sound_type > 15) {
        ESP_LOGW(TAG, "invalid sound type %d", sound_type);
        return;
    }
    
    /* 确保 Bank LSB=0 */
    engine_midi_event_t evt = {
        .type = ENGINE_MIDI_MSG_CONTROL_CHANGE,
        .channel = 0,
        .data1 = 0,   /* CC0 */
        .data2 = 0,   /* Bank LSB = 0 */
    };
    engine_midi_publish(&evt, pdMS_TO_TICKS(5));
    
    /* 确保 Bank MSB=0 */
    evt.data1 = 32;  /* CC32 */
    evt.data2 = 0;   /* Bank MSB = 0 */
    engine_midi_publish(&evt, pdMS_TO_TICKS(5));
    
    /* Program Change */
    evt.type = ENGINE_MIDI_MSG_PROGRAM_CHANGE;
    evt.channel = 0;
    evt.data1 = sound_type;
    engine_midi_publish(&evt, pdMS_TO_TICKS(5));
    
    ESP_LOGI(TAG, "Switch to sound type %d", sound_type);
}

/* -------------------- 参数持久化 -------------------- */

static void piano_load_params(void)
{
    service_nvs_piano_t params;
    service_nvs_get_piano(&params);

    s_piano.display = params.display;
    if (s_piano.display > 1) {
        s_piano.display = 0;
    }

    s_piano.scale = params.scale;
    if (s_piano.scale >= PIANO_SCALE_COUNT) {
        s_piano.scale = 0;
    }

    s_piano.root_oct = params.root_oct;
    if (s_piano.root_oct > 6) {
        s_piano.root_oct = 3;
    }

    s_piano.pitch = params.pitch;
    if (s_piano.pitch > 12) {
        s_piano.pitch = 0;
    }

    s_piano.sound_type = params.sound_type;
    if (s_piano.sound_type > 15) {
        s_piano.sound_type = 0;
    }
}

static void piano_save_params(void)
{
    service_nvs_piano_t params = {
        .display = s_piano.display,
        .scale = s_piano.scale,
        .root_oct = s_piano.root_oct,
        .pitch = s_piano.pitch,
        .sound_type = s_piano.sound_type,
    };
    service_nvs_set_piano(&params);
}

static void piano_midi_note(uint8_t note, uint8_t velocity)
{
    engine_midi_event_t midi = {0};
    midi.type = (velocity > 0) ? ENGINE_MIDI_MSG_NOTE_ON : ENGINE_MIDI_MSG_NOTE_OFF;
    midi.channel = 0;
    midi.data1 = note;
    midi.data2 = velocity;
    midi.source_port = ENGINE_MIDI_PORT_APP;
    engine_midi_publish(&midi, 0);
}

/* 触摸力度带往 MIDI velocity；无力度事件（pressure=0）回退默认值保手感 */
static uint8_t piano_velocity(uint8_t pressure)
{
    return (pressure > 0) ? pressure : PIANO_DEFAULT_VELOCITY;
}

/* 全部滑行音符立即 note-off（退出/暂停/切布局时调用） */
static void piano_glide_stop_all(void)
{
    for (int f = 0; f < PIANO_TOUCH_MAX_FINGERS; f++) {
        if (s_piano.finger_note[f] != 0) {
            piano_midi_note(s_piano.finger_note[f], 0);
            s_piano.finger_note[f] = 0;
        }
        s_piano.finger_pad[f] = -1;
    }
    s_piano.glide_pad_mask = 0;
}

/* -------------------- 矩阵垫 -------------------- */

/* 矩阵垫基准音 C3，随根音音名移调（pitch=12 即升一个八度），按音阶规则展开 */
static uint8_t piano_pad_note(int idx)
{
    const piano_scale_t *sc = &s_scales[s_piano.scale];
    return (uint8_t)(48 + s_piano.pitch + 12 * (idx / sc->len) + sc->steps[idx % sc->len]);
}

static void piano_refresh_pads(void)
{
    static const char names[12][3] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    lvgl_port_lock(portMAX_DELAY);
    const piano_scale_t *sc = &s_scales[s_piano.scale];
    for (int i = 0; i < PIANO_PAD_COUNT; i++) {
        lv_obj_t *pad = s_piano_ui.pads[i];
        if (pad == NULL) {
            continue;
        }

        uint8_t note = piano_pad_note(i);
        /* Do（调式根音）高亮：取当前调式的 step 0，与 pitch 无关 */
        bool is_do = (sc->steps[i % sc->len] == 0);
        uint32_t slot = is_do ? PIANO_C_WHITE_KEY : PIANO_C_CARD;
        lv_obj_set_style_bg_color(pad, engine_gui_theme_color((uint8_t)slot),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *label = lv_obj_get_child(pad, 0);
        if (label != NULL) {
            char text[8];
            snprintf(text, sizeof(text), "%s%d", names[note % 12], (int)(note / 12) - 1);
            lv_label_set_text(label, text);
        }
    }
    lvgl_port_unlock();
}

/* -------------------- 矩阵垫滑动命中 -------------------- */

/* 矩阵垫屏幕坐标缓存：隐藏面板不参与布局，必须在面板可见且布局收敛后读取，
 * 否则命中区域与视觉位置偏离（与 canvas ensure_buffer 同一陷阱） */
static bool piano_matrix_ensure_coords(void)
{
    if (s_piano_ui.panel_m == NULL ||
        lv_obj_has_flag(s_piano_ui.panel_m, LV_OBJ_FLAG_HIDDEN)) {
        return false;
    }

    lvgl_port_lock(portMAX_DELAY);
    lv_obj_update_layout(s_piano_ui.panel_m);
    for (int i = 0; i < PIANO_PAD_COUNT; i++) {
        if (s_piano_ui.pads[i] != NULL) {
            lv_obj_get_coords(s_piano_ui.pads[i], &s_piano.pad_area[i]);
        }
    }
    lvgl_port_unlock();

    s_piano.pad_area_ready = true;
    return true;
}

/* 命中矩阵垫返回垫序号，未命中（缝隙/界外）返回 -1 */
static int piano_matrix_hit_pad(int16_t x, int16_t y)
{
    for (int i = 0; i < PIANO_PAD_COUNT; i++) {
        if (s_piano_ui.pads[i] == NULL) {
            continue;
        }
        const lv_area_t *a = &s_piano.pad_area[i];
        if (x >= a->x1 && x <= a->x2 && y >= a->y1 && y <= a->y2) {
            return i;
        }
    }
    return -1;
}

/* 由 finger_pad 汇总高亮掩码：任一手指按住的垫都点亮 */
static void piano_matrix_sync_glide_mask(void)
{
    uint16_t mask = 0;
    for (int f = 0; f < PIANO_TOUCH_MAX_FINGERS; f++) {
        if (s_piano.finger_pad[f] >= 0) {
            mask |= (uint16_t)(1U << s_piano.finger_pad[f]);
        }
    }
    s_piano.glide_pad_mask = mask;
}

static void piano_matrix_touch(const app_input_event_t *evt)
{
    if (!s_piano.pad_area_ready) {
        return;
    }

    uint8_t old_note = s_piano.finger_note[evt->finger_id];

    if (evt->type == APP_INPUT_TOUCH_UP) {
        if (old_note != 0) {
            piano_midi_note(old_note, 0);
            s_piano.finger_note[evt->finger_id] = 0;
        }
        s_piano.finger_pad[evt->finger_id] = -1;
        piano_matrix_sync_glide_mask();
        return;
    }

    int pad = piano_matrix_hit_pad(evt->x, evt->y);
    uint8_t new_note = (pad >= 0) ? piano_pad_note(pad) : 0;

    if (new_note != old_note) {
        /* 滑入新垫发音、划走/滑出立即 note-off，按住不动则音符延续 */
        if (old_note != 0) {
            piano_midi_note(old_note, 0);
        }
        if (new_note != 0) {
            piano_midi_note(new_note, piano_velocity(evt->pressure));
        }
        s_piano.finger_note[evt->finger_id] = new_note;
    }
    s_piano.finger_pad[evt->finger_id] = pad;
    piano_matrix_sync_glide_mask();
}

/* -------------------- 钢琴 canvas -------------------- */

static void piano_draw_canvas(lv_obj_t *canvas)
{
    int w = s_piano.canvas_w;
    int h = s_piano.canvas_h;
    if (w <= 0 || h <= 0) {
        return;
    }

    float white_w = (float)w / 14.0f;
    float black_w = white_w * 0.58f;
    float black_h = h * 0.62f;

    lv_canvas_fill_bg(canvas, engine_gui_theme_color(PIANO_C_BG), LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    /* 卷帘模式根音固定为 C，标签恒显示 C */
    const char *root_name = "C";

    for (int i = 0; i < 14; i++) {
        lv_draw_rect_dsc_t key_dsc;
        lv_draw_rect_dsc_init(&key_dsc);
        key_dsc.bg_color = engine_gui_theme_color(PIANO_C_WHITE_KEY);
        key_dsc.bg_opa = LV_OPA_COVER;
        key_dsc.border_color = engine_gui_theme_color(PIANO_C_DIM);
        key_dsc.border_width = 1;
        key_dsc.border_opa = LV_OPA_COVER;
        key_dsc.radius = 4;

        lv_area_t area = {
            (int32_t)(i * white_w) + 1, 1,
            (int32_t)((i + 1) * white_w) - 2, h - 2
        };
        lv_draw_rect(&layer, &key_dsc, &area);

        if (i % 7 == 0) {
            lv_draw_label_dsc_t name_dsc;
            lv_draw_label_dsc_init(&name_dsc);
            name_dsc.font = &lv_font_montserrat_14;
            name_dsc.color = engine_gui_theme_color(PIANO_C_DIM);
            name_dsc.text = root_name;
            name_dsc.align = LV_TEXT_ALIGN_CENTER;
            lv_area_t name_area = {
                (int32_t)(i * white_w), h - 20,
                (int32_t)((i + 1) * white_w), h - 3
            };
            lv_draw_label(&layer, &name_dsc, &name_area);
        }
    }

    for (int j = 0; j < 10; j++) {
        float cx = (j / 5 * 7 + s_black_pos[j % 5] + 1) * white_w;

        lv_draw_rect_dsc_t key_dsc;
        lv_draw_rect_dsc_init(&key_dsc);
        key_dsc.bg_color = engine_gui_theme_color(PIANO_C_BLACK_KEY);
        key_dsc.bg_opa = LV_OPA_COVER;
        key_dsc.border_color = engine_gui_theme_color(PIANO_C_DIM);
        key_dsc.border_width = 1;
        key_dsc.border_opa = LV_OPA_COVER;
        key_dsc.radius = 3;

        lv_area_t area = {
            (int32_t)(cx - black_w / 2), 1,
            (int32_t)(cx + black_w / 2), (int32_t)black_h
        };
        lv_draw_rect(&layer, &key_dsc, &area);
    }

    lv_canvas_finish_layer(canvas, &layer);
}

static void piano_canvas_redraw(void)
{
    if (s_piano_ui.canvas == NULL || s_piano.canvas_buf == NULL) {
        return;
    }
    lvgl_port_lock(portMAX_DELAY);
    piano_draw_canvas(s_piano_ui.canvas);
    lv_obj_clear_flag(s_piano_ui.canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(s_piano_ui.canvas);
    lvgl_port_unlock();
}

/* canvas 尺寸与屏幕坐标由布局决定，运行期读取后再分配 buffer。
 * 返回 true 表示新建/重建了 buffer（或坐标变化），调用方需重绘。
 * Trap: panel_v 是 FLEX 布局且初始隐藏，隐藏面板不参与布局，此时读 coords
 * 会拿到 flex 安置前的位置，导致触摸命中区域与视觉位置偏离（卷帘显示在
 * 下半屏而触发区在上半屏）。因此仅在面板可见时读取，并强制布局收敛；
 * 尺寸/位置变化时重新分配并更新触摸原点。 */
static bool piano_canvas_ensure_buffer(void)
{
    if (s_piano_ui.canvas == NULL) {
        return false;
    }
    if (s_piano_ui.panel_v != NULL &&
        lv_obj_has_flag(s_piano_ui.panel_v, LV_OBJ_FLAG_HIDDEN)) {
        return false;
    }

    lv_area_t coords;
    lvgl_port_lock(portMAX_DELAY);
    lv_obj_update_layout(s_piano_ui.canvas);
    lv_obj_get_coords(s_piano_ui.canvas, &coords);
    lvgl_port_unlock();

    int w = (int)lv_area_get_width(&coords);
    int h = (int)lv_area_get_height(&coords);
    if (w <= 0 || h <= 0) {
        return false;
    }

    if (s_piano.canvas_buf != NULL && s_piano.canvas_w == w && s_piano.canvas_h == h &&
        s_piano.canvas_x == coords.x1 && s_piano.canvas_y == coords.y1) {
        return false;
    }

    void *buf = heap_caps_calloc(1, (size_t)w * h * 2, MALLOC_CAP_SPIRAM);
    if (buf == NULL) {
        ESP_LOGE(TAG, "canvas buf alloc failed");
        return false;
    }

    void *old = s_piano.canvas_buf;
    lvgl_port_lock(portMAX_DELAY);
    lv_canvas_set_buffer(s_piano_ui.canvas, buf, (uint16_t)w, (uint16_t)h,
                         LV_COLOR_FORMAT_RGB565);
    if (old != NULL) {
        heap_caps_free(old);
    }
    lvgl_port_unlock();

    s_piano.canvas_buf = buf;
    s_piano.canvas_w = w;
    s_piano.canvas_h = h;
    s_piano.canvas_x = coords.x1;
    s_piano.canvas_y = coords.y1;
    ESP_LOGI(TAG, "canvas buffer set %dx%d at %d,%d", w, h, s_piano.canvas_x, s_piano.canvas_y);
    return true;
}

/* 点按命中琴键返回音符，未命中返回 0 */
static uint8_t piano_key_hit_note(int16_t x, int16_t y)
{
    int w = s_piano.canvas_w;
    int h = s_piano.canvas_h;
    float white_w = (float)w / 14.0f;
    float black_w = white_w * 0.58f;
    float black_h = h * 0.62f;
    /* 根音固定为 C，仅由 root_oct 决定八度；pitch 不影响卷帘模式 */
    uint8_t scroll_base = (uint8_t)(12 * (s_piano.root_oct + 1));

    if (y >= 0 && y < (int)black_h) {
        for (int j = 0; j < 10; j++) {
            float cx = (j / 5 * 7 + s_black_pos[j % 5] + 1) * white_w;
            if (x >= (int32_t)(cx - black_w / 2) && x <= (int32_t)(cx + black_w / 2)) {
                return (uint8_t)(scroll_base + 12 * (j / 5) + s_black_pc[j % 5]);
            }
        }
    }

    int wi = (int)(x / white_w);
    if (x >= 0 && wi >= 0 && wi < 14 && y >= 0 && y < h) {
        /* 卷帘模式根音永远基于 C，不受 piano_pitch 影响；
         * 仅由 root_oct（八度）决定基准音高 */
        uint8_t scroll_base = (uint8_t)(12 * (s_piano.root_oct + 1));
        return (uint8_t)(scroll_base + 12 * (wi / 7) + s_white_pc[wi % 7]);
    }
    return 0;
}

/* -------------------- 布局切换 -------------------- */

static void piano_apply_display(void)
{
    lvgl_port_lock(portMAX_DELAY);
    if (s_piano.display == 0) {
        lv_obj_clear_flag(s_piano_ui.panel_m, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_piano_ui.panel_v, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_piano_ui.panel_m, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_piano_ui.panel_v, LV_OBJ_FLAG_HIDDEN);
    }
    lvgl_port_unlock();

    /* 显隐切换后矩阵垫坐标缓存失效，on_update 中按需重建 */
    s_piano.pad_area_ready = false;
}

/* -------------------- 生命周期与事件 -------------------- */

static void app_tiny_piano_home_cb(lv_event_t *e);
static void app_tiny_piano_rec_btn_cb(lv_event_t *e);
static void app_tiny_piano_set_open_cb(lv_event_t *e);
static void app_tiny_piano_set_close_cb(lv_event_t *e);

/* 首帧拆分倒计时（on_update 周期数）：卷帘 canvas 首绘推迟 ~30ms，让全屏
 * 背景先上屏，避免与全屏重绘+PPA 全帧旋转同帧挤爆 PSRAM 总线（DPI underrun
 * 闪屏）。切屏时 canvas 已被 engine_gui 统一隐藏。 */
static uint8_t s_canvas_defer = 0;
/* 首绘完成标志：尺寸需连续两拍不变才上屏，防布局未收敛先画小再闪大 */
static bool s_canvas_ready = false;

static bool app_tiny_piano_on_init(app_base_t *self, void *screen_ctx)
{
    (void)self;
    (void)screen_ctx;
    ESP_LOGI(TAG, "init");

    memset(&s_piano, 0, sizeof(s_piano));
    memset(s_piano.finger_pad, -1, sizeof(s_piano.finger_pad));
    s_piano.display = 0;
    s_piano.scale = 0;
    s_piano.root_oct = 3;
    s_piano.pitch = 0;
    s_piano.sound_type = 0;

    /* 从 NVS 恢复参数并校验 */
    piano_load_params();
    ESP_LOGI(TAG, "loaded: display=%d, scale=%d, root=C%d, pitch=%d, sound=%d",
             s_piano.display, s_piano.scale, s_piano.root_oct, s_piano.pitch,
             s_piano.sound_type);

    lvgl_port_lock(portMAX_DELAY);

    /* 把 NVS 值回写 UI 控件，避免 EEZ 默认值覆盖用户选择 */
    if (s_piano_ui.display_type != NULL) {
        lv_dropdown_set_selected(s_piano_ui.display_type, s_piano.display);
    }
    if (s_piano_ui.scale_type != NULL) {
        lv_dropdown_set_selected(s_piano_ui.scale_type, s_piano.scale);
    }
    if (s_piano_ui.root_v != NULL) {
        lv_roller_set_selected(s_piano_ui.root_v, s_piano.root_oct, LV_ANIM_OFF);
    }
    if (s_piano_ui.pitch != NULL) {
        lv_dropdown_set_selected(s_piano_ui.pitch, s_piano.pitch);
    }
    if (s_piano_ui.sound_type != NULL) {
        lv_dropdown_set_selected(s_piano_ui.sound_type, s_piano.sound_type);
    }

    if (s_piano_ui.btn_home != NULL) {
        lv_obj_add_event_cb(s_piano_ui.btn_home, app_tiny_piano_home_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_piano_ui.btn_rec != NULL) {
        lv_obj_add_event_cb(s_piano_ui.btn_rec, app_tiny_piano_rec_btn_cb, LV_EVENT_CLICKED, self);
    }
    if (s_piano_ui.btn_set != NULL) {
        lv_obj_add_event_cb(s_piano_ui.btn_set, app_tiny_piano_set_open_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_piano_ui.set_btn_return != NULL) {
        lv_obj_add_event_cb(s_piano_ui.set_btn_return, app_tiny_piano_set_close_cb, LV_EVENT_CLICKED, NULL);
    }
    lvgl_port_unlock();

    piano_apply_display();
    piano_refresh_pads();
    piano_set_sound_type(s_piano.sound_type);
    s_canvas_defer = 3;
    s_canvas_ready = false;
    return true;
}

/* 钢琴卷帘触摸：canvas 局部坐标命中琴键 */
static void piano_canvas_touch(const app_input_event_t *evt)
{
    if (s_piano.canvas_buf == NULL) {
        return;
    }

    int16_t local_x = evt->x - s_piano.canvas_x;
    int16_t local_y = evt->y - s_piano.canvas_y;
    uint8_t new_note = piano_key_hit_note(local_x, local_y);
    uint8_t old_note = s_piano.finger_note[evt->finger_id];

    if (evt->type == APP_INPUT_TOUCH_UP) {
        if (old_note != 0) {
            piano_midi_note(old_note, 0);
            s_piano.finger_note[evt->finger_id] = 0;
        }
        return;
    }

    if (new_note == old_note) {
        return;
    }

    if (old_note != 0) {
        piano_midi_note(old_note, 0);
    }
    if (new_note != 0) {
        piano_midi_note(new_note, piano_velocity(evt->pressure));
    }
    s_piano.finger_note[evt->finger_id] = new_note;
}

static void app_tiny_piano_on_input(app_base_t *self, const app_input_event_t *evt)
{
    (void)self;

    /* 设置面板可见时屏蔽主界面触控 */
    if (s_piano_ui.set != NULL && !lv_obj_has_flag(s_piano_ui.set, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    if (evt->type != APP_INPUT_TOUCH_DOWN &&
        evt->type != APP_INPUT_TOUCH_MOVE &&
        evt->type != APP_INPUT_TOUCH_UP) {
        return;
    }

    if (evt->finger_id >= PIANO_TOUCH_MAX_FINGERS) {
        return;
    }

    /* 矩阵与钢琴两种布局共用同一套多点滑动命中语义 */
    if (s_piano.display == 0) {
        piano_matrix_touch(evt);
    } else {
        piano_canvas_touch(evt);
    }
}

static void app_tiny_piano_home_cb(lv_event_t *e)
{
    (void)e;
    app_manager_request_kill_active();
}

static void app_tiny_piano_rec_btn_cb(lv_event_t *e)
{
    app_base_t *self = (app_base_t *)lv_event_get_user_data(e);
    ui_screen_piano_t *ui = (ui_screen_piano_t *)self->screen_ctx;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    if (s_piano.recording_self) {
        service_recorder_result_t r = app_manager_record_stop();
        if (r != RECORDER_OK && r != RECORDER_ERR_NOT_RECORDING) {
            app_manager_show_notification_timeout(_("停止录制失败"), 2000);
        }
        return;
    }

    service_recorder_result_t r = app_manager_record_start(TAG);
    switch (r) {
    case RECORDER_OK:
        app_manager_show_notification_timeout(_("开始录制"), 1000);
        s_piano.recording_self = true;
        s_piano.recording_stop_pending = false;
        if (ui->btn_rec != NULL) {
            lv_obj_add_state(ui->btn_rec, LV_STATE_CHECKED);
        }
        break;
    case RECORDER_ERR_NO_SD:
        app_manager_show_notification_timeout(_("未检测到 SD 卡"), 2000);
        break;
    case RECORDER_ERR_BUSY:
        app_manager_show_notification_timeout(_("正在录制中"), 1500);
        break;
    default:
        app_manager_show_notification_timeout(_("录制启动失败"), 2000);
        break;
    }
}

static void app_tiny_piano_set_open_cb(lv_event_t *e)
{
    (void)e;
    lvgl_port_lock(portMAX_DELAY);
    if (s_piano_ui.set != NULL) {
        lv_obj_clear_flag(s_piano_ui.set, LV_OBJ_FLAG_HIDDEN);
        /* 设置面板吸收点击，防止穿透触发主界面控件 */
        lv_obj_add_flag(s_piano_ui.set, LV_OBJ_FLAG_CLICKABLE);
    }
    lvgl_port_unlock();
}

static void app_tiny_piano_set_close_cb(lv_event_t *e)
{
    (void)e;
    lvgl_port_lock(portMAX_DELAY);
    if (s_piano_ui.set != NULL) {
        lv_obj_add_flag(s_piano_ui.set, LV_OBJ_FLAG_HIDDEN);
    }
    lvgl_port_unlock();
}

static void app_tiny_piano_on_update(app_base_t *self)
{
    (void)self;

    /* 矩阵滑动高亮同步：on_input（task_comm 上下文）只写掩码，LVGL 状态在此应用 */
    if (s_piano.glide_pad_mask != s_piano.glide_pad_mask_applied) {
        lvgl_port_lock(portMAX_DELAY);
        for (int i = 0; i < PIANO_PAD_COUNT; i++) {
            if (s_piano_ui.pads[i] == NULL) {
                continue;
            }
            if (s_piano.glide_pad_mask & (1U << i)) {
                lv_obj_add_state(s_piano_ui.pads[i], LV_STATE_PRESSED);
            } else {
                lv_obj_clear_state(s_piano_ui.pads[i], LV_STATE_PRESSED);
            }
        }
        s_piano.glide_pad_mask_applied = s_piano.glide_pad_mask;
        lvgl_port_unlock();
    }

    if (s_piano.display == 0 && !s_piano.pad_area_ready) {
        piano_matrix_ensure_coords();
    }

    if (s_piano.display == 1) {
        if (!s_canvas_ready) {
            /* 首帧拆分 + 尺寸两拍收敛；EEZ 网格 STRETCH 控件的最终尺寸只有
             * 本屏成为活动屏后由网格布局解析，未上屏前量到的是设计占位值，
             * 等上屏再测量（防先小后大闪屏） */
            if (s_canvas_defer > 0) {
                s_canvas_defer--;
            } else {
                lvgl_port_lock(portMAX_DELAY);
                bool screen_active = (s_piano_ui.canvas != NULL) &&
                                     (lv_obj_get_screen(s_piano_ui.canvas) == lv_screen_active());
                lvgl_port_unlock();
                if (screen_active &&
                    !piano_canvas_ensure_buffer() && s_piano.canvas_buf != NULL) {
                    piano_canvas_redraw();
                    s_canvas_ready = true;
                }
            }
        } else if (piano_canvas_ensure_buffer()) {
            piano_canvas_redraw();
        }
    }

    /* 下拉事件前端按 PRESSED 透传，选中值以轮询收敛 */
    lvgl_port_lock(portMAX_DELAY);
    uint32_t disp = lv_dropdown_get_selected(s_piano_ui.display_type);
    uint32_t scale = lv_dropdown_get_selected(s_piano_ui.scale_type);
    uint32_t root = lv_roller_get_selected(s_piano_ui.root_v);

    /* 根音音名下拉框，未绑定保持当前值 */
    uint32_t pitch = s_piano.pitch;
    if (s_piano_ui.pitch != NULL) {
        pitch = lv_dropdown_get_selected(s_piano_ui.pitch);
    }
    
    /* 获取音色选择下拉框值 */
    uint32_t sound_type = s_piano.sound_type;
    if (s_piano_ui.sound_type != NULL) {
        sound_type = lv_dropdown_get_selected(s_piano_ui.sound_type);
    }

    if (s_piano.recording_stop_pending && !app_manager_record_is_recording()) {
        char path[256];
        if (app_manager_record_get_last_path(path, sizeof(path))) {
            app_manager_show_notification_timeout(_("录音已保存"), 2000);
        } else {
            app_manager_show_notification_timeout(_("录制时间过短，已丢弃"), 2000);
        }
        s_piano.recording_stop_pending = false;
    }

    if (s_piano.recording_self && !app_manager_record_is_recording()) {
        s_piano.recording_self = false;
        s_piano.recording_stop_pending = true;
    }

    if (s_piano_ui.btn_rec != NULL) {
        if (s_piano.recording_self) {
            lv_obj_add_state(s_piano_ui.btn_rec, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(s_piano_ui.btn_rec, LV_STATE_CHECKED);
        }
    }
    lvgl_port_unlock();

    bool changed = false;
    if (disp != s_piano.display) {
        s_piano.display = (uint8_t)disp;
        ESP_LOGI(TAG, "display=%d", s_piano.display);
        piano_apply_display();
        changed = true;
    }
    if (scale != s_piano.scale && scale < PIANO_SCALE_COUNT) {
        s_piano.scale = (uint8_t)scale;
        ESP_LOGI(TAG, "scale=%d", s_piano.scale);
        piano_refresh_pads();
        changed = true;
    }
    if (root != s_piano.root_oct) {
        s_piano.root_oct = (uint8_t)root;
        ESP_LOGI(TAG, "root octave=C%d", s_piano.root_oct);
        changed = true;
    }
    if (pitch != s_piano.pitch && pitch <= 12) {
        s_piano.pitch = (uint8_t)pitch;
        ESP_LOGI(TAG, "pitch=%d", s_piano.pitch);
        /* 矩阵垫音名/配色与卷帘同步移调 */
        piano_refresh_pads();
        changed = true;
    }
    if (sound_type != s_piano.sound_type && sound_type <= 15) {
        uint8_t old_type = s_piano.sound_type;
        s_piano.sound_type = (uint8_t)sound_type;
        ESP_LOGI(TAG, "sound_type changed %d -> %d", old_type, (int)sound_type);
        piano_set_sound_type(sound_type);
        changed = true;
    }

    if (changed) {
        piano_save_params();
    }
}

static void app_tiny_piano_on_pause(app_base_t *self)
{
    (void)self;
    piano_glide_stop_all();

    if (s_piano.recording_self) {
        app_manager_record_stop();
        s_piano.recording_self = false;
        s_piano.recording_stop_pending = false;
    }

    /* 兜底保存，避免用户修改后未触发 on_update 保存就退出 */
    piano_save_params();
    ESP_LOGI(TAG, "pause");
}

static void app_tiny_piano_on_resume(app_base_t *self)
{
    (void)self;
    ESP_LOGI(TAG, "resume");
    if (s_piano.canvas_buf != NULL) {
        piano_canvas_redraw();
    }
}

static void app_tiny_piano_on_destroy(app_base_t *self)
{
    (void)self;
    ESP_LOGI(TAG, "destroy");
    piano_glide_stop_all();

    /* 移除 on_init 注册的按钮回调：EEZ 屏幕对象持久存在，
     * 不移除会在再次进入时重复注册导致一次点击多次触发 */
    lvgl_port_lock(portMAX_DELAY);
    if (s_piano_ui.btn_home != NULL) {
        lv_obj_remove_event_cb(s_piano_ui.btn_home, app_tiny_piano_home_cb);
    }
    if (s_piano_ui.btn_rec != NULL) {
        lv_obj_remove_event_cb(s_piano_ui.btn_rec, app_tiny_piano_rec_btn_cb);
    }
    if (s_piano_ui.btn_set != NULL) {
        lv_obj_remove_event_cb(s_piano_ui.btn_set, app_tiny_piano_set_open_cb);
    }
    if (s_piano_ui.set_btn_return != NULL) {
        lv_obj_remove_event_cb(s_piano_ui.set_btn_return, app_tiny_piano_set_close_cb);
    }
    lvgl_port_unlock();

    if (s_piano.recording_self) {
        app_manager_record_stop();
        s_piano.recording_self = false;
        s_piano.recording_stop_pending = false;
    }

    if (s_piano.canvas_buf != NULL) {
        heap_caps_free(s_piano.canvas_buf);
        s_piano.canvas_buf = NULL;
    }
}

esp_err_t app_tiny_piano_register(void)
{
    static app_base_t app = {
        .name = "Tiny Piano",
        .screen_name = "app_tiny_piano",
        .screen_ctx = &s_piano_ui,
        .screen_ctx_size = sizeof(s_piano_ui),
        .widget_bindings = s_piano_bindings,
        .on_init = app_tiny_piano_on_init,
        .on_update = app_tiny_piano_on_update,
        .on_pause = app_tiny_piano_on_pause,
        .on_resume = app_tiny_piano_on_resume,
        .on_destroy = app_tiny_piano_on_destroy,
        .on_input = app_tiny_piano_on_input,
    };
    return app_manager_register(&app);
}
