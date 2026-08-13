/**
 * @file app_fun.c
 * @brief Fun App：答案之书 + 塔罗牌（纯 C 实现）
 */

#include "app_fun.h"
#include "app_manager.h"
#include "service_nvs.h"
#include "service_i18n.h"
/* Trap: sdkconfig.h 必须先于 CONFIG_BOARD_HAS_IMU 门控包含 */
#include "sdkconfig.h"
#if CONFIG_BOARD_HAS_IMU
#include "bmi270.h"
#endif
#include "book_of_answer.h"
#include "board_hal.h"
#include "driver/i2c_types.h"
#include "engine_gui.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "tarot.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "app_fun";

/* 翻转检测阈值（使用滞后，避免边界抖动） */
#define UPRIGHT_Z_THRESHOLD -0.5f // IDLE 状态：z < 此值视为朝上
#define COVERED_Z_THRESHOLD 0.7f  // IDLE 状态：z > 此值视为覆盖
/* 从 COVERED 返回 IDLE 需要更严格的朝上条件 */
#define RETURN_UPRIGHT_Z -0.7f
/* 从 IDLE 进入 COVERED 需要更严格的覆盖条件 */
#define ENTER_COVERED_Z 0.7f
/* 覆盖最短持续时间 (us)，防止误触 */
#define COVERED_MIN_DURATION_US 200000
/* 两次触发最小间隔 (us) */
#define TRIGGER_COOLDOWN_US 1000000

/* 能量系统：每日 100% 刷新，塔罗抽卡每次随机消耗 10~50 */
#define FUN_MANA_MAX        100
#define FUN_MANA_COST_MIN   10
#define FUN_MANA_COST_MAX   50
#define FUN_MANA_BAR_UNITS 20 /*!< 100% 对应 20 个等号宽度单位 */

/* ==================== UI Context ==================== */

typedef struct
{
    lv_obj_t *fun_btn_home;
    lv_obj_t *fun_btn_set;
    lv_obj_t *fun_panel_book;
    lv_obj_t *panel_book_open;
    lv_obj_t *book_answer_text;
    lv_obj_t *book_answer_detail;
    lv_obj_t *panel_book_close;
    lv_obj_t *fun_panel_tarot;
    lv_obj_t *panel_tarot_open_1;
    lv_obj_t *tarot_card_1;
    lv_obj_t *tarot_card_reversed_1;
    lv_obj_t *tarot_card_detail_panel_1;
    lv_obj_t *tarot_card_detail_text_1;
    lv_obj_t *panel_tarot_close_1;
    lv_obj_t *panel_tarot_open_2;
    lv_obj_t *tarot_card_2;
    lv_obj_t *tarot_card_reversed_2;
    lv_obj_t *tarot_card_detail_panel_2;
    lv_obj_t *tarot_card_detail_text_2;
    lv_obj_t *panel_tarot_close_2;
    lv_obj_t *panel_tarot_open_3;
    lv_obj_t *tarot_card_3;
    lv_obj_t *tarot_card_reversed_3;
    lv_obj_t *tarot_card_detail_panel_3;
    lv_obj_t *tarot_card_detail_text_3;
    lv_obj_t *panel_tarot_close_3;
    lv_obj_t *fun_tip_label;
    lv_obj_t *fun_set_panel;
    lv_obj_t *fun_btn_book;
    lv_obj_t *fun_btn_tarot;
} ui_screen_fun_t;

/* ==================== State ==================== */

typedef enum
{
    FUN_MODE_NONE,
    FUN_MODE_BOOK,
    FUN_MODE_TAROT
} fun_mode_t;

/** 全局运行状态机：初始 -> 抽取 -> 展示 -> 初始 */
typedef enum
{
    FUN_APP_STATE_IDLE,    // 等待翻转
    FUN_APP_STATE_DRAWING, // 抽取/动画中
    FUN_APP_STATE_SHOWING, // 展示结果
} fun_app_state_t;

/** IMU 翻转状态机 */
typedef enum
{
    IMU_FLIP_STATE_IDLE,    // 屏幕朝上
    IMU_FLIP_STATE_COVERED, // 屏幕朝下
} imu_flip_state_t;

typedef struct
{
    bool enabled;
    int selected_index;
} book_state_t;

typedef struct
{
    bool enabled;
    int drawn_cards[3];
    bool reversed[3]; // 每张牌是否逆位
    int current_slot; // 当前正在动画的牌位 (0-2)
} tarot_state_t;

typedef struct
{
#if CONFIG_BOARD_HAS_IMU
    bmi270_handle_t *sensor;
#endif
    bool initialized;
    imu_flip_state_t flip_state;
    int64_t covered_since_us; // 进入 COVERED 状态的时间
    int64_t last_trigger_us;  // 上次成功触发翻转的时间
} imu_state_t;

static ui_screen_fun_t s_ui = {0};
static fun_mode_t s_mode = FUN_MODE_NONE;
static fun_app_state_t s_app_state = FUN_APP_STATE_IDLE;
static book_state_t s_book = {0};
static tarot_state_t s_tarot = {0};
static imu_state_t s_imu = {0};

/** 能量运行时状态 */
static struct {
    uint8_t mana;       /*!< 当前能量 0-100 */
    bool    need_save;  /*!< 有未写入 NVS 的能量变化 */
    bool    depleted_alt; /*!< 耗尽后重复抽取时，能量条与提醒交替展示 */
} s_mana = {0};

static void tarot_reset_ui(void);
static void book_reset_cover(void);
static void fun_mana_load_and_refresh(void);
static void fun_mana_show_bar(void);
static bool fun_mana_try_consume(int *out_cost);

/* ==================== Animation Callbacks ==================== */

static void cover_width_anim_cb(void *obj, int32_t width_value)
{
    lv_obj_set_width((lv_obj_t *)obj, (lv_coord_t)width_value);
}

static void close_x_anim_cb(void *obj, int32_t x_value)
{
    lv_obj_set_x((lv_obj_t *)obj, x_value);
}

/* ==================== Widgets Binding ==================== */

static const widget_binding_t s_fun_bindings[] = {
    WIDGET_BIND(ui_screen_fun_t, fun_btn_home, "fun_btn_home", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_fun_t, fun_btn_set, "fun_btn_set", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_fun_t, fun_panel_book, "fun_panel_book", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_fun_t, panel_book_open, "panel_book_open", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_fun_t, book_answer_text, "book_answer_text", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_fun_t, book_answer_detail, "book_answer_detail", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_fun_t, panel_book_close, "panel_book_close", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_fun_t, fun_panel_tarot, "fun_panel_tarot", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_fun_t, panel_tarot_open_1, "panel_tarot_open_1", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_fun_t, tarot_card_1, "tarot_card_1", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_fun_t, tarot_card_reversed_1, "tarot_card_reversed_1", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_fun_t, tarot_card_detail_panel_1, "tarot_card_detail_panel_1", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_fun_t, tarot_card_detail_text_1, "tarot_card_detail_text_1", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_fun_t, panel_tarot_close_1, "panel_tarot_close_1", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_fun_t, panel_tarot_open_2, "panel_tarot_open_2", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_fun_t, tarot_card_2, "tarot_card_2", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_fun_t, tarot_card_reversed_2, "tarot_card_reversed_2", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_fun_t, tarot_card_detail_panel_2, "tarot_card_detail_panel_2", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_fun_t, tarot_card_detail_text_2, "tarot_card_detail_text_2", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_fun_t, panel_tarot_close_2, "panel_tarot_close_2", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_fun_t, panel_tarot_open_3, "panel_tarot_open_3", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_fun_t, tarot_card_3, "tarot_card_3", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_fun_t, tarot_card_reversed_3, "tarot_card_reversed_3", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_fun_t, tarot_card_detail_panel_3, "tarot_card_detail_panel_3", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_fun_t, tarot_card_detail_text_3, "tarot_card_detail_text_3", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_fun_t, panel_tarot_close_3, "panel_tarot_close_3", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_fun_t, fun_tip_label, "fun_tip_label", WIDGET_KIND_LABEL),
    WIDGET_BIND(ui_screen_fun_t, fun_set_panel, "fun_set", WIDGET_KIND_ANY),
    WIDGET_BIND(ui_screen_fun_t, fun_btn_book, "fun_btn_book", WIDGET_KIND_BUTTON),
    WIDGET_BIND(ui_screen_fun_t, fun_btn_tarot, "fun_btn_tarot", WIDGET_KIND_BUTTON),
    WIDGET_BINDING_END,
};

/* ==================== IMU Functions ==================== */

static esp_err_t imu_init(void)
{
#if !CONFIG_BOARD_HAS_IMU
    /* 无 IMU 板型：App 壳保留可进入，翻转交互整体禁用 */
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (s_imu.initialized)
        return ESP_OK;

    s_imu.sensor = NULL;
    const bmi270_driver_config_t cfg = {
        .addr = BMI270_I2C_ADDRESS_L,
        .interface = BMI270_USE_I2C,
        .i2c_bus = (i2c_master_bus_handle_t)board_i2c_get_handle(),
    };

    /* Trap: 热复位（reset 键/panic/欠压）不给 IMU 断电，传感器残留脏状态，
     * 首次 bmi270_create 内部 soft_reset 后读寄存器常失败（ESP_ERR_INVALID_STATE）。
     * 每次 create 内部都会再做一次 soft_reset，重试几次即可等传感器稳定。 */
    esp_err_t ret = ESP_FAIL;
    for (int attempt = 0; attempt < 3; attempt++) {
        ret = bmi270_create(&cfg, &s_imu.sensor);
        if (ret == ESP_OK) {
            break;
        }
        ESP_LOGW(TAG, "BMI270 create attempt %d failed: %s",
                 attempt + 1, esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "BMI270 not found: %s", esp_err_to_name(ret));
        return ret;
    }

    uint8_t chip_id;
    bmi270_get_chip_id(s_imu.sensor, &chip_id);
    ESP_LOGI(TAG, "BMI270 chip ID: 0x%02X", chip_id);

    bmi270_config_t start_cfg = {
        .acce_odr = BMI270_ACC_ODR_100_HZ,
        .acce_range = BMI270_ACC_RANGE_2_G,
        .gyro_odr = BMI270_GYR_ODR_200_HZ,
        .gyro_range = BMI270_GYR_RANGE_2000_DPS,
    };
    ret = bmi270_start(s_imu.sensor, &start_cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "bmi270 start failed: %s", esp_err_to_name(ret));
        bmi270_delete(s_imu.sensor);
        s_imu.sensor = NULL;
        return ret;
    }

    s_imu.flip_state = IMU_FLIP_STATE_IDLE;
    s_imu.covered_since_us = 0;
    s_imu.last_trigger_us = 0;
    s_imu.initialized = true;

    /* 首次读取可能需要等待数据稳定 */
    float x, y, z;
    for (int i = 0; i < 10; i++)
    {
        vTaskDelay(pdMS_TO_TICKS(5));
        if (bmi270_get_acce_data(s_imu.sensor, &x, &y, &z) == ESP_OK)
        {
            if (fabsf(x) > 0.01f || fabsf(y) > 0.01f || fabsf(z) > 0.01f)
            {
                break;
            }
        }
    }
    ESP_LOGI(TAG, "IMU first data: (%.2f, %.2f, %.2f)", x, y, z);

    return ESP_OK;
#endif /* CONFIG_BOARD_HAS_IMU */
}

static void imu_deinit(void)
{
#if CONFIG_BOARD_HAS_IMU
    if (s_imu.sensor && s_imu.initialized)
    {
        bmi270_stop(s_imu.sensor);
        bmi270_delete(s_imu.sensor);
        s_imu.sensor = NULL;
        s_imu.initialized = false;
    }
#endif
}

static bool imu_get_acce(float *x, float *y, float *z)
{
#if !CONFIG_BOARD_HAS_IMU
    (void)x; (void)y; (void)z;
    return false;
#else
    if (!s_imu.sensor || !s_imu.initialized)
        return false;
    /* 临时监测：I2C 读取耗时异常会拖死 task_app（on_update 在其上下文），
     * 超时告警定位“进去就卡死”是否由 IMU 总线挂起引起 */
    int64_t t0 = esp_timer_get_time();
    esp_err_t ret = bmi270_get_acce_data(s_imu.sensor, x, y, z);
    int64_t dt = esp_timer_get_time() - t0;
    if (dt > 20000) {
        ESP_LOGW(TAG, "IMU i2c slow: %lld us ret=%s", dt, esp_err_to_name(ret));
    }
    if (ret != ESP_OK) {
        static uint32_t s_imu_fail_cnt = 0;
        s_imu_fail_cnt++;
        if (s_imu_fail_cnt <= 3 || (s_imu_fail_cnt % 1000) == 0) {
            ESP_LOGW(TAG, "IMU read fail #%lu: %s", (unsigned long)s_imu_fail_cnt,
                     esp_err_to_name(ret));
        }
        return false;
    }
    return true;
#endif /* CONFIG_BOARD_HAS_IMU */
}

/**
 * @brief 检测一次完整的翻转动作（朝上→覆盖→朝上）
 *
 * 纯 I2C 读取 + 状态机，不做任何 LVGL 操作（封面 UI 由调用方持锁补做）。
 * Trap: I2C 事务达 ms 级，禁止把本函数包进 LVGL 锁——持锁做 I2C 会让
 * taskLVGL 渲染/触摸分发长时间阻塞，且与触摸路径形成 lvgl→i2c 锁序交叉。
 *
 * @param[out] out_entered_covered 本拍进入 COVERED（调用方需做封面 UI）
 * @return true 表示检测到有效翻转
 */
static bool imu_detected_flip(bool *out_entered_covered)
{
    float x, y, z;
    *out_entered_covered = false;
    if (!imu_get_acce(&x, &y, &z))
        return false;

    int64_t now = esp_timer_get_time();

    switch (s_imu.flip_state)
    {
    case IMU_FLIP_STATE_IDLE:
        // 在 IDLE 状态，只有当 z 明确大于阈值时才认为是覆盖
        if (z > ENTER_COVERED_Z)
        {
            s_imu.flip_state = IMU_FLIP_STATE_COVERED;
            s_imu.covered_since_us = now;
            *out_entered_covered = true;    /* 封面 UI 由调用方持锁补做 */
            ESP_LOGI(TAG, "Enter COVERED, z=%.2f", z);
        }
        break;

    case IMU_FLIP_STATE_COVERED:
        // 在 COVERED 状态，只有 z 明确小于负阈值时才认为翻回朝上
        if (z < RETURN_UPRIGHT_Z)
        {
            // 检查覆盖持续时间是否足够
            if ((now - s_imu.covered_since_us) >= COVERED_MIN_DURATION_US)
            {
                // 检查冷却时间
                if ((now - s_imu.last_trigger_us) >= TRIGGER_COOLDOWN_US)
                {
                    s_imu.flip_state = IMU_FLIP_STATE_IDLE;
                    s_imu.last_trigger_us = now;
                    ESP_LOGI(TAG, "Flip trigger! z=%.2f", z);
                    return true;
                }
                else
                {
                    // 冷却中，只复位状态但不触发
                    s_imu.flip_state = IMU_FLIP_STATE_IDLE;
                    ESP_LOGI(TAG, "Flip ignored (cooldown), z=%.2f", z);
                }
            }
            else
            {
                // 覆盖时间太短，认为是误触，回到 IDLE 但不触发
                s_imu.flip_state = IMU_FLIP_STATE_IDLE;
                ESP_LOGI(TAG, "Flip too short, ignored");
            }
        }
        // 注意：非朝上非覆盖状态（如 -0.7 <= z <= 0.7）保持 COVERED，不切回 IDLE
        break;
    }
    return false;
}

/* ==================== Mana System ==================== */

/**
 * @brief 把能量渲染为蓝条通知文本
 * @param[in] mana 0-100
 * @param[out] buf 输出缓冲区
 * @param[in] buf_size 缓冲区大小
 *
 * 20 个等号宽度单位为 100%，空格两个等宽于一个等号。
 */
static void fun_mana_format_bar(int mana, char *buf, size_t buf_size)
{
    int filled = (mana + 2) / 5; /* 四舍五入到 5% */
    if (filled < 0)
        filled = 0;
    if (filled > FUN_MANA_BAR_UNITS)
        filled = FUN_MANA_BAR_UNITS;
    int empty = FUN_MANA_BAR_UNITS - filled;

    char bar[64];
    int pos = 0;
    bar[pos++] = '[';
    for (int i = 0; i < filled; i++)
        bar[pos++] = '=';
    for (int i = 0; i < empty * 2; i++)
        bar[pos++] = ' ';
    bar[pos++] = ']';
    bar[pos] = '\0';

    snprintf(buf, buf_size, "MP %s %d%%", bar, mana);
}

/**
 * @brief 在通知栏显示当前能量条
 */
static void fun_mana_show_bar(void)
{
    char text[80];
    fun_mana_format_bar(s_mana.mana, text, sizeof(text));
    app_manager_show_notification_timeout(text, 0);
}

/**
 * @brief 将当前能量与日期写入 NVS，掉点保存进度
 */
static void fun_mana_persist(void)
{
    struct tm now = {0};
    uint16_t year = 0;
    uint8_t month = 0;
    uint8_t day = 0;

    if (app_manager_get_time(&now) == ESP_OK) {
        year = (uint16_t)(now.tm_year + 1900);
        month = (uint8_t)(now.tm_mon + 1);
        day = (uint8_t)now.tm_mday;
    }

    service_nvs_app_fun_mana_t saved = {
        .mana = s_mana.mana,
        .year = year,
        .month = month,
        .day = day,
        .reserved = 0,
    };
    esp_err_t ret = service_nvs_set_app_fun_mana(&saved);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "能量保存失败: %s", esp_err_to_name(ret));
    }
}

/**
 * @brief 从 NVS 加载能量并检查每日刷新
 */
static void fun_mana_load_and_refresh(void)
{
    service_nvs_app_fun_mana_t saved = {0};
    esp_err_t ret = service_nvs_get_app_fun_mana(&saved);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "读取能量失败: %s", esp_err_to_name(ret));
        saved.mana = FUN_MANA_MAX;
        saved.year = 0;
    }
    if (saved.mana > FUN_MANA_MAX) {
        saved.mana = FUN_MANA_MAX;
    }

    struct tm now = {0};
    bool date_ok = (app_manager_get_time(&now) == ESP_OK);
    bool need_refresh = false;

    if (date_ok && (saved.year == 0 || saved.year != (uint16_t)(now.tm_year + 1900) ||
                    saved.month != (uint8_t)(now.tm_mon + 1) || saved.day != (uint8_t)now.tm_mday)) {
        need_refresh = true;
        ESP_LOGI(TAG, "能量跨日刷新: %04u-%02u-%02u", (uint16_t)(now.tm_year + 1900),
                 now.tm_mon + 1, now.tm_mday);
    }

    s_mana.mana = need_refresh ? FUN_MANA_MAX : saved.mana;
    s_mana.need_save = (need_refresh || s_mana.mana != saved.mana);

    if (s_mana.need_save) {
        fun_mana_persist();
        s_mana.need_save = false;
    }

    fun_mana_show_bar();
}

/**
 * @brief 尝试为一次抽卡消耗能量
 * @param[out] out_cost 成功时输出本次消耗值；可传 NULL
 * @return true 消耗成功，可以抽卡；false 能量不足，已提示
 */
static bool fun_mana_try_consume(int *out_cost)
{
    if (s_mana.mana == 0) {
        /* 耗尽后仍尝试抽取：能量条与提醒交替展示，避免单一文案被忽略 */
        s_mana.depleted_alt = !s_mana.depleted_alt;
        if (s_mana.depleted_alt) {
            app_manager_show_notification_insert_timeout(_("能量已耗尽，明日自动回满"), 3000);
        } else {
            fun_mana_show_bar();
        }
        return false;
    }

    uint32_t range = (FUN_MANA_COST_MAX - FUN_MANA_COST_MIN + 1);
    uint32_t cost = FUN_MANA_COST_MIN + (esp_random() % range);
    if (cost >= s_mana.mana) {
        s_mana.mana = 0;
    } else {
        s_mana.mana -= (uint8_t)cost;
    }

    ESP_LOGI(TAG, "抽卡消耗能量 %lu，剩余 %u", (unsigned long)cost, s_mana.mana);
    s_mana.need_save = true;
    if (out_cost != NULL) {
        *out_cost = (int)cost;
    }
    fun_mana_show_bar();
    return true;
}

/* ==================== Answer Book Logic ==================== */

static void book_cover_anim_completed_cb(lv_anim_t *a)
{
    (void)a;
    s_app_state = FUN_APP_STATE_SHOWING;
    if (s_ui.fun_tip_label)
        lv_label_set_text(s_ui.fun_tip_label, _("再次翻转查看下一则答案"));
}

static void book_start_draw(void)
{
    s_app_state = FUN_APP_STATE_DRAWING;

    int idx = (int)esp_random() % ANSWER_COUNT;
    s_book.selected_index = idx;

    const char *zh_title = answer_list[idx][0][0];
    const char *zh_desc = answer_list[idx][0][1];
    ESP_LOGI(TAG, "Answer: %s", zh_title);

    if (s_ui.book_answer_text)
        lv_label_set_text(s_ui.book_answer_text, zh_title);
    if (s_ui.book_answer_detail)
        lv_label_set_text(s_ui.book_answer_detail, zh_desc);
    if (s_ui.fun_tip_label)
        lv_label_set_text(s_ui.fun_tip_label, "");

    lv_obj_t *obj = s_ui.panel_book_close;
    if (!obj)
        return;

    /* 重入防护：删除同一对象上可能残留的旧动画，避免新旧动画叠加竞争 */
    lv_anim_delete(obj, NULL);

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_x(obj, 433);
    lv_obj_set_width(obj, 350);

    lv_anim_t a_width;
    lv_anim_init(&a_width);
    lv_anim_set_var(&a_width, obj);
    lv_anim_set_exec_cb(&a_width, (lv_anim_exec_xcb_t)cover_width_anim_cb);
    lv_anim_set_values(&a_width, 350, 0);
    lv_anim_set_duration(&a_width, 1000);
    lv_anim_set_path_cb(&a_width, lv_anim_path_linear);
    lv_anim_set_completed_cb(&a_width, book_cover_anim_completed_cb);
    lv_anim_start(&a_width);
}

static void book_on_flip(void)
{
    if (!s_book.enabled)
        return;
    if (s_app_state == FUN_APP_STATE_IDLE || s_app_state == FUN_APP_STATE_SHOWING)
    {
        int cost = 0;
        if (!fun_mana_try_consume(&cost))
        {
            if (s_ui.fun_tip_label)
                lv_label_set_text(s_ui.fun_tip_label, _("能量耗尽，明日恢复后再抽取"));
            return;
        }
        app_manager_show_notificationf_insert_timeout(3000, _("本次消耗 %d 能量，剩余 %d%%"), cost, s_mana.mana);
        book_start_draw();
    }
}

static void book_reset_cover(void)
{
    if (s_ui.panel_book_close)
    {
        lv_obj_clear_flag(s_ui.panel_book_close, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(s_ui.panel_book_close, 433);
        lv_obj_set_width(s_ui.panel_book_close, 350);
    }
}

/* ==================== Tarot Logic ==================== */

static void tarot_get_card_widgets(int slot, lv_obj_t **close_panel, lv_obj_t **open_panel,
                                   lv_obj_t **name_label, lv_obj_t **pos_label)
{
    *close_panel = *open_panel = *name_label = *pos_label = NULL;
    switch (slot)
    {
    case 0:
        *close_panel = s_ui.panel_tarot_close_1;
        *open_panel = s_ui.panel_tarot_open_1;
        *name_label = s_ui.tarot_card_1;
        *pos_label = s_ui.tarot_card_reversed_1;
        break;
    case 1:
        *close_panel = s_ui.panel_tarot_close_2;
        *open_panel = s_ui.panel_tarot_open_2;
        *name_label = s_ui.tarot_card_2;
        *pos_label = s_ui.tarot_card_reversed_2;
        break;
    case 2:
        *close_panel = s_ui.panel_tarot_close_3;
        *open_panel = s_ui.panel_tarot_open_3;
        *name_label = s_ui.tarot_card_3;
        *pos_label = s_ui.tarot_card_reversed_3;
        break;
    }
}

static void tarot_draw_all_cards(void)
{
    for (int slot = 0; slot < 3; slot++)
    {
        int idx = (int)esp_random() % TAROT_TOTAL_COUNT;
        bool reversed = (esp_random() % 2) == 1;
        s_tarot.drawn_cards[slot] = idx;
        s_tarot.reversed[slot] = reversed;

        const tarot_card_t *card = tarot_get_card_by_index(idx);
        if (!card)
            continue;

        lv_obj_t *close_panel, *open_panel, *name_label, *pos_label;
        tarot_get_card_widgets(slot, &close_panel, &open_panel, &name_label, &pos_label);
        ESP_LOGI(TAG, "Tarot card %d: %s (%s)", slot + 1, card->name, reversed ? "逆位↓" : "正位↑");

        if (name_label)
            lv_label_set_text(name_label, card->name);
        if (pos_label)
        {
            lv_label_set_text(pos_label, reversed ? "逆位" : "正位");
            lv_obj_set_style_text_color(pos_label, engine_gui_theme_color(reversed ? COLOR_ERROR : COLOR_PRIMARY), 0);
        }
    }
}

static void tarot_hide_all_details(void)
{
    if (s_ui.tarot_card_detail_panel_1)
        lv_obj_add_flag(s_ui.tarot_card_detail_panel_1, LV_OBJ_FLAG_HIDDEN);
    if (s_ui.tarot_card_detail_panel_2)
        lv_obj_add_flag(s_ui.tarot_card_detail_panel_2, LV_OBJ_FLAG_HIDDEN);
    if (s_ui.tarot_card_detail_panel_3)
        lv_obj_add_flag(s_ui.tarot_card_detail_panel_3, LV_OBJ_FLAG_HIDDEN);
}

static void tarot_reset_ui(void)
{
    for (int slot = 0; slot < 3; slot++)
    {
        lv_obj_t *close_panel, *open_panel, *name_label, *pos_label;
        tarot_get_card_widgets(slot, &close_panel, &open_panel, &name_label, &pos_label);
        if (close_panel)
        {
            lv_obj_clear_flag(close_panel, LV_OBJ_FLAG_HIDDEN);
            lv_coord_t w = lv_obj_get_width(close_panel);
            if (w <= 0)
                w = lv_obj_get_width(open_panel);
            if (w > 0)
                lv_obj_set_width(close_panel, w);
            if (open_panel)
                lv_obj_set_x(close_panel, lv_obj_get_x(open_panel));
        }
        if (open_panel)
            lv_obj_add_flag(open_panel, LV_OBJ_FLAG_HIDDEN);
    }
    tarot_hide_all_details();
}

static void tarot_card_open_anim_cb(lv_anim_t *a);

static void tarot_card_close_completed_cb(lv_anim_t *a)
{
    lv_obj_t *close_panel = (lv_obj_t *)a->var;
    lv_obj_t *open_panel = (lv_obj_t *)a->user_data;
    lv_obj_add_flag(close_panel, LV_OBJ_FLAG_HIDDEN);
    if (!open_panel)
        return;

    uint16_t x = lv_obj_get_x(open_panel);
    lv_coord_t w = lv_obj_get_width(open_panel);
    if (w <= 0)
        w = lv_obj_get_width(close_panel);
    if (w <= 0)
        return;

    lv_coord_t half_w = w / 2;
    /* 重入防护：打开动画启动前清除 open_panel 上的残留动画 */
    lv_anim_delete(open_panel, NULL);
    lv_obj_clear_flag(open_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_x(open_panel, x + half_w);
    lv_obj_set_width(open_panel, 0);

    lv_anim_t open_x;
    lv_anim_init(&open_x);
    lv_anim_set_var(&open_x, open_panel);
    lv_anim_set_exec_cb(&open_x, (lv_anim_exec_xcb_t)close_x_anim_cb);
    lv_anim_set_values(&open_x, x + half_w, x);
    lv_anim_set_duration(&open_x, 400);
    lv_anim_set_path_cb(&open_x, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&open_x, tarot_card_open_anim_cb);
    lv_anim_set_user_data(&open_x, (void *)(intptr_t)s_tarot.current_slot);
    lv_anim_start(&open_x);

    lv_anim_t open_w;
    lv_anim_init(&open_w);
    lv_anim_set_var(&open_w, open_panel);
    lv_anim_set_exec_cb(&open_w, (lv_anim_exec_xcb_t)cover_width_anim_cb);
    lv_anim_set_values(&open_w, 0, w);
    lv_anim_set_duration(&open_w, 400);
    lv_anim_set_path_cb(&open_w, lv_anim_path_ease_out);
    lv_anim_start(&open_w);
}

static void tarot_flip_one_card(int slot)
{
    lv_obj_t *close_panel, *open_panel, *name_label, *pos_label;
    tarot_get_card_widgets(slot, &close_panel, &open_panel, &name_label, &pos_label);
    if (!close_panel || !open_panel)
        return;

    uint16_t x = lv_obj_get_x(close_panel);
    lv_coord_t w = lv_obj_get_width(close_panel);
    if (w <= 0)
    {
        w = lv_obj_get_width(open_panel);
        x = x - (w / 2);
    }
    if (w <= 0)
        return;

    lv_coord_t half_w = w / 2;
    /* 重入防护：关闭动画启动前清除 close_panel 上的残留动画 */
    lv_anim_delete(close_panel, NULL);
    lv_obj_clear_flag(close_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(close_panel, w);
    lv_obj_add_flag(open_panel, LV_OBJ_FLAG_HIDDEN);

    lv_anim_t close_x;
    lv_anim_init(&close_x);
    lv_anim_set_var(&close_x, close_panel);
    lv_anim_set_exec_cb(&close_x, (lv_anim_exec_xcb_t)close_x_anim_cb);
    lv_anim_set_values(&close_x, x, x + half_w);
    lv_anim_set_duration(&close_x, 400);
    lv_anim_set_path_cb(&close_x, lv_anim_path_ease_in);
    lv_anim_start(&close_x);

    lv_anim_t close_w;
    lv_anim_init(&close_w);
    lv_anim_set_var(&close_w, close_panel);
    lv_anim_set_exec_cb(&close_w, (lv_anim_exec_xcb_t)cover_width_anim_cb);
    lv_anim_set_values(&close_w, w, 0);
    lv_anim_set_duration(&close_w, 400);
    lv_anim_set_path_cb(&close_w, lv_anim_path_ease_in);
    lv_anim_set_completed_cb(&close_w, tarot_card_close_completed_cb);
    lv_anim_set_user_data(&close_w, open_panel);
    lv_anim_start(&close_w);
    ESP_LOGI(TAG, "Tarot flip card %d", slot + 1);
}

static void tarot_card_open_anim_cb(lv_anim_t *a)
{
    int slot = (int)(intptr_t)a->user_data;
    if (slot != s_tarot.current_slot)
        return;
    if (s_tarot.current_slot < 2)
    {
        s_tarot.current_slot++;
        tarot_flip_one_card(s_tarot.current_slot);
    }
    else
    {
        s_app_state = FUN_APP_STATE_SHOWING;
        if (s_ui.fun_tip_label)
            lv_label_set_text(s_ui.fun_tip_label, _("三张牌已抽出，再次翻转抽新牌"));
    }
}

static void tarot_start_draw(void)
{
    s_app_state = FUN_APP_STATE_DRAWING;
    s_tarot.current_slot = 0;
    tarot_draw_all_cards();
    tarot_flip_one_card(0);
    if (s_ui.fun_tip_label)
        lv_label_set_text(s_ui.fun_tip_label, "");
}

static void tarot_on_flip(void)
{
    if (!s_tarot.enabled)
        return;
    if (s_app_state == FUN_APP_STATE_IDLE || s_app_state == FUN_APP_STATE_SHOWING)
    {
        int cost = 0;
        if (!fun_mana_try_consume(&cost))
        {
            if (s_ui.fun_tip_label)
                lv_label_set_text(s_ui.fun_tip_label, _("能量耗尽，明日恢复后再抽卡"));
            return;
        }
        app_manager_show_notificationf_insert_timeout(3000, _("本次消耗 %d 能量，剩余 %d%%"), cost, s_mana.mana);
        tarot_reset_ui();
        tarot_start_draw();
    }
}

/* ==================== Event Handlers ==================== */

static void fun_set_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "fun_set_cb");
    if (!lvgl_port_lock(pdMS_TO_TICKS(10))) {
        ESP_LOGW(TAG, "fun_set_cb: lock timeout");
        return;
    }
    lv_obj_clear_flag(s_ui.fun_set_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_ui.fun_set_panel, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lvgl_port_unlock();
}

static void fun_home_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "fun_home_cb");
    app_manager_request_kill_active();
}

static void mode_book_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "mode_book_cb");
    s_mode = FUN_MODE_BOOK;
    s_app_state = FUN_APP_STATE_IDLE;
    memset(&s_book, 0, sizeof(s_book));
    memset(&s_tarot, 0, sizeof(s_tarot));
    s_book.enabled = true;

    s_imu.flip_state = IMU_FLIP_STATE_IDLE;
    s_imu.covered_since_us = 0;
    s_imu.last_trigger_us = 0;

    /* on_init 调用路径正处在切屏 flush 期间，10ms 锁必超时；放宽到 200ms。
     * LVGL 事件上下文调用时锁已被 taskLVGL 递归持有，立即返回无副作用。 */
    if (!lvgl_port_lock(pdMS_TO_TICKS(200))) {
        ESP_LOGW(TAG, "mode_book_cb: lock timeout");
        return;
    }
    lv_obj_add_flag(s_ui.fun_panel_tarot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_ui.fun_panel_book, LV_OBJ_FLAG_HIDDEN);

    if (s_ui.panel_book_close)
    {
        lv_obj_clear_flag(s_ui.panel_book_close, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(s_ui.panel_book_close, 433);
        lv_obj_set_width(s_ui.panel_book_close, 350);
    }
    lv_obj_add_flag(s_ui.fun_set_panel, LV_OBJ_FLAG_HIDDEN);

    s_book.selected_index = -1;
    if (s_ui.book_answer_text)
        lv_label_set_text(s_ui.book_answer_text, "");
    if (s_ui.book_answer_detail)
        lv_label_set_text(s_ui.book_answer_detail, "");
    if (s_ui.fun_tip_label)
        lv_label_set_text(s_ui.fun_tip_label, _("请翻转设备，心中默念疑问后翻回，查看答案"));
    lvgl_port_unlock();

    fun_mana_show_bar();
}

static void mode_tarot_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "mode_tarot_cb");
    s_mode = FUN_MODE_TAROT;
    s_app_state = FUN_APP_STATE_IDLE;
    memset(&s_book, 0, sizeof(s_book));
    memset(&s_tarot, 0, sizeof(s_tarot));
    s_tarot.enabled = true;

    s_imu.flip_state = IMU_FLIP_STATE_IDLE;
    s_imu.covered_since_us = 0;
    s_imu.last_trigger_us = 0;

    /* 同 mode_book_cb：on_init 路径正处切屏 flush 期间，放宽到 200ms */
    if (!lvgl_port_lock(pdMS_TO_TICKS(200))) {
        return;
    }
    lv_obj_add_flag(s_ui.fun_panel_book, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_ui.fun_panel_tarot, LV_OBJ_FLAG_HIDDEN);
    tarot_reset_ui();
    lv_obj_add_flag(s_ui.fun_set_panel, LV_OBJ_FLAG_HIDDEN);

    if (s_ui.fun_tip_label)
        lv_label_set_text(s_ui.fun_tip_label, _("请翻转设备，心中默念疑问后翻回，抽取塔罗牌"));
    lvgl_port_unlock();

    fun_mana_show_bar();
}

static void tarot_toggle_detail(int slot)
{
    if (slot < 0 || slot > 2)
        return;
    if (s_app_state != FUN_APP_STATE_SHOWING)
        return;

    lv_obj_t *detail_panel = NULL, *detail_text = NULL;
    if (slot == 0)
    {
        detail_panel = s_ui.tarot_card_detail_panel_1;
        detail_text = s_ui.tarot_card_detail_text_1;
    }
    else if (slot == 1)
    {
        detail_panel = s_ui.tarot_card_detail_panel_2;
        detail_text = s_ui.tarot_card_detail_text_2;
    }
    else
    {
        detail_panel = s_ui.tarot_card_detail_panel_3;
        detail_text = s_ui.tarot_card_detail_text_3;
    }
    if (!detail_panel)
        return;

    if (lv_obj_has_flag(detail_panel, LV_OBJ_FLAG_HIDDEN))
    {
        const tarot_card_t *card = tarot_get_card_by_index(s_tarot.drawn_cards[slot]);
        if (card && detail_text)
        {
            const char *meaning = s_tarot.reversed[slot] ? card->reversed : card->upright;
            lv_label_set_text(detail_text, meaning);
        }
        lv_obj_clear_flag(detail_panel, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(detail_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

static void tarot_detail_click_cb(lv_event_t *e)
{
    int slot = (int)(intptr_t)lv_event_get_user_data(e);
    tarot_toggle_detail(slot);
}

/* ==================== Lifecycle ==================== */

static bool app_fun_on_init(app_base_t *self, void *screen_ctx)
{
    (void)self;
    (void)screen_ctx;
    ESP_LOGI(TAG, "init");

    memset(&s_book, 0, sizeof(s_book));
    memset(&s_tarot, 0, sizeof(s_tarot));
    memset(&s_imu, 0, sizeof(s_imu));
    s_mode = FUN_MODE_NONE;
    s_app_state = FUN_APP_STATE_IDLE;
    s_imu.flip_state = IMU_FLIP_STATE_IDLE;

    /* on_init 紧随切屏调用，必撞 task_gui 全屏 flush 持锁（单次 ~50ms），
     * 旧值 10ms 必超时 abort，App 带病激活成僵尸（真机死胡同）；放宽 500ms。
     * 与 mode_book_cb 同款理由。 */
    if (!lvgl_port_lock(pdMS_TO_TICKS(500))) {
        ESP_LOGW(TAG, "on_init: lvgl lock timeout, abort");
        return false;
    }
    ESP_LOGI(TAG, "on_init step1: registering callbacks (home=%p set=%p book=%p tarot=%p)",
             s_ui.fun_btn_home, s_ui.fun_btn_set, s_ui.fun_btn_book, s_ui.fun_btn_tarot);
    if (s_ui.fun_btn_home)
        lv_obj_add_event_cb(s_ui.fun_btn_home, fun_home_cb, LV_EVENT_CLICKED, NULL);
    if (s_ui.fun_btn_set)
        lv_obj_add_event_cb(s_ui.fun_btn_set, fun_set_cb, LV_EVENT_CLICKED, NULL);
    if (s_ui.fun_btn_book)
        lv_obj_add_event_cb(s_ui.fun_btn_book, mode_book_cb, LV_EVENT_CLICKED, NULL);
    if (s_ui.fun_btn_tarot)
        lv_obj_add_event_cb(s_ui.fun_btn_tarot, mode_tarot_cb, LV_EVENT_CLICKED, NULL);
    if (s_ui.panel_tarot_open_1)
        lv_obj_add_event_cb(s_ui.panel_tarot_open_1, tarot_detail_click_cb, LV_EVENT_CLICKED, (void *)0);
    if (s_ui.panel_tarot_open_2)
        lv_obj_add_event_cb(s_ui.panel_tarot_open_2, tarot_detail_click_cb, LV_EVENT_CLICKED, (void *)1);
    if (s_ui.panel_tarot_open_3)
        lv_obj_add_event_cb(s_ui.panel_tarot_open_3, tarot_detail_click_cb, LV_EVENT_CLICKED, (void *)2);
    if (s_ui.tarot_card_detail_panel_1)
        lv_obj_add_event_cb(s_ui.tarot_card_detail_panel_1, tarot_detail_click_cb, LV_EVENT_CLICKED, (void *)0);
    if (s_ui.tarot_card_detail_panel_2)
        lv_obj_add_event_cb(s_ui.tarot_card_detail_panel_2, tarot_detail_click_cb, LV_EVENT_CLICKED, (void *)1);
    if (s_ui.tarot_card_detail_panel_3)
        lv_obj_add_event_cb(s_ui.tarot_card_detail_panel_3, tarot_detail_click_cb, LV_EVENT_CLICKED, (void *)2);
    /* close 面板在 EEZ 层级中位于 detail panel 之上且默认可点，
     * 不挂回调会把点击吞掉（显示后点不没）；同挂 toggle，点到哪层都能显隐 */
    if (s_ui.panel_tarot_close_1)
        lv_obj_add_event_cb(s_ui.panel_tarot_close_1, tarot_detail_click_cb, LV_EVENT_CLICKED, (void *)0);
    if (s_ui.panel_tarot_close_2)
        lv_obj_add_event_cb(s_ui.panel_tarot_close_2, tarot_detail_click_cb, LV_EVENT_CLICKED, (void *)1);
    if (s_ui.panel_tarot_close_3)
        lv_obj_add_event_cb(s_ui.panel_tarot_close_3, tarot_detail_click_cb, LV_EVENT_CLICKED, (void *)2);
    lvgl_port_unlock();
    ESP_LOGI(TAG, "on_init step2: imu_init");

    esp_err_t imu_ret = imu_init();
    ESP_LOGI(TAG, "on_init step3: imu_init ret=%d, mana load", (int)imu_ret);

    fun_mana_load_and_refresh();
    ESP_LOGI(TAG, "on_init step4: enter book mode");

    mode_book_cb(NULL);
#if !CONFIG_BOARD_HAS_IMU
    /* 无 IMU：复用提示 label 告知翻转交互不可用（App 壳仍可进入浏览） */
    ESP_LOGW(TAG, "board has no IMU, flip interaction disabled");
    if (lvgl_port_lock(pdMS_TO_TICKS(200))) {
        if (s_ui.fun_tip_label)
            lv_label_set_text(s_ui.fun_tip_label, _("本设备不支持翻转交互"));
        lvgl_port_unlock();
    }
#endif
    ESP_LOGI(TAG, "on_init done: mode=%d", (int)s_mode);
    return true;
}

/* LVGL 锁持续不可得时 dump 任务状态：区分 taskLVGL 是 Blocked（持锁等资源）
 * 还是 Running（失控空转） */
static void fun_diag_dump_tasks(void)
{
    static const char *const names[] = {
        "taskLVGL", "task_gui", "task_app", "task_input", "task_comm", "task_audio",
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        TaskHandle_t h = xTaskGetHandle(names[i]);
        if (h == NULL) {
            continue;
        }
#if defined(INCLUDE_eTaskGetState) && (INCLUDE_eTaskGetState == 1)
        static const char *const state_names[] = {
            "Running", "Ready", "Blocked", "Suspended", "Deleted", "Invalid",
        };
        eTaskState st = eTaskGetState(h);
        unsigned si = ((unsigned)st <= (unsigned)eInvalid) ? (unsigned)st : (unsigned)eInvalid;
        ESP_LOGW(TAG, "[diag] %s state=%s hwm=%u", names[i], state_names[si],
                 (unsigned)uxTaskGetStackHighWaterMark(h));
#else
        ESP_LOGW(TAG, "[diag] %s hwm=%u", names[i],
                 (unsigned)uxTaskGetStackHighWaterMark(h));
#endif
    }
}

static void app_fun_on_update(app_base_t *self)
{
    (void)self;

    /* 心跳：每 5s 证明 on_update 存活（此前日志全哑无法区分
     * “未被调用”与“调用了但无事件”）；心跳断流即 task_app 被卡 */
    static uint32_t s_upd_count = 0;
    if (++s_upd_count % 500 == 1) {
        ESP_LOGI(TAG, "on_update alive: count=%u state=%d mode=%d imu=%d flip_st=%d",
                 (unsigned)s_upd_count, (int)s_app_state, (int)s_mode,
                 (int)s_imu.initialized, (int)s_imu.flip_state);
    }

    if (s_app_state == FUN_APP_STATE_DRAWING)
        return;

    /* 翻转检测（I2C+状态机）在 LVGL 锁外执行，仅状态迁移对应的 UI 动作
     * 进锁，最大限度缩短持锁时间；锁失败丢弃本次动作并 dump 任务状态。 */
    bool entered_covered = false;
    bool flip = imu_detected_flip(&entered_covered);

    if (flip || entered_covered) {
        if (lvgl_port_lock(pdMS_TO_TICKS(10))) {
            if (entered_covered) {
                if (s_mode == FUN_MODE_BOOK)
                    book_reset_cover();
                else if (s_mode == FUN_MODE_TAROT)
                    tarot_reset_ui();
            }
            if (flip) {
                ESP_LOGI(TAG, "flip detected, mode=%d", (int)s_mode);
                if (s_mode == FUN_MODE_BOOK)
                    book_on_flip();
                else if (s_mode == FUN_MODE_TAROT)
                    tarot_on_flip();
            }
            lvgl_port_unlock();
        } else {
            ESP_LOGW(TAG, "on_update: lvgl lock timeout, action dropped");
            fun_diag_dump_tasks();
        }
    }

    if (s_mana.need_save) {
        fun_mana_persist();
        s_mana.need_save = false;
    }
}

static void app_fun_on_pause(app_base_t *self)
{
    (void)self;
    imu_deinit();

    if (s_mana.need_save) {
        fun_mana_persist();
        s_mana.need_save = false;
    }
}

static void app_fun_on_resume(app_base_t *self)
{
    (void)self;
    imu_init();
    s_imu.flip_state = IMU_FLIP_STATE_IDLE;
    s_imu.covered_since_us = 0;
    fun_mana_load_and_refresh();
}

static void app_fun_on_destroy(app_base_t *self)
{
    (void)self;
    /* 同 on_init：销毁同样可能撞切屏 flush，10ms 超时会让事件回调泄漏到
     * 全局复用的 EEZ 控件上，下次进入重复注册一次点击多次触发 */
    if (!lvgl_port_lock(pdMS_TO_TICKS(500))) {
        imu_deinit();
        if (s_mana.need_save) {
            fun_mana_persist();
            s_mana.need_save = false;
        }
        return;
    }
    if (s_ui.fun_btn_home)
        lv_obj_remove_event_cb(s_ui.fun_btn_home, fun_home_cb);
    if (s_ui.fun_btn_set)
        lv_obj_remove_event_cb(s_ui.fun_btn_set, fun_set_cb);
    if (s_ui.fun_btn_book)
        lv_obj_remove_event_cb(s_ui.fun_btn_book, mode_book_cb);
    if (s_ui.fun_btn_tarot)
        lv_obj_remove_event_cb(s_ui.fun_btn_tarot, mode_tarot_cb);
    if (s_ui.panel_tarot_open_1)
        lv_obj_remove_event_cb(s_ui.panel_tarot_open_1, tarot_detail_click_cb);
    if (s_ui.panel_tarot_open_2)
        lv_obj_remove_event_cb(s_ui.panel_tarot_open_2, tarot_detail_click_cb);
    if (s_ui.panel_tarot_open_3)
        lv_obj_remove_event_cb(s_ui.panel_tarot_open_3, tarot_detail_click_cb);
    tarot_hide_all_details();
    lvgl_port_unlock();
    imu_deinit();

    if (s_mana.need_save) {
        fun_mana_persist();
        s_mana.need_save = false;
    }
}

esp_err_t app_fun_register(void)
{
    static app_base_t app = {
        .name = "Fun",
        .screen_name = "app_fun",
        .screen_ctx = &s_ui,
        .screen_ctx_size = sizeof(s_ui),
        .widget_bindings = s_fun_bindings,
        .on_init = app_fun_on_init,
        .on_update = app_fun_on_update,
        .on_pause = app_fun_on_pause,
        .on_resume = app_fun_on_resume,
        .on_destroy = app_fun_on_destroy,
    };
    return app_manager_register(&app);
}