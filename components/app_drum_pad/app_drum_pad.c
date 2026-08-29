/**
 * @file app_drum_pad.c
 * @brief 鼓垫 App：虚拟鼓组（普通 LVGL 圆形对象，父容器整体开关）与鼓垫矩阵（panel+button）双布局
 *
 * 虚拟鼓组以普通圆形 lv_obj 绘制，不再使用常驻像素缓冲的 canvas（省 ~1.3MB PSRAM）；
 * 背景 + 8 个圆挂在同一个父容器下，隐藏父容器即整体切换回矩阵布局。触摸仍按坐标圆心命中。
 * 鼓垫矩阵保持原有 drum_panel_m + LVGL 按钮事件链路不变。
 * 所有鼓键按下即按固定 GM 映射发声（10 通道），sound_type 预留未接入。
 */

#include "app_drum_pad.h"
#include "app_manager.h"
#include "engine_gui.h"
#include "engine_midi.h"
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
#define DRUM_LAYOUT_VKIT        0   /* 虚拟鼓组：EEZ 静态圆形 panel，后端仅坐标命中 */
#define DRUM_LAYOUT_MATRIX      1   /* 鼓垫矩阵：保留原 panel+button */

/* 虚拟鼓组命中表：圆心/半径从 EEZ 绑定的圆形 panel 运行时取坐标构建，EEZ 排版即真值。
 * 顺序按视觉自上而下（EEZ 中后创建的圆绘制在上层），重叠区优先命中上层圆 */
#define DRUM_VKIT_PAD_COUNT 8
typedef struct {
    int16_t cx;
    int16_t cy;
    int16_t r;
    uint8_t note;
} drum_hit_t;

static const uint8_t s_vkit_notes[DRUM_VKIT_PAD_COUNT] = {
    DRUM_NOTE_CRASH, DRUM_NOTE_CLOSEDHH, DRUM_NOTE_RIDE, DRUM_NOTE_FLOORTOM,
    DRUM_NOTE_MIDTOM, DRUM_NOTE_SNARE, DRUM_NOTE_HIGHTOM, DRUM_NOTE_KICK,
};
static drum_hit_t s_vkit_hit[DRUM_VKIT_PAD_COUNT];
static int s_vkit_hit_count = 0;

typedef struct {
    uint8_t note;
    int64_t off_at_us;
} drum_active_note_t;

typedef struct {
    lv_obj_t *vkit_wrap;        /* 虚拟鼓组背景容器，隐藏即整体切回矩阵 */
    lv_obj_t *vkit_crash;
    lv_obj_t *vkit_hihat;
    lv_obj_t *vkit_ride;
    lv_obj_t *vkit_kick;        /* EEZ 导入对象名为 drum_vkit_ */
    lv_obj_t *vkit_hitom;
    lv_obj_t *vkit_midtom;
    lv_obj_t *vkit_snare;
    lv_obj_t *vkit_floortom;
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
    WIDGET_BIND(ui_screen_drum_t, vkit_wrap,    "drum_panel_vkit",   WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_drum_t, vkit_crash,   "drum_vkit_crash",   WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_drum_t, vkit_hihat,   "drum_vkit_hihat",   WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_drum_t, vkit_ride,    "drum_vkit_ride",    WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_drum_t, vkit_kick,    "drum_vkit_",        WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_drum_t, vkit_hitom,   "drum_vkit_hitom",   WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_drum_t, vkit_midtom,  "drum_vkit_midtom",  WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_drum_t, vkit_snare,   "drum_vkit_snare",   WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_drum_t, vkit_floortom, "drum_vkit_floortom", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_drum_t, panel_m,      "drum_panel_m",      WIDGET_KIND_ANY),
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
    bool recording_self;        /* 本 App 发起的录制 */
    bool recording_stop_pending;  /* 录制已停止，等待 finalize 后提示 */
} drum_state_t;

static drum_state_t s_drum = {0};

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

/* -------------------- 虚拟鼓组命中表（仅虚拟鼓组模式） -------------------- */

/* 读取 EEZ 绑定的 8 个圆形 panel 实时坐标，构建圆心命中表。
 * EEZ 排版即真值：改布局无需同步 C 端静态常量。
 * lv_obj_get_coords 返回屏幕逻辑绝对坐标，命中检测直接与触摸事件绝对坐标比对。 */
static bool drum_build_hit_table(void)
{
    if (s_drum_ui.vkit_wrap == NULL) {
        return false;
    }
    /* 顺序与 s_vkit_notes 一致：视觉自上而下（EEZ 中后创建者在上层） */
    lv_obj_t *objs[DRUM_VKIT_PAD_COUNT] = {
        s_drum_ui.vkit_crash, s_drum_ui.vkit_hihat, s_drum_ui.vkit_ride,
        s_drum_ui.vkit_floortom, s_drum_ui.vkit_midtom, s_drum_ui.vkit_snare,
        s_drum_ui.vkit_hitom, s_drum_ui.vkit_kick,
    };
    for (int i = 0; i < DRUM_VKIT_PAD_COUNT; i++) {
        if (objs[i] == NULL) {
            return false;
        }
    }

    /* 首帧布局可能尚未解析（容器为百分比尺寸），强制完成布局后再取坐标 */
    lv_obj_update_layout(s_drum_ui.vkit_wrap);

    for (int i = 0; i < DRUM_VKIT_PAD_COUNT; i++) {
        lv_area_t a;
        lv_obj_get_coords(objs[i], &a);
        int w = (int)lv_area_get_width(&a);
        int h = (int)lv_area_get_height(&a);
        s_vkit_hit[i].r = (int16_t)((w < h ? w : h) / 2);
        s_vkit_hit[i].cx = (int16_t)(a.x1 + s_vkit_hit[i].r);
        s_vkit_hit[i].cy = (int16_t)(a.y1 + s_vkit_hit[i].r);
        s_vkit_hit[i].note = s_vkit_notes[i];
    }
    s_vkit_hit_count = DRUM_VKIT_PAD_COUNT;
    return true;
}

/* 命中检测，返回命中表索引或 -1 */
static int drum_hit_pad(int16_t x, int16_t y)
{
    for (int i = 0; i < s_vkit_hit_count; i++) {
        int32_t dx = x - s_vkit_hit[i].cx;
        int32_t dy = y - s_vkit_hit[i].cy;
        if (dx * dx + dy * dy <= (int32_t)s_vkit_hit[i].r * s_vkit_hit[i].r) {
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
        if (s_drum_ui.vkit_wrap != NULL) {
            lv_obj_clear_flag(s_drum_ui.vkit_wrap, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_drum_ui.panel_m != NULL) {
            lv_obj_add_flag(s_drum_ui.panel_m, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        if (s_drum_ui.vkit_wrap != NULL) {
            lv_obj_add_flag(s_drum_ui.vkit_wrap, LV_OBJ_FLAG_HIDDEN);
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
    drum_build_hit_table();
    lvgl_port_unlock();

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
    if (s_drum.display != DRUM_LAYOUT_VKIT || s_drum_ui.vkit_wrap == NULL) {
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

    /* 命中表与触摸事件同为屏幕逻辑绝对坐标，直接比对 */
    int new_pad = drum_hit_pad(evt->x, evt->y);
    int old_pad = s_drum.finger_pad[evt->finger_id];

    if (evt->type == APP_INPUT_TOUCH_UP) {
        s_drum.finger_pad[evt->finger_id] = -1;
        return;
    }

    if (new_pad == old_pad) {
        return;
    }

    if (new_pad >= 0) {
        drum_hit(s_vkit_hit[new_pad].note, drum_velocity(evt->pressure));
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

    /* 浠庤Е鍙戜簨浠剁殑 indev 鎷?finger_id锛屾煡 engine_gui 鐨?pressure 缂撳瓨 */
    lv_indev_t *indev = lv_event_get_indev(e);
    uint8_t vel = DRUM_DEFAULT_VELOCITY;
    if (indev != NULL) {
        int finger_id = (int)(intptr_t)lv_indev_get_driver_data(indev);
        uint8_t press = engine_gui_get_finger_pressure((uint8_t)finger_id);
        if (press > 0) {
            vel = press;
        }
    }

    if (target == ui->crash_m) {
        drum_hit(DRUM_NOTE_CRASH, vel);
    } else if (target == ui->closedhh_m) {
        drum_hit(DRUM_NOTE_CLOSEDHH, vel);
    } else if (target == ui->ride_m) {
        drum_hit(DRUM_NOTE_RIDE, vel);
    } else if (target == ui->kick_m) {
        drum_hit(DRUM_NOTE_KICK, vel);
    } else if (target == ui->snare_n) {
        drum_hit(DRUM_NOTE_SNARE, vel);
    } else if (target == ui->floortom_m) {
        drum_hit(DRUM_NOTE_FLOORTOM, vel);
    } else if (target == ui->clap_m) {
        drum_hit(DRUM_NOTE_CLAP, vel);
    } else if (target == ui->openhht_m) {
        drum_hit(DRUM_NOTE_OPENHH, vel);
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
