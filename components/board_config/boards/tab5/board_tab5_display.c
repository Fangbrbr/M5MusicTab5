/**
 * @file board_tab5_display.c
 * @brief M5Stack Tab5 显示 / I2C 板级实现
 *
 * 大部分转调 espressif__m5stack_tab5 BSP；背光为自制方案：复用 BSP 已配置的
 * LEDC TIMER_0/CHANNEL_1（10bit/5kHz），占空比经「底线 + γ 曲线」查找表输出。
 */

#include "board_hal.h"
#include "board_config.h"
#include "bsp/m5stack_tab5.h"
#include "bsp/display.h"
#include "sdkconfig.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "board_tab5_display";

/* 背光映射（自制）：duty = 5 + 1018 × ((pct-1)/99)^1.5，0 恒为 0。
 * Why 底线 5：BSP 线性 1%≈duty10 夜间仍偏亮；γ 纯曲线低端取整不可见
 * （γ2.5→duty0、γ1.5→duty2 两次真机黑屏教训）。duty5 ≈ BSP 1% 的一半亮度，
 * 是"暗但可见"的夜间档。
 * Why γ=1.5 曲线：低端细分（1%~10% 占 duty 5→33，暗档可调），高端感知线性。
 * Trap: 禁止自建 LEDC 定时器——低速模式时钟源分频全局共享，自建触发
 * timer clock conflict 致背光常灭；只能复用 BSP 通道写占空比。 */
#define TAB5_BL_LEDC_CHANNEL    CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH
#define TAB5_BL_LEDC_MAX_DUTY   1023  /* BSP 背光定时器为 10bit */

/* 索引 0~100：duty = 5 + 1018×((pct-1)/99)^1.5（pct≥1），0 → 0（熄屏路径依赖） */
static const uint16_t s_backlight_duty_lut[101] = {
    0, 5, 6, 8, 10, 13, 17, 20, 24, 28, 33,
    38, 43, 48, 53, 59, 65, 71, 77, 84, 91, 97,
    104, 112, 119, 127, 134, 142, 150, 158, 166, 175, 183,
    192, 201, 210, 219, 228, 238, 247, 257, 266, 276, 286,
    296, 307, 317, 327, 338, 349, 359, 370, 381, 393, 404,
    415, 427, 438, 450, 461, 473, 485, 497, 510, 522, 534,
    547, 559, 572, 585, 597, 610, 623, 636, 650, 663, 676,
    690, 703, 717, 731, 744, 758, 772, 786, 801, 815, 829,
    844, 858, 873, 887, 902, 917, 932, 947, 962, 977, 992,
    1008, 1023
};

esp_err_t board_i2c_init(void)
{
    return bsp_i2c_init();
}

i2c_master_bus_handle_t board_i2c_get_handle(void)
{
    return bsp_i2c_get_handle();
}

esp_err_t board_display_create(board_lcd_handles_t *out_handles)
{
    if (out_handles == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const bsp_display_config_t bsp_disp_cfg = {
        .dsi_bus = {
            .phy_clk_src = 0,
            .lane_bit_rate_mbps = BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS,
        }
    };

    bsp_lcd_handles_t lcd_handles;
    if (bsp_display_new_with_handles(&bsp_disp_cfg, &lcd_handles) != ESP_OK) {
        ESP_LOGE(TAG, "bsp_display_new_with_handles failed");
        return ESP_FAIL;
    }
    out_handles->io = lcd_handles.io;
    out_handles->panel = lcd_handles.panel;
    return ESP_OK;
}

void board_display_warm_reset_power_cycle(void)
{
    /* Why: 热复位（看门狗/软件复位）后面板供电从未断开，GRAM/时序可能残留，
     * esp_lcd 软件复位序列无法保证干净状态；经 IO 扩展器拉 LCD 供电做一次
     * 真正的 POR。延时 200ms 保证电源彻底跌落与重新稳定。 */
    bsp_feature_enable(BSP_FEATURE_LCD, false);
    vTaskDelay(pdMS_TO_TICKS(200));
    bsp_feature_enable(BSP_FEATURE_LCD, true);
    vTaskDelay(pdMS_TO_TICKS(200));
}

esp_err_t board_display_brightness_set(int percent)
{
    if (percent > 100) {
        percent = 100;
    } else if (percent < 0) {
        percent = 0;
    }

    /* 经底线+γ 查找表映射到 10bit 占空比，0 恒为 0（熄屏路径依赖） */
    uint32_t duty = s_backlight_duty_lut[percent];
    ESP_LOGD(TAG, "backlight pct=%d duty=%lu/%u", percent, (unsigned long)duty,
             (unsigned)TAB5_BL_LEDC_MAX_DUTY);

    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, TAB5_BL_LEDC_CHANNEL, duty);
    if (ret != ESP_OK) {
        return ret;
    }
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, TAB5_BL_LEDC_CHANNEL);
}

lv_indev_t *board_display_get_default_indev(void)
{
    return bsp_display_get_input_dev();
}
