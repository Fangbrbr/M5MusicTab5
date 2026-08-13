#ifndef SERVICE_I2C_H
#define SERVICE_I2C_H

#include "esp_err.h"
#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 I2C 总线全局互斥锁
 *
 * 必须在 bsp_i2c_init() 之后、任何其他 I2C 访问之前调用。
 */
esp_err_t service_i2c_init(void);

/**
 * @brief 获取 I2C 总线锁
 *
 * 递归互斥锁，所有通过 BSP I2C 总线访问外设（IO 扩展器、RTC、触摸等）
 * 的代码，在执行多字节事务或关键 I2C 操作前应调用本函数。
 *
 * @return true 成功获取，false 锁未初始化
 */
bool service_i2c_take(void);

/**
 * @brief 释放 I2C 总线锁
 */
void service_i2c_give(void);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_I2C_H */
