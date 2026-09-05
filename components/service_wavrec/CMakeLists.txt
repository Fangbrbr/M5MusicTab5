idf_component_register(SRCS "service_wavrec.c"
                       INCLUDE_DIRS "include"
                       REQUIRES service_audio
                                service_voice
                                service_xiaozhi
                                service_sd
                                service_rtc)
