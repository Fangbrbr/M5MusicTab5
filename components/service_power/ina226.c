/*
 * SPDX-FileCopyrightText: 2014-2023 Korneliusz Jarzębski
 *
 * SPDX-License-Identifier: MIT
 *
 * 由 M5Stack Tab5 UserDemo 的 C++ 驱动移植为 C，接口与调用顺序严格与 UserDemo 保持一致。
 */

#include "ina226.h"
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"

#define I2C_MASTER_TIMEOUT_MS 50

static i2c_master_dev_handle_t s_i2c_dev = NULL;
static float s_current_lsb = 0.0f;
static float s_power_lsb   = 0.0f;
static float s_r_shunt     = 0.005f;

static esp_err_t ina226_write_register(uint8_t reg, uint16_t val)
{
    if (s_i2c_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t w_buffer[3] = {reg, (uint8_t)(val >> 8), (uint8_t)(val & 0xFF)};
    return i2c_master_transmit(s_i2c_dev, w_buffer, sizeof(w_buffer), I2C_MASTER_TIMEOUT_MS);
}

static esp_err_t ina226_read_register(uint8_t reg, int16_t *val)
{
    if (s_i2c_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t r_buffer[2] = {0};
    esp_err_t ret = i2c_master_transmit_receive(s_i2c_dev, &reg, 1, r_buffer, 2, I2C_MASTER_TIMEOUT_MS);
    if (ret == ESP_OK) {
        *val = (int16_t)((r_buffer[0] << 8) | r_buffer[1]);
    }
    return ret;
}

esp_err_t ina226_init(i2c_master_bus_handle_t bus_handle, uint8_t address,
                      float r_shunt_ohm, float max_current_a)
{
    if (bus_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_i2c_dev != NULL) {
        i2c_master_bus_rm_device(s_i2c_dev);
        s_i2c_dev = NULL;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = address,
        .scl_speed_hz    = 400000,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &s_i2c_dev);
    if (ret != ESP_OK) {
        return ret;
    }

    /* 与 UserDemo 的 calibrate() 一致：仅计算 LSB，不实际写寄存器 */
    s_r_shunt = r_shunt_ohm;

    float minimum_lsb = max_current_a / 32767.0f;
    s_current_lsb     = (float)ceil(minimum_lsb / 0.0001f) * 0.0001f;
    s_power_lsb       = s_current_lsb * 25.0f;

    return ESP_OK;
}

esp_err_t ina226_configure(ina226_averages_t avg,
                           ina226_bus_conv_time_t bus_conv_time,
                           ina226_shunt_conv_time_t shunt_conv_time,
                           ina226_mode_t mode)
{
    /* 与 UserDemo 的 configure() 一致：写配置寄存器 */
    uint16_t config = (avg << 9) | (bus_conv_time << 6) | (shunt_conv_time << 3) | mode;
    esp_err_t ret = ina226_write_register(INA226_REG_CONFIG, config);
    if (ret != ESP_OK) {
        return ret;
    }

    /* 与 UserDemo 的 calibrate() 一致：写校准寄存器 */
    uint16_t calibration = 1;
    if (s_current_lsb > 0.0f && s_r_shunt > 0.0f) {
        calibration = (uint16_t)roundf(0.00512f / (s_current_lsb * s_r_shunt));
    }
    if (calibration < 1) {
        calibration = 1;
    }

    return ina226_write_register(INA226_REG_CALIBRATION, calibration);
}

float ina226_read_bus_voltage(void)
{
    int16_t voltage = 0;
    if (ina226_read_register(INA226_REG_BUSVOLTAGE, &voltage) != ESP_OK) {
        return 0.0f;
    }
    return (float)voltage * 0.00125f;
}

float ina226_read_shunt_voltage(void)
{
    int16_t voltage = 0;
    if (ina226_read_register(INA226_REG_SHUNTVOLTAGE, &voltage) != ESP_OK) {
        return 0.0f;
    }
    return (float)voltage * 0.0000025f;
}

float ina226_read_current(void)
{
    int16_t current = 0;
    if (ina226_read_register(INA226_REG_CURRENT, &current) != ESP_OK) {
        return 0.0f;
    }
    return (float)current * s_current_lsb;
}

float ina226_read_power(void)
{
    int16_t power = 0;
    if (ina226_read_register(INA226_REG_POWER, &power) != ESP_OK) {
        return 0.0f;
    }
    return (float)power * s_power_lsb;
}
