/**
 * @file service_http_client.c
 * @brief 系统级 HTTP/HTTPS 客户端服务实现
 *
 * 所有异步请求由单一按需 HTTP worker 顺序处理，
 * 完成结果进入完成队列；task_app 周期调用 service_http_client_process()
 * 分发回调，保证回调运行在 task_app 上下文。
 */

#include "service_http_client.h"

#include "service_wifi.h"

#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"
#include "string.h"
#include "stdlib.h"
#include "ctype.h"

static const char *TAG = "service_http_client";

#define HTTP_REQ_QUEUE_SIZE   4
#define HTTP_DONE_QUEUE_SIZE  4
#define HTTP_MAX_EXTRA_HEADERS 8

typedef struct {
    int                           id;
    service_http_client_method_t  method;
    char                          url[512];
    char                          auth_header[256];
    char                          content_type[64];
    service_http_client_header_t  extra_headers[HTTP_MAX_EXTRA_HEADERS];
    size_t                        extra_headers_count;
    char                         *body;          /* 已复制 */
    size_t                        body_len;
    uint32_t                      timeout_ms;
    char                         *resp_buf;
    size_t                        resp_buf_len;
    service_http_client_cb_t      callback;
    void                         *user_data;
    service_http_client_chunk_cb_t chunk_cb;
    void                          *chunk_user_data;
    bool                          sync;          /* 同步请求 */
    SemaphoreHandle_t             sync_sem;      /* 同步完成信号量 */
    esp_err_t                    *sync_ret;      /* 同步结果回写指针 */
    int                          *sync_status;   /* 同步 HTTP 状态码回写指针 */
} http_job_t;

typedef struct {
    int                       id;
    esp_err_t                 err;
    int                       http_status;
    service_http_client_cb_t  callback;
    void                     *user_data;
    char                     *resp_buf;
    size_t                    resp_len;
    bool                      truncated;
} http_done_t;

typedef struct {
    QueueHandle_t     req_queue;
    QueueHandle_t     done_queue;
    int               next_id;
    bool              initialized;
} http_ctx_t;

static http_ctx_t s_ctx = {0};
static service_http_client_notify_cb_t s_notify_cb = NULL;

/** @brief 内部 HTTP worker 任务句柄与唤醒信号量 */
static TaskHandle_t s_http_worker_task = NULL;
static SemaphoreHandle_t s_http_worker_sem = NULL;

static void http_client_tls_warmup_process(void);

/**
 * @brief 通知 worker 任务有新请求入队
 */
static void http_worker_notify(void)
{
    if (s_http_worker_sem != NULL) {
        xSemaphoreGive(s_http_worker_sem);
    }
}

/**
 * @brief HTTP worker 任务：循环执行请求队列
 */
static void http_worker_task_entry(void *arg)
{
    (void)arg;
    while (1) {
        if (!service_http_client_process_one()) {
            /* 队列为空时阻塞等待通知，超时 100ms 兜底轮询 */
            xSemaphoreTake(s_http_worker_sem, pdMS_TO_TICKS(100));
        }
    }
}

static void http_notify(void)
{
    if (s_notify_cb != NULL) {
        s_notify_cb();
    }
    http_worker_notify();
}

static esp_http_client_method_t method_to_esp(service_http_client_method_t m)
{
    switch (m) {
        case SERVICE_HTTP_METHOD_POST:   return HTTP_METHOD_POST;
        case SERVICE_HTTP_METHOD_PUT:    return HTTP_METHOD_PUT;
        case SERVICE_HTTP_METHOD_DELETE: return HTTP_METHOD_DELETE;
        default:                         return HTTP_METHOD_GET;
    }
}

static bool copy_header(char *dst, size_t dst_len, const char *src)
{
    if (src == NULL || src[0] == '\0') {
        dst[0] = '\0';
        return true;
    }
    if (strlen(src) >= dst_len) {
        return false;
    }
    strcpy(dst, src);
    return true;
}

typedef struct {
    char   *buf;
    size_t  capacity;
    size_t  len;
    bool    truncated;
    int     status_code;
    service_http_client_chunk_cb_t chunk_cb;
    void   *chunk_user_data;
    int     req_id;
} http_resp_ctx_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_resp_ctx_t *ctx = (http_resp_ctx_t *)evt->user_data;

    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (ctx == NULL || evt->data == NULL || evt->data_len == 0) {
                break;
            }
            if (ctx->chunk_cb != NULL) {
                ctx->chunk_cb(ctx->req_id, (const char *)evt->data, evt->data_len,
                              ctx->chunk_user_data);
            }
            if (ctx->len + evt->data_len >= ctx->capacity) {
                size_t copy = ctx->capacity - ctx->len - 1;
                if (copy > 0) {
                    memcpy(ctx->buf + ctx->len, evt->data, copy);
                }
                ctx->len = ctx->capacity - 1;
                ctx->truncated = true;
            } else {
                memcpy(ctx->buf + ctx->len, evt->data, evt->data_len);
                ctx->len += evt->data_len;
            }
            ctx->buf[ctx->len] = '\0';
            break;

        case HTTP_EVENT_ON_FINISH:
            if (ctx != NULL) {
                ctx->status_code = esp_http_client_get_status_code(evt->client);
            }
            break;

        default:
            break;
    }

    return ESP_OK;
}

static esp_err_t http_do_perform(const http_job_t *job, int *status_code, size_t *resp_len)
{
    if (job == NULL || job->url[0] == '\0' || job->resp_buf == NULL || job->resp_buf_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    http_resp_ctx_t ctx = {
        .buf = job->resp_buf,
        .capacity = job->resp_buf_len,
        .len = 0,
        .truncated = false,
        .status_code = 0,
        .chunk_cb = job->chunk_cb,
        .chunk_user_data = job->chunk_user_data,
        .req_id = job->id,
    };
    job->resp_buf[0] = '\0';

    esp_http_client_config_t config = {
        .url = job->url,
        .method = method_to_esp(job->method),
        .event_handler = http_event_handler,
        .user_data = &ctx,
        .timeout_ms = job->timeout_ms,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "client init failed");
        return ESP_FAIL;
    }

    if (job->auth_header[0] != '\0') {
        esp_http_client_set_header(client, "Authorization", job->auth_header);
    }

    for (size_t i = 0; i < job->extra_headers_count; i++) {
        if (job->extra_headers[i].key != NULL && job->extra_headers[i].value != NULL) {
            esp_http_client_set_header(client, job->extra_headers[i].key, job->extra_headers[i].value);
        }
    }

    if (job->body != NULL && job->body_len > 0) {
        const char *ct = job->content_type[0] != '\0' ? job->content_type : "application/json";
        esp_http_client_set_header(client, "Content-Type", ct);
        esp_http_client_set_post_field(client, job->body, (int)job->body_len);
    }

    esp_err_t ret = esp_http_client_perform(client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "request failed: %s", esp_err_to_name(ret));
        esp_http_client_cleanup(client);
        return ret;
    }

    int status = esp_http_client_get_status_code(client);
    if (status_code != NULL) {
        *status_code = status;
    }
    if (resp_len != NULL) {
        *resp_len = ctx.len;
    }

    esp_http_client_cleanup(client);

    if (ctx.truncated) {
        ESP_LOGW(TAG, "response truncated");
    }

    return ESP_OK;
}

bool service_http_client_process_one(void)
{
    if (!s_ctx.initialized) {
        return false;
    }

    http_job_t job;
    if (xQueueReceive(s_ctx.req_queue, &job, 0) != pdTRUE) {
        return false;
    }

    int status = 0;
    size_t resp_len = 0;
    esp_err_t err = http_do_perform(&job, &status, &resp_len);

    if (job.sync) {
        if (job.sync_ret != NULL) {
            *job.sync_ret = err;
        }
        if (job.sync_status != NULL) {
            *job.sync_status = status;
        }
        if (job.sync_sem != NULL) {
            xSemaphoreGive(job.sync_sem);
        }
    } else {
        http_done_t done = {
            .id = job.id,
            .err = err,
            .http_status = status,
            .callback = job.callback,
            .user_data = job.user_data,
            .resp_buf = job.resp_buf,
            .resp_len = resp_len,
            .truncated = (resp_len >= job.resp_buf_len - 1 && job.resp_buf_len > 1),
        };
        if (xQueueSend(s_ctx.done_queue, &done, 0) != pdTRUE) {
            ESP_LOGW(TAG, "done queue full, drop req_id=%d", job.id);
        }
    }

    /* Trap: 仅异步任务由 worker 释放 body；同步任务(sync)的 body 与调用方
     * 栈上 job 共享同一指针，由调用方在信号量返回后自行释放，此处再 free
     * 即双重释放 */
    if (!job.sync && job.body != NULL) {
        free(job.body);
    }

    return true;
}

esp_err_t service_http_client_init(void)
{
    if (s_ctx.initialized) {
        return ESP_OK;
    }

    s_ctx.req_queue = xQueueCreateWithCaps(HTTP_REQ_QUEUE_SIZE, sizeof(http_job_t), MALLOC_CAP_SPIRAM);
    s_ctx.done_queue = xQueueCreateWithCaps(HTTP_DONE_QUEUE_SIZE, sizeof(http_done_t), MALLOC_CAP_SPIRAM);
    if (s_ctx.req_queue == NULL || s_ctx.done_queue == NULL) {
        ESP_LOGE(TAG, "init failed: queue alloc failed");
        return ESP_ERR_NO_MEM;
    }

    if (s_http_worker_sem == NULL) {
        s_http_worker_sem = xSemaphoreCreateBinary();
        if (s_http_worker_sem == NULL) {
            ESP_LOGE(TAG, "init failed: sem alloc failed");
            return ESP_ERR_NO_MEM;
        }
    }

    if (s_http_worker_task == NULL) {
        static StackType_t *s_worker_stack = NULL;
        static StaticTask_t s_worker_tcb;
        if (s_worker_stack == NULL) {
            s_worker_stack = heap_caps_malloc(12288, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        }
        BaseType_t ret = pdFAIL;
        if (s_worker_stack != NULL) {
            s_http_worker_task = xTaskCreateStaticPinnedToCore(
                http_worker_task_entry, "http_worker", 12288, NULL, 4,
                s_worker_stack, &s_worker_tcb, 0);
            ret = (s_http_worker_task != NULL) ? pdPASS : pdFAIL;
        } else {
            ret = xTaskCreatePinnedToCore(http_worker_task_entry,
                                          "http_worker",
                                          12288,
                                          NULL,
                                          4,
                                          &s_http_worker_task,
                                          0);
        }
        if (ret != pdPASS) {
            ESP_LOGE(TAG, "init failed: worker task create failed");
            return ESP_ERR_NO_MEM;
        }
    }

    s_ctx.next_id = 1;
    s_ctx.initialized = true;
    ESP_LOGI(TAG, "initialized with worker task");
    return ESP_OK;
}

void service_http_client_set_notify_cb(service_http_client_notify_cb_t cb)
{
    s_notify_cb = cb;
}

void service_http_client_process(void)
{
    if (!s_ctx.initialized) {
        return;
    }

    http_client_tls_warmup_process();

    http_done_t done;
    while (xQueueReceive(s_ctx.done_queue, &done, 0) == pdTRUE) {
        if (done.callback != NULL) {
            done.callback(done.id, done.err, done.http_status,
                          done.resp_buf, done.resp_len, done.user_data);
        }
    }
}

int service_http_client_submit(const service_http_client_req_t *req)
{
    if (!s_ctx.initialized) {
        if (service_http_client_init() != ESP_OK) {
            return 0;
        }
    }

    if (req == NULL || req->url == NULL || req->url[0] == '\0' ||
        req->resp_buf == NULL || req->resp_buf_len == 0) {
        return 0;
    }

    http_job_t job = {0};
    job.id = s_ctx.next_id++;
    if (job.id <= 0) {
        job.id = 1;
        s_ctx.next_id = 2;
    }
    job.method = req->method;
    job.timeout_ms = req->timeout_ms == 0 ? 10000 : req->timeout_ms;
    job.resp_buf = req->resp_buf;
    job.resp_buf_len = req->resp_buf_len;
    job.callback = req->callback;
    job.user_data = req->user_data;
    job.chunk_cb = req->chunk_cb;
    job.chunk_user_data = req->chunk_user_data;
    job.sync = false;

    if (!copy_header(job.url, sizeof(job.url), req->url) ||
        !copy_header(job.auth_header, sizeof(job.auth_header), req->auth_header) ||
        !copy_header(job.content_type, sizeof(job.content_type), req->content_type)) {
        ESP_LOGE(TAG, "submit failed: header/url too long");
        return 0;
    }

    if (req->extra_headers != NULL && req->extra_headers_count > 0) {
        job.extra_headers_count = req->extra_headers_count;
        if (job.extra_headers_count > HTTP_MAX_EXTRA_HEADERS) {
            job.extra_headers_count = HTTP_MAX_EXTRA_HEADERS;
        }
        for (size_t i = 0; i < job.extra_headers_count; i++) {
            job.extra_headers[i].key = req->extra_headers[i].key;
            job.extra_headers[i].value = req->extra_headers[i].value;
        }
    }

    if (req->body != NULL && req->body_len > 0) {
        job.body = (char *)malloc(req->body_len + 1);
        if (job.body == NULL) {
            ESP_LOGE(TAG, "submit failed: body alloc failed");
            return 0;
        }
        memcpy(job.body, req->body, req->body_len);
        job.body[req->body_len] = '\0';
        job.body_len = req->body_len;
    }

    if (xQueueSend(s_ctx.req_queue, &job, 0) != pdTRUE) {
        ESP_LOGW(TAG, "submit failed: req queue full");
        if (job.body != NULL) {
            free(job.body);
        }
        return 0;
    }

    http_notify();
    return job.id;
}

bool service_http_client_cancel(int req_id)
{
    if (!s_ctx.initialized || req_id <= 0) {
        return false;
    }

    http_job_t job;
    /* 简单实现：遍历队列，移除匹配 id 的条目。不支持取消正在执行的请求。 */
    size_t count = uxQueueMessagesWaiting(s_ctx.req_queue);
    for (size_t i = 0; i < count; i++) {
        if (xQueueReceive(s_ctx.req_queue, &job, 0) != pdTRUE) {
            break;
        }
        if (job.id == req_id) {
            if (job.body != NULL) {
                free(job.body);
            }
            ESP_LOGD(TAG, "cancelled req_id=%d", req_id);
            return true;
        }
        if (xQueueSend(s_ctx.req_queue, &job, 0) != pdTRUE) {
            ESP_LOGW(TAG, "cancel restore failed");
            if (job.body != NULL) {
                free(job.body);
            }
            break;
        }
    }
    return false;
}

static esp_err_t http_perform_sync(const http_job_t *job_in, int *status_code)
{
    SemaphoreHandle_t sem = xSemaphoreCreateBinary();
    if (sem == NULL) {
        return ESP_ERR_NO_MEM;
    }

    http_job_t job = *job_in;
    job.sync = true;
    job.sync_sem = sem;
    esp_err_t ret = ESP_FAIL;
    int http_status = 0;
    job.sync_ret = &ret;
    job.sync_status = &http_status;

    if (xQueueSend(s_ctx.req_queue, &job, portMAX_DELAY) != pdTRUE) {
        vSemaphoreDelete(sem);
        return ESP_FAIL;
    }

    http_notify();

    if (xSemaphoreTake(sem, portMAX_DELAY) != pdTRUE) {
        vSemaphoreDelete(sem);
        return ESP_FAIL;
    }
    vSemaphoreDelete(sem);

    if (status_code != NULL) {
        *status_code = http_status;
    }
    return ret;
}

esp_err_t service_http_client_get(const char *url, const char *auth_header,
                                  char *resp, size_t resp_len, uint32_t timeout_ms)
{
    return service_http_client_get_with_headers(url, auth_header, NULL, 0,
                                              resp, resp_len, timeout_ms);
}

esp_err_t service_http_client_get_with_headers(const char *url, const char *auth_header,
                                               const service_http_client_header_t *extra_headers,
                                               size_t header_count,
                                               char *resp, size_t resp_len, uint32_t timeout_ms)
{
    if (url == NULL || resp == NULL || resp_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (service_http_client_init() != ESP_OK) {
        return ESP_FAIL;
    }

    http_job_t job = {0};
    job.method = SERVICE_HTTP_METHOD_GET;
    job.timeout_ms = timeout_ms == 0 ? 10000 : timeout_ms;
    job.resp_buf = resp;
    job.resp_buf_len = resp_len;

    if (!copy_header(job.url, sizeof(job.url), url) ||
        !copy_header(job.auth_header, sizeof(job.auth_header), auth_header)) {
        return ESP_ERR_NO_MEM;
    }

    if (extra_headers != NULL && header_count > 0) {
        job.extra_headers_count = header_count;
        if (job.extra_headers_count > HTTP_MAX_EXTRA_HEADERS) {
            job.extra_headers_count = HTTP_MAX_EXTRA_HEADERS;
        }
        for (size_t i = 0; i < job.extra_headers_count; i++) {
            job.extra_headers[i].key = extra_headers[i].key;
            job.extra_headers[i].value = extra_headers[i].value;
        }
    }

    int status = 0;
    esp_err_t ret = http_perform_sync(&job, &status);
    if (ret != ESP_OK) {
        return ret;
    }
    if (status < 200 || status >= 300) {
        ESP_LOGW(TAG, "HTTP status %d", status);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t service_http_client_post_json(const char *url, const char *auth_header,
                                        const char *body, char *resp, size_t resp_len,
                                        uint32_t timeout_ms)
{
    return service_http_client_post_json_with_headers(url, auth_header, NULL, 0,
                                                      body, resp, resp_len, timeout_ms);
}

esp_err_t service_http_client_post_json_with_headers(const char *url, const char *auth_header,
                                                     const service_http_client_header_t *extra_headers,
                                                     size_t header_count,
                                                     const char *body, char *resp, size_t resp_len,
                                                     uint32_t timeout_ms)
{
    if (url == NULL || resp == NULL || resp_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (service_http_client_init() != ESP_OK) {
        return ESP_FAIL;
    }

    http_job_t job = {0};
    job.method = SERVICE_HTTP_METHOD_POST;
    job.timeout_ms = timeout_ms == 0 ? 10000 : timeout_ms;
    job.resp_buf = resp;
    job.resp_buf_len = resp_len;

    if (!copy_header(job.url, sizeof(job.url), url) ||
        !copy_header(job.auth_header, sizeof(job.auth_header), auth_header)) {
        return ESP_ERR_NO_MEM;
    }

    if (extra_headers != NULL && header_count > 0) {
        job.extra_headers_count = header_count;
        if (job.extra_headers_count > HTTP_MAX_EXTRA_HEADERS) {
            job.extra_headers_count = HTTP_MAX_EXTRA_HEADERS;
        }
        for (size_t i = 0; i < job.extra_headers_count; i++) {
            job.extra_headers[i].key = extra_headers[i].key;
            job.extra_headers[i].value = extra_headers[i].value;
        }
    }

    if (body != NULL) {
        job.body_len = strlen(body);
        if (job.body_len > 0) {
            job.body = (char *)malloc(job.body_len + 1);
            if (job.body == NULL) {
                return ESP_ERR_NO_MEM;
            }
            memcpy(job.body, body, job.body_len);
            job.body[job.body_len] = '\0';
        }
    }
    copy_header(job.content_type, sizeof(job.content_type), "application/json");

    int status = 0;
    esp_err_t ret = http_perform_sync(&job, &status);
    if (job.body != NULL) {
        free(job.body);
    }
    if (ret != ESP_OK) {
        return ret;
    }
    if (status < 200 || status >= 300) {
        ESP_LOGW(TAG, "HTTP status %d", status);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t service_http_client_base64_encode(const uint8_t *in, size_t in_len,
                                            char *out, size_t out_len)
{
    if (in == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t required = 4 * ((in_len + 2) / 3) + 1;
    if (out_len < required) {
        return ESP_ERR_NO_MEM;
    }

    size_t olen = 0;
    int ret = mbedtls_base64_encode((unsigned char *)out, out_len, &olen, in, in_len);
    if (ret != 0) {
        return ESP_FAIL;
    }
    out[olen] = '\0';
    return ESP_OK;
}

bool service_http_client_json_get_string(const char *json, const char *key,
                                         char *out, size_t out_len)
{
    if (json == NULL || key == NULL || out == NULL || out_len == 0) {
        return false;
    }

    size_t key_len = strlen(key);
    const char *p = json;

    while ((p = strchr(p, '"')) != NULL) {
        p++;
        if (strncmp(p, key, key_len) == 0 && p[key_len] == '"') {
            const char *val = p + key_len + 1;
            while (*val != '\0' && (*val == ' ' || *val == '\t' || *val == '\n' || *val == '\r' || *val == ':')) {
                val++;
            }
            if (*val != '"') {
                return false;
            }
            val++;

            size_t i = 0;
            while (*val != '"' && *val != '\0' && i < out_len - 1) {
                if (*val == '\\' && *(val + 1) != '\0') {
                    val++;
                    switch (*val) {
                        case 'n': out[i++] = '\n'; break;
                        case 't': out[i++] = '\t'; break;
                        case 'r': out[i++] = '\r'; break;
                        case '\\': out[i++] = '\\'; break;
                        case '"': out[i++] = '"'; break;
                        default:  out[i++] = *val; break;
                    }
                    val++;
                } else {
                    out[i++] = *val++;
                }
            }
            out[i] = '\0';
            return true;
        }
    }

    return false;
}

/**
 * @brief WiFi 首次连接成功后预热 TLS 会话
 *
 * Why: 首次 HTTPS 请求要现跑 DNS/TLS 握手/证书校验，耗时数秒；
 * 提前发一次哑请求（结果丢弃）把这条链路跑通，App 首次请求即可秒级响应。
 * Contract: 由 service_http_client_process() 每周期调用，内部状态机保证只执行一次。
 */
static void http_client_tls_warmup_process(void)
{
    static bool s_wifi_was_connected = false;
    static bool s_tls_warm_done = false;

    bool wifi_connected = service_wifi_is_connected();
    if (wifi_connected && !s_wifi_was_connected && !s_tls_warm_done) {
        s_tls_warm_done = true;
        ESP_LOGI(TAG, "WiFi up: TLS warm-up request submitted");
        static char s_warm_resp[64];
        service_http_client_req_t req = {
            .method = SERVICE_HTTP_METHOD_GET,
            .url = "https://v1.hitokoto.cn/?c=k&encode=text",
            .resp_buf = s_warm_resp,
            .resp_buf_len = sizeof(s_warm_resp),
            .timeout_ms = 15000,
        };
        /* 结果丢弃，仅预热；失败也无所谓 */
        service_http_client_submit(&req);
    }
    s_wifi_was_connected = wifi_connected;
}
