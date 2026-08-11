/**
 * @file task_ai.c
 * @brief L3 Task：AI 语音任务
 */

#include "task_ai.h"
#include "service_xiaozhi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "task_ai";

/* 栈必须内部 RAM：激活路径 xz_save_channel_config → service_nvs_commit
 * 在本任务上下文写 SPI flash，写窗口内禁用外部存储 cache，PSRAM 栈在
 * 窗口内不可达必崩。
 * 栈深 24KB：协议处理调用链（cJSON/MCP/OTA）实测 12KB 不足，留余量。
 * Trap: 本平台 RISC-V 移植 StackType_t=uint8_t，深度参数单位即字节，
 * 数组有几项就传几，不存在 4 倍 word 换算。 */
#define TASK_AI_STACK_SIZE 24576
#define TASK_AI_PRIORITY   5
#define TASK_AI_CORE       0

static void task_ai_entry(void *arg);

/* Why 静态而非 xTaskCreate：IDF v5.4.4 中 xTaskCreate 动态栈恒落内部 RAM，
 * CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY 仅放宽 xTaskCreateStatic/
 * WithCaps 接受 PSRAM 缓冲；静态分配是自文档化的位置保证。 */
static StackType_t s_task_ai_stack[TASK_AI_STACK_SIZE / sizeof(StackType_t)];
static StaticTask_t s_task_ai_tcb;

void task_ai_start(void)
{
    if (service_xiaozhi_init() != ESP_OK) {
        ESP_LOGE(TAG, "xiaozhi init failed");
        return;
    }

    xTaskCreateStaticPinnedToCore(task_ai_entry, "task_ai",
                                  TASK_AI_STACK_SIZE / sizeof(StackType_t), NULL,
                                  TASK_AI_PRIORITY,
                                  s_task_ai_stack, &s_task_ai_tcb,
                                  TASK_AI_CORE);
}

static void task_ai_entry(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "task started");

    for (;;) {
        service_xiaozhi_process();
    }
}
