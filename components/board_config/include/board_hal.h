#pragma once

/* 板级硬件抽象接口：两板各自在 boards/<board>/ 下实现。
 * 契约：所有实现只含硬件初始化/路由，不含业务逻辑；
 * 调用方（engine_gui/service_audio/...）不感知具体板型。 */

#include <stdbool.h>
#include "esp_err.h"
#include "esp_lcd_types.h"
#include "esp_lcd_touch.h"
#include "esp_codec_dev.h"
#include "driver/i2c_master.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_handle_t panel;
} board_lcd_handles_t;

/* I2C 总线：全系统共享（codec/触摸/RTC/IMU 等），初始化幂等 */
esp_err_t board_i2c_init(void);
i2c_master_bus_handle_t board_i2c_get_handle(void);

/* 显示：创建 DSI 面板与 io 句柄（不含 LVGL port 注册，由 engine_gui 完成） */
esp_err_t board_display_create(board_lcd_handles_t *out_handles);

/* 热复位时的面板电源级重启（真正 POR）；无独立供电控制的板做空实现 */
void board_display_warm_reset_power_cycle(void);

/* 背光亮度 0-100 */
esp_err_t board_display_brightness_set(int percent);

/* BSP 可能自带默认 LVGL 输入设备，返回之以便 engine_gui 移除后自建多点 indev；
 * 无默认 indev 的板返回 NULL */
lv_indev_t *board_display_get_default_indev(void);

/* 触摸：创建触摸句柄（镜像/交换轴等板级差异在实现内处理） */
esp_err_t board_touch_create(esp_lcd_touch_handle_t *out_tp);

/* 音频 codec（esp_codec_dev 统一封装）。mic 不可用的板返回 NULL */
esp_codec_dev_handle_t board_audio_speaker_codec_init(void);
esp_codec_dev_handle_t board_audio_mic_codec_init(void);

/* 扬声器功放使能（耳机拔出/插入路由用）；无功放控制的板做 stub */
esp_err_t board_audio_speaker_pa_set(bool enable);

#ifdef __cplusplus
}
#endif
