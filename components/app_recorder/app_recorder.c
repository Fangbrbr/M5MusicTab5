/**
 * @file app_recorder.c
 * @brief 录音机 App：ES7210 mic 录音（/sdcard/wav/ 目录）+ 回放 + 文件管理
 *
 * 三模式（rec_mode_type 下拉，NVS 持久化）：
 *   0 语音   16k mono（AFE raw tap，AI 共存）
 *   1 乐器   44.1k stereo（独占 mic，录音期 AI 挂起/扬声器静音）
 *   2 环境   16k mono（AFE AEC 输出 tap，AI 共存）
 * 交互：rec_start_stop 单击开/停录；列表单击播放/停止，长按弹删除确认；
 * 录音中禁止列表操作与模式切换；返回 Home 自动停录/停播（文件正常落盘）。
 */

#include "app_recorder.h"

#include "app_manager.h"
#include "app_ui_binding.h"
#include "engine_gui.h"
#include "service_i18n.h"
#include "service_nvs.h"
#include "service_sd.h"
#include "service_wavrec.h"

#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_vfs_fat.h"
#include "lvgl.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "app_recorder";

/* --------------------------------------------------------------------------
 * 配置常量
 * ------------------------------------------------------------------------ */

#define REC_MAX_FILES        64
#define REC_FILE_NAME_LEN    96
#define REC_STATUS_SAVED_MS  5000
#define REC_STATUS_ERR_MS    5000
#define REC_STATUS_DEL_MS    3000
#define REC_TIME_REFRESH_MS  500
#define REC_REQ_RING_CAP     8

/* 电平柱：13 根竖 bar，窗口滑动（右端=最新），刷新周期决定窗口时长 */
#define REC_WAVE_NUM         13
#define REC_WAVE_REFRESH_MS  80

/* --------------------------------------------------------------------------
 * UI 绑定
 * ------------------------------------------------------------------------ */

typedef struct {
    lv_obj_t *btn_home;
    lv_obj_t *list_file;
    lv_obj_t *del_msgbox;
    lv_obj_t *mode_type;
    lv_obj_t *status;
    lv_obj_t *time_label;
    lv_obj_t *start_stop;
    lv_obj_t *storage_bar;
    lv_obj_t *storage_text;
    lv_obj_t *wave_0;
    lv_obj_t *wave_1;
    lv_obj_t *wave_2;
    lv_obj_t *wave_3;
    lv_obj_t *wave_4;
    lv_obj_t *wave_5;
    lv_obj_t *wave_6;
    lv_obj_t *wave_7;
    lv_obj_t *wave_8;
    lv_obj_t *wave_9;
    lv_obj_t *wave_10;
    lv_obj_t *wave_11;
    lv_obj_t *wave_12;
} ui_screen_recorder_t;

static ui_screen_recorder_t s_rec_ui = {0};

static const widget_binding_t s_recorder_bindings[] = {
    WIDGET_BIND(ui_screen_recorder_t, btn_home,   "rec_btn_home",   WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_recorder_t, list_file,  "rec_list_file",  WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_recorder_t, del_msgbox, "rec_del_msgbox", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_recorder_t, mode_type,  "rec_mode_type",  WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_recorder_t, status,     "rec_status",     WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_recorder_t, time_label, "rec_time",       WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_recorder_t, start_stop, "rec_start_stop", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_recorder_t, storage_bar,  "rec_storage_usage_bar",  WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_recorder_t, storage_text, "rec_storage_usage_text", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_recorder_t, wave_0,  "rec_wave_0",  WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_recorder_t, wave_1,  "rec_wave_1",  WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_recorder_t, wave_2,  "rec_wave_2",  WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_recorder_t, wave_3,  "rec_wave_3",  WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_recorder_t, wave_4,  "rec_wave_4",  WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_recorder_t, wave_5,  "rec_wave_5",  WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_recorder_t, wave_6,  "rec_wave_6",  WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_recorder_t, wave_7,  "rec_wave_7",  WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_recorder_t, wave_8,  "rec_wave_8",  WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_recorder_t, wave_9,  "rec_wave_9",  WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_recorder_t, wave_10, "rec_wave_10", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_recorder_t, wave_11, "rec_wave_11", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_recorder_t, wave_12, "rec_wave_12", WIDGET_KIND_ANY),
    WIDGET_BINDING_END,
};

/* --------------------------------------------------------------------------
 * 状态
 * ------------------------------------------------------------------------ */

typedef struct {
    char     name[REC_FILE_NAME_LEN];
    uint32_t mtime;
} rec_file_item_t;

typedef enum {
    REC_REQ_TOGGLE = 0,      /**< 大按钮：开录/停录 */
    REC_REQ_ITEM_CLICK,      /**< 列表单击：播放/停止 */
    REC_REQ_ITEM_LONG,       /**< 列表长按：删除确认 */
    REC_REQ_DEL_OK,          /**< 删除确认 */
    REC_REQ_DEL_CANCEL,      /**< 删除取消 */
    REC_REQ_MODE,            /**< 模式下拉变更 */
} rec_req_action_t;

typedef struct {
    uint8_t action;
    int     p1;
} rec_req_t;

typedef struct {
    /* NVS 持久化 */
    uint8_t mode;                /* UI 索引：0 语音 / 1 乐器 / 2 环境 */

    /* 文件列表 */
    rec_file_item_t files[REC_MAX_FILES];
    int  file_count;
    int  play_idx;               /* 正在播放的条目，-1 无 */
    int  del_pending;            /* 待删除确认条目，-1 无 */

    /* 运行态 */
    bool rec_active;             /* 已发起录音（等 busy 回落即完成） */
    bool play_active;            /* 已发起播放（等 is_playing 回落即完成） */
    bool msgbox_open;
    bool ui_sync;                /* 程序化写控件期间屏蔽事件回环 */
    bool ui_checked;             /* 大按钮 CHECKED 同步缓存 */
    bool ui_dd_disabled;         /* 模式下拉禁用同步缓存 */
    uint8_t wave_hist[REC_WAVE_NUM];  /* 电平窗口：hist[0] 最旧，hist[12] 最新 */
    uint32_t wave_tick;
    uint32_t time_tick;          /* 时间显示节流 */

    /* 状态串 */
    uint32_t status_until_ms;
} rec_state_t;

static rec_state_t s_rec;

/* SPSC 请求环：LVGL 回调（task_gui）生产，on_update（task_app）消费 */
static rec_req_t s_req_ring[REC_REQ_RING_CAP];
static volatile uint8_t s_req_head = 0;
static volatile uint8_t s_req_tail = 0;

/* --------------------------------------------------------------------------
 * 前向声明
 * ------------------------------------------------------------------------ */

static void rec_scan_files(void);
static void rec_populate_list(void);
static void rec_set_status(const char *text, uint32_t ms);
static void rec_del_msgbox_show(int idx);
static void rec_del_msgbox_hide(void);
static void rec_drain_requests(void);

/* --------------------------------------------------------------------------
 * 小工具
 * ------------------------------------------------------------------------ */

static void rec_post_request(rec_req_action_t action, int p1)
{
    uint8_t tail = s_req_tail;
    uint8_t next = (uint8_t)((tail + 1) % REC_REQ_RING_CAP);
    if (next == s_req_head) {
        return; /* 满即丢弃：UI 事件可重发 */
    }
    s_req_ring[tail].action = (uint8_t)action;
    s_req_ring[tail].p1 = p1;
    __sync_synchronize();
    s_req_tail = next;
}

/** @brief UI 下拉索引 → 服务模式（UI 顺序：语音/乐器/环境） */
static service_wavrec_mode_t rec_mode_to_service(uint8_t ui_mode)
{
    switch (ui_mode) {
    case 1:  return SERVICE_WAVREC_MODE_INSTRUMENT;
    case 2:  return SERVICE_WAVREC_MODE_AMBIENT;
    default: return SERVICE_WAVREC_MODE_VOICE;
    }
}

/** @brief 状态串：ms>0 时超时回退"待机" */
static void rec_set_status(const char *text, uint32_t ms)
{
    lvgl_port_lock(portMAX_DELAY);
    if (s_rec_ui.status != NULL) {
        lv_label_set_text(s_rec_ui.status, text);
    }
    lvgl_port_unlock();
    s_rec.status_until_ms = (ms > 0) ? (lv_tick_get() + ms) : 0;
}

static void rec_status_fallback(void)
{
    if (s_rec.status_until_ms == 0) {
        return;
    }
    if ((int32_t)(lv_tick_get() - s_rec.status_until_ms) >= 0) {
        s_rec.status_until_ms = 0;
        /* 回退到当前真实状态而非固定文案 */
        const char *text = _("待机");
        if (service_wavrec_is_recording()) {
            text = _("录制中");
        } else if (service_wavrec_is_playing()) {
            text = _("播放中");
        } else if (service_wavrec_is_rec_busy()) {
            text = _("准备中");
        }
        lvgl_port_lock(portMAX_DELAY);
        if (s_rec_ui.status != NULL) {
            lv_label_set_text(s_rec_ui.status, text);
        }
        lvgl_port_unlock();
    }
}

static void rec_format_time(char *buf, size_t len, uint32_t ms)
{
    uint32_t sec = ms / 1000;
    snprintf(buf, len, "%02lu:%02lu", (unsigned long)(sec / 60),
             (unsigned long)(sec % 60));
}

/** @brief 存储用量显示（进度条 + 文案）；进入/删文件/落盘后调用 */
static void rec_refresh_storage(void)
{
    uint64_t total = 0, free_bytes = 0;
    bool ok = service_sd_is_mounted() &&
              esp_vfs_fat_info("/sdcard", &total, &free_bytes) == ESP_OK &&
              total > 0;

    lvgl_port_lock(portMAX_DELAY);
    if (ok) {
        uint64_t used = total - free_bytes;
        int percent = (int)(used * 100 / total);
        /* 以 0.1GB 精度显示，避免浮点格式化 */
        unsigned long used_g10 = (unsigned long)((used * 10) >> 30);
        unsigned long free_g10 = (unsigned long)((free_bytes * 10) >> 30);
        if (s_rec_ui.storage_bar != NULL) {
            lv_bar_set_value(s_rec_ui.storage_bar, percent, LV_ANIM_OFF);
        }
        if (s_rec_ui.storage_text != NULL) {
            char buf[64];
            snprintf(buf, sizeof(buf),
                     _("已用: %lu.%luGB / 剩余: %lu.%luGB"),
                     used_g10 / 10, used_g10 % 10, free_g10 / 10, free_g10 % 10);
            lv_label_set_text(s_rec_ui.storage_text, buf);
        }
    } else {
        if (s_rec_ui.storage_bar != NULL) {
            lv_bar_set_value(s_rec_ui.storage_bar, 0, LV_ANIM_OFF);
        }
        if (s_rec_ui.storage_text != NULL) {
            lv_label_set_text(s_rec_ui.storage_text, _("SD 卡不可用"));
        }
    }
    lvgl_port_unlock();
}

/** @brief 电平柱窗口维护：每 80ms 采样服务瞬时电平，窗口左移（右端=最新）。
 * lv_bar_set_value 对同值写入内部早退，静止期不产生无效重绘。 */
static void rec_wave_update(void)
{
    if ((uint32_t)(lv_tick_get() - s_rec.wave_tick) < REC_WAVE_REFRESH_MS) {
        return;
    }
    s_rec.wave_tick = lv_tick_get();

    memmove(&s_rec.wave_hist[0], &s_rec.wave_hist[1], REC_WAVE_NUM - 1);
    s_rec.wave_hist[REC_WAVE_NUM - 1] = service_wavrec_get_level();

    lv_obj_t *bars[REC_WAVE_NUM] = {
        s_rec_ui.wave_0, s_rec_ui.wave_1, s_rec_ui.wave_2, s_rec_ui.wave_3,
        s_rec_ui.wave_4, s_rec_ui.wave_5, s_rec_ui.wave_6, s_rec_ui.wave_7,
        s_rec_ui.wave_8, s_rec_ui.wave_9, s_rec_ui.wave_10, s_rec_ui.wave_11,
        s_rec_ui.wave_12,
    };
    lvgl_port_lock(portMAX_DELAY);
    for (int i = 0; i < REC_WAVE_NUM; i++) {
        if (bars[i] != NULL) {
            lv_bar_set_value(bars[i], s_rec.wave_hist[i], LV_ANIM_OFF);
        }
    }
    lvgl_port_unlock();
}

/* --------------------------------------------------------------------------
 * NVS 参数
 * ------------------------------------------------------------------------ */
static void rec_load_params(void)
{
    service_nvs_recorder_t params;
    service_nvs_get_recorder(&params);
    s_rec.mode = params.mode;
    if (s_rec.mode > 2) {
        s_rec.mode = 0;
    }
}

static void rec_save_params(void)
{
    service_nvs_recorder_t params = {
        .mode = s_rec.mode,
        .reserved = {0},
    };
    service_nvs_set_recorder(&params);
}

/* --------------------------------------------------------------------------
 * 文件列表
 * ------------------------------------------------------------------------ */

static int rec_file_cmp(const void *a, const void *b)
{
    const rec_file_item_t *fa = a;
    const rec_file_item_t *fb = b;
    return (fb->mtime > fa->mtime) ? 1 : (fb->mtime < fa->mtime) ? -1 : 0;
}

/** @brief .wav 后缀判断（大小写不敏感，避免引 strings.h） */
static bool rec_is_wav(const char *name)
{
    size_t len = strlen(name);
    if (len < 5) {
        return false;
    }
    const char *s = name + len - 4;
    return s[0] == '.' &&
           (s[1] == 'w' || s[1] == 'W') &&
           (s[2] == 'a' || s[2] == 'A') &&
           (s[3] == 'v' || s[3] == 'V');
}

static void rec_scan_files(void)
{
    s_rec.file_count = 0;
    if (!service_sd_is_mounted()) {
        return;
    }

    char dir_path[64];
    snprintf(dir_path, sizeof(dir_path), "%s/wav", service_sd_get_mount_point());
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && s_rec.file_count < REC_MAX_FILES) {
        const char *name = ent->d_name;
        if (!rec_is_wav(name)) {
            continue;
        }
        rec_file_item_t *item = &s_rec.files[s_rec.file_count];
        snprintf(item->name, sizeof(item->name), "%s", name);

        char full[192];
        snprintf(full, sizeof(full), "%s/%s", dir_path, name);
        struct stat st;
        item->mtime = (stat(full, &st) == 0) ? (uint32_t)st.st_mtime : 0;
        s_rec.file_count++;
    }
    closedir(dir);

    qsort(s_rec.files, s_rec.file_count, sizeof(rec_file_item_t), rec_file_cmp);
}

static void app_recorder_item_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SHORT_CLICKED) {
        rec_post_request(REC_REQ_ITEM_CLICK, idx);
    } else if (code == LV_EVENT_LONG_PRESSED) {
        rec_post_request(REC_REQ_ITEM_LONG, idx);
    }
}

static void rec_populate_list(void)
{
    lv_obj_t *list = s_rec_ui.list_file;
    if (list == NULL) {
        return;
    }
    lvgl_port_lock(portMAX_DELAY);
    lv_obj_clean(list);

    if (s_rec.file_count == 0) {
        lv_obj_t *lbl = lv_label_create(list);
        lv_label_set_text(lbl, _("暂无录音文件"));
        lv_obj_set_style_text_color(lbl, engine_gui_theme_color(COLOR_TEXT_SECONDARY),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    for (int i = 0; i < s_rec.file_count; i++) {
        lv_obj_t *btn = lv_button_create(list);
        lv_obj_set_size(btn, LV_PCT(100), 46);
        lv_obj_set_style_bg_color(btn, engine_gui_theme_color(COLOR_CARD),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(btn, engine_gui_theme_color(COLOR_TEXT_PRIMARY),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, s_rec.files[i].name);
        lv_obj_set_style_align(lbl, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        if (s_rec.play_active && s_rec.play_idx == i) {
            lv_obj_add_state(btn, LV_STATE_CHECKED);
        }
        lv_obj_add_event_cb(btn, app_recorder_item_cb, LV_EVENT_SHORT_CLICKED,
                            (void *)(intptr_t)i);
        lv_obj_add_event_cb(btn, app_recorder_item_cb, LV_EVENT_LONG_PRESSED,
                            (void *)(intptr_t)i);
    }
    lvgl_port_unlock();
}

/* --------------------------------------------------------------------------
 * 删除确认弹窗（rec_del_msgbox：EEZ 空壳 lv_msgbox，内容运行时构建）
 * ------------------------------------------------------------------------ */

static void app_recorder_del_ok_cb(lv_event_t *e)
{
    (void)e;
    rec_post_request(REC_REQ_DEL_OK, 0);
}

static void app_recorder_del_cancel_cb(lv_event_t *e)
{
    (void)e;
    rec_post_request(REC_REQ_DEL_CANCEL, 0);
}

static void rec_del_msgbox_show(int idx)
{
    lv_obj_t *box = s_rec_ui.del_msgbox;
    if (box == NULL) {
        return;
    }
    lvgl_port_lock(portMAX_DELAY);

    /* 重建内容：msgbox add_* 是追加式，重弹前必须清 content/footer。
     * Trap: footer 惰性创建，新弹窗 get_footer==NULL，lv_obj_clean(NULL)
     * 触发 LV_ASSERT_NULL 死循环（while(1)，真机 WDT 重启）——必须判空 */
    lv_obj_clean(lv_msgbox_get_content(box));
    lv_obj_t *footer = lv_msgbox_get_footer(box);
    if (footer != NULL) {
        lv_obj_clean(footer);
    }
    /* EEZ 固定高 256 装不下 100px 按钮的 footer（底部截断），高度改按内容自适应 */
    lv_obj_set_height(box, LV_SIZE_CONTENT);

    lv_obj_t *name_lbl = lv_msgbox_add_text(box, s_rec.files[idx].name);
    lv_obj_set_style_text_align(name_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_t *ask_lbl = lv_msgbox_add_text(box, _("确认删除该录音？"));
    lv_obj_set_style_text_align(ask_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    /* EEZ 未暴露按钮配色/尺寸/圆角接口，显式按主题上色并固定 100x60(宽x高)、圆角 20
     *（项目按钮惯例值，msgbox 内部按钮不吃 EEZ 逐控件样式）：OK=删除（错误色），LEFT=返回 */
    lv_obj_t *btn_ok = lv_msgbox_add_footer_button(box, LV_SYMBOL_OK);
    lv_obj_set_size(btn_ok, 100, 60);
    lv_obj_set_style_radius(btn_ok, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn_ok, engine_gui_theme_color(COLOR_ERROR),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(btn_ok, engine_gui_theme_color(COLOR_TEXT_PRIMARY),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn_ok, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(btn_ok, app_recorder_del_ok_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_back = lv_msgbox_add_footer_button(box, LV_SYMBOL_LEFT);
    lv_obj_set_size(btn_back, 100, 60);
    lv_obj_set_style_radius(btn_back, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn_back, engine_gui_theme_color(COLOR_PRIMARY),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(btn_back, engine_gui_theme_color(COLOR_TEXT_PRIMARY),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn_back, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(btn_back, app_recorder_del_cancel_cb, LV_EVENT_CLICKED, NULL);

    /* Trap: footer class 默认高度固定 LV_DPI_DEF/3(~43px)，100px 按钮被上下裁切
     *（圆角恰在裁剪区外，可见部分就是直角矩形横带）；footer 高度改内容自适应 */
    footer = lv_msgbox_get_footer(box);
    if (footer != NULL) {
        lv_obj_set_height(footer, LV_SIZE_CONTENT);
    }

    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);  /* 吸收点击防穿透列表 */
    lv_obj_clear_flag(box, LV_OBJ_FLAG_HIDDEN);
    lvgl_port_unlock();
    s_rec.msgbox_open = true;
    s_rec.del_pending = idx;
}

static void rec_del_msgbox_hide(void)
{
    lvgl_port_lock(portMAX_DELAY);
    if (s_rec_ui.del_msgbox != NULL) {
        lv_obj_add_flag(s_rec_ui.del_msgbox, LV_OBJ_FLAG_HIDDEN);
    }
    lvgl_port_unlock();
    s_rec.msgbox_open = false;
    s_rec.del_pending = -1;
}

/* --------------------------------------------------------------------------
 * 传输控制（task_app 上下文）
 * ------------------------------------------------------------------------ */

static void rec_err_to_status(service_wavrec_err_t err)
{
    switch (err) {
    case SERVICE_WAVREC_ERR_NO_SD:
        rec_set_status(_("SD 卡不可用"), REC_STATUS_ERR_MS);
        break;
    case SERVICE_WAVREC_ERR_NO_VOICE_FE:
        rec_set_status(_("语音前端不可用"), REC_STATUS_ERR_MS);
        break;
    case SERVICE_WAVREC_ERR_BAD_FILE:
        rec_set_status(_("不支持的 WAV 格式"), REC_STATUS_ERR_MS);
        break;
    case SERVICE_WAVREC_ERR_FILE:
        rec_set_status(_("录音写盘失败"), REC_STATUS_ERR_MS);
        break;
    case SERVICE_WAVREC_ERR_MIC:
    default:
        rec_set_status(_("录音启动失败"), REC_STATUS_ERR_MS);
        break;
    }
}

static void rec_toggle(void)
{
    if (service_wavrec_is_recording()) {
        service_wavrec_stop();
        return;
    }
    /* ARMING/STOPPING 锁存：落盘收尾未完成前禁止再次触发，
     * 界面状态由 on_update 按服务状态同步恢复 */
    if (service_wavrec_is_rec_busy()) {
        return;
    }
    if (s_rec.play_active) {
        service_wavrec_play_stop();  /* 播放中开录：先停播 */
        s_rec.play_active = false;
        s_rec.play_idx = -1;
    }
    service_wavrec_err_t err = service_wavrec_start(rec_mode_to_service(s_rec.mode));
    if (err != SERVICE_WAVREC_OK) {
        rec_err_to_status(err);
        return;
    }
    s_rec.rec_active = true;
    rec_set_status(_("准备中"), 0);
}

static void rec_item_click(int idx)
{
    if (idx < 0 || idx >= s_rec.file_count) {
        return;
    }
    if (service_wavrec_is_rec_busy()) {
        return;  /* 录音中禁止操作文件 */
    }
    if (s_rec.play_active && s_rec.play_idx == idx) {
        service_wavrec_play_stop();
        return;  /* 完成态由 on_update 轮询收尾 */
    }
    char path[192];
    snprintf(path, sizeof(path), "%s/wav/%s", service_sd_get_mount_point(),
             s_rec.files[idx].name);
    service_wavrec_err_t err = service_wavrec_play(path);
    if (err != SERVICE_WAVREC_OK) {
        rec_err_to_status(err);
        return;
    }
    s_rec.play_active = true;
    s_rec.play_idx = idx;
    rec_set_status(_("播放中"), 0);
    rec_populate_list();  /* 刷新播放条目高亮 */
}

static void rec_del_confirm(void)
{
    int idx = s_rec.del_pending;
    rec_del_msgbox_hide();
    if (idx < 0 || idx >= s_rec.file_count) {
        return;
    }
    if (s_rec.play_active && s_rec.play_idx == idx) {
        service_wavrec_play_stop();  /* 删除正在播放的文件：先停播释放句柄 */
        s_rec.play_active = false;
        s_rec.play_idx = -1;
    }

    char path[192];
    snprintf(path, sizeof(path), "%s/wav/%s", service_sd_get_mount_point(),
             s_rec.files[idx].name);
    if (remove(path) == 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), _("Deleted: %s"), s_rec.files[idx].name);
        rec_set_status(msg, REC_STATUS_DEL_MS);
    } else {
        rec_set_status(_("Delete failed"), REC_STATUS_ERR_MS);
    }
    rec_scan_files();
    rec_populate_list();
    rec_refresh_storage();
}

static void rec_drain_requests(void)
{
    while (s_req_head != s_req_tail) {
        rec_req_t req = s_req_ring[s_req_head];
        __sync_synchronize();
        s_req_head = (uint8_t)((s_req_head + 1) % REC_REQ_RING_CAP);

        switch (req.action) {
        case REC_REQ_TOGGLE:
            rec_toggle();
            break;
        case REC_REQ_ITEM_CLICK:
            if (!s_rec.msgbox_open) {
                rec_item_click(req.p1);
            }
            break;
        case REC_REQ_ITEM_LONG:
            if (!s_rec.msgbox_open && !service_wavrec_is_rec_busy() &&
                req.p1 >= 0 && req.p1 < s_rec.file_count) {
                rec_del_msgbox_show(req.p1);
            }
            break;
        case REC_REQ_DEL_OK:
            rec_del_confirm();
            break;
        case REC_REQ_DEL_CANCEL:
            rec_del_msgbox_hide();
            break;
        case REC_REQ_MODE:
            s_rec.mode = (uint8_t)req.p1;
            rec_save_params();
            break;
        default:
            break;
        }
    }
}

/* --------------------------------------------------------------------------
 * LVGL 事件回调（task_gui 上下文，只登记请求/纯 UI）
 * ------------------------------------------------------------------------ */

static void app_recorder_home_cb(lv_event_t *e)
{
    (void)e;
    app_manager_request_kill_active();
}

static void app_recorder_toggle_cb(lv_event_t *e)
{
    (void)e;
    rec_post_request(REC_REQ_TOGGLE, 0);
}

static void app_recorder_mode_cb(lv_event_t *e)
{
    if (s_rec.ui_sync) {
        return;
    }
    lv_obj_t *dd = lv_event_get_target_obj(e);
    uint32_t sel = lv_dropdown_get_selected(dd);
    if (service_wavrec_is_rec_busy()) {
        /* 录音中禁止切模式：回退显示（不触发状态变更） */
        s_rec.ui_sync = true;
        lv_dropdown_set_selected(dd, s_rec.mode);
        s_rec.ui_sync = false;
        return;
    }
    rec_post_request(REC_REQ_MODE, (int)sel);
}

/* --------------------------------------------------------------------------
 * 生命周期
 * ------------------------------------------------------------------------ */

static bool app_recorder_on_init(app_base_t *self, void *screen_ctx)
{
    (void)self;
    (void)screen_ctx;
    ESP_LOGI(TAG, "init");

    memset(&s_rec, 0, sizeof(s_rec));
    s_rec.play_idx = -1;
    s_rec.del_pending = -1;
    s_req_head = 0;
    s_req_tail = 0;

    rec_load_params();

    lvgl_port_lock(portMAX_DELAY);
    s_rec.ui_sync = true;
    if (s_rec_ui.mode_type != NULL) {
        lv_dropdown_set_selected(s_rec_ui.mode_type, s_rec.mode);
        lv_obj_add_event_cb(s_rec_ui.mode_type, app_recorder_mode_cb,
                            LV_EVENT_VALUE_CHANGED, NULL);
    }
    s_rec.ui_sync = false;
    if (s_rec_ui.del_msgbox != NULL) {
        /* Trap: EEZ 侧该 msgbox 未配默认隐藏（midi 屏的配了），此处补齐 */
        lv_obj_add_flag(s_rec_ui.del_msgbox, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_rec_ui.status != NULL) {
        lv_label_set_text(s_rec_ui.status, _("待机"));
    }
    if (s_rec_ui.time_label != NULL) {
        lv_label_set_text(s_rec_ui.time_label, "--:--");
    }
    if (s_rec_ui.btn_home != NULL) {
        lv_obj_add_event_cb(s_rec_ui.btn_home, app_recorder_home_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_rec_ui.start_stop != NULL) {
        lv_obj_add_event_cb(s_rec_ui.start_stop, app_recorder_toggle_cb, LV_EVENT_CLICKED, NULL);
    }
    lvgl_port_unlock();

    rec_scan_files();
    rec_populate_list();
    rec_refresh_storage();
    return true;
}

static void app_recorder_on_update(app_base_t *self)
{
    (void)self;

    rec_drain_requests();

    /* 录音完成沿（busy 回落）：提示落盘 + 刷新列表 */
    if (s_rec.rec_active && !service_wavrec_is_rec_busy()) {
        s_rec.rec_active = false;
        service_wavrec_err_t err = service_wavrec_take_error();
        if (err != SERVICE_WAVREC_OK) {
            rec_err_to_status(err);
        } else {
            char path[160];
            if (service_wavrec_get_last_path(path, sizeof(path))) {
                const char *base = strrchr(path, '/');
                char msg[128];
                snprintf(msg, sizeof(msg), _("Saved: %s"),
                         (base != NULL) ? base + 1 : path);
                rec_set_status(msg, REC_STATUS_SAVED_MS);
            }
        }
        rec_scan_files();
        rec_populate_list();
        rec_refresh_storage();
    }

    /* 播放完成沿（自然播完） */
    if (s_rec.play_active && !service_wavrec_is_playing()) {
        s_rec.play_active = false;
        s_rec.play_idx = -1;
        rec_set_status(_("待机"), 0);
        rec_populate_list();
    }

    /* 运行态传输控件同步：CHECKED + 文案（单一数据源=服务状态） */
    bool recording = service_wavrec_is_recording();
    if (recording != s_rec.ui_checked) {
        s_rec.ui_checked = recording;
        lvgl_port_lock(portMAX_DELAY);
        if (s_rec_ui.start_stop != NULL) {
            lv_obj_t *lbl = lv_obj_get_child(s_rec_ui.start_stop, 0);
            if (lbl != NULL) {
                lv_label_set_text(lbl, recording ? _("停止") : _("录制"));
            }
            if (recording) {
                lv_obj_add_state(s_rec_ui.start_stop, LV_STATE_CHECKED);
            } else {
                lv_obj_remove_state(s_rec_ui.start_stop, LV_STATE_CHECKED);
            }
        }
        if (recording && s_rec_ui.status != NULL) {
            lv_label_set_text(s_rec_ui.status, _("录制中"));
            s_rec.status_until_ms = 0;
        }
        lvgl_port_unlock();
    }

    /* 录音链路占用期间禁用模式切换（下拉禁用态同步） */
    bool rec_busy = service_wavrec_is_rec_busy();
    if (rec_busy != s_rec.ui_dd_disabled) {
        s_rec.ui_dd_disabled = rec_busy;
        lvgl_port_lock(portMAX_DELAY);
        if (s_rec_ui.mode_type != NULL) {
            if (rec_busy) {
                lv_obj_add_state(s_rec_ui.mode_type, LV_STATE_DISABLED);
            } else {
                lv_obj_remove_state(s_rec_ui.mode_type, LV_STATE_DISABLED);
            }
        }
        lvgl_port_unlock();
    }

    /* 时间显示节流刷新 */
    if ((uint32_t)(lv_tick_get() - s_rec.time_tick) >= REC_TIME_REFRESH_MS) {
        s_rec.time_tick = lv_tick_get();
        char tbuf[24];
        if (service_wavrec_is_rec_busy()) {
            rec_format_time(tbuf, sizeof(tbuf), service_wavrec_get_elapsed_ms());
        } else if (service_wavrec_is_playing()) {
            char now[12], total[12];
            rec_format_time(now, sizeof(now), service_wavrec_play_get_pos_ms());
            rec_format_time(total, sizeof(total), service_wavrec_play_get_total_ms());
            snprintf(tbuf, sizeof(tbuf), "%s/%s", now, total);
        } else {
            snprintf(tbuf, sizeof(tbuf), "--:--");
        }
        lvgl_port_lock(portMAX_DELAY);
        if (s_rec_ui.time_label != NULL) {
            lv_label_set_text(s_rec_ui.time_label, tbuf);
        }
        lvgl_port_unlock();
    }

    rec_wave_update();

    rec_status_fallback();
}

static void app_recorder_on_pause(app_base_t *self)
{
    (void)self;
    /* Trap: 停录/停播均异步收尾（乐器模式 STOPPING 态由 task_app 继续驱动），
     * 文件落盘与 AI 恢复由 service_wavrec 保证，App 退出不影响 */
    if (service_wavrec_is_rec_busy()) {
        service_wavrec_stop();
    }
    if (service_wavrec_is_playing()) {
        service_wavrec_play_stop();
    }
    rec_save_params();
}

static void app_recorder_on_destroy(app_base_t *self)
{
    (void)self;
    ESP_LOGI(TAG, "destroy");

    /* Trap: EEZ 屏幕对象持久存在，不移除回调会在再次进入时重复注册 */
    lvgl_port_lock(portMAX_DELAY);
    if (s_rec_ui.btn_home != NULL) {
        lv_obj_remove_event_cb(s_rec_ui.btn_home, app_recorder_home_cb);
    }
    if (s_rec_ui.start_stop != NULL) {
        lv_obj_remove_event_cb(s_rec_ui.start_stop, app_recorder_toggle_cb);
    }
    if (s_rec_ui.mode_type != NULL) {
        lv_obj_remove_event_cb(s_rec_ui.mode_type, app_recorder_mode_cb);
    }
    lvgl_port_unlock();
    /* 列表条目回调随 lv_obj_clean/屏幕重建失效，无需逐个移除 */
}

esp_err_t app_recorder_register(void)
{
    static app_base_t app = {
        .name = "Recorder",
        .screen_name = "app_recorder",
        .screen_ctx = &s_rec_ui,
        .screen_ctx_size = sizeof(s_rec_ui),
        .widget_bindings = s_recorder_bindings,
        .on_init = app_recorder_on_init,
        .on_update = app_recorder_on_update,
        .on_pause = app_recorder_on_pause,
        .on_destroy = app_recorder_on_destroy,
    };
    return app_manager_register(&app);
}
