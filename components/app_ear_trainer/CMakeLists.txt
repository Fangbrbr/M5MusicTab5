idf_component_register(SRCS "app_ear_trainer.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_manager
                                engine_gui
                                engine_midi
                       PRIV_REQUIRES esp_timer
                                     esp_lvgl_port
                                     service_nvs)
