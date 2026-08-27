/**
 * @file app_drum_pad.c
 * @brief 鼓垫 App：虚拟鼓组（canvas 圆形鼓垫）与鼓垫矩阵（panel+button）双布局
 *
 * 虚拟鼓组改为 drum_panel_v canvas 绘制圆形鼓垫，触摸直接按坐标命中；
 * 鼓垫矩阵保持原有 drum_panel_m + LVGL 按钮事件链路不变。
 * 所有鼓键按下即按固定 GM 映射发声（10 通道），sound_type 预留未接入。
 */

#include "app_drum_pad.h"
#include "app_manager.h"
#include "engine_gui.h"
#include "engine_midi.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lvgl_port.h"
#include "service_i18n.h"
#include "service_nvs.h"
#include "lvgl.h"
#include <string.h>

static const char *TAG = "app_drum_pad";

#define DRUM_NOTE_OFF_MS        250
#define DRUM_ACTIVE_MAX         8
#define DRUM_TOUCH_MAX_FINGERS  5
#define DRUM_DEFAULT_VELOCITY   110

/* GM 鼓映射（10 通道） */
#define DRUM_NOTE_CRASH         49
#define DRUM_NOTE_CLOSEDHH      42
#define DRUM_NOTE_RIDE          51
#define DRUM_NOTE_KICK          36
#define DRUM_NOTE_HIGHTOM       50
#define DRUM_NOTE_MIDTOM        47
#define DRUM_NOTE_SNARE         38
#define DRUM_NOTE_FLOORTOM      41
#define DRUM_NOTE_CLAP          39
#define DRUM_NOTE_OPENHH        46

#define DRUM_CHANNEL            9

/* 布局模式 */
#define DRUM_LAYOUT_VKIT        0   /* 虚拟鼓组：canvas 圆形鼓垫 */
#define DRUM_LAYOUT_MATRIX      1   /* 鼓垫矩阵：保留原 panel+button */

/* 主题配色与固定色（金色不在 EEZ 主题表，直接写死） */
#define DRUM_C_BG               COLOR_BG_PRIMARY
#define DRUM_C_LABEL            COLOR_TEXT_PRIMARY
#define DRUM_C_GOLD             0xFFD700U
#define DRUM_C_GOLD_GRAD        0xA56D41U
#define DRUM_C_WHITE            0xF7F7F7U
#define DRUM_C_SNARE_BG         0xE9E9ECU
#define DRUM_C_SNARE_BORDER     0x9C9999U
#define DRUM_C_FT_BORDER        0x000000U

typedef struct {
    uint8_t note;
    int64_t off_at_us;
} drum_active_note_t;

typedef struct {
    const char *label;
    uint8_t note;
    int16_t x;              /* 圆外接矩形左上角 canvas 坐标 */
    int16_t y;
    int16_t w;
    int16_t h;
    uint32_t bg_color;      /* lv_color_hex 输入值，运行期转换 */
    uint32_t bg_grad_color;
    bool use_grad;          /* true 时使用 bg_color -> bg_grad_color 纵向渐变 */
    uint32_t border_color;
    int16_t border_width;
} drum_pad_def_t;

/* 虚拟鼓组布局（8 垫）——坐标/尺寸/颜色来自 EEZ 原设计 */
static const drum_pad_def_t s_pads_vkit[] = {
    { "Crash",    DRUM_NOTE_CRASH,    148,    0, 240, 240, DRUM_C_GOLD,     DRUM_C_GOLD_GRAD, true,  DRUM_C_GOLD,        0 },
    { "HiHat",    DRUM_NOTE_CLOSEDHH,   3,  163, 220, 220, DRUM_C_GOLD,     DRUM_C_GOLD_GRAD, true,  DRUM_C_GOLD,        0 },
    { "Ride",     DRUM_NOTE_RIDE,     879,   26, 300, 300, DRUM_C_GOLD,     DRUM_C_GOLD_GRAD, true,  DRUM_C_GOLD,        0 },
    { "Kick",     DRUM_NOTE_KICK,     408,  241, 360, 360, DRUM_C_WHITE,    DRUM_C_WHITE,      false, DRUM_C_WHITE,      10 },
    { "HiTom",    DRUM_NOTE_HIGHTOM,  405,   86, 166, 166, DRUM_C_WHITE,    DRUM_C_WHITE,      false, DRUM_C_WHITE,       0 },
    { "MidTom",   DRUM_NOTE_MIDTOM,   605,   86, 166, 166, DRUM_C_WHITE,    DRUM_C_WHITE,      false, DRUM_C_WHITE,       0 },
    { "Snare",    DRUM_NOTE_SNARE,    176,  320, 240, 240, DRUM_C_SNARE_BG, DRUM_C_SNARE_BG,  false, DRUM_C_SNARE_BORDER, 6 },
    { "FloorTom", DRUM_NOTE_FLOORTOM, 763,  297, 260, 260, DRUM_C_WHITE,    DRUM_C_WHITE,      false, DRUM_C_FT_BORDER,   8 },
};

typedef struct {
    lv_obj_t *panel_v;
    lv_obj_t *panel_m;
    lv_obj_t *crash_m;
    lv_obj_t *clap_m;
    lv_obj_t *openhht_m;
    lv_obj_t *closedhh_m;
    lv_obj_t *ride_m;
    lv_obj_t *snare_n;
    lv_obj_t *kick_m;
    lv_obj_t *floortom_m;
    lv_obj_t *display_type;
    lv_obj_t *sound_type;
    lv_obj_t *btn_home;
    lv_obj_t *btn_rec;
    lv_obj_t *btn_set;
    lv_obj_t *set;
    lv_obj_t *set_btn_return;
} ui_screen_drum_t;

static ui_screen_drum_t s_drum_ui = {0};

static const widget_binding_t s_drum_bindings[] = {
    WIDGET_BIND(ui_screen_drum_t, panel_v,     "drum_panel_v",      WIDGET_KIND_CANVAS),
    WIDGET_BIND(ui_screen_drum_t, panel_m,     "drum_panel_m",      WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_drum_t, crash_m,     "drum_crash_m",      WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_drum_t, clap_m,      "drum_clap_m",       WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_drum_t, openhht_m,   "drum_openhht_m",    WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_drum_t, closedhh_m,  "drum_closedhh_m",   WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_drum_t, ride_m,      "drum_ride_m",       WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_drum_t, snare_n,     "drum_snare_n",      WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_drum_t, kick_m,      "drum_kick_m",       WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_drum_t, floortom_m,  "drum_floortom_m",   WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_drum_t, display_type,"drum_display_type", WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_drum_t, sound_type,  "drum_sound_type",   WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_drum_t, btn_home,    "drum_btn_home",     WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_drum_t, btn_rec,     "drum_btn_rec",      WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_drum_t, btn_set,     "drum_btn_set",      WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_drum_t, set,         "drum_pad_set",      WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_drum_t, set_btn_return, "drum_set_btn_return", WIDGET_KIND_ANY),
    WIDGET_BINDING_END,
};

typedef struct {
    uint8_t display;            /* 0 虚拟鼓组 1 鼓垫矩阵 */
    uint8_t sound;              /* GM 鼓组下拉索引 0~8，映射 s_drum_kit_programs */
    drum_active_note_t active[DRUM_ACTIVE_MAX];
    int active_count;
    int8_t finger_pad[DRUM_TOUCH_MAX_FINGERS]; /* -1 表示未按下 */
    void *canvas_buf;
    int canvas_w;
    int canvas_h;
    int canvas_x;
    int canvas_y;
    bool canvas_attempted;      /* 虚拟鼓组 canvas 已尝试分配（失败不再重试，降级回矩阵布局） */
    bool recording_self;        /* 本 App 发起的录制 */
    bool recording_stop_pending;  /* 录制已停止，等待 finalize 后提示 */
} drum_state_t;

static drum_state_t s_drum = {0};

/* 1x1 占位 buffer，用于释放真实 canvas buffer 后避免对象悬空指向已释放内存 */
static uint16_t s_canvas_dummy_buf[1] = {0};

/* GM 鼓组 program 映射：与 EEZ drum_sound_type 下拉项一一对应
 * （000-标准/008-Room/016-Power/024-Electronic/025-TR-808/032-Jazz/040-Brush/048-Orchestra/056-SFX） */
static const uint8_t s_drum_kit_programs[] = {0, 8, 16, 24, 25, 32, 40, 48, 56};
#define DRUM_KIT_COUNT  (sizeof(s_drum_kit_programs) / sizeof(s_drum_kit_programs[0]))

/* -------------------- 鼓组切换与参数持久化 -------------------- */

/**
 * @brief 切换 GM 鼓组：engine_sf2 将 ch10 自动路由到鼓组 bank，
 * 只需发 Program Change 选择 kit（参考 tiny_piano 音色切换链路）
 */
static void drum_apply_kit(uint8_t kit_index)
{
    if (kit_index >= DRUM_KIT_COUNT) {
        kit_index = 0;
    }

    engine_midi_event_t evt = {0};
    evt.type = ENGINE_MIDI_MSG_PROGRAM_CHANGE;
    evt.channel = DRUM_CHANNEL;
    evt.data1 = s_drum_kit_programs[kit_index];
    evt.source_port = ENGINE_MIDI_PORT_APP;
    engine_midi_publish(&evt, 0);

    ESP_LOGI(TAG, "drum kit %d -> program %d", kit_index, evt.data1);
}

static void drum_save_params(void)
{
    service_nvs_drum_t params = {
        .display = s_drum.display,
        .sound = s_drum.sound,
    };
    service_nvs_set_drum(&params);
}

/* -------------------- 发声 -------------------- */

static void drum_midi_note(uint8_t note, uint8_t velocity)
{
    engine_midi_event_t midi = {0};
    midi.type = (velocity > 0) ? ENGINE_MIDI_MSG_NOTE_ON : ENGINE_MIDI_MSG_NOTE_OFF;
    midi.channel = DRUM_CHANNEL;
    midi.data1 = note;
    midi.data2 = velocity;
    midi.source_port = ENGINE_MIDI_PORT_APP;
    engine_midi_publish(&midi, 0);
}

/* 触摸力度带往 MIDI velocity；无力度事件（pressure=0，如 UI 矩阵按钮/初始帧）
 * 回退默认力度，保证老按键手感不变 */
static uint8_t drum_velocity(uint8_t pressure)
{
    return (pressure > 0) ? pressure : DRUM_DEFAULT_VELOCITY;
}

static void drum_hit(uint8_t note, uint8_t velocity)
{
    if (s_drum.active_count >= DRUM_ACTIVE_MAX) {
        drum_midi_note(s_drum.active[0].note, 0);
        memmove(&s_drum.active[0], &s_drum.active[1],
                sizeof(drum_active_note_t) * (DRUM_ACTIVE_MAX - 1));
        s_drum.active_count--;
    }

    drum_midi_note(note, velocity);
    s_drum.active[s_drum.active_count].note = note;
    s_drum.active[s_drum.active_count].off_at_us =
        esp_timer_get_time() + (int64_t)DRUM_NOTE_OFF_MS * 1000;
    s_drum.active_count++;
}

static void drum_stop_all_notes(void)
{
    for (int i = 0; i < s_drum.active_count; i++) {
        drum_midi_note(s_drum.active[i].note, 0);
    }
    s_drum.active_count = 0;
}

static void drum_process_notes(void)
{
    int64_t now = esp_timer_get_time();
    int write = 0;
    for (int i = 0; i < s_drum.active_count; i++) {
        if (now >= s_drum.active[i].off_at_us) {
            drum_midi_note(s_drum.active[i].note, 0);
        } else {
            if (write != i) {
                s_drum.active[write] = s_drum.active[i];
            }
            write++;
        }
    }
    s_drum.active_count = write;
}

/* -------------------- canvas 绘制（仅虚拟鼓组模式） -------------------- */

static void drum_draw_canvas(lv_obj_t *canvas)
{
    int w = s_drum.canvas_w;
    int h = s_drum.canvas_h;
    if (w <= 0 || h <= 0) {
        return;
    }

    lv_canvas_fill_bg(canvas, engine_gui_theme_color(DRUM_C_BG), LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    int count = (int)(sizeof(s_pads_vkit) / sizeof(s_pads_vkit[0]));

    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.font = &lv_font_montserrat_14;
    label_dsc.color = engine_gui_theme_color(DRUM_C_LABEL);
    label_dsc.align = LV_TEXT_ALIGN_CENTER;

    for (int i = 0; i < count; i++) {
        const drum_pad_def_t *pad = &s_pads_vkit[i];
        int32_t cx = (int32_t)pad->x + pad->w / 2;
        int32_t cy = (int32_t)pad->y + pad->h / 2;
        int32_t r = (pad->w < pad->h ? pad->w : pad->h) / 2;

        lv_draw_rect_dsc_t rect_dsc;
        lv_draw_rect_dsc_init(&rect_dsc);
        rect_dsc.bg_color = lv_color_hex(pad->bg_color);
        rect_dsc.bg_opa = LV_OPA_COVER;

        if (pad->use_grad) {
            rect_dsc.bg_grad.stops[0].color = lv_color_hex(pad->bg_color);
            rect_dsc.bg_grad.stops[0].opa = LV_OPA_COVER;
            rect_dsc.bg_grad.stops[0].frac = 0;
            rect_dsc.bg_grad.stops[1].color = lv_color_hex(pad->bg_grad_color);
            rect_dsc.bg_grad.stops[1].opa = LV_OPA_COVER;
            rect_dsc.bg_grad.stops[1].frac = 255;
            rect_dsc.bg_grad.stops_count = 2;
            rect_dsc.bg_grad.dir = LV_GRAD_DIR_VER;
        }

        rect_dsc.border_color = lv_color_hex(pad->border_color);
        rect_dsc.border_width = pad->border_width;
        rect_dsc.border_opa = LV_OPA_COVER;
        rect_dsc.radius = LV_RADIUS_CIRCLE;

        lv_area_t area = {
            cx - r, cy - r,
            cx + r, cy + r
        };
        lv_draw_rect(&layer, &rect_dsc, &area);

        label_dsc.text = pad->label;
        lv_area_t text_area = {
            cx - r, cy - 10,
            cx + r, cy + 10
        };
        lv_draw_label(&layer, &label_dsc, &text_area);
    }

    lv_canvas_finish_layer(canvas, &layer);
}

static void drum_canvas_redraw(void)
{
    if (s_drum_ui.panel_v == NULL || s_drum.canvas_buf == NULL) {
        return;
    }
    lvgl_port_lock(portMAX_DELAY);
    drum_draw_canvas(s_drum_ui.panel_v);
    lv_obj_clear_flag(s_drum_ui.panel_v, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(s_drum_ui.panel_v);
    lvgl_port_unlock();
}

static bool drum_canvas_ensure_buffer(void)
{
    if (s_drum.canvas_buf != NULL || s_drum_ui.panel_v == NULL) {
        return s_drum.canvas_buf != NULL;
    }

    /* 分配失败不再重试（大音源挤占 PSRAM 时每周期刷屏告警）；
     * 降级为鼓垫矩阵布局继续可用 */
    if (s_drum.canvas_attempted) {
        return false;
    }
    s_drum.canvas_attempted = true;

    lv_area_t coords;
    lvgl_port_lock(portMAX_DELAY);
    lv_obj_get_coords(s_drum_ui.panel_v, &coords);
    lvgl_port_unlock();

    int w = (int)lv_area_get_width(&coords);
    int h = (int)lv_area_get_height(&coords);
    if (w <= 0 || h <= 0) {
        return false;
    }

    void *buf = heap_caps_calloc(1, (size_t)w * h * 2, MALLOC_CAP_SPIRAM);
    if (buf == NULL) {
        ESP_LOGW(TAG, "canvas buf alloc failed, fallback to matrix layout");
        return false;
    }

    /* 分配、挂 buffer、首次绘制必须在同一个 lvgl lock 内完成，
     * 防止 unlock 后 task_gui 先刷新一帧脏数据。 */
    lvgl_port_lock(portMAX_DELAY);
    lv_canvas_set_buffer(s_drum_ui.panel_v, buf, (uint16_t)w, (uint16_t)h,
                         LV_COLOR_FORMAT_RGB565);
    s_drum.canvas_buf = buf;
    s_drum.canvas_w = w;
    s_drum.canvas_h = h;
    s_drum.canvas_x = coords.x1;
    s_drum.canvas_y = coords.y1;
    drum_draw_canvas(s_drum_ui.panel_v);
    lv_obj_invalidate(s_drum_ui.panel_v);
    lvgl_port_unlock();

    ESP_LOGI(TAG, "canvas buffer set %dx%d at %d,%d", w, h, s_drum.canvas_x, s_drum.canvas_y);
    return true;
}

/* 命中检测，返回 pad 索引或 -1 */
static int drum_hit_pad(int16_t x, int16_t y)
{
    int count = (int)(sizeof(s_pads_vkit) / sizeof(s_pads_vkit[0]));

    for (int i = 0; i < count; i++) {
        const drum_pad_def_t *pad = &s_pads_vkit[i];
        int32_t cx = (int32_t)pad->x + pad->w / 2;
        int32_t cy = (int32_t)pad->y + pad->h / 2;
        int32_t r = (pad->w < pad->h ? pad->w : pad->h) / 2;
        int32_t dx = x - cx;
        int32_t dy = y - cy;
        if (dx * dx + dy * dy <= r * r) {
            return i;
        }
    }
    return -1;
}

/* -------------------- 布局切换 -------------------- */

static void drum_apply_display(void)
{
    lvgl_port_lock(portMAX_DELAY);
    if (s_drum.display == DRUM_LAYOUT_VKIT) {
        if (s_drum_ui.panel_v != NULL) {
            lv_obj_clear_flag(s_drum_ui.panel_v, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_drum_ui.panel_m != NULL) {
            lv_obj_add_flag(s_drum_ui.panel_m, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        if (s_drum_ui.panel_v != NULL) {
            lv_obj_add_flag(s_drum_ui.panel_v, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_drum_ui.panel_m != NULL) {
            lv_obj_clear_flag(s_drum_ui.panel_m, LV_OBJ_FLAG_HIDDEN);
        }
    }
    lvgl_port_unlock();
}

/* -------------------- 生命周期与事件 -------------------- */

static void app_drum_pad_home_cb(lv_event_t *e);
static void app_drum_pad_event_cb(lv_event_t *e);
static void app_drum_pad_rec_btn_cb(lv_event_t *e);
static void app_drum_pad_set_open_cb(lv_event_t *e);
static void app_drum_pad_set_close_cb(lv_event_t *e);

static void drum_register_matrix_events(app_base_t *self)
{
    lv_obj_t *pads[] = {
        s_drum_ui.crash_m,
        s_drum_ui.clap_m,
        s_drum_ui.openhht_m,
        s_drum_ui.closedhh_m,
        s_drum_ui.ride_m,
        s_drum_ui.snare_n,
        s_drum_ui.kick_m,
        s_drum_ui.floortom_m,
    };
    for (size_t i = 0; i < sizeof(pads) / sizeof(pads[0]); i++) {
        if (pads[i] != NULL) {
            lv_obj_add_event_cb(pads[i], app_drum_pad_event_cb, LV_EVENT_PRESSED, self);
        }
    }
}

static void drum_unregister_matrix_events(void)
{
    lv_obj_t *pads[] = {
        s_drum_ui.crash_m,
        s_drum_ui.clap_m,
        s_drum_ui.openhht_m,
        s_drum_ui.closedhh_m,
        s_drum_ui.ride_m,
        s_drum_ui.snare_n,
        s_drum_ui.kick_m,
        s_drum_ui.floortom_m,
    };
    for (size_t i = 0; i < sizeof(pads) / sizeof(pads[0]); i++) {
        if (pads[i] != NULL) {
            lv_obj_remove_event_cb(pads[i], app_drum_pad_event_cb);
        }
    }
}

/* 首帧拆分倒计时（on_update 周期数）：VKIT canvas 分配/绘制推迟 ~30ms，
 * 让全屏背景先上屏（防 DPI underrun 闪屏）。切屏时 canvas 已被 engine_gui
 * 统一隐藏，推迟期间不会显示未初始化缓冲。 */
static uint8_t s_canvas_defer = 0;

static bool app_drum_pad_on_init(app_base_t *self, void *screen_ctx)
{
    (void)self;
    (void)screen_ctx;
    ESP_LOGI(TAG, "init");

    memset(&s_drum, 0, sizeof(s_drum));
    for (int i = 0; i < DRUM_TOUCH_MAX_FINGERS; i++) {
        s_drum.finger_pad[i] = -1;
    }

    /* 从 NVS 恢复布局与鼓组选择并校验 */
    service_nvs_drum_t params;
    service_nvs_get_drum(&params);
    s_drum.display = (params.display <= DRUM_LAYOUT_MATRIX) ? params.display : DRUM_LAYOUT_VKIT;
    s_drum.sound = (params.sound < DRUM_KIT_COUNT) ? params.sound : 0;

    lvgl_port_lock(portMAX_DELAY);
    lv_dropdown_set_selected(s_drum_ui.display_type, s_drum.display);
    lv_dropdown_set_selected(s_drum_ui.sound_type, s_drum.sound);
    lvgl_port_unlock();

    /* 进入即应用 NVS 恢复的鼓组 */
    drum_apply_kit(s_drum.sound);

    lvgl_port_lock(portMAX_DELAY);
    if (s_drum_ui.btn_home != NULL) {
        lv_obj_add_event_cb(s_drum_ui.btn_home, app_drum_pad_home_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_drum_ui.btn_rec != NULL) {
        lv_obj_add_event_cb(s_drum_ui.btn_rec, app_drum_pad_rec_btn_cb, LV_EVENT_CLICKED, self);
    }
    if (s_drum_ui.btn_set != NULL) {
        lv_obj_add_event_cb(s_drum_ui.btn_set, app_drum_pad_set_open_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_drum_ui.set_btn_return != NULL) {
        lv_obj_add_event_cb(s_drum_ui.set_btn_return, app_drum_pad_set_close_cb, LV_EVENT_CLICKED, NULL);
    }
    drum_register_matrix_events(self);
    lvgl_port_unlock();

    /* 虚拟鼓组模式的 canvas 分配/绘制推迟到 on_update 倒计时结束（首帧拆分，
     * 见 s_canvas_defer）；切屏时 canvas 已被 engine_gui 统一隐藏 */
    s_canvas_defer = 3;

    drum_apply_display();
    return true;
}

static void app_drum_pad_on_input(app_base_t *self, const app_input_event_t *evt)
{
    (void)self;

    /* 设置面板可见时屏蔽主界面触控 */
    if (s_drum_ui.set != NULL && !lv_obj_has_flag(s_drum_ui.set, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    /* 矩阵模式保持 LVGL 按钮事件，不处理坐标命中 */
    if (s_drum.display != DRUM_LAYOUT_VKIT || s_drum.canvas_buf == NULL) {
        return;
    }

    if (evt->type != APP_INPUT_TOUCH_DOWN &&
        evt->type != APP_INPUT_TOUCH_MOVE &&
        evt->type != APP_INPUT_TOUCH_UP) {
        return;
    }

    if (evt->finger_id >= DRUM_TOUCH_MAX_FINGERS) {
        return;
    }

    int16_t local_x = evt->x - s_drum.canvas_x;
    int16_t local_y = evt->y - s_drum.canvas_y;
    int new_pad = drum_hit_pad(local_x, local_y);
    int old_pad = s_drum.finger_pad[evt->finger_id];

    if (evt->type == APP_INPUT_TOUCH_UP) {
        s_drum.finger_pad[evt->finger_id] = -1;
        return;
    }

    if (new_pad == old_pad) {
        return;
    }

    if (new_pad >= 0) {
        drum_hit(s_pads_vkit[new_pad].note, drum_velocity(evt->pressure));
    }
    s_drum.finger_pad[evt->finger_id] = new_pad;
}

static void app_drum_pad_on_ui_event(app_base_t *self, lv_event_t *e)
{
    (void)self;

    if (lv_event_get_code(e) != LV_EVENT_PRESSED) {
        return;
    }

    lv_obj_t *target = lv_event_get_target_obj(e);
    ui_screen_drum_t *ui = &s_drum_ui;

    if (target == ui->crash_m) {
        drum_hit(DRUM_NOTE_CRASH, DRUM_DEFAULT_VELOCITY);
    } else if (target == ui->closedhh_m) {
        drum_hit(DRUM_NOTE_CLOSEDHH, DRUM_DEFAULT_VELOCITY);
    } else if (target == ui->ride_m) {
        drum_hit(DRUM_NOTE_RIDE, DRUM_DEFAULT_VELOCITY);
    } else if (target == ui->kick_m) {
        drum_hit(DRUM_NOTE_KICK, DRUM_DEFAULT_VELOCITY);
    } else if (target == ui->snare_n) {
        drum_hit(DRUM_NOTE_SNARE, DRUM_DEFAULT_VELOCITY);
    } else if (target == ui->floortom_m) {
        drum_hit(DRUM_NOTE_FLOORTOM, DRUM_DEFAULT_VELOCITY);
    } else if (target == ui->clap_m) {
        drum_hit(DRUM_NOTE_CLAP, DRUM_DEFAULT_VELOCITY);
    } else if (target == ui->openhht_m) {
        drum_hit(DRUM_NOTE_OPENHH, DRUM_DEFAULT_VELOCITY);
    }
}

static void app_drum_pad_event_cb(lv_event_t *e)
{
    app_base_t *self = (app_base_t *)lv_event_get_user_data(e);
    app_drum_pad_on_ui_event(self, e);
}

static void app_drum_pad_home_cb(lv_event_t *e)
{
    (void)e;
    app_manager_request_kill_active();
}

static void app_drum_pad_rec_btn_cb(lv_event_t *e)
{
    app_base_t *self = (app_base_t *)lv_event_get_user_data(e);
    ui_screen_drum_t *ui = (ui_screen_drum_t *)self->screen_ctx;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    if (s_drum.recording_self) {
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
        s_drum.recording_self = true;
        s_drum.recording_stop_pending = false;
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

static void app_drum_pad_set_open_cb(lv_event_t *e)
{
    (void)e;
    lvgl_port_lock(portMAX_DELAY);
    if (s_drum_ui.set != NULL) {
        lv_obj_clear_flag(s_drum_ui.set, LV_OBJ_FLAG_HIDDEN);
        /* 设置面板吸收点击，防止穿透触发主界面控件 */
        lv_obj_add_flag(s_drum_ui.set, LV_OBJ_FLAG_CLICKABLE);
    }
    lvgl_port_unlock();
}

static void app_drum_pad_set_close_cb(lv_event_t *e)
{
    (void)e;
    lvgl_port_lock(portMAX_DELAY);
    if (s_drum_ui.set != NULL) {
        lv_obj_add_flag(s_drum_ui.set, LV_OBJ_FLAG_HIDDEN);
    }
    lvgl_port_unlock();
}

static void app_drum_pad_on_update(app_base_t *self)
{
    (void)self;

    drum_process_notes();

    /* 仅在虚拟鼓组模式且尚未分配 canvas 时完成 buffer 分配与首次绘制。
     * drum_canvas_ensure_buffer() 内部已在同一 lvgl lock 内完成绘制。
     * 首帧拆分：倒计时期间不分配，让背景先上屏 */
    if (s_drum.display == DRUM_LAYOUT_VKIT && s_drum.canvas_buf == NULL) {
        if (s_canvas_defer > 0) {
            s_canvas_defer--;
        } else {
            drum_canvas_ensure_buffer();
        }
    }

    /* 下拉事件前端按 PRESSED 透传，选中值以轮询收敛 */
    lvgl_port_lock(portMAX_DELAY);
    uint32_t disp = lv_dropdown_get_selected(s_drum_ui.display_type);
    uint32_t snd = lv_dropdown_get_selected(s_drum_ui.sound_type);

    if (s_drum.recording_stop_pending && !app_manager_record_is_recording()) {
        char path[256];
        if (app_manager_record_get_last_path(path, sizeof(path))) {
            app_manager_show_notification_timeout(_("录音已保存"), 2000);
        } else {
            app_manager_show_notification_timeout(_("录制时间过短，已丢弃"), 2000);
        }
        s_drum.recording_stop_pending = false;
    }

    if (s_drum.recording_self && !app_manager_record_is_recording()) {
        s_drum.recording_self = false;
        s_drum.recording_stop_pending = true;
    }

    if (s_drum_ui.btn_rec != NULL) {
        if (s_drum.recording_self) {
            lv_obj_add_state(s_drum_ui.btn_rec, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(s_drum_ui.btn_rec, LV_STATE_CHECKED);
        }
    }
    lvgl_port_unlock();

    bool changed = false;
    if (disp != s_drum.display && disp <= DRUM_LAYOUT_MATRIX) {
        s_drum.display = (uint8_t)disp;
        ESP_LOGI(TAG, "display=%d", s_drum.display);
        drum_apply_display();
        if (s_drum.display == DRUM_LAYOUT_VKIT) {
            drum_canvas_redraw();
        }
        changed = true;
    }
    if (snd != s_drum.sound && snd < DRUM_KIT_COUNT) {
        s_drum.sound = (uint8_t)snd;
        drum_apply_kit(s_drum.sound);
        changed = true;
    }
    if (changed) {
        drum_save_params();
    }
}

static void app_drum_pad_on_pause(app_base_t *self)
{
    (void)self;
    drum_stop_all_notes();

    if (s_drum.recording_self) {
        app_manager_record_stop();
        s_drum.recording_self = false;
        s_drum.recording_stop_pending = false;
    }

    /* 兑底保存，避免仅改参数未触发保存就退出 */
    drum_save_params();
    drum_save_params();
    ESP_LOGI(TAG, "pause");
}

static void app_drum_pad_on_resume(app_base_t *self)
{
    (void)self;
    ESP_LOGI(TAG, "resume");
    if (s_drum.display == DRUM_LAYOUT_VKIT && s_drum.canvas_buf != NULL) {
        drum_canvas_redraw();
    }
}

static void app_drum_pad_on_destroy(app_base_t *self)
{
    (void)self;
    ESP_LOGI(TAG, "destroy");
    drum_stop_all_notes();

    lvgl_port_lock(portMAX_DELAY);
    drum_unregister_matrix_events();
    if (s_drum_ui.btn_home != NULL) {
        lv_obj_remove_event_cb(s_drum_ui.btn_home, app_drum_pad_home_cb);
    }
    if (s_drum_ui.btn_rec != NULL) {
        lv_obj_remove_event_cb(s_drum_ui.btn_rec, app_drum_pad_rec_btn_cb);
    }
    if (s_drum_ui.btn_set != NULL) {
        lv_obj_remove_event_cb(s_drum_ui.btn_set, app_drum_pad_set_open_cb);
    }
    if (s_drum_ui.set_btn_return != NULL) {
        lv_obj_remove_event_cb(s_drum_ui.set_btn_return, app_drum_pad_set_close_cb);
    }
    lvgl_port_unlock();

    if (s_drum.recording_self) {
        app_manager_record_stop();
        s_drum.recording_self = false;
        s_drum.recording_stop_pending = false;
    }

    if (s_drum.canvas_buf != NULL) {
        if (s_drum_ui.panel_v != NULL) {
            lvgl_port_lock(portMAX_DELAY);
            /* 释放前先设置 1x1 占位 buffer，防止 EEZ 复用对象时显示已释放内存 */
            lv_canvas_set_buffer(s_drum_ui.panel_v, s_canvas_dummy_buf, 1, 1,
                                 LV_COLOR_FORMAT_RGB565);
            lvgl_port_unlock();
        }
        heap_caps_free(s_drum.canvas_buf);
        s_drum.canvas_buf = NULL;
    }
}

esp_err_t app_drum_pad_register(void)
{
    static app_base_t app = {
        .name = "Drum Pad",
        .screen_name = "app_drum_pad",
        .screen_ctx = &s_drum_ui,
        .screen_ctx_size = sizeof(s_drum_ui),
        .widget_bindings = s_drum_bindings,
        .on_init = app_drum_pad_on_init,
        .on_update = app_drum_pad_on_update,
        .on_pause = app_drum_pad_on_pause,
        .on_resume = app_drum_pad_on_resume,
        .on_destroy = app_drum_pad_on_destroy,
        .on_input = app_drum_pad_on_input,
        .on_ui_event = app_drum_pad_on_ui_event,
    };
    return app_manager_register(&app);
}
