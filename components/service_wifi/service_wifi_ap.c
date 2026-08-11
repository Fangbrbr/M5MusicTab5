/**
 * @file service_wifi_ap.c
 * @brief SoftAP 配网（非 Captive Portal）
 *
 * 首次开机未初始化时启动 SoftAP "HammySetup"，用户手动连接后通过浏览器访问
 * http://192.168.4.1/ 进行配置。
 * 提交后保存 WiFi 凭据到 NVS 并重启。
 * HTTP 服务器在 STA 模式下也保持运行，方便后续随时登入修改配置。
 */

#include "service_wifi.h"
#include "service_nvs.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

static const char *TAG = "service_wifi_ap";

extern void service_wifi_mark_ap_mode(bool ap_mode);

#define AP_SSID             "HammySetup"
#define AP_PASS             "12345678"
#define AP_MAX_CONN         4
#define AP_CHANNEL          1

static httpd_handle_t s_httpd = NULL;
static esp_netif_t *s_ap_netif = NULL;
static bool s_config_saved = false;
static bool s_ap_mode_started = false;   /* APSTA 模式是否已启动过（启动只做一次） */

static const char *s_captive_html =
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Hammy Setup</title>"
    "<style>"
    "body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif;"
    "background:#FFF8E7;color:#333;margin:0;padding:24px;}"
    ".box{max-width:360px;margin:0 auto;background:#fff;padding:24px;border-radius:16px;"
    "box-shadow:0 4px 20px rgba(0,0,0,0.08);}"
    "h1{font-size:22px;margin:0 0 8px;text-align:center;}"
    "p{font-size:14px;color:#666;margin:0 0 20px;text-align:center;}"
    "label{display:block;font-size:14px;font-weight:600;margin:12px 0 6px;}"
    "input{width:100%;box-sizing:border-box;padding:12px;border:1px solid #ddd;"
    "border-radius:10px;font-size:15px;background:#fafafa;}"
    "button{width:100%;margin-top:20px;padding:14px;border:none;border-radius:10px;"
    "background:#FF6B6B;color:#fff;font-size:16px;font-weight:600;}"
    "#scan-list{max-height:180px;overflow:auto;border:1px solid #eee;border-radius:10px;margin-bottom:12px;}"
    ".ssid-item{padding:10px 12px;border-bottom:1px solid #f0f0f0;font-size:15px;"
    "display:flex;justify-content:space-between;cursor:pointer;}"
    ".ssid-item:active{background:#f5f5f5;}"
    ".ssid-rssi{font-size:12px;color:#999;}"
    "</style>"
    "</head>"
    "<body>"
    "<div class='box'>"
    "<h1>WiFi 配置</h1>"
    "<p>选择您的 WiFi 网络并输入密码</p>"
    "<div id='scan-list'><p style='text-align:center;padding:12px;color:#999;'>正在扫描...</p></div>"
    "<p style='font-size:13px;color:#999;text-align:center;padding:8px;'>点击列表项可快速填入，每 10 秒刷新</p>"
    "<form action='/save' method='POST' autocomplete='off'>"
    "<label>WiFi 名称</label>"
    "<input name='ssid' id='ssid' placeholder='选择上方 Wi-Fi 或手动输入' required maxlength='32'>"
    "<label>WiFi 密码</label>"
    "<input name='pwd' type='password' placeholder='开放网络可留空' maxlength='64'>"
    "<button type='submit'>保存并连接</button>"
    "</form>"
    "</div>"
    "<script>"
    "function loadScan(){"
    "fetch('/scan').then(r=>r.json()).then(data=>{"
    "const list=document.getElementById('scan-list');"
    "if(!data.aps||data.aps.length===0){"
    "list.innerHTML='<p style=\"text-align:center;padding:12px;color:#999;\">未找到 Wi-Fi</p>';return;}"
    "let html='';"
    "data.aps.forEach(ap=>{"
    "html+=`<div class=\"ssid-item\" onclick='selectSsid(${JSON.stringify(ap.ssid)})'><span>${ap.ssid}</span><span class=\"ssid-rssi\">${ap.rssi}dBm ${ap.auth}</span></div>`;});"
    "list.innerHTML=html;}).catch(e=>{"
    "document.getElementById('scan-list').innerHTML='<p style=\"text-align:center;padding:12px;color:#999;\">扫描失败，请手动输入</p>';});}"
    "function selectSsid(ssid){document.getElementById('ssid').value=ssid;}"
    "loadScan();setInterval(loadScan,10000);"
    "</script>"
    "</body>"
    "</html>";

static int url_decode(const char *src, char *dst, size_t dst_len)
{
    size_t i = 0;
    size_t j = 0;

    while (src[i] != '\0' && j < dst_len - 1) {
        if (src[i] == '%' && isxdigit((unsigned char)src[i + 1]) && isxdigit((unsigned char)src[i + 2])) {
            char hex[3] = {src[i + 1], src[i + 2], '\0'};
            dst[j++] = (char)strtol(hex, NULL, 16);
            i += 3;
        } else if (src[i] == '+') {
            dst[j++] = ' ';
            i++;
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = '\0';
    return (int)j;
}

static bool form_find_value(const char *data, const char *key, char *out, size_t out_len)
{
    size_t key_len = strlen(key);
    const char *p = data;

    while (p != NULL) {
        const char *next = strchr(p, '&');
        const char *eq = strchr(p, '=');
        if (eq == NULL || (next != NULL && eq > next)) {
            if (next == NULL) {
                break;
            }
            p = next + 1;
            continue;
        }

        if ((size_t)(eq - p) == key_len && strncmp(p, key, key_len) == 0) {
            const char *val = eq + 1;
            size_t val_len = (next != NULL) ? (size_t)(next - val) : strlen(val);
            if (val_len >= out_len) {
                val_len = out_len - 1;
            }
            /* 临时缓冲区需容纳 URL 编码后的值，避免截断。
             * 输入 maxlength=64，URL 编码后最坏情况约 3 倍，留足余量。 */
            char tmp[256];
            if (val_len >= sizeof(tmp)) {
                val_len = sizeof(tmp) - 1;
            }
            memcpy(tmp, val, val_len);
            tmp[val_len] = '\0';
            url_decode(tmp, out, out_len);
            return true;
        }

        if (next == NULL) {
            break;
        }
        p = next + 1;
    }

    out[0] = '\0';
    return false;
}

static esp_err_t ap_root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, s_captive_html, strlen(s_captive_html));
    return ESP_OK;
}

static esp_err_t ap_save_handler(httpd_req_t *req)
{
    char buf[1024];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    char ssid[64] = {0};
    char password[64] = {0};

    form_find_value(buf, "ssid", ssid, sizeof(ssid));
    form_find_value(buf, "pwd", password, sizeof(password));

    if (ssid[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "ssid required");
        return ESP_FAIL;
    }

    if (s_config_saved) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/saved");
        httpd_resp_send(req, "", 0);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "save ssid=%s", ssid);

    service_nvs_set_wifi_ssid(ssid);
    service_nvs_set_wifi_password(password);
    s_config_saved = true;
    service_nvs_set_initialized(true);
    esp_err_t commit_ret = service_nvs_commit();
    ESP_LOGI(TAG, "nvs commit: %s", esp_err_to_name(commit_ret));

    /* POST-Redirect-GET：跳转到 /saved，避免浏览器重新加载 /save 时重复提交 */
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/saved");
    httpd_resp_send(req, "", 0);

    /* 保存后立即退出 AP 模式并触发 STA 重连，无需重启 */
    service_wifi_mark_ap_mode(false);
    service_wifi_reconnect_now();
    
    return ESP_OK;
}

static esp_err_t ap_saved_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    const char *msg = "<html><body style='font-family:sans-serif;text-align:center;padding-top:80px;'>"
                      "<h1>配置已保存</h1><p>设备正在连接 Wi-Fi...</p></body></html>";
    httpd_resp_send(req, msg, strlen(msg));
    return ESP_OK;
}

static esp_err_t ap_success_handler(httpd_req_t *req)
{
    httpd_resp_send(req, "success", 7);
    return ESP_OK;
}

static void json_escape(const char *src, char *dst, size_t dst_len)
{
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j < dst_len - 1; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == '"' || c == '\\') {
            if (j + 2 >= dst_len) {
                break;
            }
            dst[j++] = '\\';
            dst[j++] = (char)c;
        } else if (c < 0x20) {
            int n = snprintf(&dst[j], dst_len - j, "\\u%04x", c);
            if (n < 0 || (size_t)n >= dst_len - j) {
                break;
            }
            j += (size_t)n;
        } else {
            dst[j++] = (char)c;
        }
    }
    dst[j] = '\0';
}

static const char *auth_mode_str(wifi_auth_mode_t auth)
{
    switch (auth) {
        case WIFI_AUTH_OPEN: return "OPEN";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
        default: return "UNKNOWN";
    }
}

#define SCAN_MAX_AP 20

static int ap_rssi_cmp(const void *a, const void *b)
{
    const wifi_ap_record_t *aa = (const wifi_ap_record_t *)a;
    const wifi_ap_record_t *bb = (const wifi_ap_record_t *)b;
    return (int)bb->rssi - (int)aa->rssi;
}

static esp_err_t ap_scan_handler(httpd_req_t *req)
{
    wifi_scan_config_t scan_cfg = {0};
    scan_cfg.show_hidden = true;
    scan_cfg.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    scan_cfg.scan_time.active.min = 100;
    scan_cfg.scan_time.active.max = 300;

    esp_err_t ret = esp_wifi_scan_start(&scan_cfg, true);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "scan start failed: %d", ret);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "scan start failed");
        return ESP_FAIL;
    }

    uint16_t ap_num = 0;
    esp_wifi_scan_get_ap_num(&ap_num);
    if (ap_num > SCAN_MAX_AP) {
        ap_num = SCAN_MAX_AP;
    }

    wifi_ap_record_t *aps = NULL;
    if (ap_num > 0) {
        aps = (wifi_ap_record_t *)calloc(ap_num, sizeof(wifi_ap_record_t));
        if (aps == NULL) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no mem");
            return ESP_FAIL;
        }
    }

    uint16_t got = ap_num;
    if (ap_num > 0) {
        esp_wifi_scan_get_ap_records(&got, aps);
        if (got > 1) {
            qsort(aps, got, sizeof(wifi_ap_record_t), ap_rssi_cmp);
        }
    }

    char *resp = (char *)malloc(4096);
    if (resp == NULL) {
        free(aps);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no mem");
        return ESP_FAIL;
    }

    int pos = snprintf(resp, 4096, "{\"aps\":[");
    for (int i = 0; i < got; i++) {
        if (aps[i].ssid[0] == '\0') {
            continue;
        }
        if (pos > (int)strlen("{\"aps\":[")) {
            pos += snprintf(resp + pos, 4096 - pos, ",");
        }
        char ssid_escaped[128];
        json_escape((const char *)aps[i].ssid, ssid_escaped, sizeof(ssid_escaped));
        pos += snprintf(resp + pos, 4096 - pos,
                        "{\"ssid\":\"%s\",\"rssi\":%d,\"auth\":\"%s\"}",
                        ssid_escaped, aps[i].rssi, auth_mode_str(aps[i].authmode));
    }
    pos += snprintf(resp + pos, 4096 - pos, "]}");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, (ssize_t)pos);

    free(resp);
    free(aps);
    return ESP_OK;
}

/* 自定义域名解析：hammy.config -> 192.168.4.1 */
#define AP_DNS_HOSTNAME       "hammy.config"
#define AP_DNS_SERVER_PORT    53

static int s_dns_sock = -1;

static bool dns_qname_matches(const uint8_t *pkt, int pkt_len, int qname_offset,
                              const char *hostname)
{
    char qname[128] = {0};
    int off = qname_offset;
    int pos = 0;

    while (off < pkt_len && pos < (int)sizeof(qname) - 1) {
        uint8_t label_len = pkt[off++];
        if (label_len == 0) {
            break;
        }
        if ((label_len & 0xC0) == 0xC0) {
            off++;
            break;
        }
        if (pos > 0) {
            qname[pos++] = '.';
        }
        for (uint8_t i = 0; i < label_len && off < pkt_len && pos < (int)sizeof(qname) - 1; i++) {
            qname[pos++] = (char)tolower((unsigned char)pkt[off++]);
        }
        qname[pos] = '\0';
    }

    return (strcmp(qname, hostname) == 0);
}

static void dns_server_process(void)
{
    if (s_dns_sock < 0) {
        return;
    }

    uint8_t rx_buf[256];
    struct sockaddr_in src_addr;
    socklen_t addr_len = sizeof(src_addr);

    while (1) {
        int len = recvfrom(s_dns_sock, rx_buf, sizeof(rx_buf), 0,
                           (struct sockaddr *)&src_addr, &addr_len);
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            continue;
        }
        if (len < 12) {
            continue;
        }

        uint16_t qdcount = (uint16_t)((rx_buf[4] << 8) | rx_buf[5]);
        if (qdcount != 1) {
            continue;
        }

        bool match = dns_qname_matches(rx_buf, len, 12, AP_DNS_HOSTNAME);

        uint8_t tx_buf[256];
        memcpy(tx_buf, rx_buf, len);

        uint8_t rd = rx_buf[2] & 0x01;
        tx_buf[2] = 0x84 | rd;                       /* QR=1, AA=1, 保留 RD */
        tx_buf[3] = match ? (0x80 | rd) : 0x83;      /* RA=1, RCODE=0 或 NXDOMAIN */
        tx_buf[4] = 0; tx_buf[5] = 1;                /* QDCOUNT = 1 */
        tx_buf[6] = 0; tx_buf[7] = match ? 1 : 0;    /* ANCOUNT */
        tx_buf[8] = tx_buf[9] = tx_buf[10] = tx_buf[11] = 0;

        int tx_len = len;
        if (match) {
            uint8_t *p = tx_buf + tx_len;
            *p++ = 0xC0; *p++ = 0x0C;                /* 指向问题中的域名 */
            *p++ = 0x00; *p++ = 0x01;                /* Type A */
            *p++ = 0x00; *p++ = 0x01;                /* Class IN */
            *p++ = 0x00; *p++ = 0x00; *p++ = 0x00; *p++ = 0x3C; /* TTL 60 */
            *p++ = 0x00; *p++ = 0x04;                /* RDLENGTH */
            *p++ = 192; *p++ = 168; *p++ = 4; *p++ = 1;
            tx_len += 16;
        }

        sendto(s_dns_sock, tx_buf, tx_len, 0,
               (struct sockaddr *)&src_addr, addr_len);
    }
}

static void dns_server_open(void)
{
    if (s_dns_sock >= 0) {
        return;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "dns socket create failed");
        return;
    }

    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        ESP_LOGE(TAG, "dns set nonblock failed");
        close(sock);
        return;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(AP_DNS_SERVER_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        ESP_LOGE(TAG, "dns bind failed");
        close(sock);
        return;
    }

    s_dns_sock = sock;
    ESP_LOGI(TAG, "dns server ready for %s", AP_DNS_HOSTNAME);
}

static void dns_server_close(void)
{
    if (s_dns_sock >= 0) {
        close(s_dns_sock);
        s_dns_sock = -1;
    }
}

void service_wifi_ap_process(void)
{
    dns_server_process();
}

static esp_err_t http_server_start(void)
{
    if (s_httpd != NULL) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.ctrl_port = 32771;
    config.stack_size = 4096;
    config.task_priority = 5;
    config.lru_purge_enable = true;

    esp_err_t ret = httpd_start(&s_httpd, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd start failed: %d", ret);
        return ret;
    }

    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = ap_root_handler,
    };
    httpd_uri_t save_uri = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = ap_save_handler,
    };
    httpd_uri_t scan_uri = {
        .uri = "/scan",
        .method = HTTP_GET,
        .handler = ap_scan_handler,
    };
    httpd_uri_t saved_uri = {
        .uri = "/saved",
        .method = HTTP_GET,
        .handler = ap_saved_handler,
    };
    httpd_uri_t success_uri = {
        .uri = "/success.txt",
        .method = HTTP_GET,
        .handler = ap_success_handler,
    };

    httpd_register_uri_handler(s_httpd, &root_uri);
    httpd_register_uri_handler(s_httpd, &save_uri);
    httpd_register_uri_handler(s_httpd, &scan_uri);
    httpd_register_uri_handler(s_httpd, &saved_uri);
    httpd_register_uri_handler(s_httpd, &success_uri);

    return ESP_OK;
}

esp_err_t service_wifi_http_server_start(void)
{
    return http_server_start();
}

esp_err_t service_wifi_start_ap(void)
{
    s_config_saved = false;

    ESP_LOGI(TAG, "start captive portal AP: %s", AP_SSID);

    esp_err_t ret = service_wifi_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "wifi init failed before ap: %d", ret);
        return ret;
    }

    wifi_config_t ap_config = {0};
    strncpy((char *)ap_config.ap.ssid, AP_SSID, sizeof(ap_config.ap.ssid) - 1);
    ap_config.ap.ssid_len = (uint8_t)strlen(AP_SSID);
    strncpy((char *)ap_config.ap.password, AP_PASS, sizeof(ap_config.ap.password) - 1);
    ap_config.ap.max_connection = AP_MAX_CONN;
    ap_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    ap_config.ap.channel = AP_CHANNEL;
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);

    if (!s_ap_mode_started) {
        /* 首次启动：需要 stop → 切 APSTA 模式 → start。
         * 此后 WiFi 一直保持 APSTA，不再 stop/start 切模式，
         * 避免每次打开面板都断开 STA 连接。 */
        ret = esp_wifi_stop();
        if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_INIT) {
            ESP_LOGW(TAG, "stop wifi before ap failed: %d", ret);
        }

        ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "set ap mode failed: %d", ret);
            return ret;
        }

        if (s_ap_netif == NULL) {
            s_ap_netif = esp_netif_create_default_wifi_ap();
            if (s_ap_netif == NULL) {
                ESP_LOGE(TAG, "create ap netif failed");
                return ESP_FAIL;
            }
            esp_netif_ip_info_t ip_info = {
                .ip = { .addr = ESP_IP4TOADDR(192, 168, 4, 1) },
                .gw = { .addr = ESP_IP4TOADDR(192, 168, 4, 1) },
                .netmask = { .addr = ESP_IP4TOADDR(255, 255, 255, 0) },
            };
            esp_netif_set_ip_info(s_ap_netif, &ip_info);
            esp_netif_dhcps_stop(s_ap_netif);
            esp_ip4_addr_t dns_ip = { .addr = ESP_IP4TOADDR(192, 168, 4, 1) };
            esp_netif_dhcps_option(s_ap_netif, ESP_NETIF_OP_SET,
                                   ESP_NETIF_DOMAIN_NAME_SERVER,
                                   &dns_ip, sizeof(dns_ip));
        }

        esp_netif_dhcps_start(s_ap_netif);

        ret = esp_wifi_start();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "wifi start ap failed: %d", ret);
            return ret;
        }
        s_ap_mode_started = true;
    } else {
        /* 再次打开面板：WiFi 已在 APSTA 模式，只需重启 DHCP，
         * 完全不触碰 esp_wifi，STA 连接不受影响。 */
        if (s_ap_netif != NULL) {
            esp_netif_dhcps_stop(s_ap_netif);
            esp_netif_dhcps_start(s_ap_netif);
        }
    }

    service_wifi_mark_ap_mode(true);

    ret = service_wifi_http_server_start();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "http server start failed: %d", ret);
    }

    dns_server_open();

    ESP_LOGI(TAG, "config AP ready at http://hammy.config/ (192.168.4.1)");
    return ESP_OK;
}

esp_err_t service_wifi_stop_ap(void)
{
    ESP_LOGI(TAG, "stop captive portal AP");

    dns_server_close();

    service_wifi_mark_ap_mode(false);

    /* 关键：只停 DHCP，WiFi 保持 APSTA 模式，STA 连接不受影响。
     * s_ap_netif 永不销毁（已在上一轮修复中确认）。
     * s_ap_mode_started 保持 true，下次 start_ap 直接重启 DHCP。 */
    if (s_ap_netif != NULL) {
        esp_netif_dhcps_stop(s_ap_netif);
    }

    return ESP_OK;
}
