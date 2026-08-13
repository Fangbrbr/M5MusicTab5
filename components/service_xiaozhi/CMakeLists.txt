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
