idf_component_register(SRCS "service_wifi.c"
                                    "service_wifi_ap.c"
                       INCLUDE_DIRS "include"
                       REQUIRES service_rtc
                                service_nvs
                                service_i18n
                                espressif__esp_wifi_remote
                                esp_netif
                                nvs_flash
                                esp_timer
                                lwip
                                esp_wifi
                                esp_http_server)
