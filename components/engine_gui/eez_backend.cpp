/**
 * @file eez_backend.cpp
 * @brief EEZ Studio Native Action 后端实现
 *
 * 本文件实现 src/ui/actions.h 中声明的所有 Native Action。
 * 所有 UI 事件统一转换为标准 MIDI 消息或内部 SysEx 消息，通过 engine_midi 总线发布。
 * 后端不直接访问 UI 控件，也不直接调用 AppManager / Service API。
 */

#include "eez-flow.h"
#include "actions.h"
#include "engine_midi.h"
#include "engine_gui.h"
#include "app_manager.h"
#include "service_page.h"
#include "vars.h"
#include "esp_log.h"
#include "string.h"

static const char *TAG = "eez_backend";

/* 用于区分 boot percent 的写入来源：true 表示来自后端 engine_gui_set_flow_var_int，
 * false 表示来自 EEZ Flow 内部（如 boot 屏幕旧演示动作）。
 * 生成代码中的 setGlobalVariable 会检查此标志，确保只有后端能控制启动进度。 */
bool g_engine_gui_boot_percent_real = false;

static void publish_midi_event(const engine_midi_event_t *evt)
{
    esp_err_t ret = engine_midi_publish(evt, 0);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "publish failed: %d", ret);
    }
}

static void publish_sysex(uint8_t cmd, uint8_t func, uint8_t p1, uint8_t p2)
{
    engine_midi_event_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.type       = ENGINE_MIDI_MSG_SYSEX;
    evt.sysex_len  = 4;
    evt.sysex_data[0] = cmd;
    evt.sysex_data[1] = func;
    evt.sysex_data[2] = p1;
    evt.sysex_data[3] = p2;
    evt.source_port = ENGINE_MIDI_PORT_INTERNAL;
    publish_midi_event(&evt);
}

extern "C" void action_midi_sysex(lv_event_t *e)
{
    (void)e;

    int cmd  = eez::flow::getUserProperty(ACTION_MIDI_SYSEX_PROPERTY_COMMAND).getInt32();
    int func = eez::flow::getUserProperty(ACTION_MIDI_SYSEX_PROPERTY_FUNCTION).getInt32();
    int p1   = eez::flow::getUserProperty(ACTION_MIDI_SYSEX_PROPERTY_PARAM_1).getInt32();
    int p2   = eez::flow::getUserProperty(ACTION_MIDI_SYSEX_PROPERTY_PARAM_2).getInt32();

    ESP_LOGD(TAG, "sysex: cmd=%d func=%d p1=%d p2=%d", cmd, func, p1, p2);

    publish_sysex((uint8_t)cmd, (uint8_t)func, (uint8_t)p1, (uint8_t)p2);
}

extern "C" void action_widget_event(lv_event_t *e)
{
    if (e == NULL) {
        return;
    }

    /* 页面级事件统一处理：屏幕加载完成即唤醒/退出 App，不进入 App 的 on_ui_event */
    if (lv_event_get_code(e) == LV_EVENT_SCREEN_LOADED) {
        engine_gui_on_screen_loaded((lv_obj_t *)lv_event_get_target_obj(e));
        return;
    }

    app_base_t *active = app_manager_get_active();
    if (active != NULL && active->on_ui_event != NULL) {
        active->on_ui_event(active, e);
        return;
    }

    /* 非 App 屏幕：交给系统屏幕处理器 */
    service_page_feed_event(e);
}

extern "C" bool engine_gui_get_flow_var_bool(uint32_t var_idx)
{
    return eez::flow::getGlobalVariable(var_idx).getBoolean();
}

extern "C" int64_t engine_gui_get_flow_var_date(uint32_t var_idx)
{
    return (int64_t)eez::flow::getGlobalVariable(var_idx).getDouble();
}

extern "C" void engine_gui_set_flow_var_bool(uint32_t var_idx, bool value)
{
    eez::flow::setGlobalVariable(var_idx, eez::BooleanValue(value));
}

extern "C" const char *engine_gui_get_flow_var_string(uint32_t var_idx)
{
    return eez::flow::getGlobalVariable(var_idx).getString();
}

extern "C" void engine_gui_set_flow_var_string(uint32_t var_idx, const char *str)
{
    eez::flow::setGlobalVariable(var_idx, eez::Value(str));
}

extern "C" void engine_gui_set_flow_var_date(uint32_t var_idx, int64_t ms)
{
    eez::flow::setGlobalVariable(var_idx, eez::Value((double)ms, eez::VALUE_TYPE_DATE));
}

extern "C" void engine_gui_set_flow_var_int(uint32_t var_idx, int32_t value)
{
    if (var_idx == FLOW_GLOBAL_VARIABLE_SYS_BOOT_PERCENT) {
        g_engine_gui_boot_percent_real = true;
        eez::flow::setGlobalVariable(var_idx, eez::Value((int)value, eez::VALUE_TYPE_INT32));
        g_engine_gui_boot_percent_real = false;
    } else {
        eez::flow::setGlobalVariable(var_idx, eez::Value((int)value, eez::VALUE_TYPE_INT32));
    }
}

extern "C" void engine_gui_install_eez_hooks(void)
{
    static auto s_stop_script_hook = []() {
        ESP_LOGE(TAG, "EEZ Flow stopScript called, check Flow expressions / page transitions");
    };
    eez::flow::stopScriptHook = s_stop_script_hook;
}

extern "C" void engine_gui_color_theme_hook(const char *themeName)
{
    engine_gui_set_theme(themeName);
}

extern "C" void engine_gui_install_color_theme_hook(void)
{
    eez::flow::lvglSetColorThemeHook = engine_gui_color_theme_hook;
}

extern "C" lv_obj_t *engine_gui_find_widget_by_name(const char *name)
{
    if (name == NULL || eez::flow::getLvglObjectByNameHook == NULL) {
        return NULL;
    }

    int32_t idx = eez::flow::getLvglObjectByNameHook(name);
    if (idx < 0) {
        return NULL;
    }

    if (eez::flow::getLvglObjectFromIndexHook == NULL) {
        return NULL;
    }

    return eez::flow::getLvglObjectFromIndexHook(idx);
}
