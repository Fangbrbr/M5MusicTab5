idf_component_register(SRCS "task_comm.c"
                       INCLUDE_DIRS "include"
                       REQUIRES engine_midi
                                service_usb_host
                                service_ws)
