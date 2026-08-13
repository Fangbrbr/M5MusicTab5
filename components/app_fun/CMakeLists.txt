# Trap: IDF 早期组件依赖展开（component_get_requirements）阶段 Kconfig 未加载，
# CONFIG_BOARD_HAS_IMU 不可见；用板型符号反向判断（默认/未知一律按 Tab5 保留 bmi270，
# 仅 JC 板剔除），与 board_config/CMakeLists.txt 同款写法。
set(APP_FUN_REQUIRES app_manager
                     engine_gui
                     service_nvs
                     service_i18n
                     board_config
                     espressif__esp_lcd_touch_st7123)
if(NOT CONFIG_BOARD_TYPE_JC4880P443)
    list(APPEND APP_FUN_REQUIRES bmi270)
endif()

idf_component_register(SRCS "app_fun.c"
                       INCLUDE_DIRS "include"
                       REQUIRES ${APP_FUN_REQUIRES})
