idf_component_register(SRCS "service_voice.c"
                           "service_voice_wake.c"
                           "service_voice_opus.c"
                           INCLUDE_DIRS "include"
                           REQUIRES service_audio
                                    espressif__esp_audio_codec
                                    espressif__esp_audio_effects
                                    espressif__esp-sr
                                    freertos)
