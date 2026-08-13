idf_component_register(SRCS "service_input.c"
                              "service_input_uart.c"
                       INCLUDE_DIRS "include"
                       REQUIRES board_config
                                engine_midi
                                driver
                                service_usb_host
                                service_usb_device
                                service_ble)
