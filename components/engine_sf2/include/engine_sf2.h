/**
 * @file engine_sf2.h
 * @brief SF2 采样合成引擎 C API
 *
 * 对项目暴露纯 C 接口，内部封装上游 SF2Sampler（SF2Parser + Synth）。
 * 作为 service_audio 的常驻主音频源，渲染输出 float 立体声，
 * 限幅/电平收口由 service_audio 统一完成。
 *
 * 线程安全：引擎内部以递归互斥锁保护所有 Synth 入口，
 * Core 0（MIDI 事件）与 Core 1（渲染）可安全并发调用。
 */

#ifndef ENGINE_SF2_H
#define ENGINE_SF2_H

#include "esp_err.h"
#include "stdint.h"
#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 SF2 引擎
 *
 * 创建合成器实例、订阅 MIDI 总线（NOTE_ON/OFF/CC/PC/BEND/SysEx），
 * 并按默认路径探测加载音色（SD 优先、SPIFFS 兜底）；
 * 音色缺失不视为失败，引擎就绪但渲染静默。
 *
 * @return ESP_OK 成功
 */
esp_err_t engine_sf2_init(void);

/**
 * @brief 反初始化 SF2 引擎
 */
void engine_sf2_deinit(void);

/**
 * @brief 渲染一帧立体声 PCM（float 域）
 * @param[out] buffer_lr 立体声 float 缓冲区（LR 交错，名义 [-1,1]）
 * @param[in]  frames    采样帧数
 */
void engine_sf2_render_stereo(float *buffer_lr, uint32_t frames);

/**
 * @brief 判断引擎是否就绪
 * @return true 已就绪（未加载音色同样就绪，渲染静默）
 */
bool engine_sf2_is_ready(void);

/**
 * @brief 加载指定 SF2 文件
 *
 * 与渲染互斥：加载期间渲染暂停。大文件加载耗时数秒，
 * 仅适合启动阶段或用户确认场景调用。
 *
 * @param[in] path 绝对路径（/sdcard/... 或 /sys_int/...）
 * @return true 加载成功
 */
bool engine_sf2_load_file(const char *path);

/**
 * @brief 向 service_audio 注册本引擎为音频源
 */
esp_err_t engine_sf2_register_source(void);

/**
 * @brief SF2 加载进度回调类型
 * @param[in] percent   0-100 的加载百分比
 * @param[in] user_data 用户数据
 */
typedef void (*engine_sf2_progress_cb_t)(int percent, void *user_data);

/**
 * @brief 设置 SF2 加载进度回调
 * @param[in] cb        回调函数，可为 NULL
 * @param[in] user_data 传给回调的用户数据
 */
void engine_sf2_set_progress_callback(engine_sf2_progress_cb_t cb, void *user_data);

/**
 * @brief 调试信息快照
 */
typedef struct {
    uint16_t max_voices;          /**< 复音池总大小 */
    uint16_t active_voices;       /**< 当前活跃 voice 数 */
    float    render_block_us_avg; /**< 距上次读取以来的平均块渲染耗时 us */
    float    render_block_us_max; /**< 距上次读取以来的最大块渲染耗时 us */
    float    render_peak_amp;     /**< 距上次读取以来的输出峰值幅度（乘 master gain 后） */
} engine_sf2_debug_info_t;

/**
 * @brief 获取渲染遥测快照
 *
 * 读取后清零 avg/max 累计窗口（配合周期打印使用）。
 *
 * @param[out] info 调试信息
 */
void engine_sf2_get_debug_info(engine_sf2_debug_info_t *info);

#ifdef __cplusplus
}
#endif

#endif /* ENGINE_SF2_H */
