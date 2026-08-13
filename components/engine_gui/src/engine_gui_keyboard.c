/**
 * @file engine_gui_keyboard.c
 * @brief 自定义 LVGL 键盘映射实现
 */

#include "engine_gui_keyboard.h"

static const char * const eg_gui_kb_user1_map[] = {
    EG_GUI_KB_USER1_KEY_COMMA,
    EG_GUI_KB_USER1_KEY_PERIOD,
    "\n",
    EG_GUI_KB_USER1_KEY_SPACE,
    "\n",
    EG_GUI_KB_USER1_KEY_DELETE,
    EG_GUI_KB_USER1_KEY_ENTER,
    "\n",
    EG_GUI_KB_USER1_KEY_READY,
    ""
};

/* LVGL 未导出 LV_KB_BTN(w) 宏，使用等价公开标志组合 */
#define EG_GUI_KB_BTN(width)    (LV_BUTTONMATRIX_CTRL_POPOVER | (width))

static const lv_buttonmatrix_ctrl_t eg_gui_kb_user1_ctrl[] = {
    EG_GUI_KB_BTN(1),
    EG_GUI_KB_BTN(1),
    6,
    LV_KEYBOARD_CTRL_BUTTON_FLAGS | 3,
    LV_KEYBOARD_CTRL_BUTTON_FLAGS | 3,
    LV_KEYBOARD_CTRL_BUTTON_FLAGS | 6
};

void engine_gui_keyboard_init(void)
{
    /* LVGL 的 kb_map[] / kb_ctrl[] 是全局静态表；创建临时键盘对象写入地图后即可删除 */
    lv_obj_t *kb = lv_keyboard_create(lv_screen_active());
    if (kb == NULL) {
        return;
    }

    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_USER_1, eg_gui_kb_user1_map, eg_gui_kb_user1_ctrl);

    lv_obj_delete(kb);
}
