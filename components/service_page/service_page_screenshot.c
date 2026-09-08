/**
 * @file service_page_screenshot.c
 * @brief 截图功能（自 engine_gui 解耦）
 *
 * 截取当前屏幕为 BMP（24bit BGR），保存至 SD 卡 /screenshot。
 *
 * 两条取帧路径：
 * 1. DSI 帧缓冲直读（Tab5 主路径）：物理帧缓冲是 PSRAM 常驻的 RGB565 现帧，
 *    按 64x64 tile 在 LVGL 锁内拷出、解锁后反旋转展开写 SD——无大块瞬时分配
 *    （峰值 band 240KB + tile 8KB，大 SF2 载入后也能保障），无整屏重绘。
 * 2. lv_snapshot 重绘（非 DSI 板兜底）：自带 PSRAM 缓冲走 take_to_draw_buf。
 *    Trap: lv_snapshot_take 的缓冲走 lv_malloc（内建 TLSF 池 64KB），全屏必失败。
 */

#include "service_page_screenshot.h"
#include "app_manager.h"
#include "engine_gui.h"
#include "service_sd.h"
#include "service_rtc.h"
#include "service_i18n.h"
#include "esp_lvgl_port.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

static const char *TAG = "service_page_screenshot";

#define SHOT_TILE_PX    64   /* tile 边长：锁内拷贝 64x64x2=8KB，微秒级，防撕裂 */
#define SHOT_BAND_ROWS  64   /* band 高：64 逻辑行 x 宽 x 3B（1280 宽时 240KB） */

/* 截屏请求标志：request（任意上下文）登记，process（task_app）消化后清 */
static volatile bool s_shot_pending = false;

static bool screenshot_write_fb(const char *filename, const uint16_t *fb,
                                lv_display_rotation_t rotation,
                                int32_t phy_w, int32_t phy_h,
                                int32_t log_w, int32_t log_h);
static bool screenshot_write_snapshot(const char *filename);

void service_page_screenshot_request(void)
{
    s_shot_pending = true;
}

void service_page_screenshot_process(void)
{
    if (!s_shot_pending) {
        return;
    }
    s_shot_pending = false;
    service_page_take_screenshot();
}

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

/* BMP 文件头（24bit BGR，行 4 字节对齐，底向上） */
static void shot_write_header(FILE *fp, int32_t w, int32_t h)
{
    int32_t row_pitch = (w * 3 + 3) & ~3;
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
}

/* RGB565 → 24bit BGR：低位复制扩展（比纯左移更接近原色） */
static void shot_px(uint16_t c, uint8_t *out)
{
    uint32_t r = (c >> 11) & 0x1F;
    uint32_t g = (c >> 5) & 0x3F;
    uint32_t b = c & 0x1F;
    out[0] = (uint8_t)((b << 3) | (b >> 2));
    out[1] = (uint8_t)((g << 2) | (g >> 4));
    out[2] = (uint8_t)((r << 3) | (r >> 2));
}

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

    /* 按日期时间生成文件名；RTC 缓存按分钟量化，同分钟内多张会同名覆盖
     *（2026-09 真机：截了 N 张只剩每分钟最后一张）——同名追加 _1/_2… */
    struct tm timeinfo = {0};
    if (service_rtc_get_time_cached(&timeinfo) != ESP_OK) {
        time_t now = time(NULL);
        localtime_r(&now, &timeinfo);
    }

    char filename[128];
    struct stat fst;
    for (int dup = 0; ; dup++) {
        if (dup == 0) {
            snprintf(filename, sizeof(filename),
                     "%s/%04d%02d%02d_%02d%02d%02d.bmp",
                     dir_path,
                     timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        } else {
            snprintf(filename, sizeof(filename),
                     "%s/%04d%02d%02d_%02d%02d%02d_%d.bmp",
                     dir_path,
                     timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, dup);
        }
        if (stat(filename, &fst) != 0) {
            break;
        }
    }

    /* 路径分派：有 DSI 帧缓冲且旋转为 90/270 走直读，否则 snapshot 兜底 */
    int32_t phy_w = 0, phy_h = 0;
    lv_display_rotation_t rotation = LV_DISPLAY_ROTATION_0;
    int32_t log_w = 0, log_h = 0;
    lvgl_port_lock(portMAX_DELAY);
    const uint16_t *fb = (const uint16_t *)engine_gui_get_dsi_fb(&phy_w, &phy_h, &rotation);
    lv_display_t *disp = lv_display_get_default();
    if (disp != NULL) {
        log_w = lv_display_get_horizontal_resolution(disp);
        log_h = lv_display_get_vertical_resolution(disp);
    }
    lvgl_port_unlock();

    bool rot_ok = (rotation == LV_DISPLAY_ROTATION_90 ||
                   rotation == LV_DISPLAY_ROTATION_270);
    if (fb != NULL && log_w > 0 && log_h > 0 && rot_ok) {
        return screenshot_write_fb(filename, fb, rotation, phy_w, phy_h, log_w, log_h);
    }
    return screenshot_write_snapshot(filename);
}

/* 路径 1：DSI 帧缓冲直读。
 * 反旋转映射（与 flush 所用 lv_draw_sw_rotate 的数学互逆，物理→逻辑）：
 *   ROT_90:  logical(x,y) = src[(phy_h-1-x) * phy_w + y]
 *   ROT_270: logical(x,y) = src[x * phy_w + (phy_w-1-y)]
 * tile 化原因：逻辑行 = 物理列（1440B 步长跨行），整行直读的 PSRAM 缓存行
 * 流量放大约 32 倍；tile 让物理读保持 tw 行 x band_h 列的连续块。 */
static bool screenshot_write_fb(const char *filename, const uint16_t *fb,
                                lv_display_rotation_t rotation,
                                int32_t phy_w, int32_t phy_h,
                                int32_t log_w, int32_t log_h)
{
    /* 防撕裂/防动画冻结的关键：整帧先在一次 LVGL 锁内 memcpy 进 PSRAM 反弹
     * 缓冲（720x1280x2=1.84MB，约 5ms 原子帧），之后转换/写盘全在锁外。
     * 分 tile 直读会让滚动文字等动效跨帧剪切（2026-09 真机），且 240 次
     * 锁竞争与 task_gui 互相等待——zen 动画停顿、全程 4.6s 皆源于此。
     * 反弹分配失败退回 tile 直读（动效可能剪切，功能仍可用）。 */
    size_t fb_size = (size_t)phy_w * (size_t)phy_h * 2;
    uint16_t *bounce = (uint16_t *)heap_caps_malloc(fb_size, MALLOC_CAP_SPIRAM);
    const uint16_t *src = fb;
    if (bounce != NULL) {
        lvgl_port_lock(portMAX_DELAY);
        memcpy(bounce, fb, fb_size);
        lvgl_port_unlock();
        src = bounce;
    } else {
        ESP_LOGW(TAG, "bounce alloc failed, tile-read fb directly (may shear)");
    }
    const bool locked_read = (bounce == NULL);

    uint8_t *band_buf = (uint8_t *)heap_caps_malloc((size_t)log_w * SHOT_BAND_ROWS * 3,
                                                    MALLOC_CAP_SPIRAM);
    uint16_t *tile_buf = (uint16_t *)heap_caps_malloc(SHOT_TILE_PX * SHOT_TILE_PX * 2,
                                                      MALLOC_CAP_SPIRAM);
    if (band_buf == NULL || tile_buf == NULL) {
        heap_caps_free(bounce);
        heap_caps_free(band_buf);
        heap_caps_free(tile_buf);
        app_manager_show_notification_timeout(_("截图内存不足"), 2000);
        ESP_LOGE(TAG, "screenshot band/tile buffer alloc failed");
        return false;
    }

    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        heap_caps_free(bounce);
        heap_caps_free(band_buf);
        heap_caps_free(tile_buf);
        app_manager_show_notification_timeout(_("截图文件创建失败"), 2000);
        ESP_LOGE(TAG, "screenshot fopen failed: %s", filename);
        return false;
    }

    shot_write_header(fp, log_w, log_h);

    const int32_t row_pitch = (log_w * 3 + 3) & ~3;
    const int32_t pad = row_pitch - log_w * 3;
    static const uint8_t s_pad[3] = {0};

    /* BMP 底向上：逻辑行从 log_h-1 写到 0，band 从高往低推进 */
    for (int32_t band_top = log_h - 1; band_top >= 0; band_top -= SHOT_BAND_ROWS) {
        int32_t band_h = (band_top + 1 < SHOT_BAND_ROWS) ? (band_top + 1) : SHOT_BAND_ROWS;
        int32_t y_lo = band_top - band_h + 1;

        for (int32_t tx = 0; tx < log_w; tx += SHOT_TILE_PX) {
            int32_t tw = (log_w - tx < SHOT_TILE_PX) ? (log_w - tx) : SHOT_TILE_PX;

            /* 拷物理块：ROT_90/270 下逻辑 tile 对应物理连续 tw 行 x band_h 列；
             * 反弹缓冲路径无需持锁（私有副本），直读路径才逐 tile 锁 */
            int32_t py0, px0;
            if (rotation == LV_DISPLAY_ROTATION_90) {
                py0 = phy_h - 1 - (tx + tw - 1);
                px0 = y_lo;
            } else {
                py0 = tx;
                px0 = phy_w - 1 - band_top;
            }
            if (locked_read) {
                lvgl_port_lock(portMAX_DELAY);
            }
            for (int32_t r = 0; r < tw; r++) {
                memcpy(&tile_buf[r * band_h], &src[(py0 + r) * phy_w + px0],
                       (size_t)band_h * 2);
            }
            if (locked_read) {
                lvgl_port_unlock();
            }

            /* 反旋转展开进 band；行序直接倒置存放（底向上），band 可单次写入 */
            for (int32_t yy = 0; yy < band_h; yy++) {
                uint8_t *out = band_buf + ((size_t)(band_h - 1 - yy) * log_w + tx) * 3;
                for (int32_t xx = 0; xx < tw; xx++) {
                    uint16_t c = (rotation == LV_DISPLAY_ROTATION_90)
                               ? tile_buf[(tw - 1 - xx) * band_h + yy]
                               : tile_buf[xx * band_h + (band_h - 1 - yy)];
                    shot_px(c, out + xx * 3);
                }
            }
        }

        /* band 已按底向上排好：无行对齐填充时单次 fwrite（720 次→12 次） */
        if (pad == 0) {
            fwrite(band_buf, (size_t)(log_w * 3), (size_t)band_h, fp);
        } else {
            for (int32_t yy = 0; yy < band_h; yy++) {
                fwrite(band_buf + (size_t)yy * log_w * 3, (size_t)(log_w * 3), 1, fp);
                fwrite(s_pad, (size_t)pad, 1, fp);
            }
        }
    }

    fclose(fp);
    heap_caps_free(bounce);
    heap_caps_free(band_buf);
    heap_caps_free(tile_buf);

    app_manager_show_notification_timeout(_("截图已保存"), 2000);
    ESP_LOGI(TAG, "screenshot saved: %s (%dx%d, fb %s)", filename,
             (int)log_w, (int)log_h, locked_read ? "tile-read" : "bounce");
    return true;
}

/* 路径 2：lv_snapshot 重绘兜底（无 DSI 帧缓冲的板子；锁内整屏重绘到自带缓冲）。
 * Trap 1: 缓冲必须自带 PSRAM（reshape 只校验容量，不重分配外部缓冲）。
 * Trap 2: 锁必须 portMAX_DELAY 拿实，try-lock 失败裸跑会与 task_gui 渲染竞争。 */
static bool screenshot_write_snapshot(const char *filename)
{
    lvgl_port_lock(portMAX_DELAY);
    lv_obj_t *scr = lv_screen_active();
    int32_t w = lv_obj_get_width(scr);
    int32_t h = lv_obj_get_height(scr);
    uint32_t stride0 = LV_DRAW_BUF_STRIDE(w, LV_COLOR_FORMAT_RGB888);
    uint32_t shot_size = stride0 * (uint32_t)h;
    uint8_t *shot_buf = (uint8_t *)heap_caps_malloc(shot_size, MALLOC_CAP_SPIRAM);
    if (shot_buf == NULL) {
        lvgl_port_unlock();
        app_manager_show_notification_timeout(_("截图内存不足"), 2000);
        ESP_LOGE(TAG, "screenshot buffer alloc failed (%lu bytes)",
                 (unsigned long)shot_size);
        return false;
    }

    lv_draw_buf_t db;
    bool snap_ok = (lv_draw_buf_init(&db, (uint32_t)w, (uint32_t)h,
                                     LV_COLOR_FORMAT_RGB888, stride0,
                                     shot_buf, shot_size) == LV_RESULT_OK) &&
                   (lv_snapshot_take_to_draw_buf(scr, LV_COLOR_FORMAT_RGB888, &db) == LV_RESULT_OK);
    lvgl_port_unlock();

    if (!snap_ok) {
        heap_caps_free(shot_buf);
        app_manager_show_notification_timeout(_("截图失败"), 2000);
        ESP_LOGE(TAG, "lv_snapshot_take_to_draw_buf failed");
        return false;
    }

    w = (int32_t)db.header.w;
    h = (int32_t)db.header.h;
    int32_t stride = (int32_t)db.header.stride;

    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        heap_caps_free(shot_buf);
        app_manager_show_notification_timeout(_("截图文件创建失败"), 2000);
        ESP_LOGE(TAG, "screenshot fopen failed: %s", filename);
        return false;
    }

    int32_t row_bytes = w * 3;
    int32_t row_pitch = (row_bytes + 3) & ~3;
    shot_write_header(fp, w, h);

    /* BMP 行数据：从下往上，BGR 顺序 */
    uint8_t *row_buf = (uint8_t *)calloc(1, (size_t)row_pitch);
    if (row_buf == NULL) {
        fclose(fp);
        heap_caps_free(shot_buf);
        app_manager_show_notification_timeout(_("截图内存不足"), 2000);
        ESP_LOGE(TAG, "screenshot row buffer alloc failed");
        return false;
    }

    const uint8_t *src = (const uint8_t *)db.data;
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
    heap_caps_free(shot_buf);

    app_manager_show_notification_timeout(_("截图已保存"), 2000);
    ESP_LOGI(TAG, "screenshot saved: %s (%dx%d, snapshot)", filename, (int)w, (int)h);
    return true;
}
