/**
 * @file app_sound_garden.h
 * @brief 禅模式 (Zen Mode) App 注册接口
 */

#ifndef APP_ZEN_H
#define APP_ZEN_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册禅模式 App 到 AppManager
 */
esp_err_t app_zen_register(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_ZEN_H */
