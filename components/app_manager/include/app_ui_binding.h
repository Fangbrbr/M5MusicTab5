/**
 * @file app_ui_binding.h
 * @brief App 屏幕控件绑定契约
 *
 * 每个需要后端操作 EEZ 控件的 App，声明一个 ui_screen_xxx_t 结构体，
 * 并用 widget_binding_t[] 描述字段与 EEZ Widget Name 的映射关系。
 * app_manager 在切屏时按名称查找控件并填充结构体，App 后续直接通过
 * 类型安全的字段访问控件，无需 objects.xxx 或字符串查找。
 */

#ifndef APP_UI_BINDING_H
#define APP_UI_BINDING_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 控件类型，仅用于调试与运行时校验
 */
typedef enum {
    WIDGET_KIND_ANY,
    WIDGET_KIND_LABEL,
    WIDGET_KIND_BUTTON,
    WIDGET_KIND_SLIDER,
    WIDGET_KIND_DROPDOWN,
    WIDGET_KIND_ROLLER,
    WIDGET_KIND_CHECKBOX,
    WIDGET_KIND_SWITCH,
    WIDGET_KIND_CANVAS,
    WIDGET_KIND_PANEL,
} widget_kind_t;

/**
 * @brief 单个控件绑定描述
 */
typedef struct {
    const char *name;       /**< EEZ 中 Widget Name */
    size_t offset;          /**< 在 ui_screen_xxx_t 中的字节偏移 */
    widget_kind_t kind;     /**< 控件类型 */
} widget_binding_t;

/** @brief 绑定表结束标记 */
#define WIDGET_BINDING_END { NULL, 0, WIDGET_KIND_ANY }

/**
 * @brief 绑定表项宏
 * @param type  App 的 screen context 结构体类型
 * @param field 结构体字段名
 * @param widget_name EEZ 中 Widget Name
 * @param kind  控件类型
 */
#define WIDGET_BIND(type, field, widget_name, kind) \
    { widget_name, offsetof(type, field), kind }

#ifdef __cplusplus
}
#endif

#endif /* APP_UI_BINDING_H */
