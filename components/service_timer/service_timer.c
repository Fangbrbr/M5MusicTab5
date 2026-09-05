/**
 * @file service_timer.c
 * @brief 周期性时基服务：薄封装 esp_timer 供 App 定时派发
 */

#include "service_timer.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "service_timer";

struct service_timer {
    esp_timer_handle_t handle;
    service_timer_hook_t hook;
    void *arg;
};

static void service_timer_dispatch(void *arg)
{
    service_timer_handle_t t = (service_timer_handle_t)arg;
    if (t->hook != NULL) {
        t->hook(t->arg);
    }
}

esp_err_t service_timer_init(void)
{
    ESP_LOGI(TAG, "init");
    return ESP_OK;
}

esp_err_t service_timer_periodic_register(uint64_t period_us,
                                          service_timer_hook_t hook,
                                          void *arg,
                                          service_timer_handle_t *out)
{
    if (hook == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    service_timer_handle_t t = (service_timer_handle_t)calloc(1, sizeof(struct service_timer));
    if (t == NULL) {
        return ESP_ERR_NO_MEM;
    }
    t->hook = hook;
    t->arg = arg;

    esp_timer_create_args_t args = {
        .callback = service_timer_dispatch,
        .arg = t,
        .name = "service_timer",
        .dispatch_method = ESP_TIMER_TASK,
    };

    esp_err_t err = esp_timer_create(&args, &t->handle);
    if (err != ESP_OK) {
        free(t);
        return err;
    }

    err = esp_timer_start_periodic(t->handle, period_us);
    if (err != ESP_OK) {
        esp_timer_delete(t->handle);
        free(t);
        return err;
    }

    *out = t;
    return ESP_OK;
}

esp_err_t service_timer_set_period(service_timer_handle_t timer, uint64_t period_us)
{
    if (timer == NULL || timer->handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    /* esp_timer 无 set_period，先停再以新周期重启（内部同步，任意上下文可调） */
    esp_err_t err = esp_timer_stop(timer->handle);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    return esp_timer_start_periodic(timer->handle, period_us);
}

esp_err_t service_timer_unregister(service_timer_handle_t timer)
{
    if (timer == NULL || timer->handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = esp_timer_stop(timer->handle);
    esp_err_t err2 = esp_timer_delete(timer->handle);
    timer->handle = NULL;
    free(timer);
    if (err == ESP_OK) {
        err = err2;
    }
    return err;
}
