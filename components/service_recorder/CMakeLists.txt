idf_component_register(SRCS "service_recorder.c"
                       INCLUDE_DIRS "include"
                       REQUIRES engine_midi
                                service_rtc
                                service_sd)
