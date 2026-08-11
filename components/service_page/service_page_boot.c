/**
 * @file service_page_boot.c
 * @brief Boot 屏幕后端（自 engine_gui 解耦）
 *
 * boot 屏由 engine_gui 的 ui_init 直接加载，本模块负责其上依赖中文字库的
 * label 显示、进度文案与静态文案翻译。
 */

#include "service_page_boot.h"
#include "screens.h"
#include "eez-flow.h"
#include "engine_gui.h"
#include "service_i18n.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"

static const char *TAG = "service_page_boot";

void service_page_boot_reveal_labels(void)
{
    lvgl_port_lock(portMAX_DELAY);
    /* 依赖中文字库的 label 默认隐藏，字体加载完成后才显示 */
    if (objects.boot_label_name != NULL) {
        lv_obj_remove_flag(objects.boot_label_name, LV_OBJ_FLAG_HIDDEN);
    }
    if (objects.boot_label_name_en != NULL) {
        lv_obj_remove_flag(objects.boot_label_name_en, LV_OBJ_FLAG_HIDDEN);
    }
    if (objects.boot_label_loading != NULL) {
        lv_obj_remove_flag(objects.boot_label_loading, LV_OBJ_FLAG_HIDDEN);
    }
    if (objects.boot_version != NULL) {
        lv_obj_remove_flag(objects.boot_version, LV_OBJ_FLAG_HIDDEN);
    }
    lvgl_port_unlock();
}

void service_page_boot_progress_text(int percent)
{
    const char *text = NULL;
    if (percent >= 80) {
        text = service_i18n_translate("喵喵起床啦！");
        ESP_LOGI(TAG, "Boot progress: %d%% - %s", percent, text);
    } else if (percent >= 70) {
        text = service_i18n_translate("喵喵正在起床...");
        ESP_LOGI(TAG, "Boot progress: %d%% - %s", percent, text);
   }

    if (text == NULL) {
        return;
    }

    lvgl_port_lock(portMAX_DELAY);
    if (objects.boot_label_loading != NULL &&
        eez_flow_get_current_screen() == SCREEN_ID_BOOT) {
        lv_label_set_text(objects.boot_label_loading, text);
    }
    lvgl_port_unlock();
}

void service_page_boot_translate(void)
{
    /* boot 屏由 ui_init 直接加载，不经 switch_screen，需在此按当前语言翻译 */
    lvgl_port_lock(portMAX_DELAY);
    engine_gui_translate_obj_tree(lv_screen_active());
    lvgl_port_unlock();
}
