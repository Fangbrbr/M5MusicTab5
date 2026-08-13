/**
 * @file service_ws.c
 * @brief 通用 WebSocket 传输服务实现
 *
 * 对 esp_websocket_client 做单例封装；WS 任务上下文只做事件入队，
 * 业务回调在 service_ws_process() 调用方（task_comm）上下文执行。
 */

#include "service_ws.h"

#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "stdio.h"
#include "string.h"

static const char *TAG = "service_ws";

/** @brief WebSocket 文本帧操作码 */
#define SERVICE_WS_OPCODE_TEXT 0x01
/** @brief WebSocket 二进制帧操作码 */
#define SERVICE_WS_OPCODE_BIN  0x02

/** @brief 静态接收缓冲大小（字节） */
#define SERVICE_WS_RX_BUF_STATIC_SIZE 4096
/** @brief PSRAM 动态接收缓冲大小（字节）：大 payload 远超静态 4KB，通道打开时尝试升级 */
#define SERVICE_WS_RX_BUF_DYN_SIZE 16384
/** @brief WS 客户端任务栈（字节）。
 * Trap: xTaskCreate 建栈恒落内部 RAM，须装入内部 largest 块（稳态 ~19KB）；
 * TLS 动态缓冲下实际栈占用低 */
#define SERVICE_WS_TASK_STACK 16384
/** @brief 网络握手/连接超时（ms） */
#define SERVICE_WS_NETWORK_TIMEOUT_MS 10000
/** @brief 内部事件队列深度。
 * Why 64: TTS 下行服务器突发明显（单窗口可达 10x 实时速率），编码包
 * ~200B/60ms 是全链路最廉价的音频缓冲层；64 深约吸收 3.8s 音频突发，
 * 再叠加下方入队阻塞形成的 TCP 反压，正常网络下不丢音频帧 */
#define SERVICE_WS_EVT_QUEUE_LEN 64
/** @brief DATA 事件入队最长阻塞（ms）。ws 任务停顿即 TCP 接收窗口收紧，
 * 形成对服务器发送速率的反压；有界取值保证断链销毁路径不会死等 */
#define SERVICE_WS_EVT_SEND_BLOCK_MS 200
/** @brief 队列满丢包日志限流窗口（ms）：突发期逐条 WARN 会进一步挤占 CPU */
#define SERVICE_WS_DROP_LOG_WINDOW_MS 1000

static esp_websocket_client_handle_t s_client = NULL;
static service_ws_event_cb_t s_cb = NULL;
static void *s_user_data = NULL;
static volatile bool s_connected = false;

static SemaphoreHandle_t s_mutex = NULL;
static StaticSemaphore_t s_mutex_buf;

static QueueHandle_t s_evt_queue = NULL;
static StaticQueue_t s_evt_qstruct;
static uint8_t *s_evt_storage = NULL;

/* 队列满丢包计数与限流时刻（仅 ws 任务上下文访问，无需锁） */
static uint32_t s_drop_count = 0;
static TickType_t s_drop_log_tick = 0;

/* 分片重组缓冲：文本/二进制共用（同一连接的消息按序到达，不会交叉）。
 * 通道打开时懒分配 PSRAM 缓冲，分配失败回退静态 4KB；
 * s_rx_buf_dyn == NULL 即表示使用静态缓冲，释放路径据此幂等 */
static uint8_t s_rx_buf_static[SERVICE_WS_RX_BUF_STATIC_SIZE];
static uint8_t *s_rx_buf_dyn = NULL;

static void service_ws_rx_buf_free(void)
{
    if (s_rx_buf_dyn != NULL) {
        heap_caps_free(s_rx_buf_dyn);
        s_rx_buf_dyn = NULL;
    }
}

/* 清空事件队列并释放其中堆数据（持锁时调用） */
static void service_ws_evt_queue_clear(void)
{
    if (s_evt_queue == NULL) {
        return;
    }
    service_ws_event_t evt;
    while (xQueueReceive(s_evt_queue, &evt, 0) == pdTRUE) {
        if (evt.data != NULL) {
            heap_caps_free(evt.data);
        }
    }
}

static void service_ws_event_handler(void *handler_args, esp_event_base_t base,
                                     int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;
    esp_websocket_event_data_t *event = (esp_websocket_event_data_t *)event_data;

    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "websocket connected");
        s_connected = true;
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
    case WEBSOCKET_EVENT_CLOSED:
        /* CLOSED 与 DISCONNECTED 可能先后到达，经标志位去重只上报一次 */
        if (s_connected) {
            s_connected = false;
            ESP_LOGW(TAG, "websocket disconnected (event %ld)", (long)event_id);
        } else {
            /* 已去重，不再入队 */
            return;
        }
        break;

    case WEBSOCKET_EVENT_DATA:
        if (event->data_len <= 0 || event->payload_len <= 0 || s_evt_queue == NULL) {
            return;
        }
        uint8_t *rx_buf = (s_rx_buf_dyn != NULL) ? s_rx_buf_dyn : s_rx_buf_static;
        const size_t rx_cap = (s_rx_buf_dyn != NULL) ? SERVICE_WS_RX_BUF_DYN_SIZE
                                                     : sizeof(s_rx_buf_static);
        /* 上限含结尾 NUL 的一字节余量：文本帧在 payload_len 处补 '\0' */
        if (event->payload_len >= (int)rx_cap) {
            ESP_LOGW(TAG, "payload too large: %d, dropped", event->payload_len);
            return;
        }
        if (event->payload_offset == 0) {
            memset(rx_buf, 0, rx_cap);
        }
        if (event->payload_offset + event->data_len > (int)rx_cap) {
            ESP_LOGW(TAG, "payload fragment overflow, dropped");
            return;
        }
        memcpy(rx_buf + event->payload_offset, event->data_ptr, event->data_len);
        if (event->payload_offset + event->data_len < event->payload_len) {
            return;
        }
        /* 分片组装完成，分配事件缓冲并入队 */
        {
            service_ws_event_t evt = {0};
            uint32_t alloc_len = (uint32_t)event->payload_len + 1;
            uint8_t *buf = heap_caps_malloc(alloc_len, MALLOC_CAP_SPIRAM);
            if (buf == NULL) {
                ESP_LOGE(TAG, "event data alloc failed, drop %d bytes", event->payload_len);
                return;
            }
            memcpy(buf, rx_buf, event->payload_len);
            buf[event->payload_len] = '\0';
            evt.data = buf;
            evt.len = (uint32_t)event->payload_len;
            if (event->op_code == SERVICE_WS_OPCODE_TEXT) {
                evt.type = SERVICE_WS_EVT_TEXT;
            } else if (event->op_code == SERVICE_WS_OPCODE_BIN) {
                evt.type = SERVICE_WS_EVT_BINARY;
            } else {
                evt.type = SERVICE_WS_EVT_ERROR;
                evt.error_code = -1;
            }
            /* 入队带界阻塞：队列满时 ws 任务在此停顿，TCP 接收窗口收紧
             * 反压服务器发送速率，替代直接丢音频帧；超时仍满才丢（限流告警） */
            if (xQueueSend(s_evt_queue, &evt, pdMS_TO_TICKS(SERVICE_WS_EVT_SEND_BLOCK_MS)) != pdTRUE) {
                heap_caps_free(buf);
                s_drop_count++;
                TickType_t now = xTaskGetTickCount();
                if (now - s_drop_log_tick >= pdMS_TO_TICKS(SERVICE_WS_DROP_LOG_WINDOW_MS)) {
                    ESP_LOGW(TAG, "event queue full, dropped %lu msgs in last %d ms",
                             (unsigned long)s_drop_count, SERVICE_WS_DROP_LOG_WINDOW_MS);
                    s_drop_count = 0;
                    s_drop_log_tick = now;
                }
            }
        }
        return;

    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGW(TAG, "websocket error: type=%d status=%d errno=%d",
                 event->error_handle.error_type,
                 event->error_handle.esp_ws_handshake_status_code,
                 event->error_handle.esp_transport_sock_errno);
        break;

    default:
        break;
    }

    /* CONNECTED / DISCONNECTED / ERROR 事件：data=NULL, len=0 */
    if (s_evt_queue != NULL) {
        service_ws_event_t evt = {0};
        switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            evt.type = SERVICE_WS_EVT_CONNECTED;
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
        case WEBSOCKET_EVENT_CLOSED:
            evt.type = SERVICE_WS_EVT_DISCONNECTED;
            break;
        case WEBSOCKET_EVENT_ERROR:
            evt.type = SERVICE_WS_EVT_ERROR;
            evt.error_code = event->error_handle.esp_transport_sock_errno;
            evt.http_status = event->error_handle.esp_ws_handshake_status_code;
            break;
        default:
            return;
        }
        if (xQueueSend(s_evt_queue, &evt, 0) != pdTRUE) {
            ESP_LOGW(TAG, "event queue full, drop type=%d", evt.type);
        }
    }
}

esp_err_t service_ws_init(void)
{
    if (s_mutex != NULL) {
        return ESP_OK;
    }

    s_mutex = xSemaphoreCreateMutexStatic(&s_mutex_buf);
    if (s_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    s_evt_storage = heap_caps_malloc(SERVICE_WS_EVT_QUEUE_LEN * sizeof(service_ws_event_t),
                                     MALLOC_CAP_SPIRAM);
    if (s_evt_storage == NULL) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_evt_queue = xQueueCreateStatic(SERVICE_WS_EVT_QUEUE_LEN, sizeof(service_ws_event_t),
                                     s_evt_storage, &s_evt_qstruct);
    if (s_evt_queue == NULL) {
        heap_caps_free(s_evt_storage);
        s_evt_storage = NULL;
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t service_ws_connect(const char *uri, const char *headers,
                             service_ws_event_cb_t cb, void *user_data)
{
    if (uri == NULL || cb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }

    if (s_client != NULL) {
        ESP_LOGW(TAG, "already connected/connecting");
        xSemaphoreGive(s_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    /* 通道打开前懒分配 PSRAM 接收缓冲；失败不致命，DATA 路径回退静态小缓冲 */
    if (s_rx_buf_dyn == NULL) {
        s_rx_buf_dyn = heap_caps_malloc(SERVICE_WS_RX_BUF_DYN_SIZE, MALLOC_CAP_SPIRAM);
        if (s_rx_buf_dyn == NULL) {
            ESP_LOGW(TAG, "psram rx buf alloc failed, fallback to static %d bytes",
                     (int)sizeof(s_rx_buf_static));
        }
    }

    s_cb = cb;
    s_user_data = user_data;
    s_connected = false;
    service_ws_evt_queue_clear();

    esp_websocket_client_config_t cfg = {
        .uri = uri,
        .headers = headers,
        .disable_auto_reconnect = true,
        .buffer_size = 1024,
        .task_stack = SERVICE_WS_TASK_STACK,
        .network_timeout_ms = SERVICE_WS_NETWORK_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    s_client = esp_websocket_client_init(&cfg);
    if (s_client == NULL) {
        ESP_LOGE(TAG, "client init failed");
        service_ws_rx_buf_free();
        xSemaphoreGive(s_mutex);
        return ESP_FAIL;
    }

    esp_err_t ret = esp_websocket_register_events(s_client, WEBSOCKET_EVENT_ANY,
                                                  service_ws_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "register events failed: %d", ret);
        esp_websocket_client_destroy(s_client);
        s_client = NULL;
        service_ws_rx_buf_free();
        xSemaphoreGive(s_mutex);
        return ret;
    }

    ret = esp_websocket_client_start(s_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "client start failed: %d", ret);
        esp_websocket_client_destroy(s_client);
        s_client = NULL;
        service_ws_rx_buf_free();
        xSemaphoreGive(s_mutex);
        return ret;
    }

    ESP_LOGI(TAG, "connecting: %s", uri);
    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

int service_ws_send_text(const char *data, int len)
{
    if (data == NULL || len <= 0) {
        return -1;
    }
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return -1;
    }
    if (s_client == NULL || !s_connected) {
        xSemaphoreGive(s_mutex);
        return -1;
    }
    int ret = esp_websocket_client_send_text(s_client, data, len, pdMS_TO_TICKS(1000));
    xSemaphoreGive(s_mutex);
    return ret;
}

int service_ws_send_bin(const uint8_t *data, int len)
{
    if (data == NULL || len <= 0) {
        return -1;
    }
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return -1;
    }
    if (s_client == NULL || !s_connected) {
        xSemaphoreGive(s_mutex);
        return -1;
    }
    int ret = esp_websocket_client_send_bin(s_client, (const char *)data, len, pdMS_TO_TICKS(1000));
    xSemaphoreGive(s_mutex);
    return ret;
}

bool service_ws_is_connected(void)
{
    return s_connected;
}

void service_ws_disconnect(void)
{
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    if (s_client == NULL) {
        /* 幂等兜底：连接失败路径已释放，重复 close 不会 double free */
        service_ws_rx_buf_free();
        service_ws_evt_queue_clear();
        xSemaphoreGive(s_mutex);
        return;
    }
    s_connected = false;
    esp_websocket_client_stop(s_client);
    esp_websocket_client_destroy(s_client);
    s_client = NULL;
    /* client 任务已销毁，不会再有 DATA 回调触碰接收缓冲 */
    service_ws_rx_buf_free();
    service_ws_evt_queue_clear();
    ESP_LOGI(TAG, "websocket destroyed");
    xSemaphoreGive(s_mutex);
}

void service_ws_process(void)
{
    if (s_evt_queue == NULL) {
        return;
    }

    /* 非阻塞出队并回调；队列操作本身线程安全，回调前释放锁 */
    service_ws_event_t evt;
    while (xQueueReceive(s_evt_queue, &evt, 0) == pdTRUE) {
        service_ws_event_cb_t cb = s_cb;
        void *user_data = s_user_data;
        if (cb != NULL) {
            cb(&evt, user_data);
        }
        if (evt.data != NULL) {
            heap_caps_free(evt.data);
        }
    }
}
