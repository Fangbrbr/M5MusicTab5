# Trap: IDF 早期组件依赖展开阶段 Kconfig 未加载，CONFIG_BOARD_TYPE_* 不可见，
# 因此 REQUIRES 必须是无条件超集（两板依赖全列，未用者不进链接）；
# 只有 SRCS 允许按板条件选择（真实 configure 阶段 Kconfig 已就绪）。
if(CONFIG_BOARD_TYPE_JC4880P443)
    set(BOARD_SRCS "boards/jc4880p443/board_jc4880p443_display.c"
                   "boards/jc4880p443/board_jc4880p443_touch.c"
                   "boards/jc4880p443/board_jc4880p443_audio.c")
else()
    set(BOARD_SRCS "boards/tab5/board_tab5_display.c"
                   "boards/tab5/board_tab5_touch.c"
                   "boards/tab5/board_tab5_audio.c")
endif()

idf_component_register(
    SRCS ${BOARD_SRCS}
    INCLUDE_DIRS "include"
    # m5stack_tab5 为 PUBLIC：Tab5 专属代码（service_power 门控段等）经此传递获得 BSP 头文件
    REQUIRES lvgl esp_lcd driver
             espressif__m5stack_tab5
             espressif__esp_codec_dev
             espressif__esp_lcd_touch
             espressif__esp_lcd_touch_gt911
             espressif__esp_lcd_st7701
             espressif__esp_lvgl_port
    PRIV_REQUIRES esp_driver_ledc esp_driver_i2c esp_driver_gpio esp_hw_support
)
