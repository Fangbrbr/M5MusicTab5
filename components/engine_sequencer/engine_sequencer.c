/**
 * @file engine_sequencer.c
 * @brief 音序器纯逻辑引擎：欧几里得生成、子 tick 播放状态机、概率/人性化、CC 锁、M5P 序列化
 *
 * 无 OS 依赖：时基由上层注入（service_timer 周期调 tick_hook），随机数内置
 * xorshift32（上层播种），MIDI 输出仅经 engine_midi 生产者入口。
 */

#include "engine_sequencer.h"
#include "engine_midi.h"
#include <string.h>

/* -------------------- 常量表（UI 只读，换音色只改此表） -------------------- */

/* GM 鼓映射：通道 10（代码层索引 9）；顺序与 EEZ 网格标签 KICK/SNR/CHH/OHH/TOM/CLP/PRC/FX 一致 */
static const uint8_t s_seq_midi_ch[SEQ_TRACK_NUM] = {
    9, 9, 9, 9, 9, 9, 9, 9,
};

/* 默认音符 = 8 轨最常用预设（用户指定；全部在 EEZ 下拉 23 项内，
 * 缩写与 s_drum_short 一致：KICK/SNAR/CLAP/CHHH/OHHH/L-TM/CRSH/RIDE）。
 * 仅作初次进入/恢复系统默认值；实际音符由 UI 下拉自选并落 NVS。 */
static const uint8_t s_seq_midi_note[SEQ_TRACK_NUM] = {
    36, 38, 39, 42, 46, 45, 49, 51,
};

/* 随机生成角色（§6）：踩点风格按"实际音色 note"分派——轨道音色可由下拉任意
 * 切换，按轨道索引固定角色会导致换音色后风格错乱（2026-09 用户指正） */
typedef enum {
    SEQ_ROLE_KICK, SEQ_ROLE_SNARE, SEQ_ROLE_CLAP, SEQ_ROLE_CHH,
    SEQ_ROLE_OHH, SEQ_ROLE_TOM, SEQ_ROLE_CRASH, SEQ_ROLE_RIDE, SEQ_ROLE_PERC,
} seq_role_t;

static seq_role_t seq_role_of_note(uint8_t note)
{
    switch (note) {
    case 35: case 36: return SEQ_ROLE_KICK;
    case 38: case 40: return SEQ_ROLE_SNARE;
    case 39:          return SEQ_ROLE_CLAP;
    case 42: case 44: return SEQ_ROLE_CHH;    /* 44=Pedal Hat 按闭合处理 */
    case 46:          return SEQ_ROLE_OHH;
    case 41: case 43: case 45:
    case 47: case 48: case 50: return SEQ_ROLE_TOM;
    case 49: case 52: case 55: return SEQ_ROLE_CRASH;  /* Crash/Chinese/Splash */
    case 51: case 53: return SEQ_ROLE_RIDE;   /* Ride/Ride Bell */
    default:          return SEQ_ROLE_PERC;   /* 37/56/57 等点缀打击 */
    }
}

/* 角色 base 脉冲（§6） */
static const uint8_t s_role_pulses[] = {
    [SEQ_ROLE_KICK] = 4, [SEQ_ROLE_SNARE] = 2, [SEQ_ROLE_CLAP] = 2,
    [SEQ_ROLE_CHH] = 8,  [SEQ_ROLE_OHH] = 2,   [SEQ_ROLE_TOM] = 2,
    [SEQ_ROLE_CRASH] = 1, [SEQ_ROLE_RIDE] = 3, [SEQ_ROLE_PERC] = 2,
};

/* 角色锚定旋转：骨架钉死经典位（Snare/Clap 2/4 拍、CHH 正拍 8 分、Crash 字头） */
static uint8_t seq_role_anchor(seq_role_t role, uint8_t steps)
{
    switch (role) {
    case SEQ_ROLE_KICK:  return 1;                        /* 正拍（原始位 +1 归 0） */
    case SEQ_ROLE_SNARE:
    case SEQ_ROLE_CLAP:  return (uint8_t)(steps / 4);     /* backbeat 16→{4,12} / 32→{8,24} */
    case SEQ_ROLE_CHH:   return 1;                        /* 正拍 8 分 */
    case SEQ_ROLE_OHH:   return (uint8_t)(steps * 3 / 8); /* 反拍 16→{6,14} / 32→{12,28} */
    case SEQ_ROLE_TOM:   return (uint8_t)(steps / 8);     /* 小节弱段 */
    case SEQ_ROLE_CRASH: return 1;                        /* 字头 */
    case SEQ_ROLE_RIDE:  return 1;                        /* 字头起 */
    default:             return (uint8_t)(steps / 8);     /* Perc：小节弱段 */
    }
}

#define SEQ_DEFAULT_BPM      120
#define SEQ_DEFAULT_SWING    0
#define SEQ_BPM_MIN          20
#define SEQ_BPM_MAX          300
#define SEQ_SWING_MAX        50

/* -------------------- 运行时状态 -------------------- */

static seq_slot_t s_slots[SEQ_SLOT_NUM];
static bool      s_playing;
static uint8_t   s_sub_tick;          /* 0..SEQ_TICKS_PER_STEP-1 */
static uint8_t   s_current_step;      /* 0..当前槽 step_count-1 */
static int8_t    s_checked_slot;      /* 当前播放/编辑槽，-1=无 */
static int8_t    s_pending_slot;      /* 播放中请求切换的槽，-1=无 */
static uint16_t  s_bpm;
static uint8_t   s_swing;             /* 0-50 */
static uint16_t s_on_mask;           /* 当前步已触发 Note On 的轨掩码（用于下一步 Note Off） */
static uint8_t  s_on_note[SEQ_TRACK_NUM]; /* 各轨实际触发的 note（note-off 用，防改 note 后旧音卡住） */
static uint32_t  s_rng_state;         /* xorshift32 状态 */
/* 装载量化暂存（§8）：目标槽==播放槽且播放中时，deserialize 先落暂存，
 * 15→0 回卷时由 tick_hook 一次性换入（避免播放中逐字段写入被 tick 读到半成品） */
static seq_pattern_t s_stage_pattern;
static int8_t        s_stage_slot = -1;

/* -------------------- 内部工具 -------------------- */

static uint32_t seq_rng_next(void)
{
    uint32_t x = s_rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_rng_state = x;
    return x;
}

/* ±range 内均匀随机（含端点），range 0 时恒 0 */
static int seq_rng_signed(int range)
{
    if (range <= 0) {
        return 0;
    }
    return (int)(seq_rng_next() % (uint32_t)(2 * range + 1)) - range;
}

static int seq_clamp(int v, int lo, int hi)
{
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return v;
}

/* 当前槽逻辑步数（16/32）；无当前槽时回落页宽 */
static uint8_t seq_cur_step_count(void)
{
    if (s_checked_slot < 0 || s_checked_slot >= SEQ_SLOT_NUM) {
        return SEQ_STEP_NUM;
    }
    uint8_t n = s_slots[s_checked_slot].pattern.step_count;
    return (n == SEQ_STEP_MAX) ? SEQ_STEP_MAX : SEQ_STEP_NUM;
}

/* CRC16-CCITT (poly 0x1021, init 0xFFFF)，覆盖整个 M5P 前部 */
static uint16_t seq_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

/* -------------------- MIDI 输出（仅生产者入口） -------------------- */

static void seq_midi_note(uint8_t ch, uint8_t note, uint8_t vel)
{
    engine_midi_event_t evt = {0};
    evt.type = (vel > 0) ? ENGINE_MIDI_MSG_NOTE_ON : ENGINE_MIDI_MSG_NOTE_OFF;
    evt.channel = ch;
    evt.data1 = note;
    evt.data2 = vel;
    evt.source_port = ENGINE_MIDI_PORT_APP;
    engine_midi_publish(&evt, 0);
}

static void seq_midi_cc(uint8_t ch, uint8_t num, uint8_t val)
{
    engine_midi_event_t evt = {0};
    evt.type = ENGINE_MIDI_MSG_CONTROL_CHANGE;
    evt.channel = ch;
    evt.data1 = num;
    evt.data2 = val;
    evt.source_port = ENGINE_MIDI_PORT_APP;
    engine_midi_publish(&evt, 0);
}

/* -------------------- 播放状态机（§5） -------------------- */

/* 暂存装载生效：一次性换入目标槽 */
static void seq_stage_apply(void)
{
    if (s_stage_slot < 0) {
        return;
    }
    memcpy(&s_slots[s_stage_slot].pattern, &s_stage_pattern, sizeof(seq_pattern_t));
    s_slots[s_stage_slot].has_content = true;
    s_stage_slot = -1;
}

static void seq_note_off_mask(uint16_t mask)
{
    for (int t = 0; t < SEQ_TRACK_NUM; t++) {
        if (mask & (1u << t)) {
            seq_midi_note(s_seq_midi_ch[t], s_on_note[t], 0);
        }
    }
}

static void seq_trigger_step(uint8_t step)
{
    if (s_checked_slot < 0 || s_checked_slot >= SEQ_SLOT_NUM) {
        return;
    }
    seq_pattern_t *pat = &s_slots[s_checked_slot].pattern;
    /* 越界步不触发：16 步模式下 B 页保留数据只存不播 */
    if (step >= pat->step_count) {
        return;
    }

    /* audible = any_solo ? solo : !mute */
    bool any_solo = false;
    for (int t = 0; t < SEQ_TRACK_NUM; t++) {
        if (pat->tracks[t].solo) {
            any_solo = true;
            break;
        }
    }

    for (int t = 0; t < SEQ_TRACK_NUM; t++) {
        seq_track_t *tr = &pat->tracks[t];
        bool audible = any_solo ? tr->solo : !tr->mute;
        if (!audible) {
            continue;
        }
        /* note=0 表示 None：该轨静音（dropdown 首项），不触发任何音 */
        if (tr->midi_note == 0) {
            continue;
        }
        seq_step_t *st = &tr->steps[step];
        if (!st->active) {
            continue;
        }

        /* 最终概率 = (覆盖?覆盖:全局) + rand±(rand_temp*5)，clamp 0-100。
         * Trap: base=100（必响）时不得再抖动/掷签——clamp 封顶后只剩向下抖动，
         * 等于每拍随机丢音（默认 rand_temp=3 → 平均 ~7% 丢步，2026-09 真机） */
        int base_prob = (st->probability == 255) ? tr->probability : st->probability;
        if (base_prob < 100) {
            int prob = seq_clamp(base_prob + seq_rng_signed((int)tr->rand_temp * 5), 0, 100);
            if ((int)(seq_rng_next() % 100) >= prob) {
                continue;
            }
        }

        /* 最终力度 = (覆盖?覆盖:全局) + rand±(rand_temp*5)，clamp 1-127 */
        int base_vel = (st->velocity == 0) ? tr->velocity : st->velocity;
        int vel = seq_clamp(base_vel + seq_rng_signed((int)tr->rand_temp * 5), 1, 127);

        /* CC 先于 Note On */
        if (st->cc_num != 0) {
            seq_midi_cc(tr->midi_ch, st->cc_num, st->cc_val);
        }
        seq_midi_note(tr->midi_ch, tr->midi_note, (uint8_t)vel);
        s_on_note[t] = tr->midi_note;
        s_on_mask |= (uint16_t)(1u << t);
    }
}

void engine_seq_tick_hook(void *arg)
{
    (void)arg;
    if (!s_playing) {
        return;
    }

    /* 新步开始（sub_tick==0）：先关上一步触发的音符（gate=1 step），再触发本步 */
    if (s_sub_tick == 0) {
        seq_note_off_mask(s_on_mask);
        s_on_mask = 0;
    }

    /* 偶数步 sub_tick==0 触发；奇数步 sub_tick==swing*8/100 触发 */
    uint8_t trigger_tick = (s_current_step & 1) ? (uint8_t)((s_swing * 8) / 100) : 0;
    if (s_sub_tick == trigger_tick) {
        seq_trigger_step(s_current_step);
    }

    s_sub_tick++;
    if (s_sub_tick >= SEQ_TICKS_PER_STEP) {
        s_sub_tick = 0;
        s_current_step++;
        if (s_current_step >= seq_cur_step_count()) {
            s_current_step = 0;
            /* 末步→0 回卷：pending_slot 生效 */
            if (s_pending_slot >= 0) {
                s_checked_slot = s_pending_slot;
                s_pending_slot = -1;
            }
            /* 回卷同步点：播放中的暂存装载在此一次性生效 */
            seq_stage_apply();
        }
    }
}

void engine_seq_start(void)
{
    if (s_playing) {
        return;
    }
    s_playing = true;
    s_sub_tick = 0;
    s_current_step = 0;
    s_on_mask = 0;
}

void engine_seq_stop(void)
{
    if (!s_playing) {
        return;
    }
    s_playing = false;
    s_sub_tick = 0;
    s_current_step = 0;
    /* 停止时全轨 Note Off 防卡音 */
    seq_note_off_mask(s_on_mask);
    s_on_mask = 0;
    /* 停止即量化点消失：暂存装载立即生效（用户停下即期望看到新内容） */
    seq_stage_apply();
}

bool engine_seq_is_playing(void)
{
    return s_playing;
}

uint8_t engine_seq_get_current_step(void)
{
    return s_current_step;
}

/* -------------------- 全局参数 -------------------- */

void engine_seq_set_bpm(uint16_t bpm)
{
    if (bpm < SEQ_BPM_MIN) bpm = SEQ_BPM_MIN;
    if (bpm > SEQ_BPM_MAX) bpm = SEQ_BPM_MAX;
    s_bpm = bpm;
}

uint16_t engine_seq_get_bpm(void)
{
    return s_bpm;
}

void engine_seq_set_swing(uint8_t swing)
{
    s_swing = (swing > SEQ_SWING_MAX) ? SEQ_SWING_MAX : swing;
}

uint8_t engine_seq_get_swing(void)
{
    return s_swing;
}

void engine_seq_set_step_count(uint8_t steps)
{
    if (steps != SEQ_STEP_NUM && steps != SEQ_STEP_MAX) {
        return;
    }
    if (s_checked_slot < 0 || s_checked_slot >= SEQ_SLOT_NUM) {
        return;
    }
    s_slots[s_checked_slot].pattern.step_count = steps;
    /* 播放中收缩步数且播放头已越界：归零，防越界步继续触发 B 页保留数据 */
    if (s_current_step >= steps) {
        s_current_step = 0;
        s_sub_tick = 0;
    }
}

uint8_t engine_seq_get_step_count(void)
{
    return seq_cur_step_count();
}

/* -------------------- 编辑 -------------------- */

static seq_track_t *seq_track_ptr(int8_t slot, uint8_t track)
{
    if (slot < 0 || slot >= SEQ_SLOT_NUM || track >= SEQ_TRACK_NUM) {
        return NULL;
    }
    return &s_slots[slot].pattern.tracks[track];
}

void engine_seq_track_set_param(uint8_t track, uint8_t param, uint8_t value)
{
    if (s_checked_slot < 0) {
        return;
    }
    seq_track_t *tr = seq_track_ptr(s_checked_slot, track);
    if (tr == NULL) {
        return;
    }
    switch (param) {
    case 0: tr->rand_temp = (value > 10) ? 10 : value; break;
    case 1: tr->velocity = (value == 0) ? 1 : value; break;   /* 力度 1-127 */
    case 2: tr->probability = (value > 100) ? 100 : value; break;
    /* mute/solo 互斥：同一轨 solo 与 mute 不能同时置位，避免状态串对不上 */
    case 3: tr->mute = (value != 0); if (tr->mute) tr->solo = false; break;
    /* solo 全局唯一：开某轨 solo 时清掉同槽所有其他轨的 solo（Trap: 漏清则
     * 多轨同时 solo 状态串混乱，2026-08）。mute 允许多轨共存，不在此清。 */
    case 4:
        if (value != 0) {
            for (int t = 0; t < SEQ_TRACK_NUM; t++) {
                seq_track_t *other = seq_track_ptr(s_checked_slot, t);
                if (other != NULL) {
                    other->solo = false;
                }
            }
            tr->solo = true;
            tr->mute = false;
        } else {
            tr->solo = false;
        }
        break;
    case 5: tr->midi_note = value; break;   /* 鼓 note 可配置（0=None，1-127=GM 鼓） */
    default: return;
    }
    s_slots[s_checked_slot].has_content = true;
}

/* 系统默认鼓 note（与常量表一致），供 restore/首次回填 */
uint8_t engine_seq_default_note(uint8_t track)
{
    if (track >= SEQ_TRACK_NUM) {
        return 0;
    }
    return s_seq_midi_note[track];
}

/* 读指定槽轨道 note（UI 回填 dropdown / 轨道名用） */
uint8_t engine_seq_track_get_note(int8_t slot, uint8_t track)
{
    if (slot < 0 || slot >= SEQ_SLOT_NUM || track >= SEQ_TRACK_NUM) {
        return 0;
    }
    return s_slots[slot].pattern.tracks[track].midi_note;
}

/* 对所有槽应用轨道偏好（NVS 恢复用）。
 * note 直接赋值：0=None 是合法选择（用户显式选 None），必须恢复，不能当作
 * "未配置"跳过（Trap: 原 `if (note != 0)` 导致选 None 重启后恢复成默认，2026-08）。
 * "是否配置过"由上层 NVS 的 velocity 哨兵判定，此处只负责应用。 */
void engine_seq_apply_track_prefs(uint8_t track, uint8_t note,
                                  uint8_t rand_temp, uint8_t velocity,
                                  uint8_t probability)
{
    if (track >= SEQ_TRACK_NUM) {
        return;
    }
    for (int s = 0; s < SEQ_SLOT_NUM; s++) {
        seq_track_t *tr = &s_slots[s].pattern.tracks[track];
        tr->midi_note = note;
        tr->rand_temp = rand_temp;
        tr->velocity = velocity;
        tr->probability = probability;
    }
}

void engine_seq_step_toggle(uint8_t track, uint8_t step)
{
    if (s_checked_slot < 0) {
        return;
    }
    seq_track_t *tr = seq_track_ptr(s_checked_slot, track);
    if (tr == NULL || step >= seq_cur_step_count()) {
        return;
    }
    tr->steps[step].active = !tr->steps[step].active;
    s_slots[s_checked_slot].has_content = true;
}

void engine_seq_step_set_lock(uint8_t track, uint8_t step,
                              uint8_t vel, uint8_t prob,
                              uint8_t cc_num, uint8_t cc_val)
{
    if (s_checked_slot < 0) {
        return;
    }
    seq_track_t *tr = seq_track_ptr(s_checked_slot, track);
    if (tr == NULL || step >= seq_cur_step_count()) {
        return;
    }
    seq_step_t *st = &tr->steps[step];
    st->velocity = vel;          /* 0=跟随全局 */
    st->probability = prob;      /* 255=跟随全局 */
    st->cc_num = cc_num;
    st->cc_val = cc_val;
    s_slots[s_checked_slot].has_content = true;
}

void engine_seq_step_clear_lock(uint8_t track, uint8_t step)
{
    if (s_checked_slot < 0) {
        return;
    }
    seq_track_t *tr = seq_track_ptr(s_checked_slot, track);
    if (tr == NULL || step >= seq_cur_step_count()) {
        return;
    }
    seq_step_t *st = &tr->steps[step];
    st->velocity = 0;
    st->probability = 255;
    st->cc_num = 0;
    st->cc_val = 0;
    s_slots[s_checked_slot].has_content = true;
}

/* -------------------- 随机生成（§6） -------------------- */

/* 欧几里得（Bresenham 变体）：steps 步内分布——累加器每步加 pulses，≥steps
 * 置音符并减 steps；结果在 steps 位内循环左移 rotate 位。
 * 验收（steps=16）：pulses=4,rotate=1 → bit0,4,8,12；0→全灭；≥steps→全亮；间距差≤1。 */
static uint32_t seq_euclid(uint8_t pulses, uint8_t rotate, uint8_t steps)
{
    if (pulses == 0) {
        return 0;
    }
    if (pulses >= steps) {
        return (steps >= 32) ? UINT32_MAX : ((uint32_t)1u << steps) - 1u;
    }
    uint32_t bits = 0;
    uint32_t acc = 0;
    for (int s = 0; s < steps; s++) {
        acc += pulses;
        if (acc >= steps) {
            bits |= (uint32_t)1u << s;
            acc -= steps;
        }
    }
    rotate = (uint8_t)(rotate % steps);
    if (rotate) {
        bits = (bits << rotate) | (bits >> (steps - rotate));
    }
    return bits;
}

void engine_seq_randomize_all(void)
{
    if (s_checked_slot < 0) {
        return;
    }
    seq_pattern_t *pat = &s_slots[s_checked_slot].pattern;
    /* 当前步数域生成：16=整小节 groove，32=双小节铺开；超域步清空（骰子本即破坏式再生成） */
    uint8_t steps = pat->step_count;
    if (steps != SEQ_STEP_NUM && steps != SEQ_STEP_MAX) {
        steps = SEQ_STEP_NUM;
    }

    for (int t = 0; t < SEQ_TRACK_NUM; t++) {
        seq_track_t *tr = &pat->tracks[t];
        uint8_t temp = tr->rand_temp;
        seq_role_t role = seq_role_of_note(tr->midi_note);

        /* 脉冲抖动只给"血肉"角色（CHH/OHH/Tom/Ride/Perc）；骨架角色
         * （Kick/Snare/Clap/Crash）脉冲固定，保证 backbeat 骨架在任何温度下不散 */
        uint8_t pulses = s_role_pulses[role];
        if (temp > 0 && role >= SEQ_ROLE_CHH && role != SEQ_ROLE_CRASH) {
            pulses = (uint8_t)(pulses + seq_rng_next() % (temp + 1));
        }

        /* 角色锚定旋转；temp≥5 才 ±1 步抖动（低温度保骨架） */
        uint8_t rotate = seq_role_anchor(role, steps);
        if (temp >= 5) {
            rotate = (uint8_t)((rotate + (int)(seq_rng_next() % 3) - 1 + steps) % steps);
        }

        uint32_t bits = seq_euclid(pulses, rotate, steps);

        /* 每步以 rand_temp*2% 概率翻转（原 *5 在 32 步域≈每轨 5 处随机音，远超人性化） */
        for (int s = 0; s < steps; s++) {
            if (temp > 0 && (seq_rng_next() % 100) < (uint32_t)(temp * 2)) {
                bits ^= (uint32_t)1u << s;
            }
        }

        /* Kick 角色且 temp<5 强制 bit0 */
        if (role == SEQ_ROLE_KICK && temp < 5) {
            bits |= 1u;
        }

        /* 只覆盖 active，不动单步锁定；超域步清空 */
        for (int s = 0; s < SEQ_STEP_MAX; s++) {
            tr->steps[s].active = (s < steps) ? (bool)((bits >> s) & 1u) : false;
        }
    }

    s_slots[s_checked_slot].has_content = true;
}

void engine_seq_clear_steps(uint8_t first, uint8_t count)
{
    if (s_checked_slot < 0 || first >= SEQ_STEP_MAX) {
        return;
    }
    uint8_t end = (uint8_t)(first + count);
    if (end > SEQ_STEP_MAX || end < first) {
        end = SEQ_STEP_MAX;
    }
    seq_pattern_t *pat = &s_slots[s_checked_slot].pattern;
    for (int t = 0; t < SEQ_TRACK_NUM; t++) {
        for (int s = first; s < end; s++) {
            pat->tracks[t].steps[s].active = false;
        }
    }
    s_slots[s_checked_slot].has_content = true;
}

/* -------------------- 槽位 -------------------- */

void engine_seq_pattern_request(int8_t slot)
{
    if (slot < 0 || slot >= SEQ_SLOT_NUM) {
        return;
    }
    if (s_playing) {
        s_pending_slot = slot;   /* 播放中量化：小节末生效 */
    } else {
        s_checked_slot = slot;   /* 停止立即生效 */
        s_pending_slot = -1;
    }
}

int8_t engine_seq_pattern_current(void)
{
    return s_checked_slot;
}

int8_t engine_seq_pattern_pending(void)
{
    return s_pending_slot;
}

const seq_slot_t *engine_seq_slot_get(int8_t slot)
{
    if (slot < 0 || slot >= SEQ_SLOT_NUM) {
        return NULL;
    }
    return &s_slots[slot];
}

bool engine_seq_slot_is_checked(int8_t slot)
{
    return slot >= 0 && slot == s_checked_slot;
}

bool engine_seq_slot_is_empty(int8_t slot)
{
    if (slot < 0 || slot >= SEQ_SLOT_NUM) {
        return true;
    }
    const seq_pattern_t *pat = &s_slots[slot].pattern;
    for (int t = 0; t < SEQ_TRACK_NUM; t++) {
        for (int s = 0; s < SEQ_STEP_MAX; s++) {
            if (pat->tracks[t].steps[s].active) {
                return false;
            }
        }
    }
    return true;
}

/* -------------------- 生命周期 -------------------- */

void engine_seq_init(void)
{
    memset(s_slots, 0, sizeof(s_slots));
    s_playing = false;
    s_sub_tick = 0;
    s_current_step = 0;
    s_checked_slot = 0;
    s_pending_slot = -1;
    s_stage_slot = -1;
    s_bpm = SEQ_DEFAULT_BPM;
    s_swing = SEQ_DEFAULT_SWING;
    s_on_mask = 0;
    memset(s_on_note, 0, sizeof(s_on_note));

    /* 常量表填充：UI 只读；新槽默认 16 步 */
    for (int t = 0; t < SEQ_TRACK_NUM; t++) {
        for (int s = 0; s < SEQ_SLOT_NUM; s++) {
            s_slots[s].pattern.step_count = SEQ_STEP_NUM;
            s_slots[s].pattern.tracks[t].midi_ch = s_seq_midi_ch[t];
            s_slots[s].pattern.tracks[t].midi_note = s_seq_midi_note[t];
            s_slots[s].pattern.tracks[t].velocity = 100;
            s_slots[s].pattern.tracks[t].probability = 100;
            s_slots[s].pattern.tracks[t].rand_temp = 3;   /* 默认 3：即开即玩，随机输出可辨 */
            for (int step = 0; step < SEQ_STEP_MAX; step++) {
                s_slots[s].pattern.tracks[t].steps[step].probability = 255;
            }
        }
    }
}

void engine_seq_deinit(void)
{
    engine_seq_stop();
    memset(s_slots, 0, sizeof(s_slots));
    s_checked_slot = -1;
    s_pending_slot = -1;
}

void engine_seq_seed(uint32_t seed)
{
    s_rng_state = seed ? seed : 0x9E3779B9u;
}

/* -------------------- M5P 序列化（§3） -------------------- */

/* 小端读写（逐字段，禁 struct memcpy） */
static void seq_put_u16le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

static uint16_t seq_get_u16le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

int engine_seq_serialize(int8_t slot, uint8_t *buf, size_t cap)
{
    if (slot < 0 || slot >= SEQ_SLOT_NUM || buf == NULL) {
        return 0;
    }
    const seq_slot_t *sl = &s_slots[slot];
    uint8_t steps = sl->pattern.step_count;
    if (steps != SEQ_STEP_NUM && steps != SEQ_STEP_MAX) {
        steps = SEQ_STEP_NUM;   /* 防御：非法值回落 16 */
    }
    size_t total = SEQ_M5P_FILE_SIZE(steps);
    if (cap < total) {
        return 0;
    }

    uint8_t *p = buf;
    *p++ = SEQ_M5P_MAGIC0;
    *p++ = SEQ_M5P_MAGIC1;
    *p++ = SEQ_M5P_MAGIC2;
    *p++ = SEQ_M5P_MAGIC3;
    seq_put_u16le(p, SEQ_M5P_MINOR); p += 2;
    *p++ = steps;                             /* step_count */
    *p++ = 0;                                 /* reserved */

    for (int t = 0; t < SEQ_TRACK_NUM; t++) {
        const seq_track_t *tr = &sl->pattern.tracks[t];
        *p++ = tr->midi_ch;
        *p++ = tr->midi_note;
        *p++ = tr->rand_temp;
        *p++ = tr->velocity;
        *p++ = tr->probability;
        *p++ = tr->mute ? 1 : 0;
        *p++ = tr->solo ? 1 : 0;
        for (int s = 0; s < steps; s++) {
            const seq_step_t *st = &tr->steps[s];
            *p++ = st->active ? 1 : 0;
            *p++ = st->velocity;
            *p++ = st->probability;
            *p++ = st->cc_num;
            *p++ = st->cc_val;
        }
    }

    uint16_t crc = seq_crc16(buf, total - SEQ_M5P_CRC_SIZE);
    seq_put_u16le(p, crc);
    return (int)total;
}

bool engine_seq_m5p_validate(const uint8_t *buf, size_t len)
{
    if (buf == NULL || len < SEQ_M5P_FILE_SIZE(SEQ_STEP_NUM)) {
        return false;
    }
    if (buf[0] != SEQ_M5P_MAGIC0 || buf[1] != SEQ_M5P_MAGIC1 ||
        buf[2] != SEQ_M5P_MAGIC2 || buf[3] != SEQ_M5P_MAGIC3) {
        return false;
    }
    if (seq_get_u16le(&buf[4]) > SEQ_M5P_MINOR) {
        return false;
    }
    /* step_count 必须合法且与文件长度一致（v1 文件该字节为 reserved=0，在此被拒） */
    uint8_t steps = buf[6];
    if (steps != SEQ_STEP_NUM && steps != SEQ_STEP_MAX) {
        return false;
    }
    if (len != SEQ_M5P_FILE_SIZE(steps)) {
        return false;
    }
    uint16_t stored = seq_get_u16le(&buf[len - 2]);
    uint16_t calc = seq_crc16(buf, len - SEQ_M5P_CRC_SIZE);
    return stored == calc;
}

/* 逐字段读出 M5P body 到指定 pattern（调用方已校验三重）；
 * 步数据只存 step_count 步，[step_count, SEQ_STEP_MAX) 复位为"未激活+跟随全局"，
 * 防残留旧数据在切回 32 步时以非法覆盖值（如 prob=0 永不发声）复活 */
static void seq_m5p_read_body(seq_pattern_t *pat, const uint8_t *buf)
{
    uint8_t steps = buf[6];
    pat->step_count = steps;
    const uint8_t *p = buf + SEQ_M5P_HEADER_SIZE;
    for (int t = 0; t < SEQ_TRACK_NUM; t++) {
        seq_track_t *tr = &pat->tracks[t];
        tr->midi_ch = *p++;
        tr->midi_note = *p++;
        tr->rand_temp = *p++;
        tr->velocity = *p++;
        tr->probability = *p++;
        tr->mute = (*p++ != 0);
        tr->solo = (*p++ != 0);
        for (int s = 0; s < steps; s++) {
            seq_step_t *st = &tr->steps[s];
            st->active = (*p++ != 0);
            st->velocity = *p++;
            st->probability = *p++;
            st->cc_num = *p++;
            st->cc_val = *p++;
        }
        for (int s = steps; s < SEQ_STEP_MAX; s++) {
            seq_step_t *st = &tr->steps[s];
            st->active = false;
            st->velocity = 0;
            st->probability = 255;
            st->cc_num = 0;
            st->cc_val = 0;
        }
    }
}

bool engine_seq_deserialize(int8_t slot, const uint8_t *buf, size_t len)
{
    if (slot < 0 || slot >= SEQ_SLOT_NUM || !engine_seq_m5p_validate(buf, len)) {
        return false;
    }

    /* §8：装入当前播放槽且播放中 → 先落暂存，回卷量化生效（stop 时立即应用）。
     * 直接写入会被 tick_hook 在 esp_timer 上下文并发读到新旧混杂的 pattern。 */
    if (slot == s_checked_slot && s_playing) {
        seq_m5p_read_body(&s_stage_pattern, buf);
        s_stage_slot = slot;
        return true;
    }

    seq_m5p_read_body(&s_slots[slot].pattern, buf);
    s_slots[slot].has_content = true;
    return true;
}
