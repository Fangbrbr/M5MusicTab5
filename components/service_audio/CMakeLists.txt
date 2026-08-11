idf_component_register(SRCS "service_audio.c"
                       INCLUDE_DIRS "include"
                       REQUIRES espressif__m5stack_tab5
                                service_power
                                service_sd)
