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
# 构建期校验：App WIDGET_BIND 名称必须与 EEZ Studio 工程中的控件 identifier
# 一一对应。前端误删/重命名控件后，若后端未同步，此处直接构建失败。
# ---------------------------------------------------------------------------
if(NOT Python_EXECUTABLE)
    find_package(Python REQUIRED)
endif()

set(WIDGET_CHECK_SCRIPT ${CMAKE_SOURCE_DIR}/tools/check_widget_bindings.py)
set(WIDGET_CHECK_STAMP  ${CMAKE_BINARY_DIR}/widget_bindings_checked.stamp)

file(GLOB WIDGET_CHECK_APP_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/components/app_*/app_*.c)

add_custom_command(
    OUTPUT ${WIDGET_CHECK_STAMP}
    COMMAND ${Python_EXECUTABLE} ${WIDGET_CHECK_SCRIPT}
    COMMAND ${CMAKE_COMMAND} -E touch ${WIDGET_CHECK_STAMP}
    DEPENDS ${WIDGET_CHECK_SCRIPT}
            ${CMAKE_SOURCE_DIR}/components/engine_gui/TAB_MusicBox.eez-project
            ${WIDGET_CHECK_APP_SOURCES}
    COMMENT "Checking widget bindings against EEZ project"
    VERBATIM
)

add_custom_target(widget_bindings_check DEPENDS ${WIDGET_CHECK_STAMP})
add_dependencies(${COMPONENT_LIB} widget_bindings_check)
