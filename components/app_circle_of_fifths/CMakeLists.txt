idf_component_register(SRCS "app_circle_of_fifths.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_manager
                                engine_gui
                                engine_midi
                                espressif__esp_lvgl_port
                       PRIV_REQUIRES esp_timer)

# 让 component 可以直接使用 EEZ UI 的头文件（包含 lvgl.h 和 fonts.h）
target_include_directories(${COMPONENT_LIB} PRIVATE "${COMPONENT_DIR}/../engine_gui/src/ui")
