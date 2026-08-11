/*
 * SPDX-FileCopyrightText: 2014-2023 Korneliusz Jarzębski
 *
 * SPDX-License-Identifier: MIT
 *
 * 由 M5Stack Tab5 UserDemo 的 C++ 驱动移植为 C，仅保留本项目所需接口。
 */

#ifndef INA226_H
#define INA226_H

#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INA226_ADDRESS 0x40

#define INA226_REG_CONFIG      0x00
#define INA226_REG_SHUNTVOLTAGE 0x01
#define INA226_REG_BUSVOLTAGE  0x02
#define INA226_REG_POWER       0x03
#define INA226_REG_CURRENT     0x04
#define INA226_REG_CALIBRATION 0x05
#define INA226_REG_MASKENABLE  0x06
#define INA226_REG_ALERTLIMIT  0x07

typedef enum {
    INA226_AVERAGES_1    = 0b000,
    INA226_AVERAGES_4    = 0b001,
    INA226_AVERAGES_16   = 0b010,
    INA226_AVERAGES_64   = 0b011,
    INA226_AVERAGES_128  = 0b100,
    INA226_AVERAGES_256  = 0b101,
    INA226_AVERAGES_512  = 0b110,
    INA226_AVERAGES_1024 = 0b111
} ina226_averages_t;

typedef enum {
    INA226_BUS_CONV_TIME_140US  = 0b000,
    INA226_BUS_CONV_TIME_204US  = 0b001,
    INA226_BUS_CONV_TIME_332US  = 0b010,
    INA226_BUS_CONV_TIME_588US  = 0b011,
    INA226_BUS_CONV_TIME_1100US = 0b100,
    INA226_BUS_CONV_TIME_2116US = 0b101,
    INA226_BUS_CONV_TIME_4156US = 0b110,
    INA226_BUS_CONV_TIME_8244US = 0b111
} ina226_bus_conv_time_t;

typedef enum {
    INA226_SHUNT_CONV_TIME_140US  = 0b000,
    INA226_SHUNT_CONV_TIME_204US  = 0b001,
    INA226_SHUNT_CONV_TIME_332US  = 0b010,
    INA226_SHUNT_CONV_TIME_588US  = 0b011,
    INA226_SHUNT_CONV_TIME_1100US = 0b100,
    INA226_SHUNT_CONV_TIME_2116US = 0b101,
    INA226_SHUNT_CONV_TIME_4156US = 0b110,
    INA226_SHUNT_CONV_TIME_8244US = 0b111
} ina226_shunt_conv_time_t;

typedef enum {
    INA226_MODE_POWER_DOWN     = 0b000,
    INA226_MODE_SHUNT_TRIG     = 0b001,
    INA226_MODE_BUS_TRIG       = 0b010,
    INA226_MODE_SHUNT_BUS_TRIG = 0b011,
    INA226_MODE_ADC_OFF        = 0b100,
    INA226_MODE_SHUNT_CONT     = 0b101,
    INA226_MODE_BUS_CONT       = 0b110,
    INA226_MODE_SHUNT_BUS_CONT = 0b111,
} ina226_mode_t;

esp_err_t ina226_init(i2c_master_bus_handle_t bus_handle, uint8_t address,
                      float r_shunt_ohm, float max_current_a);

esp_err_t ina226_configure(ina226_averages_t avg,
                           ina226_bus_conv_time_t bus_conv_time,
                           ina226_shunt_conv_time_t shunt_conv_time,
                           ina226_mode_t mode);

float ina226_read_bus_voltage(void);
float ina226_read_shunt_voltage(void);
float ina226_read_current(void);
float ina226_read_power(void);

#ifdef __cplusplus
}
#endif

#endif /* INA226_H */
