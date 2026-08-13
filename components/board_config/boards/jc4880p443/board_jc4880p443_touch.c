/**
 * @file board_jc4880p443_touch.c
 * @brief Guition JC4880P443 触摸板级实现
 *
 * GT911 挂在与 codec 共享的 I2C_NUM_1 上；坐标系 480x800，
 * mirror_y=1 匹配面板竖装方向（厂商板级包 480x800 分支）。
 */

#include "board_hal.h"
#include "board_config.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_log.h"

static const char *TAG = "board_jc_touch";

#define JC_TOUCH_RST_GPIO   GPIO_NUM_22
#define JC_TOUCH_INT_GPIO   GPIO_NUM_21
#define JC_TOUCH_I2C_SCL_HZ 100000

esp_err_t board_touch_create(esp_lcd_touch_handle_t *out_tp)
{
    if (out_tp == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Contract: 触摸复用 board_i2c 总线；engine_gui 先于 service_audio 初始化，
     * 此处必须自行保证总线已建（幂等） */
    esp_err_t ret = board_i2c_init();
    if (ret != ESP_OK) {
        return ret;
    }

    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    tp_io_config.scl_speed_hz = JC_TOUCH_I2C_SCL_HZ;
    ret = esp_lcd_new_panel_io_i2c(board_i2c_get_handle(), &tp_io_config, &tp_io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "touch io create failed: %d", ret);
        return ret;
    }

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = BOARD_LCD_H_RES,
        .y_max = BOARD_LCD_V_RES,
        .rst_gpio_num = JC_TOUCH_RST_GPIO,
        .int_gpio_num = JC_TOUCH_INT_GPIO,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 1,
        },
    };
    ret = esp_lcd_touch_new_i2c_gt911(tp_io, &tp_cfg, out_tp);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gt911 create failed: %d", ret);
        return ret;
    }
    ESP_LOGI(TAG, "GT911 touch ready (%dx%d)", BOARD_LCD_H_RES, BOARD_LCD_V_RES);
    return ESP_OK;
}
