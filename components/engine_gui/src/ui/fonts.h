#ifndef EEZ_LVGL_UI_FONTS_H
#define EEZ_LVGL_UI_FONTS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern lv_font_t *ui_font_icon_70;
extern lv_font_t *ui_font_chinese_30;
extern lv_font_t *ui_font_chinese_40;
extern lv_font_t *ui_font_clock_150;
extern lv_font_t *ui_font_clock_150_a;
extern lv_font_t *ui_font_digi_30;

#ifndef EXT_FONT_DESC_T
#define EXT_FONT_DESC_T
typedef struct _ext_font_desc_t {
    const char *name;
    const void *font_ptr;
} ext_font_desc_t;
#endif

extern ext_font_desc_t fonts[];

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_FONTS_H*/