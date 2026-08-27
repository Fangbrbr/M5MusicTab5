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
#include "service_i18n.h"

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

#include "sdkconfig.h"

#if !CONFIG_BOARD_HAS_WIFI
/* 无 WiFi 板（JC4880P443）：SoftAP 配网整体降级为空操作，符号保留供调用方链接 */
static const char *TAG = "service_wifi_ap";

void service_wifi_ap_process(void)
{
}

esp_err_t service_wifi_http_server_start(void)
{
    ESP_LOGW(TAG, "board has no WiFi, http server disabled");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t service_wifi_start_ap(void)
{
    ESP_LOGW(TAG, "board has no WiFi, SoftAP disabled");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t service_wifi_stop_ap(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}
#else /* CONFIG_BOARD_HAS_WIFI */

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
    /* 语言选择器：右上角浮动按钮 */
    ".lang-switch{position:fixed;top:16px;right:16px;z-index:99;}"
    ".lang-btn{background:#fff;border:1px solid #e0e0e0;border-radius:20px;"
    "padding:6px 12px;font-size:18px;cursor:pointer;box-shadow:0 2px 8px rgba(0,0,0,0.08);"
    "line-height:1;}"
    ".lang-btn:active{background:#f5f5f5;}"
    ".lang-menu{display:none;position:absolute;top:42px;right:0;background:#fff;"
    "border:1px solid #e0e0e0;border-radius:10px;box-shadow:0 4px 16px rgba(0,0,0,0.1);"
    "overflow:hidden;min-width:110px;}"
    ".lang-menu.show{display:block;}"
    ".lang-menu-item{padding:10px 14px;cursor:pointer;font-size:14px;color:#333;"
    "display:flex;align-items:center;gap:8px;}"
    ".lang-menu-item:hover{background:#f5f5f5;}"
    ".lang-menu-item.active{background:#fff0f0;color:#FF6B6B;font-weight:600;}"
    ".lang-menu-item .flag{font-size:18px;}"
    "</style>"
    "</head>"
    "<body>"
    "<!-- 语言选择器：右上角浮动按钮 + 下拉菜单 -->"
    "<div class='lang-switch'>"
    "<button class='lang-btn' id='langBtn' title='Language'>🌐</button>"
    "<div class='lang-menu' id='langMenu'></div>"
    "</div>"
    "<div class='box'>"
    "<h1 data-i18n='title_wifi'>WiFi Setup</h1>"
    "<p data-i18n='select_wifi'>Select your Wi-Fi network and enter password</p>"
    "<div id='scan-list'><p data-i18n='scanning' style='text-align:center;padding:12px;color:#999;'>Scanning...</p></div>"
    "<p data-i18n='scan_hint' style='font-size:13px;color:#999;text-align:center;padding:8px;'>Tap list to fill, refresh every 10s</p>"
    "<form action='/save' method='POST' autocomplete='off'>"
    "<label data-i18n='wifi_name'>Wi-Fi Name</label>"
    "<input name='ssid' id='ssid' data-i18n-ph='choose_or_type' required maxlength='32'>"
    "<label data-i18n='wifi_pwd'>Wi-Fi Password</label>"
    "<input name='pwd' type='password' data-i18n-ph='open_network_blank' maxlength='64'>"
    "<button type='submit' data-i18n='save_connect'>Save and Connect</button>"
    "</form>"
    "</div>"
    "<script>"
    "/* ---- Web i18n 字典（与 translations.tsv 同步维护，key 需手动对齐） ---- */"
    "const WEB_I18N={"
    "'zh-CN':{"
    "title_wifi:'WiFi 配置',"
    "select_wifi:'选择您的 WiFi 网络并输入密码',"
    "scanning:'正在扫描...',"
    "scan_hint:'点击列表项可快速填入，每 10 秒刷新',"
    "wifi_name:'WiFi 名称',"
    "wifi_pwd:'WiFi 密码',"
    "choose_or_type:'选择上方 Wi-Fi 或手动输入',"
    "open_network_blank:'开放网络可留空',"
    "save_connect:'保存并连接',"
    "no_wifi:'未找到 Wi-Fi',"
    "scan_fail:'扫描失败，请手动输入'"
    "},"
    "en:{"
    "title_wifi:'WiFi Setup',"
    "select_wifi:'Select your Wi-Fi network and enter password',"
    "scanning:'Scanning...',"
    "scan_hint:'Tap list to fill, refresh every 10s',"
    "wifi_name:'Wi-Fi Name',"
    "wifi_pwd:'Wi-Fi Password',"
    "choose_or_type:'Select above or type manually',"
    "open_network_blank:'Leave blank for open networks',"
    "save_connect:'Save and Connect',"
    "no_wifi:'No Wi-Fi found',"
    "scan_fail:'Scan failed, please type manually'"
    "}"
    "};"
    "/* 语言 ID -> 显示标签（emoji 国旗 + 语言名） */"
    "const WEB_LANG_META={"
    "'zh-CN':{flag:'🇨🇳',name:'中文'},"
    "'en':{flag:'🇺🇸',name:'English'}"
    "};"
    "/* 当前激活语言（从设备 API 拉取，fallback 到浏览器或 zh-CN） */"
    "let curLang='zh-CN';"
    "/* ---- 应用翻译到 DOM ---- */"
    "function applyI18n(lang){"
    "const dict=WEB_I18N[lang]||WEB_I18N['zh-CN'];"
    "document.querySelectorAll('[data-i18n]').forEach(el=>{"
    "const key=el.dataset.i18n;"
    "if(dict[key]!==undefined) el.textContent=dict[key];});"
    "document.querySelectorAll('[data-i18n-ph]').forEach(el=>{"
    "const key=el.dataset.i18nPh;"
    "if(dict[key]!==undefined) el.placeholder=dict[key];});"
    "document.documentElement.lang=lang;"
    "curLang=lang;"
    "}"
    "/* ---- 构建语言下拉菜单 ---- */"
    "function buildLangMenu(available){"
    "const menu=document.getElementById('langMenu');"
    "menu.innerHTML='';"
    "available.forEach(id=>{"
    "const meta=WEB_LANG_META[id]||{flag:'🌐',name:id};"
    "const item=document.createElement('div');"
    "item.className='lang-menu-item'+(id===curLang?' active':'');"
    "item.innerHTML=`<span class='flag'>${meta.flag}</span><span>${meta.name}</span>`;"
    "item.onclick=()=>switchLang(id);"
    "menu.appendChild(item);});"
    "/* 关闭：点击外部收起 */"
    "document.onclick=(e)=>{"
    "const btn=document.getElementById('langBtn');"
    "if(!document.querySelector('.lang-switch').contains(e.target))"
    "{menu.classList.remove('show');}};"
    "}"
    "/* 切换语言：POST 到设备 + 刷新页面 */"
    "async function switchLang(id){"
    "try{"
    "await fetch('/api/lang',{"
    "method:'POST',"
    "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
    "body:'lang='+encodeURIComponent(id)"
    "});"
    "location.reload();"
    "}catch(e){"
    "applyI18n(id);"
    "document.getElementById('langMenu').classList.remove('show');"
    "buildLangMenu(Object.keys(WEB_I18N));"
    "}}"
    "/* 下拉按钮切换显示 */"
    "document.addEventListener('DOMContentLoaded',()=>{"
    "document.getElementById('langBtn').onclick=(e)=>{"
    "e.stopPropagation();"
    "document.getElementById('langMenu').classList.toggle('show');};"
    "});"
    "/* ---- 启动：拉取设备语言 ---- */"
    "async function initI18n(){"
    "try{"
    "const r=await fetch('/api/lang');"
    "const d=await r.json();"
    "if(d.lang&&WEB_I18N[d.lang]) curLang=d.lang;"
    "applyI18n(curLang);"
    "buildLangMenu(d.available||Object.keys(WEB_I18N));"
    "}catch(e){"
    "applyI18n(curLang);"
    "buildLangMenu(Object.keys(WEB_I18N));"
    "}}"
    "initI18n();"
    "/* ---- WiFi 扫描 ---- */"
    "function loadScan(){"
    "fetch('/scan').then(r=>r.json()).then(data=>{"
    "const list=document.getElementById('scan-list');"
    "const dict=WEB_I18N[curLang]||WEB_I18N['zh-CN'];"
    "if(!data.aps||data.aps.length===0){"
    "list.innerHTML=`<p style='text-align:center;padding:12px;color:#999;'>${dict.no_wifi}</p>`;return;}"
    "let html='';"
    "data.aps.forEach(ap=>{"
    "html+=`<div class='ssid-item' onclick='selectSsid(${JSON.stringify(ap.ssid)})'><span>${ap.ssid}</span><span class='ssid-rssi'>${ap.rssi}dBm ${ap.auth}</span></div>`;});"
    "list.innerHTML=html;}).catch(e=>{"
    "const dict=WEB_I18N[curLang]||WEB_I18N['zh-CN'];"
    "document.getElementById('scan-list').innerHTML=`<p style='text-align:center;padding:12px;color:#999;'>${dict.scan_fail}</p>`;});}"
    "function selectSsid(ssid){document.getElementById('ssid').value=ssid;}"
    "loadScan();setInterval(loadScan,10000);"
    "</script>"
    "</body>"
    "</html>";

/* ---- 旧 HTML 已替换为带语言选择器的版本 ---- */

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
    /* 极简 saved 页：语言由服务器当前设置决定（无客户端 JS），
     * 避免额外代码体积。中/英硬编码二选一。 */
    const char *cur = service_i18n_get_language_id();
    bool is_en = (cur != NULL && strcmp(cur, "en") == 0);

    const char *msg;
    if (is_en) {
        msg = "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
              "<meta name='viewport' content='width=device-width,initial-scale=1'>"
              "<title>Hammy Setup</title></head>"
              "<body style='font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif;"
              "background:#FFF8E7;color:#333;text-align:center;padding-top:80px;'>"
              "<h1>Config Saved</h1><p>Device is connecting to Wi-Fi...</p>"
              "<p style='color:#999;font-size:13px;margin-top:32px;'>"
              "You can close this page.</p></body></html>";
    } else {
        msg = "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
              "<meta name='viewport' content='width=device-width,initial-scale=1'>"
              "<title>Hammy Setup</title></head>"
              "<body style='font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif;"
              "background:#FFF8E7;color:#333;text-align:center;padding-top:80px;'>"
              "<h1>配置已保存</h1><p>设备正在连接 Wi-Fi...</p>"
              "<p style='color:#999;font-size:13px;margin-top:32px;'>"
              "可以关闭本页面</p></body></html>";
    }
    httpd_resp_send(req, msg, strlen(msg));
    return ESP_OK;
}

static esp_err_t ap_success_handler(httpd_req_t *req)
{
    httpd_resp_send(req, "success", 7);
    return ESP_OK;
}

/* /api/lang GET：返回当前设备语言与可用语言列表（JSON） */
static esp_err_t ap_lang_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json; charset=utf-8");

    const char *cur = service_i18n_get_language_id();
    if (cur == NULL) {
        cur = "zh-CN";
    }

    /* 直接引用 gen_i18n 生成的语言 ID 数组（头文件声明 extern） */
    const char * const *ids = g_i18n_language_ids;
    int count = I18N_LANG_COUNT;

    /* JSON 缓冲区：当前语言 key + available 数组。
     * 每条语言 ID 最长约 16 字符，加上引号和逗号；预留 512 足够 */
    char buf[512];
    int pos = snprintf(buf, sizeof(buf), "{\"lang\":\"%s\",\"available\":[", cur);

    for (int i = 0; i < count && pos < (int)sizeof(buf) - 64; i++) {
        if (i > 0) {
            pos += snprintf(buf + pos, sizeof(buf) - pos, ",");
        }
        pos += snprintf(buf + pos, sizeof(buf) - pos, "\"%s\"", ids[i]);
    }
    pos += snprintf(buf + pos, sizeof(buf) - pos, "]}");

    httpd_resp_send(req, buf, (ssize_t)pos);
    return ESP_OK;
}

/* /api/lang POST：设置设备语言，参数 lang=xx（form-urlencoded） */
static esp_err_t ap_lang_post_handler(httpd_req_t *req)
{
    char body[256];
    int ret = httpd_req_recv(req, body, sizeof(body) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty body");
        return ESP_FAIL;
    }
    body[ret] = '\0';

    char lang_id[SERVICE_I18N_LANG_ID_MAX_LEN] = {0};
    form_find_value(body, "lang", lang_id, sizeof(lang_id));

    if (lang_id[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "lang required");
        return ESP_FAIL;
    }

    bool ok = service_i18n_set_language_by_id(lang_id);
    if (!ok) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "unsupported lang");
        return ESP_FAIL;
    }

    /* 切语言后立即落盘，下次开机恢复 */
    service_nvs_commit();

    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_send(req, "{\"ok\":true}", 12);
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
    httpd_uri_t lang_get_uri = {
        .uri = "/api/lang",
        .method = HTTP_GET,
        .handler = ap_lang_get_handler,
    };
    httpd_uri_t lang_post_uri = {
        .uri = "/api/lang",
        .method = HTTP_POST,
        .handler = ap_lang_post_handler,
    };

    httpd_register_uri_handler(s_httpd, &root_uri);
    httpd_register_uri_handler(s_httpd, &save_uri);
    httpd_register_uri_handler(s_httpd, &scan_uri);
    httpd_register_uri_handler(s_httpd, &saved_uri);
    httpd_register_uri_handler(s_httpd, &success_uri);
    httpd_register_uri_handler(s_httpd, &lang_get_uri);
    httpd_register_uri_handler(s_httpd, &lang_post_uri);

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

#endif /* CONFIG_BOARD_HAS_WIFI */
