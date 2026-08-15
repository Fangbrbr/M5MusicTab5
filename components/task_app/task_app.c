/**
 * @file task_app.c
 * @brief L3 Task：App 调度任务
 */

#include "task_app.h"
#include "app_manager.h"
#include "service_power.h"
#include "service_rtc.h"
#include "service_wifi.h"
#include "service_nvs.h"
#include "service_http_client.h"
#include "service_recorder.h"
#include "service_xiaozhi.h"
#include "service_audio.h"
#include "service_page_onboard.h"
#include "service_page_setting.h"
#include "service_input.h"
#include "engine_sf2.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_heap_task_info.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "task_app";

#define TASK_APP_PERIOD_MS  10
/* 栈余量实测：clock app 的 on_update（HTTP 回调+农历渲染准备）深路径下
 * 8KB 只剩 1484B，加大到 12KB 防溢出 */
#define TASK_APP_STACK_SIZE 12288
#define TASK_APP_PRIORITY   4
#define TASK_APP_CORE       0
#define TASK_APP_NVS_COMMIT_INTERVAL_MS 1000

/* 内存遥测周期（ms）：定期记录内部/PSRAM 空闲与最大连续块、栈高水位，
 * 用于定位内存增长与标定任务栈深 */
#define TASK_APP_MEM_TELEMETRY_INTERVAL_MS 30000

static void task_app_entry(void *arg);

/* 栈强制内部 RAM 静态分配：本任务周期 NVS commit 触发 SPI flash 写，
 * 写窗口内 cache 禁用，PSRAM 栈不可达。
 * Trap: 本平台 StackType_t=uint8_t，深度参数单位即字节 */
static StackType_t s_task_app_stack[TASK_APP_STACK_SIZE / sizeof(StackType_t)];
static StaticTask_t s_task_app_tcb;

void task_app_start(void)
{
    xTaskCreateStaticPinnedToCore(task_app_entry, "task_app",
                                  TASK_APP_STACK_SIZE / sizeof(StackType_t), NULL,
                                  TASK_APP_PRIORITY,
                                  s_task_app_stack, &s_task_app_tcb,
                                  TASK_APP_CORE);
}

/* 观测名单：覆盖 Opus 解码所在 task_comm、协议 task_ai、GUI/音频与网络工人 */
static const char *const s_stack_watch_tasks[] = {
    "task_gui", "task_input", "task_comm", "task_app", "task_audio",
    "task_ai", "http_worker", "wake_fetch", "main",
};

static void task_app_mem_telemetry(void)
{
    ESP_LOGD(TAG, "[mem] internal free=%u largest=%u | psram free=%u largest=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));

    /* SF2 渲染遥测：块预算 1450us（64 帧 @44.1kHz），max 长期超预算即渲染
     * 被总线/算力竞争饿死 */
    engine_sf2_debug_info_t sf2;
    engine_sf2_get_debug_info(&sf2);
    int16_t out_peak = 0;
    uint32_t out_knee = 0;
    service_audio_get_out_stats(&out_peak, &out_knee);
    ESP_LOGD(TAG, "[sf2] voices=%u/%u render avg=%.0fus max=%.0fus peak=%.2f | out peak=%d knee=%u",
             (unsigned)sf2.active_voices, (unsigned)sf2.max_voices,
             (double)sf2.render_block_us_avg, (double)sf2.render_block_us_max,
             (double)sf2.render_peak_amp, (int)out_peak, (unsigned)out_knee);

#ifdef CONFIG_HEAP_TASK_TRACKING
    /* 每任务堆分配归属（按分配时所在任务记账）：内部 RAM 是本项目稀缺资源，
     * 用此定位"内存慢慢被吃掉"的大户。每次调用全量重扫，已删除任务自动消失。 */
    static heap_task_totals_t s_totals[40];
    size_t totals_num = 0;
    heap_task_info_params_t params = {0};
    params.mask[0] = MALLOC_CAP_INTERNAL;
    params.caps[0] = MALLOC_CAP_INTERNAL;
    params.totals = s_totals;
    params.num_totals = &totals_num;
    params.max_totals = 40;
    heap_caps_get_per_task_info(&params);
    for (size_t i = 0; i < totals_num; i++) {
        if (s_totals[i].task != NULL && s_totals[i].size[0] > 0) {
            ESP_LOGI(TAG, "[heap] %-14s internal=%uB blocks=%u",
                     pcTaskGetName(s_totals[i].task),
                     (unsigned)s_totals[i].size[0], (unsigned)s_totals[i].count[0]);
        }
    }

    /* taskLVGL 内部堆持续增长（真机观测 772B→210KB/12min）疑似逐帧泄漏：
     * 增长超阈值时 dump 该任务分配块的 尺寸→块数 直方图（总字节前 8 名），
     * 按块尺寸定位分配者。 */
    static size_t s_prev_lvgl_internal = 0;
    size_t lvgl_now = 0;
    for (size_t i = 0; i < totals_num; i++) {
        if (s_totals[i].task != NULL &&
            strcmp(pcTaskGetName(s_totals[i].task), "taskLVGL") == 0) {
            lvgl_now = s_totals[i].size[0];
            break;
        }
    }
    if (lvgl_now > s_prev_lvgl_internal + 2048) {
        TaskHandle_t lvgl_task = xTaskGetHandle("taskLVGL");
        heap_task_block_t *blocks = heap_caps_malloc(256 * sizeof(heap_task_block_t),
                                                     MALLOC_CAP_SPIRAM);
        if (lvgl_task != NULL && blocks != NULL) {
            heap_task_info_params_t bp = {0};
            TaskHandle_t one = lvgl_task;
            bp.tasks = &one;
            bp.num_tasks = 1;
            bp.blocks = blocks;
            bp.max_blocks = 256;
            size_t got = heap_caps_get_per_task_info(&bp);
            struct { uint32_t size; uint32_t count; bool printed; } hist[16] = {0};
            size_t hist_n = 0;
            for (size_t b = 0; b < got; b++) {
                bool merged = false;
                for (size_t k = 0; k < hist_n; k++) {
                    if (hist[k].size == blocks[b].size) {
                        hist[k].count++;
                        merged = true;
                        break;
                    }
                }
                if (!merged && hist_n < 16) {
                    hist[hist_n].size = blocks[b].size;
                    hist[hist_n].count = 1;
                    hist_n++;
                }
            }
            /* 按 尺寸×块数 降序打印前 8 名 */
            for (int top = 0; top < 8 && top < (int)hist_n; top++) {
                size_t best = 0;
                uint32_t best_bytes = 0;
                for (size_t k = 0; k < hist_n; k++) {
                    if (!hist[k].printed && hist[k].size * hist[k].count > best_bytes) {
                        best_bytes = hist[k].size * hist[k].count;
                        best = k;
                    }
                }
                hist[best].printed = true;
                ESP_LOGW(TAG, "[leak] taskLVGL block %uB x %u = %uB total",
                         (unsigned)hist[best].size, (unsigned)hist[best].count,
                         (unsigned)best_bytes);
            }
            heap_caps_free(blocks);
        }
    }
    s_prev_lvgl_internal = lvgl_now;
#endif

    for (size_t i = 0; i < sizeof(s_stack_watch_tasks) / sizeof(s_stack_watch_tasks[0]); i++) {
        TaskHandle_t h = xTaskGetHandle(s_stack_watch_tasks[i]);
        if (h != NULL) {
            /* 高水位=历史最小剩余栈（字节），长期趋近 0 即栈深不足 */
            ESP_LOGD(TAG, "[stack] %s hwm=%uB", s_stack_watch_tasks[i],
                     (unsigned)uxTaskGetStackHighWaterMark(h));
        }
    }
}

static void task_app_entry(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        (void)TAG;
        /* 先处理 task_comm 等任务异步提交的生命周期请求（启动/销毁 App），
         * 避免在中断/通信任务上下文中直接执行切屏与生命周期回调。 */
        app_manager_process_requests();

        /* 在 App Manager 生命周期锁保护下调用 on_update，避免与 SysEx 触发的
         * App 切换/暂停/销毁并发访问同一个 App 实例。 */
        app_manager_process_active();

        app_manager_process();

        service_power_process();
        service_wifi_process();
        service_rtc_process();
        service_recorder_process();
        service_http_client_process();
        service_page_onboard_process();
        /* SF2 音源切换等设置页挂起请求（秒级加载在此消化，不堵 task_gui） */
        service_page_setting_process();

        /* C6 BLE MIDI 探测延后到后台执行，避免阻塞开机进度；仅执行一次 */
        service_input_late_probe_ble();

        static uint32_t s_nvs_commit_ticks = 0;
        s_nvs_commit_ticks += TASK_APP_PERIOD_MS;
        if (s_nvs_commit_ticks >= TASK_APP_NVS_COMMIT_INTERVAL_MS) {
            s_nvs_commit_ticks = 0;
            service_nvs_commit();
        }

        static uint32_t s_mem_telemetry_ticks = 0;
        s_mem_telemetry_ticks += TASK_APP_PERIOD_MS;
        if (s_mem_telemetry_ticks >= TASK_APP_MEM_TELEMETRY_INTERVAL_MS) {
            s_mem_telemetry_ticks = 0;
            task_app_mem_telemetry();
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TASK_APP_PERIOD_MS));
    }
}
