/**
 * @file task_audio.c
 * @brief L3 Task：音频任务
 */

#include "task_audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "service_audio.h"
#include "service_voice.h"
#include "esp_log.h"
#include "esp_timer.h"

/* 栈必须内部 RAM（音频实时路径+可能在 flash 写窗口内运行，PSRAM 栈不可达）。
 * Trap: Opus 编码调用链极深（celt_encode_with_ec → compute_mdcts →
 * clt_mdct_forward_c），xiaozhi 对话开启编码后实测 16KB 栈溢出 ~3KB
 * （Stack protection fault），取 24KB。 */
#define TASK_AUDIO_STACK_SIZE 24576
#define TASK_AUDIO_PRIORITY   (configMAX_PRIORITIES - 1)
#define TASK_AUDIO_CORE       1

static void task_audio_entry(void *arg);

void task_audio_start(void)
{
    BaseType_t ret;
    ret = xTaskCreatePinnedToCore(task_audio_entry, "task_audio",
                            TASK_AUDIO_STACK_SIZE, NULL,
                            TASK_AUDIO_PRIORITY, NULL,
                            TASK_AUDIO_CORE);
    if (ret != pdPASS) {
        ESP_LOGE("task_audio", "Failed to create task_audio, ret:%d", ret);
    }   
}

static void task_audio_entry(void *arg)
{
    (void)arg;

    /* 临时遥测：循环周期数与单次最长耗时，3s 窗口；实时渲染应 ≈2067 周期/3s
     * （64 帧/周期 @44.1k），max 飙升即渲染被阻塞（TTS 卡顿根因定位用） */
    uint32_t dbg_cycles = 0;
    uint32_t dbg_max_us = 0;
    int64_t dbg_window_us = esp_timer_get_time();

    while (1) {
        int64_t t0 = esp_timer_get_time();
        service_audio_process();
        service_voice_process();
        int64_t dt = esp_timer_get_time() - t0;
        dbg_cycles++;
        if (dt > (int64_t)dbg_max_us) {
            dbg_max_us = (uint32_t)dt;
        }
        if ((esp_timer_get_time() - dbg_window_us) >= 3000000) {
            ESP_LOGD("task_audio", "[dbg] loop/3s cycles=%lu max=%lu us",
                     (unsigned long)dbg_cycles, (unsigned long)dbg_max_us);
            dbg_cycles = 0;
            dbg_max_us = 0;
            dbg_window_us = esp_timer_get_time();
        }
    }
}
