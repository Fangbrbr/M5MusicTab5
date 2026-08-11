/**
 * @file service_audio_config.h
 * @brief 音频服务公共配置
 *
 * 采样率与渲染帧长的唯一权威定义。service_audio 自身与所有音频源
 * （engine_sf2 等）必须从此处取值，禁止各自重复定义。
 */

#ifndef SERVICE_AUDIO_CONFIG_H
#define SERVICE_AUDIO_CONFIG_H

/** @brief 系统音频采样率 Hz */
#define SERVICE_AUDIO_SAMPLE_RATE        44100

/** @brief 每个渲染周期帧数（约 1.45ms @44.1kHz） */
#define SERVICE_AUDIO_FRAMES_PER_PERIOD  64

#endif /* SERVICE_AUDIO_CONFIG_H */
