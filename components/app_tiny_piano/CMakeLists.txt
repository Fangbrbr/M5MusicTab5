idf_component_register(SRCS "app_tiny_piano.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_manager
                                engine_gui
                                engine_midi
                                service_i18n
                       PRIV_REQUIRES esp_timer
                                     esp_lvgl_port)
