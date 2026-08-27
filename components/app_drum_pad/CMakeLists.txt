idf_component_register(SRCS "app_drum_pad.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_manager
                                service_i18n
                                engine_midi
                       PRIV_REQUIRES esp_timer
                                     esp_lvgl_port
                                     engine_gui)
