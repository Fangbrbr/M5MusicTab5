idf_component_register(SRCS "service_nvs.c"
                       INCLUDE_DIRS "include"
                       REQUIRES nvs_flash
                                freertos)
