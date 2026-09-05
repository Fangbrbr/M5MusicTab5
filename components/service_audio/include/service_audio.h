/**
 * @file service_audio.h
 * @brief 音频服务
 *
 * 主音频源为 SF2 采样合成器（engine_sf2 注册渲染），另提供辅助混音流
 * 供 TTS/MP3 等第二发声通道使用；混音出口统一软限幅。
 * 本服务负责管理 I2S/Codec 输出、系统音量、麦克风录音。
 */

#ifndef SERVICE_AUDIO_H
#define SERVICE_AUDIO_H

#include "esp_err.h"
#include "stdint.h"
#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 音频源类型（当前仅支持 SF2 采样合成器） */
typedef enum {
    AUDIO_SOURCE_NONE = 0,        /**< 无音频源 */
    AUDIO_SOURCE_SF2,             /**< SF2 采样合成器 */
    AUDIO_SOURCE_COUNT            /**< 源数量 */
} audio_source_t;

/** @brief 音频源操作接口 */
typedef struct {
    audio_source_t source;        /**< 源类型标识 */
    const char    *name;          /**< 源名称 */

    /**
     * @brief 初始化音频源
     * @return ESP_OK 成功
     */
    esp_err_t (*init)(void);

    /**
     * @brief 反初始化音频源，释放资源
     */
    void (*deinit)(void);

    /**
     * @brief 渲染一帧立体声 PCM（float 域）
     *
     * 输出名义 [-1,1] 的 float；限幅/电平收口由 service_audio 统一完成，
     * 源内不做裁剪。
     *
     * @param[out] buffer_lr 立体声 float 缓冲区（LR 交错）
     * @param[in]  frames    采样帧数
     */
    void (*render_stereo)(float *buffer_lr, uint32_t frames);

    /**
     * @brief 当前源是否已初始化完成
     * @return true 已初始化
     */
    bool (*is_ready)(void);

    /**
     * @brief 加载音色/音频文件到当前源（可选）
     * @param[in] path 绝对路径
     * @return true 加载成功
     */
    bool (*load_file)(const char *path);
} audio_source_ops_t;

/**
 * @brief 初始化音频服务
 *
 * 初始化 BSP I2S/Codec。音频源由 engine_sf2 注册。
 */
esp_err_t service_audio_init(void);

/**
 * @brief 注册音频源
 * @param[in] ops 音频源操作接口
 */
esp_err_t service_audio_register_source(const audio_source_ops_t *ops);

/**
 * @brief 激活已注册的 SF2 音频源
 * @return ESP_OK 成功
 */
esp_err_t service_audio_activate_sf2(void);

/**
 * @brief 停用 SF2 音频源（主路径静音，但不销毁源/不卸载音色）
 *
 * MP3 播放期间隔离合成链路用：仅把活跃源置为 NONE，停止调用 render_stereo，
 * 保留 SF2 源注册与音色缓存，恢复时 activate_sf2 走轻量路径（engine_sf2_init
 * 因 initialized 缓存直接返回，无需重载音色）。
 *
 * @return ESP_OK 成功；ESP_ERR_INVALID_STATE 当前活跃源不是 SF2
 */
esp_err_t service_audio_deactivate_sf2(void);

/**
 * @brief 获取当前激活的音频源
 * @return 当前音频源
 */
audio_source_t service_audio_get_active_source(void);

/**
 * @brief 音频处理入口
 *
 * 由 task_audio 周期性调用，渲染当前源并输出到 I2S。
 */
void service_audio_process(void);

/**
 * @brief 获取音频服务采样率
 * @return 采样率 Hz
 */
uint32_t service_audio_get_sample_rate(void);

/**
 * @brief 获取每周期渲染帧数
 * @return 帧数
 */
uint32_t service_audio_get_frames_per_period(void);

/**
 * @brief 设置系统主音量
 * @param[in] volume 音量 0-100
 * @return ESP_OK 成功
 */
esp_err_t service_audio_set_volume(int32_t volume);

/**
 * @brief 获取系统主音量
 * @return 音量 0-100
 */
int32_t service_audio_get_volume(void);

/**
 * @brief 向辅助混音流写入 PCM（44.1kHz 立体声 int16 交错）
 *
 * 第二发声通道（AI 语音 TTS 回放、后续 MP3 播放）的入口：写入的 PCM
 * 与主音频源（SF2 合成器）在渲染出口混音，共享软限幅与 I2S 输出。
 *
 * Contract: 44.1kHz 立体声 int16 交错格式，采样率/声道转换由生产者负责；
 * SPSC 无锁环形缓冲，仅允许单一生产者；缓冲已满时截断本次写入。
 *
 * @param[in] data   PCM 数据
 * @param[in] frames 帧数
 * @return 实际写入帧数
 */
uint32_t service_audio_aux_write(const int16_t *data, uint32_t frames);

/**
 * @brief 查询辅助混音流剩余可写空间（帧）
 *
 * 供 TTS 生产者做反压节流：空间不足时暂缓解码，让积压倒灌回 WS 事件队列
 * （编码域缓冲，比解码后 PCM 省约 50 倍内存）。
 *
 * @return 可写入帧数（SPSC 快照，保守值；缓冲未分配时为 0）
 */
uint32_t service_audio_aux_free_frames(void);

/**
 * @brief 查询辅助混音流是否已排空（生产/消费指针重合）
 *
 * 仅判指针重合；预充状态下缓冲可能积压未播，调用方需配合超时兜底。
 *
 * @return true 已排空（或缓冲未分配）
 */
bool service_audio_aux_is_idle(void);

/**
 * @brief 取输出级遥测（读后清零）：峰值绝对值与软限幅拐点超阈样本数
 *
 * 杂音定位判据：轻弹单音时 peak 应与 SF2 渲染 peak×32767 同量级；
 * 若远超（随机满幅）= 混音/缓冲数字域注入，若同量级但听感杂音 = 波形畸变/模拟域。
 *
 * @param[out] peak_abs 距上次读取以来的输出峰值绝对值（可 NULL）
 * @param[out] knee_cnt 距上次读取以来超软限幅拐点的样本数（可 NULL）
 */
void service_audio_get_out_stats(int16_t *peak_abs, uint32_t *knee_cnt);

/**
 * @brief 清空辅助混音流（立即丢弃已缓冲数据并重新预充）
 *
 * Why: 小智打断场景需立即终止 TTS 尾巴，避免旧语音与新 listen 混叠。
 */
void service_audio_aux_clear(void);

/**
 * @brief 标记辅助流已结束（tts_stop）：一次性解除预充门，立即排出残余尾音
 *
 * Why: 预充门（攒够 ~400ms 才出声）对"不会再有新数据"的流末尾是死锁——
 * 尾音不足门限会永久卡住，跨轮残留成下一轮开头的插播。tts_stop 时调用。
 * 幂等；仅置位标志，由 Core 1 渲染侧应用，任意任务上下文可调。
 */
void service_audio_aux_end_of_stream(void);

/**
 * @brief 启动 AEC 参考信号采集
 *
 * 混音出口的立体声 PCM 会实时下混为单声道并写入 PSRAM 环形缓冲，
 * 供 AEC 前端通过 service_audio_aec_ref_read() 读取。重复调用幂等。
 * 仅在全双工路径（mic 与扬声器同采样率）中启用。
 */
void service_audio_aec_ref_start(void);

/**
 * @brief 停止 AEC 参考信号采集
 */
void service_audio_aec_ref_stop(void);

/**
 * @brief 从参考信号环形缓冲读取 44.1kHz 单声道 PCM
 *
 * 非阻塞；返回实际读到的帧数（可能小于请求）。调用方需自行保证与
 * 麦克风读取的采样数对齐。
 *
 * @param[out] buffer 单声道采样缓冲
 * @param[in]  frames  希望读取的帧数
 * @return 实际读取帧数
 */
uint32_t service_audio_aec_ref_read(int16_t *buffer, uint32_t frames);

/**
 * @brief 查询参考信号缓冲当前可读取帧数
 * @return 可读帧数
 */
uint32_t service_audio_aec_ref_available(void);

/**
 * @brief 打开麦克风录音流
 *
 * 录音期间会临时关闭扬声器 I2S 流，关闭麦克风后自动恢复。
 *
 * @param[in] sample_rate 采样率，如 16000
 * @param[in] channels    声道数，如 1
 * @return ESP_OK 成功
 */
esp_err_t service_audio_mic_open(uint32_t sample_rate, uint8_t channels);

/**
 * @brief 从麦克风读取 PCM 采样
 *
 * @param[out] buffer 采样缓冲区（按 channels 交错）
 * @param[in]  frames  希望读取的采样帧数
 * @return 实际读取的帧数，<0 表示错误
 */
int32_t service_audio_mic_read(int16_t *buffer, uint32_t frames);

/**
 * @brief 关闭麦克风录音流并恢复扬声器输出
 */
void service_audio_mic_close(void);

/**
 * @brief 设置 mic 输入增益
 *
 * Why: mic_open 每次固定写 32dB 语音向增益；乐器等大声压场景需调低防削波。
 * 必须在 mic_open 成功后调用（open 内部会重置增益）。
 *
 * @param[in] gain_db 增益 dB（ES7210 约 0~37.5dB）
 * @return ESP_OK 成功；ESP_ERR_INVALID_STATE mic 未打开
 */
esp_err_t service_audio_mic_set_gain(float gain_db);

/**
 * @brief mic 是否处于打开状态（供独占重配前的释放等待）
 */
bool service_audio_mic_is_open(void);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_AUDIO_H */
