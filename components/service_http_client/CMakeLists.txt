idf_component_register(SRCS "service_http_client.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_http_client mbedtls service_wifi)
