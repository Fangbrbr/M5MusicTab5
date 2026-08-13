/**
 * @file board_tab5_audio.c
 * @brief M5Stack Tab5 音频 codec 板级实现
 *
 * 纯转调 espressif__m5stack_tab5 BSP，不含业务逻辑。
 * ES8388（扬声器/耳机）与 ES7210（双麦）共用一组 I2S，
 * 由 BSP 在首个 codec init 时完成 I2S 初始化。
 */

#include "board_hal.h"
#include "bsp/m5stack_tab5.h"
#include "esp_log.h"

static const char *TAG = "board_tab5_audio";

esp_codec_dev_handle_t board_audio_speaker_codec_init(void)
{
    return bsp_audio_codec_speaker_init();
}

esp_codec_dev_handle_t board_audio_mic_codec_init(void)
{
    return bsp_audio_codec_microphone_init();
}

esp_err_t board_audio_speaker_pa_set(bool enable)
{
    /* Contract: 仅控制扬声器功放通断，不触碰耳机通路；
     * 耳机插入/拔出时的路由决策由调用方（service_audio）完成。 */
    return bsp_feature_enable(BSP_FEATURE_SPEAKER, enable);
}
