/**
 * @file engine_sequencer.h
 * @brief 音序器纯逻辑引擎：欧几里得生成、子 tick 播放状态机、概率/人性化、CC 锁、M5P 序列化
 *
 * 依赖约束（AGENTS.md 分层）：
 * - 仅可调用同层 engine_midi 生产者入口，不依赖 service/OS 定时器/LVGL/文件系统。
 * - 时基由上层（app_sequencer 经 service_timer）注入：周期性调用 engine_seq_tick_hook()。
 * - 随机数使用内置 xorshift32，上层以 esp_random() 播种（engine_seq_seed），保持引擎无 OS 依赖。
 */

#ifndef ENGINE_SEQUENCER_H
#define ENGINE_SEQUENCER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEQ_TRACK_NUM          8
#define SEQ_STEP_NUM           16      /* 单页物理步数（页宽）：网格恒 16 列，A/B 页各 16 步 */
#define SEQ_STEP_MAX           32      /* 模式上限：16/32 步双模式 */
#define SEQ_SLOT_NUM           6
#define SEQ_TICKS_PER_STEP     8

/* M5P 文件布局常量（v2：头部含 step_count，body 按实际步数变长；与 v1 不兼容，
 * v1 文件 reserved 字节=0 → step_count 非法被校验拒绝，天然淘汰） */
#define SEQ_M5P_MAGIC0         'M'
#define SEQ_M5P_MAGIC1         '5'
#define SEQ_M5P_MAGIC2         'P'
#define SEQ_M5P_MAGIC3         '1'
#define SEQ_M5P_MINOR          2
#define SEQ_M5P_HEADER_SIZE    8        /* magic4 + minor2 + step_count1 + reserved1 */
#define SEQ_M5P_TRACK_HEAD     7        /* ch/note/rand_temp/velocity/probability/mute/solo */
#define SEQ_M5P_STEP_SIZE      5        /* active/velocity/probability/cc_num/cc_val */
#define SEQ_M5P_CRC_SIZE       2
/* 文件尺寸按实际步数变长：body 只写 step_count 步 */
#define SEQ_M5P_FILE_SIZE(steps) \
    (SEQ_M5P_HEADER_SIZE + SEQ_TRACK_NUM * (SEQ_M5P_TRACK_HEAD + (steps) * SEQ_M5P_STEP_SIZE) + SEQ_M5P_CRC_SIZE)
/* 缓冲/读取上限按 32 步上限 */
#define SEQ_M5P_TOTAL_SIZE     SEQ_M5P_FILE_SIZE(SEQ_STEP_MAX)

/** @brief 单步数据（RAM 模型，非落盘布局） */
typedef struct {
    bool    active;          /*!< 是否启用 */
    uint8_t velocity;        /*!< 0=跟随轨道全局；1-127=单步覆盖 */
    uint8_t probability;     /*!< 255=跟随轨道全局；0-100=单步覆盖 */
    uint8_t cc_num;          /*!< 0=无锁定；1-127=锁定 CC */
    uint8_t cc_val;          /*!< 锁定 CC 值 */
} seq_step_t;

/** @brief 单轨（midi_note 可由 UI 下拉配置并落 NVS） */
typedef struct {
    uint8_t midi_ch;         /*!< MIDI 通道（常量表，UI 只读） */
    uint8_t midi_note;       /*!< MIDI 音符（0=None 静音，1-127=GM 鼓） */
    uint8_t rand_temp;       /*!< 人性化抖动 0-10，默认 0 */
    uint8_t velocity;        /*!< 全局力度 1-127，默认 100 */
    uint8_t probability;     /*!< 全局概率 0-100，默认 100 */
    bool mute;
    bool solo;
    seq_step_t steps[SEQ_STEP_MAX];  /*!< 容量上限；逻辑步数由所属 pattern.step_count 决定 */
} seq_track_t;

/** @brief 一个 Pattern（纯 RAM 结构，不直接落盘） */
typedef struct {
    uint8_t step_count;      /*!< 16 或 32，每 pattern 独立；内存+文件，不落 NVS */
    seq_track_t tracks[SEQ_TRACK_NUM];
} seq_pattern_t;

/** @brief 一个 RAM 槽（与 SD 文件无绑定） */
typedef struct {
    seq_pattern_t pattern;
    bool has_content;        /*!< load/randomize/任一编辑置 true；init 清 false */
} seq_slot_t;

/**
 * @brief 初始化引擎：6 个空槽、清运行时状态（不自动读 SD）
 */
void engine_seq_init(void);

/**
 * @brief 反初始化
 */
void engine_seq_deinit(void);

/**
 * @brief 注入随机种子（上层以 esp_random() 播种，保持引擎无 OS 依赖）
 */
void engine_seq_seed(uint32_t seed);

/* -------------------- 播放控制 -------------------- */

void engine_seq_start(void);
void engine_seq_stop(void);
bool engine_seq_is_playing(void);
uint8_t engine_seq_get_current_step(void);

/**
 * @brief 时基注入点：每 tick 调用一次（上层经 service_timer 周期驱动）
 */
void engine_seq_tick_hook(void *arg);

/* -------------------- 全局参数 -------------------- */

void engine_seq_set_bpm(uint16_t bpm);
uint16_t engine_seq_get_bpm(void);
void engine_seq_set_swing(uint8_t swing);
uint8_t engine_seq_get_swing(void);

/**
 * @brief 切换当前槽步数模式（16/32 双模式，每 pattern 独立）
 * @param steps 仅接受 SEQ_STEP_NUM(16)/SEQ_STEP_MAX(32)，其他值忽略
 * @note  不丢步数据：32→16 仅收窄逻辑上限，B 页数据保留，再激活可恢复（无损往返）。
 *        播放中收缩且播放头已越界时播放头归零，防越界步继续触发保留数据。
 *        不落 NVS：仅存内存 + M5P 文件，新 pattern/重启默认 16。
 */
void engine_seq_set_step_count(uint8_t steps);

/**
 * @brief 读当前槽步数（16 或 32）；无当前槽时返回 SEQ_STEP_NUM
 */
uint8_t engine_seq_get_step_count(void);

/* -------------------- 编辑 -------------------- */

/**
 * @brief 写轨道全局参数
 * @param track 轨道索引 0-7
 * @param param 0=rand_temp, 1=velocity, 2=probability, 3=mute, 4=solo（value 0/1）, 5=midi_note
 */
void engine_seq_track_set_param(uint8_t track, uint8_t param, uint8_t value);

/**
 * @brief 系统默认鼓 note（每轨常量表默认值，供 restore/ListView 回填）
 */
uint8_t engine_seq_default_note(uint8_t track);

/**
 * @brief 读指定槽轨道 note（UI 回填 dropdown / 轨道名用）
 */
uint8_t engine_seq_track_get_note(int8_t slot, uint8_t track);

/**
 * @brief 对所有槽应用轨道偏好（NVS 恢复用）
 * @param note 0=保持系统默认；1-127=GM 鼓 note（含 57+）
 * @param rand_temp/velocity/probability 直接写所有槽该轨
 */
void engine_seq_apply_track_prefs(uint8_t track, uint8_t note,
                                  uint8_t rand_temp, uint8_t velocity,
                                  uint8_t probability);

void engine_seq_step_toggle(uint8_t track, uint8_t step);

/**
 * @brief 设置单步覆盖（锁）
 * @param vel 0=清力度覆盖；1-127=覆盖
 * @param prob 255=清概率覆盖；0-100=覆盖
 */
void engine_seq_step_set_lock(uint8_t track, uint8_t step,
                              uint8_t vel, uint8_t prob,
                              uint8_t cc_num, uint8_t cc_val);

void engine_seq_step_clear_lock(uint8_t track, uint8_t step);

/**
 * @brief 骰子：按 §6 欧几里得随机生成当前槽全部轨道
 */
void engine_seq_randomize_all(void);

/**
 * @brief 清空当前槽指定步区间的 active（不动单步锁定）
 * @param first 起始步（含）；count 步数；越界自动截断到 SEQ_STEP_MAX
 * @note  清空按钮语义=清当前页：app 以 (page*SEQ_STEP_NUM, SEQ_STEP_NUM) 调用
 */
void engine_seq_clear_steps(uint8_t first, uint8_t count);

/* -------------------- 槽位 -------------------- */

/**
 * @brief 请求切换当前槽（§8）
 * @param slot 槽索引 0-5；-1 忽略
 * @note 播放中 → pending（小节末生效）；停止 → 立即生效
 */
void engine_seq_pattern_request(int8_t slot);

int8_t engine_seq_pattern_current(void);
int8_t engine_seq_pattern_pending(void);

/**
 * @brief 只读槽快照；slot 非法返回 NULL
 */
const seq_slot_t *engine_seq_slot_get(int8_t slot);

/**
 * @brief 槽是否为当前播放槽
 */
bool engine_seq_slot_is_checked(int8_t slot);

/**
 * @brief 槽是否为空（无任何 active 步骤；单步锁定/轨道参数不影响判断）
 * @return true=空白（8 轨 × 16 步全 inactive）
 */
bool engine_seq_slot_is_empty(int8_t slot);

/* -------------------- M5P 序列化 -------------------- */

/**
 * @brief 序列化槽到 M5P 缓冲区（§3 布局，逐字段小端）
 * @return 写入字节数；slot 非法/缓冲不足返回 0
 */
int engine_seq_serialize(int8_t slot, uint8_t *buf, size_t cap);

/**
 * @brief 反序列化并装入槽；magic/minor/CRC 任一失败返回 false（不改槽）
 * @note 目标槽==当前播放槽且播放中：内容先落内部暂存，15→0 回卷时量化生效
 *       （engine_seq_stop 时立即生效）；其余情况直接写入。
 */
bool engine_seq_deserialize(int8_t slot, const uint8_t *buf, size_t len);

/**
 * @brief 仅校验 M5P（magic/minor/CRC），供文件列表过滤；不修改任何槽
 */
bool engine_seq_m5p_validate(const uint8_t *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* ENGINE_SEQUENCER_H */
