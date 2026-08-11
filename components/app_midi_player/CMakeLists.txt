idf_component_register(SRCS "app_midi_player.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_manager
                                engine_gui
                                engine_midi
                                service_nvs)
