idf_component_register(SRCS "task_input.c"
                       INCLUDE_DIRS "include"
                       REQUIRES service_input
                                 engine_gui
                                 app_manager)
