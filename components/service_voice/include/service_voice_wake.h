/**
 * @file service_voice_wake.h
 * @brief 服务语音唤醒词前端：esp-sr AFE + WakeNet 封装
 *
 * 模型取自 "model" 分区（sdkconfig 选定 wn9_himiaomiao，「Hi，喵喵」），
 * AFE 按 MR 双通道 16kHz 配置：AEC 开（消除扬声器回采）、NS 关、VAD/WakeNet 开。
 * feed/fetch 均由调用方（xz_task）单线程驱动，模块内不建任务。
 * 打开/关闭必须与 mic 泵严格配对，关闭时连同模型列表一起释放。
 */

#ifndef SERVICE_VOICE_WAKE_H
#define SERVICE_VOICE_WAKE_H

#include "esp_err.h"
#include "stdbool.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 一次 fetch 的输出（指针有效期至下一次 fetch） */
typedef struct {
    const int16_t *data;        /*!< AFE 输出 PCM（16kHz 单声道） */
    int samples;                /*!< 采样数 */
    bool wake_detected;         /*!< 本帧为唤醒检出沿（wakeup_state==WAKENET_DETECTED，
                                     一次性事件；对齐上游 xiaozhi HandleWakeWordResult） */
    int wakenet_model_index;    /*!< 锁存状态字段：检出沿到达时有效（索引 1 起）。
                                     Trap: 命中后长期保持 >0，严禁当事件重复消费 */
    int vad_state;              /*!< 本帧 VAD 判决（0=静音 1=语音，见 esp_vad.h） */
} service_voice_wake_result_t;

/**
 * @brief 打开唤醒前端（加载模型列表并创建 AFE 实例）
 * @return ESP_OK 成功；模型分区缺失/无 WakeNet 模型/AFE 创建失败时 ESP_FAIL
 */
esp_err_t service_voice_wake_open(void);

/**
 * @brief 关闭唤醒前端（销毁 AFE 实例并释放模型列表）
 */
void service_voice_wake_close(void);

/**
 * @brief 唤醒前端是否已打开
 */
bool service_voice_wake_is_active(void);

/**
 * @brief 每次 feed 所需的单通道采样数（16kHz），未打开时返回 0
 */
int32_t service_voice_wake_get_feed_chunk(void);

/**
 * @brief 每次 fetch 产出的单通道采样数（16kHz），未打开时返回 0
 */
int32_t service_voice_wake_get_fetch_chunk(void);

/**
 * @brief 向 AFE 喂入一个 feed 块
 *
 * Contract: pcm 长度必须恰为 xiaozhi_wake_get_feed_chunk() 个采样。
 *
 * @param[in] pcm 16kHz 单声道 PCM
 * @return ESP_OK 成功
 */
esp_err_t service_voice_wake_feed(const int16_t *pcm);

/**
 * @brief 非阻塞轮询 AFE 输出（从独立 fetch 任务的结果队列取）
 *
 * 调用方仅需持续 feed，DSP 推理在独立任务中执行。
 * 每次 poll 最多取一帧；多帧积压时连续调用即可。
 *
 * @param[out] out 输出结果（data 指针有效期至下一次 poll）
 * @return true 取到数据；false 队列空
 */
bool service_voice_wake_poll(service_voice_wake_result_t *out);

/**
 * @brief 获取当前唤醒词文本（AFE 可用时）
 * @return 唤醒词字符串；不可用时返回 NULL
 */
const char *service_voice_wake_get_word(void);

/**
 * @brief 启停 WakeNet 检出（检出关闭后 fetch 结果不再携带唤醒命中）
 *
 * 命中后 esp-sr 的检出状态不会自复位，必须 disable/enable 一次才能清除
 * （上游 xiaozhi 检出即停 WakeNet 的等价物）。实际启停延迟到 fetch 任务
 * 上下文执行，调用方任意任务可调用。
 */
void service_voice_wake_set_detection(bool enable);

/**
 * @brief 请求重置 AFE 环形缓冲并清空结果队列（fetch 任务上下文异步执行）
 *
 * 丢弃 mic 关闭期间滞留的旧音频，防重新使能后对旧语音重复检出。
 */
void service_voice_wake_reset_buffer(void);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_VOICE_WAKE_H */
