/**
 * @file service_ftp.c
 * @brief SD 卡 FTP 文件管理服务（lwIP 原生，非阻塞轮询状态机）
 *
 * 单会话 FTP 服务器：cmd 端口 21，仅被动模式（PASV/EPSV），固定凭据
 * musicpad/musicpad，chroot 沙箱于 /sdcard。协议语义参考
 * tools/SimpleFTPServer（MIT），socket/文件层按本项目非阻塞轮询模式重写：
 * 全 O_NONBLOCK socket，task_comm 每 10ms 驱动 service_ftp_process()，
 * 数据通道每拍最多 4 块 × 8KB，严禁忙等/阻塞循环。
 */

#include "service_ftp.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "service_ftp";

#if CONFIG_BOARD_HAS_WIFI

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

#include "service_sd.h"
#include "service_wifi.h"

#define FTP_CMD_PORT                21
#define FTP_USER                    "musicpad"
#define FTP_PASS                    "musicpad"

#define FTP_CMD_LINE_LEN            264     /* 命令行装配缓冲（含超长截断余量） */
#define FTP_REPLY_BUF_SIZE          640     /* 控制回包 pending 缓冲 */
#define FTP_REPLY_TMP_SIZE          320     /* 单条回包格式化上限（257 含长路径） */
#define FTP_DATA_BUF_SIZE           8192    /* PSRAM 数据缓冲 */
#define FTP_DATA_BLOCKS_PER_TICK    4       /* 数据通道每拍块数上限（有界返回） */
#define FTP_DIR_LINE_MARGIN         400     /* 目录行格式化的缓冲余量（行头~80+名字255） */

#define FTP_VIRT_PATH_MAX           256     /* 虚拟路径（"/" 为 SD 根） */
#define FTP_REAL_PATH_MAX           384     /* 真实路径（/sdcard 前缀 + 虚拟路径） */
#define FTP_SD_ROOT                 "/sdcard"

#define FTP_AUTH_TIMEOUT_MS         30000       /* 连接后 30s 未登录踢出 */
#define FTP_CTRL_IDLE_TIMEOUT_MS    (5 * 60000) /* 控制连接空闲 5 分钟关闭 */
#define FTP_PASV_ACCEPT_TIMEOUT_MS  30000       /* PASV data accept 等待 30s 回 425 */

/** @brief 传输类型（s_pending=等 data accept 的请求，s_xfer=进行中的传输） */
typedef enum {
    XFER_NONE = 0,
    XFER_LIST,
    XFER_NLST,
    XFER_MLSD,
    XFER_RETR,
    XFER_STOR,
} ftp_xfer_t;

static bool s_initialized = false;
static bool s_running = false;

static int s_listen_fd = -1;    /* cmd listen socket（stop 才关闭） */
static int s_cmd_fd = -1;       /* 控制连接 */
static int s_pasv_fd = -1;      /* PASV listen（accept 后即关） */
static int s_data_fd = -1;      /* 数据连接 */

/* 8KB 数据缓冲：start 时分配（PSRAM），stop 释放；内部 RAM 预算紧张，严禁静态 */
static uint8_t *s_buf = NULL;

static char s_cmd_line[FTP_CMD_LINE_LEN];
static size_t s_cmd_len = 0;
static char s_reply[FTP_REPLY_BUF_SIZE];
static size_t s_reply_len = 0;
static size_t s_reply_sent = 0;

static char s_cwd[FTP_VIRT_PATH_MAX] = "/";     /* 虚拟 cwd，"/" 即 /sdcard */
static bool s_user_ok = false;
static bool s_authed = false;
static TickType_t s_connect_tick = 0;
static TickType_t s_last_activity = 0;
static TickType_t s_pasv_deadline = 0;

static ftp_xfer_t s_pending = XFER_NONE;
static ftp_xfer_t s_xfer = XFER_NONE;
static char s_xfer_path[FTP_REAL_PATH_MAX] = {0};   /* 传输目标真实路径（LIST 为目录） */
static FILE *s_file = NULL;
static DIR *s_dir = NULL;
static size_t s_tx_len = 0;     /* s_buf 内待发/待处理字节 */
static size_t s_tx_sent = 0;    /* 已发送偏移（EAGAIN 续发） */
static long s_rest_offset = -1; /* REST 续传偏移，-1 无；下个传输命令一次性消费 */
static char s_rnfr[FTP_REAL_PATH_MAX] = {0};

/* 状态上报字段：写侧仅 task_comm，读侧仅 task_gui 显示用，允许良性撕裂 */
static service_ftp_state_t s_state = SERVICE_FTP_STATE_OFF;
static char s_client_ip[16] = {0};
static char s_file_name[256] = {0};
static uint32_t s_file_size = 0;
static uint32_t s_bytes_done = 0;

static void ftp_session_close(void);
static void ftp_close_fd(int *fd);
static void ftp_set_nonblock(int fd);
static bool ftp_eagain(void);
static void ftp_queue_reply(const char *fmt, ...);
static void ftp_flush_reply(void);
static bool ftp_resolve(const char *arg, char *out_real, size_t len,
                        char *out_virt, size_t vlen);
static void ftp_handle_line(char *line);
static void ftp_poll_commands(void);
static void ftp_cmd_cwd(const char *arg);
static void ftp_cmd_pasv(bool epsv);
static void ftp_cmd_dir(ftp_xfer_t type, const char *arg);
static void ftp_cmd_retr(const char *arg);
static void ftp_cmd_stor(const char *arg);
static void ftp_xfer_begin(void);
static void ftp_xfer_done(void);
static void ftp_xfer_abort(const char *what);
static void ftp_xfer_pump(void);
static bool ftp_send_pending(void);
static void ftp_pump_retr(void);
static void ftp_pump_stor(void);
static void ftp_pump_dir(void);
static size_t ftp_format_dir_line(char *out, size_t len, const char *name);

esp_err_t service_ftp_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }
    s_listen_fd = -1;
    s_cmd_fd = -1;
    s_pasv_fd = -1;
    s_data_fd = -1;
    s_state = SERVICE_FTP_STATE_OFF;
    s_initialized = true;
    ESP_LOGI(TAG, "init");
    return ESP_OK;
}

esp_err_t service_ftp_start(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_running) {
        return ESP_OK;
    }
    if (!service_sd_is_mounted()) {
        ESP_LOGW(TAG, "start refused: sd not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_buf == NULL) {
        s_buf = heap_caps_malloc(FTP_DATA_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (s_buf == NULL) {
            ESP_LOGE(TAG, "psram data buf alloc failed");
            return ESP_ERR_NO_MEM;
        }
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        ESP_LOGE(TAG, "socket failed errno=%d", errno);
        heap_caps_free(s_buf);
        s_buf = NULL;
        return ESP_FAIL;
    }
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons(FTP_CMD_PORT);
    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0 || listen(fd, 1) != 0) {
        ESP_LOGE(TAG, "bind/listen :%d failed errno=%d", FTP_CMD_PORT, errno);
        close(fd);
        heap_caps_free(s_buf);
        s_buf = NULL;
        return ESP_FAIL;
    }
    ftp_set_nonblock(fd);

    s_listen_fd = fd;
    s_running = true;
    ftp_session_close();    /* 复位会话字段（不关 listen），state → LISTENING */
    ESP_LOGI(TAG, "ftp server listening on :%d", FTP_CMD_PORT);
    return ESP_OK;
}

void service_ftp_stop(void)
{
    if (!s_running) {
        return;
    }
    ftp_session_close();
    ftp_close_fd(&s_listen_fd);
    if (s_buf != NULL) {
        heap_caps_free(s_buf);
        s_buf = NULL;
    }
    s_running = false;
    s_state = SERVICE_FTP_STATE_OFF;
    ESP_LOGI(TAG, "ftp server stopped");
}

void service_ftp_process(void)
{
    if (!s_initialized || !s_running) {
        return;
    }

    /* WiFi 掉线：复位会话（关 cmd/data/pasv）但保留 listen，重连后客户端重连即可 */
    if (!service_wifi_is_connected()) {
        if (s_cmd_fd >= 0 || s_data_fd >= 0 || s_pasv_fd >= 0) {
            ESP_LOGW(TAG, "wifi lost, reset ftp session");
            ftp_session_close();
        }
        return;
    }

    /* 新控制连接：单会话，踢旧迎新 */
    if (s_listen_fd >= 0) {
        struct sockaddr_in ca;
        socklen_t cl = sizeof(ca);
        int fd = accept(s_listen_fd, (struct sockaddr *)&ca, &cl);
        if (fd >= 0) {
            if (s_cmd_fd >= 0) {
                ESP_LOGI(TAG, "new client, drop old session");
                ftp_session_close();
            }
            ftp_set_nonblock(fd);
            s_cmd_fd = fd;
            snprintf(s_client_ip, sizeof(s_client_ip), "%s", inet_ntoa(ca.sin_addr));
            s_connect_tick = xTaskGetTickCount();
            s_last_activity = s_connect_tick;
            s_state = SERVICE_FTP_STATE_CONNECTED;
            ESP_LOGI(TAG, "client connected: %s", s_client_ip);
            ftp_queue_reply("220 MusicPad FTP Server Ready\r\n");
        }
    }

    ftp_flush_reply();

    /* PASV data accept：轮询等待，30s 无连接回 425 */
    if (s_pasv_fd >= 0 && s_data_fd < 0) {
        struct sockaddr_in da;
        socklen_t dl = sizeof(da);
        int fd = accept(s_pasv_fd, (struct sockaddr *)&da, &dl);
        if (fd >= 0) {
            ftp_set_nonblock(fd);
            s_data_fd = fd;
            ftp_close_fd(&s_pasv_fd);
        } else if ((int32_t)(xTaskGetTickCount() - s_pasv_deadline) >= 0) {
            ESP_LOGW(TAG, "pasv accept timeout");
            ftp_close_fd(&s_pasv_fd);
            if (s_pending != XFER_NONE) {
                s_pending = XFER_NONE;
                ftp_queue_reply("425 Can't open data connection\r\n");
            }
        }
    }

    /* 控制通道超时：认证前 30s 未登录踢出；空闲 5 分钟关闭。
     * 传输期间控制通道本就静默，不计空闲超时。 */
    if (s_cmd_fd >= 0) {
        TickType_t now = xTaskGetTickCount();
        bool auth_timeout = !s_authed &&
            (now - s_connect_tick) > pdMS_TO_TICKS(FTP_AUTH_TIMEOUT_MS);
        bool idle_timeout = (s_xfer == XFER_NONE && s_pending == XFER_NONE) &&
            (now - s_last_activity) > pdMS_TO_TICKS(FTP_CTRL_IDLE_TIMEOUT_MS);
        if (auth_timeout || idle_timeout) {
            ESP_LOGW(TAG, "control timeout (auth=%d idle=%d)",
                     (int)auth_timeout, (int)idle_timeout);
            ftp_queue_reply("421 Timeout\r\n");
            ftp_flush_reply();
            ftp_session_close();
            return;
        }
    }

    /* data 通道就绪：启动挂起的传输请求（150/550） */
    if (s_pending != XFER_NONE && s_data_fd >= 0 && s_reply_len == 0) {
        ftp_xfer_begin();
    }

    ftp_poll_commands();
    ftp_xfer_pump();
}

void service_ftp_get_status(service_ftp_status_t *out)
{
    if (out == NULL) {
        return;
    }
    out->state = s_state;
    snprintf(out->client_ip, sizeof(out->client_ip), "%s", s_client_ip);
    snprintf(out->file_name, sizeof(out->file_name), "%s", s_file_name);
    out->file_size = s_file_size;
    out->bytes_done = s_bytes_done;
}

/* -------------------------------------------------------------------------- */
/* static 函数                                                                 */
/* -------------------------------------------------------------------------- */

static void ftp_close_fd(int *fd)
{
    if (fd != NULL && *fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}

static void ftp_set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

static bool ftp_eagain(void)
{
    return errno == EAGAIN || errno == EWOULDBLOCK;
}

/* 会话复位：中止传输、关闭 cmd/data/pasv、清认证与 cwd；保留 listen socket */
static void ftp_session_close(void)
{
    if (s_file != NULL) {
        fclose(s_file);
        s_file = NULL;
    }
    if (s_dir != NULL) {
        closedir(s_dir);
        s_dir = NULL;
    }
    ftp_close_fd(&s_data_fd);
    ftp_close_fd(&s_pasv_fd);
    ftp_close_fd(&s_cmd_fd);
    s_cmd_len = 0;
    s_reply_len = 0;
    s_reply_sent = 0;
    s_tx_len = 0;
    s_tx_sent = 0;
    s_pending = XFER_NONE;
    s_xfer = XFER_NONE;
    s_user_ok = false;
    s_authed = false;
    s_rest_offset = -1;
    s_rnfr[0] = '\0';
    s_client_ip[0] = '\0';
    s_cwd[0] = '/';
    s_cwd[1] = '\0';
    if (s_running) {
        s_state = SERVICE_FTP_STATE_LISTENING;
    }
}

/* 回包入 pending 缓冲（可拼接多条，先尽力冲）；EAGAIN 下拍由 process 续发 */
static void ftp_queue_reply(const char *fmt, ...)
{
    char tmp[FTP_REPLY_TMP_SIZE];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n <= 0) {
        return;
    }
    if (n >= (int)sizeof(tmp)) {
        n = (int)sizeof(tmp) - 1;
    }
    if (s_reply_len + (size_t)n > sizeof(s_reply)) {
        ftp_flush_reply();
        if (s_reply_len + (size_t)n > sizeof(s_reply)) {
            /* 协议串行下不应发生；丢弃新包比溢出安全 */
            ESP_LOGW(TAG, "reply buf full, drop reply");
            return;
        }
    }
    memcpy(s_reply + s_reply_len, tmp, (size_t)n);
    s_reply_len += (size_t)n;
}

static void ftp_flush_reply(void)
{
    if (s_cmd_fd < 0) {
        return;
    }
    while (s_reply_sent < s_reply_len) {
        int n = send(s_cmd_fd, s_reply + s_reply_sent, s_reply_len - s_reply_sent, 0);
        if (n > 0) {
            s_reply_sent += (size_t)n;
            continue;
        }
        if (n < 0 && ftp_eagain()) {
            return;
        }
        ESP_LOGW(TAG, "reply send failed errno=%d", errno);
        ftp_session_close();
        return;
    }
    s_reply_len = 0;
    s_reply_sent = 0;
}

/* 路径沙箱：虚拟路径拼接 cwd 后规范化（. .. 重复斜杠 尾斜杠），
 * .. 逃逸出根即失败（550）；输出真实路径 = /sdcard + 虚拟路径 */
static bool ftp_resolve(const char *arg, char *out_real, size_t len,
                        char *out_virt, size_t vlen)
{
    /* joined 按最坏情况 s_cwd + "/" + arg（命令行缓冲上限）定尺寸，
     * 避免拼接截断；越长的规范化结果在下方逐段重组时被显式拒绝 */
    char joined[FTP_VIRT_PATH_MAX + FTP_CMD_LINE_LEN + 2];
    if (arg == NULL || arg[0] == '\0') {
        snprintf(joined, sizeof(joined), "%s", s_cwd);
    } else if (arg[0] == '/') {
        snprintf(joined, sizeof(joined), "%s", arg);
    } else if (strcmp(s_cwd, "/") == 0) {
        snprintf(joined, sizeof(joined), "/%s", arg);
    } else {
        snprintf(joined, sizeof(joined), "%s/%s", s_cwd, arg);
    }

    char *segments[32];
    int depth = 0;
    char *save = NULL;
    char *seg = strtok_r(joined, "/", &save);
    while (seg != NULL) {
        if (strcmp(seg, ".") == 0) {
            /* 丢弃 */
        } else if (strcmp(seg, "..") == 0) {
            if (depth == 0) {
                return false;   /* 越根逃逸 */
            }
            depth--;
        } else {
            if (depth >= 32) {
                return false;
            }
            segments[depth++] = seg;
        }
        seg = strtok_r(NULL, "/", &save);
    }

    char virt[FTP_VIRT_PATH_MAX];
    size_t used = 0;
    virt[0] = '\0';
    if (depth == 0) {
        virt[0] = '/';
        virt[1] = '\0';
    }
    for (int i = 0; i < depth; i++) {
        int n = snprintf(virt + used, sizeof(virt) - used, "/%s", segments[i]);
        if (n < 0 || (size_t)n >= sizeof(virt) - used) {
            return false;   /* 截断即非法 */
        }
        used += (size_t)n;
    }

    int n;
    if (strcmp(virt, "/") == 0) {
        n = snprintf(out_real, len, "%s", FTP_SD_ROOT);
    } else {
        n = snprintf(out_real, len, "%s%s", FTP_SD_ROOT, virt);
    }
    if (n < 0 || (size_t)n >= len) {
        return false;
    }
    if (out_virt != NULL) {
        snprintf(out_virt, vlen, "%s", virt);
    }
    return true;
}

static void ftp_poll_commands(void)
{
    if (s_cmd_fd < 0) {
        return;
    }

    /* 收新数据装配命令行；回包 pending 未空时不收（TCP 窗口自然反压） */
    if (s_reply_len == 0 && s_cmd_len < sizeof(s_cmd_line) - 1) {
        int n = recv(s_cmd_fd, s_cmd_line + s_cmd_len,
                     sizeof(s_cmd_line) - 1 - s_cmd_len, 0);
        if (n == 0) {
            ESP_LOGI(TAG, "client closed control connection");
            ftp_session_close();
            return;
        }
        if (n < 0) {
            if (!ftp_eagain()) {
                ESP_LOGW(TAG, "cmd recv failed errno=%d", errno);
                ftp_session_close();
                return;
            }
        } else {
            s_cmd_len += (size_t)n;
            s_last_activity = xTaskGetTickCount();
        }
    }

    /* 逐条取出 \r\n 完整命令处理；回包 pending 则停手，缓冲留待下拍 */
    size_t pos = 0;
    for (size_t i = 0; i < s_cmd_len; i++) {
        if (s_cmd_line[i] != '\n') {
            continue;
        }
        size_t linelen = i - pos;
        if (linelen > 0 && s_cmd_line[pos + linelen - 1] == '\r') {
            linelen--;
        }
        char line[FTP_CMD_LINE_LEN];
        if (linelen >= sizeof(line)) {
            linelen = sizeof(line) - 1;
        }
        memcpy(line, s_cmd_line + pos, linelen);
        line[linelen] = '\0';
        ftp_handle_line(line);
        ftp_flush_reply();
        pos = i + 1;
        if (s_cmd_fd < 0) {
            return;   /* QUIT/错误已关闭会话 */
        }
        if (s_reply_len > 0) {
            break;
        }
    }
    if (pos > 0) {
        memmove(s_cmd_line, s_cmd_line + pos, s_cmd_len - pos);
        s_cmd_len -= pos;
    }
    if (s_cmd_len >= sizeof(s_cmd_line) - 1) {
        /* 超长行无换行：协议异常，丢弃缓冲防卡死 */
        s_cmd_len = 0;
    }
}

static void ftp_handle_line(char *line)
{
    ESP_LOGD(TAG, "cmd: %s", line);

    /* 路径参数可能含空格：首个空格后整段即参数，不再切分 */
    char *arg = strchr(line, ' ');
    if (arg != NULL) {
        *arg = '\0';
        arg++;
        while (*arg == ' ') {
            arg++;
        }
    } else {
        arg = (char *)"";
    }

    /* 未认证仅放行 USER/PASS/QUIT */
    if (!s_authed &&
        strcasecmp(line, "USER") != 0 &&
        strcasecmp(line, "PASS") != 0 &&
        strcasecmp(line, "QUIT") != 0) {
        ftp_queue_reply("530 Not logged in\r\n");
        return;
    }

    if (strcasecmp(line, "USER") == 0) {
        s_user_ok = (strcmp(arg, FTP_USER) == 0);
        ftp_queue_reply(s_user_ok ? "331 Password required\r\n"
                                  : "530 Invalid user\r\n");
    } else if (strcasecmp(line, "PASS") == 0) {
        if (s_user_ok && strcmp(arg, FTP_PASS) == 0) {
            s_authed = true;
            ESP_LOGI(TAG, "client logged in: %s", s_client_ip);
            ftp_queue_reply("230 Logged in\r\n");
        } else {
            ftp_queue_reply("530 Invalid password\r\n");
        }
    } else if (strcasecmp(line, "SYST") == 0) {
        ftp_queue_reply("215 UNIX Type: L8\r\n");
    } else if (strcasecmp(line, "FEAT") == 0) {
        ftp_queue_reply("211-Extensions supported:\r\n UTF8\r\n MLSD\r\n"
                        " REST STREAM\r\n SIZE\r\n MDTM\r\n211 End\r\n");
    } else if (strcasecmp(line, "NOOP") == 0) {
        ftp_queue_reply("200 OK\r\n");
    } else if (strcasecmp(line, "TYPE") == 0) {
        /* 始终按二进制传输，I/A 均接受 */
        ftp_queue_reply("200 Binary mode\r\n");
    } else if (strcasecmp(line, "PWD") == 0 || strcasecmp(line, "XPWD") == 0) {
        ftp_queue_reply("257 \"%s\" is current directory\r\n", s_cwd);
    } else if (strcasecmp(line, "CWD") == 0 || strcasecmp(line, "XCWD") == 0) {
        ftp_cmd_cwd(arg);
    } else if (strcasecmp(line, "CDUP") == 0 || strcasecmp(line, "XCUP") == 0) {
        ftp_cmd_cwd("..");
    } else if (strcasecmp(line, "PASV") == 0) {
        ftp_cmd_pasv(false);
    } else if (strcasecmp(line, "EPSV") == 0) {
        ftp_cmd_pasv(true);
    } else if (strcasecmp(line, "PORT") == 0 || strcasecmp(line, "EPRT") == 0) {
        ftp_queue_reply("502 Active mode not supported\r\n");
    } else if (strcasecmp(line, "LIST") == 0) {
        ftp_cmd_dir(XFER_LIST, arg);
    } else if (strcasecmp(line, "NLST") == 0) {
        ftp_cmd_dir(XFER_NLST, arg);
    } else if (strcasecmp(line, "MLSD") == 0) {
        ftp_cmd_dir(XFER_MLSD, arg);
    } else if (strcasecmp(line, "RETR") == 0) {
        ftp_cmd_retr(arg);
    } else if (strcasecmp(line, "STOR") == 0) {
        ftp_cmd_stor(arg);
    } else if (strcasecmp(line, "DELE") == 0) {
        char real[FTP_REAL_PATH_MAX];
        if (!ftp_resolve(arg, real, sizeof(real), NULL, 0) || remove(real) != 0) {
            ftp_queue_reply("550 Delete failed\r\n");
        } else {
            ftp_queue_reply("250 Deleted\r\n");
        }
    } else if (strcasecmp(line, "RMD") == 0 || strcasecmp(line, "XRMD") == 0) {
        char real[FTP_REAL_PATH_MAX];
        if (!ftp_resolve(arg, real, sizeof(real), NULL, 0) || rmdir(real) != 0) {
            ftp_queue_reply("550 Remove directory failed\r\n");
        } else {
            ftp_queue_reply("250 Directory removed\r\n");
        }
    } else if (strcasecmp(line, "MKD") == 0 || strcasecmp(line, "XMKD") == 0) {
        char real[FTP_REAL_PATH_MAX];
        char virt[FTP_VIRT_PATH_MAX];
        if (!ftp_resolve(arg, real, sizeof(real), virt, sizeof(virt)) ||
            mkdir(real, 0755) != 0) {
            ftp_queue_reply("550 Create directory failed\r\n");
        } else {
            ftp_queue_reply("257 \"%s\" created\r\n", virt);
        }
    } else if (strcasecmp(line, "RNFR") == 0) {
        char real[FTP_REAL_PATH_MAX];
        struct stat st;
        if (!ftp_resolve(arg, real, sizeof(real), NULL, 0) || stat(real, &st) != 0) {
            ftp_queue_reply("550 File not found\r\n");
        } else {
            snprintf(s_rnfr, sizeof(s_rnfr), "%s", real);
            ftp_queue_reply("350 Ready for destination\r\n");
        }
    } else if (strcasecmp(line, "RNTO") == 0) {
        char real[FTP_REAL_PATH_MAX];
        if (s_rnfr[0] == '\0') {
            ftp_queue_reply("503 RNFR required first\r\n");
        } else if (!ftp_resolve(arg, real, sizeof(real), NULL, 0) ||
                   rename(s_rnfr, real) != 0) {
            s_rnfr[0] = '\0';
            ftp_queue_reply("550 Rename failed\r\n");
        } else {
            s_rnfr[0] = '\0';
            ftp_queue_reply("250 Renamed\r\n");
        }
    } else if (strcasecmp(line, "SIZE") == 0) {
        char real[FTP_REAL_PATH_MAX];
        struct stat st;
        if (!ftp_resolve(arg, real, sizeof(real), NULL, 0) ||
            stat(real, &st) != 0 || !S_ISREG(st.st_mode)) {
            ftp_queue_reply("550 No such file\r\n");
        } else {
            ftp_queue_reply("213 %lu\r\n", (unsigned long)st.st_size);
        }
    } else if (strcasecmp(line, "MDTM") == 0) {
        char real[FTP_REAL_PATH_MAX];
        struct stat st;
        if (!ftp_resolve(arg, real, sizeof(real), NULL, 0) || stat(real, &st) != 0) {
            ftp_queue_reply("550 No such file\r\n");
        } else {
            char tbuf[16];
            struct tm tmv;
            time_t mt = st.st_mtime;
            gmtime_r(&mt, &tmv);
            strftime(tbuf, sizeof(tbuf), "%Y%m%d%H%M%S", &tmv);
            ftp_queue_reply("213 %s\r\n", tbuf);
        }
    } else if (strcasecmp(line, "REST") == 0) {
        char *end = NULL;
        long off = strtol(arg, &end, 10);
        if (end == arg || off < 0) {
            ftp_queue_reply("501 Bad restart offset\r\n");
        } else {
            s_rest_offset = off;
            ftp_queue_reply("350 Restart position accepted\r\n");
        }
    } else if (strcasecmp(line, "OPTS") == 0) {
        /* OPTS UTF8 ON 等：名义接受，路径本就按 UTF-8 透传 */
        ftp_queue_reply("200 OK\r\n");
    } else if (strcasecmp(line, "ALLO") == 0) {
        ftp_queue_reply("200 OK\r\n");
    } else if (strcasecmp(line, "STAT") == 0) {
        ftp_queue_reply("211-FTP server status:\r\n Connected: %s\r\n211 End\r\n",
                        s_client_ip);
    } else if (strcasecmp(line, "HELP") == 0) {
        ftp_queue_reply("214 MusicPad FTP: USER PASS SYST FEAT NOOP TYPE PWD CWD CDUP "
                        "PASV EPSV LIST NLST MLSD RETR STOR DELE RMD MKD RNFR RNTO "
                        "SIZE MDTM REST OPTS ALLO STAT QUIT HELP\r\n");
    } else if (strcasecmp(line, "QUIT") == 0) {
        ftp_queue_reply("221 Goodbye\r\n");
        ftp_flush_reply();
        ESP_LOGI(TAG, "client quit: %s", s_client_ip);
        ftp_session_close();
    } else {
        ftp_queue_reply("502 Command not implemented\r\n");
    }
}

static void ftp_cmd_cwd(const char *arg)
{
    char real[FTP_REAL_PATH_MAX];
    char virt[FTP_VIRT_PATH_MAX];
    struct stat st;
    if (!ftp_resolve(arg, real, sizeof(real), virt, sizeof(virt)) ||
        stat(real, &st) != 0 || !S_ISDIR(st.st_mode)) {
        ftp_queue_reply("550 Not a directory\r\n");
        return;
    }
    snprintf(s_cwd, sizeof(s_cwd), "%s", virt);
    ftp_queue_reply("250 Directory changed\r\n");
}

static void ftp_cmd_pasv(bool epsv)
{
    /* 新 PASV 废弃旧数据通道：客户端每次传输前重新协商 */
    ftp_close_fd(&s_data_fd);
    ftp_close_fd(&s_pasv_fd);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        ESP_LOGW(TAG, "pasv socket failed errno=%d", errno);
        ftp_queue_reply("425 Can't open data connection\r\n");
        return;
    }
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons(0);     /* ephemeral 端口 */
    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0 || listen(fd, 1) != 0) {
        ESP_LOGW(TAG, "pasv bind/listen failed errno=%d", errno);
        close(fd);
        ftp_queue_reply("425 Can't open data connection\r\n");
        return;
    }
    ftp_set_nonblock(fd);

    struct sockaddr_in bound;
    socklen_t bl = sizeof(bound);
    struct sockaddr_in local;
    socklen_t ll = sizeof(local);
    if (getsockname(fd, (struct sockaddr *)&bound, &bl) != 0 ||
        getsockname(s_cmd_fd, (struct sockaddr *)&local, &ll) != 0) {
        ESP_LOGW(TAG, "pasv getsockname failed errno=%d", errno);
        close(fd);
        ftp_queue_reply("425 Can't open data connection\r\n");
        return;
    }
    uint16_t port = ntohs(bound.sin_port);

    s_pasv_fd = fd;
    s_pasv_deadline = xTaskGetTickCount() + pdMS_TO_TICKS(FTP_PASV_ACCEPT_TIMEOUT_MS);

    if (epsv) {
        ftp_queue_reply("229 Entering Extended Passive Mode (|||%u|)\r\n",
                        (unsigned)port);
    } else {
        /* 应答 IP 取控制连接本地地址（STA 当前 IP），不依赖硬编码 */
        const uint8_t *ip = (const uint8_t *)&local.sin_addr.s_addr;
        ftp_queue_reply("227 Entering Passive Mode (%u,%u,%u,%u,%u,%u)\r\n",
                        (unsigned)ip[0], (unsigned)ip[1],
                        (unsigned)ip[2], (unsigned)ip[3],
                        (unsigned)(port >> 8), (unsigned)(port & 0xFF));
    }
}

static void ftp_cmd_dir(ftp_xfer_t type, const char *arg)
{
    if (s_xfer != XFER_NONE || s_pending != XFER_NONE) {
        ftp_queue_reply("425 Transfer already in progress\r\n");
        return;
    }
    if (s_pasv_fd < 0 && s_data_fd < 0) {
        ftp_queue_reply("425 Use PASV first\r\n");
        return;
    }
    char real[FTP_REAL_PATH_MAX];
    struct stat st;
    if (!ftp_resolve(arg, real, sizeof(real), NULL, 0) ||
        stat(real, &st) != 0 || !S_ISDIR(st.st_mode)) {
        ftp_queue_reply("550 Not a directory\r\n");
        return;
    }
    snprintf(s_xfer_path, sizeof(s_xfer_path), "%s", real);
    s_file_name[0] = '\0';
    s_file_size = 0;
    s_bytes_done = 0;
    s_pending = type;
}

static void ftp_cmd_retr(const char *arg)
{
    if (s_xfer != XFER_NONE || s_pending != XFER_NONE) {
        ftp_queue_reply("425 Transfer already in progress\r\n");
        return;
    }
    if (s_pasv_fd < 0 && s_data_fd < 0) {
        ftp_queue_reply("425 Use PASV first\r\n");
        return;
    }
    char real[FTP_REAL_PATH_MAX];
    struct stat st;
    if (!ftp_resolve(arg, real, sizeof(real), NULL, 0) ||
        stat(real, &st) != 0 || !S_ISREG(st.st_mode)) {
        ftp_queue_reply("550 No such file\r\n");
        return;
    }
    snprintf(s_xfer_path, sizeof(s_xfer_path), "%s", real);
    const char *base = strrchr(real, '/');
    /* Trap: basename 上限为 FatFs LFN 255，strlcpy 有界拷贝规避 format-truncation */
    strlcpy(s_file_name, base != NULL ? base + 1 : real, sizeof(s_file_name));
    s_file_size = (uint32_t)st.st_size;
    s_bytes_done = 0;
    s_pending = XFER_RETR;
}

static void ftp_cmd_stor(const char *arg)
{
    if (s_xfer != XFER_NONE || s_pending != XFER_NONE) {
        ftp_queue_reply("425 Transfer already in progress\r\n");
        return;
    }
    if (s_pasv_fd < 0 && s_data_fd < 0) {
        ftp_queue_reply("425 Use PASV first\r\n");
        return;
    }
    char real[FTP_REAL_PATH_MAX];
    if (!ftp_resolve(arg, real, sizeof(real), NULL, 0)) {
        ftp_queue_reply("550 Invalid path\r\n");
        return;
    }
    snprintf(s_xfer_path, sizeof(s_xfer_path), "%s", real);
    const char *base = strrchr(real, '/');
    /* Trap: basename 上限为 FatFs LFN 255，strlcpy 有界拷贝规避 format-truncation */
    strlcpy(s_file_name, base != NULL ? base + 1 : real, sizeof(s_file_name));
    s_file_size = 0;    /* 上传长度服务端不可预知，页面按已传字节显示 */
    s_bytes_done = 0;
    s_pending = XFER_STOR;
}

/* data 通道已就绪：打开文件/目录并回 150，失败回 550 并关 data */
static void ftp_xfer_begin(void)
{
    ftp_xfer_t type = s_pending;
    s_pending = XFER_NONE;

    switch (type) {
    case XFER_LIST:
    case XFER_NLST:
    case XFER_MLSD:
        s_dir = opendir(s_xfer_path);
        if (s_dir == NULL) {
            ESP_LOGW(TAG, "opendir failed: %s errno=%d", s_xfer_path, errno);
            ftp_close_fd(&s_data_fd);
            ftp_queue_reply("550 Failed to open directory\r\n");
            return;
        }
        break;
    case XFER_RETR:
        s_file = fopen(s_xfer_path, "rb");
        if (s_file != NULL && s_rest_offset >= 0 &&
            fseek(s_file, s_rest_offset, SEEK_SET) != 0) {
            fclose(s_file);
            s_file = NULL;
        }
        if (s_file == NULL) {
            ftp_close_fd(&s_data_fd);
            s_rest_offset = -1;
            ftp_queue_reply("550 Failed to open file\r\n");
            return;
        }
        s_bytes_done = (s_rest_offset >= 0) ? (uint32_t)s_rest_offset : 0;
        break;
    case XFER_STOR:
        if (s_rest_offset >= 0) {
            s_file = fopen(s_xfer_path, "r+b");
            if (s_file != NULL && fseek(s_file, s_rest_offset, SEEK_SET) != 0) {
                fclose(s_file);
                s_file = NULL;
            }
        } else {
            s_file = fopen(s_xfer_path, "wb");
        }
        if (s_file == NULL) {
            ftp_close_fd(&s_data_fd);
            s_rest_offset = -1;
            ftp_queue_reply("550 Failed to create file\r\n");
            return;
        }
        break;
    default:
        return;
    }

    /* REST 偏移一次性消费（RFC 959：下个传输命令用后失效） */
    s_rest_offset = -1;
    s_xfer = type;
    s_tx_len = 0;
    s_tx_sent = 0;
    s_state = SERVICE_FTP_STATE_TRANSFERRING;
    ESP_LOGI(TAG, "transfer begin: type=%d %s", (int)type, s_xfer_path);
    ftp_queue_reply("150 Opening data connection\r\n");
}

/* 传输正常收尾：关文件/数据通道，回 226，状态回落 CONNECTED */
static void ftp_xfer_done(void)
{
    if (s_file != NULL) {
        fclose(s_file);
        s_file = NULL;
    }
    if (s_dir != NULL) {
        closedir(s_dir);
        s_dir = NULL;
    }
    ftp_close_fd(&s_data_fd);
    s_xfer = XFER_NONE;
    s_tx_len = 0;
    s_tx_sent = 0;
    s_state = SERVICE_FTP_STATE_CONNECTED;
    ESP_LOGI(TAG, "transfer done: %s %lu bytes",
             s_file_name, (unsigned long)s_bytes_done);
    ftp_queue_reply("226 Transfer complete\r\n");
}

/* 数据通道硬错误中止：关传输，回 426；控制连接已死则整会话复位 */
static void ftp_xfer_abort(const char *what)
{
    ESP_LOGW(TAG, "transfer aborted: %s errno=%d", what, errno);
    if (s_file != NULL) {
        fclose(s_file);
        s_file = NULL;
    }
    if (s_dir != NULL) {
        closedir(s_dir);
        s_dir = NULL;
    }
    ftp_close_fd(&s_data_fd);
    s_xfer = XFER_NONE;
    s_tx_len = 0;
    s_tx_sent = 0;
    if (s_cmd_fd >= 0) {
        s_state = SERVICE_FTP_STATE_CONNECTED;
        ftp_queue_reply("426 Connection closed; transfer aborted\r\n");
    } else {
        ftp_session_close();
    }
}

static void ftp_xfer_pump(void)
{
    if (s_data_fd < 0) {
        return;
    }
    switch (s_xfer) {
    case XFER_RETR:
        ftp_pump_retr();
        break;
    case XFER_STOR:
        ftp_pump_stor();
        break;
    case XFER_LIST:
    case XFER_NLST:
    case XFER_MLSD:
        ftp_pump_dir();
        break;
    default:
        break;
    }
}

/* 发送 s_buf 内残余数据：每拍最多 4 块，EAGAIN 保留偏移下拍续发 */
static bool ftp_send_pending(void)
{
    int budget = FTP_DATA_BLOCKS_PER_TICK;
    while (s_tx_sent < s_tx_len && budget-- > 0) {
        int n = send(s_data_fd, s_buf + s_tx_sent, s_tx_len - s_tx_sent, 0);
        if (n > 0) {
            s_tx_sent += (size_t)n;
            s_bytes_done += (uint32_t)n;
            continue;
        }
        if (n < 0 && ftp_eagain()) {
            return true;
        }
        ftp_xfer_abort("send");
        return false;
    }
    return true;
}

static void ftp_pump_retr(void)
{
    if (!ftp_send_pending()) {
        return;
    }
    if (s_tx_sent < s_tx_len) {
        return;     /* 上拍残余未发完，下拍续 */
    }
    /* Trap: fread 每拍至多一块——SD 读阻塞数 ms，多块连读会拖垮 task_comm
     * 同循环的 ws/MIDI 处理；发送侧由 ftp_send_pending 的块数预算兜底 */
    s_tx_len = fread(s_buf, 1, FTP_DATA_BUF_SIZE, s_file);
    s_tx_sent = 0;
    if (s_tx_len == 0) {
        ftp_xfer_done();    /* EOF */
        return;
    }
    ftp_send_pending();
}

static void ftp_pump_stor(void)
{
    int budget = FTP_DATA_BLOCKS_PER_TICK;
    while (budget-- > 0) {
        int n = recv(s_data_fd, s_buf, FTP_DATA_BUF_SIZE, 0);
        if (n > 0) {
            size_t w = fwrite(s_buf, 1, (size_t)n, s_file);
            s_bytes_done += (uint32_t)w;
            if (w != (size_t)n) {
                ESP_LOGE(TAG, "sd write failed (%u/%d)", (unsigned)w, n);
                if (s_file != NULL) {
                    fclose(s_file);
                    s_file = NULL;
                }
                ftp_close_fd(&s_data_fd);
                s_xfer = XFER_NONE;
                s_state = SERVICE_FTP_STATE_CONNECTED;
                ftp_queue_reply("451 Write error\r\n");
                return;
            }
            continue;
        }
        if (n == 0) {
            /* 对端关闭数据通道（FIN）：上传完成 */
            ftp_xfer_done();
            return;
        }
        if (ftp_eagain()) {
            return;
        }
        ftp_xfer_abort("recv");
        return;
    }
}

static void ftp_pump_dir(void)
{
    if (!ftp_send_pending()) {
        return;
    }
    if (s_tx_sent < s_tx_len) {
        return;     /* 上拍残余未发完 */
    }
    s_tx_len = 0;
    s_tx_sent = 0;

    /* 逐条格式行进同一缓冲，近满停手；DIR* 跨拍保持，下拍续读 */
    bool dir_end = false;
    while (s_tx_len < FTP_DATA_BUF_SIZE - FTP_DIR_LINE_MARGIN) {
        struct dirent *ent = readdir(s_dir);
        if (ent == NULL) {
            dir_end = true;
            break;
        }
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        s_tx_len += ftp_format_dir_line((char *)s_buf + s_tx_len,
                                        FTP_DATA_BUF_SIZE - s_tx_len, ent->d_name);
    }
    if (dir_end && s_tx_len == 0) {
        ftp_xfer_done();
        return;
    }
    if (s_tx_len > 0) {
        ftp_send_pending();
    }
}

/* 目录行格式化：LIST=UNIX ls -l 行（SimpleFTPServer generateFileLine 同款）、
 * NLST=纯文件名、MLSD=facts（type/size/modify=YYYYMMDDHHMMSS） */
static size_t ftp_format_dir_line(char *out, size_t len, const char *name)
{
    char full[FTP_REAL_PATH_MAX + 8];
    struct stat st;
    int pn = snprintf(full, sizeof(full), "%s/%s", s_xfer_path, name);
    bool ok = pn > 0 && (size_t)pn < sizeof(full) && stat(full, &st) == 0;
    bool is_dir = ok && S_ISDIR(st.st_mode);
    long fsize = (ok && !is_dir) ? (long)st.st_size : 4096;
    time_t ftime = ok ? st.st_mtime : 0;

    if (s_xfer == XFER_NLST) {
        return (size_t)snprintf(out, len, "%s\r\n", name);
    }

    struct tm tmv;
    if (s_xfer == XFER_MLSD) {
        char tbuf[16];
        gmtime_r(&ftime, &tmv);
        strftime(tbuf, sizeof(tbuf), "%Y%m%d%H%M%S", &tmv);
        return (size_t)snprintf(out, len, "Type=%s;Modify=%s;Size=%ld; %s\r\n",
                                is_dir ? "dir" : "file", tbuf, fsize, name);
    }

    /* LIST 日期：同年 "%b %d %H:%M"，跨年 "%b %d  %Y"（ls 惯例） */
    char dbuf[16];
    gmtime_r(&ftime, &tmv);
    time_t now = time(NULL);
    struct tm tmnow;
    gmtime_r(&now, &tmnow);
    if (tmv.tm_year == tmnow.tm_year) {
        strftime(dbuf, sizeof(dbuf), "%b %d %H:%M", &tmv);
    } else {
        strftime(dbuf, sizeof(dbuf), "%b %d  %Y", &tmv);
    }
    return (size_t)snprintf(out, len, "%s\t%d\t%s\t%ld\t%s\t%s\r\n",
                            is_dir ? "drwxrwsr-x" : "-rw-rw-r--",
                            is_dir ? 2 : 1, FTP_USER, fsize, dbuf, name);
}

#else /* !CONFIG_BOARD_HAS_WIFI */

/* 板型无 WiFi：降级 stub，符号保留供页面/任务无条件链接 */

esp_err_t service_ftp_init(void)
{
    return ESP_OK;
}

esp_err_t service_ftp_start(void)
{
    ESP_LOGW(TAG, "board has no WiFi, ftp not supported");
    return ESP_ERR_NOT_SUPPORTED;
}

void service_ftp_stop(void)
{
}

void service_ftp_process(void)
{
}

void service_ftp_get_status(service_ftp_status_t *out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->state = SERVICE_FTP_STATE_OFF;
}

#endif /* CONFIG_BOARD_HAS_WIFI */
