idf_component_register(SRCS "app_fun.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_manager
                                engine_gui
                                service_nvs
                                service_i18n
                                bmi270
                                espressif__m5stack_tab5
                                espressif__esp_lcd_touch_st7123)
