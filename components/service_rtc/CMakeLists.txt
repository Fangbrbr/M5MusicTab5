idf_component_register(SRCS "service_rtc.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES espressif__m5stack_tab5
                                espp__rx8130ce
                                service_i2c)
