/**
 * @file engine_gui_keyboard.h
 * @brief 自定义 LVGL 键盘映射配置
 *
 * EEZ Flow 键盘 widget 使用 USER1~USER4 模式时，后端通过
 * engine_gui_keyboard_init() 把自定义地图注册到 LVGL 全局表，
 * 替代默认复用的 TEXT_LOWER 地图。
 */

#ifndef ENGINE_GUI_KEYBOARD_H
#define ENGINE_GUI_KEYBOARD_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EG_GUI_KB_USER1_KEY_COMMA       ","
#define EG_GUI_KB_USER1_KEY_PERIOD      "."
#define EG_GUI_KB_USER1_KEY_DELETE      LV_SYMBOL_BACKSPACE
#define EG_GUI_KB_USER1_KEY_SPACE       " "
#define EG_GUI_KB_USER1_KEY_ENTER       LV_SYMBOL_NEW_LINE
#define EG_GUI_KB_USER1_KEY_READY       LV_SYMBOL_OK

/**
 * @brief 注册所有自定义键盘地图
 *
 * 必须在 ui_init() 之前调用：EEZ 在 ui_init() 中创建键盘并设置 mode=USER1，
 * 此时 LVGL 从全局 kb_map[] 读取地图；若自定义地图尚未写入，会显示默认全键盘。
 */
void engine_gui_keyboard_init(void);

#ifdef __cplusplus
}
#endif

#endif /* ENGINE_GUI_KEYBOARD_H */
