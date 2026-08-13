/**
 * @file board_jc4880p443_display.c
 * @brief Guition JC4880P443 显示 / I2C / 背光板级实现
 *
 * 4.3" 480x800 竖屏，ST7701 驱动 IC，MIPI-DSI 2 lane；
 * 背光 GPIO23 LEDC PWM；触摸/codec 共享 I2C_NUM_1。
 * 参数来源：厂商板级包 临时/guition-jc4880p443（config.h / jc4880p443.cc）。
 */

#include "board_hal.h"
#include "board_config.h"

#include "driver/ledc.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7701.h"
#include "esp_log.h"

static const char *TAG = "board_jc_display";

#define JC_I2C_PORT                 I2C_NUM_1
#define JC_I2C_SDA_GPIO             GPIO_NUM_7
#define JC_I2C_SCL_GPIO             GPIO_NUM_8

#define JC_LCD_RST_GPIO             GPIO_NUM_5
#define JC_LCD_BACKLIGHT_GPIO       GPIO_NUM_23
#define JC_DSI_LANE_NUM             2
/* Trap: 500 为厂商默认值；若烧录后屏一直不亮且串口报 IDLE0 task_wdt
 * （DSI 初始化死等面板应答），按厂商 README 依次试 550 / 750 / 900。 */
#define JC_DSI_LANE_BIT_RATE_MBPS   500
#define JC_DSI_PHY_LDO_CHAN         3
#define JC_DSI_PHY_LDO_VOLTAGE_MV   2500
#define JC_DSI_DPI_CLOCK_MHZ        34

#define JC_BACKLIGHT_LEDC_TIMER     LEDC_TIMER_2
#define JC_BACKLIGHT_LEDC_CHANNEL   LEDC_CHANNEL_2
#define JC_BACKLIGHT_LEDC_FREQ_HZ   20000
#define JC_BACKLIGHT_LEDC_DUTY_BITS LEDC_TIMER_10_BIT
#define JC_BACKLIGHT_DUTY_MAX       ((1 << JC_BACKLIGHT_LEDC_DUTY_BITS) - 1)

/* ST7701 厂商初始化命令表，随面板玻璃批次绑定，禁止凭经验增删 */
static const st7701_lcd_init_cmd_t s_lcd_init_cmds[] = {
    {0xFF, (uint8_t []){0x77,0x01,0x00,0x00,0x13},5,0},
    {0xEF, (uint8_t []){0x08}, 1, 0},
    {0xFF, (uint8_t []){0x77,0x01,0x00,0x00,0x10},5,0},
    {0xC0, (uint8_t []){0x63, 0x00}, 2, 0},
    {0xC1, (uint8_t []){0x0D, 0x02}, 2, 0},
    {0xC2, (uint8_t []){0x10, 0x08}, 2, 0},
    {0xCC, (uint8_t []){0x10}, 1, 0},

    {0xB0, (uint8_t []){0x80, 0x09, 0x53, 0x0C, 0xD0, 0x07, 0x0C, 0x09, 0x09, 0x28, 0x06, 0xD4, 0x13, 0x69, 0x2B, 0x71}, 16, 0},
    {0xB1, (uint8_t []){0x80, 0x94, 0x5A, 0x10, 0xD3, 0x06, 0x0A, 0x08, 0x08, 0x25, 0x03, 0xD3, 0x12, 0x66, 0x6A, 0x0D}, 16, 0},
    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},

    {0xB0, (uint8_t []){0x5D}, 1, 0},
    {0xB1, (uint8_t []){0x58}, 1, 0},
    {0xB2, (uint8_t []){0x87}, 1, 0},
    {0xB3, (uint8_t []){0x80}, 1, 0},
    {0xB5, (uint8_t []){0x4E}, 1, 0},
    {0xB7, (uint8_t []){0x85}, 1, 0},
    {0xB8, (uint8_t []){0x21}, 1, 0},
    {0xB9, (uint8_t []){0x10, 0x1F}, 2, 0},
    {0xBB, (uint8_t []){0x03}, 1,0},
    {0xBC, (uint8_t []){0x00}, 1,0},

    {0xC1, (uint8_t []){0x78}, 1, 0},
    {0xC2, (uint8_t []){0x78}, 1, 0},
    {0xD0, (uint8_t []){0x88}, 1, 0},

    {0xE0, (uint8_t []){0x00, 0x3A, 0x02}, 3, 0},
    {0xE1, (uint8_t []){0x04, 0xA0, 0x00, 0xA0, 0x05,0xA0, 0x00, 0xA0, 0x00, 0x40, 0x40}, 11, 0},
    {0xE2, (uint8_t []){0x30, 0x00, 0x40, 0x40, 0x32, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00}, 13, 0},
    {0xE3, (uint8_t []){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE4, (uint8_t []){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t []){0x09, 0x2E, 0xA0, 0xA0, 0x0B, 0x30, 0xA0, 0xA0, 0x05, 0x2A, 0xA0, 0xA0, 0x07, 0x2C, 0xA0, 0xA0}, 16, 0},
    {0xE6, (uint8_t []){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE7, (uint8_t []){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t []){0x08, 0x2D, 0xA0, 0xA0, 0x0A, 0x2F, 0xA0, 0xA0, 0x04, 0x29, 0xA0, 0xA0, 0x06, 0x2B, 0xA0, 0xA0}, 16, 0},

    {0xEB, (uint8_t []){0x00, 0x00, 0x4E, 0x4E, 0x00, 0x00, 0x00}, 7, 0},
    {0xEC, (uint8_t []){0x08, 0x01}, 2, 0},

    {0xED, (uint8_t []){0xB0, 0x2B, 0x98, 0xA4, 0x56, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xF7, 0x65, 0x4A, 0x89, 0xB2, 0x0B}, 16, 0},
    {0xEF, (uint8_t []){0x08, 0x08, 0x08, 0x45, 0x3F, 0x54}, 6, 0},
    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},

    {0x11, (uint8_t []){0x00}, 1, 120},
    {0x29, (uint8_t []){0x00}, 1, 20},
};

static i2c_master_bus_handle_t s_i2c_bus = NULL;
static esp_ldo_channel_handle_t s_phy_pwr_chan = NULL;
static bool s_backlight_ready = false;

esp_err_t board_i2c_init(void)
{
    if (s_i2c_bus != NULL) {
        return ESP_OK;
    }

    /* Contract: 全系统唯一 I2C 主总线（触摸 GT911 与 codec ES8311 共享） */
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = JC_I2C_PORT,
        .sda_io_num = JC_I2C_SDA_GPIO,
        .scl_io_num = JC_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = 1,
        },
    };
    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &s_i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %d", ret);
        return ret;
    }
    ESP_LOGI(TAG, "I2C bus initialized (port %d, sda %d, scl %d)",
             JC_I2C_PORT, JC_I2C_SDA_GPIO, JC_I2C_SCL_GPIO);
    return ESP_OK;
}

i2c_master_bus_handle_t board_i2c_get_handle(void)
{
    return s_i2c_bus;
}

esp_err_t board_display_create(board_lcd_handles_t *out_handles)
{
    if (out_handles == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret;
    esp_lcd_dsi_bus_handle_t dsi_bus = NULL;
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_handle_t panel = NULL;

    /* Why: DSI PHY 上电后才从 No Power 进入 Shutdown，必须先于 esp_lcd_new_dsi_bus */
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = JC_DSI_PHY_LDO_CHAN,
        .voltage_mv = JC_DSI_PHY_LDO_VOLTAGE_MV,
    };
    ret = esp_ldo_acquire_channel(&ldo_cfg, &s_phy_pwr_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "acquire DSI PHY LDO failed: %d", ret);
        return ret;
    }

    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = JC_DSI_LANE_NUM,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = JC_DSI_LANE_BIT_RATE_MBPS,
    };
    ret = esp_lcd_new_dsi_bus(&bus_config, &dsi_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_dsi_bus failed: %d", ret);
        return ret;
    }

    /* DBI 通道仅用于下发初始化命令 */
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ret = esp_lcd_new_panel_io_dbi(dsi_bus, &dbi_config, &io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_panel_io_dbi failed: %d", ret);
        return ret;
    }

    /* Trap: num_fbs=1 对齐 Tab5 单帧缓冲部分刷新管线；
     * use_dma2d 保持默认 false —— 帧缓冲驻留 PSRAM 时 DMA2D 异步突发与 CPU
     * 旋转并发挤占 PSRAM 总线，大面积刷新必现 DPI underrun 闪屏（项目红线）。 */
    esp_lcd_dpi_panel_config_t dpi_config = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = JC_DSI_DPI_CLOCK_MHZ,
        .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,
        .num_fbs = 1,
        .video_timing = {
            .h_size = BOARD_LCD_H_RES,
            .v_size = BOARD_LCD_V_RES,
            .hsync_pulse_width = 12,
            .hsync_back_porch = 42,
            .hsync_front_porch = 42,
            .vsync_pulse_width = 2,
            .vsync_back_porch = 8,
            .vsync_front_porch = 166,
        },
    };

    st7701_vendor_config_t vendor_config = {
        .init_cmds = s_lcd_init_cmds,
        .init_cmds_size = sizeof(s_lcd_init_cmds) / sizeof(st7701_lcd_init_cmd_t),
        .mipi_config = {
            .dsi_bus = dsi_bus,
            .dpi_config = &dpi_config,
        },
        .flags = {
            .use_mipi_interface = 1,
        },
    };

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = JC_LCD_RST_GPIO,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_config,
    };
    ret = esp_lcd_new_panel_st7701(io, &panel_config, &panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_panel_st7701 failed: %d", ret);
        return ret;
    }

    ret = esp_lcd_panel_reset(panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "panel reset failed: %d", ret);
        return ret;
    }
    ret = esp_lcd_panel_init(panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "panel init failed: %d", ret);
        return ret;
    }

    out_handles->io = io;
    out_handles->panel = panel;
    ESP_LOGI(TAG, "ST7701 panel ready (%dx%d, RGB565)", BOARD_LCD_H_RES, BOARD_LCD_V_RES);
    return ESP_OK;
}

void board_display_warm_reset_power_cycle(void)
{
    /* Why: jc 无面板独立供电控制（无 IO 扩展器电源开关），无法做电源级 POR；
     * 面板 RST 脚已在 board_display_create 内随 esp_lcd_panel_reset 复位，
     * 热复位场景依赖该硬件复位即可。 */
}

esp_err_t board_display_brightness_set(int percent)
{
    if (percent < 0) {
        percent = 0;
    }
    if (percent > 100) {
        percent = 100;
    }

    /* 懒初始化：独立定时器/通道，避免与其他 LEDC 使用方冲突；亮电平=1（不反相） */
    if (!s_backlight_ready) {
        ledc_timer_config_t timer_cfg = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = JC_BACKLIGHT_LEDC_DUTY_BITS,
            .timer_num = JC_BACKLIGHT_LEDC_TIMER,
            .freq_hz = JC_BACKLIGHT_LEDC_FREQ_HZ,
            .clk_cfg = LEDC_AUTO_CLK,
        };
        esp_err_t ret = ledc_timer_config(&timer_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ledc_timer_config failed: %d", ret);
            return ret;
        }

        ledc_channel_config_t channel_cfg = {
            .gpio_num = JC_LCD_BACKLIGHT_GPIO,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = JC_BACKLIGHT_LEDC_CHANNEL,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = JC_BACKLIGHT_LEDC_TIMER,
            .duty = 0,
            .hpoint = 0,
        };
        ret = ledc_channel_config(&channel_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ledc_channel_config failed: %d", ret);
            return ret;
        }
        s_backlight_ready = true;
    }

    uint32_t duty = (JC_BACKLIGHT_DUTY_MAX * (uint32_t)percent) / 100;
    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, JC_BACKLIGHT_LEDC_CHANNEL, duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ledc_set_duty failed: %d", ret);
        return ret;
    }
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, JC_BACKLIGHT_LEDC_CHANNEL);
}

lv_indev_t *board_display_get_default_indev(void)
{
    /* jc 无 BSP，不创建默认 LVGL 输入设备 */
    return NULL;
}
