idf_component_register(SRCS "service_usb_host.c"
                       INCLUDE_DIRS "include"
                       REQUIRES engine_midi espressif__m5stack_tab5
                       PRIV_REQUIRES usb usb_host_hid)
