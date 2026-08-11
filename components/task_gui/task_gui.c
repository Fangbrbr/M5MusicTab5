/**
 * @file task_gui.c
 * @brief L3 Task：GUI 任务
 */

#include "task_gui.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "engine_gui.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "task_gui";

#define TASK_GUI_PERIOD_MS  10
#define TASK_GUI_STACK_SIZE 8192
#define TASK_GUI_PRIORITY   10
#define TASK_GUI_CORE       0

static void task_gui_entry(void *arg);

/* 栈落 PSRAM：本任务无 SPI flash 写路径（写窗口禁 cache 时 PSRAM 不可达）。
 * IDF 动态栈恒落内部 RAM（pvPortMalloc 恒 INTERNAL），改 Static+PSRAM 缓冲
 * 腾出内部 RAM。本平台 StackType_t=uint8_t，深度单位即字节。 */
static StackType_t *s_task_gui_stack = NULL;
static StaticTask_t s_task_gui_tcb;

void task_gui_start(void)
{
    if (s_task_gui_stack == NULL) {
        s_task_gui_stack = heap_caps_malloc(TASK_GUI_STACK_SIZE,
                                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (s_task_gui_stack != NULL) {
        xTaskCreateStaticPinnedToCore(task_gui_entry, "task_gui",
                                      TASK_GUI_STACK_SIZE, NULL,
                                      TASK_GUI_PRIORITY,
                                      s_task_gui_stack, &s_task_gui_tcb,
                                      TASK_GUI_CORE);
        return;
    }
    ESP_LOGW(TAG, "psram stack alloc failed, fallback to internal");
    xTaskCreatePinnedToCore(task_gui_entry, "task_gui",
                            TASK_GUI_STACK_SIZE, NULL,
                            TASK_GUI_PRIORITY, NULL,
                            TASK_GUI_CORE);
}

static void task_gui_entry(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        (void)TAG;
        engine_gui_tick();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TASK_GUI_PERIOD_MS));
    }
}
