idf_component_register(SRCS "service_usb_device.c"
                       INCLUDE_DIRS "include"
                       REQUIRES engine_midi espressif__m5stack_tab5
                       PRIV_REQUIRES espressif__esp_tinyusb sdmmc fatfs)
