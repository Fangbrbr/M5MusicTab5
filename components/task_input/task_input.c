/**
 * @file task_input.c
 * @brief L3 Task：输入任务
 */

#include "task_input.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "service_input.h"
#include "engine_gui.h"
#include "app_manager.h"
#include "service_power.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "task_input";

#define TASK_INPUT_PERIOD_MS  10
#define TASK_INPUT_STACK_SIZE 8192
#define TASK_INPUT_PRIORITY   7
#define TASK_INPUT_CORE       0

static void task_input_entry(void *arg);

/* 栈落 PSRAM：本任务无 SPI flash 写路径，同 task_gui 注释。 */
static StackType_t *s_task_input_stack = NULL;
static StaticTask_t s_task_input_tcb;

void task_input_start(void)
{
    if (s_task_input_stack == NULL) {
        s_task_input_stack = heap_caps_malloc(TASK_INPUT_STACK_SIZE,
                                              MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (s_task_input_stack != NULL) {
        xTaskCreateStaticPinnedToCore(task_input_entry, "task_input",
                                      TASK_INPUT_STACK_SIZE, NULL,
                                      TASK_INPUT_PRIORITY,
                                      s_task_input_stack, &s_task_input_tcb,
                                      TASK_INPUT_CORE);
        return;
    }
    ESP_LOGW(TAG, "psram stack alloc failed, fallback to internal");
    xTaskCreatePinnedToCore(task_input_entry, "task_input",
                            TASK_INPUT_STACK_SIZE, NULL,
                            TASK_INPUT_PRIORITY, NULL,
                            TASK_INPUT_CORE);
}

static void task_input_entry(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        (void)TAG;
        service_input_process();  /* UART MIDI 等输入源轮询 */

        /* 从 engine_gui 触摸缓存队列消费事件并分发给当前 App */
        app_input_event_t evt;
        while (engine_gui_get_touch_event(&evt)) {
            service_power_idle_reset();
            app_manager_feed_input(&evt);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TASK_INPUT_PERIOD_MS));
    }
}
