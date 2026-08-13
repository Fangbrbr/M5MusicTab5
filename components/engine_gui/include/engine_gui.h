/**
 * @file engine_gui.h
 * @brief 图形引擎
 *
 * LVGL 后端初始化、EEZ Studio 前后端通信适配。
 */

#ifndef ENGINE_GUI_H
#define ENGINE_GUI_H

#include "esp_err.h"
#include "stdint.h"
#include "stdbool.h"
#include <lvgl.h>
#include "app_input_event.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COLOR_BG_PRIMARY,      // 0
    COLOR_BG_SECONDARY,    // 1
    COLOR_CARD,            // 2
    COLOR_PRIMARY,         // 3
    COLOR_SECONDARY,       // 4
    COLOR_TEXT_PRIMARY,    // 5
    COLOR_TEXT_SECONDARY,  // 6
    COLOR_SUCCESS,         // 7
    COLOR_ERROR,           // 8
    COLOR_DISABLE,         // 9
    COLOR_M1_PERCEIVE,     // 10
    COLOR_M2_DEFINE,       // 11
    COLOR_M3_BUILD,        // 12
    COLOR_M4_PERFORM,      // 13
    COLOR_M5_EXTEND,       // 14
    COLOR_TOOL,            // 15
    COLOR_SHADOW           // 16
} ColorName;


/**
 * @brief 初始化图形引擎（LVGL 显示 + EEZ UI）
 */
esp_err_t engine_gui_init(void);

/**
 * @brief GUI 周期 tick，驱动 EEZ UI 刷新
 */
void engine_gui_tick(void);

/**
 * @brief 读取 EEZ Flow 布尔型全局变量
 * @param[in] var_idx 变量索引（见 vars.h 中 FLOW_GLOBAL_VARIABLE_*）
 * @return 变量值
 */
bool engine_gui_get_flow_var_bool(uint32_t var_idx);

/**
 * @brief 设置 EEZ Flow 布尔型全局变量
 * @param[in] var_idx 变量索引（见 vars.h 中 FLOW_GLOBAL_VARIABLE_*）
 * @param[in] value   变量值
 */
void engine_gui_set_flow_var_bool(uint32_t var_idx, bool value);

/**
 * @brief 读取 EEZ Flow 日期型全局变量（毫秒级 Unix 时间戳）
 * @param[in] var_idx 变量索引
 * @return 毫秒时间戳
 */
int64_t engine_gui_get_flow_var_date(uint32_t var_idx);

/**
 * @brief 读取 EEZ Flow 字符串型全局变量
 * @param[in] var_idx 变量索引（见 vars.h 中 FLOW_GLOBAL_VARIABLE_*）
 * @return 字符串指针，无效时返回空字符串
 */
const char *engine_gui_get_flow_var_string(uint32_t var_idx);

/**
 * @brief 设置 EEZ Flow 字符串型全局变量
 * @param[in] var_idx 变量索引（见 vars.h 中 FLOW_GLOBAL_VARIABLE_*）
 * @param[in] str     字符串指针（需保持有效或静态）
 */
void engine_gui_set_flow_var_string(uint32_t var_idx, const char *str);

/**
 * @brief 设置 EEZ Flow 日期型全局变量（毫秒级 Unix 时间戳）
 * @param[in] var_idx 变量索引
 * @param[in] ms      毫秒时间戳
 */
void engine_gui_set_flow_var_date(uint32_t var_idx, int64_t ms);

/**
 * @brief 设置 EEZ Flow 整数型全局变量
 * @param[in] var_idx 变量索引
 * @param[in] value   变量值
 */
void engine_gui_set_flow_var_int(uint32_t var_idx, int32_t value);

/**
 * @brief 设置启动进度百分比（0-100）
 *
 * 由 main.c 在启动序列中调用，boot 屏幕进度条会自动反映该值，
 * 达到 100 后 EEZ Flow 自动切换到 home 屏幕。
 *
 * @param[in] percent 进度百分比，自动限制在 0-100
 */
void engine_gui_set_boot_percent(int32_t percent);

/**
 * @brief 在资源加载期间按需喂狗
 *
 * 当系统从 SPIFFS 加载资源时，读取速度较慢可能触发 task_wdt，
 * 调用本函数可在慢速路径下重置 watchdog。
 */
void engine_gui_feed_wdt_during_load(void);

/**
 * @brief 临时放宽 task_wdt 超时，用于 ui_init 等长阻塞操作
 */
void engine_gui_wdt_relax(void);

/**
 * @brief 恢复 task_wdt 原始超时，与 engine_gui_wdt_relax 成对使用
 */
void engine_gui_wdt_restore(void);

/**
 * @brief 安装 EEZ Flow 错误处理钩子
 *
 * 替换默认的 stopScript 钩子，避免前端表达式错误触发 assert 重启。
 * 由 engine_gui_init() 在 ui_init() 之后调用。
 */
void engine_gui_install_eez_hooks(void);

/**
 * @brief 安装 EEZ Flow 主题切换钩子
 *
 * 将 EEZ Studio 的 Set Color Theme 动作转发到后端，使主题切换能够持久化到 NVS。
 * 由 engine_gui_init() 在 ui_init() 之后调用。
 */
void engine_gui_install_color_theme_hook(void);

/**
 * @brief 设置当前主题
 *
 * 切换 EEZ 主题并持久化到 NVS。主题名需与 EEZ 工程中的 theme_names 一致：
 * "hammyorange"（Hammy橙）或 "starrynight"（星空黑）。
 *
 * @param[in] name 主题名
 */
void engine_gui_set_theme(const char *name);

/**
 * @brief 获取当前主题名
 * @return 主题名字符串，生命周期与 NVS 缓存一致
 */
const char *engine_gui_get_theme_name(void);

/**
 * @brief 按索引获取主题名
 *
 * 0: hammyorange, 1: starrynight，越界返回 hammyorange。
 */
const char *engine_gui_theme_name_by_index(uint8_t index);

/** @brief 主题总数 */
uint8_t engine_gui_theme_count(void);

/* 主题配色槽位总数，与 EEZ 工程 theme_colors[2][17] 一致 */
#define ENGINE_GUI_THEME_COLOR_COUNT 17

/**
 * @brief 取当前主题的配色
 * @param[in] index 配色槽位（0~16，语义见 EEZ 主题定义）
 * @return 颜色；越界收敛到最后一个槽位
 */
lv_color_t engine_gui_theme_color(uint8_t index);

/**
 * @brief 设置显示反向（横向 180° 翻转）
 *
 * 正向横向 = LVGL 旋转 90°，反向 = 270°；渲染与触摸坐标随 LVGL 旋转自动联动。
 * 递归锁保护，设置页回调（LVGL 任务内）与任意任务上下文均可调用。
 *
 * @param[in] inverted true=反向横向
 */
void engine_gui_set_display_inverted(bool inverted);

/** @brief 查询显示是否反向 */
bool engine_gui_get_display_inverted(void);

/**
 * @brief 按 Widget Name 查找 UI 控件
 *
 * 由 engine_gui 维护命名控件映射表，App 可通过本接口获取自己在 EEZ Studio
 * 中命名过的控件，无需直接依赖生成代码。
 *
 * @param[in] name EEZ Studio 中设置的 Widget Name
 * @return 控件指针，未找到返回 NULL
 */
lv_obj_t *engine_gui_find_widget(const char *name);

/** @brief 旧名兼容别名，等同于 engine_gui_find_widget */
static inline lv_obj_t *engine_gui_find_obj(const char *name)
{
    return engine_gui_find_widget(name);
}

/**
 * @brief 屏幕名转 EEZ SCREEN_ID
 * @param[in] name 屏幕名，如 "app_chord_memory"
 * @return SCREEN_ID，未找到返回 -1
 */
int16_t engine_gui_screen_name_to_id(const char *name);

/**
 * @brief 切换到指定 EEZ 屏幕
 * @param[in] screen_name 屏幕名；NULL 表示切回 Launcher
 */
void engine_gui_switch_screen(const char *screen_name);

/**
 * @brief 递归翻译对象树内所有 label 静态文案（按当前语言）
 *
 * 未命中词条表的文本原样保留；zh-CN 下为恒等映射零副作用。
 * 供各系统屏后端（boot/setting 等）复用。
 * @param[in] obj 根对象，NULL 忽略
 */
void engine_gui_translate_obj_tree(lv_obj_t *obj);

/**
 * @brief 切换到用户设置的开机默认页面
 */
void engine_gui_switch_to_boot_screen(void);

/**
 * @brief 屏幕加载完成事件的统一入口
 * @param[in] screen 被加载的屏幕对象
 *
 * App 屏幕加载即异步唤醒对应 App；加载非 App 屏幕时销毁当前激活 App。
 * 由 eez_backend 在 widget_event 中截获 LV_EVENT_SCREEN_LOADED 后调用。
 */
void engine_gui_on_screen_loaded(lv_obj_t *screen);

/**
 * @brief 同步设置界面的亮度显示值（0-100）
 *
 * Contract: 仅更新设置屏亮度滑块/数字绑定的 native 变量，不触碰背光硬件与 NVS。
 * MCP 等外部路径自行设置硬件+NVS 后调用本函数，使设置界面显示与实际一致。
 *
 * @param[in] value 亮度百分比，自动限制在 0-100
 */
void engine_gui_sync_brightness(int32_t value);

/**
 * @brief 同步设置界面的音量显示值（0-100）
 *
 * Contract: 仅更新设置屏音量滑块/数字绑定的 native 变量，不触碰音频服务与 NVS。
 *
 * @param[in] value 音量百分比，自动限制在 0-100
 */
void engine_gui_sync_volume(int32_t value);

/**
 * @brief 从触摸事件队列取出一个事件
 * @param[out] out 事件输出
 * @return true 成功取到事件
 */
bool engine_gui_get_touch_event(app_input_event_t *out);

#ifdef __cplusplus
}
#endif

#endif /* ENGINE_GUI_H */
