/**
 * @file engine_gui_res_vfs.c
 * @brief UI 资源 VFS 透明重定向层实现
 *
 * 挂载 /sys/src，按 SD 卡 → 固件内嵌 → SPIFFS 三级回退解析资源文件。
 */

#include "engine_gui_res_vfs.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "service_sd.h"

static const char *TAG = "engine_gui_res_vfs";

#define RES_VFS_MAX_FDS     8
#define RES_VFS_MOUNT_POINT "/sys/src"
#define RES_VFS_SD_PREFIX   "/sdcard/sys/src"
#define RES_VFS_SPI_PREFIX  "/sys_int/src"

/* SPIFFS 读取较慢，分块读取并周期性喂 task watchdog */
#define RES_VFS_WDT_FEED_BYTES  (4 * 1024)

/* -------------------- 固件内嵌资源表 -------------------- */

/* 内嵌符号声明与表项由同一份名单生成，避免两处手工维护漂移 */
#define RES_EMBED_LIST(X)              \
    X(ui_font_chinese_30)              \
    X(ui_font_chinese_40)              \
    X(ui_font_clock_150)               \
    X(ui_font_clock_150_a)             \
    X(ui_font_digi_30)                 \
    X(ui_font_icon_70)                 \
    X(ui_image_bg_day)                 \
    X(ui_image_bg_night)               \
    X(ui_image_cheer)                  \
    X(ui_image_sleepy)                 \
    X(ui_image_icon)                   \
    X(ui_image_wifi_qrcode)

#define RES_EMBED_DECL(name) \
    extern const uint8_t _binary_##name##_bin_start[]; \
    extern const uint8_t _binary_##name##_bin_end[];
RES_EMBED_LIST(RES_EMBED_DECL)
#undef RES_EMBED_DECL

typedef struct {
    const char *name;
    const uint8_t *start;
    const uint8_t *end;
} res_embedded_t;

#define RES_EMBED_ENTRY(name) \
    { "/" #name ".bin", _binary_##name##_bin_start, _binary_##name##_bin_end },
static const res_embedded_t s_embedded[] = {
    RES_EMBED_LIST(RES_EMBED_ENTRY)
};
#undef RES_EMBED_ENTRY

/* -------------------- fd 槽位 -------------------- */

typedef struct {
    bool used;
    int  real_fd;                 /* -1 表示内嵌资源 */
    bool from_spiffs;             /* SPIFFS 慢路径需分块喂狗 */
    const uint8_t *emb_base;
    size_t emb_size;
    size_t emb_pos;
} res_fd_slot_t;

static res_fd_slot_t s_fd_slots[RES_VFS_MAX_FDS];
static size_t s_spiffs_read_bytes_since_feed;

/* -------------------- 路径解析 -------------------- */

static const res_embedded_t *res_vfs_find_embedded(const char *path)
{
    for (size_t i = 0; i < sizeof(s_embedded) / sizeof(s_embedded[0]); i++) {
        if (strcmp(path, s_embedded[i].name) == 0) {
            return &s_embedded[i];
        }
    }
    return NULL;
}

/* SD 卡字体文件允许短命名（/ui_font_chinese_30.bin → /chinese_30.bin），
 * 依次用完整名与短名拼出 SD 实际路径；命中返回 true */
static bool res_vfs_sd_resolve(const char *path, char *out, size_t out_size)
{
    if (!service_sd_is_mounted()) {
        return false;
    }

    snprintf(out, out_size, "%s%s", RES_VFS_SD_PREFIX, path);
    if (access(out, F_OK) == 0) {
        return true;
    }

    const char *prefix = "/ui_font_";
    if (strncmp(path, prefix, strlen(prefix)) == 0) {
        snprintf(out, out_size, "%s/%s", RES_VFS_SD_PREFIX, path + strlen(prefix));
        if (access(out, F_OK) == 0) {
            return true;
        }
    }
    return false;
}

/* -------------------- VFS 操作 -------------------- */

static void res_vfs_wdt_feed(void)
{
    if (esp_task_wdt_status(NULL) == ESP_ERR_NOT_FOUND &&
        esp_task_wdt_add(NULL) != ESP_OK) {
        return;
    }
    esp_task_wdt_reset();
}

static int res_vfs_open(const char *path, int flags, int mode)
{
    (void)flags;
    (void)mode;

    /* 查找顺序：SD 卡 -> 固件内嵌 -> SPIFFS。
     * 内嵌为 memcpy 直读，必须先于 SPIFFS 慢路径命中（含喂狗延迟） */
    int real_fd = -1;
    bool from_spiffs = false;
    const res_embedded_t *emb = NULL;
    char real_path[128];

    if (res_vfs_sd_resolve(path, real_path, sizeof(real_path))) {
        real_fd = open(real_path, O_RDONLY);
        ESP_LOGD(TAG, "sd: %s fd=%d", real_path, real_fd);
    } else {
        emb = res_vfs_find_embedded(path);
        if (emb == NULL) {
            /* 回退 SPIFFS，只使用完整 EEZ 命名 */
            snprintf(real_path, sizeof(real_path), "%s%s", RES_VFS_SPI_PREFIX, path);
            real_fd = open(real_path, O_RDONLY);
            if (real_fd >= 0) {
                from_spiffs = true;
                ESP_LOGD(TAG, "spiffs: %s", real_path);
            }
        }
    }

    if (real_fd < 0 && emb == NULL) {
        errno = ENOENT;
        return -1;
    }

    for (int i = 0; i < RES_VFS_MAX_FDS; i++) {
        res_fd_slot_t *slot = &s_fd_slots[i];
        if (slot->used) {
            continue;
        }
        slot->used = true;
        slot->from_spiffs = from_spiffs;
        slot->real_fd = (emb != NULL) ? -1 : real_fd;
        slot->emb_base = (emb != NULL) ? emb->start : NULL;
        slot->emb_size = (emb != NULL) ? (size_t)(emb->end - emb->start) : 0;
        slot->emb_pos = 0;
        return i;
    }

    if (real_fd >= 0) {
        close(real_fd);
    }
    errno = ENFILE;
    return -1;
}

static ssize_t res_vfs_read(int fd, void *dst, size_t size)
{
    if (fd < 0 || fd >= RES_VFS_MAX_FDS || !s_fd_slots[fd].used) {
        errno = EBADF;
        return -1;
    }
    res_fd_slot_t *slot = &s_fd_slots[fd];

    if (slot->emb_base != NULL) {
        size_t remain = slot->emb_size - slot->emb_pos;
        if (size > remain) {
            size = remain;
        }
        memcpy(dst, slot->emb_base + slot->emb_pos, size);
        slot->emb_pos += size;
        return (ssize_t)size;
    }

    /* SD 卡路径直接透传，无需喂狗 */
    if (!slot->from_spiffs) {
        return read(slot->real_fd, dst, size);
    }

    /* SPIFFS 慢路径：分块读取，避免单次大 read 阻塞导致 WDT 超时 */
    size_t total = 0;
    while (total < size) {
        size_t chunk = size - total;
        if (chunk > RES_VFS_WDT_FEED_BYTES) {
            chunk = RES_VFS_WDT_FEED_BYTES;
        }
        ssize_t n = read(slot->real_fd, (uint8_t *)dst + total, chunk);
        if (n < 0) {
            return total > 0 ? (ssize_t)total : -1;
        }
        if (n == 0) {
            break;
        }
        total += (size_t)n;

        s_spiffs_read_bytes_since_feed += (size_t)n;
        if (s_spiffs_read_bytes_since_feed >= RES_VFS_WDT_FEED_BYTES) {
            s_spiffs_read_bytes_since_feed = 0;
            res_vfs_wdt_feed();
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    return (ssize_t)total;
}

static int res_vfs_close(int fd)
{
    if (fd < 0 || fd >= RES_VFS_MAX_FDS || !s_fd_slots[fd].used) {
        errno = EBADF;
        return -1;
    }
    res_fd_slot_t *slot = &s_fd_slots[fd];
    int ret = 0;
    if (slot->emb_base == NULL) {
        ret = close(slot->real_fd);
    }
    slot->used = false;
    slot->emb_base = NULL;
    return ret;
}

static off_t res_vfs_lseek(int fd, off_t offset, int whence)
{
    if (fd < 0 || fd >= RES_VFS_MAX_FDS || !s_fd_slots[fd].used) {
        errno = EBADF;
        return -1;
    }
    res_fd_slot_t *slot = &s_fd_slots[fd];

    if (slot->emb_base == NULL) {
        return lseek(slot->real_fd, offset, whence);
    }

    off_t np;
    switch (whence) {
    case SEEK_SET:
        np = offset;
        break;
    case SEEK_CUR:
        np = (off_t)slot->emb_pos + offset;
        break;
    case SEEK_END:
        np = (off_t)slot->emb_size + offset;
        break;
    default:
        errno = EINVAL;
        return -1;
    }
    if (np < 0) {
        np = 0;
    }
    if (np > (off_t)slot->emb_size) {
        np = (off_t)slot->emb_size;
    }
    slot->emb_pos = (size_t)np;
    return np;
}

static int res_vfs_stat(const char *path, struct stat *st)
{
    char real_path[128];
    if (res_vfs_sd_resolve(path, real_path, sizeof(real_path))) {
        return stat(real_path, st);
    }

    const res_embedded_t *emb = res_vfs_find_embedded(path);
    if (emb != NULL) {
        memset(st, 0, sizeof(*st));
        st->st_mode = S_IFREG | 0444;
        st->st_size = (off_t)(emb->end - emb->start);
        return 0;
    }

    snprintf(real_path, sizeof(real_path), "%s%s", RES_VFS_SPI_PREFIX, path);
    return stat(real_path, st);
}

esp_err_t engine_gui_res_vfs_register(void)
{
    memset(s_fd_slots, 0, sizeof(s_fd_slots));
    s_spiffs_read_bytes_since_feed = 0;

    esp_vfs_t vfs = {
        .flags = ESP_VFS_FLAG_DEFAULT,
        .open  = res_vfs_open,
        .close = res_vfs_close,
        .read  = res_vfs_read,
        .write = NULL,
        .lseek = res_vfs_lseek,
        .stat  = res_vfs_stat,
    };

    esp_err_t ret = esp_vfs_register(RES_VFS_MOUNT_POINT, &vfs, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "register failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "registered: %s -> sd=%s, spiffs=%s",
             RES_VFS_MOUNT_POINT, RES_VFS_SD_PREFIX, RES_VFS_SPI_PREFIX);
    return ESP_OK;
}
