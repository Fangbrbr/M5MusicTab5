/**
 * @file service_wavrec.h
 * @brief WAV 录音/回放服务（ES7210 mic → SD 卡 /sdcard/wav/ 目录）
 *
 * 三种录音模式：
 * - VOICE      16kHz mono，AFE 管线旁路原始 mic tap（不携 AEC 处理痕迹），AI 共存
 * - AMBIENT    16kHz mono，AFE 输出 tap（AEC 后，抑制扬声器串音），AI 共存
 * - INSTRUMENT 44.1kHz stereo 独占 mic（须关 AFE 释放 mic），录音期 AI 挂起、
 *              扬声器静音（半双工），结束后 AFE 尽力重开（内部 RAM 不足则
 *              唤醒退化为按住说话直至重启，日志告警）
 *
 * 回放：RIFF/WAVE PCM16（8-48kHz mono/stereo）线性插值重采样到 44.1k stereo，
 * 经 service_audio_aux_write 混音输出；回放期挂起 AI（aux 为 SPSC 单生产者，
 * 与 TTS 互斥），SF2 主源不动（静默时不冲突）。
 *
 * Contract: 控制面 API（start/stop/play/play_stop/process）仅在 task_app
 * 上下文（App 生命周期回调）调用；数据面由内部 tap/采集任务驱动。
 */

#ifndef SERVICE_WAVREC_H
#define SERVICE_WAVREC_H

#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SERVICE_WAVREC_MODE_VOICE = 0,   /**< 语音：16k mono 原始 mic，省空间 */
    SERVICE_WAVREC_MODE_AMBIENT,     /**< 环境：16k mono AEC 后 */
    SERVICE_WAVREC_MODE_INSTRUMENT,  /**< 乐器：44.1k stereo 高码率 */
} service_wavrec_mode_t;

typedef enum {
    SERVICE_WAVREC_OK = 0,
    SERVICE_WAVREC_ERR_NO_SD,        /**< SD 卡未挂载 */
    SERVICE_WAVREC_ERR_BUSY,         /**< 录音/回放进行中 */
    SERVICE_WAVREC_ERR_NO_VOICE_FE,  /**< 语音前端（AFE）不可用 */
    SERVICE_WAVREC_ERR_MIC,          /**< mic 打开失败 */
    SERVICE_WAVREC_ERR_FILE,         /**< 文件创建/写入失败 */
    SERVICE_WAVREC_ERR_STATE,        /**< 状态不允许 */
    SERVICE_WAVREC_ERR_BAD_FILE,     /**< 非法/不支持的 WAV 文件 */
} service_wavrec_err_t;

/**
 * @brief 初始化服务（分配 PSRAM 环形缓冲、创建采集任务、建 wav 目录）
 */
esp_err_t service_wavrec_init(void);

/**
 * @brief 开始录音
 *
 * VOICE/AMBIENT 立即进入录音；INSTRUMENT 先挂起 AI+关 AFE 进入准备态，
 * 由 service_wavrec_process() 约 60ms 后完成 mic 重配真正开录。
 */
service_wavrec_err_t service_wavrec_start(service_wavrec_mode_t mode);

/**
 * @brief 停止录音并落盘（INSTRUMENT 模式异步收尾，几十 ms 内完成）
 */
void service_wavrec_stop(void);

/** @brief 正在录音（不含准备/收尾） */
bool service_wavrec_is_recording(void);

/** @brief 录音链路占用中（准备/录音/收尾），期间禁止播放与再次录音 */
bool service_wavrec_is_rec_busy(void);

/** @brief 已录时长（按已落盘数据量折算） */
uint32_t service_wavrec_get_elapsed_ms(void);

/** @brief 最近一次成功落盘文件绝对路径 */
bool service_wavrec_get_last_path(char *buf, size_t len);

/**
 * @brief 播放 WAV 文件（挂起 AI，播完或 play_stop 后恢复）
 */
service_wavrec_err_t service_wavrec_play(const char *path);

void service_wavrec_play_stop(void);

bool service_wavrec_is_playing(void);

uint32_t service_wavrec_play_get_pos_ms(void);

uint32_t service_wavrec_play_get_total_ms(void);

/**
 * @brief 取走最近一次异步错误（读后清零）
 */
service_wavrec_err_t service_wavrec_take_error(void);

/**
 * @brief 当前录音/回放瞬时电平（0-100，数据面逐块更新，任意上下文可读）
 *
 * 供电平柱等 UI 轮询；录音为增益后信号（可见削顶），回放着色同听感。
 */
uint8_t service_wavrec_get_level(void);

/**
 * @brief 周期处理，由 task_app 每 10ms 调用（落盘泵/回放泵/状态机收尾）
 */
void service_wavrec_process(void);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_WAVREC_H */
