idf_component_register(SRCS "service_xiaozhi.c"
                            "xiaozhi_ota.c"
                            "xiaozhi_mcp.c"
                       INCLUDE_DIRS "include"
                       REQUIRES json
                                esp_app_format
                                mbedtls
                                spi_flash
                                espressif__esp_lvgl_port
                                board_config
                                engine_gui
                                service_audio
                                service_nvs
                                service_rtc
                                service_power
                                esp_timer
                                service_voice
                                service_wifi
                                service_ws
                                service_http_client
                                espressif__esp_audio_effects)

# 版本号统一由顶层 CMakeLists 计算（FW_VERSION 缓存变量），此处仅注入编译宏
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    FIRMWARE_VERSION="${FW_VERSION}")
