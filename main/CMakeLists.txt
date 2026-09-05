idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES service_power
                                engine_sf2
                                engine_gui
                                engine_midi
                                app_manager
                                service_audio
                                service_voice
                                service_page
                                service_input
                                service_sd
                                service_timer
                                service_rtc
                                service_recorder
                                service_wavrec
                                service_ftp
                                service_i2c
                                service_wifi
                                service_http_client
                                service_nvs
                                service_usb_host
                                task_app
                                task_audio
                                task_comm
                                task_gui
                                task_input
                                task_ai
                                board_config)

# 版本号统一由顶层 CMakeLists 计算（FW_VERSION 缓存变量），此处仅注入编译宏
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    FIRMWARE_VERSION="${FW_VERSION}")
