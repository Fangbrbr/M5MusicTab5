/**
 * @file app_zen.c
 * @brief 禅模式 App：弹珠与雨滴两种环境音生成玩法
 *
 * - 弹珠：小球在活动区内匀速反弹，撞击四边音墙段发声，音墙段短时高亮；
 *   音墙内收一圈，墙外侧标注音名；点画布放球，满员后新球替换最老的球。
 * - 雨滴：下半屏随机短墙阵，上半屏自动落下雨滴，撞墙反弹并发声，
 *   跌出画布销毁；1-5 个雨滴同时下落，形成随机音阶与和弦。
 * 音高取自当前调式（大调/国风五声/小调/日式小调），配色全部取自 EEZ 主题数组。
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
#include <math.h>
#include <string.h>
#include <stdio.h>

static const char *TAG = "app_zen";

/* 画布在 app_zen_mode 屏幕中的位置与尺寸（与 EEZ 布局一致） */
#define ZEN_CANVAS_X        25
#define ZEN_CANVAS_Y        110
#define ZEN_CANVAS_W        950
#define ZEN_CANVAS_H        576

/* 运动球使用 5 个 LED 控件渲染，canvas 只承载静态音墙，避免整帧重绘 */
#define ZEN_LED_COUNT       5
#define ZEN_LED_SIZE        30

/* 活动区在画布内缩进，音墙/边框贴活动区边缘绘制 */
#define ZEN_MARGIN          12
#define ZEN_AREA_X0         ZEN_MARGIN
#define ZEN_AREA_Y0         ZEN_MARGIN
#define ZEN_AREA_X1         (ZEN_CANVAS_W - ZEN_MARGIN)
#define ZEN_AREA_Y1         (ZEN_CANVAS_H - ZEN_MARGIN)

#define ZEN_BALL_MAX        4
#define ZEN_BALL_RADIUS     14
#define ZEN_SEG_TOP_BOTTOM  8
#define ZEN_SEG_SIDES       4
#define ZEN_WALL_COUNT      (2 * (ZEN_SEG_TOP_BOTTOM + ZEN_SEG_SIDES))
#define ZEN_WALL_THICK      16
/* 音墙内收宽度：墙外侧预留音名标注空间 */
#define ZEN_WALL_INSET      26
#define ZEN_HIT_COOLDOWN_MS 120

#define ZEN_DROP_MAX        5
#define ZEN_DROP_WALL_MAX   20
#define ZEN_DROP_GRAVITY    900.0f
#define ZEN_DROP_BOUNCE     0.7f
#define ZEN_DROP_SPAWN_MIN_MS  300
#define ZEN_DROP_SPAWN_RND_MS  500

/* 引力引擎：球间相互吸引，弯曲轨迹产生轨道/散射效果 */
#define ZEN_GRAVITY_RANGE   100     /* 引力作用范围（px） */
#define ZEN_GRAVITY_K       5000  /* 引力系数，a = K / r² */
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

typedef struct {
    const int8_t *steps;
    uint8_t len;
} zen_scale_t;

/* 大调 / 国风五声（宫） / 自然小调 / 日式小调（都节调式，与小钢琴一致） */
static const int8_t s_steps_major[]   = {0, 2, 4, 5, 7, 9, 11};
static const int8_t s_steps_guofeng[] = {0, 2, 4, 7, 9};
static const int8_t s_steps_minor[]   = {0, 2, 3, 5, 7, 8, 10};
static const int8_t s_steps_japan[]   = {0, 1, 5, 7, 8};

static const zen_scale_t s_scales[] = {
    { s_steps_major,   7 },
    { s_steps_guofeng, 5 },
    { s_steps_minor,   7 },
    { s_steps_japan,   5 },
};

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
    uint8_t key_sel;    /* 0 大调 1 国风 2 小调 3 日式小调 */
    uint8_t speed_sel;  /* 弹珠：速度档；雨滴：同时下落个数-1 */
    zen_ball_t balls[ZEN_BALL_MAX];
    zen_wall_t walls[ZEN_WALL_COUNT];
    zen_drop_t drops[ZEN_DROP_MAX];
    zen_wall_t drop_walls[ZEN_DROP_WALL_MAX];
    int drop_wall_count;
    uint32_t next_spawn_ms;
    uint32_t last_update_ms;
    void *canvas_buf;
    bool recording_self;        /* 本 App 发起的录制 */
    bool recording_stop_pending;  /* 录制已停止，等待 finalize 后提示 */
} zen_state_t;

typedef struct {
    lv_obj_t *canvas;
    lv_obj_t *dropdown_mode;
    lv_obj_t *dropdown_key;
    lv_obj_t *dropdown_speed;
    lv_obj_t *balls[ZEN_LED_COUNT];
    lv_obj_t *btn_home;
    lv_obj_t *btn_rec;
} ui_screen_zen_t;

static ui_screen_zen_t s_zen_ui = {0};

static const widget_binding_t s_zen_bindings[] = {
    WIDGET_BIND(ui_screen_zen_t, canvas,          "zen_canvas",          WIDGET_KIND_CANVAS),
    WIDGET_BIND(ui_screen_zen_t, dropdown_mode,   "zen_dropdown_mode",   WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_zen_t, dropdown_key,    "zen_dropdown_key",    WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_zen_t, dropdown_speed,  "zen_dropdown_speed",  WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_zen_t, balls[0],        "zen_ball_0",          WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_zen_t, balls[1],        "zen_ball_1",          WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_zen_t, balls[2],        "zen_ball_2",          WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_zen_t, balls[3],        "zen_ball_3",          WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_zen_t, balls[4],        "zen_ball_4",          WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_zen_t, btn_home,        "zen_btn_home",        WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_zen_t, btn_rec,         "zen_btn_rec",         WIDGET_KIND_ANY),
    WIDGET_BINDING_END,
};

static zen_state_t s_zen = {0};

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

/* -------------------- 弹珠模式 -------------------- */

static void zen_init_walls(void)
{
    /* 音墙内收一圈，墙外侧留白标注音名 */
    float left = ZEN_AREA_X0 + ZEN_WALL_INSET;
    float right = ZEN_AREA_X1 - ZEN_WALL_INSET;
    float top = ZEN_AREA_Y0 + ZEN_WALL_INSET;
    float bottom = ZEN_AREA_Y1 - ZEN_WALL_INSET;
    float seg_w = (right - left) / ZEN_SEG_TOP_BOTTOM;
    float seg_h = (bottom - top) / ZEN_SEG_SIDES;
    int i;

    /* 音名按调式音级随机排列：0~11 度各两次（与原顺序排列同集合），
     * Fisher-Yates 洗牌后沿墙序分配 */
    uint8_t degrees[ZEN_WALL_COUNT];
    for (i = 0; i < ZEN_WALL_COUNT; i++) {
        degrees[i] = (uint8_t)(i / 2);
    }
    for (i = ZEN_WALL_COUNT - 1; i > 0; i--) {
        int j = (int)(esp_random() % (uint32_t)(i + 1));
        uint8_t tmp = degrees[i];
        degrees[i] = degrees[j];
        degrees[j] = tmp;
    }
    int w = 0;

    for (i = 0; i < ZEN_SEG_TOP_BOTTOM; i++) {
        zen_wall_t *wl = &s_zen.walls[i];
        wl->x = (int16_t)(left + i * seg_w);
        wl->y = (int16_t)top;
        wl->w = (int16_t)seg_w;
        wl->h = ZEN_WALL_THICK;
        wl->color_idx = ZEN_C_TOP;
        wl->note = zen_note_for_degree(degrees[w++]);
        zen_note_name(wl->note, wl->name, sizeof(wl->name));
        wl->active_ms = 0;
    }

    for (i = 0; i < ZEN_SEG_SIDES; i++) {
        zen_wall_t *wl = &s_zen.walls[ZEN_SEG_TOP_BOTTOM + i];
        wl->x = (int16_t)(right - ZEN_WALL_THICK);
        wl->y = (int16_t)(top + i * seg_h);
        wl->w = ZEN_WALL_THICK;
        wl->h = (int16_t)seg_h;
        wl->color_idx = ZEN_C_RIGHT;
        wl->note = zen_note_for_degree(degrees[w++]);
        zen_note_name(wl->note, wl->name, sizeof(wl->name));
        wl->active_ms = 0;
    }

    for (i = 0; i < ZEN_SEG_TOP_BOTTOM; i++) {
        zen_wall_t *wl = &s_zen.walls[ZEN_SEG_TOP_BOTTOM + ZEN_SEG_SIDES + i];
        wl->x = (int16_t)(left + i * seg_w);
        wl->y = (int16_t)(bottom - ZEN_WALL_THICK);
        wl->w = (int16_t)seg_w;
        wl->h = ZEN_WALL_THICK;
        wl->color_idx = ZEN_C_BOTTOM;
        wl->note = zen_note_for_degree(degrees[w++]);
        zen_note_name(wl->note, wl->name, sizeof(wl->name));
        wl->active_ms = 0;
    }

    for (i = 0; i < ZEN_SEG_SIDES; i++) {
        zen_wall_t *wl = &s_zen.walls[2 * ZEN_SEG_TOP_BOTTOM + ZEN_SEG_SIDES + i];
        wl->x = (int16_t)left;
        wl->y = (int16_t)(top + i * seg_h);
        wl->w = ZEN_WALL_THICK;
        wl->h = (int16_t)seg_h;
        wl->color_idx = ZEN_C_LEFT;
        wl->note = zen_note_for_degree(degrees[w++]);
        zen_note_name(wl->note, wl->name, sizeof(wl->name));
        wl->active_ms = 0;
    }
}

/* 弹珠可活动边界：音墙内侧 */
static float zen_ball_min_x(void) { return ZEN_AREA_X0 + ZEN_WALL_INSET + ZEN_WALL_THICK + ZEN_BALL_RADIUS; }
static float zen_ball_max_x(void) { return ZEN_AREA_X1 - ZEN_WALL_INSET - ZEN_WALL_THICK - ZEN_BALL_RADIUS; }
static float zen_ball_min_y(void) { return ZEN_AREA_Y0 + ZEN_WALL_INSET + ZEN_WALL_THICK + ZEN_BALL_RADIUS; }
static float zen_ball_max_y(void) { return ZEN_AREA_Y1 - ZEN_WALL_INSET - ZEN_WALL_THICK - ZEN_BALL_RADIUS; }

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
    // 归一化
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
    zen_spawn_ball(zen_rand_f(ZEN_CANVAS_W * 0.3f, ZEN_CANVAS_W * 0.7f),
                   zen_rand_f(ZEN_CANVAS_H * 0.3f, ZEN_CANVAS_H * 0.7f));
}

/* 球与某条边碰撞后，按位置求所在音墙段索引；无效返回 -1 */
static int zen_wall_index_at(float x, float y, int side)
{
    float left = ZEN_AREA_X0 + ZEN_WALL_INSET;
    float top = ZEN_AREA_Y0 + ZEN_WALL_INSET;
    float seg_w = (ZEN_AREA_X1 - ZEN_AREA_X0 - 2 * ZEN_WALL_INSET) / ZEN_SEG_TOP_BOTTOM;
    float seg_h = (ZEN_AREA_Y1 - ZEN_AREA_Y0 - 2 * ZEN_WALL_INSET) / ZEN_SEG_SIDES;
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

/* 两个球之间的碰撞处理：带角度偏置的完美弹性碰撞（等质量）
 * 设计目标：利用互相作用尽可能发散球的轨迹。
 * - 移除阻尼（e=1 完美弹性），守恒动能，避免"推挤"感
 * - 旋转碰撞法线 ±25° 偏置，打破对弹对称性，产生散射效果
 * - 碰撞后重新归一化方向向量，维持恒定速率 */
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

/* 雨滴专用碰撞：保留速度量纲的阻尼弹性碰撞。
 * Why 不与弹珠共用 zen_resolve_ball_collision：弹珠速度是单位方向向量
 * （速率另乘速度档），该函数碰撞后归一化；雨滴的 vy 靠重力逐帧累加，
 * 归一化会把下落速度拍成 1——雨滴相撞后悬停横飘，像带了引力（2026-08 回归）。 */
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

    /* 弹性碰撞 + 阻尼 0.85，等质量交换法向速度分量（速率随之保留） */
    a->vx -= dot * nx * 0.85f;
    a->vy -= dot * ny * 0.85f;
    b->vx += dot * nx * 0.85f;
    b->vy += dot * ny * 0.85f;
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
        /* 引力弯曲后重新归一化方向，维持单位速率（只改方向、不改速度） */
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

        // uint8_t vel = (uint8_t)(70 + s_zen.speed_sel * 8);
        // zen_note_trigger(s_zen.walls[wi].note, vel,
        //                  &b->playing_note, &b->note_on_ms, &b->note_playing);
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

/* -------------------- 雨滴模式 -------------------- */

/* 完全重写墙壁生成，避免列对齐，增加随机性 */
static void zen_init_drop_walls(void)
{
    int rows = 2 + (int)(esp_random() % 3);       // 2~4 行
    int sky = ZEN_CANVAS_H / 2;
    int band = (ZEN_CANVAS_H - sky - 40) / rows;
    int n = 0;

    // 每行最大墙壁数根据画布宽度和最小间距估算
    int max_per_row = (ZEN_CANVAS_W - 60) / (30 + 8); // 最小宽度30 + 间距8

    for (int r = 0; r < rows && n < ZEN_DROP_WALL_MAX; r++) {
        // 该行已放置墙壁的x区间列表（用于重叠检测）
        struct { int x1, x2; } placed[10];
        int placed_cnt = 0;

        int row_top = sky + 16 + r * band;
        int row_h = band - 16;
        if (row_h < 4) row_h = 4;

        // 该行计划放置的墙壁数随机（但不超过容量）
        int target_cnt = 2 + (int)(esp_random() % (max_per_row - 1));
        if (target_cnt > max_per_row) target_cnt = max_per_row;

        for (int c = 0; c < target_cnt && n < ZEN_DROP_WALL_MAX; c++) {
            if (esp_random() % 5 == 0) continue;   // 20% 跳过

            // 尝试生成不重叠的墙壁，最多重试10次
            int16_t w_width = 30 + (int16_t)(esp_random() % 41); // 30~70
            int x, y;
            bool ok = false;
            for (int retry = 0; retry < 10; retry++) {
                x = 30 + (esp_random() % (ZEN_CANVAS_W - 60 - w_width));
                // 与已放置墙壁重叠检测（最小间距8px）
                bool overlap = false;
                for (int p = 0; p < placed_cnt; p++) {
                    if (!(x + w_width + 8 < placed[p].x1 || x - 8 > placed[p].x2)) {
                        overlap = true;
                        break;
                    }
                }
                if (!overlap) {
                    ok = true;
                    break;
                }
                // 失败时尝试缩小宽度
                if (retry == 5 && w_width > 20) w_width = 20;
            }
            if (!ok) continue;   // 实在放不下，跳过一个

            // y在该行区间内随机
            y = row_top + (int)(esp_random() % row_h);

            zen_wall_t *wl = &s_zen.drop_walls[n];
            wl->w = w_width;
            wl->h = 16;
            wl->x = (int16_t)x;
            wl->y = (int16_t)y;
            wl->color_idx = s_ball_colors[n % 5];
            wl->note = zen_note_for_degree((int)(esp_random() % 14));
            zen_note_name(wl->note, wl->name, sizeof(wl->name));
            wl->active_ms = 0;
            n++;

            // 记录已放置区间
            if (placed_cnt < 10) {
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
        d->y = ZEN_AREA_Y0 + 20.0f;
        d->vx = zen_rand_f(-40.0f, 40.0f);
        d->vy = 0;
        d->color_idx = s_ball_colors[i % 5];
        d->active = true;
        d->note_playing = false;
        d->last_wall_hit_ms = 0;
        return;
    }
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
        zen_spawn_drop(zen_rand_f(ZEN_AREA_X0 + 40.0f, ZEN_AREA_X1 - 40.0f));
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

        /* 左右无边：触边即销毁，与跌出底部同等处理 */
        if (d->x - ZEN_BALL_RADIUS < ZEN_AREA_X0 ||
            d->x + ZEN_BALL_RADIUS > ZEN_AREA_X1 ||
            d->y - ZEN_BALL_RADIUS > ZEN_CANVAS_H) {
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

        /* 扫掠检测：上一帧底部在墙顶之上、本帧底部越过墙顶即命中 */
        for (int p = 0; p < s_zen.drop_wall_count; p++) {
            zen_wall_t *wl = &s_zen.drop_walls[p];
            if (d->x + ZEN_BALL_RADIUS < wl->x ||
                d->x - ZEN_BALL_RADIUS > wl->x + wl->w) {
                continue;
            }
            if (prev_y + ZEN_BALL_RADIUS > wl->y ||
                d->y + ZEN_BALL_RADIUS < wl->y) {
                continue;
            }

            /* 碰撞冷却 80ms，防止吸附抖动 */
            if (d->last_wall_hit_ms != 0 && (now - d->last_wall_hit_ms) < 80) {
                break;
            }

            d->y = wl->y - ZEN_BALL_RADIUS;
            float impact = d->vy;
            d->vy = -impact * ZEN_DROP_BOUNCE;

            /* 最小反弹速度，避免贴墙滑动   */
            if (fabsf(d->vy) < 50.0f) {
                d->vy = (d->vy > 0 ? 50.0f : -50.0f);
            }

            /*  随机水平偏移，增加自然感 */
            d->vx += zen_rand_f(-30.0f, 30.0f);

            /* 按落点偏离墙中心的程度给水平反弹分量 */
            float center = wl->x + wl->w / 2.0f;
            d->vx = (d->x - center) / (wl->w / 2.0f) * 220.0f;
            wl->active_ms = now;
            d->last_wall_hit_ms = now;

            // int vel = 60 + (int)(impact / 20.0f);
            // if (vel > 110) {
            //     vel = 110;
            // }
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

/* -------------------- 画布绘制 -------------------- */

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

static void zen_draw_canvas(lv_obj_t *canvas)
{
    uint32_t now = zen_now_ms();

    lv_canvas_fill_bg(canvas, engine_gui_theme_color(ZEN_C_BG), LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    if (s_zen.mode == 0) {
        for (int i = 0; i < ZEN_WALL_COUNT; i++) {
            bool below = (i >= ZEN_SEG_TOP_BOTTOM + ZEN_SEG_SIDES &&
                          i < 2 * ZEN_SEG_TOP_BOTTOM + ZEN_SEG_SIDES);
            zen_draw_wall(&layer, &s_zen.walls[i], now, below);
        }
    } else {
        lv_draw_rect_dsc_t frame_dsc;
        lv_draw_rect_dsc_init(&frame_dsc);
        frame_dsc.bg_opa = LV_OPA_TRANSP;
        frame_dsc.border_color = engine_gui_theme_color(ZEN_C_FRAME);
        frame_dsc.border_width = 3;
        frame_dsc.border_opa = LV_OPA_COVER;
        frame_dsc.radius = 16;
        lv_area_t frame_area = {
            ZEN_AREA_X0, ZEN_AREA_Y0,
            ZEN_AREA_X1 - 1, ZEN_AREA_Y1 - 1
        };
        lv_draw_rect(&layer, &frame_dsc, &frame_area);

        for (int i = 0; i < s_zen.drop_wall_count; i++) {
            zen_draw_wall(&layer, &s_zen.drop_walls[i], now, true);
        }
    }

    lv_canvas_finish_layer(canvas, &layer);
}

/* 运动球映射到 LED 控件：仅更新位置/颜色/可见性，重绘区域收敛到单个 LED */
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
                           ZEN_CANVAS_X + (int32_t)x - ZEN_LED_SIZE / 2,
                           ZEN_CANVAS_Y + (int32_t)y - ZEN_LED_SIZE / 2);
            lv_led_set_color(led, engine_gui_theme_color(color_idx));
            lv_obj_clear_flag(led, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(led, LV_OBJ_FLAG_HIDDEN);
        }
    }
    lvgl_port_unlock();
}

static void zen_canvas_redraw(void)
{
    if (s_zen_ui.canvas == NULL) {
        return;
    }
    lvgl_port_lock(portMAX_DELAY);
    zen_draw_canvas(s_zen_ui.canvas);
    lv_obj_clear_flag(s_zen_ui.canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(s_zen_ui.canvas);
    lvgl_port_unlock();
}

/* -------------------- 生命周期与事件 -------------------- */

static void zen_enter_mode(void)
{
    if (s_zen.mode == 0) {
        zen_init_walls();
        zen_init_balls();
    } else {
        memset(s_zen.drops, 0, sizeof(s_zen.drops));
        zen_init_drop_walls();
        s_zen.next_spawn_ms = 0;
    }
}

static void app_zen_on_input(app_base_t *self, const app_input_event_t *evt)
{
    (void)self;

    if (evt->type != APP_INPUT_TOUCH_DOWN || s_zen.mode != 0) {
        return;
    }

    int16_t local_x = evt->x - ZEN_CANVAS_X;
    int16_t local_y = evt->y - ZEN_CANVAS_Y;

    if (local_x < 0 || local_x >= ZEN_CANVAS_W ||
        local_y < 0 || local_y >= ZEN_CANVAS_H) {
        return;
    }

    zen_spawn_ball((float)local_x, (float)local_y);
}

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
            app_manager_show_notification_timeout("停止录制失败", 2000);
        }
        /* 视觉状态与最终提示由 on_update 在 finalize 完成后统一处理 */
        return;
    }

    service_recorder_result_t r = app_manager_record_start(TAG);
    switch (r) {
    case RECORDER_OK:
        app_manager_show_notification_timeout("开始录制", 1000);
        s_zen.recording_self = true;
        s_zen.recording_stop_pending = false;
        if (ui->btn_rec != NULL) {
            lv_obj_add_state(ui->btn_rec, LV_STATE_CHECKED);
        }
        break;
    case RECORDER_ERR_NO_SD:
        app_manager_show_notification_timeout("未检测到 SD 卡", 2000);
        break;
    case RECORDER_ERR_BUSY:
        app_manager_show_notification_timeout("正在录制中", 1500);
        break;
    default:
        app_manager_show_notification_timeout("录制启动失败", 2000);
        break;
    }
}

static void app_zen_on_ui_event(app_base_t *self, lv_event_t *e)
{
    ui_screen_zen_t *ui = (ui_screen_zen_t *)self->screen_ctx;
    lv_obj_t *target = lv_event_get_target_obj(e);

    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    if (target == ui->dropdown_mode) {
        s_zen.mode = (uint8_t)lv_dropdown_get_selected(ui->dropdown_mode);
        ESP_LOGI(TAG, "mode select=%d", s_zen.mode);
        zen_enter_mode();
        zen_canvas_redraw();
        zen_sync_leds();
    } else if (target == ui->dropdown_key) {
        s_zen.key_sel = (uint8_t)lv_dropdown_get_selected(ui->dropdown_key);
        ESP_LOGI(TAG, "key select=%d", s_zen.key_sel);
        zen_init_walls();
        zen_init_drop_walls();
        zen_canvas_redraw();
        zen_sync_leds();
    } else if (target == ui->dropdown_speed) {
        s_zen.speed_sel = (uint8_t)lv_dropdown_get_selected(ui->dropdown_speed);
        ESP_LOGI(TAG, "speed select=%d", s_zen.speed_sel);
    }
}

/* 首帧拆分倒计时（on_update 周期数），见 on_update 顶部 */
static uint8_t s_canvas_defer = 0;

static bool app_zen_on_init(app_base_t *self, void *screen_ctx)
{
    (void)self;
    ESP_LOGI(TAG, "zen mode init");

    memset(&s_zen, 0, sizeof(s_zen));
    s_zen.mode = 0;
    s_zen.key_sel = 0;
    s_zen.speed_sel = 2;
    s_zen.last_update_ms = zen_now_ms();

    ui_screen_zen_t *ui = (ui_screen_zen_t *)screen_ctx;

    if (ui->canvas == NULL || lv_obj_get_class(ui->canvas) != &lv_canvas_class) {
        ESP_LOGE(TAG, "canvas object invalid");
        return false;
    }

    size_t buf_size = (size_t)ZEN_CANVAS_W * ZEN_CANVAS_H * 2;
    s_zen.canvas_buf = heap_caps_calloc(1, buf_size, MALLOC_CAP_SPIRAM);
    if (s_zen.canvas_buf == NULL) {
        ESP_LOGE(TAG, "canvas buf alloc failed");
        return false;
    }

    lvgl_port_lock(portMAX_DELAY);
    lv_canvas_set_buffer(ui->canvas, s_zen.canvas_buf,
                         ZEN_CANVAS_W, ZEN_CANVAS_H,
                         LV_COLOR_FORMAT_RGB565);
    lv_dropdown_set_selected(ui->dropdown_mode, s_zen.mode);
    lv_dropdown_set_selected(ui->dropdown_key, s_zen.key_sel);
    lv_dropdown_set_selected(ui->dropdown_speed, s_zen.speed_sel);

    lv_obj_add_event_cb(ui->dropdown_mode, app_zen_event_cb, LV_EVENT_VALUE_CHANGED, self);
    lv_obj_add_event_cb(ui->dropdown_key, app_zen_event_cb, LV_EVENT_VALUE_CHANGED, self);
    lv_obj_add_event_cb(ui->dropdown_speed, app_zen_event_cb, LV_EVENT_VALUE_CHANGED, self);
    if (ui->btn_home != NULL) {
        lv_obj_add_event_cb(ui->btn_home, app_zen_home_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui->btn_rec != NULL) {
        lv_obj_add_event_cb(ui->btn_rec, app_zen_rec_btn_cb, LV_EVENT_CLICKED, self);
    }

    lvgl_port_unlock();

    zen_enter_mode();
    /* 首帧拆分：canvas 首绘推迟到 on_update 倒计时结束，背景先上屏 */
    s_canvas_defer = 3;
    return true;
}

static void app_zen_on_update(app_base_t *self)
{
    (void)self;
    ui_screen_zen_t *ui = &s_zen_ui;

    /* 首帧拆分：canvas 首绘推迟 ~30ms 让背景先上屏（防 DPI underrun 闪屏）。
     * 放在帧节流之前，保证倒计时按 on_update 周期（10ms）走 */
    if (s_canvas_defer > 0 && --s_canvas_defer == 0) {
        zen_canvas_redraw();
    }

    uint32_t now = zen_now_ms();
    uint32_t period_ms = 1000 / ZEN_UPDATE_HZ;
    if ((now - s_zen.last_update_ms) < period_ms) {
        return;
    }
    uint32_t dt = now - s_zen.last_update_ms;
    s_zen.last_update_ms = now;

    if (s_zen.mode == 0) {
        zen_update_balls(dt);
    } else {
        zen_update_drops(dt);
    }
    zen_sync_leds();

    /* 先处理上一周期检测到的停止 finalize 结果；
     * task_app 中 service_recorder_process 在 on_update 之后执行，
     * 故 pending 至少延迟一帧再读取 last_path 才能保证最终状态。 */
    if (s_zen.recording_stop_pending && !app_manager_record_is_recording()) {
        char path[256];
        if (app_manager_record_get_last_path(path, sizeof(path))) {
            app_manager_show_notification_timeout("录音已保存", 2000);
        } else {
            app_manager_show_notification_timeout("录制时间过短，已丢弃", 2000);
        }
        s_zen.recording_stop_pending = false;
    }

    if (s_zen.recording_self && !app_manager_record_is_recording()) {
        s_zen.recording_self = false;
        s_zen.recording_stop_pending = true;
    }

    if (ui->btn_rec != NULL) {
        if (s_zen.recording_self) {
            lv_obj_add_state(ui->btn_rec, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(ui->btn_rec, LV_STATE_CHECKED);
        }
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
}

static void app_zen_on_resume(app_base_t *self)
{
    (void)self;
    ESP_LOGI(TAG, "resume");
    s_zen.last_update_ms = zen_now_ms();
    zen_canvas_redraw();
    zen_sync_leds();
}

static void app_zen_on_destroy(app_base_t *self)
{
    (void)self;
    ESP_LOGI(TAG, "destroy");

    /* 移除 on_init 注册的事件回调：EEZ 屏幕对象持久存在，
     * 不移除会在再次进入时重复注册导致一次事件多次触发 */
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
    if (s_zen_ui.btn_home != NULL) {
        lv_obj_remove_event_cb(s_zen_ui.btn_home, app_zen_home_cb);
    }
    if (s_zen_ui.btn_rec != NULL) {
        lv_obj_remove_event_cb(s_zen_ui.btn_rec, app_zen_rec_btn_cb);
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
