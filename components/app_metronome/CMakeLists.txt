idf_component_register(SRCS "app_metronome.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_manager
                                service_i18n
                                service_timer
                                engine_gui
                                engine_midi
                       PRIV_REQUIRES esp_timer
                                     esp_lvgl_port
                                     service_nvs)
