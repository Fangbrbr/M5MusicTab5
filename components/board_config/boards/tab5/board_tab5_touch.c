/**
 * @file board_tab5_touch.c
 * @brief M5Stack Tab5 触摸板级实现
 */

#include "board_hal.h"
#include "bsp/touch.h"

// static const char *TAG = "board_tab5_touch";

esp_err_t board_touch_create(esp_lcd_touch_handle_t *out_tp)
{
    return bsp_touch_new(NULL, out_tp);
}
