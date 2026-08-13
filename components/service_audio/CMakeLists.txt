idf_component_register(SRCS "service_audio.c"
                       INCLUDE_DIRS "include"
                       REQUIRES board_config
                                service_power
                                service_sd)
