/**
 * @file app_zen.c
 * @brief 禅模式 App：弹珠与雨滴两种环境音生成玩法
 *
 * - 弹珠：小球在全屏音墙内匀速反弹，撞击四边音墙段发声；
 *   音墙由 24 条分段 lv_line 拼成整圈（单条 line 仅支持单色，分色必须分段），
 *   段色沿用旧 canvas 方案的按边分色；模式重载时一次性生成端点。
 * - 雨滴：无边框，雨滴在全屏范围内下落，屏幕靠下专用 canvas 中随机分布短墙，
 *   雨滴撞击 canvas 内短墙反弹并发声。
 * 调式与钢琴一致（大调/小调/中国五声/埃及调式/多利亚/日本调式），
 * 音色通过 Program Change 切换并持久化到 NVS。
 */

#include "app_zen.h"
#include "app_manager.h"
#include "engine_gui.h"
#include "engine_midi.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_lvgl_port.h"
#include "service_i18n.h"
#include "service_nvs.h"
#include "lvgl.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

static const char *TAG = "app_zen";

/* 逻辑屏幕尺寸 */
#define ZEN_SCREEN_W        1280
#define ZEN_SCREEN_H        720

/* 顶部标题/按钮区域高度，音墙从该高度下方开始 */
#define ZEN_TOP_BAR_H       150

/* 弹珠模式音墙矩形范围：x 两端距屏边 64，y 起自标题栏下方、底部同样预留 64 */
#define ZEN_WALL_LEFT       64
#define ZEN_WALL_TOP        ZEN_TOP_BAR_H
#define ZEN_WALL_RIGHT      (ZEN_SCREEN_W - 64)
#define ZEN_WALL_BOTTOM     (ZEN_SCREEN_H - 64)

/* 弹珠活动区在音墙内侧 */
#define ZEN_BALL_MAX        4
#define ZEN_BALL_RADIUS     14
#define ZEN_SEG_TOP_BOTTOM  8
#define ZEN_SEG_SIDES       4
#define ZEN_WALL_COUNT      (2 * (ZEN_SEG_TOP_BOTTOM + ZEN_SEG_SIDES))
#define ZEN_WALL_THICK      20  /* 与 EEZ 示例线 zen_line_1 线宽一致 */
#define ZEN_SEG_END_GAP     10  /* 每段两端内缩量：间隙需明显大于线宽圆角，否则段间视觉相连 */
#define ZEN_HIT_COOLDOWN_MS 120

#define ZEN_LED_COUNT       5
#define ZEN_LED_SIZE        30

/* 雨滴模式：canvas 位于屏幕靠下，动态尺寸；雨滴运动范围为全屏 */
#define ZEN_DROP_MAX        5
#define ZEN_DROP_WALL_MAX   32
#define ZEN_DROP_GRAVITY    900.0f
#define ZEN_DROP_BOUNCE     0.7f
#define ZEN_DROP_SPAWN_MIN_MS  300
#define ZEN_DROP_SPAWN_RND_MS  500

/* 引力引擎：球间相互吸引，弯曲轨迹产生轨道/散射效果 */
#define ZEN_GRAVITY_RANGE   100     /* 引力作用范围（px） */
#define ZEN_GRAVITY_K       5000    /* 引力系数，a = K / r² */
#define ZEN_GRAVITY_MAX_A   1200    /* 最大引力加速度上限，避免数值爆炸 */

#define ZEN_UPDATE_HZ       30
#define ZEN_NOTE_OFF_MS     180
#define ZEN_BASE_NOTE       48  /* C3 */

#define ZEN_SPEED_LEVELS    5

/* 主题配色槽位（语义见 EEZ 主题定义） */
#define ZEN_C_BG        COLOR_BG_PRIMARY
#define ZEN_C_TEXT      COLOR_TEXT_PRIMARY
#define ZEN_C_DIM       COLOR_TEXT_SECONDARY
#define ZEN_C_TOP       COLOR_SUCCESS
#define ZEN_C_RIGHT     COLOR_PRIMARY
#define ZEN_C_BOTTOM    COLOR_SECONDARY
#define ZEN_C_LEFT      COLOR_ERROR
#define ZEN_C_FRAME     COLOR_DISABLE

static const float s_ball_speed_table[ZEN_SPEED_LEVELS] = {
    120.0f, 240.0f, 360.0f, 480.0f, 600.0f
};

/* 小球取色循环表 */
static const uint8_t s_ball_colors[] = {
    ZEN_C_LEFT, ZEN_C_RIGHT, ZEN_C_TOP, ZEN_C_BOTTOM, 10
};

/* 音色映射：下拉索引 0~3 -> GM Program */
static const uint8_t s_sound_programs[] = { 0, 9, 10, 77 };
#define ZEN_SOUND_COUNT  (sizeof(s_sound_programs) / sizeof(s_sound_programs[0]))

/* 大调 / 自然小调 / 中国五声（宫） / 埃及调式 / 多利亚 / 日本调式（都节）
 * 顺序与 EEZ zen_dropdown_key 选项及 piano 严格一致 */
static const int8_t s_steps_major[]   = {0, 2, 4, 5, 7, 9, 11};
static const int8_t s_steps_minor[]   = {0, 2, 3, 5, 7, 8, 10};
static const int8_t s_steps_guofeng[] = {0, 2, 4, 7, 9};
static const int8_t s_steps_egypt[]   = {0, 2, 5, 7, 10};
static const int8_t s_steps_dorian[]  = {0, 2, 3, 5, 7, 9, 10};
static const int8_t s_steps_japan[]   = {0, 1, 5, 7, 8};

typedef struct {
    const int8_t *steps;
    uint8_t len;
} zen_scale_t;

static const zen_scale_t s_scales[] = {
    { s_steps_major,   7 },
    { s_steps_minor,   7 },
    { s_steps_guofeng, 5 },
    { s_steps_egypt,   5 },
    { s_steps_dorian,  7 },
    { s_steps_japan,   5 },
};
#define ZEN_SCALE_COUNT  (sizeof(s_scales) / sizeof(s_scales[0]))

typedef struct {
    int16_t x, y, w, h;
    uint8_t color_idx;
    uint8_t note;
    char name[6];
    uint32_t active_ms;
} zen_wall_t;

typedef struct {
    float x, y, vx, vy;
    uint8_t color_idx;
    bool active;
    uint8_t playing_note;
    uint32_t note_on_ms;
    bool note_playing;
    uint32_t last_wall_hit_ms;   /* 最近一次碰墙时间，仅雨滴使用 */
} zen_ball_t;

typedef zen_ball_t zen_drop_t;

typedef struct {
    uint8_t mode;       /* 0 弹珠 1 雨滴 */
    uint8_t key_sel;    /* 0~5 调式索引 */
    uint8_t speed_sel;  /* 弹珠：速度档；雨滴：同时下落个数-1 */
    uint8_t sound_sel;  /* 音色索引 0~3 */
    zen_ball_t balls[ZEN_BALL_MAX];
    zen_wall_t walls[ZEN_WALL_COUNT];
    zen_drop_t drops[ZEN_DROP_MAX];
    zen_wall_t drop_walls[ZEN_DROP_WALL_MAX];
    int drop_wall_count;
    uint32_t next_spawn_ms;
    uint32_t last_update_ms;
    void *canvas_buf;
    int16_t canvas_x, canvas_y;
    int16_t canvas_w, canvas_h;
    bool canvas_ready;
    bool recording_self;        /* 本 App 发起的录制 */
    bool recording_stop_pending;  /* 录制已停止，等待 finalize 后提示 */
} zen_state_t;

typedef struct {
    lv_obj_t *line;              /* 弹珠模式全屏音墙 */
    lv_obj_t *canvas;            /* 雨滴模式底部 key 墙 */
    lv_obj_t *dropdown_mode;
    lv_obj_t *dropdown_key;
    lv_obj_t *dropdown_speed;
    lv_obj_t *dropdown_sound;
    lv_obj_t *balls[ZEN_LED_COUNT];
    lv_obj_t *btn_home;
    lv_obj_t *btn_rec;
    lv_obj_t *btn_set;
    lv_obj_t *set;
    lv_obj_t *set_btn_return;
} ui_screen_zen_t;

static ui_screen_zen_t s_zen_ui = {0};

static const widget_binding_t s_zen_bindings[] = {
    WIDGET_BIND(ui_screen_zen_t, line,            "zen_line_1",          WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_zen_t, canvas,          "zen_canvas",          WIDGET_KIND_CANVAS),
    WIDGET_BIND(ui_screen_zen_t, dropdown_mode,   "zen_dropdown_mode",   WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_zen_t, dropdown_key,    "zen_dropdown_key",    WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_zen_t, dropdown_speed,  "zen_dropdown_speed",  WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_zen_t, dropdown_sound,  "zen_dropdown_sound",  WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_zen_t, balls[0],        "zen_ball_0",          WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_zen_t, balls[1],        "zen_ball_1",          WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_zen_t, balls[2],        "zen_ball_2",          WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_zen_t, balls[3],        "zen_ball_3",          WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_zen_t, balls[4],        "zen_ball_4",          WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_zen_t, btn_home,        "zen_btn_home",        WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_zen_t, btn_rec,         "zen_btn_rec",         WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_zen_t, btn_set,         "zen_btn_set",         WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_zen_t, set,             "zen_set",             WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_zen_t, set_btn_return,  "zen_set_btn_return",  WIDGET_KIND_ANY),
    WIDGET_BINDING_END,
};

static zen_state_t s_zen = {0};

/* 弹珠音墙分段线（每段一条 lv_line）。
 * Trap: lv_line_set_points 只存指针不拷贝，端点数组必须持久 */
static lv_obj_t *s_seg_lines[ZEN_WALL_COUNT];
static lv_point_precise_t s_seg_points[ZEN_WALL_COUNT][2];

static uint32_t zen_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static float zen_rand_f(float min, float max)
{
    uint32_t r = esp_random();
    float t = (float)r / (float)UINT32_MAX;
    return min + t * (max - min);
}

static uint8_t zen_note_for_degree(int degree)
{
    const zen_scale_t *sc = &s_scales[s_zen.key_sel];
    int oct = degree / sc->len;
    int idx = degree % sc->len;
    int note = ZEN_BASE_NOTE + 12 * oct + sc->steps[idx];
    if (note > 127) {
        note = 127;
    }
    return (uint8_t)note;
}

static void zen_note_name(uint8_t note, char *out, size_t len)
{
    static const char names[12][3] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    snprintf(out, len, "%s%d", names[note % 12], (int)(note / 12) - 1);
}

static void zen_send_note_on(uint8_t note, uint8_t velocity)
{
    engine_midi_event_t midi = {0};
    midi.type = ENGINE_MIDI_MSG_NOTE_ON;
    midi.channel = 0;
    midi.data1 = note;
    midi.data2 = velocity;
    midi.source_port = ENGINE_MIDI_PORT_APP;
    engine_midi_publish(&midi, 0);
}

static void zen_send_note_off(uint8_t note)
{
    engine_midi_event_t midi = {0};
    midi.type = ENGINE_MIDI_MSG_NOTE_OFF;
    midi.channel = 0;
    midi.data1 = note;
    midi.data2 = 0;
    midi.source_port = ENGINE_MIDI_PORT_APP;
    engine_midi_publish(&midi, 0);
}

/* 槽位单音限制：新音触发前必须关闭该槽位正在发声的音，note_off 由 zen_update 统一计时 */
static void zen_note_trigger(uint8_t note, uint8_t velocity,
                             uint8_t *playing_note, uint32_t *note_on_ms,
                             bool *note_playing)
{
    if (*note_playing) {
        zen_send_note_off(*playing_note);
    }
    zen_send_note_on(note, velocity);
    *playing_note = note;
    *note_on_ms = zen_now_ms();
    *note_playing = true;
}

static void zen_note_timed_off(uint32_t now,
                               uint8_t *playing_note, uint32_t *note_on_ms,
                               bool *note_playing)
{
    if (*note_playing && (now - *note_on_ms) > ZEN_NOTE_OFF_MS) {
        zen_send_note_off(*playing_note);
        *note_playing = false;
    }
}

/* -------------------- 音色切换与参数持久化 -------------------- */

static void zen_set_sound_type(uint8_t sound_sel)
{
    if (sound_sel >= ZEN_SOUND_COUNT) {
        sound_sel = 0;
    }

    engine_midi_event_t evt = {
        .type = ENGINE_MIDI_MSG_CONTROL_CHANGE,
        .channel = 0,
        .data1 = 0,   /* CC0 Bank MSB */
        .data2 = 0,
        .source_port = ENGINE_MIDI_PORT_APP,
    };
    engine_midi_publish(&evt, 0);

    evt.data1 = 32;  /* CC32 Bank LSB */
    engine_midi_publish(&evt, 0);

    evt.type = ENGINE_MIDI_MSG_PROGRAM_CHANGE;
    evt.data1 = s_sound_programs[sound_sel];
    engine_midi_publish(&evt, 0);

    ESP_LOGI(TAG, "sound_type %d -> program %d", sound_sel, evt.data1);
}

static void zen_load_params(void)
{
    service_nvs_zen_t params;
    service_nvs_get_zen(&params);

    s_zen.mode = params.mode;
    if (s_zen.mode > 1) {
        s_zen.mode = 0;
    }

    s_zen.key_sel = params.key_sel;
    if (s_zen.key_sel >= ZEN_SCALE_COUNT) {
        s_zen.key_sel = 0;
    }

    s_zen.speed_sel = params.speed_sel;
    if (s_zen.speed_sel >= ZEN_SPEED_LEVELS) {
        s_zen.speed_sel = 2;
    }

    s_zen.sound_sel = params.sound_sel;
    if (s_zen.sound_sel >= ZEN_SOUND_COUNT) {
        s_zen.sound_sel = 0;
    }
}

static void zen_save_params(void)
{
    service_nvs_zen_t params = {
        .mode = s_zen.mode,
        .key_sel = s_zen.key_sel,
        .speed_sel = s_zen.speed_sel,
        .sound_sel = s_zen.sound_sel,
    };
    service_nvs_set_zen(&params);
}

/* -------------------- 弹珠模式：全屏 line 音墙 -------------------- */

/* 生成矩形音墙的逻辑墙段与 24 条分段 lv_line 端点。
 * 段色沿用旧 canvas 方案的按边分色；分段线控件惰性创建一次，之后仅更新端点与颜色，
 * 显隐统一由 zen_apply_mode_ui 控制。 */
static void zen_init_walls(void)
{
    float left = ZEN_WALL_LEFT;
    float right = ZEN_WALL_RIGHT;
    float top = ZEN_WALL_TOP;
    float bottom = ZEN_WALL_BOTTOM;
    float seg_w = (right - left) / ZEN_SEG_TOP_BOTTOM;
    float seg_h = (bottom - top) / ZEN_SEG_SIDES;

    /* 24 段各配一个音级（每音级两次），洗牌后分布整圈 */
    uint8_t degrees[ZEN_WALL_COUNT];
    for (int i = 0; i < ZEN_WALL_COUNT; i++) {
        degrees[i] = (uint8_t)(i / 2);
    }
    for (int i = ZEN_WALL_COUNT - 1; i > 0; i--) {
        int j = (int)(esp_random() % (uint32_t)(i + 1));
        uint8_t tmp = degrees[i];
        degrees[i] = degrees[j];
        degrees[j] = tmp;
    }

    /* 填充逻辑墙段（绝对屏幕坐标，供碰撞检测）并计算各段线端点 */
    int w = 0;

    /* 顶边：从左到右，8 段 */
    for (int i = 0; i < ZEN_SEG_TOP_BOTTOM; i++) {
        zen_wall_t *wl = &s_zen.walls[w];
        wl->x = (int16_t)(left + i * seg_w);
        wl->y = (int16_t)(top - ZEN_WALL_THICK / 2);
        wl->w = (int16_t)seg_w;
        wl->h = ZEN_WALL_THICK;
        wl->color_idx = ZEN_C_TOP;
        wl->note = zen_note_for_degree(degrees[w]);
        zen_note_name(wl->note, wl->name, sizeof(wl->name));
        wl->active_ms = 0;
        s_seg_points[w][0].x = (int32_t)(left + i * seg_w + ZEN_SEG_END_GAP);
        s_seg_points[w][0].y = (int32_t)top;
        s_seg_points[w][1].x = (int32_t)(left + (i + 1) * seg_w - ZEN_SEG_END_GAP);
        s_seg_points[w][1].y = (int32_t)top;
        w++;
    }

    /* 右边：从上到下，4 段 */
    for (int i = 0; i < ZEN_SEG_SIDES; i++) {
        zen_wall_t *wl = &s_zen.walls[w];
        wl->x = (int16_t)(right - ZEN_WALL_THICK / 2);
        wl->y = (int16_t)(top + i * seg_h);
        wl->w = ZEN_WALL_THICK;
        wl->h = (int16_t)seg_h;
        wl->color_idx = ZEN_C_RIGHT;
        wl->note = zen_note_for_degree(degrees[w]);
        zen_note_name(wl->note, wl->name, sizeof(wl->name));
        wl->active_ms = 0;
        s_seg_points[w][0].x = (int32_t)right;
        s_seg_points[w][0].y = (int32_t)(top + i * seg_h + ZEN_SEG_END_GAP);
        s_seg_points[w][1].x = (int32_t)right;
        s_seg_points[w][1].y = (int32_t)(top + (i + 1) * seg_h - ZEN_SEG_END_GAP);
        w++;
    }

    /* 底边：从左到右，8 段（与 zen_wall_index_at 段序一致） */
    for (int i = 0; i < ZEN_SEG_TOP_BOTTOM; i++) {
        zen_wall_t *wl = &s_zen.walls[w];
        wl->x = (int16_t)(left + i * seg_w);
        wl->y = (int16_t)(bottom - ZEN_WALL_THICK / 2);
        wl->w = (int16_t)seg_w;
        wl->h = ZEN_WALL_THICK;
        wl->color_idx = ZEN_C_BOTTOM;
        wl->note = zen_note_for_degree(degrees[w]);
        zen_note_name(wl->note, wl->name, sizeof(wl->name));
        wl->active_ms = 0;
        s_seg_points[w][0].x = (int32_t)(left + i * seg_w + ZEN_SEG_END_GAP);
        s_seg_points[w][0].y = (int32_t)bottom;
        s_seg_points[w][1].x = (int32_t)(left + (i + 1) * seg_w - ZEN_SEG_END_GAP);
        s_seg_points[w][1].y = (int32_t)bottom;
        w++;
    }

    /* 左边：从上到下，4 段 */
    for (int i = 0; i < ZEN_SEG_SIDES; i++) {
        zen_wall_t *wl = &s_zen.walls[w];
        wl->x = (int16_t)(left - ZEN_WALL_THICK / 2);
        wl->y = (int16_t)(top + i * seg_h);
        wl->w = ZEN_WALL_THICK;
        wl->h = (int16_t)seg_h;
        wl->color_idx = ZEN_C_LEFT;
        wl->note = zen_note_for_degree(degrees[w]);
        zen_note_name(wl->note, wl->name, sizeof(wl->name));
        wl->active_ms = 0;
        s_seg_points[w][0].x = (int32_t)left;
        s_seg_points[w][0].y = (int32_t)(top + i * seg_h + ZEN_SEG_END_GAP);
        s_seg_points[w][1].x = (int32_t)left;
        s_seg_points[w][1].y = (int32_t)(top + (i + 1) * seg_h - ZEN_SEG_END_GAP);
        w++;
    }

    /* 分段线同步到 LVGL */
    if (s_zen_ui.line == NULL) {
        return;
    }
    lvgl_port_lock(portMAX_DELAY);
    /* 示例线仅作 EEZ 样式模板，永不显示 */
    lv_obj_add_flag(s_zen_ui.line, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *parent = lv_obj_get_parent(s_zen_ui.line);
    for (int i = 0; i < ZEN_WALL_COUNT; i++) {
        if (s_seg_lines[i] == NULL) {
            s_seg_lines[i] = lv_line_create(parent);
            lv_obj_set_style_line_width(s_seg_lines[i], ZEN_WALL_THICK, LV_PART_MAIN);
            lv_obj_set_style_line_rounded(s_seg_lines[i], true, LV_PART_MAIN);
            /* 紧跟示例线插入，保持设置面板/雨滴 canvas/球 LED 的既有层叠关系 */
            lv_obj_move_to_index(s_seg_lines[i],
                                 lv_obj_get_index(s_zen_ui.line) + 1 + i);
        }
        lv_line_set_points(s_seg_lines[i], s_seg_points[i], 2);
        lv_obj_set_style_line_color(s_seg_lines[i],
                                    engine_gui_theme_color(s_zen.walls[i].color_idx),
                                    LV_PART_MAIN);
    }
    lvgl_port_unlock();
}

/* 球心活动边界：墙线内缘（视觉线宽一半）再内收球半径 */
static float zen_ball_min_x(void) { return ZEN_WALL_LEFT + ZEN_WALL_THICK / 2 + ZEN_BALL_RADIUS; }
static float zen_ball_max_x(void) { return ZEN_WALL_RIGHT - ZEN_WALL_THICK / 2 - ZEN_BALL_RADIUS; }
static float zen_ball_min_y(void) { return ZEN_WALL_TOP + ZEN_WALL_THICK / 2 + ZEN_BALL_RADIUS; }
static float zen_ball_max_y(void) { return ZEN_WALL_BOTTOM - ZEN_WALL_THICK / 2 - ZEN_BALL_RADIUS; }

static void zen_spawn_ball(float x, float y)
{
    int slot = -1;
    for (int i = 0; i < ZEN_BALL_MAX; i++) {
        if (!s_zen.balls[i].active) {
            slot = i;
            break;
        }
    }

    /* 满员时淘汰最老的球（数组按生成先后排序），腾位给新球 */
    if (slot < 0) {
        zen_ball_t *oldest = &s_zen.balls[0];
        if (oldest->note_playing) {
            zen_send_note_off(oldest->playing_note);
        }
        memmove(&s_zen.balls[0], &s_zen.balls[1], sizeof(zen_ball_t) * (ZEN_BALL_MAX - 1));
        slot = ZEN_BALL_MAX - 1;
    }

    float min_x = zen_ball_min_x();
    float max_x = zen_ball_max_x();
    float min_y = zen_ball_min_y();
    float max_y = zen_ball_max_y();
    if (x < min_x) x = min_x;
    if (x > max_x) x = max_x;
    if (y < min_y) y = min_y;
    if (y > max_y) y = max_y;

    /* CHANGE: 初始方向限幅，避免角度过于平直 */
    float vx, vy, len;
    do {
        vx = zen_rand_f(-1.0f, 1.0f);
        vy = zen_rand_f(-1.0f, 1.0f);
        len = sqrtf(vx * vx + vy * vy);
    } while (len < 0.5f || fabsf(vx) < 0.35f || fabsf(vy) < 0.35f);
    vx /= len;
    vy /= len;

    zen_ball_t *b = &s_zen.balls[slot];
    b->x = x;
    b->y = y;
    b->vx = vx;
    b->vy = vy;
    b->color_idx = s_ball_colors[esp_random() % 5];
    b->active = true;
    b->note_playing = false;
    b->last_wall_hit_ms = 0;
}

static void zen_init_balls(void)
{
    memset(s_zen.balls, 0, sizeof(s_zen.balls));
    zen_spawn_ball(zen_rand_f(ZEN_SCREEN_W * 0.3f, ZEN_SCREEN_W * 0.7f),
                   zen_rand_f(ZEN_SCREEN_H * 0.3f, ZEN_SCREEN_H * 0.7f));
}

/* 球与某条边碰撞后，按位置求所在音墙段索引；无效返回 -1 */
static int zen_wall_index_at(float x, float y, int side)
{
    float left = ZEN_WALL_LEFT;
    float top = ZEN_WALL_TOP;
    float seg_w = (float)(ZEN_WALL_RIGHT - ZEN_WALL_LEFT) / ZEN_SEG_TOP_BOTTOM;
    float seg_h = (float)(ZEN_WALL_BOTTOM - ZEN_WALL_TOP) / ZEN_SEG_SIDES;
    int seg;

    switch (side) {
    case 0: /* top */
        seg = (int)((x - left) / seg_w);
        if (seg < 0 || seg >= ZEN_SEG_TOP_BOTTOM) return -1;
        return seg;
    case 1: /* right */
        seg = (int)((y - top) / seg_h);
        if (seg < 0 || seg >= ZEN_SEG_SIDES) return -1;
        return ZEN_SEG_TOP_BOTTOM + seg;
    case 2: /* bottom */
        seg = (int)((x - left) / seg_w);
        if (seg < 0 || seg >= ZEN_SEG_TOP_BOTTOM) return -1;
        return ZEN_SEG_TOP_BOTTOM + ZEN_SEG_SIDES + seg;
    default: /* left */
        seg = (int)((y - top) / seg_h);
        if (seg < 0 || seg >= ZEN_SEG_SIDES) return -1;
        return 2 * ZEN_SEG_TOP_BOTTOM + ZEN_SEG_SIDES + seg;
    }
}

/* 两个球之间的碰撞处理：带角度偏置的完美弹性碰撞（等质量） */
static void zen_resolve_ball_collision(zen_ball_t *a, zen_ball_t *b)
{
    float dx = b->x - a->x;
    float dy = b->y - a->y;
    float dist_sq = dx * dx + dy * dy;
    float min_dist = 2.0f * ZEN_BALL_RADIUS;
    if (dist_sq >= min_dist * min_dist) return;

    float dist = sqrtf(dist_sq);
    if (dist < 0.001f) {
        /* 退化情形：两球完全重叠，随机方向推开 */
        float ang = zen_rand_f(0.0f, 6.2832f);
        float px = cosf(ang), py = sinf(ang);
        a->x -= px * min_dist;
        a->y -= py * min_dist;
        b->x += px * min_dist;
        b->y += py * min_dist;
        return;
    }

    float nx = dx / dist;
    float ny = dy / dist;

    /* 分离重叠 */
    float overlap = min_dist - dist;
    a->x -= nx * overlap * 0.5f;
    a->y -= ny * overlap * 0.5f;
    b->x += nx * overlap * 0.5f;
    b->y += ny * overlap * 0.5f;

    /* 相对速度在法线方向投影：正在远离则跳过 */
    float dvx = a->vx - b->vx;
    float dvy = a->vy - b->vy;
    float dot = dvx * nx + dvy * ny;
    if (dot > 0) return;

    /* 角度偏置：旋转法线 ±25°，打破对弹对称性，轨迹发散 */
    float bias = zen_rand_f(-0.4363f, 0.4363f);
    float cb = cosf(bias), sb = sinf(bias);
    float rx = nx * cb - ny * sb;
    float ry = nx * sb + ny * cb;

    /* 完美弹性碰撞（e=1）沿偏置轴 —— 等质量下交换法向速度分量 */
    float dot_r = dvx * rx + dvy * ry;
    a->vx -= dot_r * rx;
    a->vy -= dot_r * ry;
    b->vx += dot_r * rx;
    b->vy += dot_r * ry;

    /* 重新归一化方向向量，维持单位长度（抵消浮点漂移，保持速率恒定） */
    float la = sqrtf(a->vx * a->vx + a->vy * a->vy);
    float lb = sqrtf(b->vx * b->vx + b->vy * b->vy);
    if (la > 0.001f) { a->vx /= la; a->vy /= la; }
    if (lb > 0.001f) { b->vx /= lb; b->vy /= lb; }
}

static void zen_update_balls(uint32_t dt_ms)
{
    uint32_t now = zen_now_ms();
    float dt_s = (float)dt_ms / 1000.0f;
    float speed = s_ball_speed_table[s_zen.speed_sel];
    float min_x = zen_ball_min_x();
    float max_x = zen_ball_max_x();
    float min_y = zen_ball_min_y();
    float max_y = zen_ball_max_y();
    static uint32_t s_last_hit_ms[ZEN_BALL_MAX];

    for (int i = 0; i < ZEN_BALL_MAX; i++) {
        zen_ball_t *b = &s_zen.balls[i];
        if (!b->active) {
            continue;
        }

        zen_note_timed_off(now, &b->playing_note, &b->note_on_ms, &b->note_playing);

        /* 引力引擎：球间相互吸引，弯曲轨迹（方向改变、速率不变） */
        for (int j = 0; j < ZEN_BALL_MAX; j++) {
            if (j == i || !s_zen.balls[j].active) continue;
            float dx = s_zen.balls[j].x - b->x;
            float dy = s_zen.balls[j].y - b->y;
            float dist_sq = dx * dx + dy * dy;
            if (dist_sq >= ZEN_GRAVITY_RANGE * ZEN_GRAVITY_RANGE) continue;
            float dist = sqrtf(dist_sq);
            if (dist < 1.0f) continue;
            /* 引力加速度 a = K / r²，沿连线方向 */
            float a = ZEN_GRAVITY_K / dist_sq;
            if (a > ZEN_GRAVITY_MAX_A) a = ZEN_GRAVITY_MAX_A;
            float ux = dx / dist;
            float uy = dy / dist;
            b->vx += ux * a * dt_s;
            b->vy += uy * a * dt_s;
        }
        /* 引力弯曲后重新归一化方向，维持单位速率 */
        float len = sqrtf(b->vx * b->vx + b->vy * b->vy);
        if (len > 0.001f) { b->vx /= len; b->vy /= len; }

        b->x += b->vx * speed * dt_s;
        b->y += b->vy * speed * dt_s;

        int side = -1;
        if (b->x < min_x) {
            b->x = min_x;
            b->vx = fabsf(b->vx);
            side = 3;
        } else if (b->x > max_x) {
            b->x = max_x;
            b->vx = -fabsf(b->vx);
            side = 1;
        }
        if (b->y < min_y) {
            b->y = min_y;
            b->vy = fabsf(b->vy);
            side = 0;
        } else if (b->y > max_y) {
            b->y = max_y;
            b->vy = -fabsf(b->vy);
            side = 2;
        }

        if (side < 0 || (now - s_last_hit_ms[i]) < ZEN_HIT_COOLDOWN_MS) {
            continue;
        }

        int wi = zen_wall_index_at(b->x, b->y, side);
        if (wi < 0) {
            continue;
        }
        s_last_hit_ms[i] = now;
        s_zen.walls[wi].active_ms = now;

        uint8_t vel = 110;
        zen_note_trigger(s_zen.walls[wi].note, vel,
                         &b->playing_note, &b->note_on_ms, &b->note_playing);
    }

    /* 球间碰撞检测 */
    for (int i = 0; i < ZEN_BALL_MAX; i++) {
        if (!s_zen.balls[i].active) continue;
        for (int j = i + 1; j < ZEN_BALL_MAX; j++) {
            if (!s_zen.balls[j].active) continue;
            zen_resolve_ball_collision(&s_zen.balls[i], &s_zen.balls[j]);
        }
    }
}

/* -------------------- 雨滴模式：全屏运动 + 底部 canvas key 墙 -------------------- */

/* canvas 尺寸与屏幕坐标由布局决定，运行期读取后再分配 buffer。
 * 返回 true 表示新建/重建了 buffer（或坐标变化），调用方需重绘。 */
static bool zen_canvas_ensure_buffer(void)
{
    if (s_zen_ui.canvas == NULL) {
        return false;
    }

    lv_area_t coords;
    lvgl_port_lock(portMAX_DELAY);
    lv_obj_update_layout(s_zen_ui.canvas);
    lv_obj_get_coords(s_zen_ui.canvas, &coords);
    lvgl_port_unlock();

    int w = (int)lv_area_get_width(&coords);
    int h = (int)lv_area_get_height(&coords);
    if (w <= 0 || h <= 0) {
        return false;
    }

    if (s_zen.canvas_buf != NULL && s_zen.canvas_w == w && s_zen.canvas_h == h &&
        s_zen.canvas_x == coords.x1 && s_zen.canvas_y == coords.y1) {
        return false;
    }

    void *buf = heap_caps_calloc(1, (size_t)w * h * 2, MALLOC_CAP_SPIRAM);
    if (buf == NULL) {
        ESP_LOGE(TAG, "canvas buf alloc failed");
        return false;
    }

    void *old = s_zen.canvas_buf;
    lvgl_port_lock(portMAX_DELAY);
    lv_canvas_set_buffer(s_zen_ui.canvas, buf, (uint16_t)w, (uint16_t)h,
                         LV_COLOR_FORMAT_RGB565);
    if (old != NULL) {
        heap_caps_free(old);
    }
    lvgl_port_unlock();

    s_zen.canvas_buf = buf;
    s_zen.canvas_w = (int16_t)w;
    s_zen.canvas_h = (int16_t)h;
    s_zen.canvas_x = (int16_t)coords.x1;
    s_zen.canvas_y = (int16_t)coords.y1;
    ESP_LOGI(TAG, "rain canvas buffer set %dx%d at %d,%d", w, h, s_zen.canvas_x, s_zen.canvas_y);
    return true;
}

/* 在雨滴 canvas 内生成随机短墙，完全覆盖整个 canvas 区域。
 * Trap: 坐标必须按墙宽/墙高收口——`esp_random() % (cw-60-w_width)` 在画布收窄后
 * 模数可 ≤0（uint32 取模溢出），墙会飞出画布（2026-09 用户调窄 canvas 后实测）；
 * y 同理须扣墙高，否则末行墙底越出 canvas 下缘 */
static void zen_init_drop_walls(void)
{
    if (s_zen.canvas_w <= 0 || s_zen.canvas_h <= 0) {
        s_zen.drop_wall_count = 0;
        return;
    }

    const int wall_h = 16;
    int cw = s_zen.canvas_w;
    int ch = s_zen.canvas_h;

    /* 边距随画布收窄：可用净宽（含墙宽）始终非负 */
    int margin_x = 30;
    if (cw < 2 * margin_x + 60) {
        margin_x = (cw - 60) / 2;
        if (margin_x < 4) {
            margin_x = 4;
        }
    }
    int avail_w = cw - 2 * margin_x;
    if (avail_w < 24) {           /* 画布过窄，放不下任何有意义短墙 */
        s_zen.drop_wall_count = 0;
        return;
    }

    /* 行高至少容纳墙高 + 随机空间，矮画布自动减行 */
    int rows = 3 + (int)(esp_random() % 3);       /* 3~5 行 */
    int max_rows = ch / (wall_h + 20);
    if (max_rows < 1) {
        max_rows = 1;
    }
    if (rows > max_rows) {
        rows = max_rows;
    }
    int band = ch / rows;
    int n = 0;

    /* 每行最大墙壁数根据画布净宽和最小间距估算 */
    int max_per_row = avail_w / (60 + 12);    /* 最小宽度60 + 间距12 */
    if (max_per_row < 2) max_per_row = 2;

    for (int r = 0; r < rows && n < ZEN_DROP_WALL_MAX; r++) {
        /* 该行已放置墙壁的x区间列表（用于重叠检测） */
        struct { int x1, x2; } placed[ZEN_DROP_WALL_MAX];
        int placed_cnt = 0;

        /* 行内随机空间扣除墙高，墙底恒在画布内 */
        int row_top = r * band + 8;
        int row_h = band - 8 - wall_h;
        if (row_h < 1) row_h = 1;

        /* 该行计划放置的墙壁数随机（但不超过容量），下限 4 保证密度；
         * Trap: max_per_row<=4 时取模基数会<=0， clamp 到 1 */
        int target_cnt = 4 + (int)(esp_random() % (uint32_t)(max_per_row > 4 ? max_per_row - 3 : 1));
        if (target_cnt > max_per_row) target_cnt = max_per_row;

        for (int c = 0; c < target_cnt && n < ZEN_DROP_WALL_MAX; c++) {
            if (esp_random() % 10 == 0) continue;   /* 10% 跳过 */

            /* 尝试生成不重叠的墙壁，最多重试10次 */
            int16_t w_width = 60 + (int16_t)(esp_random() % 81); /* 60~140，长墙接住更多雨滴 */
            if (w_width > avail_w) w_width = (int16_t)avail_w;   /* 墙宽不超越净宽 */
            int x, y;
            bool ok = false;
            for (int retry = 0; retry < 10; retry++) {
                /* span = 净宽 - 墙宽，恒 ≥0；x ∈ [margin_x, margin_x+span] 闭区间 */
                int span = avail_w - w_width;
                x = margin_x + (span > 0 ? (int)(esp_random() % (uint32_t)(span + 1)) : 0);
                /* 与已放置墙壁重叠检测（最小间距12px） */
                bool overlap = false;
                for (int p = 0; p < placed_cnt; p++) {
                    if (!(x + w_width + 12 < placed[p].x1 || x - 12 > placed[p].x2)) {
                        overlap = true;
                        break;
                    }
                }
                if (!overlap) {
                    ok = true;
                    break;
                }
                /* 失败时尝试缩小宽度 */
                if (retry == 5 && w_width > 40) w_width = 40;
            }
            if (!ok) continue;   /* 实在放不下，跳过一个 */

            /* y在该行区间内随机；双保险钳制墙底不出画布 */
            y = row_top + (int)(esp_random() % (uint32_t)row_h);
            if (y + wall_h > ch - 4) y = ch - 4 - wall_h;
            if (y < 0) y = 0;

            zen_wall_t *wl = &s_zen.drop_walls[n];
            wl->w = w_width;
            wl->h = wall_h;
            wl->x = (int16_t)x;
            wl->y = (int16_t)y;
            wl->color_idx = s_ball_colors[n % 5];
            wl->note = zen_note_for_degree((int)(esp_random() % 14));
            zen_note_name(wl->note, wl->name, sizeof(wl->name));
            wl->active_ms = 0;
            n++;

            /* 记录已放置区间 */
            if (placed_cnt < ZEN_DROP_WALL_MAX) {
                placed[placed_cnt].x1 = x;
                placed[placed_cnt].x2 = x + w_width;
                placed_cnt++;
            }
        }
    }
    s_zen.drop_wall_count = n;
}

static void zen_spawn_drop(float x)
{
    for (int i = 0; i < ZEN_DROP_MAX; i++) {
        zen_drop_t *d = &s_zen.drops[i];
        if (d->active) {
            continue;
        }
        d->x = x;
        d->y = 0.0f;
        d->vx = zen_rand_f(-40.0f, 40.0f);
        d->vy = 0;
        d->color_idx = s_ball_colors[i % 5];
        d->active = true;
        d->note_playing = false;
        d->last_wall_hit_ms = 0;
        return;
    }
}

/* 雨滴专用碰撞：保留速度量纲的阻尼弹性碰撞。 */
static void zen_resolve_drop_collision(zen_ball_t *a, zen_ball_t *b)
{
    float dx = b->x - a->x;
    float dy = b->y - a->y;
    float dist_sq = dx * dx + dy * dy;
    float min_dist = 2.0f * ZEN_BALL_RADIUS;
    if (dist_sq >= min_dist * min_dist) {
        return;
    }

    float dist = sqrtf(dist_sq);
    if (dist < 0.001f) {
        a->x -= 2.0f;
        b->x += 2.0f;
        return;
    }

    float nx = dx / dist;
    float ny = dy / dist;

    /* 分离重叠 */
    float overlap = min_dist - dist;
    a->x -= nx * overlap * 0.5f;
    a->y -= ny * overlap * 0.5f;
    b->x += nx * overlap * 0.5f;
    b->y += ny * overlap * 0.5f;

    /* 正在远离则跳过 */
    float dvx = a->vx - b->vx;
    float dvy = a->vy - b->vy;
    float dot = dvx * nx + dvy * ny;
    if (dot > 0) {
        return;
    }

    /* 弹性碰撞 + 阻尼 0.85，等质量交换法向速度分量 */
    a->vx -= dot * nx * 0.85f;
    a->vy -= dot * ny * 0.85f;
    b->vx += dot * nx * 0.85f;
    b->vy += dot * ny * 0.85f;
}

static void zen_update_drops(uint32_t dt_ms)
{
    uint32_t now = zen_now_ms();
    float dt_s = (float)dt_ms / 1000.0f;

    /* 自动补充雨滴到目标个数，随机间隔与落点 */
    int target = s_zen.speed_sel + 1;
    int active = 0;
    for (int i = 0; i < ZEN_DROP_MAX; i++) {
        if (s_zen.drops[i].active) {
            active++;
        }
    }
    if (active < target && now >= s_zen.next_spawn_ms) {
        zen_spawn_drop(zen_rand_f(20.0f, ZEN_SCREEN_W - 20.0f));
        s_zen.next_spawn_ms = now + ZEN_DROP_SPAWN_MIN_MS +
                              (uint32_t)(esp_random() % ZEN_DROP_SPAWN_RND_MS);
    }

    for (int i = 0; i < ZEN_DROP_MAX; i++) {
        zen_drop_t *d = &s_zen.drops[i];
        if (!d->active) {
            continue;
        }

        zen_note_timed_off(now, &d->playing_note, &d->note_on_ms, &d->note_playing);

        float prev_y = d->y;

        d->vy += ZEN_DROP_GRAVITY * dt_s;
        d->x += d->vx * dt_s;
        d->y += d->vy * dt_s;

        /* 雨滴全屏运动，无边框；跌出屏幕即销毁 */
        if (d->x - ZEN_BALL_RADIUS < 0.0f ||
            d->x + ZEN_BALL_RADIUS > ZEN_SCREEN_W ||
            d->y - ZEN_BALL_RADIUS > ZEN_SCREEN_H) {
            if (d->note_playing) {
                zen_send_note_off(d->playing_note);
                d->note_playing = false;
            }
            d->active = false;
            d->last_wall_hit_ms = 0;
            continue;
        }

        if (d->vy <= 0) {
            continue;
        }

        /* 扫掠检测：上一帧底部在墙顶之上、本帧底部越过墙顶即命中
         * 坐标转换：雨滴为屏幕绝对坐标，canvas 内墙为 canvas 局部坐标 */
        int canvas_bottom = s_zen.canvas_y + s_zen.canvas_h;
        if (d->y + ZEN_BALL_RADIUS < s_zen.canvas_y ||
            d->y - ZEN_BALL_RADIUS > canvas_bottom) {
            continue;
        }

        for (int p = 0; p < s_zen.drop_wall_count; p++) {
            zen_wall_t *wl = &s_zen.drop_walls[p];
            int wall_x = s_zen.canvas_x + wl->x;
            int wall_y = s_zen.canvas_y + wl->y;
            if (d->x + ZEN_BALL_RADIUS < wall_x ||
                d->x - ZEN_BALL_RADIUS > wall_x + wl->w) {
                continue;
            }
            if (prev_y + ZEN_BALL_RADIUS > wall_y ||
                d->y + ZEN_BALL_RADIUS < wall_y) {
                continue;
            }

            /* 碰撞冷却 80ms，防止吸附抖动 */
            if (d->last_wall_hit_ms != 0 && (now - d->last_wall_hit_ms) < 80) {
                break;
            }

            d->y = wall_y - ZEN_BALL_RADIUS;
            float impact = d->vy;
            d->vy = -impact * ZEN_DROP_BOUNCE;

            /* 最小反弹速度，避免贴墙滑动 */
            if (fabsf(d->vy) < 50.0f) {
                d->vy = (d->vy > 0 ? 50.0f : -50.0f);
            }

            /* 按落点偏离墙中心的程度给水平反弹分量 */
            float center = wall_x + wl->w / 2.0f;
            d->vx = (d->x - center) / (wl->w / 2.0f) * 220.0f;
            wl->active_ms = now;
            d->last_wall_hit_ms = now;

            int vel = 110;
            zen_note_trigger(wl->note, (uint8_t)vel,
                             &d->playing_note, &d->note_on_ms, &d->note_playing);
            break;
        }
    }

    /* 雨滴间碰撞检测（专用版本：保留速度量纲） */
    for (int i = 0; i < ZEN_DROP_MAX; i++) {
        if (!s_zen.drops[i].active) continue;
        for (int j = i + 1; j < ZEN_DROP_MAX; j++) {
            if (!s_zen.drops[j].active) continue;
            zen_resolve_drop_collision((zen_ball_t *)&s_zen.drops[i],
                                       (zen_ball_t *)&s_zen.drops[j]);
        }
    }
}

/* -------------------- 绘制 -------------------- */

static void zen_draw_wall(lv_layer_t *layer, const zen_wall_t *wl, uint32_t now,
                          bool name_below)
{
    (void)now;
    lv_color_t color = engine_gui_theme_color(wl->color_idx);

    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = color;
    rect_dsc.bg_opa = LV_OPA_COVER;
    rect_dsc.radius = wl->h / 2;

    lv_area_t area = {
        wl->x + 1, wl->y + 1,
        wl->x + wl->w - 2, wl->y + wl->h - 2
    };
    lv_draw_rect(layer, &rect_dsc, &area);

    if (wl->w > 44) {
        lv_draw_label_dsc_t label_dsc;
        lv_draw_label_dsc_init(&label_dsc);
        label_dsc.font = &lv_font_montserrat_14;
        label_dsc.color = engine_gui_theme_color(ZEN_C_DIM);
        label_dsc.text = wl->name;
        label_dsc.align = LV_TEXT_ALIGN_CENTER;
        lv_area_t text_area;
        if (name_below) {
            text_area.x1 = wl->x;
            text_area.y1 = wl->y + wl->h + 2;
            text_area.x2 = wl->x + wl->w;
            text_area.y2 = wl->y + wl->h + 18;
        } else {
            text_area.x1 = wl->x;
            text_area.y1 = wl->y - 18;
            text_area.x2 = wl->x + wl->w;
            text_area.y2 = wl->y - 2;
        }
        lv_draw_label(layer, &label_dsc, &text_area);
    }
}
static void zen_draw_rain_canvas(lv_obj_t *canvas)
{
    uint32_t now = zen_now_ms();

    lv_canvas_fill_bg(canvas, engine_gui_theme_color(ZEN_C_BG), LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    for (int i = 0; i < s_zen.drop_wall_count; i++) {
        zen_draw_wall(&layer, &s_zen.drop_walls[i], now, true);
    }

    lv_canvas_finish_layer(canvas, &layer);
}

static void zen_canvas_redraw(void)
{
    if (s_zen_ui.canvas == NULL || s_zen.canvas_buf == NULL) {
        return;
    }
    lvgl_port_lock(portMAX_DELAY);
    zen_draw_rain_canvas(s_zen_ui.canvas);
    lv_obj_clear_flag(s_zen_ui.canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(s_zen_ui.canvas);
    lvgl_port_unlock();
}

/* 运动球（弹珠/雨滴）映射到 LED 控件：仅更新位置/颜色/可见性 */
static void zen_sync_leds(void)
{
    lvgl_port_lock(portMAX_DELAY);
    for (int i = 0; i < ZEN_LED_COUNT; i++) {
        lv_obj_t *led = s_zen_ui.balls[i];
        if (led == NULL) {
            continue;
        }

        bool active = false;
        float x = 0.0f, y = 0.0f;
        uint8_t color_idx = 0;

        if (s_zen.mode == 0) {
            if (i < ZEN_BALL_MAX && s_zen.balls[i].active) {
                active = true;
                x = s_zen.balls[i].x;
                y = s_zen.balls[i].y;
                color_idx = s_zen.balls[i].color_idx;
            }
        } else {
            if (i < ZEN_DROP_MAX && s_zen.drops[i].active) {
                active = true;
                x = s_zen.drops[i].x;
                y = s_zen.drops[i].y;
                color_idx = s_zen.drops[i].color_idx;
            }
        }

        if (active) {
            lv_obj_set_pos(led,
                           (int32_t)x - ZEN_LED_SIZE / 2,
                           (int32_t)y - ZEN_LED_SIZE / 2);
            lv_led_set_color(led, engine_gui_theme_color(color_idx));
            lv_obj_clear_flag(led, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(led, LV_OBJ_FLAG_HIDDEN);
        }
    }
    lvgl_port_unlock();
}

static void zen_apply_mode_ui(void)
{
    lvgl_port_lock(portMAX_DELAY);
    /* 示例线仅作 EEZ 样式模板，永不显示 */
    if (s_zen_ui.line != NULL) {
        lv_obj_add_flag(s_zen_ui.line, LV_OBJ_FLAG_HIDDEN);
    }
    bool ball = (s_zen.mode == 0);
    for (int i = 0; i < ZEN_WALL_COUNT; i++) {
        if (s_seg_lines[i] == NULL) {
            continue;
        }
        if (ball) {
            lv_obj_clear_flag(s_seg_lines[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_seg_lines[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    /* 切换模式时 canvas 一律先隐藏；雨滴模式首绘完成后由 zen_canvas_redraw 解除 */
    if (s_zen_ui.canvas != NULL) {
        lv_obj_add_flag(s_zen_ui.canvas, LV_OBJ_FLAG_HIDDEN);
    }
    if (!ball) {
        s_zen.canvas_ready = false;
    }
    lvgl_port_unlock();
}

static void zen_enter_mode(void)
{
    zen_apply_mode_ui();
    if (s_zen.mode == 0) {
        zen_init_walls();
        zen_init_balls();
    } else {
        memset(s_zen.drops, 0, sizeof(s_zen.drops));
        s_zen.next_spawn_ms = 0;
        /* canvas buffer 由 on_update 中 ensure 建立后再生成 key 墙 */
    }
}

/* -------------------- 生命周期与事件 -------------------- */

static void app_zen_on_ui_event(app_base_t *self, lv_event_t *e);

static void app_zen_event_cb(lv_event_t *e)
{
    app_base_t *self = (app_base_t *)lv_event_get_user_data(e);
    app_zen_on_ui_event(self, e);
}

static void app_zen_home_cb(lv_event_t *e)
{
    (void)e;
    app_manager_request_kill_active();
}

static void app_zen_rec_btn_cb(lv_event_t *e)
{
    app_base_t *self = (app_base_t *)lv_event_get_user_data(e);
    ui_screen_zen_t *ui = (ui_screen_zen_t *)self->screen_ctx;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    if (s_zen.recording_self) {
        service_recorder_result_t r = app_manager_record_stop();
        if (r != RECORDER_OK && r != RECORDER_ERR_NOT_RECORDING) {
            app_manager_show_notification_timeout(_("停止录制失败"), 2000);
        }
        /* 视觉状态与最终提示由 on_update 在 finalize 完成后统一处理 */
        return;
    }

    service_recorder_result_t r = app_manager_record_start(TAG);
    switch (r) {
    case RECORDER_OK:
        app_manager_show_notification_timeout(_("开始录制"), 1000);
        s_zen.recording_self = true;
        s_zen.recording_stop_pending = false;
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

static void app_zen_set_open_cb(lv_event_t *e)
{
    (void)e;
    lvgl_port_lock(portMAX_DELAY);
    if (s_zen_ui.set != NULL) {
        lv_obj_clear_flag(s_zen_ui.set, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_zen_ui.set, LV_OBJ_FLAG_CLICKABLE);
    }
    lvgl_port_unlock();
}

static void app_zen_set_close_cb(lv_event_t *e)
{
    (void)e;
    lvgl_port_lock(portMAX_DELAY);
    if (s_zen_ui.set != NULL) {
        lv_obj_add_flag(s_zen_ui.set, LV_OBJ_FLAG_HIDDEN);
    }
    lvgl_port_unlock();
}

static void app_zen_on_ui_event(app_base_t *self, lv_event_t *e)
{
    ui_screen_zen_t *ui = (ui_screen_zen_t *)self->screen_ctx;
    lv_obj_t *target = lv_event_get_target_obj(e);

    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    bool changed = false;
    if (target == ui->dropdown_mode) {
        s_zen.mode = (uint8_t)lv_dropdown_get_selected(ui->dropdown_mode);
        ESP_LOGI(TAG, "mode select=%d", s_zen.mode);
        zen_enter_mode();
        changed = true;
    } else if (target == ui->dropdown_key) {
        s_zen.key_sel = (uint8_t)lv_dropdown_get_selected(ui->dropdown_key);
        if (s_zen.key_sel >= ZEN_SCALE_COUNT) s_zen.key_sel = 0;
        ESP_LOGI(TAG, "key select=%d", s_zen.key_sel);
        /* 只重建当前模式的音墙，避免跨模式误显对方的墙 */
        if (s_zen.mode == 0) {
            zen_init_walls();
        } else {
            zen_init_drop_walls();
            zen_canvas_redraw();
        }
        changed = true;
    } else if (target == ui->dropdown_speed) {
        s_zen.speed_sel = (uint8_t)lv_dropdown_get_selected(ui->dropdown_speed);
        if (s_zen.speed_sel >= ZEN_SPEED_LEVELS) s_zen.speed_sel = 2;
        ESP_LOGI(TAG, "speed select=%d", s_zen.speed_sel);
        changed = true;
    } else if (target == ui->dropdown_sound) {
        uint8_t sound = (uint8_t)lv_dropdown_get_selected(ui->dropdown_sound);
        if (sound < ZEN_SOUND_COUNT && sound != s_zen.sound_sel) {
            s_zen.sound_sel = sound;
            ESP_LOGI(TAG, "sound select=%d", s_zen.sound_sel);
            zen_set_sound_type(s_zen.sound_sel);
            changed = true;
        }
    }

    if (changed) {
        zen_sync_leds();
        zen_save_params();
    }
}

static void app_zen_on_input(app_base_t *self, const app_input_event_t *evt)
{
    (void)self;

    /* 设置面板可见时屏蔽主界面触控 */
    if (s_zen_ui.set != NULL && !lv_obj_has_flag(s_zen_ui.set, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    if (evt->type != APP_INPUT_TOUCH_DOWN || s_zen.mode != 0) {
        return;
    }

    /* 弹珠模式：触摸任意位置生成新球 */
    zen_spawn_ball((float)evt->x, (float)evt->y);
}

/* 首帧拆分倒计时（on_update 周期数） */
static uint8_t s_canvas_defer = 0;

static bool app_zen_on_init(app_base_t *self, void *screen_ctx)
{
    (void)self;
    ESP_LOGI(TAG, "zen mode init");

    memset(&s_zen, 0, sizeof(s_zen));
    s_zen.mode = 0;
    s_zen.key_sel = 0;
    s_zen.speed_sel = 2;
    s_zen.sound_sel = 0;
    s_zen.last_update_ms = zen_now_ms();

    /* 从 NVS 恢复参数并校验 */
    zen_load_params();
    ESP_LOGI(TAG, "loaded: mode=%d, key=%d, speed=%d, sound=%d",
             s_zen.mode, s_zen.key_sel, s_zen.speed_sel, s_zen.sound_sel);

    ui_screen_zen_t *ui = (ui_screen_zen_t *)screen_ctx;

    lvgl_port_lock(portMAX_DELAY);

    /* 把 NVS 值回写 UI 控件 */
    if (ui->dropdown_mode != NULL) {
        lv_dropdown_set_selected(ui->dropdown_mode, s_zen.mode);
    }
    if (ui->dropdown_key != NULL) {
        lv_dropdown_set_selected(ui->dropdown_key, s_zen.key_sel);
    }
    if (ui->dropdown_speed != NULL) {
        lv_dropdown_set_selected(ui->dropdown_speed, s_zen.speed_sel);
    }
    if (ui->dropdown_sound != NULL) {
        lv_dropdown_set_selected(ui->dropdown_sound, s_zen.sound_sel);
    }

    if (ui->btn_home != NULL) {
        lv_obj_add_event_cb(ui->btn_home, app_zen_home_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui->btn_rec != NULL) {
        lv_obj_add_event_cb(ui->btn_rec, app_zen_rec_btn_cb, LV_EVENT_CLICKED, self);
    }
    if (ui->btn_set != NULL) {
        lv_obj_add_event_cb(ui->btn_set, app_zen_set_open_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui->set_btn_return != NULL) {
        lv_obj_add_event_cb(ui->set_btn_return, app_zen_set_close_cb, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_add_event_cb(ui->dropdown_mode, app_zen_event_cb, LV_EVENT_VALUE_CHANGED, self);
    lv_obj_add_event_cb(ui->dropdown_key, app_zen_event_cb, LV_EVENT_VALUE_CHANGED, self);
    lv_obj_add_event_cb(ui->dropdown_speed, app_zen_event_cb, LV_EVENT_VALUE_CHANGED, self);
    lv_obj_add_event_cb(ui->dropdown_sound, app_zen_event_cb, LV_EVENT_VALUE_CHANGED, self);

    lvgl_port_unlock();

    /* 设置初始音色 */
    zen_set_sound_type(s_zen.sound_sel);

    zen_enter_mode();
    /* 首帧拆分：仅雨滴模式的 canvas 首绘需要推迟（buffer 分配+整幅填充较重） */
    s_canvas_defer = (s_zen.mode == 1) ? 3 : 0;
    return true;
}

static void app_zen_on_update(app_base_t *self)
{
    (void)self;
    ui_screen_zen_t *ui = &s_zen_ui;

    /* 首帧拆分倒计时：雨滴 canvas 首绘推迟 ~30ms 让背景先上屏 */
    if (s_canvas_defer > 0) {
        s_canvas_defer--;
    }

    uint32_t now = zen_now_ms();
    uint32_t period_ms = 1000 / ZEN_UPDATE_HZ;
    if ((now - s_zen.last_update_ms) < period_ms) {
        goto process_ui;
    }
    uint32_t dt = now - s_zen.last_update_ms;
    s_zen.last_update_ms = now;

    if (s_zen.mode == 0) {
        zen_update_balls(dt);
    } else {
        /* 雨滴 canvas 首帧/重建/模式重进同步：buffer 未变时 ensure 返回 false，
         * 但模式重进必须无条件重绘并解除隐藏（修复来回切换后 key 墙消失） */
        if (s_canvas_defer == 0) {
            if (!s_zen.canvas_ready) {
                zen_canvas_ensure_buffer();
                if (s_zen.canvas_buf != NULL) {
                    zen_init_drop_walls();
                    zen_canvas_redraw();
                    s_zen.canvas_ready = true;
                }
            } else if (zen_canvas_ensure_buffer()) {
                zen_init_drop_walls();
                zen_canvas_redraw();
            }
        }
        zen_update_drops(dt);
    }
    zen_sync_leds();

process_ui:
    /* 先处理上一周期检测到的停止 finalize 结果 */
    if (s_zen.recording_stop_pending && !app_manager_record_is_recording()) {
        char path[256];
        if (app_manager_record_get_last_path(path, sizeof(path))) {
            app_manager_show_notification_timeout(_("录音已保存"), 2000);
        } else {
            app_manager_show_notification_timeout(_("录制时间过短，已丢弃"), 2000);
        }
        s_zen.recording_stop_pending = false;
    }

    if (s_zen.recording_self && !app_manager_record_is_recording()) {
        s_zen.recording_self = false;
        s_zen.recording_stop_pending = true;
    }

    if (ui->btn_rec != NULL) {
        lvgl_port_lock(portMAX_DELAY);
        if (s_zen.recording_self) {
            lv_obj_add_state(ui->btn_rec, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(ui->btn_rec, LV_STATE_CHECKED);
        }
        lvgl_port_unlock();
    }
}

static void zen_all_sound_off(void)
{
    engine_midi_event_t midi = {0};
    midi.type = ENGINE_MIDI_MSG_CONTROL_CHANGE;
    midi.channel = 0;
    midi.data2 = 0;
    midi.source_port = ENGINE_MIDI_PORT_APP;

    midi.data1 = 120;
    engine_midi_publish(&midi, 0);
    midi.data1 = 123;
    engine_midi_publish(&midi, 0);
}

static void app_zen_on_pause(app_base_t *self)
{
    (void)self;
    ESP_LOGI(TAG, "pause");
    zen_all_sound_off();

    if (s_zen.recording_self) {
        app_manager_record_stop();
        s_zen.recording_self = false;
        s_zen.recording_stop_pending = false;
    }

    /* 兜底保存 */
    zen_save_params();
}

static void app_zen_on_resume(app_base_t *self)
{
    (void)self;
    ESP_LOGI(TAG, "resume");
    s_zen.last_update_ms = zen_now_ms();

    /* 恢复模式显隐状态后，按当前模式刷新对应视觉元素 */
    zen_apply_mode_ui();
    if (s_zen.mode == 0) {
        zen_init_walls();
    } else {
        s_zen.canvas_ready = false;
    }
    zen_sync_leds();
}

static void app_zen_on_destroy(app_base_t *self)
{
    (void)self;
    ESP_LOGI(TAG, "destroy");

    /* 移除 on_init 注册的事件回调 */
    lvgl_port_lock(portMAX_DELAY);
    if (s_zen_ui.dropdown_mode != NULL) {
        lv_obj_remove_event_cb(s_zen_ui.dropdown_mode, app_zen_event_cb);
    }
    if (s_zen_ui.dropdown_key != NULL) {
        lv_obj_remove_event_cb(s_zen_ui.dropdown_key, app_zen_event_cb);
    }
    if (s_zen_ui.dropdown_speed != NULL) {
        lv_obj_remove_event_cb(s_zen_ui.dropdown_speed, app_zen_event_cb);
    }
    if (s_zen_ui.dropdown_sound != NULL) {
        lv_obj_remove_event_cb(s_zen_ui.dropdown_sound, app_zen_event_cb);
    }
    if (s_zen_ui.btn_home != NULL) {
        lv_obj_remove_event_cb(s_zen_ui.btn_home, app_zen_home_cb);
    }
    if (s_zen_ui.btn_rec != NULL) {
        lv_obj_remove_event_cb(s_zen_ui.btn_rec, app_zen_rec_btn_cb);
    }
    if (s_zen_ui.btn_set != NULL) {
        lv_obj_remove_event_cb(s_zen_ui.btn_set, app_zen_set_open_cb);
    }
    if (s_zen_ui.set_btn_return != NULL) {
        lv_obj_remove_event_cb(s_zen_ui.set_btn_return, app_zen_set_close_cb);
    }

    /* 分段音墙线是运行时挂在 EEZ 屏上的自建控件，屏终身缓存不销毁，
     * App 退出时必须删除，否则下次进入重复创建 */
    for (int i = 0; i < ZEN_WALL_COUNT; i++) {
        if (s_seg_lines[i] != NULL) {
            lv_obj_delete(s_seg_lines[i]);
            s_seg_lines[i] = NULL;
        }
    }
    lvgl_port_unlock();

    if (s_zen.recording_self) {
        app_manager_record_stop();
        s_zen.recording_self = false;
        s_zen.recording_stop_pending = false;
    }

    if (s_zen.canvas_buf != NULL) {
        heap_caps_free(s_zen.canvas_buf);
        s_zen.canvas_buf = NULL;
    }

    zen_save_params();
    zen_all_sound_off();
}

esp_err_t app_zen_register(void)
{
    static app_base_t app = {
        .name = "Zen Mode",
        .screen_name = "app_zen_mode",
        .screen_ctx = &s_zen_ui,
        .screen_ctx_size = sizeof(s_zen_ui),
        .widget_bindings = s_zen_bindings,
        .on_init = app_zen_on_init,
        .on_update = app_zen_on_update,
        .on_pause = app_zen_on_pause,
        .on_resume = app_zen_on_resume,
        .on_destroy = app_zen_on_destroy,
        .on_input = app_zen_on_input,
        .on_ui_event = app_zen_on_ui_event,
    };
    return app_manager_register(&app);
}
