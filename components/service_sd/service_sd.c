/**
 * @file service_sd.c
 * @brief SD 卡服务实现
 *
 * Why: SD 卡槽依赖 Tab5 BSP 的 SDMMC 路径，由 CONFIG_BOARD_HAS_SD 编译期门控；
 * 无卡槽板型 init 返回 ESP_ERR_NOT_SUPPORTED，状态恒为 UNMOUNTED，
 * 其余 API 经 service_sd_is_mounted() 检查自然降级（与 Tab5 未插卡行为一致）。
 */

#include "service_sd.h"
/* Trap: sdkconfig.h 必须先于 CONFIG_BOARD_HAS_SD 门控包含（esp_log.h 会间接带入，
 * 但门控在其之前求值时会静默判 0，导致 Tab5 的 SD 路径被错误剔除） */
#include "sdkconfig.h"
#if CONFIG_BOARD_HAS_SD
#include "bsp/m5stack_tab5.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#endif
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

static const char *TAG = "service_sd";

#ifndef CONFIG_BSP_SD_MOUNT_POINT
#define CONFIG_BSP_SD_MOUNT_POINT "/sdcard"
#endif

static service_sd_state_t s_state = SERVICE_SD_STATE_UNMOUNTED;

/* 内部辅助：构建绝对路径 */
static esp_err_t s_build_abs_path(char *out, uint32_t out_len, const char *relative)
{
    if (!out || out_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *mount = service_sd_get_mount_point();
    const char *rel = relative ? relative : "";

    /* 如果已经是绝对路径，直接拷贝 */
    if (rel[0] == '/') {
        if (strlen(rel) >= out_len) {
            return ESP_ERR_NO_MEM;
        }
        strcpy(out, rel);
        return ESP_OK;
    }

    int written = snprintf(out, out_len, "%s/%s", mount, rel);
    if (written < 0 || (uint32_t)written >= out_len) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t service_sd_init(void)
{
#if !CONFIG_BOARD_HAS_SD
    ESP_LOGW(TAG, "board has no SD slot, SD service disabled");
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (s_state == SERVICE_SD_STATE_MOUNTED) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Mounting SD card to %s ...", CONFIG_BSP_SD_MOUNT_POINT);

    /* 与 SDIO Wi-Fi 共用 SDMMC host 时，需要启用内部上拉并降低频率以保证信号完整性。 */
    sdmmc_host_t host = {0};
    sdmmc_slot_config_t slot = {0};
    const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    bsp_sdcard_get_sdmmc_host(SDMMC_HOST_SLOT_0, &host);
    host.max_freq_khz = SDMMC_FREQ_DEFAULT; /* 20 MHz */

    bsp_sdcard_sdmmc_get_slot(SDMMC_HOST_SLOT_0, &slot);
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    bsp_sdcard_cfg_t cfg = {
        .mount = &mount_config,
        .host = &host,
        .slot.sdmmc = &slot,
    };

    esp_err_t ret = bsp_sdcard_sdmmc_mount(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD card mount failed: %s", esp_err_to_name(ret));
        s_state = SERVICE_SD_STATE_ERROR;
        return ret;
    }

    sdmmc_card_t *card = bsp_sdcard_get_handle();
    if (card != NULL) {
        uint32_t capacity_mb = (uint32_t)(card->csd.capacity / 2048);
        ESP_LOGI(TAG, "SD card: %s, capacity=%luMB, %d-bit, %lu MHz",
                 card->cid.name,
                 (unsigned long)capacity_mb,
                 card->log_bus_width + 1,
                 (unsigned long)(card->max_freq_khz / 1000));
    }

    s_state = SERVICE_SD_STATE_MOUNTED;
    ESP_LOGI(TAG, "SD card mounted successfully");
    return ESP_OK;
#endif /* CONFIG_BOARD_HAS_SD */
}

void service_sd_deinit(void)
{
    if (s_state != SERVICE_SD_STATE_MOUNTED) {
        return;
    }

#if CONFIG_BOARD_HAS_SD
    esp_err_t ret = bsp_sdcard_unmount();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD card unmount failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SD card unmounted");
    }
#endif
    s_state = SERVICE_SD_STATE_UNMOUNTED;
}

service_sd_state_t service_sd_get_state(void)
{
    return s_state;
}

const char *service_sd_get_mount_point(void)
{
    return CONFIG_BSP_SD_MOUNT_POINT;
}

FILE *service_sd_fopen(const char *relative_path, const char *mode)
{
    if (!service_sd_is_mounted() || !relative_path || !mode) {
        return NULL;
    }

    char abs_path[512];
    if (s_build_abs_path(abs_path, sizeof(abs_path), relative_path) != ESP_OK) {
        return NULL;
    }

    return fopen(abs_path, mode);
}

int service_sd_fclose(FILE *fp)
{
    if (!fp) {
        return 0;
    }
    return fclose(fp);
}

bool service_sd_file_exists(const char *relative_path)
{
    if (!service_sd_is_mounted() || !relative_path) {
        return false;
    }

    char abs_path[512];
    if (s_build_abs_path(abs_path, sizeof(abs_path), relative_path) != ESP_OK) {
        return false;
    }

    struct stat st;
    return (stat(abs_path, &st) == 0);
}

int64_t service_sd_file_size(const char *relative_path)
{
    if (!service_sd_is_mounted() || !relative_path) {
        return -1;
    }

    char abs_path[512];
    if (s_build_abs_path(abs_path, sizeof(abs_path), relative_path) != ESP_OK) {
        return -1;
    }

    struct stat st;
    if (stat(abs_path, &st) != 0) {
        return -1;
    }
    return (int64_t)st.st_size;
}

service_sd_file_type_t service_sd_detect_file_type(const char *filename)
{
    if (!filename) {
        return SERVICE_SD_FILE_UNKNOWN;
    }

    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) {
        return SERVICE_SD_FILE_UNKNOWN;
    }

    /* 简单大小写不敏感比较 */
    char ext[16];
    strncpy(ext, dot + 1, sizeof(ext) - 1);
    ext[sizeof(ext) - 1] = '\0';
    for (char *p = ext; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') {
            *p = *p - 'A' + 'a';
        }
    }

    if (strcmp(ext, "wav") == 0) {
        return SERVICE_SD_FILE_WAV;
    }
    if (strcmp(ext, "mid") == 0 || strcmp(ext, "midi") == 0) {
        return SERVICE_SD_FILE_MIDI;
    }
    if (strcmp(ext, "bmp") == 0 || strcmp(ext, "png") == 0 ||
        strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0 ||
        strcmp(ext, "gif") == 0) {
        return SERVICE_SD_FILE_IMAGE;
    }
    if (strcmp(ext, "txt") == 0 || strcmp(ext, "md") == 0 ||
        strcmp(ext, "json") == 0 || strcmp(ext, "xml") == 0 ||
        strcmp(ext, "csv") == 0) {
        return SERVICE_SD_FILE_TEXT;
    }

    return SERVICE_SD_FILE_UNKNOWN;
}

int service_sd_list_files(const char *relative_dir,
                          service_sd_file_type_t type_filter,
                          char **out_names,
                          uint32_t max_count,
                          char *name_buf,
                          uint32_t name_buf_len)
{
    if (!service_sd_is_mounted() || !out_names || max_count == 0 ||
        !name_buf || name_buf_len == 0) {
        return -1;
    }

    char abs_dir[512];
    if (s_build_abs_path(abs_dir, sizeof(abs_dir), relative_dir) != ESP_OK) {
        return -1;
    }

    DIR *dir = opendir(abs_dir);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open dir %s", abs_dir);
        return -1;
    }

    uint32_t count = 0;
    uint32_t buf_used = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG) {
            continue;
        }

        if (type_filter != SERVICE_SD_FILE_UNKNOWN) {
            if (service_sd_detect_file_type(entry->d_name) != type_filter) {
                continue;
            }
        }

        size_t name_len = strlen(entry->d_name) + 1;
        if (buf_used + name_len > name_buf_len) {
            ESP_LOGW(TAG, "Name buffer full, stopped listing");
            break;
        }
        if (count >= max_count) {
            break;
        }

        memcpy(name_buf + buf_used, entry->d_name, name_len);
        out_names[count] = name_buf + buf_used;
        buf_used += name_len;
        count++;
    }

    closedir(dir);
    return (int)count;
}

esp_err_t service_sd_read_file(const char *relative_path,
                               uint8_t *out_buf,
                               uint32_t buf_len,
                               uint32_t *out_read_len)
{
    if (!out_buf || buf_len == 0 || !out_read_len) {
        return ESP_ERR_INVALID_ARG;
    }

    FILE *fp = service_sd_fopen(relative_path, "rb");
    if (!fp) {
        return ESP_ERR_NOT_FOUND;
    }

    size_t read = fread(out_buf, 1, buf_len, fp);
    service_sd_fclose(fp);

    *out_read_len = (uint32_t)read;
    return ESP_OK;
}

esp_err_t service_sd_get_capacity(int64_t *out_total_bytes,
                                  int64_t *out_free_bytes)
{
    if (!service_sd_is_mounted() || !out_total_bytes || !out_free_bytes) {
        return ESP_ERR_INVALID_ARG;
    }

    FATFS *fs = NULL;
    DWORD free_clusters = 0;
    char drv[3] = {'0', ':', 0};

    FRESULT res = f_getfree(drv, &free_clusters, &fs);
    if (res != FR_OK || fs == NULL) {
        return ESP_FAIL;
    }

    uint64_t total_sectors = ((uint64_t)(fs->n_fatent - 2)) * fs->csize;
    uint64_t free_sectors = ((uint64_t)free_clusters) * fs->csize;
    uint16_t sector_size = (uint16_t)fs->ssize;

    *out_total_bytes = (int64_t)(total_sectors * sector_size);
    *out_free_bytes = (int64_t)(free_sectors * sector_size);
    return ESP_OK;
}

esp_err_t service_sd_build_path(char *out_abs_path,
                                uint32_t out_len,
                                const char *relative_path)
{
    return s_build_abs_path(out_abs_path, out_len, relative_path);
}
