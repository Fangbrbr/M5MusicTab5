idf_component_register(SRCS "service_usb_device.c"
                       INCLUDE_DIRS "include"
                       REQUIRES engine_midi board_config
                       PRIV_REQUIRES espressif__esp_tinyusb sdmmc fatfs)
