#ifndef EEZ_LVGL_UI_VARS_H
#define EEZ_LVGL_UI_VARS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// enum declarations

typedef enum {
    ENUM_APP_APP_ZEN_MODE = 0,
    ENUM_APP_APP_EAR_TRAINER = 1,
    ENUM_APP_APP_CHORD_TRAIN = 2,
    ENUM_APP_APP_CIRCLE_OF_FIFTHS = 3,
    ENUM_APP_APP_TINY_PIANO = 4,
    ENUM_APP_APP_DRUM_PAD = 5,
    ENUM_APP_APP_MIDI_PLAYER = 6,
    ENUM_APP_APP_XY_PAD = 7,
    ENUM_APP_APP_METRONOME = 8,
    ENUM_APP_APP_AI_AGENT = 9,
    ENUM_APP_APP_CLOCK_CALENDAR = 10,
    ENUM_APP_APP_FUN = 11,
    ENUM_APP_APP_SUM = 12
} ENUM_APP;

typedef enum {
    ENUM_SYS_COMMAND_CMD_SYSTEM = 0,
    ENUM_SYS_COMMAND_CMD_APP = 1,
    ENUM_SYS_COMMAND_CMD_AUDIO = 2,
    ENUM_SYS_COMMAND_CMD_INPUT = 3,
    ENUM_SYS_COMMAND_CMD_UI = 4,
    ENUM_SYS_COMMAND_CMD_FILE = 5,
    ENUM_SYS_COMMAND_CMD_MIDI = 6,
    ENUM_SYS_COMMAND_CMD_APP_CONTROL = 7,
    ENUM_SYS_COMMAND_CMD_SUM = 8
} ENUM_SYS_COMMAND;

typedef enum {
    ENUM_FUNC_APP_FUNC_APP_LAUNCH = 0,
    ENUM_FUNC_APP_FUNC_APP_SUSPEND = 1,
    ENUM_FUNC_APP_FUNC_APP_RESUME = 2,
    ENUM_FUNC_APP_FUNC_APP_KILL = 3,
    ENUM_FUNC_APP_FUNC_APP_BACK = 4,
    ENUM_FUNC_APP_FUNC_APP_SET_PARAM = 5,
    ENUM_FUNC_APP_FUNC_APP_KILL_ALL = 6,
    ENUM_FUNC_APP_FUNC_APP_SUM = 7
} ENUM_FUNC_APP;

typedef enum {
    ENUM_FUNC_SYSTEM_FUNC_SYSTEM_REBOOT = 0,
    ENUM_FUNC_SYSTEM_FUNC_SYSTEM_SLEEP = 1,
    ENUM_FUNC_SYSTEM_FUNC_SYSTEM_RTC_SYNC_LOCAL = 2,
    ENUM_FUNC_SYSTEM_FUNC_SYSTEM_WIFI_RESET = 3,
    ENUM_FUNC_SYSTEM_FUNC_SYSTEM_FACTORY_RESET = 4,
    ENUM_FUNC_SYSTEM_FUNC_SYSTEM_SCREENSHOT = 5,
    ENUM_FUNC_SYSTEM_FUNC_SYSTEM_CHANGE_LANGUAGE = 6,
    ENUM_FUNC_SYSTEM_FUNC_SYSTEM_SUM = 7
} ENUM_FUNC_SYSTEM;

// Flow global variables

enum FlowGlobalVariables {
    FLOW_GLOBAL_VARIABLE_SYS_BOOT_PERCENT = 0,
    FLOW_GLOBAL_VARIABLE_SYS_RANDOM = 1,
    FLOW_GLOBAL_VARIABLE_SYS_VERSION_STR = 2,
    FLOW_GLOBAL_VARIABLE_SYS_BUILD_STR = 3,
    FLOW_GLOBAL_VARIABLE_SYS_DATE = 4,
    FLOW_GLOBAL_VARIABLE_SYS_STATUS_BAR = 5,
    FLOW_GLOBAL_VARIABLE_SYS_NOTIFICATION_BAR = 6,
    FLOW_GLOBAL_VARIABLE_SYS_ANMINATION = 7,
    FLOW_GLOBAL_VARIABLE_SYS_TIME_STR = 8
};

// Native global variables

extern bool get_var_sys_onboard_flag();
extern void set_var_sys_onboard_flag(bool value);
extern bool get_var_sys_online_status();
extern void set_var_sys_online_status(bool value);
extern int32_t get_var_sys_main_volume();
extern void set_var_sys_main_volume(int32_t value);
extern int32_t get_var_sys_main_brightness();
extern void set_var_sys_main_brightness(int32_t value);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_VARS_H*/