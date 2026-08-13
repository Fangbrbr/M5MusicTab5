/**
 * @file app_manager.h
 * @brief App 生命周期管理
 *
 * 维护 App 注册表，负责启动、挂起、恢复、杀死与资源回收。
 */

#ifndef APP_MANAGER_H
#define APP_MANAGER_H

#include "esp_err.h"
#include "stdbool.h"
#include "stdint.h"
#include <time.h>
#include "engine_midi.h"
#include "service_recorder.h"
#include "lvgl.h"
#include "app_ui_binding.h"
#include "app_input_event.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct app_base app_base_t;

struct app_base {

    const char *name;                        /**< App 显示名称 */
    const char *screen_name;                 /**< 对应 EEZ 屏幕名，如 "app_chord_memory" */

    void *screen_ctx;                        /**< App 私有 screen context 指针 */
    size_t screen_ctx_size;                  /**< screen context 大小 */
    const widget_binding_t *widget_bindings; /**< 控件绑定表，以 WIDGET_BINDING_END 结束 */

    bool (*on_init)(app_base_t *self, void *screen_ctx);
    void (*on_render)(app_base_t *self, float *L, float *R, uint32_t samples);
    void (*on_update)(app_base_t *self);
    void (*on_pause)(app_base_t *self);
    void (*on_resume)(app_base_t *self);
    void (*on_destroy)(app_base_t *self);
    void (*on_input)(app_base_t *self, const app_input_event_t *evt);
    void (*on_sysex)(app_base_t *self, const engine_midi_event_t *evt);
    void (*on_ui_event)(app_base_t *self, lv_event_t *e); /**< EEZ 控件事件回调 */

    void *user_data;
};

extern uint32_t sys_monitor_tick;
/**
 * @brief GUI 后端回调：切换屏幕
 * @param[in] screen_name EEZ 屏幕名；NULL 表示切回 Launcher
 */
typedef void (*app_manager_switch_screen_cb_t)(const char *screen_name);

/**
 * @brief GUI 后端回调：按 Widget Name 查找控件
 * @param[in] name Widget Name
 * @return 控件指针；未找到返回 NULL
 */
typedef lv_obj_t *(*app_manager_find_widget_cb_t)(const char *name);

/**
 * @brief 注册 GUI 后端回调
 *
 * 由 engine_gui 在初始化时注册，使 app_manager 能够在生命周期切换时
 * 切屏并绑定控件，同时避免 app_manager 直接依赖 engine_gui。
 *
 * @param[in] switch_cb 屏幕切换回调
 * @param[in] find_cb   控件查找回调
 */
void app_manager_register_gui_callbacks(app_manager_switch_screen_cb_t switch_cb,
                                        app_manager_find_widget_cb_t find_cb);

/**
 * @brief 初始化 App 管理器
 */
esp_err_t app_manager_init(void);

/**
 * @brief 请求启动指定名称的 App（异步，由 task_app 在下一个周期执行）
 *
 * 供非 App 任务（如 task_comm 中的 engine_gui SysEx 消费者）调用，
 * 避免在中断/通信任务上下文中直接执行切屏与生命周期回调。
 *
 * @param[in] name App 名称
 */
void app_manager_request_launch(const char *name);

/**
 * @brief 请求销毁当前激活的 App（异步）
 */
void app_manager_request_kill_active(void);

/**
 * @brief 按屏幕名请求启动 App（异步）
 * @param[in] screen_name 注册 App 的 screen_name
 *
 * 注册表内按 screen_name 匹配 App；目标 App 已激活时忽略，
 * 避免切屏回调重新触发屏幕加载事件造成的唤醒环路。
 */
void app_manager_request_launch_by_screen(const char *screen_name);

/**
 * @brief 处理待执行的 App 生命周期请求
 *
 * 由 task_app 每周期调用一次。
 */
void app_manager_process_requests(void);

/**
 * @brief 为指定 App 绑定当前屏幕的控件到 screen_ctx
 * @param[in] app App 实例
 * @return true 绑定成功（允许部分控件缺失）
 */
bool app_manager_bind_screen(app_base_t *app);

/**
 * @brief 注册一个 App
 * @param[in] app 参数
 */
esp_err_t app_manager_register(app_base_t *app);

/**
 * @brief 启动指定名称的 App
 * @param[in] name 参数
 */
bool app_manager_launch(const char *name);

/**
 * @brief 挂起指定名称的 App
 * @param[in] name 参数
 */
bool app_manager_suspend(const char *name);

/**
 * @brief 恢复指定名称的 App
 * @param[in] name 参数
 */
bool app_manager_resume(const char *name);

/**
 * @brief 杀死指定名称的 App 并回收资源
 * @param[in] name 参数
 */
bool app_manager_kill(const char *name);

/**
 * @brief 杀死当前激活的 App（不从注册表移除，便于再次启动）
 */
void app_manager_kill_active(void);

/**
 * @brief 获取当前激活的 App 实例
 */
app_base_t * app_manager_get_active(void);

/**
 * @brief 向当前激活的 App 分发输入事件
 * @param[in] evt 输入事件
 */
void app_manager_feed_input(const app_input_event_t *evt);

/**
 * @brief 设置运行级通知文本，带超时自动清空（抢占式）
 * @param[in] text       提示文本；NULL 或空字符串表示清除
 * @param[in] timeout_ms 超时毫秒数；0 表示常驻直到新推送覆盖或手动清除
 *
 * 调用后会覆盖当前任何通知（包括插入式通知）。
 */
void app_manager_show_notification_timeout(const char *text, uint32_t timeout_ms);

/**
 * @brief 格式化设置运行级通知文本，带超时自动清空（抢占式）
 * @param[in] timeout_ms 超时毫秒数；0 表示常驻
 * @param[in] fmt        格式化字符串
 */
void app_manager_show_notificationf_timeout(uint32_t timeout_ms, const char *fmt, ...);

/**
 * @brief 设置插入式运行级通知，超时后自动恢复之前的通知内容
 * @param[in] text       提示文本；NULL 表示空文本
 * @param[in] timeout_ms 超时毫秒数；必须大于 0；传 0 等价于清除插入层
 *
 * 插入式通知不会破坏原有通知，超时后恢复 base 内容及其计时器状态。
 * 若插入期间 base 也已超时，则恢复时一并清空。
 */
void app_manager_show_notification_insert_timeout(const char *text, uint32_t timeout_ms);

/**
 * @brief 格式化设置插入式运行级通知，超时后自动恢复之前的通知内容
 * @param[in] timeout_ms 超时毫秒数；必须大于 0；传 0 等价于清除插入层
 * @param[in] fmt        格式化字符串
 */
void app_manager_show_notificationf_insert_timeout(uint32_t timeout_ms, const char *fmt, ...);

/**
 * @brief 清除当前运行级通知（同时结束插入式通知）
 */
void app_manager_clear_notification(void);

/**
 * @brief 清除当前插入式通知并恢复之前的 base 通知内容
 */
void app_manager_clear_notification_insert(void);

/**
 * @brief 获取当前通知文本（供 GUI 层读取）
 * @param[out] buf 输出缓冲
 * @param[in] len 缓冲长度
 * @return ESP_OK 成功
 */
esp_err_t app_manager_get_notification(char *buf, size_t len);

/**
 * @brief 在生命周期锁保护下调用当前激活 App 的 on_update
 *
 * 由 task_app 每 10 ms 调用一次。通过该接口可保证 on_update 与
 * task_comm 分发的 on_input/on_sysex 以及 App 切换操作互斥。
 */
void app_manager_process_active(void);

/**
 * @brief App Manager 周期处理（通知超时等）
 * 由 task_app 每 10 ms 调用一次
 */
void app_manager_process(void);

/**
 * @brief 获取 RTC 时间
 * @param[out] tm 时间结构体
 * @return ESP_OK 成功
 */
esp_err_t app_manager_get_time(struct tm *tm);

/**
 * @brief App 向 MIDI 总线发布一条内部 SysEx
 *
 * 自动标记来源端口为 ENGINE_MIDI_PORT_APP，App Manager 会忽略该端口事件，
 * 避免回环。供 App 向其他 MIDI 消费者（USB/BLE 等）或未来 UI 监听器发送消息。
 *
 * @param[in] cmd  指令码
 * @param[in] func 功能码
 * @param[in] p1   参数 1
 * @param[in] p2   参数 2
 * @return ESP_OK 成功
 */
esp_err_t app_manager_publish_sysex(uint8_t cmd, uint8_t func, uint8_t p1, uint8_t p2);

/**
 * @brief 请求开始录制当前 App 的 MIDI 输出
 *
 * App 不直接调用 service_recorder，统一由 AppManager 转发，
 * 保持 App 层与 Service 层解耦。
 *
 * @param[in] tag 来源标签，用于文件名与文件头
 * @return service_recorder_result_t 操作结果
 */
service_recorder_result_t app_manager_record_start(const char *tag);

/**
 * @brief 请求停止录制
 * @return service_recorder_result_t 操作结果
 */
service_recorder_result_t app_manager_record_stop(void);

/**
 * @brief 是否正在录制中
 */
bool app_manager_record_is_recording(void);

/**
 * @brief 获取最近一次成功录制文件的绝对路径
 *
 * @param[out] buf 输出缓冲
 * @param[in] len  缓冲长度
 * @return true 存在；false 无或参数错误
 */
bool app_manager_record_get_last_path(char *buf, size_t len);

/**
 * @brief 注册全部 P0 App
 * @return ESP_OK 成功
 */
esp_err_t app_manager_register_all(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_MANAGER_H */
