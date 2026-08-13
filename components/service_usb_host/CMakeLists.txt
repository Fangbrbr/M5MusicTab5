idf_component_register(SRCS "service_usb_host.c"
                       INCLUDE_DIRS "include"
                       REQUIRES engine_midi board_config
                       PRIV_REQUIRES usb usb_host_hid)
