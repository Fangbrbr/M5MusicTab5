idf_component_register(SRCS "app_midi_player.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_manager
                                service_i18n
                                engine_gui
                                engine_midi
                                service_nvs
                                service_player
                                service_audio
                                service_timer
                                service_xiaozhi)