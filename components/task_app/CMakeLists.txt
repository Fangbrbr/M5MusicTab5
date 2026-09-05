idf_component_register(SRCS "task_app.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_manager
                                service_power
                                service_rtc
                                service_recorder
                                service_wavrec
                                service_wifi
                                service_nvs
                                service_xiaozhi
                                service_http_client
                                service_page
                                service_input
                                engine_sf2)
