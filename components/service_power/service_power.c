/**
 * @file service_power.c
 * @brief 电源管理服务实现
 *
 * 移植自 M5Stack Tab5 UserDemo 的 HAL 电源相关能力：
 * - INA226 电池电压/电流/功率监测
 * - IP2326 充电使能 / 快充使能（IO 扩展器 2）
 * - USB-A 5V / 外部 5V / Wi-Fi / 外接天线 电源控制
 * - PMS150G 软件关机脉冲（IO 扩展器 2 P4）
 * - USB-C / 耳机 插入检测
 * - 触摸待机唤醒、RTC 闹钟待机唤醒
 *
 * Why: 上述前 6 项全部依赖 Tab5 的 IO 扩展器/INA226/PMS150G 电源硬件，
 * 由 CONFIG_BOARD_HAS_POWER_MGMT 编译期门控；无此硬件的板型对外 API 保留
 * 并无害降级（电池信息零值、检测返回 false、setter 空操作、关机退化为重启）。
 * 熄屏/唤醒/idle 状态机为板无关逻辑，两板均编译。
 */

#include "service_power.h"
#include "sdkconfig.h"
#include "board_hal.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_lcd_touch.h"
#include "esp_sleep.h"
#include "esp_system.h"
#if CONFIG_BOARD_HAS_POWER_MGMT
#include "ina226.h"
#include "bsp/m5stack_tab5.h"
#include "driver/gpio.h"
#include "esp_io_expander.h"
#include "esp_rom_sys.h"
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "service_rtc.h"
#include "service_i2c.h"
#include "service_nvs.h"

static const char *TAG = "service_power";

/* ---------- 硬件引脚定义（来自 M5Tab5 UserDemo） ---------- */
#if CONFIG_BOARD_HAS_POWER_MGMT
#define IOEXP2_ADDR 0x44

#define IOEXP2_PIN_WIFI_PWR     IO_EXPANDER_PIN_NUM_0
#define IOEXP2_PIN_USB_5V_EN    IO_EXPANDER_PIN_NUM_3
#define IOEXP2_PIN_POWEROFF     IO_EXPANDER_PIN_NUM_4
#define IOEXP2_PIN_CHARGE_QC    IO_EXPANDER_PIN_NUM_5
#define IOEXP2_PIN_CHARGE_EN    IO_EXPANDER_PIN_NUM_7

#define IOEXP1_PIN_EXT_ANT      IO_EXPANDER_PIN_NUM_0
#define IOEXP1_PIN_EXT_5V_EN    IO_EXPANDER_PIN_NUM_2
#define IOEXP1_PIN_HEADPHONE    IO_EXPANDER_PIN_NUM_7

#define INA226_DEFAULT_ADDR     CONFIG_SERVICE_POWER_INA226_I2C_ADDR
#define INA226_SHUNT_OHM        (CONFIG_SERVICE_POWER_INA226_SHUNT_MILLIOHM / 1000.0f)
#define INA226_MAX_CURRENT_A    (CONFIG_SERVICE_POWER_INA226_MAX_CURRENT_MA / 1000.0f)

/* IO 扩展器输入读取保护参数 */
#define IOEXP_READ_RETRY_COUNT    5
#define IOEXP_READ_RETRY_DELAY_MS 5
#define IOEXP_INPUT_ALL_MASK      0xFF

/* ---------- 内部状态 ---------- */
static esp_io_expander_handle_t s_ioexp1 = NULL;
static esp_io_expander_handle_t s_ioexp2 = NULL;
#endif /* CONFIG_BOARD_HAS_POWER_MGMT */

static service_power_event_cb_t s_event_cb = NULL;
static void *s_event_user_data = NULL;

/* 无电源硬件时保持零值/默认态：get 类 API 直接返回，不产生硬件访问 */
static service_power_battery_info_t s_bat_info = {0};
static bool s_charge_enable    = false;
static bool s_charge_qc_enable = false;
static bool s_usb_5v_enable    = true;
static bool s_ext_5v_enable    = true;
static bool s_wifi_pwr_enable  = true;
static bool s_ext_ant_enable   = false;

static bool s_initialized = false;

/* ---------- 自动熄屏/自动休眠状态机 ---------- */
#define SERVICE_POWER_IDLE_TIMEOUT_COUNT 6
static const uint32_t s_idle_timeout_ms[SERVICE_POWER_IDLE_TIMEOUT_COUNT] = {
    15000,  /* 15 秒 */
    30000,  /* 30 秒 */
    60000,  /* 1  分钟 */
    180000, /* 3  分钟 */
    300000, /* 5  分钟 */
    600000, /* 10 分钟 */
};
#define SERVICE_POWER_AUTO_SLEEP_DELAY_MS (5 * 60 * 1000)  /* 熄屏后 5 分钟 */

static bool s_idle_enabled = false;
static bool s_screen_off = false;
static uint8_t s_idle_timeout_index = 2;
static bool s_auto_sleep_enabled = false;
static uint32_t s_last_activity_ms = 0;
static uint32_t s_screen_off_since_ms = 0;
static uint8_t s_screen_off_saved_brightness = 50;
static bool s_hold_screen_on = false;

static void service_power_idle_screen_off(void);
static void service_power_idle_wake(void);

#if CONFIG_BOARD_HAS_POWER_MGMT
/* 缓存 IO 扩展器输入寄存器，避免高频直接读取导致 I2C 总线竞争 */
static uint8_t s_ioexp1_input_cache = 0;
static uint8_t s_ioexp2_input_cache = 0;
static bool s_ioexp1_input_valid = false;
static bool s_ioexp2_input_valid = false;
static uint32_t s_ioexp_read_err_cnt = 0;

/* 输入状态轮询周期：耳机/USB-C 检测需要实时性，100ms 足够且不会明显占用 I2C */
#define IOEXP_INPUT_POLL_PERIOD_MS 100

/* 电池信息轮询周期：UserDemo 电源面板每 100ms 刷新；状态栏需要插拔电池/充电器时即时变化，
 * 500ms 在实时性和 I2C 负载之间取得平衡 */
#define BATTERY_POLL_PERIOD_MS     500

/* 2S 7.2V 锂电池低电量阈值：单节 3.3V 对应串联 6.6V */
#define BAT_LOW_VOLTAGE_V          6.6f

/* 输入软件消抖：连续 N 次采样稳定才确认状态变化，避免接触噪声/电平毛刺 */
#define IOEXP_DEBOUNCE_COUNT       5

/* 上一次的输入状态，用于检测边沿变化 */
static bool s_last_headphone_state = false;

/* 消抖中间态 */
static bool s_headphone_debounce_state = false;
static uint8_t s_headphone_debounce_cnt = 0;
#endif /* CONFIG_BOARD_HAS_POWER_MGMT */

/* -------------------- ESP32-C6 提前上电（constructor） -------------------- */
#if CONFIG_BOARD_HAS_POWER_MGMT
/* ESP32-C6 (Wi-Fi) 由 IO 扩展器 2 (0x44) P0 控制供电。
 * esp_hosted 使用默认 priority 的 constructor 自动初始化 SDIO，
 * 因此必须在 constructor 阶段提前打开 C6 电源。
 * 此阶段 FreeRTOS 调度器尚未启动，不能使用 BSP I2C 驱动，
 * 所以用 GPIO bit-bang I2C 直接配置 IO 扩展器。 */
#define C6_IOEXP_ADDR       0x44
#define C6_IOEXP_SDA_GPIO   GPIO_NUM_31
#define C6_IOEXP_SCL_GPIO   GPIO_NUM_32
#define C6_IOEXP_REG_IO_DIR 0x03
#define C6_IOEXP_REG_OUT    0x05
#define C6_IOEXP_REG_OUT_HZ 0x07

static void c6_i2c_delay(void)
{
    esp_rom_delay_us(5); /* ~100 kHz */
}

static void c6_i2c_start(void)
{
    gpio_set_level(C6_IOEXP_SDA_GPIO, 1);
    c6_i2c_delay();
    gpio_set_level(C6_IOEXP_SCL_GPIO, 1);
    c6_i2c_delay();
    gpio_set_level(C6_IOEXP_SDA_GPIO, 0);
    c6_i2c_delay();
    gpio_set_level(C6_IOEXP_SCL_GPIO, 0);
    c6_i2c_delay();
}

static void c6_i2c_stop(void)
{
    gpio_set_level(C6_IOEXP_SDA_GPIO, 0);
    c6_i2c_delay();
    gpio_set_level(C6_IOEXP_SCL_GPIO, 1);
    c6_i2c_delay();
    gpio_set_level(C6_IOEXP_SDA_GPIO, 1);
    c6_i2c_delay();
}

static int c6_i2c_write_byte(uint8_t data)
{
    for (int i = 7; i >= 0; i--) {
        gpio_set_level(C6_IOEXP_SDA_GPIO, (data >> i) & 1);
        c6_i2c_delay();
        gpio_set_level(C6_IOEXP_SCL_GPIO, 1);
        c6_i2c_delay();
        gpio_set_level(C6_IOEXP_SCL_GPIO, 0);
        c6_i2c_delay();
    }
    gpio_set_level(C6_IOEXP_SDA_GPIO, 1); /* 释放 ACK */
    c6_i2c_delay();
    gpio_set_level(C6_IOEXP_SCL_GPIO, 1);
    c6_i2c_delay();
    int ack = (gpio_get_level(C6_IOEXP_SDA_GPIO) == 0);
    gpio_set_level(C6_IOEXP_SCL_GPIO, 0);
    c6_i2c_delay();
    return ack;
}

static bool c6_ioexp_write_reg(uint8_t reg, uint8_t value)
{
    c6_i2c_start();
    if (!c6_i2c_write_byte(C6_IOEXP_ADDR << 1)) {
        c6_i2c_stop();
        return false;
    }
    if (!c6_i2c_write_byte(reg)) {
        c6_i2c_stop();
        return false;
    }
    if (!c6_i2c_write_byte(value)) {
        c6_i2c_stop();
        return false;
    }
    c6_i2c_stop();
    return true;
}

static void __attribute__((constructor(101))) service_power_c6_early_enable(void)
{
    gpio_set_direction(C6_IOEXP_SDA_GPIO, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_pull_mode(C6_IOEXP_SDA_GPIO, GPIO_PULLUP_ONLY);
    gpio_set_level(C6_IOEXP_SDA_GPIO, 1);

    gpio_set_direction(C6_IOEXP_SCL_GPIO, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_pull_mode(C6_IOEXP_SCL_GPIO, GPIO_PULLUP_ONLY);
    gpio_set_level(C6_IOEXP_SCL_GPIO, 1);

    /* Why: C6 独立供电独立运行，任何 P4 复位（看门狗/软复位/烧录/深睡唤醒）
     * 都不会重启它；一旦楔死在 hosted 半开会话（2026-08 开机卡 70% 事故），
     * 链路永久不可用。每次启动无条件断电 200ms 再上电，强制 C6 与 esp_hosted
     * 从零同步；代价仅 200ms，换来所有复位路径行为一致（不猜复位源）。
     * 上电复位时轨本来就没电，此操作等效于延长放电稳定时间，无害。 */
    c6_ioexp_write_reg(C6_IOEXP_REG_IO_DIR, 0xFE);   /* P0 = output */
    c6_ioexp_write_reg(C6_IOEXP_REG_OUT_HZ, 0xFE);   /* P0 = push-pull */
    c6_ioexp_write_reg(C6_IOEXP_REG_OUT, 0x00);      /* P0 = 0 断电 */
    esp_rom_delay_us(200 * 1000);


    /* 先写输出寄存器，避免方向切换为输出时产生低电平毛刺 */
    bool ok = c6_ioexp_write_reg(C6_IOEXP_REG_OUT, 0x01);   /* P0 = 1 */
    ok &= c6_ioexp_write_reg(C6_IOEXP_REG_IO_DIR, 0xFE);    /* P0 = output */
    ok &= c6_ioexp_write_reg(C6_IOEXP_REG_OUT_HZ, 0xFE);    /* P0 = push-pull */

    ESP_EARLY_LOGI(TAG, "C6 Wi-Fi power early enable (with reset): %s", ok ? "ok" : "failed");
}
#endif /* CONFIG_BOARD_HAS_POWER_MGMT */

/* ---------- 私有函数声明 ---------- */
static void emit_event(service_power_event_t evt);
#if CONFIG_BOARD_HAS_POWER_MGMT
static esp_err_t ioexpander_init(void);
static void ioexpander_apply_demo_config(void);
static esp_err_t ioexp2_set_output(int pin, bool level);
static esp_err_t ioexp1_set_output(int pin, bool level);
static bool ioexp1_get_input(int pin);
static void poweroff_pulse(void);
static void shutdown_peripherals(void);
static bool ioexp_take(void);
static void ioexp_give(void);
static esp_err_t ioexp_read_input(esp_io_expander_handle_t handle, uint32_t *value);
static void ioexp_refresh_inputs(void);
static void ioexp_process_input_changes(void);
static bool ioexp_debounce_input(bool raw, bool *last_stable,
                                  bool *debounce_state, uint8_t *debounce_cnt);
#endif /* CONFIG_BOARD_HAS_POWER_MGMT */
static bool wait_touch_release(void);
static bool wait_touch_press(void);
static void service_power_idle_screen_off(void);
static void service_power_idle_wake(void);

/* ---------- 事件 ---------- */
static void emit_event(service_power_event_t evt)
{
    if (s_event_cb != NULL) {
        s_event_cb(evt, s_event_user_data);
    }
}

#if CONFIG_BOARD_HAS_POWER_MGMT
/* ---------- IO 扩展器访问保护 ---------- */
static bool ioexp_take(void)
{
    return service_i2c_take();
}

static void ioexp_give(void)
{
    service_i2c_give();
}

static esp_err_t ioexp_read_input(esp_io_expander_handle_t handle, uint32_t *value)
{
    esp_err_t ret = ESP_FAIL;

    for (int i = 0; i < IOEXP_READ_RETRY_COUNT; i++) {
        ret = esp_io_expander_get_level(handle, IOEXP_INPUT_ALL_MASK, value);
        if (ret == ESP_OK) {
            return ESP_OK;
        }
        if (i < IOEXP_READ_RETRY_COUNT - 1) {
            vTaskDelay(pdMS_TO_TICKS(IOEXP_READ_RETRY_DELAY_MS));
        }
    }

    return ret;
}

static void ioexp_refresh_inputs(void)
{
    bool any_ok = false;

    if (s_ioexp1 != NULL) {
        uint32_t v = 0;
        if (ioexp_read_input(s_ioexp1, &v) == ESP_OK) {
            s_ioexp1_input_cache = (uint8_t)v;
            s_ioexp1_input_valid = true;
            any_ok = true;
        }
    }

    if (s_ioexp2 != NULL) {
        uint32_t v = 0;
        if (ioexp_read_input(s_ioexp2, &v) == ESP_OK) {
            s_ioexp2_input_cache = (uint8_t)v;
            s_ioexp2_input_valid = true;
            any_ok = true;
        }
    }

    if (any_ok) {
        if (s_ioexp_read_err_cnt > 0) {
            ESP_LOGI(TAG, "IO expander input read recovered");
            s_ioexp_read_err_cnt = 0;
        }
    } else {
        s_ioexp_read_err_cnt++;
        if (s_ioexp_read_err_cnt >= 3 &&
            (s_ioexp_read_err_cnt == 3 || (s_ioexp_read_err_cnt % 20) == 0)) {
            ESP_LOGW(TAG, "IO expander input read failed (%lu consecutive)",
                     (unsigned long)s_ioexp_read_err_cnt);
        }
    }
}

static void ioexp_process_input_changes(void)
{
    if (!s_ioexp1_input_valid && !s_ioexp2_input_valid) {
        return;
    }

    /* 耳机检测：IO 扩展器 1 P7 */
    bool headphone = false;
    if (s_ioexp1_input_valid) {
        headphone = (s_ioexp1_input_cache & (uint32_t)IOEXP1_PIN_HEADPHONE) != 0;
    }
    if (s_ioexp1_input_valid &&
        ioexp_debounce_input(headphone, &s_last_headphone_state,
                             &s_headphone_debounce_state,
                             &s_headphone_debounce_cnt)) {
        ESP_LOGI(TAG, "headphone %s", s_last_headphone_state ? "connected" : "disconnected");
        emit_event(s_last_headphone_state ? SERVICE_POWER_EVT_HEADPHONE_INSERT
                                          : SERVICE_POWER_EVT_HEADPHONE_REMOVE);
    }

}

/**
 * @brief 输入软件消抖
 *
 * 当原始状态与当前消抖状态不一致时重置计数器；连续 IOEXP_DEBOUNCE_COUNT
 * 次采样稳定后，才更新最终状态并返回 true 表示发生有效边沿。
 */
static bool ioexp_debounce_input(bool raw, bool *last_stable,
                                  bool *debounce_state, uint8_t *debounce_cnt)
{
    if (raw != *debounce_state) {
        *debounce_state = raw;
        *debounce_cnt = 1;
        return false;
    }

    if (*debounce_cnt < IOEXP_DEBOUNCE_COUNT) {
        (*debounce_cnt)++;
        if (*debounce_cnt == IOEXP_DEBOUNCE_COUNT && raw != *last_stable) {
            *last_stable = raw;
            return true;
        }
    }

    return false;
}

/* ---------- IO 扩展器 ---------- */
static esp_err_t ioexpander_init(void)
{
    s_ioexp1 = bsp_io_expander_init();
    if (s_ioexp1 == NULL) {
        ESP_LOGW(TAG, "IO expander 1 (0x43) init failed");
    }

    s_ioexp2 = bsp_io_expander1_init();
    if (s_ioexp2 == NULL) {
        ESP_LOGW(TAG, "IO expander 2 (0x44) init failed");
    }

    ioexp_take();

    /* 配置两个 IO 扩展器的方向、输出模式、上下拉和默认电平，确保扬声器、Wi-Fi、耳机检测等功能。 */
    ioexpander_apply_demo_config();

    ioexp_give();

    return ESP_OK;
}

static void ioexpander_apply_demo_config(void)
{
    if (s_ioexp1 != NULL) {
        /* IO 扩展器 1 (0x43) */
        /* 方向：P0-P6 输出，P7 输入 (HP_DET) */
        uint32_t ioexp1_outputs = IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1 |
                                  IO_EXPANDER_PIN_NUM_2 | IO_EXPANDER_PIN_NUM_3 |
                                  IO_EXPANDER_PIN_NUM_4 | IO_EXPANDER_PIN_NUM_5 |
                                  IO_EXPANDER_PIN_NUM_6;
        esp_err_t ret = esp_io_expander_set_dir(s_ioexp1, ioexp1_outputs, IO_EXPANDER_OUTPUT);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "ioexp1 set outputs dir failed: %s", esp_err_to_name(ret));
        }
        ret = esp_io_expander_set_dir(s_ioexp1, IO_EXPANDER_PIN_NUM_7, IO_EXPANDER_INPUT);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "ioexp1 set p7 input failed: %s", esp_err_to_name(ret));
        }

        /* 输出模式：P0-P6 推挽，P1/P2 先保持高电平避免毛刺 */
        ret = esp_io_expander_set_level(s_ioexp1,
                                        IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2 |
                                        IO_EXPANDER_PIN_NUM_4 | IO_EXPANDER_PIN_NUM_5 |
                                        IO_EXPANDER_PIN_NUM_6,
                                        1);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "ioexp1 set pre-output high failed: %s", esp_err_to_name(ret));
        }
        ret = esp_io_expander_set_level(s_ioexp1,
                                        IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_3,
                                        0);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "ioexp1 set pre-output low failed: %s", esp_err_to_name(ret));
        }
        ret = esp_io_expander_set_output_mode(s_ioexp1, ioexp1_outputs, IO_EXPANDER_OUTPUT_MODE_PUSH_PULL);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "ioexp1 set push-pull failed: %s", esp_err_to_name(ret));
        }

        /* 上下拉：P0-P6 上拉，P7 浮空 */
        ret = esp_io_expander_set_pullupdown(s_ioexp1, ioexp1_outputs, IO_EXPANDER_PULL_UP);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "ioexp1 set pull-up failed: %s", esp_err_to_name(ret));
        }
        ret = esp_io_expander_set_pullupdown(s_ioexp1, IO_EXPANDER_PIN_NUM_7, IO_EXPANDER_PULL_NONE);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "ioexp1 set p7 pull-none failed: %s", esp_err_to_name(ret));
        }
    }

    if (s_ioexp2 != NULL) {
        /* IO 扩展器 2 (0x44) */
        /* 方向：P0,P3,P4,P5,P7 输出；P1,P2,P6 输入 */
        uint32_t ioexp2_outputs = IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_3 |
                                  IO_EXPANDER_PIN_NUM_4 | IO_EXPANDER_PIN_NUM_5 |
                                  IO_EXPANDER_PIN_NUM_7;
        uint32_t ioexp2_inputs  = IO_EXPANDER_PIN_NUM_1 | IO_EXPANDER_PIN_NUM_2 |
                                  IO_EXPANDER_PIN_NUM_6;
        esp_err_t ret = esp_io_expander_set_dir(s_ioexp2, ioexp2_outputs, IO_EXPANDER_OUTPUT);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "ioexp2 set outputs dir failed: %s", esp_err_to_name(ret));
        }
        ret = esp_io_expander_set_dir(s_ioexp2, ioexp2_inputs, IO_EXPANDER_INPUT);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "ioexp2 set inputs dir failed: %s", esp_err_to_name(ret));
        }

        /* 输出模式：P0,P3,P4,P5,P7 推挽；P1,P2 高阻（输入脚） */
        ret = esp_io_expander_set_output_mode(s_ioexp2, ioexp2_outputs, IO_EXPANDER_OUTPUT_MODE_PUSH_PULL);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "ioexp2 set push-pull failed: %s", esp_err_to_name(ret));
        }
        ret = esp_io_expander_set_output_mode(s_ioexp2, ioexp2_inputs, IO_EXPANDER_OUTPUT_MODE_OPEN_DRAIN);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "ioexp2 set inputs high-z failed: %s", esp_err_to_name(ret));
        }

        /* 上下拉：P0,P3,P4,P5,P7 上拉；P2 下拉；P6 上拉（USB-C 插入检测）；P1 浮空 */
        ret = esp_io_expander_set_pullupdown(s_ioexp2, ioexp2_outputs, IO_EXPANDER_PULL_UP);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "ioexp2 set outputs pull-up failed: %s", esp_err_to_name(ret));
        }
        ret = esp_io_expander_set_pullupdown(s_ioexp2, IO_EXPANDER_PIN_NUM_2, IO_EXPANDER_PULL_DOWN);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "ioexp2 set p2 pull-down failed: %s", esp_err_to_name(ret));
        }
        ret = esp_io_expander_set_pullupdown(s_ioexp2, IO_EXPANDER_PIN_NUM_6, IO_EXPANDER_PULL_UP);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "ioexp2 set p6 pull-up failed: %s", esp_err_to_name(ret));
        }
        ret = esp_io_expander_set_pullupdown(s_ioexp2, IO_EXPANDER_PIN_NUM_1, IO_EXPANDER_PULL_NONE);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "ioexp2 set p1 pull-none failed: %s", esp_err_to_name(ret));
        }

        /* 输出电平：P0(WLAN_PWR_EN), P3(USB5V_EN) 高；P4/P5/P7 低 */
        ret = esp_io_expander_set_level(s_ioexp2,
                                        IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_3,
                                        1);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "ioexp2 set p0/p3 high failed: %s", esp_err_to_name(ret));
        }
        ret = esp_io_expander_set_level(s_ioexp2,
                                        IO_EXPANDER_PIN_NUM_4 | IO_EXPANDER_PIN_NUM_5 |
                                        IO_EXPANDER_PIN_NUM_7,
                                        0);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "ioexp2 set p4/p5/p7 low failed: %s", esp_err_to_name(ret));
        }
    }
}

static esp_err_t ioexp2_set_output(int pin, bool level)
{
    if (s_ioexp2 == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    ioexp_take();
    esp_err_t ret = esp_io_expander_set_level(s_ioexp2, (uint32_t)pin, level ? 1 : 0);
    ioexp_give();
    return ret;
}

static esp_err_t ioexp1_set_output(int pin, bool level)
{
    if (s_ioexp1 == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    ioexp_take();
    esp_err_t ret = esp_io_expander_set_level(s_ioexp1, (uint32_t)pin, level ? 1 : 0);
    ioexp_give();
    return ret;
}

static bool ioexp1_get_input(int pin)
{
    if (s_ioexp1 == NULL) {
        return false;
    }
    /* 优先返回缓存值，避免在音频任务等实时路径中直接访问 I2C。 */
    if (s_ioexp1_input_valid) {
        return (s_ioexp1_input_cache & (uint32_t)pin) != 0;
    }

    /* 缓存无效时回退到直接读取（仅初始化阶段可能走到这里）。 */
    ioexp_take();
    ioexp_refresh_inputs();
    bool ret = s_ioexp1_input_valid && ((s_ioexp1_input_cache & (uint32_t)pin) != 0);
    ioexp_give();
    return ret;
}

/* ---------- 电池 ---------- */
static void battery_process(void)
{
    service_power_update_battery_info();
}
#endif /* CONFIG_BOARD_HAS_POWER_MGMT */

esp_err_t service_power_update_battery_info(void)
{
#if CONFIG_BOARD_HAS_POWER_MGMT
    service_power_battery_info_t info = {0};
    bool read_ok = false;

    if (service_i2c_take()) {
        info.bus_voltage_v   = ina226_read_bus_voltage();
        info.shunt_voltage_v = ina226_read_shunt_voltage();
        info.current_a       = ina226_read_current();
        info.power_w         = ina226_read_power();
        service_i2c_give();

        /* INA226 读失败时返回 0.0f；四个值同时为 0 大概率是通信失败 */
        read_ok = !(info.bus_voltage_v == 0.0f && info.shunt_voltage_v == 0.0f &&
                    info.current_a == 0.0f && info.power_w == 0.0f);
    }

    if (!read_ok) {
        ESP_LOGW(TAG, "INA226 read failed, keep previous battery info");
        return ESP_FAIL;
    }

    /* 充电状态简化：电流 > 100mA 判定为充电，否则为非充电 */
    info.is_charging = (info.current_a > 0.1f);

    bool was_charging = s_bat_info.is_charging;
    s_bat_info = info;

    if (info.is_charging && !was_charging) {
        emit_event(SERVICE_POWER_EVT_CHARGING_START);
    } else if (!info.is_charging && was_charging) {
        emit_event(SERVICE_POWER_EVT_CHARGING_STOP);
    }

    if (info.bus_voltage_v > 1.0f && info.bus_voltage_v < BAT_LOW_VOLTAGE_V) {
        emit_event(SERVICE_POWER_EVT_BATTERY_LOW);
    }

    static uint32_t s_bat_log_skip = 0;
    if (++s_bat_log_skip >= 10) {
        s_bat_log_skip = 0;
        ESP_LOGD(TAG, "bat: bus=%.3fV current=%.3fA power=%.3fW charging=%d",
                 info.bus_voltage_v, info.current_a, info.power_w, info.is_charging);
    }

    return ESP_OK;
#else
    /* 无 INA226 电量计硬件：电池信息保持零值，调用方按无电池处理 */
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t service_power_get_battery_info(service_power_battery_info_t *info)
{
    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *info = s_bat_info;
    return ESP_OK;
}

/* ---------- 轮询处理 ---------- */
void service_power_process(void)
{
    if (!s_initialized) {
        return;
    }

#if CONFIG_BOARD_HAS_POWER_MGMT
    static uint32_t s_last_bat_ms = 0;
    static uint32_t s_last_input_ms = 0;

    uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if ((now_ms - s_last_bat_ms) >= BATTERY_POLL_PERIOD_MS) {
        s_last_bat_ms = now_ms;
        battery_process();
    }

    /* IO 扩展器输入轮询：耳机/USB-C 需要实时性，100ms 足够且对 I2C 总线压力小。
     * 若硬件上 PI4IOE5V6408 的 INT 引脚连接到 ESP32-P4 GPIO，可进一步改为中断驱动。 */
    if ((now_ms - s_last_input_ms) >= IOEXP_INPUT_POLL_PERIOD_MS) {
        s_last_input_ms = now_ms;
        if (ioexp_take()) {
            ioexp_refresh_inputs();
            ioexp_process_input_changes();
            ioexp_give();
        }
    }
#endif

    service_power_idle_process();
}

/* ---------- 公共 API：充电控制 ----------
 * Why: 无充电/电源硬件的板型 setter 仅记录状态并返回 ESP_OK（空操作），
 * 设置页在开机回读硬件状态时不会刷屏报错。 */
esp_err_t service_power_set_charge_enable(bool enable)
{
    ESP_LOGI(TAG, "charge enable: %d", enable);
    s_charge_enable = enable;
#if CONFIG_BOARD_HAS_POWER_MGMT
    return ioexp2_set_output(IOEXP2_PIN_CHARGE_EN, enable);
#else
    return ESP_OK;
#endif
}

bool service_power_get_charge_enable(void)
{
    return s_charge_enable;
}

esp_err_t service_power_set_charge_qc_enable(bool enable)
{
    ESP_LOGI(TAG, "charge qc enable: %d", enable);
    s_charge_qc_enable = enable;
#if CONFIG_BOARD_HAS_POWER_MGMT
    /* 快充使能为低电平有效 */
    return ioexp2_set_output(IOEXP2_PIN_CHARGE_QC, !enable);
#else
    return ESP_OK;
#endif
}

bool service_power_get_charge_qc_enable(void)
{
    return s_charge_qc_enable;
}

/* ---------- 公共 API：5V / Wi-Fi / 天线 ---------- */
esp_err_t service_power_set_usb_5v_enable(bool enable)
{
    ESP_LOGI(TAG, "usb 5v enable: %d", enable);
    s_usb_5v_enable = enable;
#if CONFIG_BOARD_HAS_POWER_MGMT
    return bsp_feature_enable(BSP_FEATURE_USB, enable);
#else
    return ESP_OK;
#endif
}

bool service_power_get_usb_5v_enable(void)
{
    return s_usb_5v_enable;
}

esp_err_t service_power_set_ext_5v_enable(bool enable)
{
    ESP_LOGI(TAG, "ext 5v enable: %d", enable);
    s_ext_5v_enable = enable;
#if CONFIG_BOARD_HAS_POWER_MGMT
    return ioexp1_set_output(IOEXP1_PIN_EXT_5V_EN, enable);
#else
    return ESP_OK;
#endif
}

bool service_power_get_ext_5v_enable(void)
{
    return s_ext_5v_enable;
}

esp_err_t service_power_set_wifi_power_enable(bool enable)
{
    ESP_LOGI(TAG, "wifi power enable: %d", enable);
    s_wifi_pwr_enable = enable;
#if CONFIG_BOARD_HAS_POWER_MGMT
    return bsp_feature_enable(BSP_FEATURE_WIFI, enable);
#else
    return ESP_OK;
#endif
}

bool service_power_get_wifi_power_enable(void)
{
    return s_wifi_pwr_enable;
}

esp_err_t service_power_set_ext_antenna_enable(bool enable)
{
    ESP_LOGI(TAG, "ext antenna enable: %d", enable);
    s_ext_ant_enable = enable;
#if CONFIG_BOARD_HAS_POWER_MGMT
    return ioexp1_set_output(IOEXP1_PIN_EXT_ANT, enable);
#else
    return ESP_OK;
#endif
}

bool service_power_get_ext_antenna_enable(void)
{
    return s_ext_ant_enable;
}

/* ---------- 公共 API：检测 ---------- */
bool service_power_is_headphone_connected(void)
{
#if CONFIG_BOARD_HAS_POWER_MGMT
    return ioexp1_get_input(IOEXP1_PIN_HEADPHONE);
#else
    /* 无耳机检测硬件（IO 扩展器 1 P7）：恒为未插入 */
    return false;
#endif
}

/* ---------- 公共 API：事件回调 ---------- */
esp_err_t service_power_register_event_callback(service_power_event_cb_t cb, void *user_data)
{
    s_event_cb = cb;
    s_event_user_data = user_data;
    return ESP_OK;
}

/* ---------- 关机与待机 ---------- */
#if CONFIG_BOARD_HAS_POWER_MGMT
static void poweroff_pulse(void)
{
    if (s_ioexp2 == NULL) {
        return;
    }

    ESP_LOGW(TAG, "generate poweroff signal");
    /* 与 UserDemo 一致：发送 3 次高电平脉冲 */
    for (int i = 0; i < 3; i++) {
        ioexp2_set_output(IOEXP2_PIN_POWEROFF, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        ioexp2_set_output(IOEXP2_PIN_POWEROFF, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void shutdown_peripherals(void)
{
    board_display_brightness_set(0);
    vTaskDelay(pdMS_TO_TICKS(100));
    bsp_feature_enable(BSP_FEATURE_SPEAKER, false);
    bsp_feature_enable(BSP_FEATURE_CAMERA, false);
    bsp_feature_enable(BSP_FEATURE_USB, false);
    bsp_feature_enable(BSP_FEATURE_WIFI, false);
}
#endif /* CONFIG_BOARD_HAS_POWER_MGMT */

void service_power_power_off(void)
{
    ESP_LOGI(TAG, "power off");
#if CONFIG_BOARD_HAS_POWER_MGMT
    shutdown_peripherals();
    poweroff_pulse();

    /* 若 PMS150G 未成功关机，进入 deep sleep 兜底 */
    ESP_LOGW(TAG, "poweroff pulse did not cut power, entering deep sleep fallback");
    esp_deep_sleep_start();
#else
    /* Why: 无 PMS150G 断电硬件的板型无法真正关机，重启是最接近"关机"的安全动作
     * （深睡无唤醒源会变成假死，故不用 deep sleep）。 */
    ESP_LOGW(TAG, "no poweroff hardware on this board, restarting instead");
    board_display_brightness_set(0);
    esp_restart();
#endif
    while (1) {
        vTaskDelay(portMAX_DELAY);
    }
}

static bool wait_touch_release(void)
{
    esp_lcd_touch_handle_t tp = NULL;
    /* 经 board_hal 取触摸句柄：板无关，两板各自实现 */
    if (board_touch_create(&tp) != ESP_OK || tp == NULL) {
        ESP_LOGW(TAG, "touch handle not available, skip release wait");
        return false;
    }

    esp_lcd_touch_point_data_t data[1];
    uint8_t cnt = 0;
    uint32_t timeout = pdMS_TO_TICKS(5000);
    uint32_t start = xTaskGetTickCount();

    while (1) {
        service_i2c_take();
        esp_lcd_touch_read_data(tp);
        esp_err_t ret = esp_lcd_touch_get_data(tp, data, &cnt, 1);
        service_i2c_give();
        if (ret == ESP_OK && cnt == 0) {
            esp_lcd_touch_del(tp);
            return true;
        }
        if ((xTaskGetTickCount() - start) > timeout) {
            esp_lcd_touch_del(tp);
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static bool wait_touch_press(void)
{
    esp_lcd_touch_handle_t tp = NULL;
    if (board_touch_create(&tp) != ESP_OK || tp == NULL) {
        ESP_LOGW(TAG, "touch handle not available, skip press wait");
        return false;
    }

    esp_lcd_touch_point_data_t data[1];
    uint8_t cnt = 0;
    uint32_t timeout = pdMS_TO_TICKS(30000);
    uint32_t start = xTaskGetTickCount();

    while (1) {
        service_i2c_take();
        esp_lcd_touch_read_data(tp);
        esp_err_t ret = esp_lcd_touch_get_data(tp, data, &cnt, 1);
        service_i2c_give();
        if (ret == ESP_OK && cnt > 0) {
            esp_lcd_touch_del(tp);
            return true;
        }
        if ((xTaskGetTickCount() - start) > timeout) {
            esp_lcd_touch_del(tp);
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void service_power_standby_touch_wakeup(void)
{
    ESP_LOGI(TAG, "standby touch wakeup");

    /* 手动熄屏等待触摸唤醒期间，暂停自动熄屏/自动休眠计时，避免互相干扰 */
    bool was_idle_enabled = s_idle_enabled;
    s_idle_enabled = false;

    /* 参考 M5Tab5-UserDemo：先保存当前亮度，关背光等待触摸释放/按下，再恢复亮度 */
    uint8_t brightness = service_nvs_get_brightness();
    if (brightness == 0) {
        brightness = 50;
    }

    board_display_brightness_set(0);

    wait_touch_release();
    wait_touch_press();

    vTaskDelay(pdMS_TO_TICKS(500));
    board_display_brightness_set(brightness);

    s_idle_enabled = was_idle_enabled;
    service_power_idle_reset();
}

void service_power_standby_rtc_wakeup(uint32_t seconds)
{
    ESP_LOGI(TAG, "standby rtc wakeup in %lu seconds", (unsigned long)seconds);

    service_rtc_clear_alarm();
    if (service_rtc_set_alarm_relative(seconds) != ESP_OK) {
        ESP_LOGE(TAG, "set rtc alarm failed");
        return;
    }

    service_power_power_off();
}

/* -------------------- 自动熄屏/自动休眠 -------------------- */

void service_power_idle_init(void)
{
    s_idle_timeout_index = service_nvs_get_idle_timeout_index();
    if (s_idle_timeout_index >= SERVICE_POWER_IDLE_TIMEOUT_COUNT) {
        s_idle_timeout_index = 2;
    }
    s_auto_sleep_enabled = service_nvs_get_auto_sleep_enabled();
    s_idle_enabled = false;
    s_screen_off = false;
    s_last_activity_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    s_screen_off_since_ms = 0;
    ESP_LOGI(TAG, "idle init: timeout_index=%u, auto_sleep=%d",
             (unsigned)s_idle_timeout_index, (int)s_auto_sleep_enabled);
}

void service_power_idle_set_enabled(bool enable)
{
    if (enable && !s_idle_enabled) {
        service_power_idle_reset();
    }
    s_idle_enabled = enable;
    ESP_LOGI(TAG, "idle enabled: %d", (int)enable);
}

void service_power_idle_set_timeout_index(uint8_t index)
{
    if (index >= SERVICE_POWER_IDLE_TIMEOUT_COUNT) {
        return;
    }
    s_idle_timeout_index = index;
    service_nvs_set_idle_timeout_index(index);
    service_power_idle_reset();
    ESP_LOGI(TAG, "idle timeout index: %u", (unsigned)index);
}

uint8_t service_power_idle_get_timeout_index(void)
{
    return s_idle_timeout_index;
}

void service_power_idle_set_auto_sleep_enabled(bool enable)
{
    s_auto_sleep_enabled = enable;
    service_nvs_set_auto_sleep_enabled(enable);
    service_power_idle_reset();
    ESP_LOGI(TAG, "auto sleep: %d", (int)enable);
}

bool service_power_idle_get_auto_sleep_enabled(void)
{
    return s_auto_sleep_enabled;
}

void service_power_idle_reset(void)
{
    s_last_activity_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (s_screen_off) {
        service_power_idle_wake();
    }
}

bool service_power_is_screen_off(void)
{
    return s_screen_off;
}

static void service_power_idle_screen_off(void)
{
    if (s_screen_off) {
        return;
    }

    s_screen_off_saved_brightness = service_nvs_get_brightness();
    if (s_screen_off_saved_brightness == 0) {
        s_screen_off_saved_brightness = 50;
    }

    board_display_brightness_set(0);
    s_screen_off = true;
    s_screen_off_since_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    ESP_LOGI(TAG, "screen off");
}

static void service_power_idle_wake(void)
{
    if (!s_screen_off) {
        service_power_idle_reset();
        return;
    }

    s_screen_off = false;
    board_display_brightness_set(s_screen_off_saved_brightness);
    s_last_activity_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    ESP_LOGI(TAG, "screen on");
}

void service_power_wake_screen(void)
{
    service_power_idle_wake();
}

void service_power_idle_process(void)
{
    if (!s_idle_enabled) {
        return;
    }

    uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if (s_screen_off) {
        if (!s_auto_sleep_enabled) {
            return;
        }
        if ((now_ms - s_screen_off_since_ms) >= SERVICE_POWER_AUTO_SLEEP_DELAY_MS) {
            ESP_LOGI(TAG, "auto sleep timeout, entering sleep");
            service_power_enter_auto_sleep();
        }
        return;
    }

    /* 对话进行中保持屏幕常亮，重置活动计时 */
    if (s_hold_screen_on) {
        s_last_activity_ms = now_ms;
        return;
    }

    uint32_t timeout_ms = s_idle_timeout_ms[s_idle_timeout_index];
    if ((now_ms - s_last_activity_ms) >= timeout_ms) {
        service_power_idle_screen_off();
    }
}

void service_power_hold_screen_on(bool hold)
{
    s_hold_screen_on = hold;
    if (hold) {
        /* 立即刷新活动时间，防止被旧超时戳触发熄屏 */
        s_last_activity_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        /* 屏幕已灭则立即唤醒 */
        if (s_screen_off) {
            service_power_idle_wake();
        }
        ESP_LOGI(TAG, "hold screen on: enabled");
    } else {
        ESP_LOGI(TAG, "hold screen on: disabled");
    }
}

void service_power_enter_auto_sleep(void)
{
    ESP_LOGI(TAG, "enter auto sleep");
    service_power_power_off();
    while (1) {
        vTaskDelay(portMAX_DELAY);
    }
}

esp_err_t service_power_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "init");

    esp_err_t ret = board_i2c_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "board_i2c_init failed: %d", ret);
        return ret;
    }

    ret = service_i2c_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "service_i2c_init failed: %d", ret);
        return ret;
    }

    ret = nvs_flash_init();
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NO_FREE_PAGES &&
        ret != ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "nvs_flash_init failed: %d", ret);
    }

    ret = service_nvs_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "service_nvs_init failed: %d", ret);
    } else {
        ESP_LOGI(TAG, "boot count: %lu, onboard_flag: %d",
                 (unsigned long)service_nvs_get_boot_count(),
                 service_nvs_is_initialized());
    }

#if CONFIG_BOARD_HAS_POWER_MGMT
    /* IO 扩展器：先初始化并打开充电路径 */
    ioexpander_init();

    /* 初始化后立即刷新一次输入状态，建立缓存和初始边沿状态。 */
    if (ioexp_take()) {
        ioexp_refresh_inputs();
        if (s_ioexp1_input_valid) {
            s_last_headphone_state = (s_ioexp1_input_cache & (uint32_t)IOEXP1_PIN_HEADPHONE) != 0;
        }
        ioexp_give();
    }

    /* 默认打开充电，与 UserDemo 启动流程一致 */
    service_power_set_charge_qc_enable(true);
    vTaskDelay(pdMS_TO_TICKS(50));
    service_power_set_charge_enable(true);
    vTaskDelay(pdMS_TO_TICKS(100));

    /* INA226：在充电路径使能后再初始化，与 UserDemo 顺序一致 */
    i2c_master_bus_handle_t bus = board_i2c_get_handle();
    if (bus == NULL) {
        ESP_LOGW(TAG, "I2C bus not ready, battery monitor disabled");
    } else if (service_i2c_take()) {
        ESP_LOGI(TAG, "INA226 init on i2c bus %p", (void *)bus);
        esp_err_t ina_ret = ina226_init(bus, INA226_DEFAULT_ADDR, INA226_SHUNT_OHM, INA226_MAX_CURRENT_A);
        if (ina_ret != ESP_OK) {
            ESP_LOGW(TAG, "INA226 init failed: %s, battery monitor disabled", esp_err_to_name(ina_ret));
        } else {
            ina226_configure(INA226_AVERAGES_16, INA226_BUS_CONV_TIME_1100US,
                             INA226_SHUNT_CONV_TIME_1100US, INA226_MODE_SHUNT_BUS_CONT);
            ESP_LOGI(TAG, "INA226 init ok");
        }
        service_i2c_give();
    } else {
        ESP_LOGW(TAG, "i2c lock failed, battery monitor disabled");
    }
#endif /* CONFIG_BOARD_HAS_POWER_MGMT */

    s_initialized = true;

    /* service_power_init 会重新初始化 IO 扩展器并清掉输出，
     * 这里再次打开 C6 电源，确保后续 Wi-Fi 正常工作。
     * 无电源硬件的板型该 setter 为空操作。 */
    service_power_set_wifi_power_enable(true);

    service_power_idle_init();

    ESP_LOGI(TAG, "init ok");
    return ESP_OK;
}
