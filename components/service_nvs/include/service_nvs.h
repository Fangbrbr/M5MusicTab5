/**
 * @file service_nvs.h
 * @brief 下电参数持久化管理服务
 *
 * 通过 ESP-IDF NVS 统一管理设备初始化标志、启动计次、亮度、音量、
 * Wi-Fi 凭据、小智激活凭据及功能开关等下电需要保留的参数。
 */

#ifndef SERVICE_NVS_H
#define SERVICE_NVS_H

#include "esp_err.h"
#include "stdbool.h"
#include "stddef.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 功能开关位掩码
 */
#define SERVICE_NVS_FLAG_WIFI_ENABLED       (1U << 0)   /*!< Wi-Fi 使能 */
#define SERVICE_NVS_FLAG_BLE_ENABLED        (1U << 1)   /*!< BLE 使能 */
#define SERVICE_NVS_FLAG_USB_HOST_ENABLED   (1U << 2)   /*!< USB Host 使能 */
#define SERVICE_NVS_FLAG_USB_DEVICE_ENABLED (1U << 3)   /*!< USB Device 使能 */
#define SERVICE_NVS_FLAG_AUTO_BRIGHTNESS    (1U << 4)   /*!< 自动亮度 */
#define SERVICE_NVS_FLAG_MUTE               (1U << 5)   /*!< 静音 */
#define SERVICE_NVS_FLAG_DEMO_MODE          (1U << 6)   /*!< 演示模式 */
#define SERVICE_NVS_FLAG_AI_SAVE_TEXT       (1U << 7)   /*!< AI 对话落盘（SD 卡） */
#define SERVICE_NVS_FLAG_INVERT_DISPLAY     (1U << 8)   /*!< 显示反向（横向 180°） */

/** @brief 语言 ID 最大长度（含结尾 '\0'） */
#define SERVICE_NVS_LANGUAGE_MAX_LEN 8

/** @brief 主题 ID 最大长度（含结尾 '\0'） */
#define SERVICE_NVS_THEME_MAX_LEN 16

/** @brief Wi-Fi SSID 最大长度（含结尾 '\0'） */
#define SERVICE_NVS_SSID_MAX_LEN 32

/** @brief Wi-Fi 密码最大长度（含结尾 '\0'） */
#define SERVICE_NVS_PASS_MAX_LEN 64

/** @brief 小智 Client-Id（UUID）最大长度（含结尾 '\0'） */
#define SERVICE_NVS_XZ_UUID_MAX_LEN 37

/** @brief 小智 WebSocket URL 最大长度（含结尾 '\0'） */
#define SERVICE_NVS_XZ_WS_URL_MAX_LEN 128

/** @brief 小智 WebSocket Token 最大长度（含结尾 '\0'） */
#define SERVICE_NVS_XZ_WS_TOKEN_MAX_LEN 256

/** @brief 自定义 MAC 地址（6 字节 +NUL，用于 ESP32-P4 eFuse 未烧录 base MAC 时的退路） */
#define SERVICE_NVS_CUSTOM_MAC_MAX_LEN 8

/**
 * @brief 娱乐 App 蓝量持久化参数
 */
typedef struct {
    uint16_t year;  /*!< 上次保存年份，如 2026 */
    uint8_t  month; /*!< 月份 1-12 */
    uint8_t  day;   /*!< 日期 1-31 */
    uint8_t  mana;  /*!< 蓝量 0-100 */
    uint8_t  reserved; /*!< 保留对齐 */
} service_nvs_app_fun_mana_t;

/** @brief MIDI 播放器持久化参数 */
#define SERVICE_NVS_MIDI_PLAYER_FILENAME_MAX_LEN 96

typedef struct {
    uint8_t play_type;                                     /*!< 0=MIDI, 1=录音(.mid) */
    char filename[SERVICE_NVS_MIDI_PLAYER_FILENAME_MAX_LEN]; /*!< 当前曲目文件名（含后缀） */
    uint8_t reserved[3];
} service_nvs_midi_player_t;

/** @brief 节拍器持久化参数 */
typedef struct {
    uint16_t bpm;       /*!< BPM 20~300 */
    uint8_t sig_top;    /*!< 拍号分子 1~16 */
    uint8_t sig_bot;    /*!< 拍号分母选项索引 0~4（4/6/8/16/32） */
    uint8_t sound;      /*!< 音色 0~5 */
    uint8_t reserved;
} service_nvs_metronome_t;

/** @brief 小钢琴音色选择参数 */
typedef struct {
    uint8_t sound_type;       /*!< 钢琴音色类型 0~15 (0=三角大钢琴，1=亮音钢琴，...,15=扬琴) */
    uint8_t display;          /*!< 显示布局 0=矩阵垫，1=钢琴键盘 */
    uint8_t scale;            /*!< 音阶索引 0~5（大调/小调/中国五声/埃及调式/多利亚/日本调式） */
    uint8_t root_oct;         /*!< 根音符八度 0~6 */
    uint8_t pitch;            /*!< 键盘根音音名 0~11（0=C，1=C#，…，11=B） */
    uint8_t reserved[3];      /*!< 对齐保留 */
} service_nvs_piano_t;

/** @brief 音序器 App 参数 */
#define SEQ_NVS_TRACK_NUM 8

typedef struct {
    uint16_t bpm;                          /*!< 全局 BPM 20~300 */
    uint8_t swing;                         /*!< 摇摆量 0~100 */
    /* 每轨持久化参数（按轨落盘，开机恢复）。
     * 约定：track_velocity[t]==0 表示该轨"从未配置"（app 侧保持引擎常量默认）；
     * 其余三个数组只在该轨 velocity!=0 时生效（note 0=None 为合法选择）。 */
    uint8_t track_note[SEQ_NVS_TRACK_NUM];     /*!< 每轨鼓 note（0=None 或 GM 鼓 35-57） */
    uint8_t track_rand_temp[SEQ_NVS_TRACK_NUM]; /*!< 每轨人性化 0-10 */
    uint8_t track_velocity[SEQ_NVS_TRACK_NUM];  /*!< 每轨全局力度 1-127；0=未配置 */
    uint8_t track_probability[SEQ_NVS_TRACK_NUM]; /*!< 每轨全局概率 0-100 */
} service_nvs_sequencer_t;

/** @brief 录音机 App 参数 */
typedef struct {
    uint8_t mode;             /*!< 录音模式：0 语音 / 1 乐器 / 2 环境 */
    uint8_t reserved[3];
} service_nvs_recorder_t;

/** @brief 禅模式 App 参数 */
typedef struct {
    uint8_t mode;             /*!< 0=弹珠，1=雨滴 */
    uint8_t key_sel;          /*!< 调式索引 0~5（大调/小调/中国五声/埃及调式/多利亚/日本调式） */
    uint8_t speed_sel;        /*!< 速度档 0~4（弹珠：球速档；雨滴：同时下落个数-1） */
    uint8_t sound_sel;        /*!< 音色索引 0~3（GM program 0/9/10/77） */
} service_nvs_zen_t;

/** @brief 系统级参数分组（sys） */
typedef struct {
    bool     initialized;      /*!< 首次初始化标志 */
    uint32_t boot_count;       /*!< 启动计次 */
    uint8_t  custom_mac[6];    /*!< 自定义 MAC（ESP32-P4 fallback） */
} service_nvs_system_cfg_t;

/** @brief 设置级参数分组（settings） */
typedef struct {
    uint8_t  brightness;         /*!< 屏幕亮度 0-100 */
    int16_t  volume;             /*!< 系统音量 0-100 */
    uint32_t feature_flags;      /*!< 功能开关位掩码 */
    char     language[SERVICE_NVS_LANGUAGE_MAX_LEN]; /*!< 语言 ID */
    char     theme_name[SERVICE_NVS_THEME_MAX_LEN];  /*!< 主题 ID */
    uint8_t  idle_timeout_index; /*!< 自动熄屏时间选项索引 */
    bool     auto_sleep_enabled; /*!< 自动休眠开关 */
    uint8_t  boot_screen_index;  /*!< 开机默认页面索引 */
} service_nvs_settings_cfg_t;

/** @brief Wi-Fi 参数分组（wifi） */
typedef struct {
    char ssid[SERVICE_NVS_SSID_MAX_LEN];     /*!< Wi-Fi SSID */
    char password[SERVICE_NVS_PASS_MAX_LEN];   /*!< Wi-Fi 密码 */
} service_nvs_wifi_cfg_t;

/** @brief 小智参数分组（xiaozhi） */
typedef struct {
    char uuid[SERVICE_NVS_XZ_UUID_MAX_LEN];       /*!< 小智 Client-Id（UUID） */
    char ws_url[SERVICE_NVS_XZ_WS_URL_MAX_LEN];   /*!< 小智 WebSocket URL */
    char ws_token[SERVICE_NVS_XZ_WS_TOKEN_MAX_LEN]; /*!< 小智 WebSocket Token */
    bool wake_anywhere;                           /*!< 全局唤醒开关：任意界面唤醒后台对话（新增字段，旧 blob 读出保持默认 false） */
} service_nvs_xiaozhi_cfg_t;

/** @brief 练耳 App 参数分组（ear_trainer） */
typedef struct {
    uint32_t best[6]; /*!< 练耳历史最高分：模式 (0 绝对/1 相对)*3 + 难度 (0~2) */
} service_nvs_ear_trainer_cfg_t;

/** @brief 时钟 App 参数分组（clock） */
typedef struct {
    bool     use_12h;  /*!< 时钟 App 12h 制式 */
    uint32_t timer_s;  /*!< 时钟 App 定时器目标时长（秒） */
} service_nvs_clock_cfg_t;

/**
 * @brief 系统级参数结构体
 *
 * 所有下电需保留的系统参数集中维护，便于直接访问与 factory reset 恢复默认值。
 * 字段按 NVS 分组连续排放，方便 service_nvs.c 内部按组 blob 读写。
 */
struct s_system_parameters {
    /* system 组 */
    bool     initialized;                                /*!< 首次初始化标志 */
    uint32_t boot_count;                                 /*!< 启动计次 */
    uint8_t  custom_mac[6];                              /*!< 自定义 MAC */

    /* settings 组 */
    uint8_t  brightness;                                 /*!< 屏幕亮度 0-100 */
    int16_t  volume;                                     /*!< 系统音量 0-100 */
    uint32_t feature_flags;                              /*!< 功能开关位掩码 */
    char     language[SERVICE_NVS_LANGUAGE_MAX_LEN];     /*!< 语言 ID */
    char     theme_name[SERVICE_NVS_THEME_MAX_LEN];      /*!< 主题 ID */
    uint8_t  idle_timeout_index;                         /*!< 自动熄屏时间选项索引 */
    bool     auto_sleep_enabled;                         /*!< 自动休眠开关 */
    uint8_t  boot_screen_index;                          /*!< 开机默认页面索引 */

    /* wifi 组 */
    char     wifi_ssid[SERVICE_NVS_SSID_MAX_LEN];        /*!< Wi-Fi SSID */
    char     wifi_password[SERVICE_NVS_PASS_MAX_LEN];    /*!< Wi-Fi 密码 */

    /* xiaozhi 组 */
    service_nvs_xiaozhi_cfg_t xiaozhi;                   /*!< 小智 Client-Id / WebSocket URL / Token */

    /* App 分组 */
    uint32_t ear_best[6];                                /*!< 练耳历史最高分 */
    uint8_t  ear_mode;                                   /*!< 练耳当前模式：0 绝对/1 相对 */
    uint8_t  ear_difficulty;                             /*!< 练耳当前难度：0 初级/1 中级/2 高级 */
    bool     ear_practice_mode;                          /*!< 练耳练习/挑战模式：true 练习 */
    service_nvs_metronome_t metronome;                   /*!< 节拍器参数 */
    service_nvs_piano_t piano;                           /*!< 小钢琴音色选择参数 */
    service_nvs_sequencer_t sequencer;                   /*!< 音序器 App 参数 */
    service_nvs_zen_t zen;                               /*!< 禅模式 App 参数 */
    bool     clock_12h;                                  /*!< 时钟 App 12h 制式 */
    uint32_t clock_timer_s;                              /*!< 时钟 App 定时器目标时长（秒） */
    service_nvs_app_fun_mana_t app_fun_mana;             /*!< 娱乐 App 蓝量 */
    service_nvs_midi_player_t midi_player;               /*!< MIDI 播放器状态 */
    service_nvs_recorder_t recorder;                   /*!< 录音机 App 参数 */
};

/** @brief 全局系统参数实例，各模块可直接读取 */
extern struct s_system_parameters system_parameters;

/**
 * @brief 初始化 NVS 参数管理服务
 *
 * 打开 NVS 命名空间，加载所有参数到 RAM 缓存；若首次运行则写入默认值。
 *
 * @return ESP_OK 成功
 */
esp_err_t service_nvs_init(void);

/**
 * @brief 将缓存中所有脏数据提交到 NVS
 *
 * @return ESP_OK 成功
 */
esp_err_t service_nvs_commit(void);

/**
 * @brief 重新从 NVS 加载所有参数到缓存
 *
 * @return ESP_OK 成功
 */
esp_err_t service_nvs_load(void);

/**
 * @brief 恢复默认值并写入 NVS
 *
 * @return ESP_OK 成功
 */
esp_err_t service_nvs_reset_to_defaults(void);

/**
 * @brief 恢复出厂设置并立即重启
 *
 * 执行：
 * - 清空所有用户设置（Wi-Fi、语言/主题/音量/亮度、功能开关、
 *   小智激活凭据、练耳最高分、节拍器/钢琴/鼓垫参数、
 *   时钟 12h 制式与定时器、娱乐 App 蓝量、MIDI 播放器状态等）
 * - 保留内部调试参数：boot_count（上电计数）、custom_mac（自定义退路 MAC）
 * - 清除 initialized 标志：重启后 engine_gui 100% 进度会跳 onboard 引导屏
 * - 最后调用 esp_restart() 硬复位（不返回）
 *
 * @return 仅在 reset_to_defaults 阶段失败时返回错误码；成功重启路径不返回
 */
esp_err_t service_nvs_factory_reset(void);

/**
 * @brief 获取系统是否已完成首次初始化
 */
bool service_nvs_is_initialized(void);

/**
 * @brief 设置系统初始化标志
 */
esp_err_t service_nvs_set_initialized(bool initialized);

/**
 * @brief 获取启动计次
 */
uint32_t service_nvs_get_boot_count(void);

/**
 * @brief 启动计次加一并提交
 */
esp_err_t service_nvs_increment_boot_count(void);

/**
 * @brief 获取屏幕亮度（0-100）
 */
uint8_t service_nvs_get_brightness(void);

/**
 * @brief 设置屏幕亮度（0-100）
 */
esp_err_t service_nvs_set_brightness(uint8_t brightness);

/**
 * @brief 获取系统音量（0-100）
 */
int16_t service_nvs_get_volume(void);

/**
 * @brief 设置系统音量（0-100）
 */
esp_err_t service_nvs_set_volume(int16_t volume);

/**
 * @brief 获取 Wi-Fi SSID
 *
 * @param ssid 输出缓冲区
 * @param len  缓冲区长度
 */
esp_err_t service_nvs_get_wifi_ssid(char *ssid, size_t len);

/**
 * @brief 设置 Wi-Fi SSID
 */
esp_err_t service_nvs_set_wifi_ssid(const char *ssid);

/**
 * @brief 获取 Wi-Fi 密码
 *
 * @param password 输出缓冲区
 * @param len      缓冲区长度
 */
esp_err_t service_nvs_get_wifi_password(char *password, size_t len);

/**
 * @brief 设置 Wi-Fi 密码
 */
esp_err_t service_nvs_set_wifi_password(const char *password);

/**
 * @brief 获取全部功能开关位掩码
 */
uint32_t service_nvs_get_feature_flags(void);

/**
 * @brief 设置全部功能开关位掩码
 */
esp_err_t service_nvs_set_feature_flags(uint32_t flags);

/**
 * @brief 获取单个功能开关状态
 *
 * @param mask 位掩码，如 SERVICE_NVS_FLAG_WIFI_ENABLED
 */
bool service_nvs_get_feature_flag(uint32_t mask);

/**
 * @brief 设置单个功能开关状态
 */
esp_err_t service_nvs_set_feature_flag(uint32_t mask, bool enable);

/**
 * @brief 获取语言 ID
 *
 * @param[out] lang  输出缓冲区
 * @param[in]  len   缓冲区长度
 */
esp_err_t service_nvs_get_language(char *lang, size_t len);

/**
 * @brief 设置语言 ID
 */
esp_err_t service_nvs_set_language(const char *lang);

/**
 * @brief 获取主题 ID
 *
 * @param[out] theme  输出缓冲区
 * @param[in]  len    缓冲区长度
 */
esp_err_t service_nvs_get_theme(char *theme, size_t len);

/**
 * @brief 设置主题 ID
 */
esp_err_t service_nvs_set_theme(const char *theme);

/**
 * @brief 读取练耳历史最高分
 * @param[in] index 记录索引：模式(0 绝对/1 相对)*3 + 难度(0~2)，越界返回 0
 */
uint32_t service_nvs_get_ear_best(uint8_t index);

/**
 * @brief 写入练耳历史最高分（标记脏，由 service_nvs_commit 统一落盘）
 * @param[in] index 记录索引：模式(0 绝对/1 相对)*3 + 难度(0~2)
 * @param[in] score 得分
 */
esp_err_t service_nvs_set_ear_best(uint8_t index, uint32_t score);

/**
 * @brief 写入练耳 App 当前配置（模式/难度/练习挑战），下次进入时恢复
 *
 * 标记脏位后由 service_nvs_commit 统一落盘，调用方在 App 退出前调用一次即可。
 */
esp_err_t service_nvs_set_ear_cfg(uint8_t mode, uint8_t difficulty, bool practice_mode);

/**
 * @brief 读取节拍器参数
 * @param[out] out 参数输出；未初始化或参数为 NULL 时输出默认值
 */
void service_nvs_get_metronome(service_nvs_metronome_t *out);

/**
 * @brief 写入节拍器参数（标记脏，由 service_nvs_commit 统一落盘）
 */
esp_err_t service_nvs_set_metronome(const service_nvs_metronome_t *params);

/**
 * @brief 读取小钢琴参数
 * @param[out] out 参数输出；未初始化或参数为 NULL 时输出默认值
 */
void service_nvs_get_piano(service_nvs_piano_t *out);

/**
 * @brief 写入小钢琴参数（标记脏，由 service_nvs_commit 统一落盘）
 */
esp_err_t service_nvs_set_piano(const service_nvs_piano_t *params);

/** @brief 读取音序器 App 参数；未初始化或参数为 NULL 时输出默认值 */
void service_nvs_get_sequencer(service_nvs_sequencer_t *out);

/** @brief 写入音序器 App 参数（标记脏，由 service_nvs_commit 统一落盘） */
esp_err_t service_nvs_set_sequencer(const service_nvs_sequencer_t *params);

/** @brief 读取禅模式 App 参数；未初始化或参数为 NULL 时输出默认值 */
void service_nvs_get_zen(service_nvs_zen_t *out);

/** @brief 写入禅模式 App 参数（标记脏，由 service_nvs_commit 统一落盘） */
esp_err_t service_nvs_set_zen(const service_nvs_zen_t *params);

/** @brief 读取录音机 App 参数；未初始化或参数为 NULL 时输出默认值 */
void service_nvs_get_recorder(service_nvs_recorder_t *out);

/** @brief 写入录音机 App 参数（标记脏，由 service_nvs_commit 统一落盘） */
esp_err_t service_nvs_set_recorder(const service_nvs_recorder_t *params);

/** @brief SF2 音源文件名最大长度（与 engine_sf2 扫描缓存一致） */
#define SERVICE_NVS_SF2_SOURCE_MAX_LEN 96

/**
 * @brief 读取 SF2 音源选择
 * @param[out] out 文件名输出（不含路径）；空串 = 内部预设（含未初始化）
 */
esp_err_t service_nvs_get_sf2_source(char *out, size_t len);

/**
 * @brief 写入 SF2 音源选择（setter 内直接提交落盘）
 * @param[in] name SD 文件名（不含路径）；空串 = 内部预设
 */
esp_err_t service_nvs_set_sf2_source(const char *name);


/**
 * @brief 获取时钟 App 12h 制式标志
 */
bool service_nvs_get_clock_12h(void);

/**
 * @brief 设置时钟 App 12h 制式标志（标记脏，由 service_nvs_commit 统一落盘）
 */
esp_err_t service_nvs_set_clock_12h(bool use_12h);

/**
 * @brief 获取时钟 App 上次定时器目标时长（秒）
 */
uint32_t service_nvs_get_clock_timer_s(void);

/**
 * @brief 设置时钟 App 定时器目标时长（标记脏，由 service_nvs_commit 统一落盘）
 */
esp_err_t service_nvs_set_clock_timer_s(uint32_t seconds);

/**
 * @brief 获取小智 Client-Id（UUID）
 *
 * @param[out] uuid 输出缓冲区
 * @param[in]  len  缓冲区长度
 */
esp_err_t service_nvs_get_xz_uuid(char *uuid, size_t len);

/**
 * @brief 设置小智 Client-Id（UUID）
 */
esp_err_t service_nvs_set_xz_uuid(const char *uuid);

/**
 * @brief 获取小智 WebSocket URL
 *
 * @param[out] url 输出缓冲区
 * @param[in]  len 缓冲区长度
 */
esp_err_t service_nvs_get_xz_ws_url(char *url, size_t len);

/**
 * @brief 设置小智 WebSocket URL
 */
esp_err_t service_nvs_set_xz_ws_url(const char *url);

/**
 * @brief 获取小智 WebSocket Token
 *
 * @param[out] token 输出缓冲区
 * @param[in]  len   缓冲区长度
 */
esp_err_t service_nvs_get_xz_ws_token(char *token, size_t len);

/**
 * @brief 设置小智 WebSocket Token
 */
esp_err_t service_nvs_set_xz_ws_token(const char *token);

/**
 * @brief 获取小智全局唤醒开关（任意界面唤醒后台对话）
 */
bool service_nvs_get_xz_wake_anywhere(void);

/**
 * @brief 设置小智全局唤醒开关
 */
esp_err_t service_nvs_set_xz_wake_anywhere(bool enable);

/**
 * @brief 获取自定义 MAC 地址
 *
 * @param[out] mac 输出缓冲区（至少 6 字节）
 *
 * @return ESP_OK 成功，返回已设置的 custom_mac；若未设置则返回全零并填充到 out
 */
esp_err_t service_nvs_get_custom_mac(uint8_t *mac, size_t len);

/**
 * @brief 设置自定义 MAC 地址
 *
 * 当 ESP32-P4 eFuse BLK1 未烧录 base MAC 时，使用此方法存储退路 MAC。
 *
 * @param[in] mac 自定义 MAC 地址（6 字节）
 *
 * @return ESP_OK 成功
 */
esp_err_t service_nvs_set_custom_mac(const uint8_t *mac);

/**
 * @brief 读取娱乐 App 蓝量与日期
 * @param[out] out 输出参数；未保存时返回 mana=100，其余为 0
 * @return ESP_OK 成功
 */
esp_err_t service_nvs_get_app_fun_mana(service_nvs_app_fun_mana_t *out);

/**
 * @brief 写入娱乐 App 蓝量与日期并立即提交
 * @param[in] in 输入参数
 * @return ESP_OK 成功
 */
esp_err_t service_nvs_set_app_fun_mana(const service_nvs_app_fun_mana_t *in);

/**
 * @brief 读取 MIDI 播放器上次播放状态
 * @param[out] out 输出参数；未保存时返回 play_type=0，filename 为空
 * @return ESP_OK 成功
 */
esp_err_t service_nvs_get_midi_player(service_nvs_midi_player_t *out);

/**
 * @brief 写入 MIDI 播放器当前播放状态并立即提交
 * @param[in] in 输入参数
 * @return ESP_OK 成功
 */
esp_err_t service_nvs_set_midi_player(const service_nvs_midi_player_t *in);

/**
 * @brief 读取自动熄屏时间选项索引（0-5 对应 15s/30s/1m/3m/5m/10m）
 */
uint8_t service_nvs_get_idle_timeout_index(void);

/**
 * @brief 设置自动熄屏时间选项索引
 */
esp_err_t service_nvs_set_idle_timeout_index(uint8_t index);

/**
 * @brief 读取自动休眠开关
 */
bool service_nvs_get_auto_sleep_enabled(void);

/**
 * @brief 设置自动休眠开关
 */
esp_err_t service_nvs_set_auto_sleep_enabled(bool enable);

/**
 * @brief 读取开机默认页面索引（0=launcher, 1=clock, 2=tiny_piano, 3=ai_agent, 4=zen_mode）
 */
uint8_t service_nvs_get_boot_screen_index(void);

/**
 * @brief 设置开机默认页面索引
 */
esp_err_t service_nvs_set_boot_screen_index(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_NVS_H */
