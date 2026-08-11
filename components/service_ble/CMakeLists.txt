idf_component_register(SRCS "service_ble_midi.c"
                       INCLUDE_DIRS "include"
                       REQUIRES engine_midi
                                espressif__esp_hosted
                                bt
                                esp_timer)
