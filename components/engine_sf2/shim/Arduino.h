/**
 * @file Arduino.h
 * @brief Arduino 基础 API 的最小替代（shim 层）
 *
 * 仅覆盖上游 SF2Sampler 实际使用的符号，通过 include 路径阴影
 * 替换 Arduino 框架头文件，vendor 文件零修改。
 *
 * Trap: min/max/constrain 用模板而非宏实现，避免与 <algorithm> 的
 * std::min/std::max 及 STL 头文件冲突。
 */

#ifndef SHIM_ARDUINO_H
#define SHIM_ARDUINO_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <algorithm>
#include <array>
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "WString.h"

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif

#ifndef TWO_PI
#define TWO_PI 6.283185307179586476925286766559
#endif

static inline uint32_t micros(void)
{
    return (uint32_t)esp_timer_get_time();
}

static inline uint32_t millis(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static inline void delay(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static inline void delayMicroseconds(uint32_t us)
{
    esp_rom_delay_us(us);
}

static inline void yield(void)
{
    vTaskDelay(1);
}

template <class T>
static inline T min(T a, T b)
{
    return (a < b) ? a : b;
}

template <class T>
static inline T max(T a, T b)
{
    return (a > b) ? a : b;
}

template <class T>
static inline T constrain(T x, T lo, T hi)
{
    return (x < lo) ? lo : (x > hi) ? hi : x;
}

#endif /* SHIM_ARDUINO_H */
