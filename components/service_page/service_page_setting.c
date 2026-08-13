/**
 * @file service_page_setting.c
 * @brief Setting 屏幕后端事件处理
 *
 * 处理亮度、音量、语言、主题、开机页面、自动熄屏时间、自动休眠、显示反向、
 * Wi-Fi/系统重置等设置项。
 */

#include "service_page_setting.h"
#include "screens.h"
#include "engine_gui.h"
#include "service_nvs.h"
#include "service_power.h"
#include "service_audio.h"
#include "service_i18n.h"
#include "service_wifi.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "stdio.h"
#include "string.h"

static const char *TAG = "service_page_setting";

#define SETTING_LANG_COUNT 2
static const char *s_lang_ids[SETTING_LANG_COUNT] = {"zh-CN", "en"};

/* static 函数前向声明 */
static void setting_apply_tab_button_styles(void);
static void setting_update_wifi_tip(void);

static void setting_update_brightness_label(int32_t value)
{
    if (objects.setting_slide_brightness_num == NULL) {
        return;
    }
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", (int)value);
    lv_label_set_text(objects.setting_slide_brightness_num, buf);
}

static void setting_update_volume_label(int32_t value)
{
    if (objects.setting_slide_volume_num == NULL) {
        return;
    }
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", (int)value);
    lv_label_set_text(objects.setting_slide_volume_num, buf);
}

static void setting_home_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "return to launcher");
    engine_gui_switch_screen(NULL);
}

static void setting_about_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "open about");
    engine_gui_switch_screen("about");
}

static void setting_ftp_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "open ftp");
    engine_gui_switch_screen("ftp");
}

static void setting_language_cb(lv_event_t *e)
{
    (void)e;
    if (objects.setting_language == NULL) {
        return;
    }
    uint16_t sel = lv_dropdown_get_selected(objects.setting_language);
    if (sel >= SETTING_LANG_COUNT) {
        return;
    }
    const char *lang_id = s_lang_ids[sel];
    ESP_LOGI(TAG, "language changed: %s", lang_id);
    service_i18n_set_language_by_id(lang_id);
    service_nvs_set_language(lang_id);
}

static void setting_brightness_cb(lv_event_t *e)
{
    (void)e;
    if (objects.setting_slide_brightness == NULL) {
        return;
    }
    /* 背光下发与 NVS 落盘统一由 engine_gui 亮度 settle 逻辑在松手后执行
     * （EEZ native 变量 setter 路径），此处只同步标签显示 */
    int32_t value = lv_slider_get_value(objects.setting_slide_brightness);
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    setting_update_brightness_label(value);
}

static void setting_volume_cb(lv_event_t *e)
{
    (void)e;
    if (objects.setting_slide_volume == NULL) {
        return;
    }
    int32_t value = lv_slider_get_value(objects.setting_slide_volume);
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    service_audio_set_volume(value);
    service_nvs_set_volume(value);
    setting_update_volume_label(value);
}

static void setting_theme_cb(lv_event_t *e)
{
    (void)e;
    if (objects.setting_theme == NULL) {
        return;
    }
    uint16_t sel = lv_dropdown_get_selected(objects.setting_theme);
    const char *theme_name = engine_gui_theme_name_by_index((uint8_t)sel);
    ESP_LOGI(TAG, "theme changed: %s", theme_name);
    engine_gui_set_theme(theme_name);
    /* 切主题后按新主题色重新注入 tab 按钮样式 */
    setting_apply_tab_button_styles();
}

static void setting_on_screen_cb(lv_event_t *e)
{
    (void)e;
    if (objects.setting_on_screen == NULL) {
        return;
    }
    uint16_t sel = lv_dropdown_get_selected(objects.setting_on_screen);
    ESP_LOGI(TAG, "boot screen changed: %u", (unsigned)sel);
    service_nvs_set_boot_screen_index((uint8_t)sel);
}

static void setting_time2idle_cb(lv_event_t *e)
{
    (void)e;
    if (objects.setting_time2idle == NULL) {
        return;
    }
    uint16_t sel = lv_dropdown_get_selected(objects.setting_time2idle);
    ESP_LOGI(TAG, "time2idle changed: %u", (unsigned)sel);
    service_power_idle_set_timeout_index((uint8_t)sel);
}

static void setting_auto_sleep_cb(lv_event_t *e)
{
    (void)e;
    if (objects.setting_auto_sleep == NULL) {
        return;
    }
    bool checked = lv_obj_has_state(objects.setting_auto_sleep, LV_STATE_CHECKED);
    ESP_LOGI(TAG, "auto sleep: %d", (int)checked);
    service_power_idle_set_auto_sleep_enabled(checked);
}

static void setting_wifi_reset_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "reset wifi");
    service_wifi_disconnect();
    service_nvs_set_wifi_ssid("");
    service_nvs_set_wifi_password("");
    setting_update_wifi_tip();
}

/* -------------------- WiFi 配置面板 -------------------- */

/* 按当前状态刷新连接提示文案 */
static void setting_update_wifi_tip(void)
{
    if (objects.setting_wifi_connect_tip == NULL) {
        return;
    }
    char buf[96];
    if (!service_nvs_get_feature_flag(SERVICE_NVS_FLAG_WIFI_ENABLED)) {
        lv_label_set_text(objects.setting_wifi_connect_tip,
                          "WiFi 已关闭");
        return;
    }
    char ssid[SERVICE_NVS_SSID_MAX_LEN] = {0};
    service_nvs_get_wifi_ssid(ssid, sizeof(ssid));
    if (ssid[0] == '\0') {
        lv_label_set_text(objects.setting_wifi_connect_tip,
                          "扫码连接 HammySetup（密码：12345678）\n浏览器访问 http://192.168.4.1");
    } else if (service_wifi_is_connected()) {
        snprintf(buf, sizeof(buf), "已连接 %s", ssid);
        lv_label_set_text(objects.setting_wifi_connect_tip, buf);
    } else {
        snprintf(buf, sizeof(buf), "已配置 %s，连接中…", ssid);
        lv_label_set_text(objects.setting_wifi_connect_tip, buf);
    }
}

/* 设置界面 WiFi 面板：
 * setting_btn_wifi_detail → 显示面板 + 启动 AP 配网服务器
 * wifi_set_panel_return → 隐藏面板 + 停止 AP 服务器
 * 不再使用 toggle，避免面板状态错乱 */
static void setting_wifi_detail_show_cb(lv_event_t *e)
{
    (void)e;
    if (objects.wifi_set_panel == NULL) {
        return;
    }
    lv_obj_clear_flag(objects.wifi_set_panel, LV_OBJ_FLAG_HIDDEN);
    if (service_nvs_get_feature_flag(SERVICE_NVS_FLAG_WIFI_ENABLED)) {
        if (service_wifi_start_ap() != ESP_OK) {
            ESP_LOGW(TAG, "config AP start failed");
        }
    }
    setting_update_wifi_tip();
}

static void setting_wifi_detail_hide_cb(lv_event_t *e)
{
    (void)e;
    if (objects.wifi_set_panel == NULL) {
        return;
    }
    lv_obj_add_flag(objects.wifi_set_panel, LV_OBJ_FLAG_HIDDEN);
    if (service_nvs_get_feature_flag(SERVICE_NVS_FLAG_WIFI_ENABLED)) {
        service_wifi_stop_ap();
    }
    setting_update_wifi_tip();
}

/* 显示反向硬开关：持久化 + 立即应用（LVGL 旋转 90°/270°，渲染与触摸随动） */
static void setting_invert_display_cb(lv_event_t *e)
{
    (void)e;
    if (objects.setting_invert_display == NULL) {
        return;
    }
    bool checked = lv_obj_has_state(objects.setting_invert_display, LV_STATE_CHECKED);
    ESP_LOGI(TAG, "invert display: %d", (int)checked);
    service_nvs_set_feature_flag(SERVICE_NVS_FLAG_INVERT_DISPLAY, checked);
    engine_gui_set_display_inverted(checked);
}

/* WiFi 总开关：持久化 + 经 expand 芯片直接驱动 C6 电源。
 * 运行时断电会使 esp_hosted 总线失效，需重启恢复，文案如实提示 */
static void setting_wifi_switch_cb(lv_event_t *e)
{
    (void)e;
    if (objects.setting_wifi_switch == NULL) {
        return;
    }
    bool checked = lv_obj_has_state(objects.setting_wifi_switch, LV_STATE_CHECKED);
    ESP_LOGI(TAG, "wifi switch: %d", (int)checked);

    /* 持久化开关状态：下次开机 service_wifi_boot 据此决定是否初始化协议栈 */
    service_nvs_set_feature_flag(SERVICE_NVS_FLAG_WIFI_ENABLED, checked);

    if (checked) {
        /* 打开：全局重新启用。“开机被开关跳过初始化”的场景下，这里
         * 必须完整拉起协议栈，否则会出现“开关打开了但 WiFi 永远不可用”
         * 的比崩溃更严重的状态。service_wifi_boot 幂等：已初始化则直通。 */
        service_wifi_set_user_disabled(false);
        esp_err_t ret = service_wifi_boot();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "wifi boot on enable failed: %d", ret);
        }
        bool panel_visible = (objects.wifi_set_panel != NULL &&
                              !lv_obj_has_flag(objects.wifi_set_panel, LV_OBJ_FLAG_HIDDEN));
        if (panel_visible) {
            /* 面板展开：按需求启动配网服务器（start_ap 内部幂等含 init） */
            if (service_wifi_start_ap() != ESP_OK) {
                ESP_LOGW(TAG, "config AP start failed");
            }
        } else {
            /* 面板未展开：STA 场景立即重连（重置退避） */
            service_wifi_reconnect_now();
        }
    } else {
        /* 关闭：全局软关断（抑制重连风暴 + 断开 + 停 AP）。
         * 不在线控制 C6 电源：hosted/lwip 运转中断电会在 netif 拆除时
         * 触发 Instruction access fault，且断电后无法可靠恢复；
         * C6 电源由开机统一供给，本开关只做软关断。 */
        service_wifi_set_user_disabled(true);
        bool panel_visible = (objects.wifi_set_panel != NULL &&
                              !lv_obj_has_flag(objects.wifi_set_panel, LV_OBJ_FLAG_HIDDEN));
        if (panel_visible) {
            service_wifi_stop_ap();
        }
        service_wifi_disconnect();
    }
    setting_update_wifi_tip();
}

/* -------------------- 系统重置（长按 5s 触发 factory reset） -------------------- */

/* Why 不走 LVGL 默认 LV_EVENT_LONG_PRESSED：
 *   LV_INDEV_DEF_LONG_PRESS_TIME 默认仅 ~400ms，改全局值会影响所有按钮
 *   的长按触发。对"恢复出厂"这种不可逆操作，需要显式 5s 门槛和倒计时
 *   提示，所以用 PRESSING 事件按实际经过时间累积计算，RELEASE/CANCEL
 *   清零，到点触发，并在按钮子 label 上实时显示倒计时。 */

#define SETTING_SYSRESET_HOLD_MS   5000

static uint32_t s_sysreset_press_start_ms = 0;   /* 0 = 未按下 */
static lv_obj_t *s_sysreset_btn_label = NULL;    /* setting_btn_system_reset 子 label，懒解析 */
static char s_sysreset_btn_orig_text[16] = {0};  /* 原始按钮文字，松手还原 */

/** @brief 在"再按 N 秒重置…"和"长按重置"原始文案之间切换按钮 label */
static void setting_sysreset_update_label(int32_t remain_sec)
{
    if (s_sysreset_btn_label == NULL) {
        /* 懒解析：按钮内只有一个 lv_label 子对象（EEZ 生成的 child） */
        if (objects.setting_btn_system_reset == NULL) return;
        lv_obj_t *child = lv_obj_get_child(objects.setting_btn_system_reset, 0);
        if (child == NULL) return;
        if (!lv_obj_check_type(child, &lv_label_class)) return;
        s_sysreset_btn_label = child;
        const char *orig = lv_label_get_text(s_sysreset_btn_label);
        if (orig != NULL) {
            strncpy(s_sysreset_btn_orig_text, orig, sizeof(s_sysreset_btn_orig_text) - 1);
        }
    }
    if (s_sysreset_btn_label == NULL) return;

    if (remain_sec <= 0) {
        if (s_sysreset_btn_orig_text[0] != '\0') {
            lv_label_set_text_static(s_sysreset_btn_label, s_sysreset_btn_orig_text);
        }
        return;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "再按 %lds …", (long)remain_sec);
    lv_label_set_text(s_sysreset_btn_label, buf);
}

static void setting_system_reset_cb(lv_event_t *e)
{
    (void)e;
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        s_sysreset_press_start_ms = lv_tick_get();
        setting_sysreset_update_label(SETTING_SYSRESET_HOLD_MS / 1000);
        return;
    }
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST || code == LV_EVENT_CANCEL) {
        s_sysreset_press_start_ms = 0;
        setting_sysreset_update_label(0);
        return;
    }
    if (code == LV_EVENT_PRESSING) {
        if (s_sysreset_press_start_ms == 0) {
            /* 丢失过 PRESSED 事件的兜底：从这次 PRESSING 开始算 */
            s_sysreset_press_start_ms = lv_tick_get();
        }
        uint32_t elapsed = lv_tick_elaps(s_sysreset_press_start_ms);
        if (elapsed >= SETTING_SYSRESET_HOLD_MS) {
            /* 到点：防重入（设回 0）后触发；factory_reset 内部 esp_restart 不返回 */
            s_sysreset_press_start_ms = 0;
            ESP_LOGW(TAG, "system reset button held %u ms -> factory reset", (unsigned)elapsed);
            setting_sysreset_update_label(0);
            service_nvs_factory_reset();
            return;
        }
        /* 倒计时按秒跳字：剩余时间向上取整，避免一开始只剩 4s 的视觉心理落差 */
        uint32_t remain = (SETTING_SYSRESET_HOLD_MS - elapsed + 999) / 1000;
        static uint32_t s_last_shown = 0;
        if (remain != s_last_shown) {
            s_last_shown = remain;
            setting_sysreset_update_label((int32_t)remain);
        }
        return;
    }
}

/* 设置屏加载时刷新控件值为 NVS 当前值（原经 EEZ SCREEN_LOADED 链路触发，
 * 事件体系重构后改为后端显式注册 LVGL 屏幕事件） */
static void setting_screen_loaded_cb(lv_event_t *e)
{
    (void)e;
    service_page_setting_on_screen_loaded();
}

/* EEZ TabView 控件不暴露 tab 按钮样式配置，由后端注入。
 *
 * LVGL 9.5.0 TabView 结构：tab_bar 内每个 tab 按钮是 lv_button，内含 lv_label 子控件。
 * 默认主题给按钮添加了 bg_color_primary 样式（含白色文字色），会覆盖继承来的文字颜色。
 * 因此必须先清除默认主题样式，再同时给按钮和 label 设置背景色/文字颜色。
 *
 * 未选中态：BG_PRIMARY 背景 + TEXT_SECONDARY 文字（融入背景）
 * 选中态：  BG_SECONDARY 背景 + TEXT_PRIMARY 文字（突出）
 *
 * 必须在每次屏幕加载后重新应用：屏幕懒创建，开机时 objects.setting_tab 为 NULL。 */
static void setting_apply_tab_button_styles(void)
{
    lv_color_t bg_default   = engine_gui_theme_color(COLOR_BG_PRIMARY);
    lv_color_t bg_checked   = engine_gui_theme_color(COLOR_CARD);
    lv_color_t text_default = engine_gui_theme_color(COLOR_TEXT_SECONDARY);
    lv_color_t text_checked = engine_gui_theme_color(COLOR_TEXT_PRIMARY);

    if (objects.setting_tab == NULL) {
        return;
    }

    lv_obj_t *tab_bar = lv_tabview_get_tab_bar(objects.setting_tab);
    if (tab_bar == NULL) {
        return;
    }
    lv_obj_set_style_bg_color(tab_bar, bg_default, 0);

    uint32_t tab_count = lv_tabview_get_tab_count(objects.setting_tab);
    for (uint32_t i = 0; i < tab_count; i++) {
        lv_obj_t *btn = lv_tabview_get_tab_button(objects.setting_tab, (int32_t)i);
        if (btn == NULL) {
            continue;
        }
        /* 按钮底部线条去除 */
        lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN | LV_STATE_CHECKED);

        /* 按钮背景色 */
        lv_obj_set_style_bg_color(btn, bg_default, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn, bg_checked, LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_bg_opa(btn, LV_OPA_100, LV_PART_MAIN | LV_STATE_CHECKED);

        /* 按钮文字色（label 可继承此文字色，但显式设置更可靠） */
        lv_obj_set_style_text_color(btn, text_default, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(btn, text_checked, LV_PART_MAIN | LV_STATE_CHECKED);
    }
}

static uint8_t setting_theme_index_from_name(const char *name)
{
    if (name == NULL) {
        return 0;
    }
    if (strcmp(name, "starrynight") == 0) {
        return 1;
    }
    return 0;
}

void service_page_setting_init(void)
{
    ESP_LOGI(TAG, "init");

    lvgl_port_lock(portMAX_DELAY);

    if (objects.setting != NULL) {
        lv_obj_add_event_cb(objects.setting, setting_screen_loaded_cb, LV_EVENT_SCREEN_LOADED, NULL);
    }
    if (objects.setting_btn_home != NULL) {
        lv_obj_add_event_cb(objects.setting_btn_home, setting_home_cb, LV_EVENT_CLICKED, NULL);
    }
    if (objects.setting_btn_about != NULL) {
        lv_obj_add_event_cb(objects.setting_btn_about, setting_about_cb, LV_EVENT_CLICKED, NULL);
    }
    if (objects.setting_language != NULL) {
        lv_obj_add_event_cb(objects.setting_language, setting_language_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
    if (objects.setting_slide_brightness != NULL) {
        lv_obj_add_event_cb(objects.setting_slide_brightness, setting_brightness_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(objects.setting_slide_brightness, setting_brightness_cb, LV_EVENT_RELEASED, NULL);
    }
    if (objects.setting_slide_volume != NULL) {
        lv_obj_add_event_cb(objects.setting_slide_volume, setting_volume_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
    if (objects.setting_theme != NULL) {
        lv_obj_add_event_cb(objects.setting_theme, setting_theme_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
    if (objects.setting_on_screen != NULL) {
        lv_obj_add_event_cb(objects.setting_on_screen, setting_on_screen_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
    if (objects.setting_time2idle != NULL) {
        lv_obj_add_event_cb(objects.setting_time2idle, setting_time2idle_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
    if (objects.setting_auto_sleep != NULL) {
        lv_obj_add_event_cb(objects.setting_auto_sleep, setting_auto_sleep_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
    if (objects.setting_invert_display != NULL) {
        lv_obj_add_event_cb(objects.setting_invert_display, setting_invert_display_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
    if (objects.setting_btn_wifi_detail != NULL) {
        lv_obj_add_event_cb(objects.setting_btn_wifi_detail, setting_wifi_detail_show_cb, LV_EVENT_CLICKED, NULL);
    }
    if (objects.setting_btn_ftp != NULL) {
        lv_obj_add_event_cb(objects.setting_btn_ftp, setting_ftp_cb, LV_EVENT_CLICKED, NULL);
    }
    if (objects.wifi_set_panel_return != NULL) {
        lv_obj_add_event_cb(objects.wifi_set_panel_return, setting_wifi_detail_hide_cb, LV_EVENT_CLICKED, NULL);
    }
    if (objects.setting_wifi_switch != NULL) {
        lv_obj_add_event_cb(objects.setting_wifi_switch, setting_wifi_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
    if (objects.setting_btn_wifi_reset != NULL) {
        lv_obj_add_event_cb(objects.setting_btn_wifi_reset, setting_wifi_reset_cb, LV_EVENT_LONG_PRESSED, NULL);
    }
    if (objects.setting_btn_system_reset != NULL) {
        /* 不用 LV_EVENT_LONG_PRESSED：默认 400ms 对出厂重置太短；用
         * LV_EVENT_PRESSING 每帧累计时间，阈值 5s 并显示倒计时。
         * PRESSED 登记起点，RELEASED / PRESS_LOST / CANCELLED 清零。 */
        lv_obj_add_event_cb(objects.setting_btn_system_reset, setting_system_reset_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(objects.setting_btn_system_reset, setting_system_reset_cb, LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(objects.setting_btn_system_reset, setting_system_reset_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(objects.setting_btn_system_reset, setting_system_reset_cb, LV_EVENT_PRESS_LOST, NULL);
        lv_obj_add_event_cb(objects.setting_btn_system_reset, setting_system_reset_cb, LV_EVENT_CANCEL, NULL);
    }

    lvgl_port_unlock();
}

void service_page_setting_event(lv_event_t *e)
{
    (void)e;
}

void service_page_setting_apply_saved_theme(void)
{
    /* 开机阶段：按持久化主题刷新 EEZ 主题色与 launcher 背景（set_theme 内部
     * 含 NVS 回写与 launcher 背景刷新）。下拉选中项在 setting 屏加载时由
     * on_screen_loaded 同步，此处不碰控件，避免 boot 阶段时序问题。 */
    engine_gui_set_theme(engine_gui_get_theme_name());
}

void service_page_setting_on_screen_loaded(void)
{
    lvgl_port_lock(portMAX_DELAY);

    /* tab 按钮样式注入：每次进入设置屏均重新应用 */
    setting_apply_tab_button_styles();

    if (objects.setting_slide_brightness != NULL) {
        int32_t brightness = (int32_t)service_nvs_get_brightness();
        lv_slider_set_value(objects.setting_slide_brightness, brightness, LV_ANIM_OFF);
        setting_update_brightness_label(brightness);
    }

    if (objects.setting_slide_volume != NULL) {
        int32_t volume = service_nvs_get_volume();
        lv_slider_set_value(objects.setting_slide_volume, volume, LV_ANIM_OFF);
        setting_update_volume_label(volume);
    }

    if (objects.setting_language != NULL) {
        char lang_id[SERVICE_I18N_LANG_ID_MAX_LEN] = {0};
        service_nvs_get_language(lang_id, sizeof(lang_id));
        uint16_t sel = 0;
        for (int i = 0; i < SETTING_LANG_COUNT; i++) {
            if (strcmp(lang_id, s_lang_ids[i]) == 0) {
                sel = (uint16_t)i;
                break;
            }
        }
        lv_dropdown_set_selected(objects.setting_language, sel);
    }

    if (objects.setting_theme != NULL) {
        uint8_t idx = setting_theme_index_from_name(engine_gui_get_theme_name());
        lv_dropdown_set_selected(objects.setting_theme, idx);
    }

    if (objects.setting_on_screen != NULL) {
        uint8_t idx = service_nvs_get_boot_screen_index();
        if (idx >= 5) {
            idx = 0;
        }
        lv_dropdown_set_selected(objects.setting_on_screen, idx);
    }

    if (objects.setting_time2idle != NULL) {
        uint8_t idx = service_power_idle_get_timeout_index();
        if (idx >= 6) {
            idx = 2;
        }
        lv_dropdown_set_selected(objects.setting_time2idle, idx);
    }

    if (objects.setting_auto_sleep != NULL) {
        bool checked = service_power_idle_get_auto_sleep_enabled();
        if (checked) {
            lv_obj_add_state(objects.setting_auto_sleep, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(objects.setting_auto_sleep, LV_STATE_CHECKED);
        }
    }

    if (objects.setting_invert_display != NULL) {
        if (service_nvs_get_feature_flag(SERVICE_NVS_FLAG_INVERT_DISPLAY)) {
            lv_obj_add_state(objects.setting_invert_display, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(objects.setting_invert_display, LV_STATE_CHECKED);
        }
    }

    /* WiFi 面板每次进入默认收起（服务器不常驻），同步开关与提示文案 */
    if (objects.wifi_set_panel != NULL) {
        lv_obj_add_flag(objects.wifi_set_panel, LV_OBJ_FLAG_HIDDEN);
    }
    if (objects.setting_wifi_switch != NULL) {
        bool wifi_enabled = service_nvs_get_feature_flag(SERVICE_NVS_FLAG_WIFI_ENABLED);
        if (wifi_enabled) {
            lv_obj_add_state(objects.setting_wifi_switch, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(objects.setting_wifi_switch, LV_STATE_CHECKED);
        }
    }
    setting_update_wifi_tip();

    lvgl_port_unlock();
}

void service_page_setting_tick(void)
{
    /* 每秒周期（service_page 定时器，LVGL 锁内）：面板展开时跟随连接状态刷文案 */
    if (objects.wifi_set_panel != NULL &&
        !lv_obj_has_flag(objects.wifi_set_panel, LV_OBJ_FLAG_HIDDEN)) {
        setting_update_wifi_tip();
    }
}

void service_page_setting_refresh_tab_styles(void)
{
    /* 切主题后按需重新注入 tab 按钮样式（外部路径如 MCP 调用）；
     * 若设置屏尚未创建则跳过，screen loaded 时会自行应用。 */
    lvgl_port_lock(portMAX_DELAY);
    service_page_setting_refresh_tab_styles_locked();
    lvgl_port_unlock();
}

/* 已持有锁的版本：供 engine_gui_set_theme 等已持锁的调用者使用 */
void service_page_setting_refresh_tab_styles_locked(void)
{
    if (objects.setting_tab != NULL) {
        setting_apply_tab_button_styles();
    }

    /* 同步主题下拉菜单选中状态 */
    if (objects.setting_theme != NULL) {
        uint8_t idx = setting_theme_index_from_name(engine_gui_get_theme_name());
        lv_dropdown_set_selected(objects.setting_theme, idx);
    }
}
