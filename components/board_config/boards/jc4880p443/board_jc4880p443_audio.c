/**
 * @file board_jc4880p443_audio.c
 * @brief Guition JC4880P443 音频 codec 板级实现
 *
 * ES8311（DAC+ADC 单芯片）挂 I2S_NUM_0：MCLK13/BCLK12/WS10/DOUT9/DIN48，
 * 控制口走共享 I2C_NUM_1；PA 功放使能脚 GPIO11。
 * 采样率/位宽由调用方（service_audio，44.1kHz 16bit）在 esp_codec_dev_open 时下发，
 * 此处仅给出可工作的 I2S 初始时钟。
 */

#include "board_hal.h"

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_codec_dev_defaults.h"
#include "es8311_codec.h"
#include "esp_log.h"

static const char *TAG = "board_jc_audio";

#define JC_I2S_PORT         I2S_NUM_0
#define JC_I2S_MCLK_GPIO    GPIO_NUM_13
#define JC_I2S_BCLK_GPIO    GPIO_NUM_12
#define JC_I2S_WS_GPIO      GPIO_NUM_10
#define JC_I2S_DOUT_GPIO    GPIO_NUM_9
#define JC_I2S_DIN_GPIO     GPIO_NUM_48
#define JC_PA_GPIO          GPIO_NUM_11
#define JC_I2S_INIT_RATE_HZ 44100

static i2s_chan_handle_t s_i2s_tx = NULL;
static i2s_chan_handle_t s_i2s_rx = NULL;
static const audio_codec_data_if_t *s_data_if = NULL;

static esp_err_t board_audio_i2s_init(void);

esp_codec_dev_handle_t board_audio_speaker_codec_init(void)
{
    if (s_data_if == NULL) {
        if (board_i2c_init() != ESP_OK) {
            ESP_LOGE(TAG, "i2c init failed");
            return NULL;
        }
        if (board_audio_i2s_init() != ESP_OK) {
            return NULL;
        }
    }

    /* PA 引脚由板级独占管理（board_audio_speaker_pa_set），默认使能；
     * Why: 不传 pa_pin 给 es8311 设备，避免 codec 开关流与板级 PA 控制双写同一脚 */
    gpio_config_t pa_cfg = {
        .pin_bit_mask = 1ULL << JC_PA_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    if (gpio_config(&pa_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "pa gpio config failed");
        return NULL;
    }
    gpio_set_level(JC_PA_GPIO, 1);

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    if (gpio_if == NULL) {
        ESP_LOGE(TAG, "new gpio if failed");
        return NULL;
    }

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = I2C_NUM_1,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = board_i2c_get_handle(),
    };
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    if (ctrl_if == NULL) {
        ESP_LOGE(TAG, "new i2c ctrl failed");
        return NULL;
    }

    /* ES8311 单芯片同时含 ADC/DAC，按 BOTH 建 codec 接口，ADC 留待 mic 路径启用 */
    es8311_codec_cfg_t codec_cfg = {
        .ctrl_if = ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
        .pa_pin = -1,
        .pa_reverted = false,
        .master_mode = false,
        .use_mclk = true,
        .hw_gain = {
            .pa_voltage = 5.0,
            .codec_dac_voltage = 3.3,
        },
    };
    const audio_codec_if_t *codec_if = es8311_codec_new(&codec_cfg);
    if (codec_if == NULL) {
        ESP_LOGE(TAG, "es8311_codec_new failed");
        return NULL;
    }

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = codec_if,
        .data_if = s_data_if,
    };
    esp_codec_dev_handle_t dev = esp_codec_dev_new(&dev_cfg);
    if (dev == NULL) {
        ESP_LOGE(TAG, "esp_codec_dev_new failed");
        return NULL;
    }
    ESP_LOGI(TAG, "ES8311 speaker codec ready");
    return dev;
}

esp_codec_dev_handle_t board_audio_mic_codec_init(void)
{
    /* Why: jc v1 BOARD_HAS_MIC=n，mic 路径不启用；ES8311 的 ADC 已随 I2S DIN
     * 接线与 BOTH 模式预留，后续启用 mic 时在此返回 ESP_CODEC_DEV_TYPE_IN 设备。
     * 调用方（service_audio）对 NULL 有既有失败路径兜底。 */
    return NULL;
}

esp_err_t board_audio_speaker_pa_set(bool enable)
{
    /* Contract: 仅控制扬声器功放通断；jc 无耳机孔，调用方保持恒使能 */
    return gpio_set_level(JC_PA_GPIO, enable ? 1 : 0);
}

static esp_err_t board_audio_i2s_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(JC_I2S_PORT, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    esp_err_t ret = i2s_new_channel(&chan_cfg, &s_i2s_tx, &s_i2s_rx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %d", ret);
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(JC_I2S_INIT_RATE_HZ),
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = JC_I2S_MCLK_GPIO,
            .bclk = JC_I2S_BCLK_GPIO,
            .ws = JC_I2S_WS_GPIO,
            .dout = JC_I2S_DOUT_GPIO,
            .din = JC_I2S_DIN_GPIO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    ret = i2s_channel_init_std_mode(s_i2s_tx, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s tx init failed: %d", ret);
        return ret;
    }
    ret = i2s_channel_init_std_mode(s_i2s_rx, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s rx init failed: %d", ret);
        return ret;
    }
    ret = i2s_channel_enable(s_i2s_tx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s tx enable failed: %d", ret);
        return ret;
    }
    ret = i2s_channel_enable(s_i2s_rx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s rx enable failed: %d", ret);
        return ret;
    }

    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = JC_I2S_PORT,
        .rx_handle = s_i2s_rx,
        .tx_handle = s_i2s_tx,
    };
    s_data_if = audio_codec_new_i2s_data(&i2s_cfg);
    if (s_data_if == NULL) {
        ESP_LOGE(TAG, "new i2s data if failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}
