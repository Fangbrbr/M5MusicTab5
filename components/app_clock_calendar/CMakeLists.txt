idf_component_register(
    SRCS "app_clock_calendar.c"
         "cnlunar/src/cnlunar.c"
         "cnlunar/src/cnlunar_rules.c"
         "cnlunar/src/cnlunar_rules_tables.c"
         "cnlunar/src/cnlunar_level.c"
         "cnlunar/src/cnlunar_data.c"
    INCLUDE_DIRS "include" "cnlunar/include"
    PRIV_INCLUDE_DIRS "cnlunar/src"
    REQUIRES app_manager engine_gui service_nvs service_wifi service_http_client esp_timer lvgl__lvgl espressif__cjson
)
