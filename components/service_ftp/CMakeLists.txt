idf_component_register(SRCS "service_ftp.c"
                       INCLUDE_DIRS "include"
                       REQUIRES lwip
                                service_wifi
                                service_sd)
