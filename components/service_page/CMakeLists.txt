idf_component_register(SRCS "service_page.c"
                                    "service_page_launcher.c"
                                    "service_page_setting.c"
                                    "service_page_onboard.c"
                                    "service_page_about.c"
                                    "service_page_ftp.c"
                                    "service_page_boot.c"
                                    "service_page_screenshot.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_manager
                                engine_gui
                                engine_sf2
                                service_nvs
                                service_audio
                                service_i18n
                                service_wifi
                                service_rtc
                                service_sd
                                service_ftp
                                service_xiaozhi
                                service_power
                                espressif__esp_lvgl_port
                                board_config)
