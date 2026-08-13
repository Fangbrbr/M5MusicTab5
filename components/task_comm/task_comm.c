/**
 * @file task_comm.c
 * @brief L3 Task：通信任务
 */

#include "task_comm.h"
#include "engine_midi.h"
#include "service_usb_host.h"
#include "service_ws.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "task_comm";

#define TASK_COMM_PERIOD_MS  10
/* 栈须容纳 TTS Opus 解码全链：service_ws_process 在本任务上下文分发下行
 * 音频 → service_voice_opus_decode_to_aux → opus_decode（celt_synthesis
 * 等大栈帧 + 调用链 ~10KB）。旧值 8192 在真机解码真实 CELT 包时栈溢出
 * （Stack protection fault 崩溃）；栈落 PSRAM，加量不占内部 RAM */
#define TASK_COMM_STACK_SIZE 16384
#define TASK_COMM_PRIORITY   4
#define TASK_COMM_CORE       0

static void task_comm_entry(void *arg);

/* 栈落 PSRAM：本任务无 SPI flash 写路径，同 task_gui 注释。 */
static StackType_t *s_task_comm_stack = NULL;
static StaticTask_t s_task_comm_tcb;

void task_comm_start(void)
{
    if (s_task_comm_stack == NULL) {
        s_task_comm_stack = heap_caps_malloc(TASK_COMM_STACK_SIZE,
                                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (s_task_comm_stack != NULL) {
        xTaskCreateStaticPinnedToCore(task_comm_entry, "task_comm",
                                      TASK_COMM_STACK_SIZE, NULL,
                                      TASK_COMM_PRIORITY,
                                      s_task_comm_stack, &s_task_comm_tcb,
                                      TASK_COMM_CORE);
        return;
    }
    ESP_LOGW(TAG, "psram stack alloc failed, fallback to internal");
    xTaskCreatePinnedToCore(task_comm_entry, "task_comm",
                            TASK_COMM_STACK_SIZE, NULL,
                            TASK_COMM_PRIORITY, NULL,
                            TASK_COMM_CORE);
}

static void task_comm_entry(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        (void)TAG;
        service_usb_host_process();  /* USB Host MIDI/HID 事件非阻塞处理 */
        engine_midi_process();
        service_ws_process();          /* WebSocket 事件出队并回调（非阻塞） */
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TASK_COMM_PERIOD_MS));
    }
}
