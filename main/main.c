/**
 * @file main.c
 * @brief 应用入口
 */

#include "app_manager.h"
#include "engine_sf2.h"
#include "engine_gui.h"
#include "engine_midi.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "service_audio.h"
#include "service_page.h"
#include "service_http_client.h"
#include "service_i18n.h"
#include "service_input.h"
#include "service_nvs.h"
#include "service_power.h"
#include "service_rtc.h"
#include "service_recorder.h"
#include "service_ftp.h"
#include "service_sd.h"
#include "service_timer.h"
#include "service_usb_host.h"
#include "service_voice.h"
#include "service_wavrec.h"
#include "service_wifi.h"
#include "service_ws.h"
#include "task_app.h"
#include "task_audio.h"
#include "task_comm.h"
#include "task_gui.h"
#include "task_input.h"
#include "task_ai.h"

static const char *TAG = "main";

#define BOOT_PROGRESS_10    10
#define BOOT_PROGRESS_20    20
#define BOOT_PROGRESS_30    30
#define BOOT_PROGRESS_40    40
#define BOOT_PROGRESS_50    50
#define BOOT_PROGRESS_60    60
#define BOOT_PROGRESS_70    70
#define BOOT_PROGRESS_80    80
#define BOOT_PROGRESS_90    90
#define BOOT_PROGRESS_100   100

static inline void main_boot_progress(int percent)
{
    engine_gui_set_boot_percent(percent);
}

/* SF2 默认音色在 activate 阶段同步加载，进度回调把 0~100% 映射到 boot 50~59 */
static void main_sf2_progress(int percent, void *user_data)
{
    (void)user_data;
    main_boot_progress(BOOT_PROGRESS_50 +
                       percent * (BOOT_PROGRESS_60 - 1 - BOOT_PROGRESS_50) / 100);

    /* Trap: 无 SD 卡时 SF2 从 SPIFFS 加载可超 10s，main 独占 CPU0 饿死 IDLE0
     * 触发 task_wdt，周期性让出 CPU 喂狗 */
    static int64_t s_last_yield_us = 0;
    int64_t now = esp_timer_get_time();
    if (now - s_last_yield_us > 100000) {
        s_last_yield_us = now;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    /* 电池计量/耳机检测失败不影响核心功能，降级 */
    ESP_ERROR_CHECK_WITHOUT_ABORT(service_power_init());
    main_boot_progress(BOOT_PROGRESS_10);

    ESP_ERROR_CHECK(engine_midi_init());

    ESP_ERROR_CHECK(app_manager_init());
    ESP_ERROR_CHECK(app_manager_register_all());

    ESP_ERROR_CHECK_WITHOUT_ABORT(service_sd_init());
    main_boot_progress(BOOT_PROGRESS_20);

    service_i18n_init();
    service_recorder_init();
    service_wavrec_init();
    service_timer_init();
    /* FTP 仅初始化状态，socket/缓冲等进入 FTP 页才分配；失败降级不影响开机 */
    if (service_ftp_init() != ESP_OK) {
        ESP_LOGW(TAG, "service_ftp_init failed, ftp disabled");
    }
    /* 必须早于 WiFi/AFE：任务栈落内部 RAM，AFE/WiFi 启动后枯竭会分配失败；
     * USB MIDI 键盘仅为输入路径之一，失败降级 */
    ESP_ERROR_CHECK_WITHOUT_ABORT(service_usb_host_init());

    ESP_ERROR_CHECK(engine_gui_init());

    service_page_init();

    /* 后续初始化可能阻塞数秒，把 main 从 WDT 移除 */
    if (esp_task_wdt_status(NULL) == ESP_OK) {
        esp_task_wdt_delete(NULL);
    }

    main_boot_progress(BOOT_PROGRESS_30);

    task_gui_start();
    task_comm_start();
    task_app_start();
    main_boot_progress(BOOT_PROGRESS_40);

    task_input_start();

    ESP_ERROR_CHECK(service_audio_init());
    main_boot_progress(BOOT_PROGRESS_50);

    /* 必须在 AFE 打开前创建：AFE 打开后内部 RAM 碎片化，建任务必失败 */
    task_audio_start();

    /* 末尾内部打开 AFE；AFE 不可用时退化 manual 按住说话，不阻断开机 */
    ESP_ERROR_CHECK_WITHOUT_ABORT(service_voice_init());

    /* 网络为可选增值功能，离线音乐必须可用，失败降级。
     * 位置：SF2 大音源加载之前——SDIO DMA buffer 需内部 RAM，SF2 元数据会耗尽它。
     * hosted 链路由 esp_wifi_init 内部自行建立（reconfigure→card init→等有界超时），
     * 调用前链路必然未就绪，属正常；探测式跳过曾致 WiFi 全灭（2026-08，已拆）。 */
    ESP_ERROR_CHECK_WITHOUT_ABORT(service_wifi_boot());

    /* 音色加载失败时“开机但静默”优于重启循环（存储损坏时 abort 死循环）。
     * Trap: 无卡时 SPIFFS 加载 10s+，main 独占 CPU0 饿死 IDLE0 触发 task_wdt；
     * 进度回调内的让出不足以救回 IDLE0，用 WDT relax 临时把超时提到 30s */
    /* 音源选择（设置页持久化）：NVS 在 service_power_init 阶段已就绪；
     * 空串 = 内部预设，SD 文件加载失败由引擎内部回退内部预设 */
    char sf2_source[SERVICE_NVS_SF2_SOURCE_MAX_LEN];
    if (service_nvs_get_sf2_source(sf2_source, sizeof(sf2_source)) == ESP_OK) {
        engine_sf2_set_boot_source(sf2_source);
    }
    engine_sf2_set_progress_callback(main_sf2_progress, NULL);
    engine_gui_wdt_relax();
    ESP_ERROR_CHECK_WITHOUT_ABORT(engine_sf2_register_source());
    ESP_ERROR_CHECK_WITHOUT_ABORT(service_audio_activate_sf2());
    engine_gui_wdt_restore();

    /* 诊断：SF2 大音源加载后的内部/DMA RAM 余量——定位 SD 读卡 DMA buffer 是否够用 */
    ESP_LOGI(TAG, "heap_diag sf2_loaded internal=%u psram=%u dma=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA));

    main_boot_progress(BOOT_PROGRESS_60);

    /* RTC 失败仅影响时钟类 App，降级 */
    ESP_ERROR_CHECK_WITHOUT_ABORT(service_rtc_init());
    main_boot_progress(BOOT_PROGRESS_70);

    ESP_ERROR_CHECK_WITHOUT_ABORT(service_http_client_init());

    /* 初始化通用 WebSocket 组件（幂等）：xiaozhi 等上层协议在 task_comm 周期驱动。 */
    ESP_ERROR_CHECK_WITHOUT_ABORT(service_ws_init());

    /* 启动 AI 常驻任务：service_xiaozhi 的激活/语音通道/mic 泵均在其中闭环。
     * 原 task_network（旧 AI 方案的 HTTP worker）已由其替代，不新增任务数量。 */
    task_ai_start();

    main_boot_progress(BOOT_PROGRESS_80);

    /* 触摸是唯一 UI 操作途径，失败即不可用 */
    ESP_ERROR_CHECK(service_input_init());
    main_boot_progress(BOOT_PROGRESS_90);

    /* 系统完全就绪后再启用自动熄屏/自动休眠计时，避免 boot 期间误触发 */
    service_power_idle_set_enabled(true);

    /* 版本号与关于页同源：构建期注入的 FIRMWARE_VERSION 宏（tag 或 7 位 commit id） */
    ESP_LOGI(TAG, "firmware version: %s", FIRMWARE_VERSION);

    ESP_LOGI(TAG, "system boot complete, entering idle loop");

    /* Contract: engine_gui 仅在 percent>=100 时切 boot 屏，必须打到 100 */
    main_boot_progress(BOOT_PROGRESS_100);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        sys_monitor_tick++;
    }
}
