idf_component_register(SRCS "app_manager.c"
                       INCLUDE_DIRS "include"
                       REQUIRES engine_midi
                                 service_recorder
                                 espressif__esp_lvgl_port
                       PRIV_REQUIRES service_rtc
                                     app_ai_agent
                                     app_chord_trainer
                                     app_circle_of_fifths
                                     app_clock_calendar
                                     app_drum_pad
                                     app_ear_trainer
                                     app_midi_player
                                     app_metronome
                                     app_tiny_piano
                                     app_xy_pad
                                     app_zen
                                     app_fun)

# ---------------------------------------------------------------------------
# 构建期校验：App WIDGET_BIND 名称必须与生成代码（src/ui/screens.h 的
# ui_screen_*_t 结构体字段）一一对应，不依赖 EEZ 工程文件。
# 前端误删/重命名控件后，若后端未同步，此处直接构建失败。
# ---------------------------------------------------------------------------
if(NOT Python_EXECUTABLE)
    find_package(Python REQUIRED)
endif()

set(WIDGET_CHECK_SCRIPT ${CMAKE_SOURCE_DIR}/tools/check_widget_bindings.py)
set(WIDGET_CHECK_STAMP  ${CMAKE_BINARY_DIR}/widget_bindings_checked.stamp)

file(GLOB WIDGET_CHECK_APP_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/components/app_*/app_*.c)
# 生成代码变化时触发重跑（EEZ 重导出会刷新 screens.h）
set(WIDGET_CHECK_GENERATED ${CMAKE_SOURCE_DIR}/components/engine_gui/src/ui/screens.h)

add_custom_command(
    OUTPUT ${WIDGET_CHECK_STAMP}
    COMMAND ${Python_EXECUTABLE} ${WIDGET_CHECK_SCRIPT}
    COMMAND ${CMAKE_COMMAND} -E touch ${WIDGET_CHECK_STAMP}
    DEPENDS ${WIDGET_CHECK_SCRIPT}
            ${WIDGET_CHECK_GENERATED}
            ${WIDGET_CHECK_APP_SOURCES}
    COMMENT "Checking widget bindings against generated UI code"
    VERBATIM
)

add_custom_target(widget_bindings_check DEPENDS ${WIDGET_CHECK_STAMP})
add_dependencies(${COMPONENT_LIB} widget_bindings_check)
