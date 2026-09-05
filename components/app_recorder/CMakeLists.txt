idf_component_register(SRCS "app_recorder.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_manager
                                service_i18n
                                service_nvs
                                service_sd
                                service_wavrec
                                fatfs
                       PRIV_REQUIRES esp_lvgl_port
                                     engine_gui)
