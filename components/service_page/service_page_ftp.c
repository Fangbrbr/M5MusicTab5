/**
 * @file service_page_ftp.c
 * @brief FTP 屏幕后端事件处理
 *
 * FTP 为独占系统屏（service_page 体系，非 App）：进入时禁熄屏、暂停小智
 * 语音、启动 FTP 服务并锁切屏/App 启动；返回按钮任何时候可点，主动中止
 * 传输、断开客户端并解除全部锁定后回设置屏。
 */

#include "service_page_ftp.h"
#include "screens.h"
#include "engine_gui.h"
#include "app_manager.h"
#include "service_ftp.h"
#include "service_page_setting.h"
#include "service_power.h"
#include "service_wifi.h"
#include "service_xiaozhi.h"
#include "service_i18n.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "stdio.h"

static const char *TAG = "service_page_ftp";

/* 100ms 进度条定时器：常驻创建，cb 内按屏/传输状态空转返回 */
static lv_timer_t *s_progress_timer = NULL;

/* 进入页面时 service_ftp_start 的结果：错误态显示用，失败不崩仍可退出 */
static esp_err_t s_start_err = ESP_OK;

static void ftp_back_cb(lv_event_t *e);
static void ftp_screen_loaded_cb(lv_event_t *e);
static void ftp_progress_timer_cb(lv_timer_t *timer);
static void ftp_refresh_status(const service_ftp_status_t *st);

void service_page_ftp_init(void)
{
    ESP_LOGI(TAG, "init");

    lvgl_port_lock(portMAX_DELAY);
    /* 系统屏的 SCREEN_LOADED 不经 EEZ action 分发，各屏在 init 自注册回调
     * （同 setting 屏 service_page_setting.c:451 的约定）；缺了它进入逻辑永不执行 */
    if (objects.ftp != NULL) {
        lv_obj_add_event_cb(objects.ftp, ftp_screen_loaded_cb,
                            LV_EVENT_SCREEN_LOADED, NULL);
    }
    if (objects.ftp_btn_back2setting != NULL) {
        lv_obj_add_event_cb(objects.ftp_btn_back2setting, ftp_back_cb,
                          LV_EVENT_CLICKED, NULL);
    }
    if (s_progress_timer == NULL) {
        s_progress_timer = lv_timer_create(ftp_progress_timer_cb, 100, NULL);
    }
    lvgl_port_unlock();
}

void service_page_ftp_event(lv_event_t *e)
{
    (void)e;
}

void service_page_ftp_on_screen_loaded(void)
{
    ESP_LOGI(TAG, "ftp screen enter");

    /* 独占清场：engine_gui_on_screen_loaded 尾部的系统屏 kill 实际不可达
     * （SCREEN_LOADED 不进 EEZ action），此处自行异步 kill 当前 App；
     * kill 走 request 不受 launch 锁影响，与后边上锁无竞态 */
    if (app_manager_get_active() != NULL) {
        app_manager_request_kill_active();
    }

    /* 顺序即契约：先接管系统行为（熄屏/语音），再启动服务，最后上锁——
     * 上锁后 engine_gui_switch_screen 全量丢弃，本页退出路径先解锁再切屏 */
    service_power_idle_set_enabled(false);
    service_xiaozhi_set_suspended(true);
    s_start_err = service_ftp_start();
    if (s_start_err != ESP_OK) {
        ESP_LOGW(TAG, "ftp start failed: %d（页面显示错误态，仍可退出）", (int)s_start_err);
    }
    app_manager_set_launch_locked(true);
    engine_gui_set_screen_locked(true);

    /* 进入即刷一次，不等下一秒定时器；on_screen_loaded 在 LVGL 上下文 */
    service_page_ftp_tick();
}

void service_page_ftp_tick(void)
{
    /* 调用方锁状态不一（1Hz 定时器已持锁 / on_screen_loaded 未持锁），
     * esp_lvgl_port 递归互斥锁双持无害，统一在此收口 */
    lvgl_port_lock(portMAX_DELAY);
    service_ftp_status_t st;
    service_ftp_get_status(&st);
    ftp_refresh_status(&st);
    lvgl_port_unlock();
}

/* SCREEN_LOADED → 进入逻辑（EEZ 不参与系统屏事件分发，C 侧自注册） */
static void ftp_screen_loaded_cb(lv_event_t *e)
{
    (void)e;
    service_page_ftp_on_screen_loaded();
}

static void ftp_back_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "exit ftp page");

    /* 先停服务（中止传输、断客户端、释缓冲），再解锁，最后恢复系统行为 */
    service_ftp_stop();
    /* FTP 可能增删过 SD 文件：登记音源重扫/校验（异步，process 消化） */
    service_page_setting_sf2_on_sd_changed();
    engine_gui_set_screen_locked(false);
    app_manager_set_launch_locked(false);
    service_power_idle_set_enabled(true);
    service_xiaozhi_set_suspended(false);
    engine_gui_switch_screen("setting");
}

/* 100ms 周期（LVGL 锁内）：仅传输中刷进度条与字节计数，其余一律空转 */
static void ftp_progress_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (objects.ftp == NULL || lv_screen_active() != objects.ftp) {
        return;
    }
    service_ftp_status_t st;
    service_ftp_get_status(&st);
    if (st.state != SERVICE_FTP_STATE_TRANSFERRING) {
        return;
    }

    if (objects.ftp_bar_progress != NULL) {
        int32_t pct;
        if (st.file_size > 0) {
            pct = (int32_t)((uint64_t)st.bytes_done * 100 / st.file_size);
            if (pct > 100) {
                pct = 100;
            }
        } else {
            /* 上传长度服务端不可预知（STOR file_size=0）：锯齿滚动指示
             * 传输活跃，真实进度看 label 的已传字节数 */
            pct = (int32_t)((st.bytes_done >> 14) % 100);
        }
        lv_bar_set_value(objects.ftp_bar_progress, pct, LV_ANIM_OFF);
    }
    if (objects.ftp_label_file != NULL && st.file_name[0] != '\0') {
        /* Trap: 文件名上限 255 + 字节数后缀，96 会触发 -Werror=format-truncation */
        char buf[320];
        if (st.file_size > 0) {
            snprintf(buf, sizeof(buf), _("%s  %lu/%lu 字节"), st.file_name,
                     (unsigned long)st.bytes_done, (unsigned long)st.file_size);
        } else {
            /* STOR 上传总字节未知，只显示已传 */
            snprintf(buf, sizeof(buf), _("%s  %lu 字节"), st.file_name,
                     (unsigned long)st.bytes_done);
        }
        lv_label_set_text(objects.ftp_label_file, buf);
    }
}

/* 全量状态刷新（LVGL 锁内）：1Hz tick 与进入页面时调用 */
static void ftp_refresh_status(const service_ftp_status_t *st)
{
    if (objects.ftp_label_ip != NULL) {
        char ip[16];
        if (service_wifi_get_sta_ip_str(ip, sizeof(ip)) == ESP_OK) {
            char buf[64];
            snprintf(buf, sizeof(buf), _("ftp://%s  账号密码: musicpad"), ip);
            lv_label_set_text(objects.ftp_label_ip, buf);
        } else {
            lv_label_set_text(objects.ftp_label_ip, _("WiFi 未连接"));
        }
    }

    if (objects.ftp_label_state != NULL) {
        char buf[64];
        const char *text = "";
        if (s_start_err != ESP_OK) {
            snprintf(buf, sizeof(buf), _("服务启动失败 (%d)，请检查 SD 卡"), (int)s_start_err);
            text = buf;
        } else {
            switch (st->state) {
            case SERVICE_FTP_STATE_OFF:
                text = _("服务未启动");
                break;
            case SERVICE_FTP_STATE_LISTENING:
                text = _("等待客户端连接...");
                break;
            case SERVICE_FTP_STATE_CONNECTED:
                snprintf(buf, sizeof(buf), _("已连接: %s"), st->client_ip);
                text = buf;
                break;
            case SERVICE_FTP_STATE_TRANSFERRING:
                snprintf(buf, sizeof(buf), _("传输中: %s"), st->client_ip);
                text = buf;
                break;
            default:
                break;
            }
        }
        lv_label_set_text(objects.ftp_label_state, text);
    }

    if (objects.ftp_label_file != NULL) {
        /* Trap: 文件名上限 255 + 字节数后缀，96 会触发 -Werror=format-truncation */
        char buf[320];
        buf[0] = '\0';
        if (st->file_name[0] != '\0') {
            if (st->file_size > 0) {
                snprintf(buf, sizeof(buf), _("%s  %lu/%lu 字节"), st->file_name,
                         (unsigned long)st->bytes_done, (unsigned long)st->file_size);
            } else {
                snprintf(buf, sizeof(buf), _("%s  %lu 字节"), st->file_name,
                         (unsigned long)st->bytes_done);
            }
        }
        lv_label_set_text(objects.ftp_label_file, buf);
    }

    if (objects.ftp_bar_progress != NULL) {
        int32_t pct = 0;
        if (st->file_size > 0) {
            pct = (int32_t)((uint64_t)st->bytes_done * 100 / st->file_size);
            if (pct > 100) {
                pct = 100;
            }
        }
        lv_bar_set_value(objects.ftp_bar_progress, pct, LV_ANIM_OFF);
    }
}
