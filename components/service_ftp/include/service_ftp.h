/**
 * @file service_ftp.h
 * @brief SD 卡 FTP 文件管理服务（lwIP 原生，非阻塞轮询）
 *
 * 供 FTP 系统屏独占使用：进入页面 start、退出页面 stop，
 * 协议状态机由 task_comm 每 10 ms 经 service_ftp_process() 驱动。
 */

#ifndef SERVICE_FTP_H
#define SERVICE_FTP_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief FTP 服务状态（页面显示用） */
typedef enum {
    SERVICE_FTP_STATE_OFF = 0,      /**< 未启动 */
    SERVICE_FTP_STATE_LISTENING,    /**< 等待客户端连接 */
    SERVICE_FTP_STATE_CONNECTED,    /**< 控制连接已建立 */
    SERVICE_FTP_STATE_TRANSFERRING, /**< 数据传输中 */
} service_ftp_state_t;

/** @brief FTP 运行状态快照 */
typedef struct {
    service_ftp_state_t state;
    char client_ip[16];      /**< 已连接客户端 IP，无连接为空串 */
    char file_name[256];     /**< 传输中/上次传输的文件 basename */
    uint32_t file_size;      /**< 总字节数（STOR 上传未知时为 0） */
    uint32_t bytes_done;     /**< 已传字节数 */
} service_ftp_status_t;

/**
 * @brief 初始化服务状态（不开 socket、不分配缓冲）
 *
 * 开机阶段调用一次；无 WiFi 板型为 stub，返回 ESP_OK。
 */
esp_err_t service_ftp_init(void);

/**
 * @brief 启动 FTP 服务：分配 PSRAM 数据缓冲并监听 21 端口
 *
 * @return ESP_OK 成功；ESP_ERR_INVALID_STATE SD 未挂载；
 *         ESP_ERR_NO_MEM 缓冲分配失败；无 WiFi 板型返回 ESP_ERR_NOT_SUPPORTED
 */
esp_err_t service_ftp_start(void);

/**
 * @brief 停止服务：中止传输、断开客户端、关闭全部 socket、释放缓冲
 *
 * 未启动时调用安全（空转）。
 */
void service_ftp_stop(void);

/**
 * @brief 协议状态机轮询（非阻塞，有界返回）
 *
 * 由 task_comm 每 10 ms 调用一次；未 init/未 start 时直接返回。
 * 数据通道每拍最多处理 4 块 × 8 KB，绝不忙等。
 */
void service_ftp_process(void);

/**
 * @brief 读取运行状态快照
 *
 * Contract: 写侧在 task_comm（process），读侧在 task_gui（页面 tick），
 * 无锁读取仅用于显示，允许良性撕裂（错一帧数值，下拍自愈）。
 */
void service_ftp_get_status(service_ftp_status_t *out);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_FTP_H */
