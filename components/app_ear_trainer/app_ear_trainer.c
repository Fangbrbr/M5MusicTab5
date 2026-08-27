/**
 * @file app_ear_trainer.c
 * @brief 练耳 App：绝对音感与相对音感训练
 *
 * 判题规则：
 * - 绝对音感：按音名（pitch class）判定，回答键盘只有一个八度布局，
 *   三个八度的目标音均映射到同一组 12 键比对
 * - 相对音感：判定音程半音数，音程名与 ear_key_interval 按钮文案逐字对齐
 *
 * 出题流程：
 * - 练习模式（默认）：通知栏常驻展示答案并自动播放一次，引导点击匹配；
 *   答错无惩罚、常驻重贴答案直到点对；答对进入下一题
 * - 挑战模式：听音后作答，答对计分，答错扣血并直接下一题；
 *   血量耗尽展示 3s 后重开一局
 * 历史最高分按模式×难度分 6 组持久化到 NVS。
 */

#include "app_ear_trainer.h"
#include "app_manager.h"
#include "engine_gui.h"
#include "engine_midi.h"
#include "service_i18n.h"
#include "service_nvs.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "src/widgets/buttonmatrix/lv_buttonmatrix.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "app_ear_trainer";

#define EAR_MAX_TRIES           3
#define EAR_NOTE_DURATION_MS    500
#define EAR_MAX_LIVES           3

/* 生命灯配色：主题 success/error 色槽（见 EEZ 主题定义） */
#define EAR_INTERVAL_GAP_MS     500
#define EAR_WELCOME_MS          3000
#define EAR_AUTOPLAY_DELAY_MS   1000
#define EAR_RESULT_MS           1000
#define EAR_RESULT_WRONG_MS     2000
#define EAR_GAME_OVER_MS        3000

/* 欢迎词与引导词：均常驻展示（timeout 0），用户错过时机也不会不知所措 */
#define EAR_WELCOME_PRACTICE    _("欢迎来到练耳！练习模式：通知栏将展示答案，听音后请点击对应按钮")
#define EAR_WELCOME_CHALLENGE   _("欢迎来到练耳！挑战模式：听音后作答，答错扣血，无二次机会")
#define EAR_CHALLENGE_HINT      _("挑战模式：请听音后点击按钮作答，可点播放键重听")
#define EAR_MIN_NOTE            48  /* C3 */
#define EAR_MAX_NOTE            83  /* B5 */
#define EAR_MIDDLE_C            60  /* C4 */

#define EAR_ACTIVE_NOTES_MAX    4

/* 音感训练模式 */
typedef enum {
    EAR_MODE_ABSOLUTE = 0,
    EAR_MODE_RELATIVE = 1,
} ear_mode_t;

/* 难度等级 */
typedef enum {
    EAR_DIFF_EASY = 0,
    EAR_DIFF_MEDIUM = 1,
    EAR_DIFF_HARD = 2,
} ear_difficulty_t;

typedef struct {
    lv_obj_t *ear_key_try_play;
    lv_obj_t *ear_mode;
    lv_obj_t *ear_difficult;
    lv_obj_t *ear_trainer_test; /* 练习/挑战模式下拉框 */
    lv_obj_t *ear_key_major;
    lv_obj_t *ear_key_minor2;
    lv_obj_t *ear_key_minor3;
    lv_obj_t *ear_key_interval;
    lv_obj_t *ear_score;
    lv_obj_t *ear_score_title;
    lv_obj_t *ear_best_score;
    lv_obj_t *try_count_label;
    lv_obj_t *ear_life1;
    lv_obj_t *ear_life2;
    lv_obj_t *ear_life3;
    lv_obj_t *ear_life_panel;
    lv_obj_t *btn_home;
} ui_screen_ear_t;

static ui_screen_ear_t s_ear_ui = {0};

static const widget_binding_t s_ear_bindings[] = {
    WIDGET_BIND(ui_screen_ear_t, ear_key_try_play, "ear_key_try_play", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_ear_t, ear_mode,           "ear_mode",           WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_ear_t, ear_difficult,      "ear_difficult",      WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_ear_t, ear_trainer_test, "ear_trainer_test",     WIDGET_KIND_DROPDOWN), /* 练习/挑战模式选择 */
    WIDGET_BIND(ui_screen_ear_t, ear_key_major,      "ear_key_major",      WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_ear_t, ear_key_minor2,     "ear_key_minor2",     WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_ear_t, ear_key_minor3,     "ear_key_minor3",     WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_ear_t, ear_key_interval,   "ear_key_interval",   WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_ear_t, ear_score,          "ear_score",          WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_ear_t, ear_score_title,    "ear_score_title",    WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_ear_t, ear_best_score,     "ear_best_score",     WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_ear_t, try_count_label,    "ear_label_try_count", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_ear_t, ear_life1,          "ear_life1",          WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_ear_t, ear_life2,          "ear_life2",          WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_ear_t, ear_life3,          "ear_life3",          WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_ear_t, ear_life_panel,     "ear_life_panel",     WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_ear_t, btn_home,           "ear_btn_home",       WIDGET_KIND_ANY),
    WIDGET_BINDING_END,
};

typedef struct {
    uint8_t note;
    int64_t on_at_us;
    int64_t off_at_us;
    bool started;
} ear_active_note_t;

static ear_active_note_t s_active_notes[EAR_ACTIVE_NOTES_MAX];
static int s_active_note_count = 0;

typedef struct {
    ear_mode_t mode;
    ear_difficulty_t difficulty;
    uint8_t target_note;   /* 绝对音感目标音 */
    uint8_t root_note;     /* 相对音感根音 */
    uint8_t interval;      /* 相对音感音程（半音数） */
    bool ascending;        /* 相对音感方向 */
    uint32_t score;        /* 答对次数 */
    uint32_t total;        /* 总题数 */
    uint32_t tries_left;   /* 当前题剩余试听次数 */
    uint8_t lives;         /* 剩余生命（答错 -1，耗尽后游戏结束重开） */
    int64_t welcome_until_us;
    bool welcome_switched;          /* 欢迎提示已切换为操作提示 */
    bool result_pending;            /* 结果展示中，判题输入与播放键屏蔽 */
    int64_t result_until_us;
    bool game_over_restart_pending; /* 生命耗尽，延迟重开中 */
    bool practice_mode;             /* 练习模式（ear_trainer_test 下拉框）：无惩罚，先展示答案再出题 */
} ear_state_t;

static ear_state_t s_state = {0};

/* 初级/中级可用的音程集合（半音数） */
static const uint8_t s_interval_easy[]   = {4, 7, 12};
static const uint8_t s_interval_medium[] = {2, 3, 4, 5, 7, 9, 12};
static const uint8_t s_interval_hard[]   = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

/* 绝对音感初级：C4-B4 自然大调音阶 */
static const uint8_t s_major_scale[] = {60, 62, 64, 65, 67, 69, 71};

/* 半音数 -> 音程名。文案必须与 EEZ 工程 ear_key_interval 按钮逐字一致，
 * 保证通知栏答案与用户所见按钮对齐：等音程统一跟随按钮写法，
 * 如 6 半音写「增四度」不写「减五度」、12 半音写「八度」不写「纯八度」 */
static const char * const s_interval_names[] = {
    "", "小二度", "大二度", "小三度", "大三度", "纯四度", "增四度",
    "纯五度", "小六度", "大六度", "小七度", "大七度", "八度"
};

/* -------------------- 底层 MIDI 播放 -------------------- */

static void ear_midi_note(uint8_t note, uint8_t velocity)
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
static void ear_reset_timbre(void)
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

static void ear_play_note(uint8_t note, uint32_t duration_ms, uint32_t delay_ms)
{
    if (s_active_note_count >= EAR_ACTIVE_NOTES_MAX) {
        return;
    }

    int64_t now = esp_timer_get_time();
    ear_active_note_t *slot = &s_active_notes[s_active_note_count];
    slot->note = note;
    slot->on_at_us = now + (int64_t)delay_ms * 1000;
    slot->off_at_us = slot->on_at_us + (int64_t)duration_ms * 1000;
    slot->started = false;
    s_active_note_count++;
}

static void ear_stop_all_notes(void)
{
    for (int i = 0; i < s_active_note_count; i++) {
        if (s_active_notes[i].started) {
            ear_midi_note(s_active_notes[i].note, 0);
        }
    }
    s_active_note_count = 0;
}

static void ear_note_name(uint8_t note, char *out, size_t len)
{
    static const char names[12][3] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    snprintf(out, len, "%s%d", names[note % 12], (int)(note / 12) - 1);
}

/* 答案文案：绝对音感为音名，相对音感为与按钮文案一致的音程名 */
static void ear_answer_text(char *out, size_t len)
{
    if (s_state.mode == EAR_MODE_ABSOLUTE) {
        ear_note_name(s_state.target_note, out, len);
    } else {
        snprintf(out, len, "%s", _(s_interval_names[s_state.interval]));
    }
}

/* 练习模式引导通知：答案与点击引导同屏常驻（timeout 0），用户不会错过提示。
 * lead 为前缀措辞：答错时追加鼓励与继续引导，出题时置空 */
static void ear_show_practice_notification(const char *lead)
{
    char answer[16];
    char buf[96];

    ear_answer_text(answer, sizeof(answer));

    if (s_state.mode == EAR_MODE_ABSOLUTE) {
        snprintf(buf, sizeof(buf), _("%s答案：%s，听音后点击对应琴键"), lead, answer);
    } else {
        char root_name[8];
        char target_name[8];
        uint8_t target = s_state.ascending ?
                         s_state.root_note + s_state.interval :
                         s_state.root_note - s_state.interval;
        ear_note_name(s_state.root_note, root_name, sizeof(root_name));
        ear_note_name(target, target_name, sizeof(target_name));
        snprintf(buf, sizeof(buf), _("%s答案：%s%s（%s - %s），点击对应音程按钮"),
                 lead, s_state.ascending ? _("上行") : _("下行"), answer,
                 root_name, target_name);
    }
    app_manager_show_notification_timeout(buf, 0);
}

/* -------------------- 题目生成 -------------------- */

static uint8_t ear_generate_absolute_target(void)
{
    switch (s_state.difficulty) {
    case EAR_DIFF_EASY:
        return s_major_scale[esp_random() % (sizeof(s_major_scale) / sizeof(s_major_scale[0]))];
    case EAR_DIFF_MEDIUM:
        return EAR_MIDDLE_C + (uint8_t)(esp_random() % 12);
    default:
        return EAR_MIN_NOTE + (uint8_t)(esp_random() % (EAR_MAX_NOTE - EAR_MIN_NOTE + 1));
    }
}

static uint8_t ear_pick_interval(void)
{
    const uint8_t *set;
    size_t len;

    switch (s_state.difficulty) {
    case EAR_DIFF_EASY:
        set = s_interval_easy;
        len = sizeof(s_interval_easy);
        break;
    case EAR_DIFF_MEDIUM:
        set = s_interval_medium;
        len = sizeof(s_interval_medium);
        break;
    default:
        set = s_interval_hard;
        len = sizeof(s_interval_hard);
        break;
    }

    return set[esp_random() % (len / sizeof(uint8_t))];
}

static void ear_generate_relative_question(void)
{
    s_state.interval = ear_pick_interval();

    bool ascending = (esp_random() % 2) != 0;

    /* 随机根音，若超出范围则翻转方向；仍越界则 clamp 到合法区间 */
    for (int i = 0; i < 10; i++) {
        uint8_t root = EAR_MIN_NOTE + (uint8_t)(esp_random() % (EAR_MAX_NOTE - EAR_MIN_NOTE + 1));
        uint8_t target = ascending ? root + s_state.interval : root - s_state.interval;

        if (target >= EAR_MIN_NOTE && target <= EAR_MAX_NOTE) {
            s_state.root_note = root;
            s_state.ascending = ascending;
            return;
        }
    }

    s_state.ascending = ascending;
    uint8_t root = ascending ? EAR_MIN_NOTE : EAR_MAX_NOTE;
    uint8_t target = ascending ? root + s_state.interval : root - s_state.interval;

    if (target > EAR_MAX_NOTE) {
        s_state.ascending = false;
        s_state.root_note = EAR_MAX_NOTE;
    } else if (target < EAR_MIN_NOTE) {
        s_state.ascending = true;
        s_state.root_note = EAR_MIN_NOTE;
    } else {
        s_state.root_note = root;
    }
}

/* -------------------- UI 更新 -------------------- */

/* 得分控件在练习模式下隐藏（ear_apply_stat_visibility），此处只刷数字 */
static void ear_update_score_label(void)
{
    char buf[16];

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)s_state.score);
    lvgl_port_lock(portMAX_DELAY);
    lv_label_set_text(s_ear_ui.ear_score, buf);
    lvgl_port_unlock();
}

/* 生命灯刷新：存活 success 色，已消耗 error 色 */
static void ear_update_lives(void)
{
    lv_obj_t *lifes[EAR_MAX_LIVES] = {s_ear_ui.ear_life1, s_ear_ui.ear_life2, s_ear_ui.ear_life3};

    lvgl_port_lock(portMAX_DELAY);
    for (int i = 0; i < EAR_MAX_LIVES; i++) {
        if (lifes[i] == NULL) {
            continue;
        }
        lv_color_t c = engine_gui_theme_color((i < (int)s_state.lives) ? COLOR_SUCCESS : COLOR_ERROR);
        lv_led_set_color(lifes[i], c);
    }
    lvgl_port_unlock();
}

/* 试听次数区（「点击试听」标题 + 次数值）两种模式常驻不隐藏：
 * 练习模式无限试听，次数刷为 888 表意「管够」 */
static void ear_update_try_label(void)
{
    char buf[8];

    if (s_state.practice_mode) {
        snprintf(buf, sizeof(buf), "888");
    } else {
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)s_state.tries_left);
    }
    lvgl_port_lock(portMAX_DELAY);
    lv_label_set_text(s_ear_ui.try_count_label, buf);
    lvgl_port_unlock();
}

static void ear_set_play_enabled(bool enabled)
{
    lvgl_port_lock(portMAX_DELAY);
    if (enabled) {
        lv_obj_clear_state(s_ear_ui.ear_key_try_play, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(s_ear_ui.ear_key_try_play, LV_STATE_DISABLED);
    }
    lvgl_port_unlock();
}

/* 练习模式隐藏血量/得分控件，挑战模式恢复显示。
 * 试听次数区不隐藏：练习模式下次数刷为 888（见 ear_update_try_label） */
static void ear_apply_stat_visibility(void)
{
    lv_obj_t *stats[] = {
        s_ear_ui.ear_life_panel,
        s_ear_ui.ear_score_title,
        s_ear_ui.ear_score,
    };

    lvgl_port_lock(portMAX_DELAY);
    for (size_t i = 0; i < sizeof(stats) / sizeof(stats[0]); i++) {
        if (stats[i] == NULL) {
            continue;
        }
        if (s_state.practice_mode) {
            lv_obj_add_flag(stats[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(stats[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    lvgl_port_unlock();
}

static uint8_t ear_best_index(void)
{
    return (uint8_t)(s_state.mode * 3 + s_state.difficulty);
}

static void ear_update_best_label(void)
{
    if (s_ear_ui.ear_best_score == NULL) {
        return;
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu",
             (unsigned long)service_nvs_get_ear_best(ear_best_index()));
    lvgl_port_lock(portMAX_DELAY);
    lv_label_set_text(s_ear_ui.ear_best_score, buf);
    lvgl_port_unlock();
}

static void ear_check_best(void)
{
    uint8_t idx = ear_best_index();
    if (s_state.score > service_nvs_get_ear_best(idx)) {
        service_nvs_set_ear_best(idx, s_state.score);
        ear_update_best_label();
    }
}

static void ear_apply_mode_ui(void)
{
    lv_obj_t *piano_parent = lv_obj_get_parent(s_ear_ui.ear_key_major);
    lv_obj_t *interval_parent = lv_obj_get_parent(s_ear_ui.ear_key_interval);

    lvgl_port_lock(portMAX_DELAY);
    if (s_state.mode == EAR_MODE_ABSOLUTE) {
        if (piano_parent != NULL) {
            lv_obj_clear_flag(piano_parent, LV_OBJ_FLAG_HIDDEN);
        }
        if (interval_parent != NULL) {
            lv_obj_add_flag(interval_parent, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        if (piano_parent != NULL) {
            lv_obj_add_flag(piano_parent, LV_OBJ_FLAG_HIDDEN);
        }
        if (interval_parent != NULL) {
            lv_obj_clear_flag(interval_parent, LV_OBJ_FLAG_HIDDEN);
        }
    }
    lvgl_port_unlock();
}

static void ear_set_interval_button_enabled(uint8_t semitones, bool enabled)
{
    if (s_ear_ui.ear_key_interval == NULL || semitones < 1 || semitones > 12) {
        return;
    }

    uint32_t btn_id = semitones - 1;
    lvgl_port_lock(portMAX_DELAY);
    if (enabled) {
        lv_buttonmatrix_clear_button_ctrl(s_ear_ui.ear_key_interval, btn_id,
                                          LV_BUTTONMATRIX_CTRL_DISABLED);
    } else {
        lv_buttonmatrix_set_button_ctrl(s_ear_ui.ear_key_interval, btn_id,
                                        LV_BUTTONMATRIX_CTRL_DISABLED);
    }
    lvgl_port_unlock();
}

static void ear_apply_difficulty_ui(void)
{
    /* 绝对音感初级禁用黑键 */
    bool black_disabled = (s_state.mode == EAR_MODE_ABSOLUTE &&
                           s_state.difficulty == EAR_DIFF_EASY);

    lvgl_port_lock(portMAX_DELAY);
    if (s_ear_ui.ear_key_minor2 != NULL) {
        for (uint32_t i = 0; i < 2; i++) {
            if (black_disabled) {
                lv_buttonmatrix_set_button_ctrl(s_ear_ui.ear_key_minor2, i,
                                                LV_BUTTONMATRIX_CTRL_DISABLED);
            } else {
                lv_buttonmatrix_clear_button_ctrl(s_ear_ui.ear_key_minor2, i,
                                                  LV_BUTTONMATRIX_CTRL_DISABLED);
            }
        }
    }
    if (s_ear_ui.ear_key_minor3 != NULL) {
        for (uint32_t i = 0; i < 3; i++) {
            if (black_disabled) {
                lv_buttonmatrix_set_button_ctrl(s_ear_ui.ear_key_minor3, i,
                                                LV_BUTTONMATRIX_CTRL_DISABLED);
            } else {
                lv_buttonmatrix_clear_button_ctrl(s_ear_ui.ear_key_minor3, i,
                                                  LV_BUTTONMATRIX_CTRL_DISABLED);
            }
        }
    }
    lvgl_port_unlock();

    /* 根据难度启用/禁用对应音程按钮 */
    const uint8_t *allowed;
    size_t allowed_len = 0;

    switch (s_state.difficulty) {
    case EAR_DIFF_EASY:
        allowed = s_interval_easy;
        allowed_len = sizeof(s_interval_easy);
        break;
    case EAR_DIFF_MEDIUM:
        allowed = s_interval_medium;
        allowed_len = sizeof(s_interval_medium);
        break;
    default:
        allowed = s_interval_hard;
        allowed_len = sizeof(s_interval_hard);
        break;
    }

    bool enabled_map[13] = {false};
    for (size_t i = 0; i < allowed_len / sizeof(uint8_t); i++) {
        if (allowed[i] >= 1 && allowed[i] <= 12) {
            enabled_map[allowed[i]] = true;
        }
    }

    for (uint8_t s = 1; s <= 12; s++) {
        ear_set_interval_button_enabled(s, enabled_map[s]);
    }
}

/* -------------------- 答题处理 -------------------- */

/* 琴键矩阵按钮索引 -> 音名：白键 C D E F G A B，黑键两组 C# D# / F# G# A# */
static int ear_key_to_pitch_class(lv_obj_t *btnm, uint32_t btn)
{
    static const int8_t s_white_pc[7]  = {0, 2, 4, 5, 7, 9, 11};
    static const int8_t s_minor2_pc[2] = {1, 3};
    static const int8_t s_minor3_pc[3] = {6, 8, 10};

    if (btnm == s_ear_ui.ear_key_major && btn < 7) {
        return s_white_pc[btn];
    }
    if (btnm == s_ear_ui.ear_key_minor2 && btn < 2) {
        return s_minor2_pc[btn];
    }
    if (btnm == s_ear_ui.ear_key_minor3 && btn < 3) {
        return s_minor3_pc[btn];
    }
    return -1;
}

static void ear_next_question(bool auto_play);
static void ear_restart_game(void);

static void ear_handle_answer(bool correct, const char *answer_text)
{
    /* 练习模式：无惩罚。答错不结束本题，常驻重贴答案继续引导，
     * 直到点对匹配按钮本轮才结束；答对短暂展示后进入下一题 */
    if (s_state.practice_mode) {
        if (correct) {
            app_manager_show_notification_timeout(_("回答正确！"), EAR_RESULT_MS);
            ear_set_play_enabled(false);
            s_state.result_pending = true;
            s_state.result_until_us = esp_timer_get_time() + (int64_t)EAR_RESULT_MS * 1000;
        } else {
            ear_show_practice_notification(_("不对哦，再试试。"));
        }
        return;
    }

    if (correct) {
        s_state.score++;
        app_manager_show_notification_timeout(_("回答正确！"), EAR_RESULT_MS);
        ear_check_best();
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), _("回答错误，正确答案是%s。"), answer_text);
        app_manager_show_notification_timeout(buf, EAR_RESULT_WRONG_MS);

        /* 答错消耗一条生命；耗尽即游戏结束，展示血量耗尽 3s 后重开一局 */
        if (s_state.lives > 0) {
            s_state.lives--;
        }
        ear_update_lives();
        if (s_state.lives == 0) {
            app_manager_show_notification_timeout(_("三条生命已用完，游戏即将重新开始"), EAR_GAME_OVER_MS);
            ear_set_play_enabled(false);
            s_state.result_pending = true;
            s_state.result_until_us = esp_timer_get_time() + (int64_t)EAR_GAME_OVER_MS * 1000;
            s_state.game_over_restart_pending = true;
            return;
        }
    }

    /* 挑战模式无论对错只作答一次，直接下一题；答错多留 1s 让用户看清正确答案 */
    s_state.total++;
    ear_update_score_label();
    ear_set_play_enabled(false);
    s_state.result_pending = true;
    s_state.result_until_us = esp_timer_get_time() +
        (int64_t)(correct ? EAR_RESULT_MS : EAR_RESULT_WRONG_MS) * 1000;
}

static void ear_handle_key_answer(lv_obj_t *btnm)
{
    if (s_state.result_pending) {
        return;
    }

    uint32_t btn = lv_buttonmatrix_get_selected_button(btnm);
    if (btn == LV_BUTTONMATRIX_BUTTON_NONE) {
        return;
    }

    int pc = ear_key_to_pitch_class(btnm, btn);
    if (pc < 0) {
        return;
    }

    /* 试听所答音：pc 是 0-11 音级，须落到目标音同八度才是可闻音高，
     * 直接播 pc 会是 MIDI 0-11（8-16Hz 不可闻） */
    ear_play_note((uint8_t)((s_state.target_note / 12) * 12 + pc), EAR_NOTE_DURATION_MS, 0);
    bool correct = ((s_state.target_note % 12) == (uint8_t)pc);

    char name[8];
    ear_note_name(s_state.target_note, name, sizeof(name));
    ear_handle_answer(correct, name);
}

static void ear_handle_interval_answer(lv_obj_t *btnm)
{
    if (s_state.result_pending) {
        return;
    }

    uint32_t btn = lv_buttonmatrix_get_selected_button(btnm);
    if (btn == LV_BUTTONMATRIX_BUTTON_NONE || btn >= 12) {
        return;
    }

    uint8_t selected = (uint8_t)(btn + 1);
    bool correct = (selected == s_state.interval);
    ear_handle_answer(correct, _(s_interval_names[s_state.interval]));
}

/* -------------------- 题目生命周期 -------------------- */

static void ear_play_current_question(uint32_t base_delay_ms)
{
    if (s_state.mode == EAR_MODE_ABSOLUTE) {
        ear_play_note(s_state.target_note, EAR_NOTE_DURATION_MS, base_delay_ms);
    } else {
        uint8_t target = s_state.ascending ?
                         s_state.root_note + s_state.interval :
                         s_state.root_note - s_state.interval;
        /* 根音 → 500ms 静默 → 目标音 */
        ear_play_note(s_state.root_note, EAR_NOTE_DURATION_MS, base_delay_ms);
        ear_play_note(target, EAR_NOTE_DURATION_MS,
                      base_delay_ms + EAR_NOTE_DURATION_MS + EAR_INTERVAL_GAP_MS);
    }
}

static void ear_next_question(bool auto_play)
{
    s_state.tries_left = EAR_MAX_TRIES;
    s_state.result_pending = false;

    if (s_state.mode == EAR_MODE_ABSOLUTE) {
        s_state.target_note = ear_generate_absolute_target();
    } else {
        ear_generate_relative_question();
    }

    ear_update_try_label();
    ear_update_score_label();
    ear_set_play_enabled(true);

    if (auto_play) {
        ear_play_current_question(EAR_AUTOPLAY_DELAY_MS);
    }

    if (s_state.practice_mode) {
        /* 练习模式：出题即常驻展示答案与点击引导，timeout 0 防错过 */
        ear_show_practice_notification("");
    } else {
        app_manager_show_notification_timeout(EAR_CHALLENGE_HINT, 0);
    }
}

/* 切换模式/难度/练习挑战均视为新开局：清分恢复血量、应用控件可见性、
 * 重发首题与引导通知，因此切模式后引导无断档 */
static void ear_restart_game(void)
{
    s_state.score = 0;
    s_state.total = 0;
    s_state.lives = EAR_MAX_LIVES;
    s_state.result_pending = false;
    s_state.game_over_restart_pending = false;
    ear_stop_all_notes();
    ear_apply_mode_ui();
    ear_apply_difficulty_ui();
    ear_apply_stat_visibility();
    ear_update_lives();
    ear_update_best_label();
    ear_next_question(true);
}

/* -------------------- App 生命周期回调 -------------------- */

static void app_ear_trainer_home_cb(lv_event_t *e);
static void app_ear_trainer_event_cb(lv_event_t *e);

static bool app_ear_trainer_on_init(app_base_t *self, void *screen_ctx)
{
    (void)self;
    (void)screen_ctx;

    memset(&s_state, 0, sizeof(s_state));
    /* 从 NVS 恢复上次选择的模式/难度/练习挑战；系统参数在 service_nvs_init 已读入 */
    s_state.mode = (ear_mode_t)system_parameters.ear_mode;
    s_state.difficulty = (ear_difficulty_t)system_parameters.ear_difficulty;
    s_state.practice_mode = system_parameters.ear_practice_mode;
    /* 防御性边界：NVS 中若写入了非法值则回默认 */
    if (s_state.mode > EAR_MODE_RELATIVE)          s_state.mode = EAR_MODE_ABSOLUTE;
    if (s_state.difficulty > EAR_DIFF_HARD)        s_state.difficulty = EAR_DIFF_EASY;

    s_state.welcome_until_us = esp_timer_get_time() + (int64_t)EAR_WELCOME_MS * 1000;
    s_state.welcome_switched = false;

    ear_reset_timbre();

    /* 新开局：重置分数/血量、应用控件可见性、生成首题并贴引导通知 */
    ear_restart_game();

    /* 欢迎词常驻到欢迎期结束，由 on_update 切换回答案/引导通知，全程无提示空窗 */
    app_manager_show_notification_timeout(s_state.practice_mode ?
                                          EAR_WELCOME_PRACTICE : EAR_WELCOME_CHALLENGE, 0);

    ESP_LOGI(TAG, "init");

    ui_screen_ear_t *ui = (ui_screen_ear_t *)screen_ctx;
    lvgl_port_lock(portMAX_DELAY);
    if (ui->btn_home != NULL) {
        lv_obj_add_event_cb(ui->btn_home, app_ear_trainer_home_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui->ear_key_try_play != NULL) {
        lv_obj_add_event_cb(ui->ear_key_try_play, app_ear_trainer_event_cb, LV_EVENT_PRESSED, self);
    }
    if (ui->ear_mode != NULL) {
        /* 先同步选中态再注册回调，避免 set_selected 触发伪切换 */
        lv_dropdown_set_selected(ui->ear_mode, s_state.mode);
        lv_obj_add_event_cb(ui->ear_mode, app_ear_trainer_event_cb, LV_EVENT_VALUE_CHANGED, self);
    }
    if (ui->ear_trainer_test != NULL) {
        /* 默认选中练习模式（索引 1）；先设置再注册回调，避免 set_selected
         * 触发的伪 VALUE_CHANGED 进入模式切换逻辑 */
        lv_dropdown_set_selected(ui->ear_trainer_test, s_state.practice_mode ? 1 : 0);
        lv_obj_add_event_cb(ui->ear_trainer_test, app_ear_trainer_event_cb, LV_EVENT_VALUE_CHANGED, self);
    }
    if (ui->ear_difficult != NULL) {
        lv_dropdown_set_selected(ui->ear_difficult, s_state.difficulty);
        lv_obj_add_event_cb(ui->ear_difficult, app_ear_trainer_event_cb, LV_EVENT_VALUE_CHANGED, self);
    }
    if (ui->ear_key_major != NULL) {
        lv_obj_add_event_cb(ui->ear_key_major, app_ear_trainer_event_cb, LV_EVENT_VALUE_CHANGED, self);
    }
    if (ui->ear_key_minor2 != NULL) {
        lv_obj_add_event_cb(ui->ear_key_minor2, app_ear_trainer_event_cb, LV_EVENT_VALUE_CHANGED, self);
    }
    if (ui->ear_key_minor3 != NULL) {
        lv_obj_add_event_cb(ui->ear_key_minor3, app_ear_trainer_event_cb, LV_EVENT_VALUE_CHANGED, self);
    }
    if (ui->ear_key_interval != NULL) {
        /* 动态翻译 ear_key_interval 按钮矩阵文字：EEZ buttonmatrix 不支持多语言配置，
         * 运行时按当前语言重建 map。
         * Trap-1: lv_buttonmatrix_set_map 只存指针不拷贝，map 数组必须 static。
         * Trap-2: 二次进入时 get_map 返回的是上次已翻译的 map，按中文表 strcmp 会失配；
         *         经 _() 双向查表（中/英可互反查）可幂等重译。 */
        static const char *s_translated_map[16];
        const char * const *orig_map = lv_buttonmatrix_get_map(ui->ear_key_interval);
        int idx = 0;
        if (orig_map != NULL) {
            for (int i = 0; orig_map[i] != NULL && idx < 15; i++) {
                const char *s = orig_map[i];
                if (s[0] == '\0' || (s[0] == '\n' && s[1] == '\0')) {
                    s_translated_map[idx++] = s;   /* 换行/空占位原样保留 */
                } else {
                    s_translated_map[idx++] = _(s);
                }
            }
        }
        s_translated_map[idx] = NULL;
        lv_buttonmatrix_set_map(ui->ear_key_interval, s_translated_map);
        lv_obj_add_event_cb(ui->ear_key_interval, app_ear_trainer_event_cb, LV_EVENT_VALUE_CHANGED, self);
    }
    lvgl_port_unlock();

    return true;
}

static void app_ear_trainer_on_ui_event(app_base_t *self, lv_event_t *e)
{
    (void)self;
    ui_screen_ear_t *ui = &s_ear_ui;
    lv_obj_t *target = lv_event_get_target_obj(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (target == ui->ear_key_try_play && code == LV_EVENT_PRESSED) {
        if (s_state.result_pending) {
            return;
        }

        /* 练习模式无限试听，不扣减次数 */
        if (s_state.practice_mode) {
            ear_play_current_question(0);
            return;
        }

        if (s_state.tries_left == 0) {
            return;
        }

        s_state.tries_left--;
        ear_update_try_label();
        ear_play_current_question(0);

        if (s_state.tries_left == 0) {
            ear_set_play_enabled(false);
            app_manager_show_notification_timeout(_("没有试听次数啦，试着做出选择吧~"), 0);
        }
        return;
    }

    if (code == LV_EVENT_VALUE_CHANGED) {
        if (target == ui->ear_mode) {
            /* EEZ 选项顺序：0=绝对音感 1=相对音感，与 ear_mode_t 一致 */
            ear_mode_t mode = (ear_mode_t)lv_dropdown_get_selected(ui->ear_mode);
            if (mode != s_state.mode) {
                s_state.mode = mode;
                ear_restart_game();
            }
            return;
        }

        if (target == ui->ear_difficult) {
            /* EEZ 选项顺序：0=初级 1=中级 2=高级，与 ear_difficulty_t 一致 */
            ear_difficulty_t diff = (ear_difficulty_t)lv_dropdown_get_selected(ui->ear_difficult);
            if (diff != s_state.difficulty) {
                s_state.difficulty = diff;
                ear_restart_game();
            }
            return;
        }

        if (target == ui->ear_trainer_test) {
            /* EEZ 选项顺序：0="= 挑战 ="  1="= 练习=" */
            bool new_practice = (lv_dropdown_get_selected(ui->ear_trainer_test) == 1);
            if (new_practice != s_state.practice_mode) {
                s_state.practice_mode = new_practice;
                /* 切模式后跳过欢迎流程：ear_restart_game 会重新应用控件
                 * 可见性并立即重贴对应模式的引导通知，引导不断档 */
                s_state.welcome_switched = true;
                ear_restart_game();
            }
            return;
        }

        if (target == ui->ear_key_major ||
            target == ui->ear_key_minor2 ||
            target == ui->ear_key_minor3) {
            ear_handle_key_answer(target);
            return;
        }

        if (target == ui->ear_key_interval) {
            ear_handle_interval_answer(target);
            return;
        }
    }
}

static void app_ear_trainer_event_cb(lv_event_t *e)
{
    app_base_t *self = (app_base_t *)lv_event_get_user_data(e);
    app_ear_trainer_on_ui_event(self, e);
}

static void app_ear_trainer_home_cb(lv_event_t *e)
{
    (void)e;
    app_manager_request_kill_active();
}

static void app_ear_trainer_process_active_notes(void)
{
    if (s_active_note_count == 0) {
        return;
    }

    int64_t now = esp_timer_get_time();
    int write = 0;

    for (int i = 0; i < s_active_note_count; i++) {
        ear_active_note_t *n = &s_active_notes[i];

        if (!n->started && now >= n->on_at_us) {
            ear_midi_note(n->note, 100);
            n->started = true;
        }

        if (n->started && now >= n->off_at_us) {
            ear_midi_note(n->note, 0);
        } else {
            if (write != i) {
                s_active_notes[write] = s_active_notes[i];
            }
            write++;
        }
    }

    s_active_note_count = write;
}

static void app_ear_trainer_on_update(app_base_t *self)
{
    (void)self;
    app_ear_trainer_process_active_notes();

    /* 生命耗尽后先展示 3s 再重开 */
    if (s_state.game_over_restart_pending &&
        esp_timer_get_time() >= s_state.result_until_us) {
        s_state.game_over_restart_pending = false;
        s_state.result_pending = false;
        ear_restart_game();
        return;
    }

    /* 结果展示约 1s 后进入下一题；下一题已负责刷新提示，这里不再覆盖 */
    if (s_state.result_pending &&
        esp_timer_get_time() >= s_state.result_until_us) {
        ear_next_question(true);
    }

    /* 欢迎期结束后切回答案/引导通知；若用户已作答，通知已不是
     * 欢迎词（前缀匹配失败），不会被覆盖 */
    if (!s_state.welcome_switched &&
        esp_timer_get_time() >= s_state.welcome_until_us) {
        s_state.welcome_switched = true;
        char cur[128];
        if (app_manager_get_notification(cur, sizeof(cur)) == ESP_OK &&
            (strcmp(cur, EAR_WELCOME_PRACTICE) == 0 || strcmp(cur, EAR_WELCOME_CHALLENGE) == 0)) {
            if (s_state.practice_mode) {
                ear_show_practice_notification("");
            } else {
                app_manager_show_notification_timeout(EAR_CHALLENGE_HINT, 0);
            }
        }
    }
}

static void app_ear_trainer_on_pause(app_base_t *self)
{
    (void)self;
    ear_stop_all_notes();
    ESP_LOGI(TAG, "pause");
}

static void app_ear_trainer_on_resume(app_base_t *self)
{
    (void)self;
    ESP_LOGI(TAG, "resume");
}

static void app_ear_trainer_on_destroy(app_base_t *self)
{
    (void)self;
    ear_stop_all_notes();

    /* 持久化当前模式/难度/练习挑战，下次进入时恢复 */
    service_nvs_set_ear_cfg((uint8_t)s_state.mode,
                            (uint8_t)s_state.difficulty,
                            s_state.practice_mode);
    service_nvs_commit();

    /* 移除 on_init 注册的事件回调：EEZ 屏幕对象持久存在，
     * 不移除会在再次进入时重复注册导致一次事件多次触发（判题/试听重复） */
    lvgl_port_lock(portMAX_DELAY);
    if (s_ear_ui.ear_key_try_play != NULL) {
        lv_obj_remove_event_cb(s_ear_ui.ear_key_try_play, app_ear_trainer_event_cb);
    }
    if (s_ear_ui.ear_mode != NULL) {
        lv_obj_remove_event_cb(s_ear_ui.ear_mode, app_ear_trainer_event_cb);
    }
    if (s_ear_ui.ear_difficult != NULL) {
        lv_obj_remove_event_cb(s_ear_ui.ear_difficult, app_ear_trainer_event_cb);
    }
    if (s_ear_ui.ear_key_major != NULL) {
        lv_obj_remove_event_cb(s_ear_ui.ear_key_major, app_ear_trainer_event_cb);
    }
    if (s_ear_ui.ear_key_minor2 != NULL) {
        lv_obj_remove_event_cb(s_ear_ui.ear_key_minor2, app_ear_trainer_event_cb);
    }
    if (s_ear_ui.ear_key_minor3 != NULL) {
        lv_obj_remove_event_cb(s_ear_ui.ear_key_minor3, app_ear_trainer_event_cb);
    }
    if (s_ear_ui.ear_key_interval != NULL) {
        lv_obj_remove_event_cb(s_ear_ui.ear_key_interval, app_ear_trainer_event_cb);
    }
    if (s_ear_ui.btn_home != NULL) {
        lv_obj_remove_event_cb(s_ear_ui.btn_home, app_ear_trainer_home_cb);
    }
    if (s_ear_ui.ear_trainer_test != NULL) {
        lv_obj_remove_event_cb(s_ear_ui.ear_trainer_test, app_ear_trainer_event_cb);
    }
    lvgl_port_unlock();

    ESP_LOGI(TAG, "destroy");
}

esp_err_t app_ear_trainer_register(void)
{
    static app_base_t app = {
        .name = "Ear Trainer",
        .screen_name = "app_ear_train",
        .screen_ctx = &s_ear_ui,
        .screen_ctx_size = sizeof(s_ear_ui),
        .widget_bindings = s_ear_bindings,
        .on_init = app_ear_trainer_on_init,
        .on_update = app_ear_trainer_on_update,
        .on_pause = app_ear_trainer_on_pause,
        .on_resume = app_ear_trainer_on_resume,
        .on_destroy = app_ear_trainer_on_destroy,
        .on_ui_event = app_ear_trainer_on_ui_event,
    };
    return app_manager_register(&app);
}
