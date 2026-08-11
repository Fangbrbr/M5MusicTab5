/**
 * @file service_recorder.h
 * @brief MIDI 总线录音服务
 *
 * 作为 engine_midi 总线消费者，录制系统内部 MIDI 事件流到 SD 卡。
 */

#ifndef SERVICE_RECORDER_H
#define SERVICE_RECORDER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 录音操作结果枚举
 */
typedef enum {
    RECORDER_OK = 0,                /**< 成功 */
    RECORDER_ERR_NO_SD,             /**< SD 卡未挂载 */
    RECORDER_ERR_BUSY,              /**< 已在录制中 */
    RECORDER_ERR_NO_MEM,            /**< 内存/队列分配失败 */
    RECORDER_ERR_FILE_CREATE,       /**< 无法创建录音文件 */
    RECORDER_ERR_NOT_RECORDING,     /**< 停止时未在录制 */
    RECORDER_ERR_WRITE,             /**< 文件写入失败 */
    RECORDER_ERR_TOO_SHORT,           /**< 录制时长不足，文件已丢弃 */
    RECORDER_ERR_INTERNAL,            /**< 其他内部错误 */
} service_recorder_result_t;

/**
 * @brief 初始化录音服务并创建 record 目录
 */
service_recorder_result_t service_recorder_init(void);

/**
 * @brief 开始录制
 *
 * @param[in] source_tag 来源标签，用于文件名与文件头；将被截断至 15 字符
 * @return 操作结果，调用方可据此向用户显示通知
 */
service_recorder_result_t service_recorder_start(const char *source_tag);

/**
 * @brief 请求停止录制
 *
 * 实际落盘与关闭文件由 service_recorder_process() 异步完成。
 */
service_recorder_result_t service_recorder_stop(void);

/**
 * @brief 周期处理，由 task_app 每 10ms 调用
 */
void service_recorder_process(void);

/**
 * @brief 是否正在录制中
 */
bool service_recorder_is_recording(void);

/**
 * @brief 获取最近一次成功录制文件的绝对路径
 *
 * @param[out] buf 输出缓冲
 * @param[in] len  缓冲长度
 * @return true 存在；false 无或参数错误
 */
bool service_recorder_get_last_path(char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_RECORDER_H */
