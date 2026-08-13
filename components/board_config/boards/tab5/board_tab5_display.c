/**
 * @file board_tab5_display.c
 * @brief M5Stack Tab5 显示 / I2C 板级实现
 *
 * 纯转调 espressif__m5stack_tab5 BSP，不含业务逻辑。
 */

#include "board_hal.h"
#include "board_config.h"
#include "bsp/m5stack_tab5.h"
#include "bsp/display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "board_tab5_display";

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
    return bsp_display_brightness_set(percent);
}

lv_indev_t *board_display_get_default_indev(void)
{
    return bsp_display_get_input_dev();
}
