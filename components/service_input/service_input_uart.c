/**
 * @file service_input_uart.c
 * @brief UART MIDI transport 实现
 *
 * 不再创建独立接收任务；UART 驱动通过事件队列通知，
 * 由 task_input 周期调用 service_input_uart_process() 统一处理。
 *
 * Why: 无 UART MIDI 引脚的板型由 CONFIG_BOARD_HAS_UART_MIDI 编译期门控，
 * 对外符号保留并无害降级（init 返回 NOT_SUPPORTED，process/deinit 空操作）。
 */

#include "service_input_uart.h"
#include "sdkconfig.h"
#include "esp_log.h"

static const char *TAG = "input_uart";

#if CONFIG_BOARD_HAS_UART_MIDI
#include "engine_midi.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define UART_MIDI_NUM           CONFIG_SERVICE_INPUT_UART_NUM
#define UART_MIDI_TX_PIN        CONFIG_SERVICE_INPUT_UART_TX_PIN
#define UART_MIDI_RX_PIN        CONFIG_SERVICE_INPUT_UART_RX_PIN
#define UART_MIDI_BAUDRATE      CONFIG_SERVICE_INPUT_UART_BAUDRATE
#define UART_MIDI_BUF_SIZE      256
#define UART_MIDI_QUEUE_LEN     16

static QueueHandle_t s_uart_queue = NULL;
static bool s_initialized = false;

static void uart_midi_out_handler(const engine_midi_event_t *evt, void *user_data);
static uint8_t encode_midi_event(const engine_midi_event_t *evt, uint8_t *out_buf);

esp_err_t service_input_uart_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    uart_config_t uart_cfg = {
        .baud_rate = UART_MIDI_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_param_config(UART_MIDI_NUM, &uart_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_set_pin(UART_MIDI_NUM, UART_MIDI_TX_PIN, UART_MIDI_RX_PIN,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_driver_install(UART_MIDI_NUM, UART_MIDI_BUF_SIZE * 2, 0,
                              UART_MIDI_QUEUE_LEN, &s_uart_queue, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 订阅输出事件，实现双工。 */
    ret = engine_midi_subscribe(
        ENGINE_MIDI_MASK_NOTE_ON |
        ENGINE_MIDI_MASK_NOTE_OFF |
        ENGINE_MIDI_MASK_CONTROL_CHANGE |
        ENGINE_MIDI_MASK_PROGRAM_CHANGE |
        ENGINE_MIDI_MASK_PITCH_BEND |
        ENGINE_MIDI_MASK_CHANNEL_PRESSURE |
        ENGINE_MIDI_MASK_POLY_PRESSURE,
        0xFFFF,
        uart_midi_out_handler,
        NULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "subscribe output failed: %s", esp_err_to_name(ret));
    }

    s_initialized = true;
    ESP_LOGI(TAG, "UART MIDI initialized on UART%d TX=%d RX=%d baud=%d",
             UART_MIDI_NUM, UART_MIDI_TX_PIN, UART_MIDI_RX_PIN, UART_MIDI_BAUDRATE);
    return ESP_OK;
}

void service_input_uart_deinit(void)
{
    if (!s_initialized) {
        return;
    }

    engine_midi_unsubscribe(uart_midi_out_handler);

    uart_driver_delete(UART_MIDI_NUM);
    s_uart_queue = NULL;
    s_initialized = false;
}

void service_input_uart_process(void)
{
    if (s_uart_queue == NULL) {
        return;
    }

    uart_event_t event;
    while (xQueueReceive(s_uart_queue, &event, 0) == pdTRUE) {
        if (event.type == UART_DATA ||
            event.type == UART_BUFFER_FULL ||
            event.type == UART_FIFO_OVF) {

            uint8_t buf[16];
            int len;
            while ((len = uart_read_bytes(UART_MIDI_NUM, buf, sizeof(buf), 0)) > 0) {
                engine_midi_feed_stream_from_port(buf, (uint32_t)len, ENGINE_MIDI_PORT_UART);
            }
        }
    }
}

static void uart_midi_out_handler(const engine_midi_event_t *evt, void *user_data)
{
    (void)user_data;

    /* 忽略来自 UART 自身的事件，防止回环。 */
    if (evt->source_port == ENGINE_MIDI_PORT_UART) {
        return;
    }

    uint8_t buf[4];
    uint8_t len = encode_midi_event(evt, buf);
    if (len > 0) {
        uart_write_bytes(UART_MIDI_NUM, (const char *)buf, len);
    }
}

static uint8_t encode_midi_event(const engine_midi_event_t *evt, uint8_t *out_buf)
{
    if (evt == NULL || out_buf == NULL) {
        return 0;
    }

    switch (evt->type) {
    case ENGINE_MIDI_MSG_NOTE_OFF:
        out_buf[0] = (uint8_t)(ENGINE_MIDI_MSG_NOTE_OFF | (evt->channel & 0x0F));
        out_buf[1] = evt->data1;
        out_buf[2] = evt->data2;
        return 3;

    case ENGINE_MIDI_MSG_NOTE_ON:
        out_buf[0] = (uint8_t)(ENGINE_MIDI_MSG_NOTE_ON | (evt->channel & 0x0F));
        out_buf[1] = evt->data1;
        out_buf[2] = evt->data2;
        return 3;

    case ENGINE_MIDI_MSG_POLY_PRESSURE:
        out_buf[0] = (uint8_t)(ENGINE_MIDI_MSG_POLY_PRESSURE | (evt->channel & 0x0F));
        out_buf[1] = evt->data1;
        out_buf[2] = evt->data2;
        return 3;

    case ENGINE_MIDI_MSG_CONTROL_CHANGE:
        out_buf[0] = (uint8_t)(ENGINE_MIDI_MSG_CONTROL_CHANGE | (evt->channel & 0x0F));
        out_buf[1] = evt->data1;
        out_buf[2] = evt->data2;
        return 3;

    case ENGINE_MIDI_MSG_PROGRAM_CHANGE:
        out_buf[0] = (uint8_t)(ENGINE_MIDI_MSG_PROGRAM_CHANGE | (evt->channel & 0x0F));
        out_buf[1] = evt->data1;
        return 2;

    case ENGINE_MIDI_MSG_CHANNEL_PRESSURE:
        out_buf[0] = (uint8_t)(ENGINE_MIDI_MSG_CHANNEL_PRESSURE | (evt->channel & 0x0F));
        out_buf[1] = evt->data1;
        return 2;

    case ENGINE_MIDI_MSG_PITCH_BEND:
        out_buf[0] = (uint8_t)(ENGINE_MIDI_MSG_PITCH_BEND | (evt->channel & 0x0F));
        out_buf[1] = evt->value & 0x7F;
        out_buf[2] = (evt->value >> 7) & 0x7F;
        return 3;

    default:
        return 0;
    }
}

#else /* !CONFIG_BOARD_HAS_UART_MIDI */

/* 无 UART MIDI 引脚：对外符号无害降级 */
esp_err_t service_input_uart_init(void)
{
    ESP_LOGW(TAG, "board has no UART MIDI pins, transport disabled");
    return ESP_ERR_NOT_SUPPORTED;
}

void service_input_uart_deinit(void)
{
}

void service_input_uart_process(void)
{
}

#endif /* CONFIG_BOARD_HAS_UART_MIDI */
