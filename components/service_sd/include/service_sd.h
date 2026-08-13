/**
 * @file service_sd.h
 * @brief SD 卡服务
 *
 * SDMMC 挂载与文件系统抽象。为上层引擎/应用提供统一的 SD 卡文件访问接口，
 * 支持 WAV、MIDI、图片、文本等文件类型。
 */

#ifndef SERVICE_SD_H
#define SERVICE_SD_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief SD 卡挂载状态 */
typedef enum {
    SERVICE_SD_STATE_UNMOUNTED = 0,
    SERVICE_SD_STATE_MOUNTED,
    SERVICE_SD_STATE_ERROR,
} service_sd_state_t;

/** @brief 文件类型分类，供上层模块做路径过滤或菜单展示 */
typedef enum {
    SERVICE_SD_FILE_UNKNOWN = 0,
    SERVICE_SD_FILE_WAV,
    SERVICE_SD_FILE_MIDI,
    SERVICE_SD_FILE_IMAGE,
    SERVICE_SD_FILE_TEXT,
} service_sd_file_type_t;

/**
 * @brief 初始化并挂载 SD 卡
 *
 * 调用 BSP 的 bsp_sdcard_mount() 将 SD 卡挂载到默认挂载点（通常为 /sdcard）。
 * 挂载失败会记录日志，但不会阻塞系统启动；上层应通过 service_sd_is_mounted()
 * 判断后再访问文件。
 *
 * @return ESP_OK 挂载成功；其他错误码表示挂载失败，系统仍应继续启动。
 */
esp_err_t service_sd_init(void);

/**
 * @brief 反初始化并卸载 SD 卡
 */
void service_sd_deinit(void);

/**
 * @brief 获取当前 SD 卡状态
 */
service_sd_state_t service_sd_get_state(void);

/**
 * @brief 判断 SD 卡是否已挂载
 */
static inline bool service_sd_is_mounted(void)
{
    return service_sd_get_state() == SERVICE_SD_STATE_MOUNTED;
}

/**
 * @brief 获取 SD 卡挂载根目录（通常为 "/sdcard"）
 */
const char *service_sd_get_mount_point(void);

/**
 * @brief 用 stdio 打开 SD 卡上的文件
 *
 * @param[in] relative_path 相对于挂载点的路径，如 "test.wav"
 * @param[in] mode          stdio 模式，如 "rb"
 * @return FILE* 成功，NULL 失败
 */
FILE *service_sd_fopen(const char *relative_path, const char *mode);

/**
 * @brief 关闭由 service_sd_fopen 打开的文件
 */
int service_sd_fclose(FILE *fp);

/**
 * @brief 检查文件是否存在
 */
bool service_sd_file_exists(const char *relative_path);

/**
 * @brief 获取文件大小（字节），失败返回 -1
 */
int64_t service_sd_file_size(const char *relative_path);

/**
 * @brief 枚举目录中的文件
 *
 * @param[in]  relative_dir 相对目录，如 "" 或 "music/"
 * @param[in]  type_filter  文件类型过滤，SERVICE_SD_FILE_UNKNOWN 表示不过滤
 * @param[out] out_names    输出文件名数组（调用者分配）
 * @param[in]  max_count    最大条目数
 * @param[in]  name_buf     连续缓冲区，用于存储文件名
 * @param[in]  name_buf_len 缓冲区总长度
 * @return >=0 实际枚举到的文件数，<0 出错
 */
int service_sd_list_files(const char *relative_dir,
                          service_sd_file_type_t type_filter,
                          char **out_names,
                          uint32_t max_count,
                          char *name_buf,
                          uint32_t name_buf_len);

/**
 * @brief 根据扩展名判断文件类型
 */
service_sd_file_type_t service_sd_detect_file_type(const char *filename);

/**
 * @brief 读取整个文件到已分配内存（适合配置文件、小图片等）
 *
 * @param[in]  relative_path 相对路径
 * @param[out] out_buf       输出缓冲区（调用者分配）
 * @param[in]  buf_len       缓冲区长度
 * @param[out] out_read_len  实际读取长度
 * @return ESP_OK 成功
 */
esp_err_t service_sd_read_file(const char *relative_path,
                               uint8_t *out_buf,
                               uint32_t buf_len,
                               uint32_t *out_read_len);

/**
 * @brief 获取 SD 卡总容量与剩余空间（字节）
 */
esp_err_t service_sd_get_capacity(int64_t *out_total_bytes,
                                  int64_t *out_free_bytes);

/**
 * @brief 拼接挂载点与相对路径，生成绝对路径
 *
 * @param[out] out_abs_path 输出缓冲区
 * @param[in]  out_len      缓冲区长度
 * @param[in]  relative_path 相对路径
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 参数错误，ESP_ERR_NO_MEM 缓冲区不足
 */
esp_err_t service_sd_build_path(char *out_abs_path,
                                uint32_t out_len,
                                const char *relative_path);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_SD_H */
