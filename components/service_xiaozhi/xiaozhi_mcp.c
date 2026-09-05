/**
 * @file xiaozhi_mcp.c
 * @brief 小智 MCP（JSON-RPC 2.0）设备工具实现
 *
 * 协议对齐上游 xiaozhi-esp32 mcp_server（协议版本 2024-11-05）：
 * initialize / notifications.* 忽略 / tools/list（cursor 分页，单回复 ≤8000 字节）
 * / tools/call（参数校验，失败 isError=true 附中文原因）。
 */

#include "xiaozhi_mcp.h"

#include "xiaozhi_ota.h"
#include "service_ws.h"

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"

#include "stdio.h"
#include "string.h"

static const char *TAG = "xiaozhi_mcp";

/** @brief tools/list 单回复字节上限（协议约定，超出经 nextCursor 翻页） */
#define XIAOZHI_MCP_LIST_MAX_BYTES 8000

/** @brief 工具执行结果文本缓冲（中文结果 + 失败原因 + get_system_info JSON） */
#define XIAOZHI_MCP_RESULT_TEXT_LEN 512

/** @brief 主题参数 → EEZ 主题名映射（dark=星空黑，light=Hammy橙） */
#define XIAOZHI_MCP_THEME_DARK_EEZ "starrynight"
#define XIAOZHI_MCP_THEME_LIGHT_EEZ "hammyorange"

/**
 * @brief 工具执行入口
 * @param[in]  args    tools/call 的 arguments 对象（可为 NULL）
 * @param[out] out     结果文本（中文）
 * @param[in]  out_len 文本缓冲长度
 * @return true 成功；false 失败（out 为原因）
 */
typedef bool (*xz_mcp_tool_exec_t)(const cJSON *args, char *out, size_t out_len);

/** @brief 内置工具表项 */
typedef struct {
    const char *name;
    const char *description;
    const char *input_schema;       /*!< JSON Schema 文本（构建 tools/list 时解析） */
    xz_mcp_tool_exec_t exec;
} xz_mcp_tool_t;

/* 注册的 MCP 产品回调表；默认全 NULL，未注册工具返回未实现错误 */
static xiaozhi_mcp_callbacks_t s_cbs = {0};

/* 延迟重启定时器：self.reboot 先回成功响应，1s 后在定时器任务上下文重启 */
static esp_timer_handle_t s_reboot_timer = NULL;

void xiaozhi_mcp_register_callbacks(const xiaozhi_mcp_callbacks_t *cbs)
{
    if (cbs == NULL) {
        memset(&s_cbs, 0, sizeof(s_cbs));
        return;
    }
    s_cbs = *cbs;
}

static bool xz_mcp_set_volume(const cJSON *args, char *out, size_t out_len)
{
    const cJSON *volume = cJSON_GetObjectItem(args, "volume");
    if (!cJSON_IsNumber(volume)) {
        snprintf(out, out_len, "缺少有效参数 volume（0-100 整数）");
        return false;
    }
    int v = volume->valueint;
    if (v < 0 || v > 100) {
        snprintf(out, out_len, "volume 超出范围（0-100）：%d", v);
        return false;
    }
    if (s_cbs.set_volume == NULL) {
        snprintf(out, out_len, "音量设置未实现");
        return false;
    }
    if (!s_cbs.set_volume(v, s_cbs.user_data)) {
        snprintf(out, out_len, "音量设置失败");
        return false;
    }
    snprintf(out, out_len, "音量已设置为 %d", v);
    return true;
}

static bool xz_mcp_set_brightness(const cJSON *args, char *out, size_t out_len)
{
    const cJSON *brightness = cJSON_GetObjectItem(args, "brightness");
    if (!cJSON_IsNumber(brightness)) {
        snprintf(out, out_len, "缺少有效参数 brightness（0-100 整数）");
        return false;
    }
    int v = brightness->valueint;
    if (v < 0 || v > 100) {
        snprintf(out, out_len, "brightness 超出范围（0-100）：%d", v);
        return false;
    }
    if (s_cbs.set_brightness == NULL) {
        snprintf(out, out_len, "亮度设置未实现");
        return false;
    }
    if (!s_cbs.set_brightness(v, s_cbs.user_data)) {
        snprintf(out, out_len, "亮度设置失败");
        return false;
    }
    snprintf(out, out_len, "亮度已设置为 %d", v);
    return true;
}

static bool xz_mcp_set_theme(const cJSON *args, char *out, size_t out_len)
{
    const cJSON *theme = cJSON_GetObjectItem(args, "theme");
    if (!cJSON_IsString(theme)) {
        snprintf(out, out_len, "缺少有效参数 theme");
        return false;
    }
    const char *theme_str = theme->valuestring;

    /* 主题参数直接透传给回调，由 ai_mcp_set_theme 统一处理多种命名 */
    if (s_cbs.set_theme == NULL) {
        snprintf(out, out_len, "主题切换未实现");
        return false;
    }
    if (!s_cbs.set_theme(theme_str, s_cbs.user_data)) {
        snprintf(out, out_len, "主题切换失败");
        return false;
    }
    snprintf(out, out_len, "主题已切换为 %s", theme_str);
    return true;
}

static bool xz_mcp_app_launch(const cJSON *args, char *out, size_t out_len)
{
    const cJSON *name = cJSON_GetObjectItem(args, "name");
    if (!cJSON_IsString(name) || name->valuestring == NULL || name->valuestring[0] == '\0') {
        snprintf(out, out_len, "缺少有效参数 name（App 注册名）");
        return false;
    }
    if (s_cbs.app_launch == NULL) {
        snprintf(out, out_len, "App 启动未实现");
        return false;
    }
    if (!s_cbs.app_launch(name->valuestring, s_cbs.user_data)) {
        snprintf(out, out_len, "App 启动失败");
        return false;
    }
    snprintf(out, out_len, "已启动 %s", name->valuestring);
    return true;
}

static bool xz_mcp_app_exit(const cJSON *args, char *out, size_t out_len)
{
    (void)args;
    if (s_cbs.app_exit == NULL) {
        snprintf(out, out_len, "App 退出未实现");
        return false;
    }
    if (!s_cbs.app_exit(s_cbs.user_data)) {
        snprintf(out, out_len, "当前没有运行中的 App");
        return false;
    }
    snprintf(out, out_len, "已返回主界面");
    return true;
}

static bool xz_mcp_get_device_status(const cJSON *args, char *out, size_t out_len)
{
    (void)args;
    if (s_cbs.get_device_status == NULL) {
        snprintf(out, out_len, "设备状态获取未实现");
        return false;
    }
    /* Trap: 回调返回 esp_err_t（ESP_OK=0），按 bool 判定会把成功误判为失败 */
    if (s_cbs.get_device_status(out, out_len, s_cbs.user_data) != ESP_OK) {
        snprintf(out, out_len, "设备状态获取失败");
        return false;
    }
    return true;
}

static bool xz_mcp_get_system_info(const cJSON *args, char *out, size_t out_len)
{
    (void)args;

    char device_id[18];
    xiaozhi_ota_get_device_id(device_id, sizeof(device_id));

    /* 未激活设备尚无 uuid：只读 NVS 缓存，空串上报，不触发首启生成。
     * service_nvs.h 经 xiaozhi_ota.h 间接包含。 */
    char uuid[SERVICE_NVS_XZ_UUID_MAX_LEN] = {0};
    service_nvs_get_xz_uuid(uuid, sizeof(uuid));

    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);

    const esp_app_desc_t *app = esp_app_get_description();

    /* 字段对齐 xiaozhi_ota.c build_body 子集（缺 language，协议端不需要） */
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        snprintf(out, out_len, "内存不足，无法构造系统信息");
        return false;
    }
    cJSON_AddNumberToObject(root, "version", 2);
    cJSON_AddNumberToObject(root, "flash_size", (double)flash_size);
    cJSON_AddStringToObject(root, "mac_address", device_id);
    cJSON_AddStringToObject(root, "uuid", uuid);
    cJSON_AddStringToObject(root, "chip_model_name", CONFIG_IDF_TARGET);
    cJSON *application = cJSON_CreateObject();
    cJSON_AddStringToObject(application, "name", app->project_name);
    /* 版本号统一走 FIRMWARE_VERSION（与关于页/启动日志同源）；app->version 是
     * project() VERSION，非 tag 构建恒为 0.0.0，不能用 */
    cJSON_AddStringToObject(application, "version", FIRMWARE_VERSION);
    cJSON_AddItemToObject(root, "application", application);

    char *printed = cJSON_PrintUnformatted(root);
    if (printed != NULL) {
        strncpy(out, printed, out_len - 1);
        out[out_len - 1] = '\0';
        cJSON_free(printed);
    } else {
        snprintf(out, out_len, "系统信息序列化失败");
    }
    cJSON_Delete(root);
    return printed != NULL;
}

static void xz_mcp_reboot_timer_cb(void *arg)
{
    (void)arg;
    ESP_LOGW(TAG, "user requested reboot");
    esp_restart();
}

static bool xz_mcp_reboot(const cJSON *args, char *out, size_t out_len)
{
    (void)args;
    if (s_reboot_timer == NULL) {
        const esp_timer_create_args_t timer_cfg = {
            .callback = xz_mcp_reboot_timer_cb,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "xz_reboot",
        };
        if (esp_timer_create(&timer_cfg, &s_reboot_timer) != ESP_OK) {
            snprintf(out, out_len, "重启定时器创建失败");
            return false;
        }
    }
    /* Trap: 本回调在 xz_task 上下文同步执行，禁止 vTaskDelay 阻塞等响应发完，
     * 故用一次性定时器异步重启，成功响应先行发出 */
    if (s_cbs.reboot != NULL) {
        s_cbs.reboot(s_cbs.user_data);
    }
    if (esp_timer_start_once(s_reboot_timer, 1000ULL * 1000ULL) != ESP_OK) {
        snprintf(out, out_len, "重启调度失败");
        return false;
    }
    snprintf(out, out_len, "设备将在 1 秒后重启");
    return true;
}

/**
 * @brief 退出对话回待机：只登记请求，道别 TTS 播完才真正关通道，
 * 避免告别语被掉。收尾时序在 service_xiaozhi 主循环闭环。
 */
static bool xz_mcp_standby(const cJSON *args, char *out, size_t out_len)
{
    (void)args;
    if (s_cbs.standby == NULL) {
        snprintf(out, out_len, "待机功能未实现");
        return false;
    }
    s_cbs.standby(s_cbs.user_data);
    snprintf(out, out_len, "好的，对话已结束，设备进入待机");
    return true;
}

/* 工具表顺序对齐上游：常用状态查询在前，利于提示词缓存 */
static const xz_mcp_tool_t s_tools[] = {
    {
        .name = "self.app.launch",
        .description = "Launch an application by its registered name. "
                       "Available apps (use the exact English `name` parameter): "
                       "`Zen Mode` (禅模式/冥想/白噪音/放松助眠), "
                       "`Metronome` (节拍器), "
                       "`Sequencer` (音序器/鼓机/节奏编辑/编曲), "
                       "`Recorder` (录音机/录音/录制声音/语音备忘录), "
                       "`Tiny Piano` (小钢琴/钢琴), "
                       "`Ear Trainer` (练耳/视唱练耳), "
                       "`Chord Trainer` (和弦练习/五度圈), "
                       "`XY Pad` (XY模式), "
                       "`Clock Calendar` (时钟日历/时钟/日历/天气/闹钟), "
                       "`Fun` (趣味/答案之书/塔罗牌/抽卡/占卜/翻书/运势), "
                       "`MIDI Player` (MIDI播放器), "
                       "`AI Agent` (AI导师/聊天助手).",
        .input_schema = "{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"}},\"required\":[\"name\"]}",
        .exec = xz_mcp_app_launch,
    },
    {
        .name = "self.app.exit",
        .description = "Exit the current running application and return to the main launcher / home screen. Call this when the user asks to exit/close/quit the current app or go back to home/main menu.",
        .input_schema = "{\"type\":\"object\",\"properties\":{}}",
        .exec = xz_mcp_app_exit,
    },
    {
        .name = "self.get_device_status",
        .description = "Provides the real-time information of the device, including the current status of the audio speaker, screen, battery, network, etc.\n"
                       "Use this tool for: \n"
                       "1. Answering questions about current condition (e.g. what is the current volume of the audio speaker?)\n"
                       "2. As the first step to control the device (e.g. turn up / down the volume of the audio speaker, etc.)",
        .input_schema = "{\"type\":\"object\",\"properties\":{}}",
        .exec = xz_mcp_get_device_status,
    },
    {
        .name = "self.audio_speaker.set_volume",
        .description = "Set the volume of the audio speaker. If the current volume is unknown, you must call `self.get_device_status` tool first and then call this tool.",
        .input_schema = "{\"type\":\"object\",\"properties\":{\"volume\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":100}},\"required\":[\"volume\"]}",
        .exec = xz_mcp_set_volume,
    },
    {
        .name = "self.screen.set_brightness",
        .description = "Set the brightness of the screen.",
        .input_schema = "{\"type\":\"object\",\"properties\":{\"brightness\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":100}},\"required\":[\"brightness\"]}",
        .exec = xz_mcp_set_brightness,
    },
    {
        .name = "self.screen.set_theme",
        .description = "Set the theme of the screen. The theme can be `light` / `hammyorange` (金丝熊主题, warm orange with hammy) or `dark` / `starrynight` (星空黑主题, dark starry night).",
        .input_schema = "{\"type\":\"object\",\"properties\":{\"theme\":{\"type\":\"string\"}},\"required\":[\"theme\"]}",
        .exec = xz_mcp_set_theme,
    },
    {
        .name = "self.get_system_info",
        .description = "Get the system information",
        .input_schema = "{\"type\":\"object\",\"properties\":{}}",
        .exec = xz_mcp_get_system_info,
    },
    {
        .name = "self.reboot",
        .description = "Reboot the system",
        .input_schema = "{\"type\":\"object\",\"properties\":{}}",
        .exec = xz_mcp_reboot,
    },
    {
        .name = "self.standby",
        .description = "End the current voice conversation and put the assistant into standby. "
                       "Call this tool when the user says goodbye or asks the assistant to leave/step back/rest "
                       "(e.g. 退下吧 / 你先下去吧 / 去休息吧 / 没事了), i.e. clearly wants to END the conversation. "
                       "After calling it, just say a brief farewell; the channel will close automatically.",
        .input_schema = "{\"type\":\"object\",\"properties\":{}}",
        .exec = xz_mcp_standby,
    },
};

#define XIAOZHI_MCP_TOOL_COUNT (sizeof(s_tools) / sizeof(s_tools[0]))

/**
 * @brief 封装并发送一条 JSON-RPC 报文（payload 对象所有权随之移交）
 */
static void xz_mcp_send_rpc(cJSON *rpc, const char *session_id)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        cJSON_Delete(rpc);
        return;
    }
    cJSON_AddStringToObject(root, "session_id", (session_id != NULL) ? session_id : "");
    cJSON_AddStringToObject(root, "type", "mcp");
    cJSON_AddItemToObject(root, "payload", rpc);

    char *str = cJSON_PrintUnformatted(root);
    if (str != NULL) {
        service_ws_send_text(str, (int)strlen(str));
        cJSON_free(str);
    }
    cJSON_Delete(root);
}

/**
 * @brief 回复 JSON-RPC result（result 对象所有权随之移交）
 */
static void xz_mcp_reply_result(int id, cJSON *result, const char *session_id)
{
    cJSON *rpc = cJSON_CreateObject();
    if (rpc == NULL) {
        cJSON_Delete(result);
        return;
    }
    cJSON_AddStringToObject(rpc, "jsonrpc", "2.0");
    cJSON_AddNumberToObject(rpc, "id", id);
    cJSON_AddItemToObject(rpc, "result", result);
    xz_mcp_send_rpc(rpc, session_id);
}

/**
 * @brief 回复 JSON-RPC error
 */
static void xz_mcp_reply_error(int id, const char *message, const char *session_id)
{
    cJSON *rpc = cJSON_CreateObject();
    if (rpc == NULL) {
        return;
    }
    cJSON_AddStringToObject(rpc, "jsonrpc", "2.0");
    cJSON_AddNumberToObject(rpc, "id", id);
    cJSON *err = cJSON_CreateObject();
    cJSON_AddStringToObject(err, "message", (message != NULL) ? message : "error");
    cJSON_AddItemToObject(rpc, "error", err);
    xz_mcp_send_rpc(rpc, session_id);
}

/**
 * @brief initialize：回协议版本/能力/服务器信息
 */
static void xz_mcp_on_initialize(int id, const char *session_id)
{
    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "protocolVersion", "2024-11-05");
    cJSON *caps = cJSON_CreateObject();
    cJSON_AddItemToObject(caps, "tools", cJSON_CreateObject());
    cJSON_AddItemToObject(result, "capabilities", caps);
    cJSON *info = cJSON_CreateObject();
    cJSON_AddStringToObject(info, "name", "TAB5_Music_Pad");
    /* 版本号统一走 FIRMWARE_VERSION（与关于页/启动日志同源） */
    cJSON_AddStringToObject(info, "version", FIRMWARE_VERSION);
    cJSON_AddItemToObject(result, "serverInfo", info);
    xz_mcp_reply_result(id, result, session_id);
}

/**
 * @brief tools/list：cursor 分页（含 cursor 所指工具本身，与上游一致），单回复 ≤8000 字节
 */
static void xz_mcp_on_tools_list(int id, const cJSON *params, const char *session_id)
{
    const char *cursor = "";
    if (params != NULL) {
        const cJSON *c = cJSON_GetObjectItem(params, "cursor");
        if (cJSON_IsString(c)) {
            cursor = c->valuestring;
        }
    }

    cJSON *result = cJSON_CreateObject();
    cJSON *tools = cJSON_AddArrayToObject(result, "tools");

    bool found = (cursor[0] == '\0');
    const char *next_cursor = NULL;
    int size = 16;  /* 框架开销：{"tools":[]} */
    for (size_t i = 0; i < XIAOZHI_MCP_TOOL_COUNT; i++) {
        const xz_mcp_tool_t *t = &s_tools[i];
        if (!found) {
            if (strcmp(t->name, cursor) == 0) {
                found = true;
            } else {
                continue;
            }
        }
        int item_len = (int)(strlen(t->name) + strlen(t->description) + strlen(t->input_schema)) + 64;
        if (size + item_len + 32 > XIAOZHI_MCP_LIST_MAX_BYTES) {
            next_cursor = t->name;
            break;
        }
        cJSON *tool = cJSON_CreateObject();
        cJSON_AddStringToObject(tool, "name", t->name);
        cJSON_AddStringToObject(tool, "description", t->description);
        cJSON *schema = cJSON_Parse(t->input_schema);
        if (schema != NULL) {
            cJSON_AddItemToObject(tool, "inputSchema", schema);
        }
        cJSON_AddItemToArray(tools, tool);
        size += item_len;
    }
    if (next_cursor != NULL) {
        cJSON_AddStringToObject(result, "nextCursor", next_cursor);
    }
    xz_mcp_reply_result(id, result, session_id);
}

/**
 * @brief tools/call：校验参数 → 执行 → 回 content/isError
 */
static void xz_mcp_on_tools_call(int id, const cJSON *params, const char *session_id)
{
    if (!cJSON_IsObject(params)) {
        xz_mcp_reply_error(id, "Missing params", session_id);
        return;
    }
    const cJSON *name = cJSON_GetObjectItem(params, "name");
    if (!cJSON_IsString(name)) {
        xz_mcp_reply_error(id, "Missing name", session_id);
        return;
    }
    const cJSON *arguments = cJSON_GetObjectItem(params, "arguments");
    if (arguments != NULL && !cJSON_IsObject(arguments)) {
        xz_mcp_reply_error(id, "Invalid arguments", session_id);
        return;
    }

    const xz_mcp_tool_t *tool = NULL;
    for (size_t i = 0; i < XIAOZHI_MCP_TOOL_COUNT; i++) {
        if (strcmp(s_tools[i].name, name->valuestring) == 0) {
            tool = &s_tools[i];
            break;
        }
    }
    if (tool == NULL) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Unknown tool: %s", name->valuestring);
        xz_mcp_reply_error(id, msg, session_id);
        return;
    }

    char text[XIAOZHI_MCP_RESULT_TEXT_LEN] = {0};
    bool ok = tool->exec(arguments, text, sizeof(text));

    cJSON *result = cJSON_CreateObject();
    cJSON *content = cJSON_AddArrayToObject(result, "content");
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "type", "text");
    cJSON_AddStringToObject(item, "text", text);
    cJSON_AddItemToArray(content, item);
    cJSON_AddBoolToObject(result, "isError", !ok);
    xz_mcp_reply_result(id, result, session_id);
}

void xiaozhi_mcp_handle(const cJSON *payload, const char *session_id)
{
    if (!cJSON_IsObject(payload)) {
        return;
    }

    const cJSON *version = cJSON_GetObjectItem(payload, "jsonrpc");
    if (!cJSON_IsString(version) || strcmp(version->valuestring, "2.0") != 0) {
        ESP_LOGW(TAG, "invalid jsonrpc version");
        return;
    }

    const cJSON *method = cJSON_GetObjectItem(payload, "method");
    if (!cJSON_IsString(method)) {
        ESP_LOGW(TAG, "missing method");
        return;
    }
    const char *method_str = method->valuestring;

    /* 通知无 id，直接忽略 */
    if (strncmp(method_str, "notifications", strlen("notifications")) == 0) {
        return;
    }

    const cJSON *params = cJSON_GetObjectItem(payload, "params");
    if (params != NULL && !cJSON_IsObject(params)) {
        ESP_LOGW(TAG, "invalid params for method: %s", method_str);
        return;
    }

    const cJSON *id = cJSON_GetObjectItem(payload, "id");
    if (!cJSON_IsNumber(id)) {
        ESP_LOGW(TAG, "invalid id for method: %s", method_str);
        return;
    }
    int id_int = id->valueint;

    if (strcmp(method_str, "initialize") == 0) {
        xz_mcp_on_initialize(id_int, session_id);
    } else if (strcmp(method_str, "tools/list") == 0) {
        xz_mcp_on_tools_list(id_int, params, session_id);
    } else if (strcmp(method_str, "tools/call") == 0) {
        xz_mcp_on_tools_call(id_int, params, session_id);
    } else {
        char msg[160];
        snprintf(msg, sizeof(msg), "Method not implemented: %s", method_str);
        ESP_LOGW(TAG, "%s", msg);
        xz_mcp_reply_error(id_int, msg, session_id);
    }
}
