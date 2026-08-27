/**
 * @file app_xy_pad.c
 * @brief XY 演奏面板：多点触控连续滑音
 *
 * X 轴音高仿二胡琴杆：音域 D4~A6（纯十二度），左低右高，指位移与音高
 * 满足弦长定律（低把位宽、高把位窄），可选线性均匀分布。
 * Y 轴音量：上方大音量、下方小音量。
 * 最多 3 指同时演奏，各占独立 MIDI 通道：按下触发音符并持续保持，
 * 滑动经弯音（Pitch Bend，范围经 RPN 设为 ±24 半音）连续改调不重触发，
 * 瞬时跨域超过弯音窗时换锚点重触发，松开关闭音符。
 * 3 个 LED 控件指示触点位置；配色全部取自 EEZ 主题数组。
 */

#include "app_xy_pad.h"
#include "app_manager.h"
#include "engine_gui.h"
#include "engine_midi.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "service_i18n.h"
#include <math.h>
#include <string.h>

static const char *TAG = "app_xy_pad";

#define XY_MAX_FINGERS      3
#define XY_VALID_Y0         110   /* 有效触控区：顶部工具栏以下 */
#define XY_LED_SIZE         50
#define XY_LED_HALF         (XY_LED_SIZE / 2)
#define XY_SOUND_COUNT      28    /* 音色下拉项数（GM 线性乐器表） */

/* 二胡模型：内弦空弦 D4，常用音域 D4~A6（纯十二度，频率比 6:1） */
#define XY_NOTE_ROOT        62.0f   /* D4 */
#define XY_RANGE_SEMITONES  31.0f   /* D4 ~ A6 */
/* 满行程指位移占有效弦长比例：1 - 2^(-31/12) ≈ 0.8331 */
#define XY_NECK_TRAVEL      0.8331f

/* 弯音窗：on_init 经 RPN 把通道 0-2 的弯音范围设为 ±24 半音，
 * 覆盖 D4~A6 全程滑动；偏离锚点超过 ±22 半音时换锚点重触发（边界情形） */
#define XY_BEND_RANGE_SEMITONES  24
#define XY_BEND_REANCHOR         22.0f
#define XY_BEND_CENTER           8192

/* 主题配色槽位（语义见 EEZ 主题定义） */
static const uint8_t s_finger_colors[XY_MAX_FINGERS] = { COLOR_ERROR, COLOR_PRIMARY, COLOR_SECONDARY };

typedef struct {
    bool active;
    uint8_t finger_id;
    uint8_t channel;
    uint8_t anchor_note;   /* 当前锚点量化音，voice 匹配与 note off 用 */
    uint8_t velocity;      /* 最近一次力度，换锚点重触发时沿用 */
} xy_finger_t;

typedef struct {
    xy_finger_t fingers[XY_MAX_FINGERS];
    uint8_t sound;         /* GM 音色索引（0~XY_SOUND_COUNT-1，见 s_xy_prog_map） */
    uint8_t curve;         /* 0 二胡琴杆 1 线性 */
    bool recording_self;        /* 本 App 发起的录制 */
    bool recording_stop_pending;  /* 录制已停止，等待 finalize 后提示 */
} xy_state_t;

typedef struct {
    lv_obj_t *points[XY_MAX_FINGERS];
    lv_obj_t *sound;
    lv_obj_t *step;
    lv_obj_t *btn_home;
    lv_obj_t *btn_rec;
    lv_obj_t *btn_set;
    lv_obj_t *set;
    lv_obj_t *set_btn_return;
} ui_screen_xy_t;

static ui_screen_xy_t s_xy_ui = {0};

static const widget_binding_t s_xy_bindings[] = {
    WIDGET_BIND(ui_screen_xy_t, points[0], "xy_point_1", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_xy_t, points[1], "xy_point_2", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_xy_t, points[2], "xy_point_3", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_xy_t, sound,     "xy_sound",   WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_xy_t, step,      "xy_step",    WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_xy_t, btn_home,  "xy_btn_home", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_xy_t, btn_rec,   "xy_btn_rec",  WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_xy_t, btn_set,   "xy_btn_set", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_xy_t, set,       "xy_set",     WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_xy_t, set_btn_return, "xy_set_btn_return", WIDGET_KIND_ANY),
    WIDGET_BINDING_END,
};

static xy_state_t s_xy = {0};

/* -------------------- 映射 -------------------- */

/* X -> 音高（半音浮点）
 * 二胡琴杆：频率与剩余弦长成反比 f = f0·L/(L-x)，音高 = 12·log2(L/(L-x))。
 * 面板 1280px 映射全行程后，低把端约 89px/半音，高把端收紧到约 15px/半音。 */
static float xy_pitch_from_x(int16_t x)
{
    float frac = (float)x / 1279.0f;
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    if (s_xy.curve == 1) {
        /* 线性：半音沿面板均匀分布 */
        return XY_NOTE_ROOT + frac * XY_RANGE_SEMITONES;
    }
    float remain = 1.0f - XY_NECK_TRAVEL * frac;
    return XY_NOTE_ROOT + 12.0f * log2f(1.0f / remain);
}

/* Y -> 音量 0~127：上方大音量，下方小音量。
 * 触点已是 engine_gui 旋转映射后的逻辑 1280×720 坐标，UI 上方即 y 小。 */
static uint8_t xy_volume_from_y(int16_t y)
{
    int v = (720 - (int)y) * 127 / (720 - XY_VALID_Y0);
    if (v < 0) v = 0;
    if (v > 127) v = 127;
    return (uint8_t)v;
}

/* -------------------- MIDI -------------------- */

static void xy_send_midi(uint8_t type, uint8_t ch, uint8_t d1, uint8_t d2)
{
    engine_midi_event_t midi = {0};
    midi.type = type;
    midi.channel = ch;
    midi.data1 = d1;
    midi.data2 = d2;
    midi.source_port = ENGINE_MIDI_PORT_APP;
    engine_midi_publish(&midi, 0);
}

static void xy_send_pitch_bend(uint8_t ch, int value14)
{
    if (value14 < 0) value14 = 0;
    if (value14 > 16383) value14 = 16383;

    engine_midi_event_t midi = {0};
    midi.type = ENGINE_MIDI_MSG_PITCH_BEND;
    midi.channel = ch;
    midi.value = (uint16_t)value14;
    midi.source_port = ENGINE_MIDI_PORT_APP;
    engine_midi_publish(&midi, 0);
}

/* 把进行中的音符连续滑到目标音高：弯音窗内只发弯音，跨窗换锚点重触发 */
static void xy_bend_to(xy_finger_t *f, float pitch)
{
    float delta = pitch - (float)f->anchor_note;
    if (delta > XY_BEND_REANCHOR || delta < -XY_BEND_REANCHOR) {
        xy_send_midi(ENGINE_MIDI_MSG_NOTE_OFF, f->channel, f->anchor_note, 0);
        f->anchor_note = (uint8_t)(pitch + 0.5f);
        xy_send_midi(ENGINE_MIDI_MSG_NOTE_ON, f->channel, f->anchor_note, f->velocity);
        delta = pitch - (float)f->anchor_note;
    }
    xy_send_pitch_bend(f->channel,
                       XY_BEND_CENTER + (int)(delta * (XY_BEND_CENTER / (float)XY_BEND_RANGE_SEMITONES)));
}

/* -------------------- LED 指示 -------------------- */

static void xy_led_sync(int slot, int16_t x, int16_t y, bool visible)
{
    if (s_xy_ui.points[slot] == NULL) {
        return;
    }

    if (x < XY_LED_HALF) x = XY_LED_HALF;
    if (x > 1280 - XY_LED_HALF) x = 1280 - XY_LED_HALF;
    if (y < XY_VALID_Y0 + XY_LED_HALF) y = XY_VALID_Y0 + XY_LED_HALF;
    if (y > 720 - XY_LED_HALF) y = 720 - XY_LED_HALF;

    lvgl_port_lock(portMAX_DELAY);
    lv_obj_set_pos(s_xy_ui.points[slot], x - XY_LED_HALF, y - XY_LED_HALF);
    lv_led_set_color(s_xy_ui.points[slot],
                     engine_gui_theme_color(s_finger_colors[slot]));
    if (visible) {
        lv_obj_clear_flag(s_xy_ui.points[slot], LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_xy_ui.points[slot], LV_OBJ_FLAG_HIDDEN);
    }
    lvgl_port_unlock();
}

/* -------------------- 音色选择 -------------------- */

/* GM 线性乐器表：与前端音色下拉 24 项一一对应（Program Change 号，0 基 GM 编号） */
static const uint8_t s_xy_prog_map[XY_SOUND_COUNT] = {
    40, 41, 42, 43, 44, 45, 48, 49, 50, 51, 52, 53,
    54, 56, 57, 58, 59, 60, 61, 62, 64, 65, 66, 67,
    68, 69, 70, 71,
};

static void xy_apply_sound(void)
{
    /* 弓弦/人声/铜管/木管类线性音色，适配弯音滑音演奏；Program 号直达 SF2/GM 音色表 */
    uint8_t prog = s_xy_prog_map[s_xy.sound % XY_SOUND_COUNT];
    for (int ch = 0; ch < XY_MAX_FINGERS; ch++) {
        xy_send_midi(ENGINE_MIDI_MSG_PROGRAM_CHANGE, (uint8_t)ch, prog, 0);
    }
}

/* -------------------- 触控处理 -------------------- */

static int xy_slot_by_finger(uint8_t finger_id)
{
    for (int i = 0; i < XY_MAX_FINGERS; i++) {
        if (s_xy.fingers[i].active && s_xy.fingers[i].finger_id == finger_id) {
            return i;
        }
    }
    return -1;
}

static int xy_slot_free(void)
{
    for (int i = 0; i < XY_MAX_FINGERS; i++) {
        if (!s_xy.fingers[i].active) {
            return i;
        }
    }
    return -1;
}

static void app_xy_pad_on_input(app_base_t *self, const app_input_event_t *evt)
{
    (void)self;

    if (s_xy_ui.set != NULL && !lv_obj_has_flag(s_xy_ui.set, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    if (evt->type == APP_INPUT_TOUCH_DOWN) {
        if (evt->y < XY_VALID_Y0) {
            return;
        }
        int slot = xy_slot_free();
        if (slot < 0) {
            return;
        }

        xy_finger_t *f = &s_xy.fingers[slot];
        float pitch = xy_pitch_from_x(evt->x);
        f->active = true;
        f->finger_id = evt->finger_id;
        f->channel = (uint8_t)slot;
        f->anchor_note = (uint8_t)(pitch + 0.5f);

        uint8_t vol = xy_volume_from_y(evt->y);
        f->velocity = vol;
        /* NOTE_ON 用最大力度 127，CC11 独立控制 0-127 完整音量范围。
         * 避免 NOTE_ON velocity 随按下位置变化导致音量范围不一致。 */
        xy_send_midi(ENGINE_MIDI_MSG_CONTROL_CHANGE, f->channel, 11, vol);
        xy_send_midi(ENGINE_MIDI_MSG_NOTE_ON, f->channel, f->anchor_note, 127);
        /* 按下点可能偏离量化音，立即弯到真实音高 */
        xy_bend_to(f, pitch);
        xy_led_sync(slot, evt->x, evt->y, true);
        return;
    }

    int slot = xy_slot_by_finger(evt->finger_id);
    if (slot < 0) {
        return;
    }
    xy_finger_t *f = &s_xy.fingers[slot];

    if (evt->type == APP_INPUT_TOUCH_MOVE) {
        float pitch = xy_pitch_from_x(evt->x);
        uint8_t vol = xy_volume_from_y(evt->y);
        f->velocity = vol;
        xy_bend_to(f, pitch);
        xy_send_midi(ENGINE_MIDI_MSG_CONTROL_CHANGE, f->channel, 11, vol);
        xy_led_sync(slot, evt->x, evt->y, true);
        return;
    }

    if (evt->type == APP_INPUT_TOUCH_UP) {
        xy_send_midi(ENGINE_MIDI_MSG_NOTE_OFF, f->channel, f->anchor_note, 0);
        f->active = false;
        xy_led_sync(slot, 0, 0, false);
        return;
    }
}

/* -------------------- 生命周期 -------------------- */

static void app_xy_pad_home_cb(lv_event_t *e);
static void app_xy_pad_rec_btn_cb(lv_event_t *e);
static void app_xy_pad_set_open_cb(lv_event_t *e);
static void app_xy_pad_set_close_cb(lv_event_t *e);

static void xy_all_sound_off(void)
{
    for (int i = 0; i < XY_MAX_FINGERS; i++) {
        if (s_xy.fingers[i].active) {
            xy_send_midi(ENGINE_MIDI_MSG_NOTE_OFF, s_xy.fingers[i].channel,
                         s_xy.fingers[i].anchor_note, 0);
            s_xy.fingers[i].active = false;
        }
        xy_led_sync(i, 0, 0, false);
        /* 只关音符并恢复可加性控制器；不改 program/bank，避免破坏其他 App 的音色选择 */
        xy_send_midi(ENGINE_MIDI_MSG_CONTROL_CHANGE, (uint8_t)i, 7, 127);
        xy_send_midi(ENGINE_MIDI_MSG_CONTROL_CHANGE, (uint8_t)i, 11, 127);
        xy_send_pitch_bend((uint8_t)i, XY_BEND_CENTER);
    }
}

static bool app_xy_pad_on_init(app_base_t *self, void *screen_ctx)
{
    (void)self;
    (void)screen_ctx;
    ESP_LOGI(TAG, "init");

    memset(&s_xy, 0, sizeof(s_xy));

    lvgl_port_lock(portMAX_DELAY);
    lv_dropdown_set_selected(s_xy_ui.sound, s_xy.sound);
    lv_dropdown_set_selected(s_xy_ui.step, s_xy.curve);
    if (s_xy_ui.btn_home != NULL) {
        lv_obj_add_event_cb(s_xy_ui.btn_home, app_xy_pad_home_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_xy_ui.btn_rec != NULL) {
        lv_obj_add_event_cb(s_xy_ui.btn_rec, app_xy_pad_rec_btn_cb, LV_EVENT_CLICKED, self);
    }
    if (s_xy_ui.btn_set != NULL) {
        lv_obj_add_event_cb(s_xy_ui.btn_set, app_xy_pad_set_open_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_xy_ui.set_btn_return != NULL) {
        lv_obj_add_event_cb(s_xy_ui.set_btn_return, app_xy_pad_set_close_cb, LV_EVENT_CLICKED, NULL);
    }
    lvgl_port_unlock();

    /* 演奏通道初始化：RPN 设弯音范围 + 音量/表情/弯音归位，避免污染其他 App */
    for (int ch = 0; ch < XY_MAX_FINGERS; ch++) {
        xy_send_midi(ENGINE_MIDI_MSG_CONTROL_CHANGE, (uint8_t)ch, 101, 0);  /* RPN MSB */
        xy_send_midi(ENGINE_MIDI_MSG_CONTROL_CHANGE, (uint8_t)ch, 100, 0);  /* RPN LSB：弯音范围 */
        xy_send_midi(ENGINE_MIDI_MSG_CONTROL_CHANGE, (uint8_t)ch, 6,
                     XY_BEND_RANGE_SEMITONES);                                /* Data Entry MSB：半音数 */
        xy_send_midi(ENGINE_MIDI_MSG_CONTROL_CHANGE, (uint8_t)ch, 38, 0);   /* Data Entry LSB：音分 */
        xy_send_midi(ENGINE_MIDI_MSG_CONTROL_CHANGE, (uint8_t)ch, 7, 127);
        xy_send_midi(ENGINE_MIDI_MSG_CONTROL_CHANGE, (uint8_t)ch, 11, 127);
        xy_send_pitch_bend((uint8_t)ch, XY_BEND_CENTER);
    }

    xy_apply_sound();
    return true;
}

static void app_xy_pad_on_update(app_base_t *self)
{
    (void)self;

    /* 下拉选中值以轮询收敛 */
    lvgl_port_lock(portMAX_DELAY);
    uint32_t sound = lv_dropdown_get_selected(s_xy_ui.sound);
    uint32_t curve = lv_dropdown_get_selected(s_xy_ui.step);

    if (s_xy.recording_stop_pending && !app_manager_record_is_recording()) {
        char path[256];
        if (app_manager_record_get_last_path(path, sizeof(path))) {
            app_manager_show_notification_timeout(_("录音已保存"), 2000);
        } else {
            app_manager_show_notification_timeout(_("录制时间过短，已丢弃"), 2000);
        }
        s_xy.recording_stop_pending = false;
    }

    if (s_xy.recording_self && !app_manager_record_is_recording()) {
        s_xy.recording_self = false;
        s_xy.recording_stop_pending = true;
    }

    if (s_xy_ui.btn_rec != NULL) {
        if (s_xy.recording_self) {
            lv_obj_add_state(s_xy_ui.btn_rec, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(s_xy_ui.btn_rec, LV_STATE_CHECKED);
        }
    }
    lvgl_port_unlock();

    if (sound != s_xy.sound && sound < XY_SOUND_COUNT) {
        s_xy.sound = (uint8_t)sound;
        ESP_LOGI(TAG, "sound=%d", s_xy.sound);
        xy_apply_sound();
    }
    if (curve != s_xy.curve) {
        s_xy.curve = (uint8_t)curve;
        ESP_LOGI(TAG, "curve=%d", s_xy.curve);
    }
}

static void app_xy_pad_on_pause(app_base_t *self)
{
    (void)self;
    xy_all_sound_off();

    if (s_xy.recording_self) {
        app_manager_record_stop();
        s_xy.recording_self = false;
        s_xy.recording_stop_pending = false;
    }
    ESP_LOGI(TAG, "pause");
}

static void app_xy_pad_on_resume(app_base_t *self)
{
    (void)self;
    xy_apply_sound();
    ESP_LOGI(TAG, "resume");
}

static void app_xy_pad_on_destroy(app_base_t *self)
{
    (void)self;

    /* 移除 on_init 注册的事件回调：EEZ 屏幕对象持久存在，
     * 不移除会在再次进入时重复注册导致一次事件多次触发 */
    lvgl_port_lock(portMAX_DELAY);
    if (s_xy_ui.btn_home != NULL) {
        lv_obj_remove_event_cb(s_xy_ui.btn_home, app_xy_pad_home_cb);
    }
    if (s_xy_ui.btn_rec != NULL) {
        lv_obj_remove_event_cb(s_xy_ui.btn_rec, app_xy_pad_rec_btn_cb);
    }
    if (s_xy_ui.btn_set != NULL) {
        lv_obj_remove_event_cb(s_xy_ui.btn_set, app_xy_pad_set_open_cb);
    }
    if (s_xy_ui.set_btn_return != NULL) {
        lv_obj_remove_event_cb(s_xy_ui.set_btn_return, app_xy_pad_set_close_cb);
    }
    lvgl_port_unlock();

    if (s_xy.recording_self) {
        app_manager_record_stop();
        s_xy.recording_self = false;
        s_xy.recording_stop_pending = false;
    }

    xy_all_sound_off();
    ESP_LOGI(TAG, "destroy");
}

static void app_xy_pad_home_cb(lv_event_t *e)
{
    (void)e;
    app_manager_request_kill_active();
}

static void app_xy_pad_rec_btn_cb(lv_event_t *e)
{
    app_base_t *self = (app_base_t *)lv_event_get_user_data(e);
    ui_screen_xy_t *ui = (ui_screen_xy_t *)self->screen_ctx;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    if (s_xy.recording_self) {
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
        s_xy.recording_self = true;
        s_xy.recording_stop_pending = false;
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

static void app_xy_pad_set_open_cb(lv_event_t *e)
{
    (void)e;
    lvgl_port_lock(portMAX_DELAY);
    if (s_xy_ui.set != NULL) {
        lv_obj_clear_flag(s_xy_ui.set, LV_OBJ_FLAG_HIDDEN);
        /* 设置面板吸收点击，防止穿透触发主界面控件 */
        lv_obj_add_flag(s_xy_ui.set, LV_OBJ_FLAG_CLICKABLE);
    }
    lvgl_port_unlock();
    xy_all_sound_off();
}

static void app_xy_pad_set_close_cb(lv_event_t *e)
{
    (void)e;
    lvgl_port_lock(portMAX_DELAY);
    if (s_xy_ui.set != NULL) {
        lv_obj_add_flag(s_xy_ui.set, LV_OBJ_FLAG_HIDDEN);
    }
    lvgl_port_unlock();
}

esp_err_t app_xy_pad_register(void)
{
    static app_base_t app = {
        .name = "XY Pad",
        .screen_name = "app_xy_mode",
        .screen_ctx = &s_xy_ui,
        .screen_ctx_size = sizeof(s_xy_ui),
        .widget_bindings = s_xy_bindings,
        .on_init = app_xy_pad_on_init,
        .on_update = app_xy_pad_on_update,
        .on_pause = app_xy_pad_on_pause,
        .on_resume = app_xy_pad_on_resume,
        .on_destroy = app_xy_pad_on_destroy,
        .on_input = app_xy_pad_on_input,
    };
    return app_manager_register(&app);
}
