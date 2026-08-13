/**
 * @file service_xiaozhi_config.h
 * @brief 小智语音助手服务配置常量
 */

#ifndef SERVICE_XIAOZHI_CONFIG_H
#define SERVICE_XIAOZHI_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 默认 OTA（激活/设备信息上报）地址 */
#define SERVICE_XIAOZHI_OTA_URL "https://api.tenclass.net/xiaozhi/ota/"

/** @brief HTTP User-Agent */
#define SERVICE_XIAOZHI_USER_AGENT "TAB5_Music_Pad/1.0.0"

/** @brief Accept-Language 头 */
#define SERVICE_XIAOZHI_ACCEPT_LANGUAGE "zh-CN"

/** @brief OTA HTTP 超时（ms） */
#define SERVICE_XIAOZHI_HTTP_TIMEOUT_MS 15000

/** @brief OTA 检查失败重试次数（指数退避，起始 10s） */
#define SERVICE_XIAOZHI_OTA_MAX_RETRY 5

/** @brief OTA 检查重试起始退避（ms），逐次翻倍 */
#define SERVICE_XIAOZHI_OTA_RETRY_BASE_MS 10000

/** @brief 激活轮询间隔（ms）：HTTP 202 表示等待用户绑定 */
#define SERVICE_XIAOZHI_ACTIVATE_POLL_MS 3000

/** @brief 激活码缺省有效期（ms）：服务器未下发 timeout_ms 时使用，超时回外层换新码 */
#define SERVICE_XIAOZHI_ACTIVATE_DEFAULT_TIMEOUT_MS (10 * 60 * 1000)

/** @brief 未激活设备的激活重试间隔（ms）：WiFi 上线沿/凭据被 401 清除后按此周期补激活 */
#define SERVICE_XIAOZHI_ACTIVATION_RETRY_MS 60000

/** @brief WebSocket 服务器 hello 等待超时（ms） */
#define SERVICE_XIAOZHI_HELLO_TIMEOUT_MS 10000

/** @brief WebSocket 断线重连起始退避（ms），逐次翻倍封顶 */
#define SERVICE_XIAOZHI_RECONNECT_BASE_MS 1000

/** @brief WebSocket 断线重连退避上限（ms） */
#define SERVICE_XIAOZHI_RECONNECT_MAX_MS 30000

/** @brief 连续对话/待应答空闲超时（ms）：通道打开但持续无服务器活动超过此值，
 * 主动关闭通道回 STANDBY，避免无限「卡 Listen」。auto 连续对话与 manual 等待
 * TTS 均适用；SPEAKING 播报期不计入（防止长句被截断）。
 * Why 60s：阈值须覆盖慢 LLM 首 token 延迟——manual 松手等应答、auto 断句后等
 * 回复都计入空闲，20s 时复杂问题的应答在到达前通道已被本地关闭（应答永久丢失
 * 且无任何提示）。 */
#define SERVICE_XIAOZHI_LISTEN_IDLE_MS 60000

/** @brief 上行 Opus 编码采样率（全双工：与扬声器同 44.1kHz，不再关扬声器） */
#define SERVICE_XIAOZHI_OPUS_ENC_SAMPLE_RATE 44100

/** @brief 上行 Opus 帧长（ms），协议固定 60 */
#define SERVICE_XIAOZHI_OPUS_FRAME_DURATION_MS 60

/** @brief 上行 Opus 每帧采样数：16000 * 60 / 1000 */
#define SERVICE_XIAOZHI_OPUS_ENC_FRAME_SAMPLES 960

/** @brief 下行 Opus 默认采样率（服务器 hello 未给 audio_params 时） */
#define SERVICE_XIAOZHI_OPUS_DEC_DEFAULT_SAMPLE_RATE 24000

/** @brief 下行解码输出 PCM 缓冲（采样数，按 48kHz*60ms 上限） */
#define SERVICE_XIAOZHI_DEC_PCM_MAX_SAMPLES 2880

/** @brief 麦克风单次读取帧数（I2S DMA 缓冲上限 256 帧） */
#define SERVICE_XIAOZHI_MIC_READ_FRAMES 160

/** @brief WebSocket 收发重组缓冲（字节），超限的报文直接丢弃 */
#define SERVICE_XIAOZHI_WS_RX_BUF_SIZE 4096

/** @brief WebSocket 客户端任务栈（字节）：esp_websocket_client 收发/TLS 用。
 * Trap: 下行 Opus 解码实际发生在 task_comm（service_ws_process 分发），
 * 其栈需求由 task_comm 承担（16384）；本值勿再按解码需求估量。 */
#define SERVICE_XIAOZHI_WS_TASK_STACK 20480

/** @brief Opus 编码输出缓冲（字节） */
#define SERVICE_XIAOZHI_OPUS_ENC_OUT_SIZE 1024

/** @brief 唤醒词（esp-sr AFE + WakeNet）编译开关：0 整体裁掉，退化为仅按钮说话 */
#define SERVICE_XIAOZHI_WAKE_WORD_ENABLE 1

/** @brief AFE feed/fetch 块采样数上限（16kHz 单通道），MR 格式下缓冲按 2× 使用 */
#define SERVICE_XIAOZHI_AFE_MAX_CHUNK 2048

/** @brief 内部消息队列深度（ws 回调 → xz_task） */
#define SERVICE_XIAOZHI_MSG_QUEUE_LEN 16

/** @brief 命令队列深度（App → xz_task） */
#define SERVICE_XIAOZHI_CMD_QUEUE_LEN 8

/** @brief App 事件队列深度（xz_task → App poll） */
#define SERVICE_XIAOZHI_EVT_QUEUE_LEN 16

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_XIAOZHI_CONFIG_H */
