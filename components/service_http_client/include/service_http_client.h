/**
 * @file service_http_client.h
 * @brief 系统级 HTTP/HTTPS 客户端服务
 *
 * 提供同步 GET/POST 接口（兼容旧代码）与异步 submit 接口。
 * 异步请求由 service_http_client 内部唯一的按需 HTTP worker 处理，
 * 回调在 task_app 上下文通过 service_http_client_process() 分发，
 * 因此调用方可在回调中安全更新 UI 或 App 状态。
 */

#ifndef SERVICE_HTTP_CLIENT_H
#define SERVICE_HTTP_CLIENT_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief HTTP 方法
 */
typedef enum {
    SERVICE_HTTP_METHOD_GET,
    SERVICE_HTTP_METHOD_POST,
    SERVICE_HTTP_METHOD_PUT,
    SERVICE_HTTP_METHOD_DELETE,
} service_http_client_method_t;

/**
 * @brief 异步请求完成回调
 *
 * @param[in] req_id      请求 ID（即 service_http_client_submit 返回值）
 * @param[in] err         ESP_OK 表示请求成功发出并收到 HTTP 响应；否则为网络/协议错误
 * @param[in] http_status HTTP 状态码；err 不为 ESP_OK 时可能为 0
 * @param[in] resp        响应缓冲区（由调用者提供，回调时仍有效）
 * @param[in] resp_len    实际响应长度（不含终止符）
 * @param[in] user_data   提交请求时传入的私有数据
 */
typedef void (*service_http_client_cb_t)(int req_id, esp_err_t err, int http_status,
                                          const char *resp, size_t resp_len,
                                          void *user_data);

/**
 * @brief HTTP 响应数据块回调（用于流式/SSE 等场景）
 *
 * 在 network_task 上下文中直接调用，每收到一段响应数据即触发一次。
 * 调用方应保证回调内只做轻量处理（如拷贝到本地缓冲区或设置标志），
 * 避免在 network_task 中执行耗时或阻塞操作。
 */
typedef void (*service_http_client_chunk_cb_t)(int req_id, const char *chunk,
                                                size_t chunk_len, void *user_data);

/**
 * @brief 额外 HTTP 请求头（key-value 对）
 */
typedef struct {
    const char *key;
    const char *value;
} service_http_client_header_t;

/**
 * @brief 异步 HTTP 请求描述
 *
 * 提交前调用者需保证 url、auth_header、body、resp_buf 在请求完成前有效。
 * service_http_client_submit 会复制此结构内容，但不会复制字符串/缓冲区本身。
 * extra_headers 数组只需在提交调用期间有效，提交后会被拷贝到内部作业。
 */
typedef struct {
    service_http_client_method_t method;      /**< 请求方法 */
    const char                  *url;         /**< 完整 URL */
    const char                  *auth_header; /**< 可选 Authorization 头，如 "Bearer xxx" */
    const char                  *content_type;/**< 可选 Content-Type；NULL 时 POST 默认 application/json */
    const service_http_client_header_t *extra_headers; /**< 可选额外请求头数组 */
    size_t                       extra_headers_count;    /**< 额外请求头数量 */
    const void                  *body;        /**< 请求体 */
    size_t                       body_len;    /**< 请求体长度 */
    uint32_t                     timeout_ms;  /**< 超时时间 */

    char                        *resp_buf;    /**< 调用者提供的响应缓冲区 */
    size_t                       resp_buf_len;/**< 响应缓冲区长度（含终止符） */

    service_http_client_cb_t     callback;    /**< 完成回调 */
    void                        *user_data;    /**< 完成回调私有数据 */

    service_http_client_chunk_cb_t chunk_cb;  /**< 可选：流式数据块回调 */
    void                          *chunk_user_data; /**< 流式回调私有数据 */
} service_http_client_req_t;

/**
 * @brief 初始化 HTTP 客户端服务
 *
 * 创建内部请求队列与完成队列。可重复调用，首次调用后返回 ESP_OK。
 *
 * @return ESP_OK 成功
 */
esp_err_t service_http_client_init(void);

/**
 * @brief 轮询分发已完成请求的回调
 *
 * 由 task_app 周期调用，使所有异步回调都在 task_app 上下文执行。
 */
void service_http_client_process(void);

/**
 * @brief 由执行器任务调用，执行请求队列中的一个 HTTP 请求
 *
 * 当前由 service_http_client 内部创建的 worker 任务循环调用，
 * 亦可在调用方自建执行循环中周期性调用。
 *
 * @return true 执行了一个请求；false 当前队列为空
 */
bool service_http_client_process_one(void);

/**
 * @brief 提交异步 HTTP 请求
 *
 * 请求进入内部队列，由 HTTP worker 顺序执行。返回大于 0 的请求 ID，
 * 可用于 service_http_client_cancel；返回 0 或负值表示提交失败。
 *
 * @param[in] req 请求描述
 * @return 请求 ID（>0 成功），<=0 失败
 */
int service_http_client_submit(const service_http_client_req_t *req);

/**
 * @brief HTTP 请求入队通知回调类型
 *
 * 由执行器注册，在请求提交时被调用以唤醒执行器。
 */
typedef void (*service_http_client_notify_cb_t)(void);

/**
 * @brief 注册请求入队通知回调
 *
 * @param[in] cb 通知回调；传 NULL 取消注册
 */
void service_http_client_set_notify_cb(service_http_client_notify_cb_t cb);

/**
 * @brief 取消尚未执行的异步请求
 *
 * 仅能从等待队列中移除请求；已正在执行的请求无法中断，
 * 其回调仍会触发，调用方应在回调中根据当前状态决定是否处理。
 *
 * @param[in] req_id 请求 ID
 * @return true 取消成功；false 未找到或已开始执行
 */
bool service_http_client_cancel(int req_id);

/**
 * @brief 同步 HTTPS GET 请求（兼容旧代码，内部走异步 worker + 信号量阻塞）
 *
 * @param[in]  url         请求地址
 * @param[in]  auth_header 可选 Authorization 头；无需时传 NULL
 * @param[out] resp        响应缓冲区
 * @param[in]  resp_len    缓冲区长度
 * @param[in]  timeout_ms  超时时间
 * @return ESP_OK 成功且 HTTP 2xx
 */
esp_err_t service_http_client_get(const char *url, const char *auth_header,
                                  char *resp, size_t resp_len, uint32_t timeout_ms);

/**
 * @brief 同步 HTTPS GET 请求，支持额外请求头
 *
 * @param[in]  url           请求地址
 * @param[in]  auth_header   可选 Authorization 头；无需时传 NULL
 * @param[in]  extra_headers 可选额外请求头数组；无需时传 NULL
 * @param[in]  header_count  额外请求头数量
 * @param[out] resp          响应缓冲区
 * @param[in]  resp_len      缓冲区长度
 * @param[in]  timeout_ms    超时时间
 * @return ESP_OK 成功且 HTTP 2xx
 */
esp_err_t service_http_client_get_with_headers(const char *url, const char *auth_header,
                                               const service_http_client_header_t *extra_headers,
                                               size_t header_count,
                                               char *resp, size_t resp_len, uint32_t timeout_ms);

/**
 * @brief 同步 HTTPS POST 请求，Content-Type 固定为 application/json，支持额外请求头
 *
 * @param[in]  url           请求地址
 * @param[in]  auth_header   可选 Authorization 头
 * @param[in]  extra_headers 可选额外请求头数组；无需时传 NULL
 * @param[in]  header_count  额外请求头数量
 * @param[in]  body          JSON 请求体（以 '\0' 结尾）
 * @param[out] resp          响应缓冲区
 * @param[in]  resp_len      缓冲区长度
 * @param[in]  timeout_ms    超时时间
 * @return ESP_OK 成功且 HTTP 2xx
 */
esp_err_t service_http_client_post_json_with_headers(const char *url, const char *auth_header,
                                                     const service_http_client_header_t *extra_headers,
                                                     size_t header_count,
                                                     const char *body, char *resp, size_t resp_len,
                                                     uint32_t timeout_ms);

/**
 * @brief 对二进制数据进行 Base64 编码
 */
esp_err_t service_http_client_base64_encode(const uint8_t *in, size_t in_len,
                                            char *out, size_t out_len);

/**
 * @brief 从 JSON 中提取某个字符串字段的值
 */
bool service_http_client_json_get_string(const char *json, const char *key,
                                         char *out, size_t out_len);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_HTTP_CLIENT_H */
