idf_component_register(SRCS "task_audio.c"
                       INCLUDE_DIRS "include"
                       REQUIRES service_audio
                                service_voice)
