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
 * Trap: 上游 loadSf2File 进入即 parser.clear() 释放旧音色，加载失败
 * 后引擎处于无音色静默态，调用方必须自行 fallback（如改载内部预设）。
 *
 * @param[in] path 绝对路径（/sdcard/... 或 /sys_int/...）
 * @return true 加载成功
 */
bool engine_sf2_load_file(const char *path);

/* -------------------- SD 卡音源选择（设置页 setting_sf2_source 后端） -------------------- */

/** SD 卡音源目录与单文件大小上限：PSRAM 被显示/AFE/WS 等多方共享。
 * 16 MB 是文件级硬上限；真正能否加载由 engine_sf2_check_fit 按加载时刻的
 * 实际剩余 PSRAM 动态判定（需 ≥ 文件大小 + 安全保留，否则拒绝以防功能缺失） */
#define ENGINE_SF2_SD_DIR            "/sdcard/soundfonts"
#define ENGINE_SF2_SD_MAX_BYTES      (16u * 1024u * 1024u)
#define ENGINE_SF2_SD_NAME_MAX_LEN   96
#define ENGINE_SF2_SD_MAX_FILES      32

/**
 * @brief 预检音源文件当前能否加载（PSRAM 预算闸门）
 *
 * Why: 大音源加载后占用 PSRAM，可能导致 Zen/鼓组等 App 的 canvas 分配失败、
 * 功能缺失。加载前先校验：剩余 PSRAM 需 ≥ 文件大小 + 安全保留（默认 3 MB，
 * 覆盖最大 App canvas + 余量），否则拒绝并保留当前音源。
 *
 * @param[in] path 绝对路径
 * @return true 可加载；false 内存不足
 */
bool engine_sf2_check_fit(const char *path);

/**
 * @brief 重新扫描 SD 卡音源目录，缓存 .sf2 文件名列表（按名字序）
 * @return 可用文件数（无卡/无目录/无文件返回 0）
 */
int engine_sf2_sd_rescan(void);

/**
 * @brief 取扫描缓存中第 index 个文件名（不含路径）
 * @return 文件名指针（内部缓存，下次 rescan 失效）；越界返回 NULL
 */
const char *engine_sf2_sd_name_at(int index);

/**
 * @brief 当前生效音源
 * @return SD 文件名（不含路径）；内部预设返回空字符串 ""
 */
const char *engine_sf2_current_source(void);

/**
 * @brief 指定开机加载的音源（main 在 activate 前按 NVS 调用一次）
 * @param[in] sd_name SD 文件名；NULL/空串 = 内部预设
 */
void engine_sf2_set_boot_source(const char *sd_name);

/**
 * @brief 加载内部预设（SPIFFS 内置音色）
 * @return true 加载成功
 */
bool engine_sf2_load_internal(void);

/**
 * @brief 按文件名加载 SD 卡音源（含 16 MB 上限检查）
 * @param[in] sd_name 文件名（不含路径）
 * @return true 加载成功；文件缺失/超限/解析失败返回 false
 */
bool engine_sf2_load_sd(const char *sd_name);

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
