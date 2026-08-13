# Trap: IDF 早期组件依赖展开（component_get_requirements）阶段 Kconfig 未加载，
# CONFIG_BOARD_HAS_RTC 不可见；用板型符号反向判断（默认/未知一律按 Tab5 保留
# rx8130ce，仅 JC 板剔除），与 board_config/CMakeLists.txt 同款写法。
set(SERVICE_RTC_REQUIRES board_config
                         service_i2c)
if(NOT CONFIG_BOARD_TYPE_JC4880P443)
    list(APPEND SERVICE_RTC_REQUIRES espp__rx8130ce)
endif()

idf_component_register(SRCS "service_rtc.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES ${SERVICE_RTC_REQUIRES})
