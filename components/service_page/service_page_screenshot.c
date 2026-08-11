/**
 * @file service_page_screenshot.c
 * @brief 截图功能（自 engine_gui 解耦）
 *
 * 截取当前 LVGL 屏幕为 BMP（RGB888 → 24bit BGR），保存至 SD 卡 /screenshot。
 */

#include "service_page_screenshot.h"
#include "app_manager.h"
#include "service_sd.h"
#include "service_rtc.h"
#include "service_i18n.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "lvgl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

static const char *TAG = "service_page_screenshot";

typedef struct __attribute__((packed)) {
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
    uint32_t hdr_size;
    int32_t  width;
    int32_t  height;
    uint16_t planes;
    uint16_t bpp;
    uint32_t compression;
    uint32_t img_size;
    int32_t  x_ppm;
    int32_t  y_ppm;
    uint32_t colors;
    uint32_t important;
} bmp_header_t;

bool service_page_take_screenshot(void)
{
    /* 无 SD 卡直接拦截，避免分配内存浪费资源 */
    if (!service_sd_is_mounted()) {
        app_manager_show_notification_timeout(_("无 SD 卡，无法截图"), 2000);
        ESP_LOGW(TAG, "screenshot skipped: no SD card");
        return false;
    }

    /* 确保 /screenshot 目录存在 */
    char dir_path[64];
    snprintf(dir_path, sizeof(dir_path), "%s/screenshot", service_sd_get_mount_point());
    struct stat st;
    if (stat(dir_path, &st) != 0) {
        if (mkdir(dir_path, 0755) != 0) {
            app_manager_show_notification_timeout(_("截图目录创建失败"), 2000);
            ESP_LOGE(TAG, "screenshot mkdir failed: %s", dir_path);
            return false;
        }
    }

    /* 按日期时间生成文件名 */
    struct tm timeinfo = {0};
    if (service_rtc_get_time_cached(&timeinfo) != ESP_OK) {
        time_t now = time(NULL);
        localtime_r(&now, &timeinfo);
    }

    char filename[128];
    snprintf(filename, sizeof(filename),
             "%s/%04d%02d%02d_%02d%02d%02d.bmp",
             dir_path,
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    /* 截图：在 LVGL 锁内完成 */
    lvgl_port_lock(0);
    lv_obj_t *scr = lv_screen_active();
    lv_draw_buf_t *img = lv_snapshot_take(scr, LV_COLOR_FORMAT_RGB888);
    lvgl_port_unlock();

    if (img == NULL) {
        app_manager_show_notification_timeout(_("截图失败"), 2000);
        ESP_LOGE(TAG, "lv_snapshot_take failed");
        return false;
    }

    int32_t w = (int32_t)img->header.w;
    int32_t h = (int32_t)img->header.h;
    int32_t stride = (int32_t)img->header.stride;

    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        lv_draw_buf_destroy(img);
        app_manager_show_notification_timeout(_("截图文件创建失败"), 2000);
        ESP_LOGE(TAG, "screenshot fopen failed: %s", filename);
        return false;
    }

    /* BMP 行大小需 4 字节对齐 */
    int32_t row_bytes = w * 3;
    int32_t row_pitch = (row_bytes + 3) & ~3;

    /* BMP 文件头 */
    bmp_header_t hdr = {0};
    hdr.type = 0x4D42;  /* 'BM' */
    hdr.offset = sizeof(bmp_header_t);
    hdr.hdr_size = 40;
    hdr.width = w;
    hdr.height = h;
    hdr.planes = 1;
    hdr.bpp = 24;
    hdr.img_size = (uint32_t)(row_pitch * h);
    hdr.size = hdr.offset + hdr.img_size;
    fwrite(&hdr, sizeof(hdr), 1, fp);

    /* BMP 行数据：从下往上，BGR 顺序 */
    uint8_t *row_buf = (uint8_t *)calloc(1, (size_t)row_pitch);
    if (row_buf == NULL) {
        fclose(fp);
        lv_draw_buf_destroy(img);
        app_manager_show_notification_timeout(_("截图内存不足"), 2000);
        ESP_LOGE(TAG, "screenshot row buffer alloc failed");
        return false;
    }

    const uint8_t *src = (const uint8_t *)img->data;
    uint8_t padding[3] = {0};
    for (int32_t y = h - 1; y >= 0; y--) {
        const uint8_t *line = src + y * stride;
        for (int32_t x = 0; x < w; x++) {
            row_buf[x * 3 + 0] = line[x * 3 + 2]; /* B */
            row_buf[x * 3 + 1] = line[x * 3 + 1]; /* G */
            row_buf[x * 3 + 2] = line[x * 3 + 0]; /* R */
        }
        fwrite(row_buf, (size_t)row_bytes, 1, fp);
        if (row_pitch > row_bytes) {
            fwrite(padding, (size_t)(row_pitch - row_bytes), 1, fp);
        }
    }

    free(row_buf);
    fclose(fp);
    lv_draw_buf_destroy(img);

    app_manager_show_notification_timeout(_("截图已保存"), 2000);
    ESP_LOGI(TAG, "screenshot saved: %s (%dx%d)", filename, (int)w, (int)h);
    return true;
}
