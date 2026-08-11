idf_component_register(SRCS "app_chord_trainer.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_manager
                                engine_gui
                                engine_midi
                       PRIV_REQUIRES esp_lvgl_port
                                     esp_timer)
