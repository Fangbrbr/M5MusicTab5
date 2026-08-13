/**
 * @file app_ai_agent.c
 * @brief 小智（xiaozhi）语音助手 App：聊天气泡 UI 壳
 *
 * 激活流、语音通道、唤醒词与 MCP 全部在 service_xiaozhi 内闭环，本 App 只做：
 *  - on_update 轮询 service_xiaozhi_poll_event，把 STT/TTS 句子/状态/激活码/错误
 *    映射为聊天气泡（示例气泡为样式模板，克隆生成）；
 *  - 按住说话键转发 talk_start/talk_stop；
 *  - 设置浮层：对话落盘开关（SD 卡 ai_chat.txt）与长按重置绑定。
 * 历史记录开机期间常驻（PSRAM 堆分配，重启丢失），上限 AI_CHAT_MAX_MESSAGES 条，
 * 超过淘汰最旧一条。
 */

#include "app_ai_agent.h"
#include "app_manager.h"
#include "engine_gui.h"
#include "fonts.h"
#include "service_nvs.h"
#include "service_sd.h"
#include "service_xiaozhi.h"
#include "xiaozhi_mcp.h"
#include "service_audio.h"
#include "service_wifi.h"
#include "service_power.h"
#include "board_hal.h"
#include "esp_lvgl_port.h"

#include "esp_log.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_app_desc.h"
#include "esp_mac.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>

static const char *TAG = "app_ai_agent";

/* 历史条数上限与单条文本长度。
 * Why 1024：流式气泡把一轮回复的多句累积进同一缓冲，256 字节（约 85 个中文）
 * 只够 ~3 行，长回复后续句子被整条丢弃（真机投诉：气泡三行后不再增长）。
 * 单句仍受 service_xiaozhi_event_t.text[256] 限制，1024 允许累积 4 句以上。 */
#define AI_CHAT_MAX_MESSAGES    20
#define AI_CHAT_TEXT_LEN        1024

/* 对话落盘文件（SD 卡根目录，相对挂载点路径） */
#define AI_CHAT_LOG_FILE        "ai_chat.txt"

/* 6 位数字激活码 + '\0' */
#define AI_BIND_CODE_LEN        8

/* 头像字形：与 EEZ 工程 ai_context_user0/ai0 的 avatar label 一致（U+F2BD 用户 / U+F501 AI）。
 * Trap: 字形需 ui_font_chinese_40 覆盖，EEZ 字体 symbols 未收录时在设备上显示为缺字框 */
#define AI_AVATAR_USER          "\xEF\x8A\xBD"  /* U+F2BD */
#define AI_AVATAR_AI            "\xEF\x94\x81"  /* U+F501 */

/* 绑定提示句式与 screens.c 示例默认文案保持一致（仅把 %6d 换成实际激活码） */
#define AI_BIND_TEXT_FMT        "你好，我是喵喵。你的设备还没有绑定，请用设备ID: %s, 在 xiaozhi.me 完成绑定~"
#define AI_GREETING_TEXT        "你好，我是喵喵。按住麦克风按钮或喊“Hi，喵喵”和我说话~"
#define AI_ERROR_PREFIX         "出错了："

typedef struct {
    lv_obj_t *btn_home;
    lv_obj_t *btn_speak;
    lv_obj_t *btn_set;
    lv_obj_t *set_panel;
    lv_obj_t *set_btn_return;
    lv_obj_t *switch_save;
    lv_obj_t *switch_wake_anywhere;
    lv_obj_t *btn_reset;
    lv_obj_t *ctx_user0;
    lv_obj_t *ctx_user0_text;
    lv_obj_t *ctx_ai0;
    lv_obj_t *ctx_ai0_text;
} ui_screen_ai_t;

static ui_screen_ai_t s_ai_ui = {0};

static const widget_binding_t s_ai_bindings[] = {
    WIDGET_BIND(ui_screen_ai_t, btn_home,       "ai_btn_home",         WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_ai_t, btn_speak,      "ai_btn_speak",        WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_ai_t, btn_set,        "ai_btn_set",          WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_ai_t, set_panel,      "ai_set",              WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_ai_t, set_btn_return, "ai_set_btn_return",   WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_ai_t, switch_save,    "ai_switch_save_text", WIDGET_KIND_SWITCH),
    WIDGET_BIND(ui_screen_ai_t, switch_wake_anywhere, "ai_switch_wake_anywhere", WIDGET_KIND_SWITCH),
    WIDGET_BIND(ui_screen_ai_t, btn_reset,      "ai_btn_config_reset", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_ai_t, ctx_user0,      "ai_context_user0",    WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_ai_t, ctx_user0_text, "ai_context_user0_text", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_ai_t, ctx_ai0,        "ai_context_ai0",      WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_ai_t, ctx_ai0_text,   "ai_context_ai0_text", WIDGET_KIND_LABEL),
    WIDGET_BINDING_END,
};

/* -------------------- 消息历史（开机期间持久） -------------------- */

typedef enum {
    AI_ROLE_USER = 0,
    AI_ROLE_AI,
} ai_role_t;

typedef struct {
    uint8_t  role;
    uint32_t seq;                       /* 单调序号，仅用于调试 */
    char     text[AI_CHAT_TEXT_LEN];
} ai_msg_t;

/* Why PSRAM：20 条 x 1KB 约 20KB，静态落内部 RAM 会挤压 AFE/Wi-Fi 的内部内存预算 */
static ai_msg_t *s_history = NULL;
static int       s_history_count = 0;
static uint32_t  s_msg_seq = 0;

/* 历史表懒分配（PSRAM）；失败返回 NULL，调用方按丢弃处理 */
static ai_msg_t *ai_history_storage(void)
{
    if (s_history == NULL) {
        s_history = heap_caps_malloc(sizeof(ai_msg_t) * AI_CHAT_MAX_MESSAGES, MALLOC_CAP_SPIRAM);
        if (s_history == NULL) {
            ESP_LOGE(TAG, "history psram alloc failed, drop message");
        }
    }
    return s_history;
}

/* 未提交的流式 AI 回复：READY（tts stop）时落历史 */
static char s_stream_text[AI_CHAT_TEXT_LEN] = {0};

/* 状态气泡文本（连接中/激活中），READY 时移除 */
static char s_status_text[64] = {0};

/* 最近一次激活码：未激活期间每次进入都要能重绘绑定气泡 */
static char s_activation_code[AI_BIND_CODE_LEN] = {0};

/* -------------------- 运行期 UI 状态 -------------------- */

static lv_obj_t *s_stream_bubble = NULL;    /* 当前流式 AI 气泡（未落历史） */
static lv_obj_t *s_status_bubble = NULL;
static lv_obj_t *s_bind_bubble = NULL;
static bool      s_scroll_pending = false;  /* 有气泡追加，待滚到底 */
static bool      s_need_render = false;     /* 重置后要求整体重绘 */
static bool      s_reset_pending = false;   /* 长按重置延迟到 task_app 执行 */
static bool      s_wake_guard = false;      /* 吸收开关态回滚触发的 VALUE_CHANGED 重入 */
static bool      s_wake_sync_ui = false;    /* 重置后待同步全局唤醒开关 UI（需 LVGL 锁） */
static char      s_theme_name[SERVICE_NVS_THEME_MAX_LEN] = {0};

static void app_ai_agent_home_cb(lv_event_t *e);
static void app_ai_agent_speak_cb(lv_event_t *e);
static void app_ai_agent_set_open_cb(lv_event_t *e);
static void app_ai_agent_set_close_cb(lv_event_t *e);
static void app_ai_agent_switch_cb(lv_event_t *e);
static void app_ai_agent_reset_cb(lv_event_t *e);

/* -------------------- 气泡克隆 -------------------- */

/* 聊天气泡的父 panel：两个示例气泡的直接父容器（FLEX 列布局，自动排行） */
static lv_obj_t *ai_chat_panel(void)
{
    if (s_ai_ui.ctx_user0 == NULL) {
        return NULL;
    }
    return lv_obj_get_parent(s_ai_ui.ctx_user0);
}

/**
 * @brief 按示例气泡样式克隆一条空气泡（panel + 头像 + 文本）
 *
 * 布局/配色参数照抄 screens.c 中 ai_context_user0 / ai_context_ai0 的创建代码；
 * 颜色经 engine_gui_theme_color 取当前主题，主题切换后整体重绘即可联动。
 *
 * Contract: 必须持 LVGL 锁调用。
 * @return 气泡容器；文本 label 为 child 1（头像为 child 0）
 */
static lv_obj_t *ai_bubble_create(lv_obj_t *parent, ai_role_t role)
{
    static const lv_coord_t row_dsc[] = {50, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const lv_coord_t col_dsc[] = {50, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

    lv_color_t text_color = engine_gui_theme_color(role == AI_ROLE_USER ? COLOR_TEXT_SECONDARY
                                                                        : COLOR_TEXT_PRIMARY);

    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_layout(cont, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(cont, text_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_grid_row_dsc_array(cont, row_dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_grid_column_dsc_array(cont, col_dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(cont, 60, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);

    lv_obj_t *avatar = lv_label_create(cont);
    if (role == AI_ROLE_USER) {
        lv_obj_set_size(avatar, 50, 50);
        lv_obj_set_style_text_font(avatar, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_size(avatar, LV_PCT(90), LV_SIZE_CONTENT);
        lv_obj_set_style_text_color(avatar, text_color, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(avatar, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    lv_label_set_text_static(avatar, role == AI_ROLE_USER ? AI_AVATAR_USER : AI_AVATAR_AI);

    lv_obj_t *text = lv_label_create(cont);
    lv_obj_set_size(text, LV_PCT(90), LV_SIZE_CONTENT);
    if (role == AI_ROLE_AI) {
        lv_obj_set_style_text_color(text, text_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    lv_obj_set_style_grid_cell_column_pos(text, 1, LV_PART_MAIN | LV_STATE_DEFAULT);

    return cont;
}

/* Contract: 必须持 LVGL 锁调用 */
static void ai_bubble_set_text(lv_obj_t *bubble, const char *text)
{
    lv_obj_t *label = lv_obj_get_child(bubble, 1);
    if (label != NULL) {
        lv_label_set_text(label, text);
    }
}

/**
 * @brief 删除全部克隆气泡（示例原件保留并保持隐藏）
 *
 * 示例气泡不删只隐：widget binding 持有的指针在 EEZ 屏幕复用期间必须保持有效。
 * Contract: 必须持 LVGL 锁调用。
 */
static void ai_bubbles_clear(void)
{
    lv_obj_t *panel = ai_chat_panel();
    if (panel == NULL) {
        return;
    }

    uint32_t count = lv_obj_get_child_count(panel);
    for (int32_t i = (int32_t)count - 1; i >= 0; i--) {
        lv_obj_t *child = lv_obj_get_child(panel, i);
        if (child != s_ai_ui.ctx_user0 && child != s_ai_ui.ctx_ai0) {
            lv_obj_delete(child);
        }
    }

    if (s_ai_ui.ctx_user0 != NULL) {
        lv_obj_add_flag(s_ai_ui.ctx_user0, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_ai_ui.ctx_ai0 != NULL) {
        lv_obj_add_flag(s_ai_ui.ctx_ai0, LV_OBJ_FLAG_HIDDEN);
    }

    s_stream_bubble = NULL;
    s_status_bubble = NULL;
    s_bind_bubble = NULL;
}

/* 说话键高亮：LISTENING/SPEAKING 时填充 error 色，READY 恢复透明底（screens.c 默认 bg_opa 0） */
static void ai_speak_btn_highlight(bool on)
{
    if (s_ai_ui.btn_speak == NULL) {
        return;
    }
    if (on) {
        lv_obj_set_style_bg_opa(s_ai_ui.btn_speak, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(s_ai_ui.btn_speak, engine_gui_theme_color(COLOR_ERROR),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_bg_opa(s_ai_ui.btn_speak, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

/**
 * @brief 按历史 + 瞬态文本整体重绘气泡区
 *
 * 进入 App、主题切换、重置与历史淘汰时调用；流式回复/状态/绑定气泡由
 * s_stream_text / s_status_text / s_activation_code 无损重建。
 * Contract: 必须持 LVGL 锁调用。
 */
static void ai_render_all(void)
{
    lv_obj_t *panel = ai_chat_panel();
    if (panel == NULL) {
        return;
    }

    ai_bubbles_clear();

    for (int i = 0; i < s_history_count; i++) {
        lv_obj_t *bubble = ai_bubble_create(panel, (ai_role_t)s_history[i].role);
        ai_bubble_set_text(bubble, s_history[i].text);
    }

    if (!service_xiaozhi_is_activated() && s_activation_code[0] != '\0') {
        char buf[AI_CHAT_TEXT_LEN];
        snprintf(buf, sizeof(buf), AI_BIND_TEXT_FMT, s_activation_code);
        s_bind_bubble = ai_bubble_create(panel, AI_ROLE_AI);
        ai_bubble_set_text(s_bind_bubble, buf);
    }

    if (s_stream_text[0] != '\0') {
        s_stream_bubble = ai_bubble_create(panel, AI_ROLE_AI);
        ai_bubble_set_text(s_stream_bubble, s_stream_text);
    }

    if (s_status_text[0] != '\0') {
        s_status_bubble = ai_bubble_create(panel, AI_ROLE_AI);
        ai_bubble_set_text(s_status_bubble, s_status_text);
    }

    service_xiaozhi_state_t state = service_xiaozhi_get_state();
    ai_speak_btn_highlight(state == SERVICE_XIAOZHI_STATE_LISTENING ||
                           state == SERVICE_XIAOZHI_STATE_SPEAKING);

    s_scroll_pending = true;
}

/* -------------------- 历史与落盘 -------------------- */

/* 开关开启时把落历史的消息追加到 SD 卡；失败仅告警，不阻断会话 */
static void ai_log_to_sd(ai_role_t role, const char *text)
{
    if (!service_nvs_get_feature_flag(SERVICE_NVS_FLAG_AI_SAVE_TEXT)) {
        return;
    }
    if (!service_sd_is_mounted()) {
        return;
    }

    FILE *fp = service_sd_fopen(AI_CHAT_LOG_FILE, "a");
    if (fp == NULL) {
        ESP_LOGW(TAG, "open %s failed", AI_CHAT_LOG_FILE);
        app_manager_show_notification_timeout("对话写入 SD 卡失败", 2000);
        return;
    }

    char ts[24] = "----/--/-- --:--:--";
    struct tm now = {0};
    if (app_manager_get_time(&now) == ESP_OK) {
        (void)strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &now);
    }
    fprintf(fp, "[%s][%s] %s\n", ts, role == AI_ROLE_USER ? "user" : "喵喵", text);
    service_sd_fclose(fp);
}

/**
 * @brief 追加一条历史；满员时淘汰最旧一条
 * @return true 发生了淘汰（调用方需整体重绘）
 */
static bool ai_history_append(ai_role_t role, const char *text)
{
    ai_msg_t *hist = ai_history_storage();
    if (hist == NULL) {
        return false;
    }

    bool evicted = false;
    if (s_history_count >= AI_CHAT_MAX_MESSAGES) {
        memmove(&hist[0], &hist[1], sizeof(hist[0]) * (AI_CHAT_MAX_MESSAGES - 1));
        s_history_count = AI_CHAT_MAX_MESSAGES - 1;
        evicted = true;
    }

    ai_msg_t *msg = &hist[s_history_count++];
    msg->role = (uint8_t)role;
    msg->seq = s_msg_seq++;
    strncpy(msg->text, text, sizeof(msg->text) - 1);
    msg->text[sizeof(msg->text) - 1] = '\0';

    ai_log_to_sd(role, msg->text);
    return evicted;
}

/**
 * @brief 追加一条历史并同步上屏
 * Contract: 必须持 LVGL 锁调用。
 */
static void ai_history_append_show(ai_role_t role, const char *text)
{
    if (ai_history_append(role, text)) {
        ai_render_all();
        return;
    }
    lv_obj_t *panel = ai_chat_panel();
    if (panel != NULL) {
        lv_obj_t *bubble = ai_bubble_create(panel, role);
        ai_bubble_set_text(bubble, text);
        s_scroll_pending = true;
    }
}

/* -------------------- 瞬态气泡 -------------------- */

/* 状态气泡原地改文本，不累积；READY/IDLE 时移除。Contract: 持 LVGL 锁调用 */
static void ai_status_set(const char *text)
{
    if (strcmp(s_status_text, text) == 0 && s_status_bubble != NULL) {
        return;
    }
    strncpy(s_status_text, text, sizeof(s_status_text) - 1);
    s_status_text[sizeof(s_status_text) - 1] = '\0';

    if (s_status_bubble == NULL) {
        lv_obj_t *panel = ai_chat_panel();
        if (panel == NULL) {
            return;
        }
        s_status_bubble = ai_bubble_create(panel, AI_ROLE_AI);
    }
    ai_bubble_set_text(s_status_bubble, s_status_text);
    s_scroll_pending = true;
}

/* Contract: 持 LVGL 锁调用 */
static void ai_status_clear(void)
{
    s_status_text[0] = '\0';
    if (s_status_bubble != NULL) {
        lv_obj_delete(s_status_bubble);
        s_status_bubble = NULL;
    }
}

/* 绑定气泡：激活码更新时原地刷新；激活成功后移除。Contract: 持 LVGL 锁调用 */
static void ai_bind_bubble_update(void)
{
    if (service_xiaozhi_is_activated() || s_activation_code[0] == '\0') {
        if (s_bind_bubble != NULL) {
            lv_obj_delete(s_bind_bubble);
            s_bind_bubble = NULL;
        }
        return;
    }

    char buf[AI_CHAT_TEXT_LEN];
    snprintf(buf, sizeof(buf), AI_BIND_TEXT_FMT, s_activation_code);
    if (s_bind_bubble == NULL) {
        lv_obj_t *panel = ai_chat_panel();
        if (panel == NULL) {
            return;
        }
        s_bind_bubble = ai_bubble_create(panel, AI_ROLE_AI);
    }
    ai_bubble_set_text(s_bind_bubble, buf);
    s_scroll_pending = true;
}

/* 流式回复句子拼接进当前 AI 气泡；首句新建。Contract: 持 LVGL 锁调用 */
static void ai_stream_append(const char *sentence)
{
    size_t used = strlen(s_stream_text);
    size_t room = sizeof(s_stream_text) - 1 - used;
    if (room == 0) {
        /* 单条气泡文本受历史上限约束，溢出的句子直接丢弃 */
        ESP_LOGW(TAG, "stream text full, drop sentence");
        return;
    }
    strncat(s_stream_text, sentence, room);

    if (s_stream_bubble == NULL) {
        lv_obj_t *panel = ai_chat_panel();
        if (panel == NULL) {
            return;
        }
        s_stream_bubble = ai_bubble_create(panel, AI_ROLE_AI);
    }
    ai_bubble_set_text(s_stream_bubble, s_stream_text);
    s_scroll_pending = true;
}

/* READY 时把流式气泡落历史；气泡本体留在屏上即代表该历史条目。Contract: 持 LVGL 锁调用 */
static void ai_stream_commit(void)
{
    if (s_stream_text[0] == '\0') {
        return;
    }
    if (ai_history_append(AI_ROLE_AI, s_stream_text)) {
        s_stream_text[0] = '\0';
        s_stream_bubble = NULL;
        ai_render_all();
        return;
    }
    s_stream_text[0] = '\0';
    s_stream_bubble = NULL;
}

/* -------------------- 事件处理 -------------------- */

/* Contract: 持 LVGL 锁调用 */
static void ai_handle_state(service_xiaozhi_state_t state)
{
    /* 状态栏同步轨迹，与 service_xiaozhi 的 state -> 日志对照，
     * 定位界面卡在某状态是事件未投递还是 App 未消费 */
    ESP_LOGD(TAG, "[ui-sync] App 收到 STATE 同步: state=%d", (int)state);
    switch (state) {
    case SERVICE_XIAOZHI_STATE_CONNECTING:
        ai_status_set("正在连接服务器…");
        app_manager_show_notification_timeout("正在连接…", 0);
        break;
    case SERVICE_XIAOZHI_STATE_ACTIVATING:
        ai_status_set("设备激活中，请稍候…");
        app_manager_show_notification_timeout("设备激活中…", 0);
        break;
    case SERVICE_XIAOZHI_STATE_READY:
        ai_status_clear();
        ai_stream_commit();
        ai_speak_btn_highlight(false);
        app_manager_clear_notification();
        if (service_xiaozhi_is_activated()) {
            s_activation_code[0] = '\0';
            ai_bind_bubble_update();
        }
        break;
    case SERVICE_XIAOZHI_STATE_LISTENING:
        ai_speak_btn_highlight(true);
        /* 提示文案按模式适配：auto 服务器 VAD 自动断句，manual 需按住 */
        app_manager_show_notification_timeout(
            service_xiaozhi_is_auto_mode() ? "聆听中…（说完稍候）" : "聆听中…（按住说话）", 0);
        break;
    case SERVICE_XIAOZHI_STATE_SPEAKING:
        ai_speak_btn_highlight(true);
        app_manager_show_notification_timeout("播报中…", 0);
        break;
    case SERVICE_XIAOZHI_STATE_IDLE:
    default:
        ai_status_clear();
        ai_speak_btn_highlight(false);
        app_manager_clear_notification();
        break;
    }
}

/* Contract: 持 LVGL 锁调用 */
static void ai_handle_event(const service_xiaozhi_event_t *evt)
{
    switch (evt->type) {
    case SERVICE_XIAOZHI_EVT_STATE:
        ai_handle_state(evt->state);
        break;
    case SERVICE_XIAOZHI_EVT_STT:
        /* 新一轮用户发言到达：上一轮 AI 流式回复必已完结，先落历史另起
         * 气泡。auto 连续对话永不回 READY，若等 READY 才 commit，所有轮次
         * 的 AI 回复会永久堆在同一个流式气泡里（真机投诉：用户消息更新了
         * 几个泡，AI 回复全挤在旧泡）。 */
        ai_stream_commit();
        ai_history_append_show(AI_ROLE_USER, evt->text);
        break;
    case SERVICE_XIAOZHI_EVT_TTS_SENTENCE:
        ai_stream_append(evt->text);
        break;
    case SERVICE_XIAOZHI_EVT_ACTIVATION_CODE:
        ESP_LOGI(TAG, "received ACTIVATION_CODE: %s", evt->text);
        strncpy(s_activation_code, evt->text, sizeof(s_activation_code) - 1);
        s_activation_code[sizeof(s_activation_code) - 1] = '\0';
        ai_bind_bubble_update();
        break;
    case SERVICE_XIAOZHI_EVT_ERROR: {
        /* 缓冲取前缀 + 事件文本全长，落历史时再截到 AI_CHAT_TEXT_LEN */
        char buf[AI_CHAT_TEXT_LEN + 16];
        snprintf(buf, sizeof(buf), AI_ERROR_PREFIX "%s", evt->text);
        ai_history_append_show(AI_ROLE_AI, buf);
        break;
    }
    case SERVICE_XIAOZHI_EVT_VAD: {
        /* 本地 VAD 边沿驱动收音指示：仅聆听态展示，其余状态忽略 */
        if (evt->state == SERVICE_XIAOZHI_STATE_LISTENING) {
            const char *notice = (strcmp(evt->text, "speech") == 0)
                    ? "识别到语音…"
                    : (service_xiaozhi_is_auto_mode() ? "聆听中…（说完稍候）" : "聆听中…（按住说话）");
            ESP_LOGD(TAG, "[ui-sync] 通知栏更新: %s", notice);
            app_manager_show_notification_timeout(notice, 0);
        }
        break;
    }
    default:
        break;
    }
}

/* -------------------- MCP 产品回调 -------------------- */

static bool ai_mcp_set_volume(int v, void *user_data)
{
    (void)user_data;
    esp_err_t ret = service_audio_set_volume(v);
    if (ret != ESP_OK) {
        return false;
    }
    /* MCP 改音量与设置页滑条同权持久化（旧版只调 audio 不写 NVS，重启即丢） */
    service_nvs_set_volume((int16_t)service_audio_get_volume());
    return true;
}

static bool ai_mcp_set_brightness(int v, void *user_data)
{
    (void)user_data;
    esp_err_t ret = service_nvs_set_brightness((uint8_t)v);
    if (ret != ESP_OK) {
        return false;
    }
    board_display_brightness_set((uint8_t)v);
    /* 回填 EEZ native 变量并清除 pending，防止 300ms 后被 settle 旧值覆盖 */
    engine_gui_sync_brightness(v);
    return true;
}

static bool ai_mcp_set_theme(const char *theme_str, void *user_data)
{
    (void)user_data;
    if (theme_str == NULL || theme_str[0] == '\0') {
        return false;
    }

    const char *eez_theme = NULL;

    /* 深色主题关键词匹配（星空黑 / dark / 深色 / 暗夜 / 黑色） */
    if (strstr(theme_str, "starrynight") != NULL ||
        strstr(theme_str, "dark") != NULL ||
        strstr(theme_str, "星空黑") != NULL ||
        strstr(theme_str, "深色") != NULL ||
        strstr(theme_str, "暗夜") != NULL ||
        strstr(theme_str, "黑色") != NULL ||
        strstr(theme_str, "黑主题") != NULL) {
        eez_theme = "starrynight";
    }
    /* 浅色主题关键词匹配（金丝熊 / hammyorange / light / 浅色 / 橙色 / 暖橙 / 喵喵） */
    else if (strstr(theme_str, "hammyorange") != NULL ||
             strstr(theme_str, "light") != NULL ||
             strstr(theme_str, "金丝熊") != NULL ||
             strstr(theme_str, "浅色") != NULL ||
             strstr(theme_str, "橙色") != NULL ||
             strstr(theme_str, "暖橙") != NULL ||
             strstr(theme_str, "橘色") != NULL ||
             strstr(theme_str, "喵喵") != NULL ||
             strstr(theme_str, "亮色") != NULL) {
        eez_theme = "hammyorange";
    } else {
        return false;
    }

    engine_gui_set_theme(eez_theme);
    return true;
}

static bool ai_mcp_app_launch(const char *name, void *user_data)
{
    (void)user_data;
    if (name == NULL || name[0] == '\0') {
        return false;
    }

    /* App 别名映射表：左边是关键词（子串匹配，不区分大小写太麻烦直接多写几个），
     * 右边是 app_manager 里的精确注册名。
     * Why 模糊匹配而不是直接传英文名：用户说中文时云端 AI 翻译不一定准，
     * 设备端兜底一层关键词匹配，提高命中率。 */
    static const struct {
        const char *keyword;
        const char *reg_name;
    } s_app_aliases[] = {
        /* Zen Mode - 禅模式 / 冥想 / 白噪音 / 放松 / 助眠 */
        { "zen",          "Zen Mode" },
        { "禅",           "Zen Mode" },
        { "冥想",         "Zen Mode" },
        { "白噪音",       "Zen Mode" },
        { "放松",         "Zen Mode" },
        { "助眠",         "Zen Mode" },
        { "睡眠",         "Zen Mode" },
        /* Ear Trainer - 练耳 / 视唱练耳 / 耳训 */
        { "ear trainer",  "Ear Trainer" },
        { "练耳",         "Ear Trainer" },
        { "视唱",         "Ear Trainer" },
        { "耳训",         "Ear Trainer" },
        /* Circle Of Fifths - 五度圈 */
        { "circle",       "Circle Of Fifths" },
        { "五度圈",       "Circle Of Fifths" },
        { "五度",         "Circle Of Fifths" },
        /* Chord Trainer - 和弦练习 / 和弦 */
        { "chord",        "Chord Trainer" },
        { "和弦",         "Chord Trainer" },
        /* XY Pad - XY模式 / XY */
        { "xy pad",       "XY Pad" },
        { "xy模式",       "XY Pad" },
        { "xy",           "XY Pad" },
        /* Drum Pad - 组鼓 / 鼓垫 / 架子鼓 / 打鼓 */
        { "drum pad",     "Drum Pad" },
        { "drum",         "Drum Pad" },
        { "组鼓",         "Drum Pad" },
        { "鼓垫",         "Drum Pad" },
        { "架子鼓",       "Drum Pad" },
        { "打鼓",         "Drum Pad" },
        /* Tiny Piano - 小钢琴 / 钢琴 */
        { "tiny piano",   "Tiny Piano" },
        { "piano",        "Tiny Piano" },
        { "小钢琴",       "Tiny Piano" },
        { "钢琴",         "Tiny Piano" },
        /* Clock Calendar - 时钟日历 / 时钟 / 日历 / 天气 / 闹钟 */
        { "clock calendar", "Clock Calendar" },
        { "clock",        "Clock Calendar" },
        { "时钟",         "Clock Calendar" },
        { "日历",         "Clock Calendar" },
        { "天气",         "Clock Calendar" },
        { "闹钟",         "Clock Calendar" },
        /* AI Agent - AI导师 / AI / 聊天 / 助手 */
        { "ai agent",     "AI Agent" },
        { "ai导师",       "AI Agent" },
        { "ai",           "AI Agent" },
        { "聊天",         "AI Agent" },
        { "助手",         "AI Agent" },
        { "喵喵",         "AI Agent" },
        /* MIDI Player - MIDI播放器 / MIDI / 播放器 */
        { "midi player",  "MIDI Player" },
        { "midi播放",     "MIDI Player" },
        { "midi",         "MIDI Player" },
        { "播放器",       "MIDI Player" },
        /* Metronome - 节拍器 / 节拍 */
        { "metronome",    "Metronome" },
        { "节拍器",       "Metronome" },
        { "节拍",         "Metronome" },
        /* Fun - 趣味 / 答案之书 / 塔罗牌 / 抽卡 / 占卜 / 翻书 */
        { "fun",          "Fun" },
        { "趣味",         "Fun" },
        { "答案之书",     "Fun" },
        { "答案书",       "Fun" },
        { "塔罗",         "Fun" },
        { "占卜",         "Fun" },
        { "抽卡",         "Fun" },
        { "翻书",         "Fun" },
        { "运势",         "Fun" },
    };

    const char *target = NULL;

    /* 先尝试精确匹配注册名（大小写敏感，AI 直接传英文注册名时走这里） */
    for (size_t i = 0; i < sizeof(s_app_aliases) / sizeof(s_app_aliases[0]); i++) {
        if (strcasecmp(name, s_app_aliases[i].reg_name) == 0) {
            target = s_app_aliases[i].reg_name;
            break;
        }
    }

    /* 精确匹配失败，走关键词模糊匹配（小写化比较） */
    if (target == NULL) {
        /* 先把输入转小写放栈上 */
        char lower[64];
        size_t len = strlen(name);
        if (len >= sizeof(lower)) {
            len = sizeof(lower) - 1;
        }
        for (size_t i = 0; i < len; i++) {
            char c = name[i];
            if (c >= 'A' && c <= 'Z') {
                c += 'a' - 'A';
            }
            lower[i] = c;
        }
        lower[len] = '\0';

        for (size_t i = 0; i < sizeof(s_app_aliases) / sizeof(s_app_aliases[0]); i++) {
            /* 关键词也转小写后做子串匹配 */
            const char *kw = s_app_aliases[i].keyword;
            size_t kw_len = strlen(kw);
            char kw_lower[32];
            if (kw_len >= sizeof(kw_lower)) {
                kw_len = sizeof(kw_lower) - 1;
            }
            for (size_t j = 0; j < kw_len; j++) {
                char c = kw[j];
                if (c >= 'A' && c <= 'Z') {
                    c += 'a' - 'A';
                }
                kw_lower[j] = c;
            }
            kw_lower[kw_len] = '\0';

            if (strstr(lower, kw_lower) != NULL) {
                target = s_app_aliases[i].reg_name;
                break;
            }
        }
    }

    if (target == NULL) {
        /* 匹配不到，原样传进去让 app_manager 自己决定（可能失败） */
        ESP_LOGW(TAG, "app launch: no alias match for '%s', passing through", name);
        app_manager_request_launch(name);
    } else {
        ESP_LOGI(TAG, "app launch: '%s' -> '%s'", name, target);
        app_manager_request_launch(target);
    }
    return true;
}

static bool ai_mcp_app_exit(void *user_data)
{
    (void)user_data;
    if (app_manager_get_active() == NULL) {
        return false;
    }
    app_manager_request_kill_active();
    return true;
}

static esp_err_t ai_mcp_get_device_status(char *out, size_t out_len, void *user_data)
{
    (void)user_data;

    service_power_battery_info_t batt = {0};
    (void)service_power_get_battery_info(&batt);

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        snprintf(out, out_len, "内存不足");
        return ESP_FAIL;
    }

    cJSON_AddNumberToObject(root, "volume", (int)service_audio_get_volume());
    cJSON_AddNumberToObject(root, "brightness", (int)service_nvs_get_brightness());
    cJSON_AddStringToObject(root, "theme", engine_gui_get_theme_name());
    cJSON_AddBoolToObject(root, "wifi_connected", service_wifi_is_connected());
    cJSON_AddBoolToObject(root, "headphone_connected", service_power_is_headphone_connected());
    cJSON_AddBoolToObject(root, "charging", batt.is_charging);
    cJSON_AddNumberToObject(root, "battery_voltage_v", batt.bus_voltage_v);
    cJSON_AddNumberToObject(root, "battery_current_a", batt.current_a);

    char *printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (printed == NULL) {
        snprintf(out, out_len, "序列化失败");
        return ESP_FAIL;
    }
    strncpy(out, printed, out_len - 1);
    out[out_len - 1] = '\0';
    cJSON_free(printed);
    return ESP_OK;
}

static esp_err_t ai_mcp_get_system_info(char *out, size_t out_len, void *user_data)
{
    (void)user_data;

    uint8_t mac[6] = {0};
    (void)esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char device_id[18];
    snprintf(device_id, sizeof(device_id), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    char uuid[SERVICE_NVS_XZ_UUID_MAX_LEN] = {0};
    service_nvs_get_xz_uuid(uuid, sizeof(uuid));

    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);

    const esp_app_desc_t *app = esp_app_get_description();

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        snprintf(out, out_len, "内存不足");
        return ESP_FAIL;
    }
    cJSON_AddNumberToObject(root, "version", 2);
    cJSON_AddNumberToObject(root, "flash_size", (double)flash_size);
    cJSON_AddStringToObject(root, "mac_address", device_id);
    cJSON_AddStringToObject(root, "uuid", uuid);
    cJSON_AddStringToObject(root, "chip_model_name", CONFIG_IDF_TARGET);
    cJSON *application = cJSON_CreateObject();
    cJSON_AddStringToObject(application, "name", app->project_name);
    cJSON_AddStringToObject(application, "version", app->version);
    cJSON_AddItemToObject(root, "application", application);

    char *printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (printed == NULL) {
        snprintf(out, out_len, "序列化失败");
        return ESP_FAIL;
    }
    strncpy(out, printed, out_len - 1);
    out[out_len - 1] = '\0';
    cJSON_free(printed);
    return ESP_OK;
}

static void ai_mcp_reboot(void *user_data)
{
    (void)user_data;
    /* 与 xiaozhi_mcp 内部 reboot 工具一致：异步重启由 service_xiaozhi 侧调度 */
}

static void ai_mcp_standby(void *user_data)
{
    (void)user_data;
    /* 用户道别/要求退下：等告别播报播完后 service_xiaozhi 自行关通道回待机 */
    service_xiaozhi_request_standby();
}

static void ai_mcp_register_callbacks(void)
{
    static const xiaozhi_mcp_callbacks_t cbs = {
        .user_data = NULL,
        .set_volume = ai_mcp_set_volume,
        .set_brightness = ai_mcp_set_brightness,
        .set_theme = ai_mcp_set_theme,
        .app_launch = ai_mcp_app_launch,
        .app_exit = ai_mcp_app_exit,
        .get_device_status = ai_mcp_get_device_status,
        .get_system_info = ai_mcp_get_system_info,
        .reboot = ai_mcp_reboot,
        .standby = ai_mcp_standby,
    };
    xiaozhi_mcp_register_callbacks(&cbs);
}

/* 长按重置：清记录与绑定凭据，重启激活流；stop/start 可能阻塞，放 task_app 执行 */
static void ai_do_reset(void)
{
    ESP_LOGI(TAG, "resetting xiaozhi credentials and history");
    
    /* 关闭当前会话 */
    service_xiaozhi_stop();

    /* 彻底清除所有 xiaozhi 凭据 */
    esp_err_t ret = service_nvs_set_xz_uuid("");
    if (ret == ESP_OK) {
        ret = service_nvs_set_xz_ws_url("");
    }
    if (ret == ESP_OK) {
        ret = service_nvs_set_xz_ws_token("");
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "clear nvs credentials failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "cleared all xiaozhi credentials from NVS");
    }

    /* 绑定已失效：全局唤醒一并强制关闭（状态栏 AI 图标以此为准），
     * 开关 UI 由 on_update 持锁后按 s_wake_sync_ui 同步 */
    if (service_nvs_get_xz_wake_anywhere()) {
        service_nvs_set_xz_wake_anywhere(false);
        service_xiaozhi_set_wake_anywhere(false);
        s_wake_sync_ui = true;
    }

    /* 服务器按 MAC 认设备，仅清 uuid/token 仍会被识为已绑定而不下发新激活码；
     * 换新 MAC 身份才能真正走重新激活流 */
    service_xiaozhi_reset_device_identity();

    /* 清空历史记录 */
    s_history_count = 0;
    s_stream_text[0] = '\0';
    s_status_text[0] = '\0';
    s_activation_code[0] = '\0';
    s_need_render = true;

    /* 重新开始会话 */
    ret = service_xiaozhi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "restart xiaozhi after reset failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "xiaozhi restart after reset");
    }

    app_manager_show_notification_timeout("AI 配置已完全重置，请等待 6 位配对码", 3000);
}

/* -------------------- 控件事件回调（LVGL 任务上下文，锁已持有） -------------------- */

static void app_ai_agent_home_cb(lv_event_t *e)
{
    (void)e;
    app_manager_request_kill_active();
}

static void app_ai_agent_speak_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        /* 点按触发一轮对话；talk_start 内部已含 SPEAKING 中 abort 打断。
         * auto 模式下后续断句/续听交给服务器 VAD，对话由服务器主动断开
         * 或空闲超时收尾，无需松手动作 */
        service_xiaozhi_talk_start();
    } else if (code == LV_EVENT_RELEASED) {
        /* 仅 manual 兜底（AFE 不可用）保持按住说话语义，松手结束本轮 */
        if (!service_xiaozhi_is_auto_mode()) {
            service_xiaozhi_talk_stop();
        }
    }
}

static void app_ai_agent_set_open_cb(lv_event_t *e)
{
    (void)e;
    if (s_ai_ui.set_panel == NULL) {
        return;
    }

    if (s_ai_ui.switch_save != NULL) {
        if (service_nvs_get_feature_flag(SERVICE_NVS_FLAG_AI_SAVE_TEXT)) {
            lv_obj_add_state(s_ai_ui.switch_save, LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(s_ai_ui.switch_save, LV_STATE_CHECKED);
        }
    }

    if (s_ai_ui.switch_wake_anywhere != NULL) {
        if (service_nvs_get_xz_wake_anywhere()) {
            lv_obj_add_state(s_ai_ui.switch_wake_anywhere, LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(s_ai_ui.switch_wake_anywhere, LV_STATE_CHECKED);
        }
    }

    if (s_ai_ui.btn_reset != NULL) {
        /* 无绑定凭据且无聊天记录时重置无意义，禁用防误触 */
        if (service_xiaozhi_is_activated() || s_history_count > 0) {
            lv_obj_remove_state(s_ai_ui.btn_reset, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(s_ai_ui.btn_reset, LV_STATE_DISABLED);
        }
    }

    lv_obj_remove_flag(s_ai_ui.set_panel, LV_OBJ_FLAG_HIDDEN);
    /* 设置面板吸收点击，防止穿透触发主界面控件 */
    lv_obj_add_flag(s_ai_ui.set_panel, LV_OBJ_FLAG_CLICKABLE);
}

static void app_ai_agent_set_close_cb(lv_event_t *e)
{
    (void)e;
    if (s_ai_ui.set_panel != NULL) {
        lv_obj_add_flag(s_ai_ui.set_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

static void app_ai_agent_switch_cb(lv_event_t *e)
{
    (void)e;
    if (s_ai_ui.switch_save == NULL) {
        return;
    }

    bool on = lv_obj_has_state(s_ai_ui.switch_save, LV_STATE_CHECKED);
    if (on && !service_sd_is_mounted()) {
        /* 程序改回开关状态不会再触发 VALUE_CHANGED，可安全回弹 */
        lv_obj_remove_state(s_ai_ui.switch_save, LV_STATE_CHECKED);
        app_manager_show_notification_timeout("未检测到 SD 卡，无法开启对话存储", 2000);
        return;
    }

    esp_err_t ret = service_nvs_set_feature_flag(SERVICE_NVS_FLAG_AI_SAVE_TEXT, on);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "save ai_save_text flag failed: %s", esp_err_to_name(ret));
    }
}

/* 全局唤醒开关：持久化 + 同步 service_xiaozhi 运行时状态 */
static void app_ai_agent_wake_anywhere_cb(lv_event_t *e)
{
    (void)e;
    if (s_ai_ui.switch_wake_anywhere == NULL) {
        return;
    }
    if (s_wake_guard) {
        return;
    }
    bool on = lv_obj_has_state(s_ai_ui.switch_wake_anywhere, LV_STATE_CHECKED);
    /* 全局唤醒以完成绑定激活为前提：未激活时回滚开关态，不落 NVS；
     * 回滚触发的 VALUE_CHANGED 重入由 s_wake_guard 吸收 */
    if (on && !service_xiaozhi_is_activated()) {
        s_wake_guard = true;
        lv_obj_remove_state(s_ai_ui.switch_wake_anywhere, LV_STATE_CHECKED);
        s_wake_guard = false;
        app_manager_show_notification_timeout("请先完成绑定激活，再开启全局唤醒", 2500);
        return;
    }
    esp_err_t ret = service_nvs_set_xz_wake_anywhere(on);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "save ai_wake_anywhere flag failed: %s", esp_err_to_name(ret));
    }
    service_xiaozhi_set_wake_anywhere(on);
    app_manager_show_notification_timeout(
        on ? "全局唤醒已开启：任意界面可唤醒后台对话" : "全局唤醒已关闭：仅 AI 界面响应唤醒", 2000);
}

static void app_ai_agent_reset_cb(lv_event_t *e)
{
    (void)e;
    /* 延迟到 on_update（task_app）执行，避免在 LVGL 任务内阻塞 service_xiaozhi_stop */
    s_reset_pending = true;
}

/* -------------------- 生命周期 -------------------- */

static bool app_ai_agent_on_init(app_base_t *self, void *screen_ctx)
{
    (void)self;
    (void)screen_ctx;
    ESP_LOGI(TAG, "init");

    /* 幂等：main.c 未集中初始化，首次进入 App 时自建队列 */
    esp_err_t ret = service_xiaozhi_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "xiaozhi init failed: %s", esp_err_to_name(ret));
        return false;
    }

    /* 首次进入且已激活且历史为空：补一条问候（落历史，本次开机内常驻）
     * Trap: 如果设备未激活，不调用这条问候，避免混淆绑定流程 */
    if (s_history_count == 0 && service_xiaozhi_is_activated()) {
        ESP_LOGI(TAG, "first enter and activated, showing greeting");
        ai_history_append(AI_ROLE_AI, AI_GREETING_TEXT);
    } else if (s_history_count == 0) {
        ESP_LOGI(TAG, "first enter but not activated, will show bind bubble");
    }
    /* 欢迎词 */
    app_manager_show_notification_timeout("欢迎使用 AI 助手，可以点击按钮或说“Hi,唤醒”", 3000);

    lvgl_port_lock(portMAX_DELAY);

    if (s_ai_ui.btn_home != NULL) {
        lv_obj_add_event_cb(s_ai_ui.btn_home, app_ai_agent_home_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_ai_ui.btn_speak != NULL) {
        lv_obj_add_event_cb(s_ai_ui.btn_speak, app_ai_agent_speak_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(s_ai_ui.btn_speak, app_ai_agent_speak_cb, LV_EVENT_RELEASED, NULL);
    }
    if (s_ai_ui.btn_set != NULL) {
        lv_obj_add_event_cb(s_ai_ui.btn_set, app_ai_agent_set_open_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_ai_ui.set_btn_return != NULL) {
        lv_obj_add_event_cb(s_ai_ui.set_btn_return, app_ai_agent_set_close_cb, LV_EVENT_CLICKED, NULL);
    }
    if (s_ai_ui.switch_save != NULL) {
        lv_obj_add_event_cb(s_ai_ui.switch_save, app_ai_agent_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
    if (s_ai_ui.btn_reset != NULL) {
        lv_obj_add_event_cb(s_ai_ui.btn_reset, app_ai_agent_reset_cb, LV_EVENT_LONG_PRESSED, NULL);
    }

    /* 全局唤醒开关（绑定表提供，缺失时构建期门控脚本拦截） */
    if (s_ai_ui.switch_wake_anywhere != NULL) {
        lv_obj_add_event_cb(s_ai_ui.switch_wake_anywhere, app_ai_agent_wake_anywhere_cb,
                            LV_EVENT_VALUE_CHANGED, NULL);
        if (service_nvs_get_xz_wake_anywhere()) {
            lv_obj_add_state(s_ai_ui.switch_wake_anywhere, LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(s_ai_ui.switch_wake_anywhere, LV_STATE_CHECKED);
        }
    }

    /* AI 屏进前台：全局唤醒关闭时唤醒仅在此屏响应。
     * set_ai_ui_active 内部会重放暂存的激活码事件到队列，
     * 此处立即消费队列，确保 ai_render_all 时 s_activation_code 已就绪 */
    service_xiaozhi_set_ai_ui_active(true);

    /* 消费重放的激活码事件，使 s_activation_code 在渲染前就绪 */
    {
        service_xiaozhi_event_t init_evt;
        while (service_xiaozhi_poll_event(&init_evt) == 1) {
            ai_handle_event(&init_evt);
        }
    }

    const char *theme = engine_gui_get_theme_name();
    if (theme != NULL) {
        strncpy(s_theme_name, theme, sizeof(s_theme_name) - 1);
        s_theme_name[sizeof(s_theme_name) - 1] = '\0';
    }

    ai_render_all();

    lvgl_port_unlock();

    ret = service_xiaozhi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "xiaozhi start failed: %s", esp_err_to_name(ret));
    }
    return true;
}

static void app_ai_agent_on_update(app_base_t *self)
{
    (void)self;

    if (s_reset_pending) {
        s_reset_pending = false;
        ai_do_reset();
    }

    lvgl_port_lock(portMAX_DELAY);

    /* 重置绑定后同步全局唤醒开关 UI（ai_do_reset 在锁外执行，延后到此） */
    if (s_wake_sync_ui) {
        s_wake_sync_ui = false;
        if (s_ai_ui.switch_wake_anywhere != NULL) {
            s_wake_guard = true;
            lv_obj_remove_state(s_ai_ui.switch_wake_anywhere, LV_STATE_CHECKED);
            s_wake_guard = false;
        }
    }

    if (s_need_render) {
        s_need_render = false;
        ai_render_all();
    }

    service_xiaozhi_event_t evt;
    while (service_xiaozhi_poll_event(&evt) == 1) {
        ai_handle_event(&evt);
    }

    /* 主题切换后克隆气泡颜色不随 EEZ 刷色，按历史整体重绘 */
    const char *theme = engine_gui_get_theme_name();
    if (theme != NULL && strcmp(theme, s_theme_name) != 0) {
        strncpy(s_theme_name, theme, sizeof(s_theme_name) - 1);
        s_theme_name[sizeof(s_theme_name) - 1] = '\0';
        ai_render_all();
    }

    /* 气泡追加后布局下一拍才生效，滚动延迟到此收敛 */
    if (s_scroll_pending) {
        lv_obj_t *panel = ai_chat_panel();
        if (panel != NULL) {
            int32_t remain = lv_obj_get_scroll_bottom(panel);
            if (remain > 0) {
                lv_obj_scroll_to_y(panel, lv_obj_get_scroll_y(panel) + remain, LV_ANIM_OFF);
            } else {
                s_scroll_pending = false;
            }
        } else {
            s_scroll_pending = false;
        }
    }

    lvgl_port_unlock();
}

static void app_ai_agent_on_pause(app_base_t *self)
{
    (void)self;
    ESP_LOGI(TAG, "pause");

    /* 切出时流式回复落历史，避免重进时半截回答丢失 */
    lvgl_port_lock(portMAX_DELAY);
    ai_stream_commit();
    lvgl_port_unlock();

    /* 会话常驻：切出不再 stop，保持全局唤醒词监听（开机即常驻，
     * 对齐原版 Idle 常驻行为）；仅凭据重置流程才显式 stop */
    /* Trap: pause 后 on_update 停轮询，UI 事件队列无消费者；不清此标志
     * 后台对话会把 s_evt_queue 打满刷 drop 告警（真机实测）。重新进入
     * 由 on_init 重新置位 */
    service_xiaozhi_set_ai_ui_active(false);
}

static void app_ai_agent_on_resume(app_base_t *self)
{
    (void)self;
    ESP_LOGI(TAG, "resume");

    esp_err_t ret = service_xiaozhi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "xiaozhi start failed: %s", esp_err_to_name(ret));
    }

    lvgl_port_lock(portMAX_DELAY);
    const char *theme = engine_gui_get_theme_name();
    if (theme != NULL) {
        strncpy(s_theme_name, theme, sizeof(s_theme_name) - 1);
        s_theme_name[sizeof(s_theme_name) - 1] = '\0';
    }
    ai_render_all();
    lvgl_port_unlock();
}

static void app_ai_agent_on_destroy(app_base_t *self)
{
    (void)self;
    ESP_LOGI(TAG, "destroy");

    /* 移除 on_init 注册的全部事件回调：EEZ 屏幕对象全局复用，
     * 不移除会在再次进入时重复注册，导致一次点击多次触发 */
    lvgl_port_lock(portMAX_DELAY);
    if (s_ai_ui.btn_home != NULL) {
        lv_obj_remove_event_cb(s_ai_ui.btn_home, app_ai_agent_home_cb);
    }
    if (s_ai_ui.btn_speak != NULL) {
        lv_obj_remove_event_cb(s_ai_ui.btn_speak, app_ai_agent_speak_cb);
    }
    if (s_ai_ui.btn_set != NULL) {
        lv_obj_remove_event_cb(s_ai_ui.btn_set, app_ai_agent_set_open_cb);
    }
    if (s_ai_ui.set_btn_return != NULL) {
        lv_obj_remove_event_cb(s_ai_ui.set_btn_return, app_ai_agent_set_close_cb);
    }
    if (s_ai_ui.switch_save != NULL) {
        lv_obj_remove_event_cb(s_ai_ui.switch_save, app_ai_agent_switch_cb);
    }
    if (s_ai_ui.btn_reset != NULL) {
        lv_obj_remove_event_cb(s_ai_ui.btn_reset, app_ai_agent_reset_cb);
    }
    if (s_ai_ui.switch_wake_anywhere != NULL) {
        lv_obj_remove_event_cb(s_ai_ui.switch_wake_anywhere, app_ai_agent_wake_anywhere_cb);
    }

    ai_stream_commit();
    ai_bubbles_clear();
    lvgl_port_unlock();

    /* AI 屏退出前台：全局唤醒关闭时唤醒不再响应（直到再次进入） */
    service_xiaozhi_set_ai_ui_active(false);

    /* 会话常驻：销毁仅清理 UI，不停会话，唤醒词监听持续可用 */
}

esp_err_t app_ai_agent_register(void)
{
    static app_base_t app = {
        .name = "AI Agent",
        .screen_name = "app_ai_agent",
        .screen_ctx = &s_ai_ui,
        .screen_ctx_size = sizeof(s_ai_ui),
        .widget_bindings = s_ai_bindings,
        .on_init = app_ai_agent_on_init,
        .on_update = app_ai_agent_on_update,
        .on_pause = app_ai_agent_on_pause,
        .on_resume = app_ai_agent_on_resume,
        .on_destroy = app_ai_agent_on_destroy,
    };

    /* 注册 MCP 产品回调，使服务器音量/亮度/主题/App 切换等工具可用 */
    ai_mcp_register_callbacks();

    return app_manager_register(&app);
}
