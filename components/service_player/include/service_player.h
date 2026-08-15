/**
 * @file service_player.h
 * @brief 通用音频播放服务：封装 micro-mp3 解码器，对外暴露纯 C API
 *
 * Why: micro-mp3 是 C++ 流式 MP3 解码器，App 层（C）不应直接触碰 C++ 对象；
 * 本服务把「打开文件 → ID3v2 标题解析 → 流式解码 → 线性插值重采样到
 * 44.1kHz 立体声 → 写入 service_audio aux 混音流」整条链路封装在 C++ 侧，
 * 对 App 只暴露 load/play/pause/stop/poll 与元数据查询的 C 接口。
 *
 * Contract: 播放推进由调用方（App 的 on_update，task_app 上下文）周期性调用
 * service_player_poll() 驱动；解码产生的 PCM 经 service_audio_aux_write 混音，
 * aux 为 SPSC 单生产者缓冲，播放期间不应与其它 aux 生产者（如 AI TTS）并发。
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化播放服务（分配缓冲，可重复调用）
 */
esp_err_t service_player_init(void);

/**
 * @brief 加载并打开 MP3 文件（解析 ID3v2 标题 + 探测解码格式）
 * @param[in] path 完整文件路径（如 /sdcard/music/xxx.mp3）
 * @return ESP_OK 成功；ESP_ERR_NOT_FOUND 打不开；ESP_ERR_INVALID_STATE 已加载
 */
esp_err_t service_player_load(const char *path);

/**
 * @brief 卸载当前文件并复位（停止播放、释放资源）
 */
void service_player_unload(void);

/**
 * @brief 开始播放（从当前位置，未加载时返回错误）
 */
esp_err_t service_player_play(void);

/**
 * @brief 暂停播放（保留位置，可继续）
 */
esp_err_t service_player_pause(void);

/**
 * @brief 停止播放并清空 aux 缓冲（回到已加载状态）
 */
void service_player_stop(void);

/**
 * @brief 推进解码：读文件 → 解码 → 重采样 → 写 aux，直到缓冲积压或文件读完
 *
 * 应在 PLAYING 状态下由 App 每周期调用；完播后自动停止并解除 aux 预充。
 *
 * @return ESP_OK 正常；ESP_ERR_INVALID_STATE 未在播放
 */
esp_err_t service_player_poll(void);

/**
 * @brief 获取播放标题（ID3v2 TIT2 解析，缺失回退文件名）
 * @return UTF-8 字符串，未加载时返回 NULL
 */
const char *service_player_get_title(void);

/**
 * @brief 获取解码采样率（Hz，探测前为 0）
 */
uint32_t service_player_get_sample_rate(void);

/**
 * @brief 获取解码声道数（1/2，探测前为 0）
 */
uint8_t service_player_get_channels(void);

/**
 * @brief 当前是否处于播放推进状态
 */
bool service_player_is_playing(void);

/**
 * @brief 是否已成功加载文件
 */
bool service_player_is_loaded(void);

/**
 * @brief 当前播放位置（微秒）
 */
int64_t service_player_get_position_us(void);

/**
 * @brief 估算总时长（微秒，按文件大小与平均码率估算，VBR 存在误差）
 */
int64_t service_player_get_duration_us(void);

#ifdef __cplusplus
}
#endif