idf_component_register(SRCS "app_chord_trainer.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_manager
                                engine_gui
                                engine_midi
                       PRIV_REQUIRES esp_lvgl_port
                                     esp_timer)

# 让 component 可以直接使用 EEZ UI 的头文件（fonts.h：五度圈调名字库）
target_include_directories(${COMPONENT_LIB} PRIVATE "${COMPONENT_DIR}/../engine_gui/src/ui")
