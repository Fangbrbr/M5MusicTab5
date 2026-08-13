set(UI_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/ui")

if(EXISTS "${UI_SRC_DIR}")
    # CONFIGURE_DEPENDS: 文件增删时自动重新配置 CMake，避免增量构建遗漏
    file(GLOB UI_C_SOURCES CONFIGURE_DEPENDS "${UI_SRC_DIR}/*.c")
    file(GLOB UI_CXX_SOURCES CONFIGURE_DEPENDS "${UI_SRC_DIR}/*.cpp")
    file(GLOB UI_EMBED_BINS CONFIGURE_DEPENDS "${UI_SRC_DIR}/*.bin")
    list(FILTER UI_C_SOURCES EXCLUDE REGEX "user_actions\\.c$")
    set(UI_INCLUDES "${UI_SRC_DIR}")

    # src/ui 为 EEZ Studio 生成代码，禁止手动修改；此处仅抑制常见生成代码警告
    foreach(src ${UI_C_SOURCES} ${UI_CXX_SOURCES})
        set_source_files_properties(${src} PROPERTIES COMPILE_FLAGS "-Wno-unused-variable -Wno-unused-parameter -Wno-unused-but-set-variable")
    endforeach()
else()
    set(UI_C_SOURCES "")
    set(UI_CXX_SOURCES "")
    set(UI_INCLUDES "")
endif()

idf_component_register(SRCS "engine_gui.c"
                                    "eez_backend.cpp"
                                    "src/engine_gui_keyboard.c"
                                    "src/engine_gui_res_vfs.c"
                                    ${UI_C_SOURCES}
                                    ${UI_CXX_SOURCES}
                       INCLUDE_DIRS "include"
                                    ${UI_INCLUDES}
                       REQUIRES board_config
                                espressif__esp_lcd_touch
                                espressif__esp_lvgl_port
                                app_manager
                                engine_midi
                                service_usb_host
                                service_wifi
                                service_rtc
                                service_i2c
                                service_power
                                service_sd
                                service_spiffs
                                service_audio
                                service_ble
                                service_nvs
                                service_i18n
                                service_xiaozhi
                                esp_app_format
                       PRIV_REQUIRES service_page espcoredump
                       EMBED_FILES ${UI_EMBED_BINS})

# EEZ Studio 生成代码直接使用 lv_qrcode_* API，且 translated-literal 生成代码需要 _() 宏。
# 通过强制包含统一 wrapper 头，避免多次 -include 导致 CMake/gcc 解析异常。
# 仅作用于 C/CXX：EMBED_FILES 生成的 .S 汇编被强制包含 C 头会报 unrecognized opcode。
target_compile_options(${COMPONENT_LIB} PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-include$<SEMICOLON>eez_forced_includes.h>
    $<$<COMPILE_LANGUAGE:CXX>:-include$<SEMICOLON>eez_forced_includes.h>)

# 版本号统一由顶层 CMakeLists 计算（FW_VERSION 缓存变量），此处仅注入编译宏
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    FIRMWARE_VERSION="${FW_VERSION}")
