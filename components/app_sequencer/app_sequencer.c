/**
 * @file app_sequencer.c
 * @brief 音序器 App：主界面交互（C4a）
 *
 * 轨道行选中↔取消、Mute/Solo、三滑块写引擎、BPM 半区点按+拖动、Play/Stop
 * 注册注销 service_timer hook、骰子随机、状态串助手。网格直绘/槽位文件为
 * C4b/C4c 内容。
 *
 * Trap: LVGL 事件回调跑在 task_gui，不得直接触碰 engine 播放/解析状态；
 * 统一登记到 SPSC 请求环，由 on_update（task_app，lifecycle 锁内）串行消化。
 */

#include "app_sequencer.h"
#include "app_manager.h"
#include "engine_gui.h"
#include "engine_midi.h"
#include "engine_sequencer.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_lvgl_port.h"
#include "service_i18n.h"
#include "service_nvs.h"
#include "service_sd.h"
#include "service_timer.h"
#include "lvgl.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static const char *TAG = "app_sequencer";

#define SEQ_TRACK_COUNT   8
#define SEQ_PATTERN_SLOTS 6
#define SEQ_BPM_MIN       20
#define SEQ_BPM_MAX       300

/* BPM 拖动：每 4px ±1；4px 以下视为点按（半区 ±1） */
#define SEQ_BPM_DRAG_PX   4

/* -------------------- 控件绑定 -------------------- */

typedef struct {
    lv_obj_t *btn_home;
    lv_obj_t *btn_rec;
    lv_obj_t *btn_random;
    lv_obj_t *btn_play_stop;
    lv_obj_t *label_play_stop;   /* play/stop 按钮内子 label（EEZ 未命名，运行时缓存） */
    lv_obj_t *label_bpm_slide;
    lv_obj_t *panel_track;
    lv_obj_t *track[SEQ_TRACK_COUNT];
    lv_obj_t *track_state[SEQ_TRACK_COUNT];
    lv_obj_t *panel_pattern;
    lv_obj_t *pattern_str;
    lv_obj_t *pattern_save_selected;
    lv_obj_t *pattern_load_selected;
    lv_obj_t *pattern_slot[SEQ_PATTERN_SLOTS];
    lv_obj_t *panel_track_param;
    lv_obj_t *track_btn_mute;
    lv_obj_t *track_btn_solo;
    lv_obj_t *track_btn_lock;
    lv_obj_t *track_dropdown;
    lv_obj_t *track_btn_restore;
    lv_obj_t *track_sld_randtemp;
    lv_obj_t *track_sld_velocity;
    lv_obj_t *track_sld_probability;
    lv_obj_t *track_name;
    lv_obj_t *grid_track_name;
    lv_obj_t *btn_clear_grid;
    lv_obj_t *btn_grid_page_switch;   /* 16/32 模式 + A/B 页切换 */
    lv_obj_t *grid_step_name;         /* 步号指示 01..16 / 17..32 */
    lv_obj_t *grid;
    lv_obj_t *pattern_list;
    lv_obj_t *list_file_return;
    lv_obj_t *list_file_delect;
    lv_obj_t *list_file_select;
    lv_obj_t *list_file;
    lv_obj_t *panel_grid_set;
    lv_obj_t *grid_set_return;
    lv_obj_t *grid_set_cancel;
    lv_obj_t *grid_set_confirm;
    lv_obj_t *grid_set_velocity;
    lv_obj_t *grid_set_probability;
    lv_obj_t *grid_set_cc_value;
    lv_obj_t *grid_set_cc_select;
    lv_obj_t *grid_set_cc_value_display;
} ui_screen_sequencer_t;

static ui_screen_sequencer_t s_seq_ui = {0};

static const widget_binding_t s_seq_bindings[] = {
    WIDGET_BIND(ui_screen_sequencer_t, btn_home, "seq_btn_home", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, btn_rec, "seq_btn_rec", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, btn_random, "seq_btn_random", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, btn_play_stop, "seq_btn_play_stop", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, label_bpm_slide, "seq_label_bpm_slide", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_sequencer_t, panel_track, "seq_panel_track", WIDGET_KIND_PANEL),
    WIDGET_BIND(ui_screen_sequencer_t, track[0], "seq_track_1", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, track[1], "seq_track_2", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, track[2], "seq_track_3", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, track[3], "seq_track_4", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, track[4], "seq_track_5", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, track[5], "seq_track_6", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, track[6], "seq_track_7", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, track[7], "seq_track_8", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, track_state[0], "seq_track_1_state", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, track_state[1], "seq_track_2_state", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, track_state[2], "seq_track_3_state", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, track_state[3], "seq_track_4_state", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, track_state[4], "seq_track_5_state", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, track_state[5], "seq_track_6_state", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, track_state[6], "seq_track_7_state", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, track_state[7], "seq_track_8_state", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, panel_pattern, "seq_panel_pattern", WIDGET_KIND_PANEL),
    WIDGET_BIND(ui_screen_sequencer_t, pattern_str, "seq_pattern_str", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_sequencer_t, pattern_save_selected, "seq_pattern_save_selected", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, pattern_load_selected, "seq_pattern_load_selected", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, pattern_slot[0], "seq_pattern_a", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, pattern_slot[1], "seq_pattern_b", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, pattern_slot[2], "seq_pattern_c", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, pattern_slot[3], "seq_pattern_d", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, pattern_slot[4], "seq_pattern_e", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, pattern_slot[5], "seq_pattern_f", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, panel_track_param, "seq_panel_track_param", WIDGET_KIND_PANEL),
    WIDGET_BIND(ui_screen_sequencer_t, track_btn_lock, "seq_track_btn_lock", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, track_dropdown, "seq_track_dropdown", WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_sequencer_t, track_btn_restore, "seq_track_btn_restore", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, track_btn_mute, "seq_track_btn_mute", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, track_btn_solo, "seq_track_btn_solo", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, track_sld_randtemp, "seq_track_sld_randtemp", WIDGET_KIND_SLIDER),
    WIDGET_BIND(ui_screen_sequencer_t, track_sld_velocity, "seq_track_sld_velocity", WIDGET_KIND_SLIDER),
    WIDGET_BIND(ui_screen_sequencer_t, track_sld_probability, "seq_track_sld_probability", WIDGET_KIND_SLIDER),
    WIDGET_BIND(ui_screen_sequencer_t, track_name, "seq_track_name", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_sequencer_t, grid_track_name, "seq_grid_track_name", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_sequencer_t, btn_clear_grid, "seq_btn_clear_grid", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, btn_grid_page_switch, "seq_btn_grid_page_switch", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, grid_step_name, "seq_grid_step_name", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_sequencer_t, grid, "seq_grid", WIDGET_KIND_PANEL),
    WIDGET_BIND(ui_screen_sequencer_t, pattern_list, "seq_pattern_list", WIDGET_KIND_PANEL),
    WIDGET_BIND(ui_screen_sequencer_t, list_file_return, "seq_list_file_return", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, list_file_delect, "seq_list_file_delect", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, list_file_select, "seq_list_file_select", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, list_file, "seq_list_file", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, panel_grid_set, "seq_panel_grid_set", WIDGET_KIND_PANEL),
    WIDGET_BIND(ui_screen_sequencer_t, grid_set_return, "seq_grid_set_return", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, grid_set_cancel, "seq_grid_set_cancel", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, grid_set_confirm, "seq_grid_set_confirm", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_sequencer_t, grid_set_velocity, "seq_grid_set_velocity", WIDGET_KIND_SLIDER),
    WIDGET_BIND(ui_screen_sequencer_t, grid_set_probability, "seq_grid_set_probability", WIDGET_KIND_SLIDER),
    WIDGET_BIND(ui_screen_sequencer_t, grid_set_cc_value, "seq_grid_set_cc_value", WIDGET_KIND_SLIDER),
    WIDGET_BIND(ui_screen_sequencer_t, grid_set_cc_select, "seq_grid_set_cc_select", WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_sequencer_t, grid_set_cc_value_display, "seq_grid_set_cc_value_display", WIDGET_KIND_LABEL),
    WIDGET_BINDING_END,
};

/* -------------------- 状态 -------------------- */

typedef struct {
    uint16_t bpm;
    uint8_t swing;
    int8_t selected_track;      /* -1=无选中 */
    bool playing;
    service_timer_handle_t timer;
    bool recording_self;
    bool recording_stop_pending;
    /* 状态串超时回退 */
    char pattern_str[64];
    uint32_t pattern_str_until_ms;
    /* 网格播放头 */
    uint8_t play_step;
    uint8_t play_step_prev;
    bool play_step_valid;
    /* 页模式（16/32 双模式）：grid_page 当前页 0=A(0-15)/1=B(16-31)；
     * step_count 为引擎当前槽步数的缓存（引擎为真源，on_update 轮询比对同步 UI） */
    uint8_t grid_page;
    uint8_t step_count;
    int8_t slot_cache;          /* 当前槽缓存：回卷量化切槽/装载生效的 UI 同步检测 */
    /* 槽位与文件（C4c） */
    int8_t file_target_slot;    /* 文件目标槽（边框5），-1=无 */
    uint32_t slot_pending_flash_ms;   /* pending 闪烁节拍 */
    bool slot_flash_on;
    /* 文件列表缓存 */
    char file_names[12][48];
    int file_count;
    int file_sel;               /* 面板高亮索引，-1=无 */
    bool pattern_panel_open;
} seq_state_t;

static seq_state_t s_seq = {0};

/* -------------------- 网格几何与快照（C4b） -------------------- */

#define SEQ_GRID_GAP       5      /* 格子间隙 4-6 */
#define SEQ_GRID_OPA_MIN   64     /* 有效概率透明度下限 64/255 */

/* 轨道绘制色（主题槽位；与 EEZ 行标签顺序一致） */
static const uint8_t s_seq_track_color[SEQ_TRACK_COUNT] = {
    COLOR_M1_PERCEIVE, COLOR_M2_DEFINE, COLOR_M3_BUILD, COLOR_M4_PERFORM,
    COLOR_PRIMARY, COLOR_SECONDARY, COLOR_SUCCESS, COLOR_ERROR,
};

/* -------------------- 鼓 note 表（GM 打击乐，dropdown 显示名 + 4 字符缩写） --------------------
 * 顺序与 EEZ 下拉 seq_track_dropdown 的选项一一对应（23 项）：
 *   None + 35~53 + 55~57（跳过 54 Tambourine / 58 Vibraslap，与 EEZ 工程一致）
 * 缩写用于左侧行标签 seq_track_name 与网格左标签 seq_grid_track_name，
 * 全部 ASCII 等宽、最长 4 字符。 */
#define SEQ_DRUM_COUNT 23   /* None(0) + 22 个 GM 鼓 */

static const uint8_t s_drum_note[SEQ_DRUM_COUNT] = {
    0,   /* ----- */
    35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 55, 56, 57,
};

static const char *const s_drum_short[SEQ_DRUM_COUNT] = {
    "----", /* 0  None */
    "ABD2", /* 35 Acoustic Bass Drum 2 */
    "KICK", /* 36 Bass Drum 1 */
    "SDST", /* 37 Side Stick */
    "SNAR", /* 38 Acoustic Snare */
    "CLAP", /* 39 Hand Clap */
    "ESNR", /* 40 Electric Snare */
    "LFTM", /* 41 Low Floor Tom */
    "CHHH", /* 42 Closed Hi-Hat */
    "HFTM", /* 43 High Floor Tom */
    "PHHH", /* 44 Pedal Hi-Hat */
    "L-TM", /* 45 Low Tom */
    "OHHH", /* 46 Open Hi-Hat */
    "LMTM", /* 47 Low-Mid Tom */
    "HMTM", /* 48 Hi-Mid Tom */
    "CRSH", /* 49 Crash Cymbal 1 */
    "H-TM", /* 50 High Tom */
    "RIDE", /* 51 Ride Cymbal 1 */
    "CHIN", /* 52 Chinese Cymbal */
    "RDBL", /* 53 Ride Bell */
    "SPLS", /* 55 Splash Cymbal */
    "COWB", /* 56 Cowbell */
    "CRS2", /* 57 Crash Cymbal 2 */
};

/* note 值 → dropdown 索引；无匹配返回 -1（如 54 Tambourine / 58 Vibraslap） */
static int seq_drum_index(uint8_t note)
{
    if (note == 0) {
        return 0;
    }
    for (int i = 1; i < SEQ_DRUM_COUNT; i++) {
        if (s_drum_note[i] == note) {
            return i;
        }
    }
    return -1;
}

/* 轨道显示名（缩写）；note 不在鼓表回退 "----" */
static const char *seq_track_display_name(uint8_t note)
{
    int idx = seq_drum_index(note);
    return (idx >= 0) ? s_drum_short[idx] : s_drum_short[0];
}

/* 同步左右两侧轨道名称：左侧 panel track_name + 网格左测 grid_track_name */
static void seq_sync_track_names(void)
{
    lvgl_port_lock(portMAX_DELAY);
    const seq_slot_t *slot = engine_seq_slot_get(engine_seq_pattern_current());
    if (slot == NULL) {
        lvgl_port_unlock();
        return;
    }

    /* 逐行换行，每轨一行缩写 */
    char buf[64];
    int pos = 0;
    for (int t = 0; t < SEQ_TRACK_COUNT; t++) {
        const char *name = seq_track_display_name(slot->pattern.tracks[t].midi_note);
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s\n", name);
    }
    if (pos > 0 && buf[pos - 1] == '\n') {
        buf[pos - 1] = '\0';
    }

    if (s_seq_ui.track_name != NULL) {
        lv_label_set_text(s_seq_ui.track_name, buf);
    }
    if (s_seq_ui.grid_track_name != NULL) {
        lv_label_set_text(s_seq_ui.grid_track_name, buf);
    }

    lvgl_port_unlock();
}

/* 回填 dropdown 为当前选中轨 note（仅当值不同，避免触发 VALUE_CHANGED 反馈） */
static void seq_sync_dropdown_note(void)
{
    if (s_seq.selected_track < 0 || s_seq_ui.track_dropdown == NULL) {
        return;
    }
    const seq_slot_t *slot = engine_seq_slot_get(engine_seq_pattern_current());
    if (slot == NULL) {
        return;
    }
    uint8_t cur_note = slot->pattern.tracks[s_seq.selected_track].midi_note;
    int idx = seq_drum_index(cur_note);
    if (idx >= 0) {
        lvgl_port_lock(portMAX_DELAY);
        uint32_t now_sel = lv_dropdown_get_selected(s_seq_ui.track_dropdown);
        if (now_sel != (uint32_t)idx) {
            lv_dropdown_set_selected(s_seq_ui.track_dropdown, (uint16_t)idx);
        }
        lvgl_port_unlock();
    }
}

/* 锁态 UI 同步：lock 按钮 CHECKED=锁定（default=未锁定，两态翻转，不用 DISABLE——
 * Trap: DISABLE 原生不可点，会导致解锁后无法加锁 / 重进后无法解锁，2026-08）。
 * 锁定 → dropdown 禁点（DISABLED）、restore 隐藏；未锁定 → dropdown 可切、restore 显示。
 * 所有入口（on_init/lock 点击/切轨/restore/装载/切槽）必须收敛到此，避免状态失配。 */
static void seq_apply_lock_ui(void)
{
    lvgl_port_lock(portMAX_DELAY);
    if (s_seq_ui.track_btn_lock == NULL || s_seq_ui.track_dropdown == NULL) {
        lvgl_port_unlock();
        return;
    }
    bool locked = lv_obj_has_state(s_seq_ui.track_btn_lock, LV_STATE_CHECKED);
    if (locked) {
        lv_obj_add_state(s_seq_ui.track_dropdown, LV_STATE_DISABLED);
        if (s_seq_ui.track_btn_restore != NULL) {
            lv_obj_add_flag(s_seq_ui.track_btn_restore, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        lv_obj_clear_state(s_seq_ui.track_dropdown, LV_STATE_DISABLED);
        if (s_seq_ui.track_btn_restore != NULL) {
            lv_obj_clear_flag(s_seq_ui.track_btn_restore, LV_OBJ_FLAG_HIDDEN);
        }
    }
    lvgl_port_unlock();
}

/* 同步参数面板 UI 状态：下拉回填当前轨 note + 按锁态设 dropdown/restore */
static void seq_sync_param_panel_ui(void)
{
    if (s_seq.selected_track < 0) {
        return;
    }
    seq_sync_dropdown_note();
    seq_apply_lock_ui();
}

typedef struct {
    int32_t ox, oy;         /* 对象内可用区原点（预留 gutter 用） */
    int32_t w, h;           /* 可用区宽高 */
    int32_t stride_x;       /* 每列步长 */
    int32_t stride_y;       /* 每行步长 */
    int32_t cell_w;         /* 格宽 = stride_x - GAP */
    int32_t cell_h;
    bool valid;
} seq_grid_geo_t;

static seq_grid_geo_t s_grid_geo = {0};

/* 绘制快照：task_app 从 engine 拷贝，绘制回调（task_gui）只读，
 * 避免跨任务读 engine 复合结构撕裂 */
static seq_pattern_t s_grid_snap = {0};

/* 快照刷新：与 invalidate 同一 LVGL 锁内完成，保证渲染永远不会拿到"已失效区域
 * + 旧快照"的组合。
 * Trap: task_gui(prio10) 可抢占 task_app(prio4)，若 invalidate 与快照更新分处
 * 不同锁区段，绘制会在两者之间插入并画上旧数据，且该区域之后不再失效——格子
 * 永久残留/丢失（2026-09 真机：重进显示上次序列但无声、播放中偶发丢格）。
 * Trap: 仅 task_app 上下文可调（与 engine 写同上下文）；engine 侧唯一并发写是
 * tick 回卷的暂存换入，错位一帧且由 slot_cache 全量失效自愈，可接受。 */
static void seq_grid_snapshot_refresh(void)
{
    const seq_slot_t *slot = engine_seq_slot_get(engine_seq_pattern_current());
    if (slot != NULL) {
        memcpy(&s_grid_snap, &slot->pattern, sizeof(seq_pattern_t));
    }
}

/* Grid Set 弹窗编辑目标 */
static int8_t s_grid_set_track = -1;
static int8_t s_grid_set_step = -1;

/* 轨道常量色语义（§9.5）：active 格用轨道色 × 有效概率透明度 */

static void seq_grid_cache_geometry(void)
{
    lv_obj_t *obj = s_seq_ui.grid;
    if (obj == NULL) {
        return;
    }
    int32_t w = lv_obj_get_width(obj);
    int32_t h = lv_obj_get_height(obj);
    /* 居中偏移：格子矩阵总占宽 = 16*stride - GAP，两侧各留 GAP/2 + 2 使视觉居中且不贴边 */
    s_grid_geo.ox = SEQ_GRID_GAP / 2 + 2;
    s_grid_geo.oy = SEQ_GRID_GAP / 2 + 2;
    s_grid_geo.w = w;
    s_grid_geo.h = h;
    s_grid_geo.stride_x = w / SEQ_STEP_NUM;
    s_grid_geo.stride_y = h / SEQ_TRACK_NUM;
    s_grid_geo.cell_w = s_grid_geo.stride_x - SEQ_GRID_GAP;
    s_grid_geo.cell_h = s_grid_geo.stride_y - SEQ_GRID_GAP;
    s_grid_geo.valid = (s_grid_geo.stride_x > 0 && s_grid_geo.stride_y > 0);
}

static void seq_grid_size_changed_cb(lv_event_t *e)
{
    (void)e;
    seq_grid_cache_geometry();
}

/* 失效指定列（播放头：旧+新两列）。
 * Trap: lv_obj_invalidate_area 用绝对屏幕坐标；且跨任务（on_update task_app 调用）
 * 必须持 LVGL 锁，否则渲染中途 invalidate 触发 lv_inv_area 断言崩溃。 */
static void seq_grid_invalidate_col(int32_t col)
{
    lv_obj_t *obj = s_seq_ui.grid;
    if (obj == NULL || !s_grid_geo.valid || col < 0 || col >= SEQ_STEP_NUM) {
        return;
    }
    lv_area_t oa;
    lv_obj_get_coords(obj, &oa);
    lv_area_t a = {
        .x1 = oa.x1 + s_grid_geo.ox + col * s_grid_geo.stride_x,
        .y1 = oa.y1 + s_grid_geo.oy,
        .x2 = oa.x1 + s_grid_geo.ox + (col + 1) * s_grid_geo.stride_x - 1,
        .y2 = oa.y1 + s_grid_geo.oy + s_grid_geo.h - 1,
    };
    lvgl_port_lock(portMAX_DELAY);
    seq_grid_snapshot_refresh();   /* 快照与失效同锁，渲染不会读到旧快照 */
    lv_obj_invalidate_area(obj, &a);
    lvgl_port_unlock();
}

static void seq_grid_invalidate_cell(int32_t row, int32_t col)
{
    lv_obj_t *obj = s_seq_ui.grid;
    if (obj == NULL || !s_grid_geo.valid || row < 0 || row >= SEQ_TRACK_NUM ||
        col < 0 || col >= SEQ_STEP_NUM) {
        return;
    }
    lv_area_t oa;
    lv_obj_get_coords(obj, &oa);
    lv_area_t a = {
        .x1 = oa.x1 + s_grid_geo.ox + col * s_grid_geo.stride_x,
        .y1 = oa.y1 + s_grid_geo.oy + row * s_grid_geo.stride_y,
        .x2 = oa.x1 + s_grid_geo.ox + col * s_grid_geo.stride_x + s_grid_geo.cell_w,
        .y2 = oa.y1 + s_grid_geo.oy + row * s_grid_geo.stride_y + s_grid_geo.cell_h,
    };
    lvgl_port_lock(portMAX_DELAY);
    seq_grid_snapshot_refresh();   /* 快照与失效同锁 */
    lv_obj_invalidate_area(obj, &a);
    lvgl_port_unlock();
}

/* 绘制回调（task_gui，LVGL 锁内）：只读 s_grid_snap，不写不分配不加锁。
 * Trap: DRAW 事件坐标为绝对屏幕坐标（obj->coords 基准），非对象本地坐标。 */
static void seq_grid_draw_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_current_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    if (!s_grid_geo.valid || obj == NULL) {
        return;
    }

    lv_area_t oa;
    lv_obj_get_coords(obj, &oa);
    int32_t ox = oa.x1;
    int32_t oy = oa.y1;

    lv_color_t bg = engine_gui_theme_color(COLOR_BG_PRIMARY);
    /* 页偏移：引擎步 = 页*16 + 列；播放头仅当落当前页时绘制 */
    int step_base = (int)s_seq.grid_page * SEQ_STEP_NUM;
    int32_t play_col = -1;
    if (s_seq.playing && s_seq.play_step_valid) {
        int rel = (int)s_seq.play_step - step_base;
        if (rel >= 0 && rel < SEQ_STEP_NUM) {
            play_col = rel;
        }
    }

    /* mute/solo 灰化（与轨道行同步）：mute 行或有 solo 时的非 solo 行，格色改文字副色 */
    bool any_solo = false;
    for (int r = 0; r < SEQ_TRACK_NUM; r++) {
        if (s_grid_snap.tracks[r].solo) {
            any_solo = true;
            break;
        }
    }

    for (int r = 0; r < SEQ_TRACK_NUM; r++) {
        const seq_track_t *tr = &s_grid_snap.tracks[r];
        bool row_gray = tr->mute || (any_solo && !tr->solo);
        lv_color_t row_color = engine_gui_theme_color(
            row_gray ? (uint8_t)COLOR_TEXT_SECONDARY : s_seq_track_color[r]);
        for (int c = 0; c < SEQ_STEP_NUM; c++) {
            const seq_step_t *st = &tr->steps[step_base + c];

            int32_t x = ox + s_grid_geo.ox + c * s_grid_geo.stride_x;
            int32_t y = oy + s_grid_geo.oy + r * s_grid_geo.stride_y;

            lv_draw_rect_dsc_t dsc;
            lv_draw_rect_dsc_init(&dsc);
            dsc.base.layer = layer;

            if (!st->active) {
                dsc.bg_color = bg;
                dsc.bg_opa = LV_OPA_COVER;
                dsc.radius = 4;
                lv_area_t area = { x, y, x + s_grid_geo.cell_w, y + s_grid_geo.cell_h };
                lv_draw_rect(layer, &dsc, &area);
                continue;
            }

            /* active：轨道色 × 有效概率透明度（下限 64/255）；mute/非 solo 行灰化 */
            int prob = (st->probability == 255) ? tr->probability : st->probability;
            int opa = prob * 255 / 100;
            if (opa < SEQ_GRID_OPA_MIN) opa = SEQ_GRID_OPA_MIN;
            dsc.bg_color = row_color;
            dsc.bg_opa = (lv_opa_t)opa;
            dsc.radius = 4;
            lv_area_t area = { x, y, x + s_grid_geo.cell_w, y + s_grid_geo.cell_h };
            lv_draw_rect(layer, &dsc, &area);

            /* velocity 覆盖：右上白色小圆点 */
            if (st->velocity != 0) {
                lv_draw_rect_dsc_t dot;
                lv_draw_rect_dsc_init(&dot);
                dot.base.layer = layer;
                dot.bg_color = lv_color_white();
                dot.bg_opa = LV_OPA_COVER;
                dot.radius = 3;
                int ds = 6;
                lv_area_t da = {
                    x + s_grid_geo.cell_w - ds - 2, y + 2,
                    x + s_grid_geo.cell_w - 2, y + 2 + ds,
                };
                lv_draw_rect(layer, &dot, &da);
            }

            /* cc 锁：右下锁标记（实心小方块 + 空槽） */
            if (st->cc_num != 0) {
                lv_draw_rect_dsc_t lock;
                lv_draw_rect_dsc_init(&lock);
                lock.base.layer = layer;
                lock.bg_color = lv_color_white();
                lock.bg_opa = LV_OPA_COVER;
                int ls = 5;
                lv_area_t la = {
                    x + s_grid_geo.cell_w - ls - 2, y + s_grid_geo.cell_h - ls - 2,
                    x + s_grid_geo.cell_w - 2, y + s_grid_geo.cell_h - 2,
                };
                lv_draw_rect(layer, &lock, &la);
            }
        }
    }

    /* 当前列半透明高亮竖条（shadow 色，亮/暗主题均可见） */
    if (play_col >= 0 && play_col < SEQ_STEP_NUM) {
        lv_draw_rect_dsc_t hl;
        lv_draw_rect_dsc_init(&hl);
        hl.base.layer = layer;
        hl.bg_color = engine_gui_theme_color(COLOR_SHADOW);
        hl.bg_opa = 110;
        lv_area_t ha = {
            ox + s_grid_geo.ox + play_col * s_grid_geo.stride_x,
            oy + s_grid_geo.oy,
            ox + s_grid_geo.ox + play_col * s_grid_geo.stride_x + s_grid_geo.stride_x,
            oy + s_grid_geo.oy + s_grid_geo.h,
        };
        lv_draw_rect(layer, &hl, &ha);
    }
}

/* 网格命中：屏幕坐标 → (row,col)；间隙归属左上格，无死区；返回 true 命中 */
static bool seq_grid_hit(int16_t sx, int16_t sy, int32_t *row, int32_t *col)
{
    lv_obj_t *obj = s_seq_ui.grid;
    if (obj == NULL || !s_grid_geo.valid) {
        return false;
    }
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    int32_t lx = sx - a.x1;
    int32_t ly = sy - a.y1;
    if (lx < s_grid_geo.ox || ly < s_grid_geo.oy) {
        return false;
    }
    int32_t c = (lx - s_grid_geo.ox) / s_grid_geo.stride_x;
    int32_t r = (ly - s_grid_geo.oy) / s_grid_geo.stride_y;
    if (c >= SEQ_STEP_NUM || r >= SEQ_TRACK_NUM) {
        return false;
    }
    *row = r;
    *col = c;
    return true;
}

/* -------------------- 请求环（LVGL 回调 → on_update 串行消化） -------------------- */

typedef enum {
    SEQ_REQ_NONE = 0,
    SEQ_REQ_TRACK_SELECT,   /* p1=track，再点已选中=取消 */
    SEQ_REQ_TRACK_PARAM,    /* p1=track p2=param p3=value */
    SEQ_REQ_MUTE,           /* p1=track p2=0/1 */
    SEQ_REQ_SOLO,           /* p1=track p2=0/1 */
    SEQ_REQ_BPM_SET,        /* p1=bpm */
    SEQ_REQ_PLAY,
    SEQ_REQ_STOP,
    SEQ_REQ_RANDOM,
    SEQ_REQ_GRID_TOGGLE,    /* p1=track p2=step */
    SEQ_REQ_GRID_SET_OPEN,  /* p1=track p2=step */
    SEQ_REQ_GRID_SET_CONFIRM, /* p1=track p2=step；vel/prob/cc 从弹窗控件读取 */
    SEQ_REQ_GRID_SET_CLEAR, /* p1=track p2=step */
    SEQ_REQ_GRID_SET_CLOSE, /* 放弃关闭 */
    SEQ_REQ_SLOT_TAP,       /* p1=slot：单击=仅移动文件目标 */
    SEQ_REQ_SLOT_HOLD,      /* p1=slot：长按=pattern_request（播放中量化/停止立即）+文件目标跟随 */
    SEQ_REQ_SAVE,           /* 序列化 file_target 槽 → 新 .m5p */
    SEQ_REQ_PANEL_OPEN,     /* 打开文件列表面板（每次重扫） */
    SEQ_REQ_FILE_SELECT,    /* ✓：装入 file_target 槽 */
    SEQ_REQ_FILE_DELETE,    /* 🗑：删除高亮文件 */
    SEQ_REQ_FILE_CLOSE,     /* <：关闭面板 */
    SEQ_REQ_FILE_SELECT_ITEM, /* p1=index：文件列表单选高亮 */
    SEQ_REQ_CLEAR_GRID,     /* 单击 clear 按钮：清空当前页（grid_page 指向的 16 步） */
    SEQ_REQ_TRACK_NOTE_RESTORE, /* p1=track：恢复系统默认 note */
    SEQ_REQ_PERSIST_PREFS,  /* lock 等低频动作的轨道偏好立即落盘（在 task_app 消化） */
    SEQ_REQ_STEP_COUNT_SET, /* p1=16/32：长按 page 按钮切换步数模式 */
    SEQ_REQ_PAGE_SWITCH,    /* 单击 page 按钮：A↔B 翻页（仅 32 步模式） */
} seq_req_action_t;

typedef struct {
    seq_req_action_t action;
    int p1, p2, p3;
} seq_req_t;

#define SEQ_REQ_RING_CAP 16
static seq_req_t s_req_ring[SEQ_REQ_RING_CAP];
static volatile uint8_t s_req_head = 0;   /* 消费者 on_update（task_app）写 */
static volatile uint8_t s_req_tail = 0;   /* 生产者 LVGL 回调（task_gui）写 */

static void seq_post_request(seq_req_action_t action, int p1, int p2, int p3)
{
    uint8_t tail = s_req_tail;
    uint8_t next = (uint8_t)((tail + 1) % SEQ_REQ_RING_CAP);
    if (next == s_req_head) {
        ESP_LOGW(TAG, "request ring full, drop action=%d", (int)action);
        return;
    }
    s_req_ring[tail].action = action;
    s_req_ring[tail].p1 = p1;
    s_req_ring[tail].p2 = p2;
    s_req_ring[tail].p3 = p3;
    __sync_synchronize();
    s_req_tail = next;
}

/* -------------------- 引擎同步（task_app 内） -------------------- */

static void seq_sync_bpm_label(void)
{
    lvgl_port_lock(portMAX_DELAY);
    if (s_seq_ui.label_bpm_slide == NULL) {
        lvgl_port_unlock();
        return;
    }
    char buf[48];
    snprintf(buf, sizeof(buf), "<      BPM: %d      >", (int)s_seq.bpm);
    lv_label_set_text(s_seq_ui.label_bpm_slide, buf);
    lvgl_port_unlock();
}

static void seq_sync_track_sliders(void)
{
    lvgl_port_lock(portMAX_DELAY);
    if (s_seq.selected_track < 0) {
        lvgl_port_unlock();
        return;
    }
    const seq_slot_t *slot = engine_seq_slot_get(engine_seq_pattern_current());
    if (slot == NULL) {
        lvgl_port_unlock();
        return;
    }
    const seq_track_t *tr = &slot->pattern.tracks[s_seq.selected_track];
    if (s_seq_ui.track_sld_randtemp != NULL) {
        lv_slider_set_value(s_seq_ui.track_sld_randtemp, tr->rand_temp, LV_ANIM_OFF);
    }
    if (s_seq_ui.track_sld_velocity != NULL) {
        lv_slider_set_value(s_seq_ui.track_sld_velocity, tr->velocity, LV_ANIM_OFF);
    }
    if (s_seq_ui.track_sld_probability != NULL) {
        lv_slider_set_value(s_seq_ui.track_sld_probability, tr->probability, LV_ANIM_OFF);
    }
    if (s_seq_ui.track_btn_mute != NULL) {
        if (tr->mute) {
            lv_obj_add_state(s_seq_ui.track_btn_mute, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(s_seq_ui.track_btn_mute, LV_STATE_CHECKED);
        }
    }
    if (s_seq_ui.track_btn_solo != NULL) {
        if (tr->solo) {
            lv_obj_add_state(s_seq_ui.track_btn_solo, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(s_seq_ui.track_btn_solo, LV_STATE_CHECKED);
        }
    }
    lvgl_port_unlock();
}

/* 行状态指示刷新（选中高亮 + M/S 文本："" 空 / "M" mute / "S" solo，solo 优先）
 * + mute/solo 灰化：mute 行灰底；任一轨 solo 时其余行灰底，仅 solo 行保留主题色。
 * 灰色取文字副色（COLOR_SHADOW 已被播放光标占用，用户指定 2026-09）。
 * Trap: 必须脏检查——无变化零 LVGL 写；否则每周期 ~20 次属性写逼 task_gui 持续
 * 重绘，LVGL 锁等待拉长 lifecycle 持锁，task_input 投喂被丢（2026-09 真机） */
static int8_t  s_rows_cache_sel = -2;
static uint8_t s_rows_cache_mute = 0xFF, s_rows_cache_solo = 0xFF;

static void seq_sync_track_rows(void)
{
    const seq_slot_t *slot = engine_seq_slot_get(engine_seq_pattern_current());
    uint8_t mute_mask = 0, solo_mask = 0;
    if (slot != NULL) {
        for (int t = 0; t < SEQ_TRACK_COUNT; t++) {
            if (slot->pattern.tracks[t].mute) {
                mute_mask |= (uint8_t)(1u << t);
            }
            if (slot->pattern.tracks[t].solo) {
                solo_mask |= (uint8_t)(1u << t);
            }
        }
    }
    if (s_rows_cache_sel == s_seq.selected_track &&
        s_rows_cache_mute == mute_mask && s_rows_cache_solo == solo_mask) {
        return;
    }
    s_rows_cache_sel = s_seq.selected_track;
    s_rows_cache_mute = mute_mask;
    s_rows_cache_solo = solo_mask;

    bool any_solo = (solo_mask != 0);
    lvgl_port_lock(portMAX_DELAY);
    lv_color_t col_bg = engine_gui_theme_color(COLOR_BG_SECONDARY);
    lv_color_t col_gray = engine_gui_theme_color(COLOR_TEXT_SECONDARY);
    for (int t = 0; t < SEQ_TRACK_COUNT; t++) {
        if (s_seq_ui.track[t] != NULL) {
            if (s_seq.selected_track == t) {
                lv_obj_add_state(s_seq_ui.track[t], LV_STATE_CHECKED);
            } else {
                lv_obj_clear_state(s_seq_ui.track[t], LV_STATE_CHECKED);
            }
            bool grayed = (mute_mask & (1u << t)) != 0 ||
                          (any_solo && (solo_mask & (1u << t)) == 0);
            lv_obj_set_style_bg_color(s_seq_ui.track[t], grayed ? col_gray : col_bg,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        if (s_seq_ui.track_state[t] != NULL) {
            lv_obj_t *st = s_seq_ui.track_state[t];
            lv_obj_clear_state(st, LV_STATE_CHECKED);
            lv_obj_clear_flag(st, LV_OBJ_FLAG_HIDDEN);
            const char *mark = "";
            if (solo_mask & (1u << t)) {
                mark = "S";
            } else if (mute_mask & (1u << t)) {
                mark = "M";
            }
            lv_label_set_text(st, mark);
        }
    }
    lvgl_port_unlock();
}

/* 面板互换：参数面板（选中轨）↔ Pattern 面板 */
static void seq_apply_panel(void)
{
    lvgl_port_lock(portMAX_DELAY);
    if (s_seq.selected_track >= 0) {
        if (s_seq_ui.panel_track_param != NULL) {
            lv_obj_clear_flag(s_seq_ui.panel_track_param, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_seq_ui.panel_pattern != NULL) {
            lv_obj_add_flag(s_seq_ui.panel_pattern, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        if (s_seq_ui.panel_track_param != NULL) {
            lv_obj_add_flag(s_seq_ui.panel_track_param, LV_OBJ_FLAG_HIDDEN);
        }
        if (s_seq_ui.panel_pattern != NULL) {
            lv_obj_clear_flag(s_seq_ui.panel_pattern, LV_OBJ_FLAG_HIDDEN);
        }
    }
    lvgl_port_unlock();
}

/* 状态串助手：显示后超时回退 Pattern */
static void seq_set_pattern_str(const char *text, uint32_t ms)
{
    lvgl_port_lock(portMAX_DELAY);
    if (s_seq_ui.pattern_str != NULL) {
        lv_label_set_text(s_seq_ui.pattern_str, text);
    }
    lvgl_port_unlock();
    snprintf(s_seq.pattern_str, sizeof(s_seq.pattern_str), "%s", text);
    s_seq.pattern_str_until_ms = lv_tick_get() + ms;
}

static void seq_status_fallback(void)
{
    if (s_seq.pattern_str_until_ms == 0) {
        return;
    }
    if ((int32_t)(lv_tick_get() - s_seq.pattern_str_until_ms) >= 0) {
        s_seq.pattern_str_until_ms = 0;
        lvgl_port_lock(portMAX_DELAY);
        if (s_seq_ui.pattern_str != NULL) {
            lv_label_set_text(s_seq_ui.pattern_str, _("Pattern"));
        }
        lvgl_port_unlock();
    }
}

/* -------------------- 页模式 UI（步号 + page 按钮色） -------------------- */

/* 步号指示：A 页=01..16，B 页=17..32（16 步模式 grid_page 恒 0，不显示 B 串） */
#define SEQ_STEP_NAME_A   "01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16"
#define SEQ_STEP_NAME_B   "17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32"

static void seq_sync_step_name(void)
{
    lvgl_port_lock(portMAX_DELAY);
    if (s_seq_ui.grid_step_name != NULL) {
        lv_label_set_text(s_seq_ui.grid_step_name,
                          (s_seq.grid_page == 0) ? SEQ_STEP_NAME_A : SEQ_STEP_NAME_B);
    }
    lvgl_port_unlock();
}

/* page 按钮文字色：16 步=shadow（无页可切态）；32 步=文字主色。
 * Trap: EEZ 按钮内含子 label 且自带样式，文字色须落到子 label（父样式被覆盖）。 */
static void seq_sync_page_btn(void)
{
    lvgl_port_lock(portMAX_DELAY);
    lv_obj_t *btn = s_seq_ui.btn_grid_page_switch;
    if (btn != NULL) {
        lv_color_t c = engine_gui_theme_color(
            (s_seq.step_count == SEQ_STEP_MAX) ? COLOR_TEXT_PRIMARY : COLOR_SHADOW);
        lv_obj_t *lbl = (lv_obj_get_child_cnt(btn) > 0) ? lv_obj_get_child(btn, 0) : NULL;
        lv_obj_set_style_text_color(lbl != NULL ? lbl : btn, c,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    lvgl_port_unlock();
}

static void seq_sync_page_ui(void)
{
    seq_sync_step_name();
    seq_sync_page_btn();
}

/* -------------------- 播放控制（engine 调用点） -------------------- */

static void seq_engine_tick(void *arg)
{
    (void)arg;
    engine_seq_tick_hook(NULL);
}

static void seq_start_play(void)
{
    if (s_seq.playing) {
        return;
    }
    uint64_t tick_us = 60000000ULL / s_seq.bpm / 32;
    service_timer_periodic_register(tick_us, seq_engine_tick, NULL, &s_seq.timer);
    engine_seq_start();
    s_seq.playing = true;
    lvgl_port_lock(portMAX_DELAY);
    if (s_seq_ui.btn_play_stop != NULL) {
        lv_obj_add_state(s_seq_ui.btn_play_stop, LV_STATE_CHECKED);
    }
    if (s_seq_ui.label_play_stop != NULL) {
        lv_label_set_text(s_seq_ui.label_play_stop, LV_SYMBOL_STOP);
    }
    lvgl_port_unlock();
    ESP_LOGI(TAG, "play bpm=%d", (int)s_seq.bpm);
}

static void seq_stop_play(void)
{
    if (!s_seq.playing) {
        return;
    }
    if (s_seq.timer != NULL) {
        service_timer_unregister(s_seq.timer);
        s_seq.timer = NULL;
    }
    engine_seq_stop();
    s_seq.playing = false;
    lvgl_port_lock(portMAX_DELAY);
    if (s_seq_ui.btn_play_stop != NULL) {
        lv_obj_clear_state(s_seq_ui.btn_play_stop, LV_STATE_CHECKED);
    }
    if (s_seq_ui.label_play_stop != NULL) {
        lv_label_set_text(s_seq_ui.label_play_stop, LV_SYMBOL_PLAY);
    }
    lvgl_port_unlock();
    ESP_LOGI(TAG, "stop");
}

/* 保存当前槽全部轨道偏好到 NVS。
 * Why: service_nvs_set_sequencer 只写内存+dirty，靠 task_app 周期 commit；
 * 若修改后立即锁定/退出/断电，1s 窗口未到会丢（2026-08）。
 * Trap: commit 即 flash 写（~10ms 级），滑块拖动的 VALUE_CHANGED 洪峰下逐次
 * commit 既磨损 flash 又堵 task_app——force=false 时限频（≤1 次/s），RAM 影子
 * 始终更新，最终值由 task_app 周期 commit 与 pause/destroy 的 force 落盘兜底。 */
static void seq_persist_track_prefs(bool force)
{
    service_nvs_sequencer_t pv;
    service_nvs_get_sequencer(&pv);
    pv.bpm = s_seq.bpm;
    pv.swing = s_seq.swing;
    const seq_slot_t *slot = engine_seq_slot_get(engine_seq_pattern_current());
    if (slot != NULL) {
        for (int t = 0; t < SEQ_TRACK_COUNT; t++) {
            const seq_track_t *tr = &slot->pattern.tracks[t];
            pv.track_note[t] = tr->midi_note;
            pv.track_rand_temp[t] = tr->rand_temp;
            pv.track_velocity[t] = tr->velocity;
            pv.track_probability[t] = tr->probability;
        }
    }
    service_nvs_set_sequencer(&pv);

    static uint32_t s_last_commit_ms = 0;
    uint32_t now = lv_tick_get();
    if (force || (uint32_t)(now - s_last_commit_ms) >= 1000) {
        service_nvs_commit();
        s_last_commit_ms = now;
    }
}

static void seq_set_bpm(int bpm)
{
    if (bpm < SEQ_BPM_MIN) bpm = SEQ_BPM_MIN;
    if (bpm > SEQ_BPM_MAX) bpm = SEQ_BPM_MAX;
    if (s_seq.bpm == (uint16_t)bpm) {
        return;
    }
    s_seq.bpm = (uint16_t)bpm;
    engine_seq_set_bpm(s_seq.bpm);
    /* 播放中同步 set_period */
    if (s_seq.playing && s_seq.timer != NULL) {
        uint64_t tick_us = 60000000ULL / s_seq.bpm / 32;
        service_timer_set_period(s_seq.timer, tick_us);
    }
    seq_sync_bpm_label();
    /* BPM 落 NVS：读-改-写，保留每轨偏好数组不被清零 */
    service_nvs_sequencer_t params;
    service_nvs_get_sequencer(&params);
    params.bpm = s_seq.bpm;
    params.swing = s_seq.swing;
    service_nvs_set_sequencer(&params);
}

static void seq_grid_invalidate_all_stub(void);   /* 前向声明：编辑/随机/装载后全量重绘 */

/* 全局步号（0..31）→ 当前页列失效；弹窗期间若自动跟随翻页导致该步不在当前页，全量失效兜底 */
static void seq_grid_invalidate_global_step(int32_t row, int32_t step)
{
    int32_t col = step - (int32_t)s_seq.grid_page * SEQ_STEP_NUM;
    if (col >= 0 && col < SEQ_STEP_NUM) {
        seq_grid_invalidate_cell(row, col);
    } else {
        seq_grid_invalidate_all_stub();
    }
}

static void seq_randomize(void)
{
    engine_seq_randomize_all();
    seq_grid_invalidate_all_stub();   /* 全量重绘：随机结果即时显示 */
    seq_set_pattern_str(_("Randomized"), 1500);
}

/* -------------------- Grid Set 弹窗（§9.6） -------------------- */

/* CC 下拉选项 → CC 号（与 EEZ 生成下拉一一对应：None/74/71/10/7/91） */
static const uint8_t s_cc_map[6] = { 0, 74, 71, 10, 7, 91 };
#define SEQ_CC_MAP_COUNT (sizeof(s_cc_map) / sizeof(s_cc_map[0]))

static void seq_grid_set_close(void)
{
    lvgl_port_lock(portMAX_DELAY);
    if (s_seq_ui.panel_grid_set != NULL) {
        lv_obj_add_flag(s_seq_ui.panel_grid_set, LV_OBJ_FLAG_HIDDEN);
    }
    lvgl_port_unlock();
    s_grid_set_track = -1;
    s_grid_set_step = -1;
}

static void app_sequencer_file_item_cb(lv_event_t *e);   /* 前向声明：列表条目回调 */

/* 打开弹窗并回填当前格值 */
static void seq_grid_set_open(int track, int step)
{
    const seq_slot_t *slot = engine_seq_slot_get(engine_seq_pattern_current());
    if (slot == NULL) {
        return;
    }
    const seq_step_t *st = &slot->pattern.tracks[track].steps[step];
    s_grid_set_track = (int8_t)track;
    s_grid_set_step = (int8_t)step;

    lvgl_port_lock(portMAX_DELAY);
    if (s_seq_ui.panel_grid_set != NULL) {
        lv_obj_clear_flag(s_seq_ui.panel_grid_set, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_seq_ui.grid_set_velocity != NULL) {
        lv_slider_set_value(s_seq_ui.grid_set_velocity, st->velocity ? st->velocity : 100, LV_ANIM_OFF);
    }
    if (s_seq_ui.grid_set_probability != NULL) {
        int prob = (st->probability == 255) ? 100 : st->probability;
        lv_slider_set_value(s_seq_ui.grid_set_probability, prob, LV_ANIM_OFF);
    }
    /* CC 下拉：回填 None/74/71/10/7/91 */
    if (s_seq_ui.grid_set_cc_select != NULL) {
        uint32_t sel = 0;
        for (uint32_t i = 0; i < SEQ_CC_MAP_COUNT; i++) {
            if (s_cc_map[i] == st->cc_num) {
                sel = i;
                break;
            }
        }
        lv_dropdown_set_selected(s_seq_ui.grid_set_cc_select, sel);
    }
    if (s_seq_ui.grid_set_cc_value != NULL) {
        lv_slider_set_value(s_seq_ui.grid_set_cc_value, st->cc_val ? st->cc_val : 100, LV_ANIM_OFF);
    }
    if (s_seq_ui.grid_set_cc_value_display != NULL) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", st->cc_val ? st->cc_val : 100);
        lv_label_set_text(s_seq_ui.grid_set_cc_value_display, buf);
    }
    lvgl_port_unlock();
}

static void seq_grid_set_confirm(void)
{
    if (s_grid_set_track < 0 || s_grid_set_step < 0) {
        seq_grid_set_close();
        return;
    }
    uint8_t vel = (s_seq_ui.grid_set_velocity != NULL) ?
                  (uint8_t)lv_slider_get_value(s_seq_ui.grid_set_velocity) : 100;
    uint8_t prob = (s_seq_ui.grid_set_probability != NULL) ?
                   (uint8_t)lv_slider_get_value(s_seq_ui.grid_set_probability) : 100;
    uint8_t cc_num = 0;
    uint8_t cc_val = 0;
    if (s_seq_ui.grid_set_cc_select != NULL) {
        uint32_t sel = lv_dropdown_get_selected(s_seq_ui.grid_set_cc_select);
        cc_num = (sel < SEQ_CC_MAP_COUNT) ? s_cc_map[sel] : 0;
    }
    if (cc_num != 0 && s_seq_ui.grid_set_cc_value != NULL) {
        cc_val = (uint8_t)lv_slider_get_value(s_seq_ui.grid_set_cc_value);
    }
    int32_t tr = s_grid_set_track;
    int32_t st = s_grid_set_step;
    engine_seq_step_set_lock((uint8_t)tr, (uint8_t)st, vel, prob, cc_num, cc_val);
    /* Trap: 必须先失效再 close——close 会清空 s_grid_set_track（置 -1），
     * 若失效在 close 之后传入 -1 被 row<0 检查吞掉，格子要等播放扫过才刷新 */
    seq_grid_invalidate_global_step(tr, st);
    seq_grid_set_close();
}

static void seq_grid_set_clear(void)
{
    if (s_grid_set_track < 0 || s_grid_set_step < 0) {
        seq_grid_set_close();
        return;
    }
    int32_t tr = s_grid_set_track;
    int32_t st = s_grid_set_step;
    engine_seq_step_clear_lock((uint8_t)tr, (uint8_t)st);
    /* 同 confirm：先失效再 close */
    seq_grid_invalidate_global_step(tr, st);
    seq_grid_set_close();
}

static void seq_grid_set_cc_value_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    lvgl_port_lock(portMAX_DELAY);
    if (s_seq_ui.grid_set_cc_value_display != NULL && s_seq_ui.grid_set_cc_value != NULL) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", (int)lv_slider_get_value(s_seq_ui.grid_set_cc_value));
        lv_label_set_text(s_seq_ui.grid_set_cc_value_display, buf);
    }
    lvgl_port_unlock();
}

/* 网格命中事件：SHORT_CLICKED=翻转格；LONG_PRESSED=开 Grid Set */
static void app_sequencer_grid_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_SHORT_CLICKED && code != LV_EVENT_LONG_PRESSED) {
        return;
    }
    lv_indev_t *indev = lv_indev_active();
    if (indev == NULL) {
        return;
    }
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    int32_t row, col;
    if (!seq_grid_hit((int16_t)p.x, (int16_t)p.y, &row, &col)) {
        return;
    }
    /* 命中列 → 引擎全局步（页偏移）：Grid Set 弹窗的 step 索引为引擎全局步 */
    int step = (int)col + (int)s_seq.grid_page * SEQ_STEP_NUM;
    if (code == LV_EVENT_SHORT_CLICKED) {
        seq_post_request(SEQ_REQ_GRID_TOGGLE, (int)row, step, 0);
    } else {
        seq_post_request(SEQ_REQ_GRID_SET_OPEN, (int)row, step, 0);
    }
}

static void app_sequencer_grid_confirm_cb(lv_event_t *e)
{
    (void)e;
    seq_post_request(SEQ_REQ_GRID_SET_CONFIRM, 0, 0, 0);
}

static void app_sequencer_grid_clear_cb(lv_event_t *e)
{
    (void)e;
    seq_post_request(SEQ_REQ_GRID_SET_CLEAR, 0, 0, 0);
}

static void app_sequencer_grid_cancel_cb(lv_event_t *e)
{
    (void)e;
    seq_post_request(SEQ_REQ_GRID_SET_CLOSE, 0, 0, 0);
}

/* -------------------- 槽位状态机与文件（§8/§10/§11） -------------------- */

#define SEQ_PATTERN_DIR     "/sdcard/sequencer"
#define SEQ_FILE_MAX        12
#define SEQ_M5P_BUF_SIZE    SEQ_M5P_TOTAL_SIZE

/* 保存按钮可用性：当前编辑槽空白时置 DISABLED（视觉置灰 + 阻断点击），
 * 有内容后恢复。脏检查：空白态不变则零 LVGL 写（同 seq_sync_track_rows 的 Trap） */
static int8_t s_save_btn_cache_empty = -1;

static void seq_sync_save_btn(void)
{
    bool empty = engine_seq_slot_is_empty(engine_seq_pattern_current());
    if (s_save_btn_cache_empty == (int8_t)empty) {
        return;
    }
    s_save_btn_cache_empty = (int8_t)empty;
    lvgl_port_lock(portMAX_DELAY);
    lv_obj_t *btn = s_seq_ui.pattern_save_selected;
    if (btn == NULL) {
        lvgl_port_unlock();
        return;
    }
    if (empty) {
        lv_obj_add_state(btn, LV_STATE_DISABLED);
    } else {
        lv_obj_clear_state(btn, LV_STATE_DISABLED);
    }
    lvgl_port_unlock();
}

/* 槽位视觉刷新（§8）：文字色 / CHECKED / 边框5 / pending 闪烁。
 * Trap: 脏检查同 seq_sync_track_rows——无变化零 LVGL 写，防周期写逼 task_gui
 * 持续重绘拉长 lifecycle 持锁（2026-09 触摸投喂丢失根因之一） */
static int8_t  s_slots_cache_checked = -2, s_slots_cache_pending = -2, s_slots_cache_file = -2;
static uint8_t s_slots_cache_content = 0xFF;
static bool    s_slots_cache_flash = false;

static void seq_sync_slots(void)
{
    /* pending 闪烁节拍 ~500ms（纯内存节拍，不进锁） */
    uint32_t now = lv_tick_get();
    if (now - s_seq.slot_pending_flash_ms >= 500) {
        s_seq.slot_pending_flash_ms = now;
        s_seq.slot_flash_on = !s_seq.slot_flash_on;
    }

    int8_t checked = engine_seq_pattern_current();
    int8_t pending = engine_seq_pattern_pending();
    uint8_t content_mask = 0;
    for (int i = 0; i < SEQ_PATTERN_SLOTS; i++) {
        const seq_slot_t *sl = engine_seq_slot_get((int8_t)i);
        if (sl != NULL && sl->has_content) {
            content_mask |= (uint8_t)(1u << i);
        }
    }
    /* 无 pending 时闪烁位不影响画面，不参与脏判定 */
    bool flash_vis = (pending >= 0) && s_seq.slot_flash_on;
    if (s_slots_cache_checked == checked && s_slots_cache_pending == pending &&
        s_slots_cache_file == s_seq.file_target_slot &&
        s_slots_cache_content == content_mask && s_slots_cache_flash == flash_vis) {
        return;
    }
    s_slots_cache_checked = checked;
    s_slots_cache_pending = pending;
    s_slots_cache_file = s_seq.file_target_slot;
    s_slots_cache_content = content_mask;
    s_slots_cache_flash = flash_vis;

    lvgl_port_lock(portMAX_DELAY);
    lv_color_t col_on = engine_gui_theme_color(COLOR_TEXT_PRIMARY);
    lv_color_t col_off = engine_gui_theme_color(COLOR_TEXT_SECONDARY);
    lv_color_t border = engine_gui_theme_color(COLOR_PRIMARY);

    for (int i = 0; i < SEQ_PATTERN_SLOTS; i++) {
        lv_obj_t *slot = s_seq_ui.pattern_slot[i];
        if (slot == NULL) {
            continue;
        }

        /* 1. 文字色：has_content==false→副色；true→主色 */
        lv_obj_set_style_text_color(slot, (content_mask & (1u << i)) ? col_on : col_off,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);

        /* 2. Checked：checked_slot 唯一 */
        if (checked == i) {
            lv_obj_add_state(slot, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(slot, LV_STATE_CHECKED);
        }

        /* 3. 边框宽5：file_target_slot 唯一 */
        if (s_seq.file_target_slot == i) {
            lv_obj_set_style_border_width(slot, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(slot, border, LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_obj_set_style_border_width(slot, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        /* 4. pending 闪烁：pending_slot 在播放中 → 边框闪烁，小节末转 Checked */
        if (pending == i) {
            lv_obj_set_style_border_width(slot, s_seq.slot_flash_on ? 5 : 0,
                                          LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
    lvgl_port_unlock();
}

/* 槽位 A~F 内文字以 A/B/C… 标记，由 EEZ 静态文本；此处仅刷边框/状态 */
static void seq_slot_tap(int slot)
{
    if (slot < 0 || slot >= SEQ_PATTERN_SLOTS) {
        return;
    }
    s_seq.file_target_slot = (int8_t)slot;   /* 单击=仅移动文件目标，不打断播放 */
}

static void seq_slot_hold(int slot)
{
    if (slot < 0 || slot >= SEQ_PATTERN_SLOTS) {
        return;
    }
    /* 长按=加入播放队列（播放中量化/停止中立即）；文件目标跟随 */
    engine_seq_pattern_request((int8_t)slot);
    s_seq.file_target_slot = (int8_t)slot;
}

/* 生成 M5P 文件名：<YYYYMMDD>_<HHMMSS>.m5p；时间无效用单调计数兜底；重名 _1/_2… */
static void seq_make_filename(char *buf, size_t len)
{
    struct tm tm;
    bool ok = (app_manager_get_time(&tm) == ESP_OK);
    if (ok) {
        snprintf(buf, len, "%04d%02d%02d_%02d%02d%02d.m5p",
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                 tm.tm_hour, tm.tm_min, tm.tm_sec);
    } else {
        /* 单调计数兜底 */
        snprintf(buf, len, "seq_%u.m5p", (unsigned)(esp_timer_get_time() / 1000));
    }

    /* 重名追加 _1/_2 */
    char base[64];
    snprintf(base, sizeof(base), "%s", buf);
    char path[128];
    snprintf(path, sizeof(path), SEQ_PATTERN_DIR "/%s", base);
    if (service_sd_file_exists(path + strlen("/sdcard"))) {
        for (int i = 1; i < 100; i++) {
            size_t stem = strlen(base) - 4;   /* 去掉 ".m5p" */
            snprintf(buf, len, "%.*s_%d.m5p", (int)stem, base, i);
            snprintf(path, sizeof(path), SEQ_PATTERN_DIR "/%s", buf);
            if (!service_sd_file_exists(path + strlen("/sdcard"))) {
                break;
            }
        }
    }
}

/* 保存：序列化当前编辑槽（checked slot）→ 新 .m5p 文件。
 * Trap: Load 需自由选目标槽（file_target_slot），Save 语义上是"保存正在编辑的
 * 槽"——即当前播放槽；空白槽拒绝保存（无 active 步骤的文件无意义）。 */
static void seq_save(void)
{
    if (!service_sd_is_mounted()) {
        seq_set_pattern_str(_("No SD card"), 5000);
        return;
    }

    int8_t cur = engine_seq_pattern_current();
    if (cur < 0) {
        return;
    }
    if (engine_seq_slot_is_empty(cur)) {
        seq_set_pattern_str(_("Pattern is empty"), 3000);
        return;
    }

    uint8_t buf[SEQ_M5P_BUF_SIZE];
    int n = engine_seq_serialize(cur, buf, sizeof(buf));
    if (n <= 0) {
        seq_set_pattern_str(_("Save failed"), 5000);
        return;
    }

    /* 确保目录存在 */
    struct stat st;
    if (stat(SEQ_PATTERN_DIR, &st) != 0 || !S_ISDIR(st.st_mode)) {
        mkdir(SEQ_PATTERN_DIR, 0755);
    }

    char fname[64];
    seq_make_filename(fname, sizeof(fname));
    char path[128];
    snprintf(path, sizeof(path), SEQ_PATTERN_DIR "/%s", fname);

    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        seq_set_pattern_str(_("Save failed"), 5000);
        return;
    }
    size_t wr = fwrite(buf, 1, (size_t)n, fp);
    fclose(fp);
    if (wr != (size_t)n) {
        remove(path);
        seq_set_pattern_str(_("Save failed"), 5000);
        return;
    }

    char msg[80];
    snprintf(msg, sizeof(msg), _("Saved: %s"), fname);
    seq_set_pattern_str(msg, 5000);
}

/* 扫描 /sdcard/sequencer/，仅收录合法 .m5p，文件名降序 */
static void seq_scan_files(void)
{
    s_seq.file_count = 0;
    s_seq.file_sel = -1;

    if (!service_sd_is_mounted()) {
        return;
    }
    DIR *dir = opendir(SEQ_PATTERN_DIR);
    if (dir == NULL) {
        return;
    }
    struct dirent *entry;
    while (s_seq.file_count < SEQ_FILE_MAX && (entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG) {
            continue;
        }
        const char *dot = strrchr(entry->d_name, '.');
        if (dot == NULL || strcasecmp(dot, ".m5p") != 0) {
            continue;
        }
        char path[128];
        snprintf(path, sizeof(path), SEQ_PATTERN_DIR "/%.47s", entry->d_name);
        FILE *fp = fopen(path, "rb");
        if (fp == NULL) {
            continue;
        }
        uint8_t hdr[SEQ_M5P_TOTAL_SIZE];
        size_t rd = fread(hdr, 1, sizeof(hdr), fp);
        fclose(fp);
        /* 变长格式：实际读取长度交由 validate 与头部 step_count 比对（含 CRC） */
        if (!engine_seq_m5p_validate(hdr, rd)) {
            continue;   /* 非法文件忽略 */
        }
        snprintf(s_seq.file_names[s_seq.file_count], sizeof(s_seq.file_names[0]),
                 "%.47s", entry->d_name);
        s_seq.file_count++;
    }
    closedir(dir);

    /* 文件名降序（新→旧） */
    for (int i = 0; i < s_seq.file_count - 1; i++) {
        for (int j = i + 1; j < s_seq.file_count; j++) {
            if (strcasecmp(s_seq.file_names[j], s_seq.file_names[i]) > 0) {
                char tmp[48];
                snprintf(tmp, sizeof(tmp), "%s", s_seq.file_names[i]);
                snprintf(s_seq.file_names[i], sizeof(s_seq.file_names[0]), "%s", s_seq.file_names[j]);
                snprintf(s_seq.file_names[j], sizeof(s_seq.file_names[0]), "%s", tmp);
            }
        }
    }
}

/* 重建文件列表 UI 条目 */
static void seq_populate_file_list(void)
{
    lv_obj_t *list = s_seq_ui.list_file;
    if (list == NULL) {
        return;
    }
    lvgl_port_lock(portMAX_DELAY);
    lv_obj_clean(list);

    if (s_seq.file_count == 0) {
        lv_obj_t *lbl = lv_label_create(list);
        lv_label_set_text(lbl, _("No patterns"));
    }

    for (int i = 0; i < s_seq.file_count; i++) {
        lv_obj_t *btn = lv_button_create(list);
        lv_obj_set_size(btn, LV_PCT(100), 46);
        lv_obj_set_style_bg_color(btn, engine_gui_theme_color(COLOR_CARD),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(btn, engine_gui_theme_color(COLOR_TEXT_PRIMARY),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, s_seq.file_names[i]);
        lv_obj_set_style_align(lbl, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        /* 单选高亮 */
        if (s_seq.file_sel == i) {
            lv_obj_add_state(btn, LV_STATE_CHECKED);
        }
        lv_obj_add_event_cb(btn, app_sequencer_file_item_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
    }
    lvgl_port_unlock();
}

static void app_sequencer_file_item_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    /* 单选高亮：登记请求，由 on_update 设置 s_seq.file_sel 并重建 */
    seq_post_request(SEQ_REQ_FILE_SELECT_ITEM, idx, 0, 0);
}

/* 打开文件面板：重扫 + 重建 */
static void seq_panel_open(void)
{
    seq_scan_files();
    seq_populate_file_list();
    lvgl_port_lock(portMAX_DELAY);
    if (s_seq_ui.pattern_list != NULL) {
        lv_obj_clear_flag(s_seq_ui.pattern_list, LV_OBJ_FLAG_HIDDEN);
    }
    lvgl_port_unlock();
    s_seq.pattern_panel_open = true;
}

/* ✓：装入 file_target 槽（若==checked 且播放中→量化生效） */
static void seq_file_select(void)
{
    if (s_seq.file_target_slot < 0) {
        seq_set_pattern_str(_("Select a slot first"), 3000);
        return;
    }
    if (s_seq.file_sel < 0 || s_seq.file_sel >= s_seq.file_count) {
        seq_set_pattern_str(_("Select a file first"), 3000);
        return;
    }
    if (!service_sd_is_mounted()) {
        seq_set_pattern_str(_("No SD card"), 5000);
        return;
    }

    char path[128];
    snprintf(path, sizeof(path), SEQ_PATTERN_DIR "/%s", s_seq.file_names[s_seq.file_sel]);
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        seq_set_pattern_str(_("Load failed"), 5000);
        return;
    }
    uint8_t buf[SEQ_M5P_BUF_SIZE];
    size_t rd = fread(buf, 1, sizeof(buf), fp);
    fclose(fp);
    if (!engine_seq_deserialize(s_seq.file_target_slot, buf, rd)) {
        seq_set_pattern_str(_("Invalid file"), 5000);
        return;
    }

    /* 目标槽==播放槽且播放中：引擎内部已暂存，回卷量化生效（无需额外请求） */

    char msg[80];
    snprintf(msg, sizeof(msg), _("Loaded: %s"), s_seq.file_names[s_seq.file_sel]);
    seq_set_pattern_str(msg, 3000);
    seq_grid_invalidate_all_stub();
    seq_sync_track_names();      /* 文件内 note 可能不同：轨道名同步 */
    seq_sync_track_sliders();
    seq_sync_param_panel_ui();
}

/* 🗑：删除高亮文件 */
static void seq_file_delete(void)
{
    if (s_seq.file_sel < 0 || s_seq.file_sel >= s_seq.file_count) {
        seq_set_pattern_str(_("Select a file first"), 3000);
        return;
    }
    char path[128];
    snprintf(path, sizeof(path), SEQ_PATTERN_DIR "/%s", s_seq.file_names[s_seq.file_sel]);
    if (remove(path) != 0) {
        seq_set_pattern_str(_("Delete failed"), 5000);
        return;
    }
    char msg[80];
    snprintf(msg, sizeof(msg), _("Deleted: %s"), s_seq.file_names[s_seq.file_sel]);
    seq_set_pattern_str(msg, 3000);
    seq_scan_files();
    seq_populate_file_list();
}

/* <：关闭面板 */
static void seq_file_close(void)
{
    lvgl_port_lock(portMAX_DELAY);
    if (s_seq_ui.pattern_list != NULL) {
        lv_obj_add_flag(s_seq_ui.pattern_list, LV_OBJ_FLAG_HIDDEN);
    }
    lvgl_port_unlock();
    s_seq.pattern_panel_open = false;
}

static void seq_grid_invalidate_all_stub(void)
{
    if (s_seq_ui.grid != NULL) {
        lvgl_port_lock(portMAX_DELAY);
        seq_grid_snapshot_refresh();   /* 快照与失效同锁 */
        lv_obj_invalidate(s_seq_ui.grid);
        lvgl_port_unlock();
    }
}

/* -------------------- 请求消化（on_update，task_app 锁内） -------------------- */

static void seq_drain_requests(void)
{
    while (s_req_head != s_req_tail) {
        seq_req_t req = s_req_ring[s_req_head];
        __sync_synchronize();
        s_req_head = (uint8_t)((s_req_head + 1) % SEQ_REQ_RING_CAP);
        switch (req.action) {
        case SEQ_REQ_TRACK_SELECT: {
            int t = req.p1;
            if (t < 0 || t >= SEQ_TRACK_COUNT) break;
            if (s_seq.selected_track == t) {
                s_seq.selected_track = -1;   /* 再点取消 */
            } else {
                s_seq.selected_track = (int8_t)t;
            }
            seq_apply_panel();
            seq_sync_track_rows();
            seq_sync_track_sliders();
            seq_sync_param_panel_ui();
            break;
        }
        case SEQ_REQ_TRACK_PARAM:
            if (req.p1 >= 0 && req.p1 < SEQ_TRACK_COUNT) {
                engine_seq_track_set_param((uint8_t)req.p1, (uint8_t)req.p2, (uint8_t)req.p3);
                if ((uint8_t)req.p2 == 5) {
                    /* note 变化：刷新两侧轨道名 + 网格（音色名同步） */
                    seq_sync_track_names();
                    seq_grid_invalidate_all_stub();
                }
                /* 改动即存：note（p2==5）低频立即落盘；滑块（0/1/2）限频防 flash 磨损 */
                seq_persist_track_prefs(req.p2 == 5);
            }
            break;
        case SEQ_REQ_TRACK_NOTE_RESTORE:
            if (req.p1 >= 0 && req.p1 < SEQ_TRACK_COUNT) {
                uint8_t def_note = engine_seq_default_note((uint8_t)req.p1);
                engine_seq_track_set_param((uint8_t)req.p1, 5, def_note);
                seq_sync_track_names();
                seq_grid_invalidate_all_stub();
                seq_sync_param_panel_ui();
                /* restore 也是修改：立即落盘 */
                seq_persist_track_prefs(true);
            }
            break;
        case SEQ_REQ_PERSIST_PREFS:
            /* lock 等低频动作：在 task_app 上下文落盘（flash 写不进 task_gui） */
            seq_persist_track_prefs(true);
            break;
        case SEQ_REQ_MUTE:
            if (req.p1 >= 0 && req.p1 < SEQ_TRACK_COUNT) {
                engine_seq_track_set_param((uint8_t)req.p1, 3, (uint8_t)(req.p2 ? 1 : 0));
                seq_sync_track_sliders();
                seq_grid_invalidate_all_stub();   /* mute 灰化波及整行格子 */
            }
            break;
        case SEQ_REQ_SOLO:
            if (req.p1 >= 0 && req.p1 < SEQ_TRACK_COUNT) {
                engine_seq_track_set_param((uint8_t)req.p1, 4, (uint8_t)(req.p2 ? 1 : 0));
                seq_sync_track_sliders();
                seq_grid_invalidate_all_stub();   /* solo 灰化波及所有行 */
            }
            break;
        case SEQ_REQ_BPM_SET:
            seq_set_bpm(req.p1);
            break;
        case SEQ_REQ_PLAY:
            seq_start_play();
            break;
        case SEQ_REQ_STOP:
            seq_stop_play();
            break;
        case SEQ_REQ_RANDOM:
            seq_randomize();
            break;
        case SEQ_REQ_GRID_TOGGLE:
            if (req.p1 >= 0 && req.p1 < SEQ_TRACK_NUM &&
                req.p2 >= 0 && req.p2 < SEQ_STEP_MAX) {
                engine_seq_step_toggle((uint8_t)req.p1, (uint8_t)req.p2);
                seq_grid_invalidate_global_step(req.p1, req.p2);
            }
            break;
        case SEQ_REQ_GRID_SET_OPEN:
            if (req.p1 >= 0 && req.p1 < SEQ_TRACK_NUM &&
                req.p2 >= 0 && req.p2 < SEQ_STEP_MAX) {
                seq_grid_set_open(req.p1, req.p2);
            }
            break;
        case SEQ_REQ_GRID_SET_CONFIRM:
            seq_grid_set_confirm();
            break;
        case SEQ_REQ_GRID_SET_CLEAR:
            seq_grid_set_clear();
            break;
        case SEQ_REQ_GRID_SET_CLOSE:
            seq_grid_set_close();
            break;
        case SEQ_REQ_SLOT_TAP:
            seq_slot_tap(req.p1);
            seq_sync_slots();
            break;
        case SEQ_REQ_SLOT_HOLD:
            seq_slot_hold(req.p1);
            seq_sync_slots();
            /* 槽位内容已切换：未播放时引擎立即生效，网格需全量失效，
             * 否则新槽的格子在自身未 invalidate 前保持旧画面 */
            seq_grid_invalidate_all_stub();
            seq_sync_track_rows();
            seq_sync_track_names();      /* 不同槽可能有不同 note：轨道名同步 */
            seq_sync_track_sliders();
            seq_sync_param_panel_ui();
            break;
        case SEQ_REQ_SAVE:
            seq_save();
            break;
        case SEQ_REQ_PANEL_OPEN:
            seq_panel_open();
            break;
        case SEQ_REQ_FILE_SELECT:
            seq_file_select();
            break;
        case SEQ_REQ_FILE_DELETE:
            seq_file_delete();
            break;
        case SEQ_REQ_FILE_CLOSE:
            seq_file_close();
            break;
        case SEQ_REQ_FILE_SELECT_ITEM:
            if (req.p1 >= 0 && req.p1 < s_seq.file_count) {
                s_seq.file_sel = req.p1;
                seq_populate_file_list();
            }
            break;
        case SEQ_REQ_CLEAR_GRID:
            /* 单击清空当前页：16 步=清 0-15；32 步=清当前页 16 步，另一页不动 */
            engine_seq_clear_steps((uint8_t)(s_seq.grid_page * SEQ_STEP_NUM), SEQ_STEP_NUM);
            seq_grid_invalidate_all_stub();
            seq_sync_slots();
            seq_set_pattern_str(_("Page cleared"), 1500);
            break;
        case SEQ_REQ_STEP_COUNT_SET:
            /* 引擎为真源：写入后由 on_update 的 step_count 缓存比对统一同步
             * 页号回退/步号/按钮色/全量失效（同路径覆盖切槽、装载量化生效） */
            engine_seq_set_step_count((uint8_t)req.p1);
            break;
        case SEQ_REQ_PAGE_SWITCH:
            if (s_seq.step_count == SEQ_STEP_MAX) {
                s_seq.grid_page ^= 1;
                seq_sync_step_name();
                seq_grid_invalidate_all_stub();
            }
            break;
        default:
            break;
        }
    }
}

/* -------------------- LVGL 回调（task_gui，仅登记请求） -------------------- */

static void app_sequencer_home_cb(lv_event_t *e)
{
    (void)e;
    app_manager_request_kill_active();
}

static void app_sequencer_rec_btn_cb(lv_event_t *e)
{
    app_base_t *self = (app_base_t *)lv_event_get_user_data(e);
    ui_screen_sequencer_t *ui = (ui_screen_sequencer_t *)self->screen_ctx;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    if (s_seq.recording_self) {
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
        s_seq.recording_self = true;
        s_seq.recording_stop_pending = false;
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

static void app_sequencer_track_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target_obj(e);
    for (int t = 0; t < SEQ_TRACK_COUNT; t++) {
        if (s_seq_ui.track[t] == target) {
            seq_post_request(SEQ_REQ_TRACK_SELECT, t, 0, 0);
            return;
        }
    }
}

static void app_sequencer_param_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    lv_obj_t *target = lv_event_get_target_obj(e);
    int track = s_seq.selected_track;
    if (track < 0) {
        return;
    }
    int val = (int)lv_slider_get_value(target);
    if (target == s_seq_ui.track_sld_randtemp) {
        seq_post_request(SEQ_REQ_TRACK_PARAM, track, 0, val);
    } else if (target == s_seq_ui.track_sld_velocity) {
        seq_post_request(SEQ_REQ_TRACK_PARAM, track, 1, val);
    } else if (target == s_seq_ui.track_sld_probability) {
        seq_post_request(SEQ_REQ_TRACK_PARAM, track, 2, val);
    }
}

static void app_sequencer_mute_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    int track = s_seq.selected_track;
    if (track < 0) {
        return;
    }
    bool state = lv_obj_has_state(s_seq_ui.track_btn_mute, LV_STATE_CHECKED);
    seq_post_request(SEQ_REQ_MUTE, track, state ? 0 : 1, 0);
}

static void app_sequencer_solo_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    int track = s_seq.selected_track;
    if (track < 0) {
        return;
    }
    bool state = lv_obj_has_state(s_seq_ui.track_btn_solo, LV_STATE_CHECKED);
    seq_post_request(SEQ_REQ_SOLO, track, state ? 0 : 1, 0);
}

/* lock 按钮：点击翻转 CHECKED（锁定↔解锁两态），dropdown/restore 由 seq_apply_lock_ui 同步。
 * 锁定即存：点击时把当前轨道偏好落盘（用户语义=配置定稿）。
 * 不用 DISABLE 表示解锁——原生 disable 不可点，会导致解锁后无法加锁（2026-08）。
 * Trap: LVGL 回调（task_gui）只翻转 UI 态；flash 写（nvs_commit ~10ms）不得进
 * task_gui，落盘登记 SEQ_REQ_PERSIST_PREFS 由 on_update（task_app）执行。 */
static void app_sequencer_lock_cb(lv_event_t *e)
{
    (void)e;
    lvgl_port_lock(portMAX_DELAY);
    if (s_seq_ui.track_btn_lock != NULL) {
        if (lv_obj_has_state(s_seq_ui.track_btn_lock, LV_STATE_CHECKED)) {
            lv_obj_clear_state(s_seq_ui.track_btn_lock, LV_STATE_CHECKED);   /* 解锁 */
        } else {
            lv_obj_add_state(s_seq_ui.track_btn_lock, LV_STATE_CHECKED);     /* 加锁 */
        }
    }
    lvgl_port_unlock();
    seq_apply_lock_ui();
    seq_post_request(SEQ_REQ_PERSIST_PREFS, 0, 0, 0);   /* 锁定即存 */
}

/* restore 按钮：恢复选中轨系统默认 note（引擎写入走请求环） */
static void app_sequencer_restore_cb(lv_event_t *e)
{
    (void)e;
    int track = s_seq.selected_track;
    if (track < 0) {
        return;
    }
    seq_post_request(SEQ_REQ_TRACK_NOTE_RESTORE, track, 0, 0);
}

/* dropdown 选择变化：切换选中轨 note（引擎写入走请求环） */
static void app_sequencer_dropdown_cb(lv_event_t *e)
{
    (void)e;
    int track = s_seq.selected_track;
    if (track < 0 || s_seq_ui.track_dropdown == NULL) {
        return;
    }
    uint16_t sel = lv_dropdown_get_selected(s_seq_ui.track_dropdown);
    if (sel >= SEQ_DRUM_COUNT) {
        return;
    }
    uint8_t note = s_drum_note[sel];

    /* 与引擎当前 note 相同时不重复登记（避免 set_selected 触发 VALUE_CHANGED 反馈环） */
    const seq_slot_t *slot = engine_seq_slot_get(engine_seq_pattern_current());
    if (slot != NULL && slot->pattern.tracks[track].midi_note == note) {
        return;
    }
    seq_post_request(SEQ_REQ_TRACK_PARAM, track, 5, note);
}

/* BPM label：左半点按 -1、右半 +1；按住拖动每 4px ±1 */
typedef struct {
    int press_x;
    int base_bpm;
    bool dragging;
} seq_bpm_drag_t;

static seq_bpm_drag_t s_bpm_drag = {0};

static void app_sequencer_bpm_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = s_seq_ui.label_bpm_slide;
    if (obj == NULL) {
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        lv_point_t p;
        lv_indev_get_point(lv_indev_active(), &p);
        lv_area_t a;
        lv_obj_get_coords(obj, &a);
        s_bpm_drag.press_x = p.x - a.x1;
        s_bpm_drag.base_bpm = (int)s_seq.bpm;
        s_bpm_drag.dragging = false;
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        lv_point_t p;
        lv_indev_get_point(lv_indev_active(), &p);
        lv_area_t a;
        lv_obj_get_coords(obj, &a);
        int dx = (p.x - a.x1) - s_bpm_drag.press_x;
        if (dx >= SEQ_BPM_DRAG_PX || dx <= -SEQ_BPM_DRAG_PX) {
            s_bpm_drag.dragging = true;
            int steps = dx / SEQ_BPM_DRAG_PX;
            seq_post_request(SEQ_REQ_BPM_SET, s_bpm_drag.base_bpm + steps, 0, 0);
        }
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST || code == LV_EVENT_CANCEL) {
        /* 未进入拖动 → 按半区点按 ∓1 */
        if (!s_bpm_drag.dragging) {
            lv_area_t a;
            lv_obj_get_coords(obj, &a);
            int w = lv_area_get_width(&a);
            int delta = (s_bpm_drag.press_x < w / 2) ? -1 : 1;
            seq_post_request(SEQ_REQ_BPM_SET, s_bpm_drag.base_bpm + delta, 0, 0);
        }
        return;
    }
}

/* 单按钮 play/stop：按当前状态切换（CHECKED=播放中→点击停止；否则→播放） */
static void app_sequencer_play_stop_cb(lv_event_t *e)
{
    (void)e;
    if (s_seq.playing) {
        seq_post_request(SEQ_REQ_STOP, 0, 0, 0);
    } else {
        seq_post_request(SEQ_REQ_PLAY, 0, 0, 0);
    }
}

static void app_sequencer_random_cb(lv_event_t *e)
{
    (void)e;
    seq_post_request(SEQ_REQ_RANDOM, 0, 0, 0);
}

static void app_sequencer_clear_grid_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    seq_post_request(SEQ_REQ_CLEAR_GRID, 0, 0, 0);
}

/* page 按钮：长按=16↔32 模式切换；单击=A↔B 翻页（仅 32 步模式，16 步无页可切）。
 * 按钮文字色不在此改：引擎写入经请求环，UI 由 on_update 的 step_count 缓存比对
 * 统一同步（单一路径，防请求环满丢弃时 UI 与引擎失配）。 */
static void app_sequencer_page_switch_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_LONG_PRESSED) {
        seq_post_request(SEQ_REQ_STEP_COUNT_SET,
                         (s_seq.step_count == SEQ_STEP_MAX) ? SEQ_STEP_NUM : SEQ_STEP_MAX,
                         0, 0);
    } else if (code == LV_EVENT_SHORT_CLICKED) {
        if (s_seq.step_count == SEQ_STEP_MAX) {
            seq_post_request(SEQ_REQ_PAGE_SWITCH, 0, 0, 0);
        }
    }
}

/* 槽位事件：SHORT_CLICKED=文件目标；LONG_PRESSED=播放队列 */
static void app_sequencer_slot_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_SHORT_CLICKED && code != LV_EVENT_LONG_PRESSED) {
        return;
    }
    lv_obj_t *target = lv_event_get_target_obj(e);
    for (int i = 0; i < SEQ_PATTERN_SLOTS; i++) {
        if (s_seq_ui.pattern_slot[i] == target) {
            if (code == LV_EVENT_SHORT_CLICKED) {
                seq_post_request(SEQ_REQ_SLOT_TAP, i, 0, 0);
            } else {
                seq_post_request(SEQ_REQ_SLOT_HOLD, i, 0, 0);
            }
            return;
        }
    }
}

static void app_sequencer_save_cb(lv_event_t *e)
{
    (void)e;
    /* LV_STATE_DISABLED 仅视觉，不阻断事件；空白时直接忽略（seq_save 内亦有兜底） */
    if (lv_obj_has_state(s_seq_ui.pattern_save_selected, LV_STATE_DISABLED)) {
        return;
    }
    seq_post_request(SEQ_REQ_SAVE, 0, 0, 0);
}

static void app_sequencer_load_cb(lv_event_t *e)
{
    (void)e;
    seq_post_request(SEQ_REQ_PANEL_OPEN, 0, 0, 0);
}

static void app_sequencer_file_select_cb(lv_event_t *e)
{
    (void)e;
    seq_post_request(SEQ_REQ_FILE_SELECT, 0, 0, 0);
}

static void app_sequencer_file_delete_cb(lv_event_t *e)
{
    (void)e;
    seq_post_request(SEQ_REQ_FILE_DELETE, 0, 0, 0);
}

static void app_sequencer_file_close_cb(lv_event_t *e)
{
    (void)e;
    seq_post_request(SEQ_REQ_FILE_CLOSE, 0, 0, 0);
}

/* -------------------- 生命周期 -------------------- */

static bool app_sequencer_on_init(app_base_t *self, void *screen_ctx)
{
    (void)self;
    (void)screen_ctx;
    ESP_LOGI(TAG, "init");

    memset(&s_seq, 0, sizeof(s_seq));
    s_seq.selected_track = -1;
    s_seq.file_target_slot = 0;   /* 默认槽 A 为文件目标（与 checked 对齐） */
    s_seq.file_sel = -1;
    /* 脏检查缓存复位为哨兵：EEZ 屏对象终身缓存，跨会话重进必须强制首轮全量刷新 */
    s_rows_cache_sel = -2;
    s_rows_cache_mute = 0xFF;
    s_rows_cache_solo = 0xFF;
    s_slots_cache_checked = -2;
    s_slots_cache_pending = -2;
    s_slots_cache_file = -2;
    s_slots_cache_content = 0xFF;
    s_slots_cache_flash = false;
    s_save_btn_cache_empty = -1;

    engine_seq_init();
    engine_seq_seed(esp_random());
    /* 缓存引擎步数真源（默认 16），on_update 轮询比对驱动页 UI 同步 */
    s_seq.step_count = engine_seq_get_step_count();
    s_seq.grid_page = 0;
    s_seq.slot_cache = engine_seq_pattern_current();

    /* 从 NVS 恢复 BPM/摇摆/每轨偏好并校验 */
    service_nvs_sequencer_t params;
    service_nvs_get_sequencer(&params);
    s_seq.bpm = (params.bpm >= SEQ_BPM_MIN && params.bpm <= SEQ_BPM_MAX) ? params.bpm : 120;
    s_seq.swing = (params.swing <= 50) ? params.swing : 0;
    engine_seq_set_bpm(s_seq.bpm);
    engine_seq_set_swing(s_seq.swing);
    /* 每轨偏好应用到全部槽：velocity 1-127 为"已保存"哨兵（NVS 全 0=从未配置，
     * 保持引擎常量默认）；note 0=None/无匹配也合法，需区分未配置。 */
    for (int t = 0; t < SEQ_TRACK_COUNT; t++) {
        if (params.track_velocity[t] != 0) {
            /* 曾保存过该轨偏好：直接应用（含 None=0 的合法选择） */
            uint8_t note = params.track_note[t];
            if (note != 0 && seq_drum_index(note) < 0) {
                note = engine_seq_default_note((uint8_t)t);   /* 旧文件里不存在的 note 兜底 */
            }
            engine_seq_apply_track_prefs((uint8_t)t, note,
                                         params.track_rand_temp[t],
                                         params.track_velocity[t],
                                         params.track_probability[t]);
        }
    }

    lvgl_port_lock(portMAX_DELAY);
    if (s_seq_ui.btn_home != NULL) {
        lv_obj_add_event_cb(s_seq_ui.btn_home, app_sequencer_home_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_seq_ui.btn_rec != NULL) {
        lv_obj_add_event_cb(s_seq_ui.btn_rec, app_sequencer_rec_btn_cb, LV_EVENT_CLICKED, self);
    }
    if (s_seq_ui.btn_play_stop != NULL) {
        lv_obj_add_event_cb(s_seq_ui.btn_play_stop, app_sequencer_play_stop_cb,
                            LV_EVENT_CLICKED, NULL);
        /* 缓存子 label 用于动态切换"播放/停止"文字 */
        uint32_t n = lv_obj_get_child_cnt(s_seq_ui.btn_play_stop);
        for (uint32_t i = 0; i < n; i++) {
            lv_obj_t *child = lv_obj_get_child(s_seq_ui.btn_play_stop, i);
            if (child != NULL) {
                s_seq_ui.label_play_stop = child;
                break;
            }
        }
        if (s_seq_ui.label_play_stop != NULL) {
            lv_label_set_text(s_seq_ui.label_play_stop, LV_SYMBOL_PLAY);
        }
    }
    if (s_seq_ui.btn_random != NULL) {
        lv_obj_add_event_cb(s_seq_ui.btn_random, app_sequencer_random_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_seq_ui.btn_clear_grid != NULL) {
        lv_obj_add_flag(s_seq_ui.btn_clear_grid, LV_OBJ_FLAG_CLICKABLE);
        /* 单击清空当前页（原长按清全部已随页模式改语义） */
        lv_obj_add_event_cb(s_seq_ui.btn_clear_grid, app_sequencer_clear_grid_cb,
                            LV_EVENT_CLICKED, NULL);
    }
    /* 16/32 模式 + A/B 页切换（NULL 防御：EEZ 导出漂移丢控件时仅该功能缺席） */
    if (s_seq_ui.btn_grid_page_switch != NULL) {
        lv_obj_add_event_cb(s_seq_ui.btn_grid_page_switch, app_sequencer_page_switch_cb,
                            LV_EVENT_SHORT_CLICKED, NULL);
        lv_obj_add_event_cb(s_seq_ui.btn_grid_page_switch, app_sequencer_page_switch_cb,
                            LV_EVENT_LONG_PRESSED, NULL);
    }
    for (int t = 0; t < SEQ_TRACK_COUNT; t++) {
        if (s_seq_ui.track[t] != NULL) {
            lv_obj_add_flag(s_seq_ui.track[t], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(s_seq_ui.track[t], app_sequencer_track_cb, LV_EVENT_CLICKED, NULL);
        }
    }
    if (s_seq_ui.track_sld_randtemp != NULL) {
        lv_obj_add_event_cb(s_seq_ui.track_sld_randtemp, app_sequencer_param_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
    if (s_seq_ui.track_sld_velocity != NULL) {
        lv_obj_add_event_cb(s_seq_ui.track_sld_velocity, app_sequencer_param_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
    if (s_seq_ui.track_sld_probability != NULL) {
        lv_obj_add_event_cb(s_seq_ui.track_sld_probability, app_sequencer_param_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
    if (s_seq_ui.track_btn_mute != NULL) {
        lv_obj_add_event_cb(s_seq_ui.track_btn_mute, app_sequencer_mute_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_seq_ui.track_btn_solo != NULL) {
        lv_obj_add_event_cb(s_seq_ui.track_btn_solo, app_sequencer_solo_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_seq_ui.track_btn_lock != NULL) {
        lv_obj_add_event_cb(s_seq_ui.track_btn_lock, app_sequencer_lock_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_seq_ui.track_btn_restore != NULL) {
        lv_obj_add_event_cb(s_seq_ui.track_btn_restore, app_sequencer_restore_cb,
                            LV_EVENT_CLICKED, NULL);
    }
    if (s_seq_ui.track_dropdown != NULL) {
        lv_obj_add_event_cb(s_seq_ui.track_dropdown, app_sequencer_dropdown_cb,
                            LV_EVENT_VALUE_CHANGED, NULL);
    }
    if (s_seq_ui.label_bpm_slide != NULL) {
        lv_obj_add_event_cb(s_seq_ui.label_bpm_slide, app_sequencer_bpm_cb,
                            LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(s_seq_ui.label_bpm_slide, app_sequencer_bpm_cb,
                            LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(s_seq_ui.label_bpm_slide, app_sequencer_bpm_cb,
                            LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(s_seq_ui.label_bpm_slide, app_sequencer_bpm_cb,
                            LV_EVENT_PRESS_LOST, NULL);
        lv_obj_add_event_cb(s_seq_ui.label_bpm_slide, app_sequencer_bpm_cb,
                            LV_EVENT_CANCEL, NULL);
    }
    /* 网格：自绘容器 + 命中 */
    if (s_seq_ui.grid != NULL) {
        lv_obj_remove_flag(s_seq_ui.grid, LV_OBJ_FLAG_SCROLLABLE |
                           LV_OBJ_FLAG_CLICK_FOCUSABLE);
        lv_obj_add_flag(s_seq_ui.grid, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_seq_ui.grid, seq_grid_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
        lv_obj_add_event_cb(s_seq_ui.grid, seq_grid_size_changed_cb, LV_EVENT_SIZE_CHANGED, NULL);
        lv_obj_add_event_cb(s_seq_ui.grid, app_sequencer_grid_event_cb,
                            LV_EVENT_SHORT_CLICKED, NULL);
        lv_obj_add_event_cb(s_seq_ui.grid, app_sequencer_grid_event_cb,
                            LV_EVENT_LONG_PRESSED, NULL);
    }
    /* Grid Set 弹窗按钮 */
    if (s_seq_ui.grid_set_confirm != NULL) {
        lv_obj_add_event_cb(s_seq_ui.grid_set_confirm, app_sequencer_grid_confirm_cb,
                            LV_EVENT_CLICKED, NULL);
    }
    if (s_seq_ui.grid_set_cancel != NULL) {
        lv_obj_add_event_cb(s_seq_ui.grid_set_cancel, app_sequencer_grid_clear_cb,
                            LV_EVENT_CLICKED, NULL);
    }
    if (s_seq_ui.grid_set_return != NULL) {
        lv_obj_add_event_cb(s_seq_ui.grid_set_return, app_sequencer_grid_cancel_cb,
                            LV_EVENT_CLICKED, NULL);
    }
    if (s_seq_ui.grid_set_cc_value != NULL) {
        lv_obj_add_event_cb(s_seq_ui.grid_set_cc_value, seq_grid_set_cc_value_cb,
                            LV_EVENT_VALUE_CHANGED, NULL);
    }
    /* 槽位 A~F：单击=文件目标，长按=播放队列 */
    for (int i = 0; i < SEQ_PATTERN_SLOTS; i++) {
        if (s_seq_ui.pattern_slot[i] != NULL) {
            lv_obj_add_event_cb(s_seq_ui.pattern_slot[i], app_sequencer_slot_cb,
                                LV_EVENT_SHORT_CLICKED, NULL);
            lv_obj_add_event_cb(s_seq_ui.pattern_slot[i], app_sequencer_slot_cb,
                                LV_EVENT_LONG_PRESSED, NULL);
        }
    }
    /* 文件操作 */
    if (s_seq_ui.pattern_save_selected != NULL) {
        lv_obj_add_event_cb(s_seq_ui.pattern_save_selected, app_sequencer_save_cb,
                            LV_EVENT_CLICKED, NULL);
    }
    if (s_seq_ui.pattern_load_selected != NULL) {
        lv_obj_add_event_cb(s_seq_ui.pattern_load_selected, app_sequencer_load_cb,
                            LV_EVENT_CLICKED, NULL);
    }
    if (s_seq_ui.list_file_select != NULL) {
        lv_obj_add_event_cb(s_seq_ui.list_file_select, app_sequencer_file_select_cb,
                            LV_EVENT_CLICKED, NULL);
    }
    if (s_seq_ui.list_file_delect != NULL) {
        lv_obj_add_event_cb(s_seq_ui.list_file_delect, app_sequencer_file_delete_cb,
                            LV_EVENT_CLICKED, NULL);
    }
    if (s_seq_ui.list_file_return != NULL) {
        lv_obj_add_event_cb(s_seq_ui.list_file_return, app_sequencer_file_close_cb,
                            LV_EVENT_CLICKED, NULL);
    }
    /* 面板初始显隐：主界面（track + pattern），参数/文件/Grid Set 面板隐藏。
     * 覆盖面板必须 CLICKABLE 吸收点击，否则触摸穿透到下层网格会误翻转格子
     * （xy_pad 设置面板同例） */
    if (s_seq_ui.panel_track_param != NULL) {
        lv_obj_add_flag(s_seq_ui.panel_track_param, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_seq_ui.pattern_list != NULL) {
        lv_obj_add_flag(s_seq_ui.pattern_list, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_seq_ui.pattern_list, LV_OBJ_FLAG_CLICKABLE);
    }
    if (s_seq_ui.panel_grid_set != NULL) {
        lv_obj_add_flag(s_seq_ui.panel_grid_set, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_seq_ui.panel_grid_set, LV_OBJ_FLAG_CLICKABLE);
    }
    if (s_seq_ui.panel_pattern != NULL) {
        lv_obj_clear_flag(s_seq_ui.panel_pattern, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_seq_ui.panel_track != NULL) {
        lv_obj_clear_flag(s_seq_ui.panel_track, LV_OBJ_FLAG_HIDDEN);
    }
    seq_sync_bpm_label();
    if (s_seq_ui.pattern_str != NULL) {
        lv_label_set_text(s_seq_ui.pattern_str, _("Pattern"));
    }
    /* 轨道名初始填充（按当前槽实际 note） */
    seq_sync_track_names();
    /* 页模式 UI 初始同步（步号串 + page 按钮色；控件未导出时 NULL 跳过） */
    seq_sync_page_ui();
    /* 默认锁定：强制 lock CHECKED（EEZ 屏对象终身缓存，CHECKED/DISABLED 状态跨
     * on_init 保留，若不强制，上次解锁退出后重进仍是解锁态；统一走 seq_apply_lock_ui）。 */
    if (s_seq_ui.track_btn_lock != NULL) {
        lv_obj_add_state(s_seq_ui.track_btn_lock, LV_STATE_CHECKED);
    }
    seq_apply_lock_ui();
    /* 初始保存按钮状态（首进入空白槽即 DISABLE） */
    if (s_seq_ui.pattern_save_selected != NULL) {
        if (engine_seq_slot_is_empty(engine_seq_pattern_current())) {
            lv_obj_add_state(s_seq_ui.pattern_save_selected, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(s_seq_ui.pattern_save_selected, LV_STATE_DISABLED);
        }
    }
    lvgl_port_unlock();

    /* 首帧几何缓存（SIZE_CHANGED 可能在事件注册前已发过） */
    lvgl_port_lock(portMAX_DELAY);
    /* 网格撑满 obj159 内容区右下（左侧 69 行标签 / 顶部 36 列号留白），消除右下空隙 */
    if (s_seq_ui.grid != NULL && lv_obj_get_parent(s_seq_ui.grid) != NULL) {
        lv_obj_t *par = lv_obj_get_parent(s_seq_ui.grid);
        int32_t pw = lv_obj_get_content_width(par);
        int32_t ph = lv_obj_get_content_height(par);
        int32_t gx = lv_obj_get_x(s_seq_ui.grid);
        int32_t gy = lv_obj_get_y(s_seq_ui.grid);
        lv_obj_set_size(s_seq_ui.grid, pw - gx, ph - gy);
    }
    lv_obj_update_layout(s_seq_ui.grid);
    seq_grid_cache_geometry();
    /* Trap: EEZ 屏终身缓存只建一次，kill 后重进复用的是同一屏幕对象，
     * 自绘网格不会自动重绘（LVGL 只刷新 invalidate 区域），必须显式失效，
     * 否则背景空白直到播放光标 step 失效才出现。 */
    seq_grid_invalidate_all_stub();
    lvgl_port_unlock();

    return true;
}

static void app_sequencer_on_update(app_base_t *self)
{
    (void)self;
    lvgl_port_lock(portMAX_DELAY);
    if (s_seq.recording_stop_pending && !app_manager_record_is_recording()) {
        char path[256];
        if (app_manager_record_get_last_path(path, sizeof(path))) {
            app_manager_show_notification_timeout(_("录音已保存"), 2000);
        } else {
            app_manager_show_notification_timeout(_("录制时间过短，已丢弃"), 2000);
        }
        s_seq.recording_stop_pending = false;
    }

    if (s_seq.recording_self && !app_manager_record_is_recording()) {
        s_seq.recording_self = false;
        s_seq.recording_stop_pending = true;
    }

    if (s_seq_ui.btn_rec != NULL) {
        if (s_seq.recording_self) {
            lv_obj_add_state(s_seq_ui.btn_rec, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(s_seq_ui.btn_rec, LV_STATE_CHECKED);
        }
    }
    lvgl_port_unlock();

    /* 步数模式同步：引擎为真源（STEP_COUNT_SET/切槽/装载量化生效都可能改），
     * UI 变化（页回退/步号/按钮色/全量失效）统一收口于此，防多路径失配 */
    uint8_t sc = engine_seq_get_step_count();
    if (sc != s_seq.step_count) {
        s_seq.step_count = sc;
        if (s_seq.grid_page >= sc / SEQ_STEP_NUM) {
            s_seq.grid_page = 0;   /* 收缩到 16 步：页回退 A */
        }
        seq_sync_page_ui();
        seq_grid_invalidate_all_stub();
    }

    /* 播放槽切换同步：pending 量化/暂存装载在回卷生效，当前槽变化时全量刷新
     * （否则网格保持旧槽画面，要等播放头逐列扫过才更新） */
    int8_t cur_slot = engine_seq_pattern_current();
    if (cur_slot != s_seq.slot_cache) {
        s_seq.slot_cache = cur_slot;
        seq_grid_invalidate_all_stub();
        seq_sync_track_names();
        seq_sync_track_sliders();
        seq_sync_param_panel_ui();
    }

    /* 播放头轮询：只失效新旧两列；自动跟随仅在播放头"越页"瞬间触发（沿），
     * 手动翻页后不下拽——播放头再次越页时才重新跟随（需求 §2.1 已确认决策） */
    if (s_seq.playing) {
        uint8_t step = engine_seq_get_current_step();
        if (!s_seq.play_step_valid || step != s_seq.play_step_prev) {
            uint8_t play_pg = (uint8_t)(step / SEQ_STEP_NUM);   /* 16 步模式恒 0 */
            uint8_t prev_pg = (uint8_t)(s_seq.play_step_prev / SEQ_STEP_NUM);
            bool crossed = (!s_seq.play_step_valid || play_pg != prev_pg);
            int prev_col = (int)s_seq.play_step_prev - (int)s_seq.grid_page * SEQ_STEP_NUM;
            bool had_prev = s_seq.play_step_valid && prev_col >= 0 && prev_col < SEQ_STEP_NUM;

            /* Trap: 必须先更新 play_step/grid_page 再失效——task_gui(prio10) 可在
             * 两次失效之间抢占渲染，若先失效旧列后更新状态，旧列会用旧光标位置
             * 重绘且之后不再失效 → 光标影子残留到下圈（2026-09 真机） */
            if (crossed && play_pg != s_seq.grid_page &&
                play_pg < s_seq.step_count / SEQ_STEP_NUM) {
                s_seq.grid_page = play_pg;
                s_seq.play_step = step;
                s_seq.play_step_prev = step;
                s_seq.play_step_valid = true;
                seq_sync_step_name();
                seq_grid_invalidate_all_stub();   /* 跨页：两页内容都变，全量失效 */
            } else {
                s_seq.play_step = step;
                s_seq.play_step_prev = step;
                s_seq.play_step_valid = true;
                if (had_prev) {
                    seq_grid_invalidate_col(prev_col);
                }
                int col = (int)step - (int)s_seq.grid_page * SEQ_STEP_NUM;
                if (col >= 0 && col < SEQ_STEP_NUM) {
                    seq_grid_invalidate_col(col);
                }
            }
        }
    } else if (s_seq.play_step_valid) {
        int prev_col = (int)s_seq.play_step_prev - (int)s_seq.grid_page * SEQ_STEP_NUM;
        if (prev_col >= 0 && prev_col < SEQ_STEP_NUM) {
            seq_grid_invalidate_col(prev_col);
        }
        s_seq.play_step_valid = false;
    }

    seq_status_fallback();
    seq_drain_requests();
    seq_sync_slots();
    seq_sync_save_btn();      /* 空白槽 DISABLE 保存按钮（编辑/清空/切槽后随周期刷新） */
    seq_sync_track_rows();   /* M/S 状态与选中高亮随 mute/solo 变化即时刷新 */

    /* 几何自愈：首进屏时若布局未就绪导致尺寸为 0（stride<=0 → 网格整片不绘），
     * SIZE_CHANGED 不会再来，这里每周期重试直到有效并补一次全量失效 */
    if (!s_grid_geo.valid && s_seq_ui.grid != NULL) {
        lvgl_port_lock(portMAX_DELAY);
        seq_grid_cache_geometry();
        lvgl_port_unlock();
        if (s_grid_geo.valid) {
            seq_grid_invalidate_all_stub();
        }
    }
}

static void app_sequencer_on_pause(app_base_t *self)
{
    (void)self;

    seq_stop_play();

    if (s_seq.recording_self) {
        app_manager_record_stop();
        s_seq.recording_self = false;
        s_seq.recording_stop_pending = false;
    }

    /* 兑底保存：bpm/swing + 每轨偏好（note/rand_temp/velocity/probability），
     * 立即落盘（force commit），退出即存不丢 */
    seq_persist_track_prefs(true);
    ESP_LOGI(TAG, "pause");
}

static void app_sequencer_on_resume(app_base_t *self)
{
    (void)self;
    ESP_LOGI(TAG, "resume");
    /* 同 on_init：pause/切屏后重回，缓存屏幕对象不自动重绘自绘网格 */
    seq_grid_invalidate_all_stub();
    seq_sync_track_rows();
}

static void app_sequencer_on_destroy(app_base_t *self)
{
    (void)self;

    seq_stop_play();

    lvgl_port_lock(portMAX_DELAY);
    if (s_seq_ui.btn_home != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.btn_home, app_sequencer_home_cb);
    }
    if (s_seq_ui.btn_rec != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.btn_rec, app_sequencer_rec_btn_cb);
    }
    if (s_seq_ui.btn_random != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.btn_random, app_sequencer_random_cb);
    }
    if (s_seq_ui.btn_play_stop != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.btn_play_stop, app_sequencer_play_stop_cb);
    }
    if (s_seq_ui.btn_clear_grid != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.btn_clear_grid, app_sequencer_clear_grid_cb);
    }
    if (s_seq_ui.btn_grid_page_switch != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.btn_grid_page_switch, app_sequencer_page_switch_cb);
    }
    for (int t = 0; t < SEQ_TRACK_COUNT; t++) {
        if (s_seq_ui.track[t] != NULL) {
            lv_obj_remove_event_cb(s_seq_ui.track[t], app_sequencer_track_cb);
        }
    }
    if (s_seq_ui.track_sld_randtemp != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.track_sld_randtemp, app_sequencer_param_cb);
    }
    if (s_seq_ui.track_sld_velocity != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.track_sld_velocity, app_sequencer_param_cb);
    }
    if (s_seq_ui.track_sld_probability != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.track_sld_probability, app_sequencer_param_cb);
    }
    if (s_seq_ui.track_btn_mute != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.track_btn_mute, app_sequencer_mute_cb);
    }
    if (s_seq_ui.track_btn_solo != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.track_btn_solo, app_sequencer_solo_cb);
    }
    if (s_seq_ui.track_btn_lock != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.track_btn_lock, app_sequencer_lock_cb);
    }
    if (s_seq_ui.track_btn_restore != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.track_btn_restore, app_sequencer_restore_cb);
    }
    if (s_seq_ui.track_dropdown != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.track_dropdown, app_sequencer_dropdown_cb);
    }
    if (s_seq_ui.label_bpm_slide != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.label_bpm_slide, app_sequencer_bpm_cb);
    }
    if (s_seq_ui.grid != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.grid, seq_grid_draw_cb);
        lv_obj_remove_event_cb(s_seq_ui.grid, seq_grid_size_changed_cb);
        lv_obj_remove_event_cb(s_seq_ui.grid, app_sequencer_grid_event_cb);
    }
    if (s_seq_ui.grid_set_confirm != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.grid_set_confirm, app_sequencer_grid_confirm_cb);
    }
    if (s_seq_ui.grid_set_cancel != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.grid_set_cancel, app_sequencer_grid_clear_cb);
    }
    if (s_seq_ui.grid_set_return != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.grid_set_return, app_sequencer_grid_cancel_cb);
    }
    if (s_seq_ui.grid_set_cc_value != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.grid_set_cc_value, seq_grid_set_cc_value_cb);
    }
    for (int i = 0; i < SEQ_PATTERN_SLOTS; i++) {
        if (s_seq_ui.pattern_slot[i] != NULL) {
            lv_obj_remove_event_cb(s_seq_ui.pattern_slot[i], app_sequencer_slot_cb);
        }
    }
    if (s_seq_ui.pattern_save_selected != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.pattern_save_selected, app_sequencer_save_cb);
    }
    if (s_seq_ui.pattern_load_selected != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.pattern_load_selected, app_sequencer_load_cb);
    }
    if (s_seq_ui.list_file_select != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.list_file_select, app_sequencer_file_select_cb);
    }
    if (s_seq_ui.list_file_delect != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.list_file_delect, app_sequencer_file_delete_cb);
    }
    if (s_seq_ui.list_file_return != NULL) {
        lv_obj_remove_event_cb(s_seq_ui.list_file_return, app_sequencer_file_close_cb);
    }
    lvgl_port_unlock();

    if (s_seq.recording_self) {
        app_manager_record_stop();
        s_seq.recording_self = false;
        s_seq.recording_stop_pending = false;
    }

    /* 兜底保存：destroy 前引擎数据仍有效，若未经过 pause 也确保落盘 */
    seq_persist_track_prefs(true);

    engine_seq_deinit();
    ESP_LOGI(TAG, "destroy");
}

esp_err_t app_sequencer_register(void)
{
    static app_base_t app = {
        .name = "Sequencer",
        .screen_name = "app_sequencer",
        .screen_ctx = &s_seq_ui,
        .screen_ctx_size = sizeof(s_seq_ui),
        .widget_bindings = s_seq_bindings,
        .on_init = app_sequencer_on_init,
        .on_update = app_sequencer_on_update,
        .on_pause = app_sequencer_on_pause,
        .on_resume = app_sequencer_on_resume,
        .on_destroy = app_sequencer_on_destroy,
    };
    return app_manager_register(&app);
}
