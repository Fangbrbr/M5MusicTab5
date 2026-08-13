idf_component_register(SRCS "app_ai_agent.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_manager
                                engine_gui
                                service_xiaozhi
                                service_nvs
                                service_sd
                                service_audio
                                service_wifi
                                service_power
                                board_config
                                espressif__esp_lvgl_port)

# app_ai_agent → service_xiaozhi → engine_gui → app_manager → app_ai_agent
# 构成静态库循环依赖，CMake 默认重复扫描次数不足以按序解析，提高到 4
set_property(TARGET ${COMPONENT_LIB} PROPERTY LINK_INTERFACE_MULTIPLICITY 4)
