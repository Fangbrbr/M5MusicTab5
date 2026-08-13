#pragma once

#include "sdkconfig.h"

/* 板卡参数统一出口：所有组件只 include 本头，禁止再直接 include bsp/m5stack_tab5.h。
 * 物理分辨率 = 面板原生（竖）；UI 逻辑分辨率 = 旋转 90°/270° 后的横向。 */

#if CONFIG_BOARD_TYPE_TAB5

#define BOARD_LCD_H_RES         720
#define BOARD_LCD_V_RES         1280
#define BOARD_UI_H_RES          1280
#define BOARD_UI_V_RES          720

/* AI 对话 LED 停靠坐标（与 EEZ 工程中 xx_led_ai 摆放耦合，重排 UI 后需同步） */
#define BOARD_AI_LED_HOME_X     620
#define BOARD_AI_LED_HOME_Y     677
#define BOARD_AI_LED_OFF_Y      760
#define BOARD_AI_LED_W          40
#define BOARD_AI_LED_H          35

#elif CONFIG_BOARD_TYPE_JC4880P443

#define BOARD_LCD_H_RES         480
#define BOARD_LCD_V_RES         800
#define BOARD_UI_H_RES          800
#define BOARD_UI_V_RES          480

/* jc 版 EEZ 重绘前的按比缩放占位值 */
#define BOARD_AI_LED_HOME_X     388
#define BOARD_AI_LED_HOME_Y     451
#define BOARD_AI_LED_OFF_Y      507
#define BOARD_AI_LED_W          25
#define BOARD_AI_LED_H          23

#else
#error "No board selected: please set CONFIG_BOARD_TYPE_* in menuconfig -> Board Selection"
#endif

/* 刷新管线红线：部分缓冲块高 50 为 PSRAM 驻留方案实测 underrun 安全上限，禁止调大。
 * jc4880p443 带宽压力约为 Tab5 的 40%，沿用同一安全值起步。 */
#define BOARD_LCD_DRAW_BUF_HEIGHT 50
