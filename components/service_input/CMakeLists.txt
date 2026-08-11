idf_component_register(SRCS "service_input.c"
                              "service_input_uart.c"
                       INCLUDE_DIRS "include"
                       REQUIRES espressif__m5stack_tab5
                                engine_midi
                                driver
                                service_usb_host
                                service_usb_device
                                service_ble)
