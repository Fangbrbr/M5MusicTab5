idf_component_register(
    SRCS "service_power.c"
         "ina226.c"
    INCLUDE_DIRS "include"
    REQUIRES driver
             espressif__m5stack_tab5
             espressif__esp_io_expander
             espressif__esp_lcd_touch
             nvs_flash
             service_rtc
             service_i2c
             service_nvs
)
