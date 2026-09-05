/**
 * @file service_timer.h
 * @brief 周期性时基服务：薄封装 esp_timer 供 App 定时派发
 *
 * 音序器/节拍器等需要精确节拍的应用不再各自管理 esp_timer 句柄与
 * 周期换算，统一经本服务注册周期 hook。
 *
 * Contract:
 * - hook 运行在 esp_timer 任务上下文（高优先级、非 ISR），
 *   禁止阻塞、禁止触碰 LVGL、禁止获取可能被阻塞的锁。
 * - hook 内可安全调用 engine_midi_publish(..., 0)。
 * - set_period 与 unregister 可在任意上下文调用（esp_timer 内部同步）。
 */

#ifndef SERVICE_TIMER_H
#define SERVICE_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 周期 hook 类型 */
typedef void (*service_timer_hook_t)(void *arg);

/** @brief 定时器句柄（不透明） */
typedef struct service_timer *service_timer_handle_t;

/**
 * @brief 初始化时基服务（幂等）
 */
esp_err_t service_timer_init(void);

/**
 * @brief 注册一个周期 hook
 * @param period_us 周期（微秒）
 * @param hook      周期回调（见文件头 Contract）
 * @param arg       透传给 hook 的用户数据
 * @param[out] out  返回句柄；失败为 NULL
 */
esp_err_t service_timer_periodic_register(uint64_t period_us,
                                          service_timer_hook_t hook,
                                          void *arg,
                                          service_timer_handle_t *out);

/**
 * @brief 运行时修改周期（微秒）；周期为 0 等价暂停
 */
esp_err_t service_timer_set_period(service_timer_handle_t timer, uint64_t period_us);

/**
 * @brief 注销定时器并释放句柄
 */
esp_err_t service_timer_unregister(service_timer_handle_t timer);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_TIMER_H */
