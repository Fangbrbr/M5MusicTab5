/**
 * @file app_metronome.c
 * @brief 节拍器 App：BPM 节拍调度与 16 节拍灯
 *
 * BPM 20~300（按钮 ±1 / 滑块粗调 / TAP 测速），拍号分子 1~16、分母 4~32；
 * 播放时按当前拍号点亮节拍灯（首拍重拍灯）并按音色表发声；
 * BPM/拍号在点击播放时存 NVS，音色切换即时存 NVS。
 */

#include "app_metronome.h"
#include "app_manager.h"
#include "engine_gui.h"
#include "engine_midi.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "service_i18n.h"
#include "service_nvs.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "app_metronome";

#define METRON_BPM_MIN 20
#define METRON_BPM_MAX 300
#define METRON_LED_COUNT 16
#define METRON_NOTE_OFF_MS 60
#define METRON_TAP_MIN_MS 200
#define METRON_TAP_MAX_MS 2000
#define METRON_TAP_RESET_MS 2000 /* 超过此间隔重置测速记录 */
#define METRON_CHANNEL 9

/* 主题配色槽位 */
#define METRON_C_PRIMARY    COLOR_PRIMARY
#define METRON_C_SECONDARY  COLOR_SECONDARY

typedef struct
{
    uint8_t accent_note;
    uint8_t accent_vel;
    uint8_t normal_note;
    uint8_t normal_vel;
    bool pattern_24; /* true：2/4 节奏型，重音每 2 拍循环（强-弱），而非仅首拍 */
} metron_sound_t;

static const metron_sound_t s_sounds[] = {
    {34, 110, 33, 90, false}, /* 标准 */
    {75, 127, 75, 80, false}, /* 指针式 */
    {76, 110, 77, 90, false}, /* 木鱼式 */
    {36, 110, 42, 90, false}, /* 鼓组式 */
    {56, 110, 54, 90, false}, /* 打击乐式 */
    {39, 127, 39, 80, false}, /* 体感式 */
    {36, 120, 42, 70, true},  /* 2/4强弱式：低音鼓强拍+闭镲弱拍，重音两拍一循环 */
};
#define METRON_SOUND_COUNT (sizeof(s_sounds) / sizeof(s_sounds[0]))

static const uint8_t s_sig_bot_values[] = {4, 6, 8, 16, 32};

typedef struct
{
    lv_obj_t *btn_minus;
    lv_obj_t *label_bpm;
    lv_obj_t *btn_plus;
    lv_obj_t *leds[METRON_LED_COUNT];
    lv_obj_t *timesig_top;
    lv_obj_t *label_timesig;
    lv_obj_t *timesig_bot;
    lv_obj_t *btn_tempo;
    lv_obj_t *slider_bpm;
    lv_obj_t *btn_play_stop;
    lv_obj_t *btn_play_stop_label;
    lv_obj_t *sound;
    lv_obj_t *btn_home;
    lv_obj_t *btn_set;
    lv_obj_t *set;
    lv_obj_t *set_btn_return;
} ui_screen_metron_t;

static ui_screen_metron_t s_metron_ui = {0};

static const widget_binding_t s_metron_bindings[] = {
    WIDGET_BIND(ui_screen_metron_t, btn_minus, "metron_btn_minus", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_metron_t, label_bpm, "metron_label_bpm", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_metron_t, btn_plus, "metron_btn_plus", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_metron_t, leds[0], "metron_led_heavy", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_metron_t, leds[1], "metron_led_1", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_metron_t, leds[2], "metron_led_2", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_metron_t, leds[3], "metron_led_3", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_metron_t, leds[4], "metron_led_4", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_metron_t, leds[5], "metron_led_5", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_metron_t, leds[6], "metron_led_6", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_metron_t, leds[7], "metron_led_7", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_metron_t, leds[8], "metron_led_8", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_metron_t, leds[9], "metron_led_9", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_metron_t, leds[10], "metron_led_10", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_metron_t, leds[11], "metron_led_11", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_metron_t, leds[12], "metron_led_12", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_metron_t, leds[13], "metron_led_13", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_metron_t, leds[14], "metron_led_14", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_metron_t, leds[15], "metron_led_15", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_metron_t, timesig_top, "metron_timesig_top", WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_metron_t, label_timesig, "metron_label_timesig", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_metron_t, timesig_bot, "metron_timesig_bot", WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_metron_t, btn_tempo, "metron_btn_tempo", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_metron_t, slider_bpm, "metron_slider_bpm", WIDGET_KIND_SLIDER),
    WIDGET_BIND(ui_screen_metron_t, btn_play_stop, "metron_btn_play_stop", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_metron_t, btn_play_stop_label, "metron_btn_play_stop_label", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_metron_t, sound, "metron_sound", WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_metron_t, btn_home, "metron_btn_home", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_metron_t, btn_set, "metron_btn_set", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_metron_t, set, "metron_set", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_metron_t, set_btn_return, "metron_set_btn_return", WIDGET_KIND_ANY),
    WIDGET_BINDING_END,
};

typedef struct
{
    uint16_t bpm;
    uint8_t sig_top; /* 1~16 */
    uint8_t sig_bot; /* 分母选项索引 0~4 */
    uint8_t sound;   /* 0~5 */
    bool playing;
    uint8_t beat_idx;
    uint16_t bar_count;
    int64_t next_beat_us;
    int64_t tap_last_us;
    uint8_t active_note;
    int64_t note_off_us;
    bool note_playing;
} metron_state_t;

static metron_state_t s_metron = {0};

/* -------------------- 发声 -------------------- */

static void metron_midi_note(uint8_t note, uint8_t velocity)
{
    engine_midi_event_t midi = {0};
    midi.type = (velocity > 0) ? ENGINE_MIDI_MSG_NOTE_ON : ENGINE_MIDI_MSG_NOTE_OFF;
    midi.channel = METRON_CHANNEL;
    midi.data1 = note;
    midi.data2 = velocity;
    midi.source_port = ENGINE_MIDI_PORT_APP;
    engine_midi_publish(&midi, 0);
}

static void metron_play_hit(bool accent)
{
    const metron_sound_t *snd = &s_sounds[s_metron.sound];
    uint8_t note, vel;

    if (s_metron.sound == 3)
    { /* 鼓组式交替 */
        if (accent)
        {
            note = (s_metron.bar_count % 2 == 0) ? 36 : 38;
            vel = snd->accent_vel;
        }
        else
        {
            note = (s_metron.beat_idx % 2 == 0) ? 42 : 37;
            vel = snd->normal_vel;
        }
    }
    else
    {
        note = accent ? snd->accent_note : snd->normal_note;
        vel = accent ? snd->accent_vel : snd->normal_vel;
    }

    if (s_metron.note_playing)
    {
        metron_midi_note(s_metron.active_note, 0);
    }
    metron_midi_note(note, vel);
    s_metron.active_note = note;
    s_metron.note_off_us = esp_timer_get_time() + (int64_t)METRON_NOTE_OFF_MS * 1000;
    s_metron.note_playing = true;
}

/* -------------------- UI 更新 -------------------- */

static void metron_update_bpm_label(void)
{
    lvgl_port_lock(portMAX_DELAY);
    if (s_metron_ui.label_bpm)
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%u", s_metron.bpm);
        lv_label_set_text(s_metron_ui.label_bpm, buf);
    }
    if (s_metron_ui.slider_bpm)
    {
        lv_slider_set_value(s_metron_ui.slider_bpm, s_metron.bpm, LV_ANIM_OFF);
    }
    lvgl_port_unlock();
}

static void metron_update_timesig_label(void)
{
    lvgl_port_lock(portMAX_DELAY);
    if (s_metron_ui.label_timesig)
    {
        char buf[12];
        snprintf(buf, sizeof(buf), "%d/%d", s_metron.sig_top,
                 s_sig_bot_values[s_metron.sig_bot]);
        lv_label_set_text(s_metron_ui.label_timesig, buf);
    }
    lvgl_port_unlock();
}

static void metron_update_leds(void)
{
    lvgl_port_lock(portMAX_DELAY);
    for (int i = 0; i < METRON_LED_COUNT; i++)
    {
        lv_obj_t *led = s_metron_ui.leds[i];
        if (led == NULL)
            continue;
        if (i < s_metron.sig_top)
        {
            lv_obj_clear_flag(led, LV_OBJ_FLAG_HIDDEN);
            lv_led_set_brightness(led, (s_metron.playing && i == s_metron.beat_idx) ? 255 : 0);
        }
        else
        {
            lv_obj_add_flag(led, LV_OBJ_FLAG_HIDDEN);
        }
    }
    lvgl_port_unlock();
}

static void metron_set_bpm(int bpm)
{
    if (bpm < METRON_BPM_MIN)
        bpm = METRON_BPM_MIN;
    if (bpm > METRON_BPM_MAX)
        bpm = METRON_BPM_MAX;
    if ((uint16_t)bpm == s_metron.bpm)
        return;
    s_metron.bpm = (uint16_t)bpm;
    metron_update_bpm_label();
}

/* -------------------- 参数持久化 -------------------- */

static void metron_load_params(void)
{
    service_nvs_metronome_t params;
    service_nvs_get_metronome(&params);

    s_metron.bpm = params.bpm;
    if (s_metron.bpm < METRON_BPM_MIN || s_metron.bpm > METRON_BPM_MAX) {
        s_metron.bpm = 120;
    }

    s_metron.sig_top = params.sig_top;
    if (s_metron.sig_top < 1 || s_metron.sig_top > 16) {
        s_metron.sig_top = 4;
    }

    s_metron.sig_bot = params.sig_bot;
    if (s_metron.sig_bot > 4) {
        s_metron.sig_bot = 0;
    }

    s_metron.sound = params.sound;
    if (s_metron.sound >= METRON_SOUND_COUNT) {
        s_metron.sound = 0;
    }
}

static void metron_save_params(void)
{
    service_nvs_metronome_t params = {
        .bpm = s_metron.bpm,
        .sig_top = s_metron.sig_top,
        .sig_bot = s_metron.sig_bot,
        .sound = s_metron.sound,
        .reserved = 0,
    };
    service_nvs_set_metronome(&params);
}

/* -------------------- 播放控制 -------------------- */

static void metron_set_playing(bool play)
{
    s_metron.playing = play;
    // metron_set_button_icon(play);

    lvgl_port_lock(portMAX_DELAY);
    if (play)
    {
        lv_label_set_text(s_metron_ui.btn_play_stop_label, LV_SYMBOL_STOP);
        lv_obj_set_style_bg_color(s_metron_ui.btn_play_stop,
                                  engine_gui_theme_color(METRON_C_SECONDARY),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    else
    {
        lv_label_set_text(s_metron_ui.btn_play_stop_label, LV_SYMBOL_PLAY);
        lv_obj_set_style_bg_color(s_metron_ui.btn_play_stop,
                                  engine_gui_theme_color(METRON_C_PRIMARY),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    lvgl_port_unlock();

    if (play)
    {
        s_metron.beat_idx = 0;
        s_metron.bar_count = 0;
        s_metron.next_beat_us = esp_timer_get_time();
    }
    else if (s_metron.note_playing)
    {
        metron_midi_note(s_metron.active_note, 0);
        s_metron.note_playing = false;
    }
    metron_update_leds();
}

static void metron_fire_beat(void)
{
    /* 2/4 节奏型：重音每 2 拍循环（强-弱交替）；其余音色仅小节首拍重音 */
    bool accent;
    if (s_sounds[s_metron.sound].pattern_24) {
        accent = (s_metron.beat_idx % 2) == 0;
    } else {
        accent = s_metron.beat_idx == 0;
    }
    metron_play_hit(accent);
    metron_update_leds();

    s_metron.beat_idx++;
    if (s_metron.beat_idx >= s_metron.sig_top)
    {
        s_metron.beat_idx = 0;
        s_metron.bar_count++;
    }
}

static void metron_tap_tempo(void)
{
    int64_t now = esp_timer_get_time();
    if (s_metron.tap_last_us != 0)
    {
        int64_t interval_ms = (now - s_metron.tap_last_us) / 1000;
        if (interval_ms >= METRON_TAP_MIN_MS && interval_ms <= METRON_TAP_MAX_MS)
        {
            metron_set_bpm((int)(60000L / interval_ms));
        }
    }
    s_metron.tap_last_us = now;
}

/* -------------------- 生命周期 -------------------- */

static void app_metronome_home_cb(lv_event_t *e);
static void app_metronome_event_cb(lv_event_t *e);
static void app_metronome_set_open_cb(lv_event_t *e);
static void app_metronome_set_close_cb(lv_event_t *e);

static bool app_metronome_on_init(app_base_t *self, void *screen_ctx)
{
    (void)self;
    ESP_LOGI(TAG, "init");

    memset(&s_metron, 0, sizeof(s_metron));

    /* 恢复 NVS 参数 */
    metron_load_params();

    ui_screen_metron_t *ui = (ui_screen_metron_t *)screen_ctx;

    lvgl_port_lock(portMAX_DELAY);

    /* 滑块初始化 */
    if (ui->slider_bpm)
    {
        lv_slider_set_range(ui->slider_bpm, METRON_BPM_MIN, METRON_BPM_MAX);
        lv_slider_set_value(ui->slider_bpm, s_metron.bpm, LV_ANIM_OFF);
    }

    /* 拍号下拉初始化（清除后重建选项，避免依赖 EEZ 预设） */
    if (ui->timesig_top)
    {
        lv_dropdown_clear_options(ui->timesig_top);
        for (int i = 1; i <= 16; i++)
        {
            char buf[4];
            snprintf(buf, sizeof(buf), "%d", i);
            lv_dropdown_add_option(ui->timesig_top, buf, LV_DROPDOWN_POS_LAST);
        }
        lv_dropdown_set_selected(ui->timesig_top, s_metron.sig_top - 1);
    }
    if (ui->timesig_bot)
    {
        lv_dropdown_set_options(ui->timesig_bot, "4\n6\n8\n16\n32");
        lv_dropdown_set_selected(ui->timesig_bot, s_metron.sig_bot);
    }

    /* 音色下拉初始化 */
    if (ui->sound)
    {
        lv_dropdown_set_options(ui->sound, _("标准\n指针式\n木鱼式\n鼓组式\n打击乐式\n体感式\n2/4强弱式"));
        lv_dropdown_set_selected(ui->sound, s_metron.sound);
    }

    if (ui->btn_home != NULL)
    {
        lv_obj_add_event_cb(ui->btn_home, app_metronome_home_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui->btn_set != NULL)
    {
        lv_obj_add_event_cb(ui->btn_set, app_metronome_set_open_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui->set_btn_return != NULL)
    {
        lv_obj_add_event_cb(ui->set_btn_return, app_metronome_set_close_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui->btn_plus != NULL)
    {
        lv_obj_add_event_cb(ui->btn_plus, app_metronome_event_cb, LV_EVENT_PRESSED, self);
    }
    if (ui->btn_minus != NULL)
    {
        lv_obj_add_event_cb(ui->btn_minus, app_metronome_event_cb, LV_EVENT_PRESSED, self);
    }
    if (ui->btn_tempo != NULL)
    {
        lv_obj_add_event_cb(ui->btn_tempo, app_metronome_event_cb, LV_EVENT_PRESSED, self);
    }
    if (ui->btn_play_stop != NULL)
    {
        lv_obj_add_event_cb(ui->btn_play_stop, app_metronome_event_cb, LV_EVENT_PRESSED, self);
    }

    lvgl_port_unlock();

    metron_update_bpm_label();
    metron_update_timesig_label();
    metron_set_playing(false);
    return true;
}

static void app_metronome_on_ui_event(app_base_t *self, lv_event_t *e)
{
    (void)self;
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target_obj(e);
    ui_screen_metron_t *ui = &s_metron_ui;

    if (code == LV_EVENT_PRESSED)
    {
        if (target == ui->btn_plus)
        {
            metron_set_bpm(s_metron.bpm + 1);
        }
        else if (target == ui->btn_minus)
        {
            metron_set_bpm(s_metron.bpm - 1);
        }
        else if (target == ui->btn_tempo)
        {
            metron_tap_tempo();
        }
        else if (target == ui->btn_play_stop)
        {
            bool new_play = !s_metron.playing;
            if (new_play)
            {
                metron_save_params(); /* 仅在播放时保存 BPM/拍号 */
            }
            metron_set_playing(new_play);
        }
    }
}

static void app_metronome_event_cb(lv_event_t *e)
{
    app_base_t *self = (app_base_t *)lv_event_get_user_data(e);
    app_metronome_on_ui_event(self, e);
}

static void app_metronome_home_cb(lv_event_t *e)
{
    (void)e;
    app_manager_request_kill_active();
}

static void app_metronome_set_open_cb(lv_event_t *e)
{
    (void)e;
    lvgl_port_lock(portMAX_DELAY);
    if (s_metron_ui.set != NULL)
    {
        lv_obj_clear_flag(s_metron_ui.set, LV_OBJ_FLAG_HIDDEN);
        /* 设置面板吸收点击，防止穿透触发主界面控件 */
        lv_obj_add_flag(s_metron_ui.set, LV_OBJ_FLAG_CLICKABLE);
    }
    lvgl_port_unlock();
}

static void app_metronome_set_close_cb(lv_event_t *e)
{
    (void)e;
    lvgl_port_lock(portMAX_DELAY);
    if (s_metron_ui.set != NULL)
    {
        lv_obj_add_flag(s_metron_ui.set, LV_OBJ_FLAG_HIDDEN);
    }
    lvgl_port_unlock();
}

static void app_metronome_on_update(app_base_t *self)
{
    (void)self;
    int64_t now = esp_timer_get_time();

    /* TAP 超时重置（2秒无操作则清空上次记录） */
    if (s_metron.tap_last_us != 0 && (now - s_metron.tap_last_us) > METRON_TAP_RESET_MS * 1000)
    {
        s_metron.tap_last_us = 0;
    }

    /* 节拍调度 */
    if (s_metron.playing && now >= s_metron.next_beat_us)
    {
        uint32_t period_us = 60000000UL / s_metron.bpm;
        s_metron.next_beat_us += period_us;
        metron_fire_beat();
    }

    /* 音符关闭 */
    if (s_metron.note_playing && now >= s_metron.note_off_us)
    {
        metron_midi_note(s_metron.active_note, 0);
        s_metron.note_playing = false;
    }

    /* 轮询滑块与下拉 */
    lvgl_port_lock(portMAX_DELAY);
    int32_t slider = (s_metron_ui.slider_bpm) ? lv_slider_get_value(s_metron_ui.slider_bpm) : s_metron.bpm;
    uint32_t top = (s_metron_ui.timesig_top) ? lv_dropdown_get_selected(s_metron_ui.timesig_top) + 1 : s_metron.sig_top;
    uint32_t bot = (s_metron_ui.timesig_bot) ? lv_dropdown_get_selected(s_metron_ui.timesig_bot) : s_metron.sig_bot;
    uint32_t sound = (s_metron_ui.sound) ? lv_dropdown_get_selected(s_metron_ui.sound) : s_metron.sound;
    lvgl_port_unlock();

    bool changed = false;

    if (slider != s_metron.bpm)
    {
        s_metron.bpm = (uint16_t)slider;
        metron_update_bpm_label();
        changed = true;
    }

    if (top != s_metron.sig_top || bot != s_metron.sig_bot)
    {
        s_metron.sig_top = (uint8_t)top;
        s_metron.sig_bot = (uint8_t)bot;
        if (s_metron.beat_idx >= s_metron.sig_top)
            s_metron.beat_idx = 0;
        metron_update_timesig_label();
        metron_update_leds();
        changed = true;
    }

    if (sound != s_metron.sound && sound < METRON_SOUND_COUNT)
    {
        s_metron.sound = (uint8_t)sound;
        ESP_LOGI(TAG, "sound=%d", s_metron.sound);
        changed = true;
    }

    if (changed)
    {
        metron_save_params();
    }
}

static void app_metronome_on_pause(app_base_t *self)
{
    (void)self;
    if (s_metron.playing)
    {
        metron_set_playing(false);
    }

    /* 兜底保存，避免用户仅通过滑块/下拉修改参数后直接退出 */
    metron_save_params();
    ESP_LOGI(TAG, "pause");
}

static void app_metronome_on_resume(app_base_t *self)
{
    (void)self;
    ESP_LOGI(TAG, "resume");
}

static void app_metronome_on_destroy(app_base_t *self)
{
    (void)self;
    if (s_metron.note_playing)
    {
        metron_midi_note(s_metron.active_note, 0);
        s_metron.note_playing = false;
    }

    /* 移除 on_init 注册的事件回调：EEZ 屏幕对象持久存在，
     * 不移除会在再次进入时重复注册导致一次点击多次触发（播放键双切换） */
    lvgl_port_lock(portMAX_DELAY);
    if (s_metron_ui.btn_plus != NULL) {
        lv_obj_remove_event_cb(s_metron_ui.btn_plus, app_metronome_event_cb);
    }
    if (s_metron_ui.btn_minus != NULL) {
        lv_obj_remove_event_cb(s_metron_ui.btn_minus, app_metronome_event_cb);
    }
    if (s_metron_ui.btn_tempo != NULL) {
        lv_obj_remove_event_cb(s_metron_ui.btn_tempo, app_metronome_event_cb);
    }
    if (s_metron_ui.btn_play_stop != NULL) {
        lv_obj_remove_event_cb(s_metron_ui.btn_play_stop, app_metronome_event_cb);
    }
    if (s_metron_ui.btn_home != NULL) {
        lv_obj_remove_event_cb(s_metron_ui.btn_home, app_metronome_home_cb);
    }
    if (s_metron_ui.btn_set != NULL) {
        lv_obj_remove_event_cb(s_metron_ui.btn_set, app_metronome_set_open_cb);
    }
    if (s_metron_ui.set_btn_return != NULL) {
        lv_obj_remove_event_cb(s_metron_ui.set_btn_return, app_metronome_set_close_cb);
    }
    lvgl_port_unlock();

    ESP_LOGI(TAG, "destroy");
}

esp_err_t app_metronome_register(void)
{
    static app_base_t app = {
        .name = "Metronome",
        .screen_name = "app_metronome",
        .screen_ctx = &s_metron_ui,
        .screen_ctx_size = sizeof(s_metron_ui),
        .widget_bindings = s_metron_bindings,
        .on_init = app_metronome_on_init,
        .on_update = app_metronome_on_update,
        .on_pause = app_metronome_on_pause,
        .on_resume = app_metronome_on_resume,
        .on_destroy = app_metronome_on_destroy,
        .on_ui_event = app_metronome_on_ui_event,
    };
    return app_manager_register(&app);
}