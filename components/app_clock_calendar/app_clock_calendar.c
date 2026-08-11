/**
 * @file app_clock_calendar.c
 * @brief 时钟日历 App：三面板（时钟/月历/定时器）+ 网络一言/天气 + 农历黄历
 */

#include "app_clock_calendar.h"
#include "app_manager.h"
#include "cnlunar.h"
#include "engine_gui.h"
#include "engine_midi.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "service_http_client.h"
#include "service_nvs.h"
#include "service_rtc.h"
#include "service_wifi.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = "app_clock_calendar";

#define CLOCK_REFRESH_INTERVAL_US   1000000  /* 时钟面板 1s 刷新节流 */
#define CAL_REFRESH_INTERVAL_US     2000000  /* 月历面板 2s 刷新节流 */
#define HTTP_TASK_STACK             4096
#define HTTP_TASK_PRIORITY          3
#define NETWORK_AUTO_REFRESH_US     (30 * 60 * 1000000ULL)  /* 一言/天气自动刷新周期：30 分钟 */
#define RETRY_BACKOFF_INIT_US       (30 * 1000000ULL)        /* 失败重试初始间隔：30 秒 */
#define RETRY_BACKOFF_MAX_US        (5 * 60 * 1000000ULL)    /* 失败重试最大间隔：5 分钟 */
#define HITOKOTO_URL                "https://v1.hitokoto.cn/?c=k&encode=text"   /* 哲学 */
/*
    句子类型（参数c=?）
    参数	说明
    a	动画
    b	漫画
    c	游戏
    d	文学
    e	原创
    f	来自网络
    g	其他
    h	影视
    i	诗词
    j	网易云
    k	哲学
    l	抖机灵
*/
#define WEATHER_URL                 "https://uapis.cn/api/v1/misc/weather?forecast=true&lang=zh"
#define WEATHER_URL_EN              "https://uapis.cn/api/v1/misc/weather?forecast=true&lang=en"
#define NEWS_URL                    "https://3g.163.com/touch/reconstruct/article/list/BBM54PGAwangning/%d-%d.html"
#define NETWORK_RESP_BUF_SIZE       12288   /* 天气/新闻共享 HTTP 响应缓冲 */
#define NEWS_TITLE_COUNT            3
#define NEWS_TITLE_LEN              96
#define NEWS_FETCH_INTERVAL_US      (30 * 60 * 1000000ULL)  /* 新闻更新周期：30 分钟 */
#define NEWS_SHOW_DURATION_US       (10 * 1000000ULL)        /* 新闻面板单次显示时长：10 秒 */
#define NEWS_HIDE_DURATION_US       (5 * 1000000ULL)         /* 新闻面板单次隐藏时长：5 秒 */

#define TIMER_BELL_NOTE             75
#define TIMER_BELL_CHANNEL          9
#define TIMER_BELL_COUNT            3
#define HUANGLI_PAGE_COUNT          4   /* 黄历翻页总数：宜忌/农历干支/节气星宿/冲煞纳音 */

/* -------------------- 类型定义 -------------------- */

typedef enum {
    PANEL_CLOCK = 0,
    PANEL_CALENDAR,
    PANEL_TIMER,
} clock_panel_t;

typedef enum {
    TIMER_CONFIG = 0,
    TIMER_RUNNING,
    TIMER_PAUSED,
} timer_state_t;

/**
 * HTTP 缓冲当前占用者。
 * 天气与新闻共享同一块大缓冲，通过 owner 串行化，避免并发请求覆盖响应数据。
 */
typedef enum {
    HTTP_OWNER_NONE = 0,
    HTTP_OWNER_WEATHER,
    HTTP_OWNER_NEWS,
} http_owner_t;

typedef struct {
    char city[32];          // 市
    char district[32];      // 区
    char date[16];          // 日期
    char week[16];          // 星期
    char weather[16];       // 当前天气
    char wind_dir[16];      // 当前风向
    char wind_power[8];     // 当前风力
    int temperature;        // 当前温度
    int humidity;           // 当前湿度
    int pop;                // 今日降水概率
    int cloud;              // 当前云量
    int uv_index;           // 当前紫外线指数

    char day2_week[16];          // 明天星期
    char day2_weather[16];          // 明天天气
    int day2_temperature_high;      // 明天最高温度
    int day2_temperature_low;       // 明天最低温度
    int day2_cloud;                 // 明天云量
    int day2_pop;                   // 明天降水概率
    int day2_uv_index;              // 明天紫外线指数

    char day3_week[16];          // 后天星期
    char day3_weather[16];          // 后天天气
    int day3_temperature_high;      // 后天最高温度
    int day3_temperature_low;       // 后天最低温度
    int day3_cloud;                 // 后天云量
    int day3_pop;                   // 后天降水概率
    int day3_uv_index;              // 后天紫外线指数

    bool valid;             // 数据是否有效
} weather_data_t;

typedef enum {
    REFRESH_TIME     = 1 << 0, /* 时钟面板：大时间、日期、农历 */
    REFRESH_CALENDAR = 1 << 1, /* 月历面板：今日、周数、进度、黄历 */
    REFRESH_TIMER    = 1 << 2, /* 定时器面板：剩余/总时长 */
    REFRESH_NETWORK  = 1 << 3, /* 网络结果：一言、天气 */
} clock_refresh_part_t;

typedef struct {
    /* 时钟面板 */
    char clock_bigtime[16];
    char clock_date[64];
    char clock_lunar[96];
    char clock_12h_label[8];
    bool clock_12h_visible;
    char hitokoto[128];
    bool hitokoto_visible;
    /* 天气 */
    bool weather_visible;
    char weather_today_text[64];
    char weather_today_text_1[256];
    char weather_location[70];
    char weather_temp_today[32];
    char weather_humi_today[32];
    char weather_day2_text[32];
    char weather_day2_text_1[64];
    char weather_day3_text[32];
    char weather_day3_text_1[64];
    /* 新闻 */
    bool news_visible;
    char news_titles[NEWS_TITLE_COUNT][NEWS_TITLE_LEN];
    /* 月历面板 */
    uint16_t cal_year;
    uint8_t cal_month;
    uint8_t cal_day;
    char cal_today[16];
    char cal_week[32];
    char cal_progress[64];
    int cal_progress_pct;
    char cal_huangli_1[64];
    char cal_huangli_2[192];
    char cal_huangli_3[64];
    char cal_huangli_4[192];
    char cal_huangli_5[128];
    char cal_huangli_6[128];
    /* 定时器面板 */
    char timer_pv[16];
    char timer_sv[16];
    char timer_bell_text[8];
} clock_display_state_t;

typedef struct {
    clock_panel_t active_panel;
    bool use_12h;
    /* 新闻显隐轮换（会话内状态） */
    bool news_visible;
    int64_t news_toggle_next_us;
    /* 定时器 */
    timer_state_t timer_state;
    uint32_t timer_total_s;
    uint32_t timer_remain_s;
    int64_t timer_next_tick_us;
    bool timer_bell;
    uint8_t bell_rings_left;
    bool bell_note_on;      /* 当前铃声是否处于 NOTE_ON 阶段，60ms 后切 NOTE_OFF */
    int64_t bell_next_us;
    /* 刷新节流 */
    int64_t last_cal_us;
    /* 黄历翻页（0..CLOCK_HUANGLI_PAGE_MAX） */
    uint8_t huangli_page;
    /* 月历面板手动选中的日期，有效时替代当前时间计算黄历 */
    uint16_t cal_sel_year;
    uint8_t cal_sel_month;
    uint8_t cal_sel_day;
    bool cal_sel_valid;
} clock_state_t;

/* -------------------- UI 控件结构体 -------------------- */

typedef struct {
    lv_obj_t *btn_home;
    lv_obj_t *btn_set;
    /* 时钟面板 */
    lv_obj_t *panel_clock;
    lv_obj_t *clock_12h_label;
    lv_obj_t *clock_bigtime;
    lv_obj_t *clock_date;
    lv_obj_t *clock_lunar;
    lv_obj_t *clock_hitokoto;
    /* 天气 */
    lv_obj_t *panel_weather;
    lv_obj_t *weather_today_text;
    lv_obj_t *weather_today_text_1;
    lv_obj_t *weather_location;
    lv_obj_t *weather_temp;
    lv_obj_t *weather_humi;
    lv_obj_t *weather_day2;
    lv_obj_t *weather_day2_1;
    lv_obj_t *weather_day3;
    lv_obj_t *weather_day3_1;
    /* 新闻面板 */
    lv_obj_t *panel_news;
    lv_obj_t *news_labels[NEWS_TITLE_COUNT];
    /* 月历面板 */
    lv_obj_t *panel_calender;
    lv_obj_t *calender;
    lv_obj_t *calender_today;
    lv_obj_t *calender_today_week;
    lv_obj_t *calender_progress_text;
    lv_obj_t *calender_progress_bar;
    lv_obj_t *calender_panel_huangli;
    lv_obj_t *calender_huangli_1;
    lv_obj_t *calender_huangli_2;
    lv_obj_t *calender_huangli_3;
    lv_obj_t *calender_huangli_4;
    lv_obj_t *calender_huangli_5;
    lv_obj_t *calender_huangli_6;
    /* 定时器面板 */
    lv_obj_t *panel_timer;
    lv_obj_t *timer_bell;
    lv_obj_t *timer_pv;
    lv_obj_t *timer_btn_min_1min;
    lv_obj_t *timer_sv;
    lv_obj_t *timer_btn_add_1min;
    lv_obj_t *timer_quick_1min;
    lv_obj_t *timer_quick_3min;
    lv_obj_t *timer_quick_5min;
    lv_obj_t *timer_quick_10min;
    lv_obj_t *timer_quick_20min;
    lv_obj_t *timer_quick_30min;
    lv_obj_t *timer_quick_40min;
    lv_obj_t *timer_quick_50min;
    lv_obj_t *timer_quick_60min;
    lv_obj_t *timer_reset;
    lv_obj_t *timer_start_pause;
    /* 设置面板 */
    lv_obj_t *set;
    lv_obj_t *set_btn_return;
    lv_obj_t *set_12_24h;
    lv_obj_t *set_time_font;
    /* 面板切换按钮 */
    lv_obj_t *btn_clock;
    lv_obj_t *btn_calender;
    lv_obj_t *btn_timer;
} ui_screen_clock_t;

static ui_screen_clock_t s_ui = {0};

static const widget_binding_t s_clock_bindings[] = {
    WIDGET_BIND(ui_screen_clock_t, btn_home, "clock_btn_home", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_clock_t, btn_set, "clock_btn_set", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_clock_t, panel_clock, "clock_panel_clock", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_clock_t, clock_12h_label, "clock_clock_12h_label", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, clock_bigtime, "clock_clock_bigtime", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, clock_date, "clock_clock_date", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, clock_lunar, "clock_clock_lunar", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, clock_hitokoto, "clock_clock_hitokoto", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, panel_weather, "clock_panel_weather", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_clock_t, weather_today_text, "weather_today_text", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, weather_today_text_1, "weather_today_text_1", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, weather_location, "weather_location", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, weather_temp, "weather_temp", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, weather_humi, "weather_humi", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, weather_day2, "weather_day2", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, weather_day2_1, "weather_day2_1", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, weather_day3, "weather_day3", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, weather_day3_1, "weather_day3_1", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, panel_news, "weatehr_panel_news", WIDGET_KIND_PANEL),
    WIDGET_BIND(ui_screen_clock_t, panel_calender, "clock_panel_calender", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_clock_t, calender, "clock_calender", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_clock_t, calender_today, "clock_calender_today", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, calender_today_week, "clock_calender_today_week", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, calender_progress_text, "clock_calender_progress_text", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, calender_progress_bar, "clock_calender_progress_bar", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_clock_t, calender_panel_huangli, "clock_calender_panel_huangli", WIDGET_KIND_PANEL),
    WIDGET_BIND(ui_screen_clock_t, calender_huangli_1, "clock_calender_huangli_1", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, calender_huangli_2, "clock_calender_huangli_2", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, calender_huangli_3, "clock_calender_huangli_3", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, calender_huangli_4, "clock_calender_huangli_4", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, calender_huangli_5, "clock_calender_huangli_5", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, calender_huangli_6, "clock_calender_huangli_6", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, panel_timer, "clock_panel_timer", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_clock_t, timer_bell, "clock_timer_bell", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_clock_t, timer_pv, "clock_timer_pv", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, timer_btn_min_1min, "clock_timer_btn_min_1min", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_clock_t, timer_sv, "clock_timer_sv", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_clock_t, timer_btn_add_1min, "clock_timer_btn_add_1min", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_clock_t, timer_quick_1min, "clock_timer_quick_1min", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_clock_t, timer_quick_3min, "clock_timer_quick_3min", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_clock_t, timer_quick_5min, "clock_timer_quick_5min", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_clock_t, timer_quick_10min, "clock_timer_quick_10min", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_clock_t, timer_quick_20min, "clock_timer_quick_20min", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_clock_t, timer_quick_30min, "clock_timer_quick_30min", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_clock_t, timer_quick_40min, "clock_timer_quick_40min", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_clock_t, timer_quick_50min, "clock_timer_quick_50min", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_clock_t, timer_quick_60min, "clock_timer_quick_60min", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_clock_t, timer_reset, "clock_timer_reset", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_clock_t, timer_start_pause, "clock_timer_start_pause", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_clock_t, set, "clock_set", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_clock_t, set_btn_return, "clock_set_btn_return", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_clock_t, set_12_24h, "clock_set_12_24h", WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_clock_t, set_time_font, "clock_set_time_font", WIDGET_KIND_DROPDOWN),
    WIDGET_BIND(ui_screen_clock_t, btn_clock, "clock_btn_clock", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_clock_t, btn_calender, "clock_btn_calender", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_clock_t, btn_timer, "clock_btn_timer", WIDGET_KIND_BUTTON),
    WIDGET_BINDING_END,
};

static clock_state_t s_state = {0};
static clock_display_state_t s_display_state = {0};

/* 联网数据驻留缓存：文件作用域，不随 App init/destroy 清零，本次上电全程
 * 有效。HTTP 回调目标全部在此，快速开关 App 不产生悬空引用、不重复拉取。
 * 每个接口独立维护刷新周期与失败退避：成功按 NETWORK_AUTO_REFRESH_US 周期，
 * 失败按指数退避（RETRY_BACKOFF_INIT_US 起步，翻倍，上限 RETRY_BACKOFF_MAX_US）。 */
typedef struct {
    bool wifi_was_connected;
    volatile http_owner_t http_owner;   /* 共享 HTTP 缓冲的当前占用者 */
    volatile bool hitokoto_fetching;
    volatile bool hitokoto_done;
    char hitokoto_buf[128];
    volatile bool weather_fetching;
    volatile bool weather_done;
    weather_data_t weather;
    volatile bool news_fetching;
    volatile bool news_done;
    char news_titles[NEWS_TITLE_COUNT][NEWS_TITLE_LEN];
    bool news_valid;
    /* 一言：成功时间戳与下次重试时间 */
    int64_t hitokoto_last_ok_us;
    int64_t hitokoto_next_retry_us;
    uint32_t hitokoto_backoff_us;
    /* 天气：成功时间戳与下次重试时间 */
    int64_t weather_last_ok_us;
    int64_t weather_next_retry_us;
    uint32_t weather_backoff_us;
    /* 新闻：成功时间戳与下次重试时间 */
    int64_t news_last_ok_us;
    int64_t news_next_retry_us;
    uint32_t news_backoff_us;
} clock_net_cache_t;

static clock_net_cache_t s_net = {0};

/* -------------------- 前向声明 -------------------- */

static void clock_home_cb(lv_event_t *e);
static void clock_set_open_cb(lv_event_t *e);
static void clock_set_close_cb(lv_event_t *e);
static void clock_panel_switch_cb(lv_event_t *e);
static void clock_timer_btn_cb(lv_event_t *e);
static void clock_bell_toggle_cb(lv_event_t *e);
static void clock_calendar_changed_cb(lv_event_t *e);
static void clock_huangli_panel_click_cb(lv_event_t *e);
static void clock_news_panel_click_cb(lv_event_t *e);

static void clock_prepare_time(clock_display_state_t *st);
static void clock_prepare_calendar(clock_display_state_t *st);
static void clock_prepare_timer(clock_display_state_t *st);
static void clock_prepare_network(clock_display_state_t *st);
static void clock_render(const clock_display_state_t *new_state);
static void clock_refresh(uint32_t mask);

/* -------------------- 面板切换 -------------------- */

static void clock_switch_panel(clock_panel_t panel)
{
    s_state.active_panel = panel;

    lvgl_port_lock(portMAX_DELAY);

    /* 面板显隐 */
    if (s_ui.panel_clock) {
        if (panel == PANEL_CLOCK) lv_obj_clear_flag(s_ui.panel_clock, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_ui.panel_clock, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_ui.panel_calender) {
        if (panel == PANEL_CALENDAR) lv_obj_clear_flag(s_ui.panel_calender, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_ui.panel_calender, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_ui.panel_timer) {
        if (panel == PANEL_TIMER) lv_obj_clear_flag(s_ui.panel_timer, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_ui.panel_timer, LV_OBJ_FLAG_HIDDEN);
    }
    /* 按钮 pressed 状态互斥 */
    lv_obj_t *btns[] = {s_ui.btn_clock, s_ui.btn_calender, s_ui.btn_timer};
    for (int i = 0; i < 3; i++) {
        if (btns[i] == NULL) continue;
        if (i == (int)panel) {
            lv_obj_add_state(btns[i], LV_STATE_PRESSED);
        } else {
            lv_obj_clear_state(btns[i], LV_STATE_PRESSED);
        }
    }

    /* 切换面板时同时收起设置面板，避免设置浮层遮挡新面板 */
    if (s_ui.set) {
        lv_obj_add_flag(s_ui.set, LV_OBJ_FLAG_HIDDEN);
    }

    lvgl_port_unlock();
}

/* -------------------- 时钟面板刷新 -------------------- */

static const char *s_weekday_cn[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};

/* 系统时钟未对时或异常时会读出默认值（如 1970/1999 年），过滤避免跳变 */
static bool clock_time_is_valid(const struct tm *tm)
{
    int year = tm->tm_year + 1900;
    return year >= 2020;
}

static bool clock_get_local_time(struct tm *tm)
{
    time_t now = time(NULL);
    if (now == (time_t)-1) {
        return false;
    }
    localtime_r(&now, tm);
    return clock_time_is_valid(tm);
}

static void clock_prepare_time(clock_display_state_t *st)
{
    static bool lunar_refresh = true;
    struct tm tm;
    if (!clock_get_local_time(&tm)) {
        return;
    }

    int hour = tm.tm_hour;
    bool is_pm = (hour >= 12);

    if (s_state.use_12h) {
        if (hour == 0) hour = 12;
        else if (hour > 12) hour -= 12;
    }

    uint8_t hh = (uint8_t)hour;
    uint8_t mm = (uint8_t)tm.tm_min;
    uint8_t ss = (uint8_t)tm.tm_sec;
    snprintf(st->clock_bigtime, sizeof(st->clock_bigtime), "%02u:%02u:%02u", hh, mm, ss);
    snprintf(st->clock_date, sizeof(st->clock_date), "%04d-%02d-%02d %s",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             s_weekday_cn[tm.tm_wday]);

    if (s_state.use_12h) {
        st->clock_12h_visible = true;
        snprintf(st->clock_12h_label, sizeof(st->clock_12h_label), "%s",
                 is_pm ? "下午" : "上午");
    } else {
        st->clock_12h_visible = false;
        st->clock_12h_label[0] = '\0';
    }

    /* 农历：首次必刷新，然后每分钟刷新一次 */
    if (lunar_refresh || tm.tm_sec == 0) {
        lunar_refresh = false;
        cnlunar_t lunar;
        if (cnlunar_compute(&lunar, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, 12, NULL) == 0) {
            const char *term = (strcmp(lunar.today_solar_term, "无") != 0) ? lunar.today_solar_term : "";
            snprintf(st->clock_lunar, sizeof(st->clock_lunar), "%s年%s %s %s",
                     lunar.year_ganzhi, lunar.lunar_month_cn, lunar.lunar_day_cn, term);
        }
    }
}

/* -------------------- 月历面板刷新 -------------------- */

static int clock_iso_week(int year, int month, int day)
{
    /* 简化 ISO 周计算 */
    struct tm t = {0};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    mktime(&t);
    int yday = t.tm_yday + 1;
    int wday = (t.tm_wday == 0) ? 7 : t.tm_wday;
    return (yday - wday + 10) / 7;
}

static int clock_day_of_year(int year, int month, int day)
{
    struct tm t = {0};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    mktime(&t);
    return t.tm_yday + 1;
}

static bool clock_is_leap_year(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/* 取月历面板要展示的日期：优先使用手动选中的日期，否则回退到 RTC 当前时间 */
static bool clock_get_calendar_date(struct tm *tm)
{
    if (s_state.cal_sel_valid) {
        tm->tm_year = (int)s_state.cal_sel_year - 1900;
        tm->tm_mon = (int)s_state.cal_sel_month - 1;
        tm->tm_mday = (int)s_state.cal_sel_day;
        tm->tm_hour = 12;
        tm->tm_min = 0;
        tm->tm_sec = 0;
        mktime(tm);
        return true;
    }
    return clock_get_local_time(tm);
}

static void clock_prepare_calendar(clock_display_state_t *st)
{
    struct tm tm;
    if (!clock_get_calendar_date(&tm)) {
        return;
    }

    int year = tm.tm_year + 1900;
    int month = tm.tm_mon + 1;
    int day = tm.tm_mday;

    st->cal_year = (uint16_t)year;
    st->cal_month = (uint8_t)month;
    st->cal_day = (uint8_t)day;

    snprintf(st->cal_today, sizeof(st->cal_today), "%04u-%02u-%02u",
             st->cal_year, st->cal_month, st->cal_day);
    snprintf(st->cal_week, sizeof(st->cal_week), "%d年第%d周",
             year, clock_iso_week(year, month, day));

    int doy = clock_day_of_year(year, month, day);
    int total_days = clock_is_leap_year(year) ? 366 : 365;
    int remain = total_days - doy;

    snprintf(st->cal_progress, sizeof(st->cal_progress),
             "%d年第%d天，还剩%d天", year, doy, remain);
    st->cal_progress_pct = (doy * 100) / total_days;

    cnlunar_t lunar;
    bool lunar_ok = (cnlunar_compute(&lunar, year, month, day, 12, NULL) == 0);
    if (!lunar_ok) {
        return;
    }

    uint8_t page = s_state.huangli_page % HUANGLI_PAGE_COUNT;
    switch (page) {
        case 0: { /* 宜忌 */
            char things_buf[192];
            cnlunar_join_things(&lunar, true, things_buf, sizeof(things_buf));
            snprintf(st->cal_huangli_1, sizeof(st->cal_huangli_1), "宜");
            snprintf(st->cal_huangli_2, sizeof(st->cal_huangli_2), "%s",
                     things_buf[0] ? things_buf : "诸事不宜");
            cnlunar_join_things(&lunar, false, things_buf, sizeof(things_buf));
            snprintf(st->cal_huangli_3, sizeof(st->cal_huangli_3), "忌");
            snprintf(st->cal_huangli_4, sizeof(st->cal_huangli_4), "%s",
                     things_buf[0] ? things_buf : "无");
            snprintf(st->cal_huangli_5, sizeof(st->cal_huangli_5), "值日");
            snprintf(st->cal_huangli_6, sizeof(st->cal_huangli_6), "%s %s",
                     lunar.day_god, lunar.is_yellow_day ? "黄道" : "黑道");
            break;
        }
        case 1: { /* 农历干支 */
            snprintf(st->cal_huangli_1, sizeof(st->cal_huangli_1), "农历");
            snprintf(st->cal_huangli_2, sizeof(st->cal_huangli_2), "%s%s%s",
                     lunar.lunar_year_cn, lunar.lunar_month_cn, lunar.lunar_day_cn);
            snprintf(st->cal_huangli_3, sizeof(st->cal_huangli_3), "干支");
            snprintf(st->cal_huangli_4, sizeof(st->cal_huangli_4), "%s年 %s月 %s日",
                     lunar.year_ganzhi, lunar.month_ganzhi, lunar.day_ganzhi);
            snprintf(st->cal_huangli_5, sizeof(st->cal_huangli_5), "生肖");
            snprintf(st->cal_huangli_6, sizeof(st->cal_huangli_6), "%s年 %s日",
                     lunar.zodiac, lunar.zodiac_win);
            break;
        }
        case 2: { /* 节气星宿 */
            const char *term = (strcmp(lunar.today_solar_term, "无") != 0)
                                   ? lunar.today_solar_term
                                   : "今日无节气";
            snprintf(st->cal_huangli_1, sizeof(st->cal_huangli_1), "节气");
            snprintf(st->cal_huangli_2, sizeof(st->cal_huangli_2), "%s 下一%s %d/%d",
                     term, lunar.next_solar_term,
                     (int)lunar.next_solar_term_month, (int)lunar.next_solar_term_day);
            snprintf(st->cal_huangli_3, sizeof(st->cal_huangli_3), "二十八宿");
            snprintf(st->cal_huangli_4, sizeof(st->cal_huangli_4), "%s", lunar.star28);
            snprintf(st->cal_huangli_5, sizeof(st->cal_huangli_5), "建除十二神");
            snprintf(st->cal_huangli_6, sizeof(st->cal_huangli_6), "%s", lunar.day_officer);
            break;
        }
        case 3: { /* 冲煞纳音 */
            snprintf(st->cal_huangli_1, sizeof(st->cal_huangli_1), "冲煞");
            snprintf(st->cal_huangli_2, sizeof(st->cal_huangli_2), "%s", lunar.zodiac_clash);
            snprintf(st->cal_huangli_3, sizeof(st->cal_huangli_3), "纳音");
            snprintf(st->cal_huangli_4, sizeof(st->cal_huangli_4), "%s", lunar.nayin);
            snprintf(st->cal_huangli_5, sizeof(st->cal_huangli_5), "彭祖百忌");
            snprintf(st->cal_huangli_6, sizeof(st->cal_huangli_6), "%s %s",
                     lunar.peng_taboo_stem, lunar.peng_taboo_branch);
            break;
        }
        default:
            break;
    }
}

/* -------------------- 定时器面板 -------------------- */

static void timer_format_time(uint32_t total_s, char *buf, size_t len)
{
    unsigned int m = (unsigned int)(total_s / 60);
    unsigned int s = (unsigned int)(total_s % 60);
    snprintf(buf, len, "%02u:%02u", m, s);
}

static void clock_prepare_timer(clock_display_state_t *st)
{
    timer_format_time(s_state.timer_remain_s, st->timer_pv, sizeof(st->timer_pv));
    timer_format_time(s_state.timer_total_s, st->timer_sv, sizeof(st->timer_sv));
    snprintf(st->timer_bell_text, sizeof(st->timer_bell_text), "%s",
             s_state.timer_bell ? "" : "");
}

static void timer_set_config_mode(bool config)
{
    lvgl_port_lock(portMAX_DELAY);

    /* 配置态控件：quick 按钮 + add/sub */
    lv_obj_t *config_btns[] = {
        s_ui.timer_quick_1min, s_ui.timer_quick_3min, s_ui.timer_quick_5min,
        s_ui.timer_quick_10min, s_ui.timer_quick_20min, s_ui.timer_quick_30min,
        s_ui.timer_quick_40min, s_ui.timer_quick_50min, s_ui.timer_quick_60min,
        s_ui.timer_btn_min_1min, s_ui.timer_btn_add_1min,
    };
    for (int i = 0; i < 11; i++) {
        if (config_btns[i] == NULL) continue;
        if (config) {
            lv_obj_clear_state(config_btns[i], LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(config_btns[i], LV_STATE_DISABLED);
        }
    }

    lvgl_port_unlock();
}

static void timer_set_total(uint32_t seconds)
{
    if (seconds > 99 * 60) seconds = 99 * 60;
    s_state.timer_total_s = seconds;
    s_state.timer_remain_s = seconds;
    service_nvs_set_clock_timer_s(seconds);
    clock_refresh(REFRESH_TIMER);
}

static void timer_start(void)
{
    if (s_state.timer_total_s == 0) return;
    s_state.timer_state = TIMER_RUNNING;
    s_state.timer_remain_s = s_state.timer_total_s;
    s_state.timer_next_tick_us = esp_timer_get_time() + 1000000;
    timer_set_config_mode(false);
    clock_refresh(REFRESH_TIMER);
}

static void timer_pause(void)
{
    s_state.timer_state = TIMER_PAUSED;
}

static void timer_resume(void)
{
    s_state.timer_state = TIMER_RUNNING;
    s_state.timer_next_tick_us = esp_timer_get_time() + 1000000;
}

static void timer_reset(void)
{
    s_state.timer_state = TIMER_CONFIG;
    s_state.timer_remain_s = s_state.timer_total_s;
    s_state.bell_rings_left = 0;
    s_state.bell_note_on = false;
    timer_set_config_mode(true);
    clock_refresh(REFRESH_TIMER);
}

static void timer_midi_hit_on(void)
{
    engine_midi_event_t evt = {0};
    evt.type = ENGINE_MIDI_MSG_NOTE_ON;
    evt.channel = TIMER_BELL_CHANNEL;
    evt.data1 = TIMER_BELL_NOTE;
    evt.data2 = 100;
    evt.source_port = ENGINE_MIDI_PORT_APP;
    engine_midi_publish(&evt, 0);
}

static void timer_midi_hit_off(void)
{
    engine_midi_event_t evt = {0};
    evt.type = ENGINE_MIDI_MSG_NOTE_OFF;
    evt.channel = TIMER_BELL_CHANNEL;
    evt.data1 = TIMER_BELL_NOTE;
    evt.data2 = 0;
    evt.source_port = ENGINE_MIDI_PORT_APP;
    engine_midi_publish(&evt, 0);
}

static void timer_process(void)
{
    int64_t now = esp_timer_get_time();

    if (s_state.timer_state == TIMER_RUNNING && now >= s_state.timer_next_tick_us) {
        s_state.timer_next_tick_us += 1000000;
        if (s_state.timer_remain_s > 0) {
            s_state.timer_remain_s--;
            clock_refresh(REFRESH_TIMER);
        }
        if (s_state.timer_remain_s == 0) {
            /* 到时 */
            s_state.timer_state = TIMER_CONFIG;
            timer_set_config_mode(true);
            if (s_state.timer_bell) {
                s_state.bell_rings_left = TIMER_BELL_COUNT;
                s_state.bell_note_on = false;
                s_state.bell_next_us = now;
            }
            app_manager_show_notification_timeout("定时器到时！", 3000);
        }
    }

    /* 铃声连击：每声 60ms ON + 300ms 间隔 */
    if (s_state.bell_rings_left > 0 && now >= s_state.bell_next_us) {
        if (!s_state.bell_note_on) {
            timer_midi_hit_on();
            s_state.bell_note_on = true;
            s_state.bell_next_us = now + 60000; /* 60ms 后关闭 */
        } else {
            timer_midi_hit_off();
            s_state.bell_note_on = false;
            s_state.bell_rings_left--;
            if (s_state.bell_rings_left > 0) {
                s_state.bell_next_us = now + 300000; /* 300ms 后下一声 */
            }
        }
    }
}

/* -------------------- 网络：HTTP 异步获取 -------------------- */

static char s_hitokoto_resp[128];
static char *s_http_resp_buf = NULL;   /* 天气/新闻共享缓冲（PSRAM 懒分配，常驻不释放） */

/* 懒分配共享 HTTP 响应缓冲：网络功能非时钟核心路径，分配失败仅天气/新闻
 * 不更新；12KB 落 PSRAM 省内部 RAM（原静态 .bss 常驻内部） */
static char *clock_http_buf(void)
{
    if (s_http_resp_buf == NULL) {
        s_http_resp_buf = heap_caps_malloc(NETWORK_RESP_BUF_SIZE, MALLOC_CAP_SPIRAM);
        if (s_http_resp_buf == NULL) {
            ESP_LOGW(TAG, "http resp buf alloc failed");
        }
    }
    return s_http_resp_buf;
}

/**
 * @brief 占用共享 HTTP 响应缓冲
 * @return true 占用成功；false 当前缓冲正被其他请求使用
 */
static bool clock_http_acquire(http_owner_t owner)
{
    if (s_net.http_owner != HTTP_OWNER_NONE) {
        return false;
    }
    s_net.http_owner = owner;
    return true;
}

/**
 * @brief 释放共享 HTTP 响应缓冲
 */
static void clock_http_release(http_owner_t owner)
{
    if (s_net.http_owner == owner) {
        s_net.http_owner = HTTP_OWNER_NONE;
    }
}

static void clock_parse_weather_response(const char *resp)
{
    cJSON *root = cJSON_Parse(resp);
    if (root == NULL) {
        return;
    }

    weather_data_t *w = &s_net.weather;
    memset(w, 0, sizeof(*w));

    cJSON *item;
    item = cJSON_GetObjectItem(root, "district");
    if (item && cJSON_IsString(item)) {
        strncpy(w->district, item->valuestring, sizeof(w->district) - 1);
    }
    item = cJSON_GetObjectItem(root, "city");
    if (item && cJSON_IsString(item)) {
        strncpy(w->city, item->valuestring, sizeof(w->city) - 1);
    }

    item = cJSON_GetObjectItem(root, "weather");
    if (item && cJSON_IsString(item))
        strncpy(w->weather, item->valuestring, sizeof(w->weather) - 1);

    item = cJSON_GetObjectItem(root, "wind_direction");
    if (item && cJSON_IsString(item))
        strncpy(w->wind_dir, item->valuestring, sizeof(w->wind_dir) - 1);

    item = cJSON_GetObjectItem(root, "wind_power");
    if (item && cJSON_IsString(item))
        strncpy(w->wind_power, item->valuestring, sizeof(w->wind_power) - 1);

    item = cJSON_GetObjectItem(root, "temperature");
    if (item && cJSON_IsNumber(item)) w->temperature = item->valueint;

    item = cJSON_GetObjectItem(root, "humidity");
    if (item && cJSON_IsNumber(item)) w->humidity = item->valueint;

    /* 预报 day2/day3 */
    cJSON *forecast = cJSON_GetObjectItem(root, "forecast");
    if (forecast && cJSON_IsArray(forecast)) {
        cJSON *d0 = cJSON_GetArrayItem(forecast, 0);
        cJSON *d1 = cJSON_GetArrayItem(forecast, 1);
        cJSON *d2 = cJSON_GetArrayItem(forecast, 2);
        if (d0) {   // 补充今日的天气预报情况
            item = cJSON_GetObjectItem(d0, "date");
            if (item && cJSON_IsString(item)) {
                strncpy(w->date, item->valuestring, sizeof(w->date) - 1);
            }
            item = cJSON_GetObjectItem(d0, "week");
            if (item && cJSON_IsString(item)) {
                strncpy(w->week, item->valuestring, sizeof(w->week) - 1);
            }

            item = cJSON_GetObjectItem(d0, "pop");
            if (item && cJSON_IsNumber(item))
                w->pop = item->valueint;

            item = cJSON_GetObjectItem(d0, "cloud");
            if (item && cJSON_IsNumber(item))
                w->cloud = item->valueint;

            item = cJSON_GetObjectItem(d0, "uv_index");
            if (item && cJSON_IsNumber(item))
                w->uv_index = item->valueint;
        }
        if (d1) {   // 明天的天气预报情况
            item = cJSON_GetObjectItem(d1, "week");
            if (item && cJSON_IsString(item))
                strncpy(w->day2_week, item->valuestring, sizeof(w->day2_week) - 1);

            item = cJSON_GetObjectItem(d1, "weather_day");
            if (item && cJSON_IsString(item))
                strncpy(w->day2_weather, item->valuestring, sizeof(w->day2_weather) - 1);

            item = cJSON_GetObjectItem(d1, "temp_max");
            if (item && cJSON_IsNumber(item)) w->day2_temperature_high = item->valueint;

            item = cJSON_GetObjectItem(d1, "temp_min");
            if (item && cJSON_IsNumber(item)) w->day2_temperature_low = item->valueint;

            item = cJSON_GetObjectItem(d1, "pop");
            if (item && cJSON_IsNumber(item)) w->day2_pop = item->valueint;

            item = cJSON_GetObjectItem(d1, "uv_index");
            if (item && cJSON_IsNumber(item)) w->day2_uv_index = item->valueint;
        }
        if (d2) {   // 后天的天气预报情况
            item = cJSON_GetObjectItem(d2, "week");
            if (item && cJSON_IsString(item))
                strncpy(w->day3_week, item->valuestring, sizeof(w->day3_week) - 1);

            item = cJSON_GetObjectItem(d2, "weather_day");
            if (item && cJSON_IsString(item))
                strncpy(w->day3_weather, item->valuestring, sizeof(w->day3_weather) - 1);

            item = cJSON_GetObjectItem(d2, "temp_max");
            if (item && cJSON_IsNumber(item)) w->day3_temperature_high = item->valueint;

            item = cJSON_GetObjectItem(d2, "temp_min");
            if (item && cJSON_IsNumber(item)) w->day3_temperature_low = item->valueint;

            item = cJSON_GetObjectItem(d2, "pop");
            if (item && cJSON_IsNumber(item)) w->day3_pop = item->valueint;

            item = cJSON_GetObjectItem(d2, "uv_index");
            if (item && cJSON_IsNumber(item)) w->day3_uv_index = item->valueint;
        }
    }

    w->valid = true;
    cJSON_Delete(root);
}

static void http_hitokoto_cb(int req_id, esp_err_t err, int http_status,
                             const char *resp, size_t resp_len, void *user_data)
{
    (void)req_id;
    (void)user_data;
    (void)resp_len;

    int64_t now = esp_timer_get_time();
    if (err == ESP_OK && http_status >= 200 && http_status < 300 &&
        resp != NULL && resp[0] != '\0') {
        ESP_LOGI(TAG, "Hitokoto: %s", resp);
        strncpy(s_net.hitokoto_buf, resp, sizeof(s_net.hitokoto_buf) - 1);
        s_net.hitokoto_buf[sizeof(s_net.hitokoto_buf) - 1] = '\0';
        s_net.hitokoto_last_ok_us = now;
        s_net.hitokoto_next_retry_us = now + NETWORK_AUTO_REFRESH_US;
        s_net.hitokoto_backoff_us = RETRY_BACKOFF_INIT_US;
    } else {
        /* 失败：指数退避 */
        if (s_net.hitokoto_backoff_us == 0) {
            s_net.hitokoto_backoff_us = RETRY_BACKOFF_INIT_US;
        }
        s_net.hitokoto_next_retry_us = now + s_net.hitokoto_backoff_us;
        uint32_t next = s_net.hitokoto_backoff_us * 2;
        if (next > RETRY_BACKOFF_MAX_US || next < s_net.hitokoto_backoff_us) {
            next = RETRY_BACKOFF_MAX_US;
        }
        s_net.hitokoto_backoff_us = next;
    }
    s_net.hitokoto_done = true;
    s_net.hitokoto_fetching = false;
}

static void http_weather_cb(int req_id, esp_err_t err, int http_status,
                            const char *resp, size_t resp_len, void *user_data)
{
    (void)req_id;
    (void)user_data;
    (void)resp_len;

    int64_t now = esp_timer_get_time();
    if (err == ESP_OK && http_status >= 200 && http_status < 300 && resp != NULL) {
        ESP_LOGI(TAG, "Weather: %s", resp);
        clock_parse_weather_response(resp);
        if (s_net.weather.valid) {
            s_net.weather_last_ok_us = now;
            s_net.weather_next_retry_us = now + NETWORK_AUTO_REFRESH_US;
            s_net.weather_backoff_us = RETRY_BACKOFF_INIT_US;
        } else {
            /* 解析失败也算失败，走退避 */
            if (s_net.weather_backoff_us == 0) {
                s_net.weather_backoff_us = RETRY_BACKOFF_INIT_US;
            }
            s_net.weather_next_retry_us = now + s_net.weather_backoff_us;
            uint32_t next = s_net.weather_backoff_us * 2;
            if (next > RETRY_BACKOFF_MAX_US || next < s_net.weather_backoff_us) {
                next = RETRY_BACKOFF_MAX_US;
            }
            s_net.weather_backoff_us = next;
        }
    } else {
        if (s_net.weather_backoff_us == 0) {
            s_net.weather_backoff_us = RETRY_BACKOFF_INIT_US;
        }
        s_net.weather_next_retry_us = now + s_net.weather_backoff_us;
        uint32_t next = s_net.weather_backoff_us * 2;
        if (next > RETRY_BACKOFF_MAX_US || next < s_net.weather_backoff_us) {
            next = RETRY_BACKOFF_MAX_US;
        }
        s_net.weather_backoff_us = next;
    }
    clock_http_release(HTTP_OWNER_WEATHER);
    s_net.weather_done = true;
    s_net.weather_fetching = false;
}

static void clock_fetch_hitokoto(void)
{
    if (s_net.hitokoto_fetching) return;
    s_net.hitokoto_fetching = true;
    s_net.hitokoto_done = false;

    service_http_client_req_t req = {
        .method = SERVICE_HTTP_METHOD_GET,
        .url = HITOKOTO_URL,
        .resp_buf = s_hitokoto_resp,
        .resp_buf_len = sizeof(s_hitokoto_resp),
        .timeout_ms = 8000,
        .callback = http_hitokoto_cb,
    };
    if (service_http_client_submit(&req) <= 0) {
        s_net.hitokoto_fetching = false;
    }
}

static void clock_fetch_weather(void)
{
    if (s_net.weather_fetching) return;
    if (!clock_http_acquire(HTTP_OWNER_WEATHER)) return;

    char *resp_buf = clock_http_buf();
    if (resp_buf == NULL) {
        clock_http_release(HTTP_OWNER_WEATHER);
        return;
    }

    s_net.weather_fetching = true;
    s_net.weather_done = false;

    service_http_client_req_t req = {
        .method = SERVICE_HTTP_METHOD_GET,
        .url = WEATHER_URL,
        .resp_buf = resp_buf,
        .resp_buf_len = NETWORK_RESP_BUF_SIZE,
        .timeout_ms = 10000,
        .callback = http_weather_cb,
    };
    if (service_http_client_submit(&req) <= 0) {
        clock_http_release(HTTP_OWNER_WEATHER);
        s_net.weather_fetching = false;
    }
}

static void clock_parse_news_response(const char *resp)
{
    /* 接口返回 JSONP（artiList({...})），直接 cJSON_Parse 必失败；
     * 剥离外层包裹，仅解析首个 '{' 到末个 '}' 的内层 JSON */
    const char *json_start = strchr(resp, '{');
    const char *json_end = (json_start != NULL) ? strrchr(json_start, '}') : NULL;
    if (json_start == NULL || json_end == NULL || json_end <= json_start) {
        return;
    }
    cJSON *root = cJSON_ParseWithLength(json_start,
                                        (size_t)(json_end - json_start + 1));
    if (root == NULL) {
        return;
    }

    cJSON *arr = cJSON_GetObjectItem(root, "BBM54PGAwangning");
    if (arr == NULL || !cJSON_IsArray(arr)) {
        cJSON_Delete(root);
        return;
    }

    int n = cJSON_GetArraySize(arr);
    if (n > NEWS_TITLE_COUNT) {
        n = NEWS_TITLE_COUNT;
    }

    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        if (item == NULL) {
            continue;
        }
        cJSON *title = cJSON_GetObjectItem(item, "title");
        if (title != NULL && cJSON_IsString(title) && title->valuestring != NULL) {
            strncpy(s_net.news_titles[i], title->valuestring, NEWS_TITLE_LEN - 1);
            s_net.news_titles[i][NEWS_TITLE_LEN - 1] = '\0';
        } else {
            s_net.news_titles[i][0] = '\0';
        }
    }

    s_net.news_valid = (n > 0);
    cJSON_Delete(root);
}

static void http_news_cb(int req_id, esp_err_t err, int http_status,
                         const char *resp, size_t resp_len, void *user_data)
{
    (void)req_id;
    (void)user_data;
    (void)resp_len;

    int64_t now = esp_timer_get_time();
    if (err == ESP_OK && http_status >= 200 && http_status < 300 && resp != NULL) {
        ESP_LOGI(TAG, "News: %s", resp);
        clock_parse_news_response(resp);
        if (s_net.news_valid) {
            s_net.news_last_ok_us = now;
            s_net.news_next_retry_us = now + NEWS_FETCH_INTERVAL_US;
            s_net.news_backoff_us = RETRY_BACKOFF_INIT_US;
        } else {
            if (s_net.news_backoff_us == 0) {
                s_net.news_backoff_us = RETRY_BACKOFF_INIT_US;
            }
            s_net.news_next_retry_us = now + s_net.news_backoff_us;
            uint32_t next = s_net.news_backoff_us * 2;
            if (next > RETRY_BACKOFF_MAX_US || next < s_net.news_backoff_us) {
                next = RETRY_BACKOFF_MAX_US;
            }
            s_net.news_backoff_us = next;
        }
    } else {
        if (s_net.news_backoff_us == 0) {
            s_net.news_backoff_us = RETRY_BACKOFF_INIT_US;
        }
        s_net.news_next_retry_us = now + s_net.news_backoff_us;
        uint32_t next = s_net.news_backoff_us * 2;
        if (next > RETRY_BACKOFF_MAX_US || next < s_net.news_backoff_us) {
            next = RETRY_BACKOFF_MAX_US;
        }
        s_net.news_backoff_us = next;
    }
    clock_http_release(HTTP_OWNER_NEWS);
    s_net.news_done = true;
    s_net.news_fetching = false;
}

static void clock_fetch_news(void)
{
    if (s_net.news_fetching) {
        return;
    }
    if (!clock_http_acquire(HTTP_OWNER_NEWS)) return;

    char *resp_buf = clock_http_buf();
    if (resp_buf == NULL) {
        clock_http_release(HTTP_OWNER_NEWS);
        return;
    }

    s_net.news_fetching = true;
    s_net.news_done = false;

    char url[256];
    snprintf(url, sizeof(url), NEWS_URL, 0, NEWS_TITLE_COUNT);

    service_http_client_req_t req = {
        .method = SERVICE_HTTP_METHOD_GET,
        .url = url,
        .resp_buf = resp_buf,
        .resp_buf_len = NETWORK_RESP_BUF_SIZE,
        .timeout_ms = 10000,
        .callback = http_news_cb,
    };
    if (service_http_client_submit(&req) <= 0) {
        clock_http_release(HTTP_OWNER_NEWS);
        s_net.news_fetching = false;
    }
}

static void clock_prepare_network(clock_display_state_t *st)
{
    st->hitokoto_visible = (s_net.hitokoto_buf[0] != '\0');
    if (st->hitokoto_visible) {
        snprintf(st->hitokoto, sizeof(st->hitokoto), "%s", s_net.hitokoto_buf);
    }

    st->weather_visible = s_net.weather.valid;
    if (st->weather_visible) {
        weather_data_t *w = &s_net.weather;
        
        snprintf(st->weather_location, sizeof(st->weather_location), "%s\n%s", w->city, w->district);
        snprintf(st->weather_today_text, sizeof(st->weather_today_text),
                 "%s %s", w->date, w->week);
        snprintf(st->weather_today_text_1, sizeof(st->weather_today_text_1),
                 "%s\n%s %s\n %d%%", w->weather, w->wind_dir, w->wind_power, w->pop);
        snprintf(st->weather_temp_today, sizeof(st->weather_temp_today), " %d°C", w->temperature);
        snprintf(st->weather_humi_today, sizeof(st->weather_humi_today), " %d%%", w->humidity);

        snprintf(st->weather_day2_text, sizeof(st->weather_day2_text), "%s", w->day2_weather);
        snprintf(st->weather_day2_text_1, sizeof(st->weather_day2_text_1), "%d°C~%d°C", w->day2_temperature_low, w->day2_temperature_high);
        snprintf(st->weather_day3_text, sizeof(st->weather_day3_text), "%s", w->day3_weather);
        snprintf(st->weather_day3_text_1, sizeof(st->weather_day3_text_1), "%d°C~%d°C", w->day3_temperature_low, w->day3_temperature_high);
    }

    st->news_visible = s_net.news_valid && s_state.news_visible;
    for (int i = 0; i < NEWS_TITLE_COUNT; i++) {
        snprintf(st->news_titles[i], sizeof(st->news_titles[i]),
                 "%d. %s", i + 1, s_net.news_titles[i]);
    }
}

/* 网络结果轮询：在 on_update 中调用，仅决定是否触发获取并返回需刷新的掩码 */
static uint32_t clock_poll_network(void)
{
    uint32_t mask = 0;
    int64_t now = esp_timer_get_time();

    /* WiFi 边沿检测：刚连上时三个接口各发起一次（受各自 fetching 互斥保护） */
    bool connected = service_wifi_is_connected();
    if (connected && !s_net.wifi_was_connected) {
        clock_fetch_hitokoto();
        clock_fetch_weather();
        clock_fetch_news();
    }
    s_net.wifi_was_connected = connected;

    if (connected) {
        /* 一言：首次（last_ok=0 且 next_retry=0）立即发起；
         * 否则到 next_retry_us 再试（成功 30min 周期 / 失败退避）。 */
        if (!s_net.hitokoto_fetching &&
            ((s_net.hitokoto_last_ok_us == 0 && s_net.hitokoto_next_retry_us == 0) ||
             (s_net.hitokoto_next_retry_us != 0 && now >= s_net.hitokoto_next_retry_us))) {
            clock_fetch_hitokoto();
        }
        /* 天气：同上 */
        if (!s_net.weather_fetching &&
            ((s_net.weather_last_ok_us == 0 && s_net.weather_next_retry_us == 0) ||
             (s_net.weather_next_retry_us != 0 && now >= s_net.weather_next_retry_us))) {
            clock_fetch_weather();
        }
        /* 新闻：同上 */
        if (!s_net.news_fetching &&
            ((s_net.news_last_ok_us == 0 && s_net.news_next_retry_us == 0) ||
             (s_net.news_next_retry_us != 0 && now >= s_net.news_next_retry_us))) {
            clock_fetch_news();
        }
    }

    if (s_net.hitokoto_done) {
        s_net.hitokoto_done = false;
        mask |= REFRESH_NETWORK;
    }
    if (s_net.weather_done) {
        s_net.weather_done = false;
        mask |= REFRESH_NETWORK;
    }
    if (s_net.news_done) {
        s_net.news_done = false;
        mask |= REFRESH_NETWORK;
    }

    /* 新闻面板与天气面板按固定周期轮换显隐 */
    if (s_net.news_valid && now >= s_state.news_toggle_next_us) {
        s_state.news_visible = !s_state.news_visible;
        s_state.news_toggle_next_us = now + (s_state.news_visible ? NEWS_SHOW_DURATION_US : NEWS_HIDE_DURATION_US);
        mask |= REFRESH_NETWORK;
    }

    return mask;
}

/* -------------------- 统一渲染 -------------------- */

static void clock_render(const clock_display_state_t *new_state)
{
    lvgl_port_lock(portMAX_DELAY);

    /* 时钟面板 */
    if (s_ui.clock_bigtime &&
        strcmp(s_display_state.clock_bigtime, new_state->clock_bigtime) != 0) {
        lv_label_set_text(s_ui.clock_bigtime, new_state->clock_bigtime);
    }
    if (s_ui.clock_date &&
        strcmp(s_display_state.clock_date, new_state->clock_date) != 0) {
        lv_label_set_text(s_ui.clock_date, new_state->clock_date);
    }
    if (s_ui.clock_lunar &&
        strcmp(s_display_state.clock_lunar, new_state->clock_lunar) != 0) {
        lv_label_set_text(s_ui.clock_lunar, new_state->clock_lunar);
    }
    if (s_ui.clock_12h_label) {
        bool vis_changed = (s_display_state.clock_12h_visible != new_state->clock_12h_visible);
        bool text_changed = (strcmp(s_display_state.clock_12h_label,
                                    new_state->clock_12h_label) != 0);
        if (vis_changed) {
            if (new_state->clock_12h_visible) {
                lv_obj_clear_flag(s_ui.clock_12h_label, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(s_ui.clock_12h_label, LV_OBJ_FLAG_HIDDEN);
            }
        }
        if (text_changed && new_state->clock_12h_visible) {
            lv_label_set_text(s_ui.clock_12h_label, new_state->clock_12h_label);
        }
    }

    /* 一言 */
    if (s_ui.clock_hitokoto) {
        bool vis_changed = (s_display_state.hitokoto_visible != new_state->hitokoto_visible);
        bool text_changed = (strcmp(s_display_state.hitokoto, new_state->hitokoto) != 0);
        if (vis_changed) {
            if (new_state->hitokoto_visible) {
                lv_obj_clear_flag(s_ui.clock_hitokoto, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(s_ui.clock_hitokoto, LV_OBJ_FLAG_HIDDEN);
            }
        }
        if (text_changed && new_state->hitokoto_visible) {
            lv_label_set_text(s_ui.clock_hitokoto, new_state->hitokoto);
        }
    }

    /* 天气 */
    if (s_ui.panel_weather) {
        bool vis_changed = (s_display_state.weather_visible != new_state->weather_visible);
        if (vis_changed) {
            if (new_state->weather_visible) {
                lv_obj_clear_flag(s_ui.panel_weather, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(s_ui.panel_weather, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    if (new_state->weather_visible) {
        if (s_ui.weather_location &&
            strcmp(s_display_state.weather_location, new_state->weather_location) != 0) {
            lv_label_set_text(s_ui.weather_location, new_state->weather_location);
        }
        if (s_ui.weather_today_text &&
            strcmp(s_display_state.weather_today_text, new_state->weather_today_text) != 0) {
            lv_label_set_text(s_ui.weather_today_text, new_state->weather_today_text);
        }
        if (s_ui.weather_today_text_1 &&
            strcmp(s_display_state.weather_today_text_1, new_state->weather_today_text_1) != 0) {
            lv_label_set_text(s_ui.weather_today_text_1, new_state->weather_today_text_1);
        }
        if (s_ui.weather_humi &&
            strcmp(s_display_state.weather_humi_today, new_state->weather_humi_today) != 0) {
            lv_label_set_text(s_ui.weather_humi, new_state->weather_humi_today);
        }
        if (s_ui.weather_temp &&
            strcmp(s_display_state.weather_temp_today, new_state->weather_temp_today) != 0) {
            lv_label_set_text(s_ui.weather_temp, new_state->weather_temp_today);
        }
        if (s_ui.weather_day2 &&
            strcmp(s_display_state.weather_day2_text, new_state->weather_day2_text) != 0) {
            lv_label_set_text(s_ui.weather_day2, new_state->weather_day2_text);
        }
        if (s_ui.weather_day2_1 &&
            strcmp(s_display_state.weather_day2_text_1, new_state->weather_day2_text_1) != 0) {
            lv_label_set_text(s_ui.weather_day2_1, new_state->weather_day2_text_1);
        }
        if (s_ui.weather_day3 &&
            strcmp(s_display_state.weather_day3_text, new_state->weather_day3_text) != 0) {
            lv_label_set_text(s_ui.weather_day3, new_state->weather_day3_text);
        }
        if (s_ui.weather_day3_1 &&
            strcmp(s_display_state.weather_day3_text_1, new_state->weather_day3_text_1) != 0) {
            lv_label_set_text(s_ui.weather_day3_1, new_state->weather_day3_text_1);
        }
    }

    /* 新闻面板（覆盖在天气层之上，按周期显隐） */
    if (s_ui.panel_news) {
        bool vis_changed = (s_display_state.news_visible != new_state->news_visible);
        if (vis_changed) {
            if (new_state->news_visible) {
                lv_obj_clear_flag(s_ui.panel_news, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(s_ui.panel_news, LV_OBJ_FLAG_HIDDEN);
            }
        }
        for (int i = 0; i < NEWS_TITLE_COUNT; i++) {
            if (s_ui.news_labels[i] &&
                strcmp(s_display_state.news_titles[i], new_state->news_titles[i]) != 0) {
                lv_label_set_text(s_ui.news_labels[i], new_state->news_titles[i]);
            }
        }
    }

    /* 月历面板 */
    if (s_ui.calender) {
        bool date_changed = (s_display_state.cal_year != new_state->cal_year ||
                             s_display_state.cal_month != new_state->cal_month ||
                             s_display_state.cal_day != new_state->cal_day);
        if (date_changed && new_state->cal_year >= 2020) {
            lv_calendar_set_today_date(s_ui.calender, new_state->cal_year,
                                       new_state->cal_month, new_state->cal_day);
            lv_calendar_set_month_shown(s_ui.calender, new_state->cal_year,
                                        new_state->cal_month);
        }
    }
    if (s_ui.calender_today &&
        strcmp(s_display_state.cal_today, new_state->cal_today) != 0) {
        lv_label_set_text(s_ui.calender_today, new_state->cal_today);
    }
    if (s_ui.calender_today_week &&
        strcmp(s_display_state.cal_week, new_state->cal_week) != 0) {
        lv_label_set_text(s_ui.calender_today_week, new_state->cal_week);
    }
    if (s_ui.calender_progress_text &&
        strcmp(s_display_state.cal_progress, new_state->cal_progress) != 0) {
        lv_label_set_text(s_ui.calender_progress_text, new_state->cal_progress);
    }
    if (s_ui.calender_progress_bar &&
        s_display_state.cal_progress_pct != new_state->cal_progress_pct) {
        lv_bar_set_value(s_ui.calender_progress_bar, new_state->cal_progress_pct, LV_ANIM_OFF);
    }
    if (s_ui.calender_huangli_1 &&
        strcmp(s_display_state.cal_huangli_1, new_state->cal_huangli_1) != 0) {
        lv_label_set_text(s_ui.calender_huangli_1, new_state->cal_huangli_1);
    }
    if (s_ui.calender_huangli_2 &&
        strcmp(s_display_state.cal_huangli_2, new_state->cal_huangli_2) != 0) {
        lv_label_set_text(s_ui.calender_huangli_2, new_state->cal_huangli_2);
    }
    if (s_ui.calender_huangli_3 &&
        strcmp(s_display_state.cal_huangli_3, new_state->cal_huangli_3) != 0) {
        lv_label_set_text(s_ui.calender_huangli_3, new_state->cal_huangli_3);
    }
    if (s_ui.calender_huangli_4 &&
        strcmp(s_display_state.cal_huangli_4, new_state->cal_huangli_4) != 0) {
        lv_label_set_text(s_ui.calender_huangli_4, new_state->cal_huangli_4);
    }
    if (s_ui.calender_huangli_5 &&
        strcmp(s_display_state.cal_huangli_5, new_state->cal_huangli_5) != 0) {
        lv_label_set_text(s_ui.calender_huangli_5, new_state->cal_huangli_5);
    }
    if (s_ui.calender_huangli_6 &&
        strcmp(s_display_state.cal_huangli_6, new_state->cal_huangli_6) != 0) {
        lv_label_set_text(s_ui.calender_huangli_6, new_state->cal_huangli_6);
    }

    /* 定时器面板 */
    if (s_ui.timer_pv &&
        strcmp(s_display_state.timer_pv, new_state->timer_pv) != 0) {
        lv_label_set_text(s_ui.timer_pv, new_state->timer_pv);
    }
    if (s_ui.timer_sv &&
        strcmp(s_display_state.timer_sv, new_state->timer_sv) != 0) {
        lv_label_set_text(s_ui.timer_sv, new_state->timer_sv);
    }
    if (s_ui.timer_bell &&
        strcmp(s_display_state.timer_bell_text, new_state->timer_bell_text) != 0) {
        lv_label_set_text(s_ui.timer_bell, new_state->timer_bell_text);
    }

    s_display_state = *new_state;
    lvgl_port_unlock();
}

/* 统一刷新入口：先按 mask 填充显示状态，再一次性渲染到 UI */
static void clock_refresh(uint32_t mask)
{
    clock_display_state_t new_state = s_display_state;

    if (mask & REFRESH_TIME) {
        clock_prepare_time(&new_state);
    }
    if (mask & REFRESH_CALENDAR) {
        clock_prepare_calendar(&new_state);
    }
    if (mask & REFRESH_TIMER) {
        clock_prepare_timer(&new_state);
    }
    if (mask & REFRESH_NETWORK) {
        clock_prepare_network(&new_state);
    }

    clock_render(&new_state);
}

/* -------------------- 事件回调 -------------------- */

static void clock_home_cb(lv_event_t *e)
{
    (void)e;
    app_manager_request_kill_active();
}

static void clock_set_open_cb(lv_event_t *e)
{
    (void)e;
    lvgl_port_lock(portMAX_DELAY);
    if (s_ui.set) {
        lv_obj_clear_flag(s_ui.set, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_ui.set, LV_OBJ_FLAG_CLICKABLE);
    }
    lvgl_port_unlock();
}

static void clock_set_close_cb(lv_event_t *e)
{
    (void)e;
    lvgl_port_lock(portMAX_DELAY);
    if (s_ui.set) {
        lv_obj_add_flag(s_ui.set, LV_OBJ_FLAG_HIDDEN);
    }
    lvgl_port_unlock();
}

static void clock_panel_switch_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target_obj(e);

    if (target == s_ui.btn_clock) {
        clock_switch_panel(PANEL_CLOCK);
        clock_refresh(REFRESH_TIME);
    } else if (target == s_ui.btn_calender) {
        clock_switch_panel(PANEL_CALENDAR);
        /* 进入月历面板重置黄历为第一页、清空手动选中日期 */
        s_state.huangli_page = 0;
        s_state.cal_sel_valid = false;
        s_state.last_cal_us = 0;
        clock_refresh(REFRESH_CALENDAR);
    } else if (target == s_ui.btn_timer) {
        clock_switch_panel(PANEL_TIMER);
        clock_refresh(REFRESH_TIMER);
    }
}

static void clock_huangli_panel_click_cb(lv_event_t *e)
{
    (void)e;
    s_state.huangli_page = (s_state.huangli_page + 1) % HUANGLI_PAGE_COUNT;
    s_state.last_cal_us = 0;
}

/* 新闻面板显示期被点击：重置显示倒计时，让用户看完；隐藏态面板不可见，
 * 收不到点击，无需额外判分支 */
static void clock_news_panel_click_cb(lv_event_t *e)
{
    (void)e;
    if (s_state.news_visible && s_net.news_valid) {
        s_state.news_toggle_next_us = esp_timer_get_time() + NEWS_SHOW_DURATION_US;
    }
}

static void clock_timer_btn_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target_obj(e);

    /* 快捷按钮 */
    struct { lv_obj_t *obj; uint32_t sec; } quicks[] = {
        {s_ui.timer_quick_1min, 60},   {s_ui.timer_quick_3min, 180},
        {s_ui.timer_quick_5min, 300},  {s_ui.timer_quick_10min, 600},
        {s_ui.timer_quick_20min, 1200},{s_ui.timer_quick_30min, 1800},
        {s_ui.timer_quick_40min, 2400},{s_ui.timer_quick_50min, 3000},
        {s_ui.timer_quick_60min, 3600},
    };

    for (int i = 0; i < 9; i++) {
        if (target == quicks[i].obj) {
            timer_set_total(quicks[i].sec);
            return;
        }
    }

    if (target == s_ui.timer_btn_add_1min) {
        timer_set_total(s_state.timer_total_s + 60);
    } else if (target == s_ui.timer_btn_min_1min) {
        if (s_state.timer_total_s >= 60) {
            timer_set_total(s_state.timer_total_s - 60);
        }
    } else if (target == s_ui.timer_start_pause) {
        if (s_state.timer_state == TIMER_CONFIG) {
            timer_start();
        } else if (s_state.timer_state == TIMER_RUNNING) {
            timer_pause();
        } else if (s_state.timer_state == TIMER_PAUSED) {
            timer_resume();
        }
    } else if (target == s_ui.timer_reset) {
        timer_reset();
    }
}

static void clock_bell_toggle_cb(lv_event_t *e)
{
    (void)e;
    s_state.timer_bell = !s_state.timer_bell;
    clock_refresh(REFRESH_TIMER);
}

static void clock_calendar_changed_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target_obj(e);
    if (target != s_ui.calender) {
        return;
    }

    lv_calendar_date_t date;
    if (lv_calendar_get_pressed_date(target, &date) == LV_RESULT_OK) {
        s_state.cal_sel_year = date.year;
        s_state.cal_sel_month = date.month;
        s_state.cal_sel_day = date.day;
        s_state.cal_sel_valid = true;
        s_state.huangli_page = 0;
        s_state.last_cal_us = 0;
    }
}

/* -------------------- 日历 dropdown 初始化 -------------------- */

/* 初始化日历 dropdown 选项列表：2025-2050 可选 */
static void clock_init_calendar_dropdown(void)
{
    if (s_ui.calender == NULL) return;
    
    const char *year_list = "1995\n1996\n1997\n1998\n1999\n2000\n2001\n2002\n2003\n2004\n2005\n2006\n2007\n2008\n2009\n2010\n2011\n2012\n2013\n2014\n2015\n2016\n2017\n2018\n2019\n2020\n2021\n2022\n2023\n2024\n2025\n2026\n2027\n2028\n2029\n2030\n2031\n2032\n2033\n2034\n2035\n2036\n2037\n2038\n2039\n2040\n2041\n2042\n2043\n2044\n2045\n2046\n2047\n2048\n2049\n2050";
    
    lvgl_port_lock(portMAX_DELAY);
    /* 使用 LVGL 官方 API 设置年份下拉框的范围 */
    lv_calendar_header_dropdown_set_year_list(s_ui.calender, year_list);

    lvgl_port_unlock();
}

/* -------------------- 生命周期 -------------------- */

static bool app_clock_calendar_on_init(app_base_t *self, void *screen_ctx)
{
    (void)self;
    ESP_LOGI(TAG, "init");

    memset(&s_state, 0, sizeof(s_state));
    memset(&s_display_state, 0, sizeof(s_display_state));
    /* s_net（联网驻留缓存）不清零：本次上电全程有效，30min 节流跨会话生效 */
    s_state.use_12h = service_nvs_get_clock_12h();
    s_state.timer_total_s = service_nvs_get_clock_timer_s();
    s_state.timer_remain_s = s_state.timer_total_s;
    s_state.timer_state = TIMER_CONFIG;
    s_state.timer_bell = true;

    ui_screen_clock_t *ui = (ui_screen_clock_t *)screen_ctx;

    lvgl_port_lock(portMAX_DELAY);

    /* 设置面板下拉初始化 */
    if (ui->set_12_24h) {
        // lv_dropdown_set_options(ui->set_12_24h, "24小时制\n12小时制");
        lv_dropdown_set_selected(ui->set_12_24h, s_state.use_12h ? 1 : 0);
    }

    /* 隐藏设置面板 */
    if (ui->set) lv_obj_add_flag(ui->set, LV_OBJ_FLAG_HIDDEN);

    /* 天气/一言：有驻留缓存直接显示，无则隐藏待联网获取 */
    if (ui->panel_weather && !s_net.weather.valid) {
        lv_obj_add_flag(ui->panel_weather, LV_OBJ_FLAG_HIDDEN);
    }
    if (ui->clock_hitokoto && s_net.hitokoto_buf[0] == '\0') {
        lv_obj_add_flag(ui->clock_hitokoto, LV_OBJ_FLAG_HIDDEN);
    }

    /* 新闻面板初始隐藏，并动态创建 3 条标题 label（on_destroy 成对删除，
     * EEZ 屏幕对象全局复用，不删会随重开次数累积） */
    if (ui->panel_news) {
        for (int i = 0; i < NEWS_TITLE_COUNT; i++) {
            ui->news_labels[i] = lv_label_create(ui->panel_news);
            lv_obj_set_size(ui->news_labels[i], LV_PCT(100), LV_SIZE_CONTENT);
            // lv_label_set_long_mode(ui->news_labels[i], LV_LABEL_LONG_SCROLL_CIRCULAR);
            lv_label_set_text(ui->news_labels[i], "");
            lv_obj_clear_flag(ui->news_labels[i], LV_OBJ_FLAG_CLICKABLE);
        }
        lv_obj_add_flag(ui->panel_news, LV_OBJ_FLAG_HIDDEN);
        /* 显示期点击重置显示计时（用户想仔细看），隐藏态不可见收不到点击 */
        lv_obj_add_flag(ui->panel_news, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ui->panel_news, clock_news_panel_click_cb, LV_EVENT_CLICKED, NULL);
    }

    /* 注册事件回调 */
    if (ui->btn_home) lv_obj_add_event_cb(ui->btn_home, clock_home_cb, LV_EVENT_CLICKED, NULL);
    if (ui->btn_set) lv_obj_add_event_cb(ui->btn_set, clock_set_open_cb, LV_EVENT_CLICKED, NULL);
    if (ui->set_btn_return) lv_obj_add_event_cb(ui->set_btn_return, clock_set_close_cb, LV_EVENT_CLICKED, NULL);
    if (ui->btn_clock) lv_obj_add_event_cb(ui->btn_clock, clock_panel_switch_cb, LV_EVENT_CLICKED, NULL);
    if (ui->btn_calender) lv_obj_add_event_cb(ui->btn_calender, clock_panel_switch_cb, LV_EVENT_CLICKED, NULL);
    if (ui->btn_timer) lv_obj_add_event_cb(ui->btn_timer, clock_panel_switch_cb, LV_EVENT_CLICKED, NULL);
    if (ui->timer_bell) lv_obj_add_event_cb(ui->timer_bell, clock_bell_toggle_cb, LV_EVENT_CLICKED, NULL);
    if (ui->calender) lv_obj_add_event_cb(ui->calender, clock_calendar_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* 定时器按钮组 */
    lv_obj_t *timer_btns[] = {
        ui->timer_quick_1min, ui->timer_quick_3min, ui->timer_quick_5min,
        ui->timer_quick_10min, ui->timer_quick_20min, ui->timer_quick_30min,
        ui->timer_quick_40min, ui->timer_quick_50min, ui->timer_quick_60min,
        ui->timer_btn_min_1min, ui->timer_btn_add_1min,
        ui->timer_start_pause, ui->timer_reset,
    };
    for (int i = 0; i < 13; i++) {
        if (timer_btns[i]) lv_obj_add_event_cb(timer_btns[i], clock_timer_btn_cb, LV_EVENT_CLICKED, NULL);
    }

    /* 黄历 panel 点击翻页；子 label 不拦截点击，确保事件落到 panel */
    if (ui->calender_panel_huangli) {
        lv_obj_add_flag(ui->calender_panel_huangli, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ui->calender_panel_huangli, clock_huangli_panel_click_cb, LV_EVENT_CLICKED, NULL);
    }
    lv_obj_t *huangli_labels[] = {
        ui->calender_huangli_1, ui->calender_huangli_2, ui->calender_huangli_3,
        ui->calender_huangli_4, ui->calender_huangli_5, ui->calender_huangli_6,
    };
    for (int i = 0; i < 6; i++) {
        if (huangli_labels[i]) {
            lv_obj_clear_flag(huangli_labels[i], LV_OBJ_FLAG_CLICKABLE);
        }
    }

    lvgl_port_unlock();

    /* 日历箭头样式 */
    clock_init_calendar_dropdown();

    /* 新闻默认隐藏；进入后一个隐藏周期即可见（news_valid 置位前轮换不会启动） */
    s_state.news_toggle_next_us = esp_timer_get_time() + NEWS_HIDE_DURATION_US;
    s_state.news_visible = false;

    /* 切到前台时请求立即把 RTC 同步到系统时钟，避免显示滞后 */
    service_rtc_request_sync();

    /* 默认显示时钟面板，并预填充三面板数据避免切屏空白/跳变；
     * 联网数据用驻留缓存立即渲染，消除打开后数秒空白 */
    clock_switch_panel(PANEL_CLOCK);
    clock_refresh(REFRESH_TIME | REFRESH_CALENDAR | REFRESH_TIMER | REFRESH_NETWORK);

    /* 联网拉取不在这里直接发起：首个 on_update 的 clock_poll_network 会按
     * 驻留时间戳裁决（首次未取过或超过 30min 才拉），避免重开重复请求 */

    return true;
}

static void app_clock_calendar_on_update(app_base_t *self)
{
    (void)self;
    int64_t now = esp_timer_get_time();
    uint32_t mask = 0;

    /* 时钟面板每秒刷新 */
    if (s_state.active_panel == PANEL_CLOCK) {
        mask |= REFRESH_TIME;
    }

    /* 月历面板节流刷新 */
    if (s_state.active_panel == PANEL_CALENDAR &&
        (now - s_state.last_cal_us) >= CAL_REFRESH_INTERVAL_US) {
        s_state.last_cal_us = now;
        mask |= REFRESH_CALENDAR;
    }

    /* 定时器 */
    timer_process();
    mask |= REFRESH_TIMER;

    /* 网络轮询：仅返回需刷新的掩码 */
    mask |= clock_poll_network();

    /* 设置面板下拉轮询：12h/24h 切换 */
    lvgl_port_lock(portMAX_DELAY);
    if (s_ui.set_12_24h) {
        uint32_t sel = lv_dropdown_get_selected(s_ui.set_12_24h);
        bool new_12h = (sel == 1);
        if (new_12h != s_state.use_12h) {
            s_state.use_12h = new_12h;
            service_nvs_set_clock_12h(new_12h);
            mask |= REFRESH_TIME; /* 强制刷新时钟 */
        }
    }
    lvgl_port_unlock();

    if (mask != 0) {
        clock_refresh(mask);
    }
}

static void app_clock_calendar_on_pause(app_base_t *self)
{
    (void)self;
    /* 暂停时若定时器正在运行，保持状态但不计时 */
    ESP_LOGI(TAG, "pause");
}

static void app_clock_calendar_on_resume(app_base_t *self)
{
    (void)self;
    /* 恢复后重置 tick 基准，避免补偿大量 tick */
    if (s_state.timer_state == TIMER_RUNNING) {
        s_state.timer_next_tick_us = esp_timer_get_time() + 1000000;
    }
    /* 切回前台时请求立即同步 RTC，避免后台期间系统时钟漂移 */
    service_rtc_request_sync();
    /* 恢复后立即刷新当前面板，避免时间/定时器跳变 */
    clock_refresh(REFRESH_TIME | REFRESH_TIMER);
    ESP_LOGI(TAG, "resume");
}

static void app_clock_calendar_on_destroy(app_base_t *self)
{
    (void)self;

    lvgl_port_lock(portMAX_DELAY);

    /* 移除所有事件回调 */
    if (s_ui.btn_home) lv_obj_remove_event_cb(s_ui.btn_home, clock_home_cb);
    if (s_ui.btn_set) lv_obj_remove_event_cb(s_ui.btn_set, clock_set_open_cb);
    if (s_ui.set_btn_return) lv_obj_remove_event_cb(s_ui.set_btn_return, clock_set_close_cb);
    if (s_ui.btn_clock) lv_obj_remove_event_cb(s_ui.btn_clock, clock_panel_switch_cb);
    if (s_ui.btn_calender) lv_obj_remove_event_cb(s_ui.btn_calender, clock_panel_switch_cb);
    if (s_ui.btn_timer) lv_obj_remove_event_cb(s_ui.btn_timer, clock_panel_switch_cb);
    if (s_ui.timer_bell) lv_obj_remove_event_cb(s_ui.timer_bell, clock_bell_toggle_cb);
    if (s_ui.calender) lv_obj_remove_event_cb(s_ui.calender, clock_calendar_changed_cb);
    if (s_ui.calender_panel_huangli) lv_obj_remove_event_cb(s_ui.calender_panel_huangli, clock_huangli_panel_click_cb);
    if (s_ui.panel_news) lv_obj_remove_event_cb(s_ui.panel_news, clock_news_panel_click_cb);

    /* 删除动态创建的新闻标题 label，防重开累积（EEZ 面板全局复用） */
    for (int i = 0; i < NEWS_TITLE_COUNT; i++) {
        if (s_ui.news_labels[i] != NULL) {
            lv_obj_del(s_ui.news_labels[i]);
            s_ui.news_labels[i] = NULL;
        }
    }

    lv_obj_t *timer_btns[] = {
        s_ui.timer_quick_1min, s_ui.timer_quick_3min, s_ui.timer_quick_5min,
        s_ui.timer_quick_10min, s_ui.timer_quick_20min, s_ui.timer_quick_30min,
        s_ui.timer_quick_40min, s_ui.timer_quick_50min, s_ui.timer_quick_60min,
        s_ui.timer_btn_min_1min, s_ui.timer_btn_add_1min,
        s_ui.timer_start_pause, s_ui.timer_reset,
    };
    for (int i = 0; i < 13; i++) {
        if (timer_btns[i]) lv_obj_remove_event_cb(timer_btns[i], clock_timer_btn_cb);
    }

    lvgl_port_unlock();

    ESP_LOGI(TAG, "destroy");
}

/* -------------------- 注册 -------------------- */

esp_err_t app_clock_calendar_register(void)
{
    static app_base_t app = {
        .name = "Clock Calendar",
        .screen_name = "app_clock",
        .screen_ctx = &s_ui,
        .screen_ctx_size = sizeof(s_ui),
        .widget_bindings = s_clock_bindings,
        .on_init = app_clock_calendar_on_init,
        .on_update = app_clock_calendar_on_update,
        .on_pause = app_clock_calendar_on_pause,
        .on_resume = app_clock_calendar_on_resume,
        .on_destroy = app_clock_calendar_on_destroy,
    };
    return app_manager_register(&app);
}
