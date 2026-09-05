idf_component_register(SRCS "app_sequencer.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_manager
                                service_i18n
                                service_nvs
                                service_sd
                                service_timer
                                engine_midi
                                engine_sequencer
                       PRIV_REQUIRES esp_timer
                                     esp_lvgl_port
                                     engine_gui)
