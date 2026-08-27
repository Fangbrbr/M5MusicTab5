/**
 * @file app_input_event.h
 * @brief App 输入事件类型
 *
 * 与 App Manager 解耦的通用输入事件定义，供 GUI 层、输入任务及 App 共享，
 * 避免 engine_gui 与 app_manager 之间出现循环依赖。
 */

#ifndef APP_INPUT_EVENT_H
#define APP_INPUT_EVENT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_INPUT_TOUCH_DOWN = 0,
    APP_INPUT_TOUCH_MOVE,
    APP_INPUT_TOUCH_UP,
    APP_INPUT_MOUSE_MOVE,
    APP_INPUT_MOUSE_BUTTON,
    APP_INPUT_KEYBOARD,
} app_input_type_t;

typedef struct {
    app_input_type_t type;
    int16_t x;
    int16_t y;
    uint8_t finger_id;  /**< touch: finger id; mouse: button id; keyboard: key code */
    uint8_t flags;      /**< mouse: button state; keyboard: modifier */
    uint8_t pressure;   /**< touch: 按键力度 0~127（引擎层归一化）；非触摸输入置 0 */
} app_input_event_t;

#ifdef __cplusplus
}
#endif

#endif /* APP_INPUT_EVENT_H */
