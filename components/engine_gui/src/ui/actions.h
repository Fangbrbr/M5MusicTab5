#ifndef EEZ_LVGL_UI_EVENTS_H
#define EEZ_LVGL_UI_EVENTS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    ACTION_WIDGET_EVENT_PROPERTY_EVENT,
};
extern void action_widget_event(lv_event_t * e);

enum {
    ACTION_MIDI_SYSEX_PROPERTY_COMMAND,
    ACTION_MIDI_SYSEX_PROPERTY_FUNCTION,
    ACTION_MIDI_SYSEX_PROPERTY_PARAM_1,
    ACTION_MIDI_SYSEX_PROPERTY_PARAM_2,
};
extern void action_midi_sysex(lv_event_t * e);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_EVENTS_H*/