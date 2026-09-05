/**
 * @file xiaozhi_ota.c
 * @brief 小智 OTA 设备激活实现
 */

#include "xiaozhi_ota.h"

#include "service_xiaozhi_config.h"

#include "service_http_client.h"
#include "service_nvs.h"
#include "service_rtc.h"

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_system.h"

#include "stdio.h"
#include "string.h"
#include "sys/time.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "xiaozhi_ota";

/** @brief OTA 响应体缓冲（激活/配置 JSON 体积很小） */
#define XIAOZHI_OTA_RESP_BUF_SIZE 4096

/** @brief 请求体缓冲（精简系统信息 JSON） */
#define XIAOZHI_OTA_BODY_BUF_SIZE 1024

/** @brief 激活轮询响应缓冲（响应体极小，仅需满足 http 客户端非空要求） */
#define XIAOZHI_OTA_ACTIVATE_RESP_BUF_SIZE 1024

/* device_id 缓存：多路径调用避免重复解析+打印；重置绑定后可被失效 */
static char s_cached_device_id[18] = {0};

void xiaozhi_ota_get_device_id(char *out, size_t len)
{
    /* 多路径调用（会话/OTA 请求体/MCP）：首次解析后缓存，避免重复解析+打印刷屏 */
    if (s_cached_device_id[0] != '\0') {
        snprintf(out, len, "%s", s_cached_device_id);
        return;
    }

    uint8_t mac[6] = {0};
    esp_err_t ret;

    /* Trap: ESP32-P4 eFuse BLK1 可能未烧录 factory MAC（esp_read_mac 返回 262）
     * 此时使用 NVS 中持久化的 custom_mac 作为退路 */
    ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "read mac failed, using persistent fallback");

        /* 检查是否已有持久化的 fallback MAC */
        esp_err_t nvs_ret = service_nvs_get_custom_mac(mac, sizeof(mac));
        if (nvs_ret == ESP_OK && 
            (mac[0] != 0 || mac[1] != 0 || mac[2] != 0)) {
            ESP_LOGI(TAG, "using stored custom_mac: %02x:%02x:%02x:%02x:%02x:%02x",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        } else {
            /* 首次生成：DE:AD:BE:EF:xx:xx */
            ESP_LOGI(TAG, "no custom_mac found, generating new one");
            mac[0] = 0xDE; mac[1] = 0xAD; mac[2] = 0xBE; mac[3] = 0xEF;
            esp_fill_random(mac + 4, 2);

            /* 保存自定义 MAC 到 NVS */
            nvs_ret = service_nvs_set_custom_mac(mac);
            if (nvs_ret == ESP_OK) {
                ESP_LOGI(TAG, "stored custom_mac to NVS for persistence");
            } else {
                ESP_LOGE(TAG, "failed to store custom_mac: %s", esp_err_to_name(nvs_ret));
            }
        }
    } else {
        ESP_LOGI(TAG, "read mac from eFuse successfully: %02x:%02x:%02x:%02x:%02x:%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    int n = snprintf(out, len, "%02x:%02x:%02x:%02x:%02x:%02x",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    if (n > 0) {
        ESP_LOGI(TAG, "device_id=%s", out);
        snprintf(s_cached_device_id, sizeof(s_cached_device_id), "%s", out);
    }
}

void xiaozhi_ota_regenerate_device_id(void)
{
    /* 重置绑定时换新设备身份：服务器按 MAC 识别设备，MAC 不变则永远返回
     * 「已绑定」而不会下发新激活码；换 MAC 后失效缓存，下次 get 重新解析 */
    uint8_t mac[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0, 0};
    esp_fill_random(mac + 4, 2);
    esp_err_t ret = service_nvs_set_custom_mac(mac);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "regenerate: store mac failed: %s", esp_err_to_name(ret));
    }
    s_cached_device_id[0] = '\0';
    ESP_LOGI(TAG, "regenerated device mac: %02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/**
 * @brief 生成 UUID v4 字符串（36 字符 + NUL）
 */
static void xiaozhi_ota_gen_uuid(char *out, size_t len)
{
    uint8_t b[16];
    esp_fill_random(b, sizeof(b));
    b[6] = (b[6] & 0x0F) | 0x40; /* version 4 */
    b[8] = (b[8] & 0x3F) | 0x80; /* variant 1 */
    snprintf(out, len,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
             b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
}

esp_err_t xiaozhi_ota_ensure_client_id(char *out, size_t len)
{
    if (out == NULL || len < SERVICE_NVS_XZ_UUID_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = service_nvs_get_xz_uuid(out, len);
    if (ret != ESP_OK) {
        return ret;
    }

    if (out[0] == '\0') {
        xiaozhi_ota_gen_uuid(out, len);
        ESP_LOGI(TAG, "generated client uuid: %s", out);
        ret = service_nvs_set_xz_uuid(out);
        if (ret != ESP_OK) {
            return ret;
        }
        /* 立即落盘：激活流程中掉电会导致服务端记录与本地 uuid 不一致 */
        service_nvs_commit();
    }
    return ESP_OK;
}

/** @brief OTA 同步请求上下文 */
typedef struct {
    SemaphoreHandle_t sem;
    esp_err_t err;
    int http_status;
} ota_sync_ctx_t;

static SemaphoreHandle_t s_ota_sync_sem = NULL;

static void xiaozhi_ota_http_callback(int req_id, esp_err_t err, int http_status,
                                      const char *resp, size_t resp_len, void *user_data)
{
    (void)req_id;
    (void)resp;
    (void)resp_len;
    ota_sync_ctx_t *ctx = (ota_sync_ctx_t *)user_data;
    ctx->err = err;
    ctx->http_status = http_status;
    xSemaphoreGive(ctx->sem);
}

/**
 * @brief 执行一次 POST 并读取完整响应体
 *
 * 通过 service_http_client 异步接口同步等待，返回 HTTP 状态码。
 *
 * @return HTTP 状态码，<0 表示传输层错误
 */
static int xiaozhi_ota_post(const char *url, const char *body,
                            char *resp, size_t resp_len)
{
    char device_id[18];
    char client_id[SERVICE_NVS_XZ_UUID_MAX_LEN];
    xiaozhi_ota_get_device_id(device_id, sizeof(device_id));
    if (xiaozhi_ota_ensure_client_id(client_id, sizeof(client_id)) != ESP_OK) {
        return -1;
    }

    if (s_ota_sync_sem == NULL) {
        s_ota_sync_sem = xSemaphoreCreateBinary();
        if (s_ota_sync_sem == NULL) {
            return -1;
        }
    }

    service_http_client_header_t headers[] = {
        {"Activation-Version", "1"},
        {"Device-Id", device_id},
        {"Client-Id", client_id},
        {"User-Agent", SERVICE_XIAOZHI_USER_AGENT},
        {"Accept-Language", SERVICE_XIAOZHI_ACCEPT_LANGUAGE},
    };

    ota_sync_ctx_t ctx = {
        .sem = s_ota_sync_sem,
        .err = ESP_FAIL,
        .http_status = -1,
    };

    /* 确保信号量空，防止上次残留 */
    xSemaphoreTake(ctx.sem, 0);

    service_http_client_req_t req = {
        .method = SERVICE_HTTP_METHOD_POST,
        .url = url,
        .extra_headers = headers,
        .extra_headers_count = sizeof(headers) / sizeof(headers[0]),
        .body = body,
        .body_len = body ? strlen(body) : 0,
        .resp_buf = resp,
        .resp_buf_len = resp_len,
        .timeout_ms = SERVICE_XIAOZHI_HTTP_TIMEOUT_MS,
        .callback = xiaozhi_ota_http_callback,
        .user_data = &ctx,
    };

    int id = service_http_client_submit(&req);
    if (id <= 0) {
        ESP_LOGE(TAG, "ota submit failed");
        return -1;
    }

    /* Trap: done 回调经 task_app 分发，done 队列满会丢回调（或 task_app 卡死），
     * portMAX_DELAY 会让 task_ai 永久挂起、AI 子系统静默死亡；超时兜底 =
     * HTTP 超时(15s) + 分发余量 */
    if (xSemaphoreTake(ctx.sem, pdMS_TO_TICKS(SERVICE_XIAOZHI_HTTP_TIMEOUT_MS + 5000)) != pdTRUE) {
        ESP_LOGW(TAG, "ota sync wait timeout");
        return -1;
    }

    if (ctx.err != ESP_OK) {
        return (ctx.http_status > 0) ? ctx.http_status : -1;
    }
    return ctx.http_status;
}

/**
 * @brief 构造精简系统信息 JSON 请求体
 */
static void xiaozhi_ota_build_body(char *out, size_t len)
{
    char device_id[18];
    char client_id[SERVICE_NVS_XZ_UUID_MAX_LEN];
    xiaozhi_ota_get_device_id(device_id, sizeof(device_id));
    xiaozhi_ota_ensure_client_id(client_id, sizeof(client_id));

    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);

    const esp_app_desc_t *app = esp_app_get_description();

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "version", 2);
    cJSON_AddStringToObject(root, "language", SERVICE_XIAOZHI_ACCEPT_LANGUAGE);
    cJSON_AddNumberToObject(root, "flash_size", (double)flash_size);
    cJSON_AddStringToObject(root, "mac_address", device_id);
    cJSON_AddStringToObject(root, "uuid", client_id);
    cJSON_AddStringToObject(root, "chip_model_name", CONFIG_IDF_TARGET);

    cJSON *application = cJSON_CreateObject();
    cJSON_AddStringToObject(application, "name", app->project_name);
    /* 版本号统一走 FIRMWARE_VERSION（与关于页/启动日志同源）；app->version 是
     * project() VERSION，非 tag 构建恒为 0.0.0，不能用 */
    cJSON_AddStringToObject(application, "version", FIRMWARE_VERSION);
    cJSON_AddItemToObject(root, "application", application);

    char *printed = cJSON_PrintUnformatted(root);
    if (printed != NULL) {
        strncpy(out, printed, len - 1);
        out[len - 1] = '\0';
        cJSON_free(printed);
    } else {
        out[0] = '\0';
    }
    cJSON_Delete(root);
}

esp_err_t xiaozhi_ota_check(xiaozhi_ota_result_t *result)
{
    if (result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(result, 0, sizeof(*result));

    /* 请求/响应缓冲按需从 PSRAM 分配（原静态 .bss 常驻内部 RAM，合计约 5KB；
     * check 每开机/激活仅数次，分配释放开销可忽略） */
    char *body = heap_caps_malloc(XIAOZHI_OTA_BODY_BUF_SIZE, MALLOC_CAP_SPIRAM);
    char *resp = heap_caps_malloc(XIAOZHI_OTA_RESP_BUF_SIZE, MALLOC_CAP_SPIRAM);
    if (body == NULL || resp == NULL) {
        heap_caps_free(body);
        heap_caps_free(resp);
        ESP_LOGE(TAG, "ota buf alloc failed");
        return ESP_ERR_NO_MEM;
    }
    xiaozhi_ota_build_body(body, XIAOZHI_OTA_BODY_BUF_SIZE);

    int status = xiaozhi_ota_post(SERVICE_XIAOZHI_OTA_URL, body, resp, XIAOZHI_OTA_RESP_BUF_SIZE);
    heap_caps_free(body);
    if (status != 200) {
        ESP_LOGW(TAG, "ota check failed, http status=%d", status);
        heap_caps_free(resp);
        return (status < 0) ? ESP_FAIL : ESP_ERR_INVALID_RESPONSE;
    }

    /* cJSON_Parse 会把字符串拷入节点树，此后 resp 即可释放 */
    cJSON *root = cJSON_Parse(resp);
    heap_caps_free(resp);
    if (root == NULL) {
        ESP_LOGE(TAG, "parse ota response failed");
        return ESP_ERR_INVALID_RESPONSE;
    }

    cJSON *websocket = cJSON_GetObjectItem(root, "websocket");
    if (cJSON_IsObject(websocket)) {
        cJSON *url = cJSON_GetObjectItem(websocket, "url");
        cJSON *token = cJSON_GetObjectItem(websocket, "token");
        if (cJSON_IsString(url) && cJSON_IsString(token)) {
            strncpy(result->ws_url, url->valuestring, sizeof(result->ws_url) - 1);
            strncpy(result->ws_token, token->valuestring, sizeof(result->ws_token) - 1);
            result->has_websocket = true;
        }
    }

    cJSON *activation = cJSON_GetObjectItem(root, "activation");
    if (cJSON_IsObject(activation)) {
        cJSON *code = cJSON_GetObjectItem(activation, "code");
        cJSON *message = cJSON_GetObjectItem(activation, "message");
        cJSON *timeout_ms = cJSON_GetObjectItem(activation, "timeout_ms");
        if (cJSON_IsString(code)) {
            strncpy(result->activation_code, code->valuestring, sizeof(result->activation_code) - 1);
            result->has_activation = true;
        }
        if (cJSON_IsString(message)) {
            strncpy(result->activation_message, message->valuestring, sizeof(result->activation_message) - 1);
        }
        if (cJSON_IsNumber(timeout_ms)) {
            result->activation_timeout_ms = (uint32_t)timeout_ms->valueint;
        }
    }

    /* 服务器时间用于校准系统时钟，保障 TLS 证书有效期校验。
     * Why: RTC 服务每 60s 按本地语义（TZ=CST-8）把 RTC 芯片回写系统时钟，
     * 故此处必须写同一时间源：timestamp + timezone_offset 换算本地墙钟写入
     * RTC 芯片并立即回写系统时钟，避免两套语义互相覆盖（绑定窗口可差 8 小时）。
     * RTC 不可用时退回原 settimeofday 兜底 */
    cJSON *server_time = cJSON_GetObjectItem(root, "server_time");
    if (cJSON_IsObject(server_time)) {
        cJSON *timestamp = cJSON_GetObjectItem(server_time, "timestamp");
        if (cJSON_IsNumber(timestamp)) {
            time_t t_utc = (time_t)(timestamp->valuedouble / 1000);
            /* timezone_offset 单位分钟，缺失默认东八区（与项目 TZ=CST-8 一致） */
            int tz_offset_min = 480;
            cJSON *timezone_offset = cJSON_GetObjectItem(server_time, "timezone_offset");
            if (cJSON_IsNumber(timezone_offset)) {
                tz_offset_min = timezone_offset->valueint;
            }
            /* 本地墙钟按 UTC 拆解存芯片；sync_to_system 经 mktime(TZ) 还原正确 epoch */
            time_t t_local = t_utc + (time_t)tz_offset_min * 60;
            struct tm tm_local;
            gmtime_r(&t_local, &tm_local);
            if (service_rtc_set_time(&tm_local) == ESP_OK) {
                service_rtc_sync_to_system();
            } else {
                ESP_LOGW(TAG, "rtc set time failed, fallback to settimeofday");
                struct timeval tv = {
                    .tv_sec = t_utc,
                    .tv_usec = 0,
                };
                settimeofday(&tv, NULL);
            }
        }
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "ota check ok: websocket=%d activation=%d",
             result->has_websocket, result->has_activation);
    return ESP_OK;
}

esp_err_t xiaozhi_ota_activate(void)
{
    char url[sizeof(SERVICE_XIAOZHI_OTA_URL) + 16];
    snprintf(url, sizeof(url), "%sactivate", SERVICE_XIAOZHI_OTA_URL);

    /* Trap: service_http_client_submit 拒绝无响应缓冲的请求，传 NULL 会静默
     * 拒收致绑定状态轮询永远失败（设备停在激活码界面）；响应体极小 */
    char *resp = heap_caps_malloc(XIAOZHI_OTA_ACTIVATE_RESP_BUF_SIZE, MALLOC_CAP_SPIRAM);
    if (resp == NULL) {
        return ESP_ERR_NO_MEM;
    }

    /* v1 设备（无序列号）激活请求体为空 JSON */
    int status = xiaozhi_ota_post(url, "{}", resp, XIAOZHI_OTA_ACTIVATE_RESP_BUF_SIZE);
    heap_caps_free(resp);
    if (status == 200) {
        ESP_LOGI(TAG, "activation success");
        return ESP_OK;
    }
    if (status == 202) {
        return ESP_ERR_TIMEOUT;
    }
    ESP_LOGW(TAG, "activate failed, http status=%d", status);
    return ESP_FAIL;
}
