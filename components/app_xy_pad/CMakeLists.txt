idf_component_register(SRCS "app_xy_pad.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_manager
                                service_i18n
                                engine_gui
                                engine_midi)
