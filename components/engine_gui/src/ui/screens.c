#include <string.h>

#include "screens.h"
#include "images.h"
#include "fonts.h"
#include "actions.h"
#include "vars.h"
#include "styles.h"
#include "ui.h"

#include <string.h>

objects_t objects;

static const char *screen_names[] = { "boot", "onboard_step", "launcher", "setting", "about", "app_zen_mode", "app_ear_train", "app_chord_memory", "app_circle_of_fifths", "app_tiny_piano", "app_drum_pad", "app_midi_player", "app_xy_mode", "app_metronome", "app_ai_agent", "app_clock", "app_fun", "ftp" };
static const char *object_names[] = { "boot", "onboard_step", "launcher", "setting", "about", "app_zen_mode", "app_ear_train", "app_chord_memory", "app_circle_of_fifths", "app_tiny_piano", "app_drum_pad", "app_midi_player", "app_xy_mode", "app_metronome", "app_ai_agent", "app_clock", "app_fun", "ftp", "boot_hammy", "boot_label_name_en", "boot_percent", "boot_label_loading", "boot_version", "boot_label_name", "obj0", "ob_step_prev", "ob_step_next", "step01_welcome", "ob_hammy", "ob_str1", "ob_str2", "ob_str3", "step02_datetime", "obj1", "obj2", "label_loading_2", "obj3", "ob_set_hour", "obj4", "ob_set_minute", "obj5", "obj6", "ob_set_second", "ob_set_time_result", "ob_set_time_save", "obj7", "ob_set_year", "obj8", "obj9", "ob_set_month", "obj10", "obj11", "ob_set_day", "obj12", "obj13", "step03_bg_vol", "obj14", "obj15", "label_loading_3", "obj16", "ob_slide_brightness", "ob_slide_brightness_num", "obj17", "ob_slide_volume", "ob_slide_volume_num", "ob_key_try_volume", "obj18", "step04_feature", "obj19", "obj20", "label_loading_5", "obj21", "ob_setting_theme", "obj22", "obj23", "ob_setting_on_screen", "obj24", "obj25", "ob_setting_time2idle", "obj26", "obj27", "ob_setting_auto_sleep", "step05_online", "obj28", "obj29", "label_loading_4", "obj30", "ob_setting_wifi_switch", "ob_set_wifi_connect_tip", "step06_finish_reboot", "obj31", "obj32", "obj33", "obj34", "launcher_zen", "obj35", "launcher_ear", "obj36", "launcher_fifth", "obj37", "launcher_chord", "obj38", "launcher_midi", "obj39", "launcher_xy", "obj40", "launcher_drum", "obj41", "launcher_piano", "obj42", "launcher_clock", "obj43", "launcher_ai", "obj44", "launcher_metron", "obj45", "launcher_fun", "obj46", "launcher_btn_setting", "launcher_led_ai", "obj47", "obj48", "obj49", "obj50", "setting_btn_home", "setting_btn_about", "setting_tab", "setting_tab_basic", "obj51", "setting_language", "obj52", "obj53", "setting_slide_brightness", "setting_slide_brightness_num", "obj54", "setting_slide_volume", "setting_slide_volume_num", "obj55", "setting_theme", "obj56", "obj57", "setting_on_screen", "obj58", "obj59", "setting_time2idle", "obj60", "obj61", "setting_auto_sleep", "obj62", "setting_invert_display", "setting_tab_advanced", "obj63", "setting_btn_wifi_detail", "obj64", "setting_btn_ftp", "obj65", "setting_btn_system_reset", "wifi_set_panel", "wifi_set_panel_return", "obj66", "setting_wifi_switch", "setting_wifi_connect_tip", "obj67", "setting_btn_wifi_reset", "setting_led_ai", "obj68", "obj69", "obj70", "obj71", "about_btn_return", "obj72", "obj73", "obj74", "obj75", "obj76", "obj77", "obj78", "obj79", "obj80", "obj81", "about_system_monitor_tick", "about_led_ai", "obj82", "obj83", "obj84", "obj85", "zen_btn_home", "obj86", "zen_canvas", "zen_ball_0", "zen_ball_1", "zen_ball_2", "zen_ball_3", "zen_ball_4", "obj87", "obj88", "zen_dropdown_mode", "obj89", "obj90", "zen_dropdown_key", "obj91", "obj92", "zen_dropdown_speed", "obj93", "zen_btn_rec", "obj94", "zen_led_ai", "obj95", "obj96", "obj97", "obj98", "ear_btn_home", "ear_key_try_play", "obj99", "obj100", "ear_label_try_count", "ear_score_title", "ear_score", "obj101", "ear_trainer_test", "obj102", "obj103", "ear_mode", "obj104", "obj105", "ear_difficult", "obj106", "obj107", "ear_best_score", "ear_key_major", "ear_key_minor2", "ear_key_minor3", "ear_key_interval", "ear_life_panel", "ear_life1", "ear_life2", "ear_life3", "ear_led_ai", "obj108", "obj109", "obj110", "obj111", "chord_btn_home", "chord_key_key", "obj112", "obj113", "chord_definition", "chord_panel_type_poll", "chord_type_maj", "chord_name", "chord_canvas_piano", "chord_led_ai", "obj114", "obj115", "obj116", "fifth_btn_home", "obj117", "fifth_canvas_circle", "fifth_panel_info", "fifth_name", "fifth_key_sig", "obj118", "fifth_scale", "fifth_canvas_piano", "obj119", "fifth_dominant", "obj120", "fifth_parallel", "obj121", "fifth_subdominant", "fifth_led_ai", "obj122", "obj123", "obj124", "piano_btn_home", "piano_btn_rec", "obj125", "piano_btn_set", "obj126", "piano_panel_m", "piano_pad0", "piano_pad1", "piano_pad2", "piano_pad3", "piano_pad4", "piano_pad5", "piano_pad6", "piano_pad7", "piano_pad8", "piano_pad9", "piano_pad10", "piano_pad11", "piano_pad12", "piano_pad13", "piano_pad14", "piano_panel_v", "obj127", "piano_root_v", "piano_canvas_key", "piano_set", "piano_set_btn_return", "obj128", "piano_display_type", "obj129", "obj130", "piano_scale_type", "obj131", "obj132", "piano_pitch", "obj133", "obj134", "piano_sound_type", "obj135", "piano_led_ai", "obj136", "obj137", "obj138", "obj139", "obj140", "obj141", "drum_btn_home", "drum_btn_rec", "obj142", "drum_btn_set", "drum_panel_v", "drum_panel_m", "drum_crash_m", "drum_clap_m", "drum_openhht_m", "drum_closedhh_m", "drum_ride_m", "drum_snare_n", "drum_kick_m", "drum_floortom_m", "drum_pad_set", "drum_set_btn_return", "obj143", "drum_display_type", "obj144", "obj145", "drum_sound_type", "obj146", "drum_led_ai", "obj147", "obj148", "obj149", "obj150", "obj151", "obj152", "midi_btn_home", "midi_btn_set", "midi_panel_mid_list", "midi_list_music_file", "midi_file_example", "midi_panel_hmr_list", "midi_list_record_file", "obj153", "obj154", "midi_music_name_label", "obj155", "midi_music_path_label", "obj156", "midi_music_bpm_num", "obj157", "midi_music_track_count", "midi_prev", "midi_play_stop", "midi_play_stop_label", "midi_next", "obj158", "midi_progress", "midi_play_time_now", "midi_play_time_total", "midi_set", "midi_set_btn_return", "obj159", "midi_play_type", "obj160", "midi_led_ai", "obj161", "obj162", "obj163", "obj164", "obj165", "obj166", "obj167", "obj168", "obj169", "obj170", "obj171", "obj172", "xy_btn_home", "xy_btn_rec", "obj173", "xy_btn_set", "xy_point_1", "xy_point_2", "xy_point_3", "xy_set", "xy_set_btn_return", "obj174", "xy_sound", "obj175", "obj176", "xy_step", "obj177", "xy_led_ai", "obj178", "obj179", "obj180", "obj181", "obj182", "obj183", "metron_btn_home", "metron_btn_set", "metron_panel", "metron_btn_minus", "metron_label_bpm", "obj184", "metron_btn_plus", "obj185", "metron_led_heavy", "metron_led_1", "metron_led_2", "metron_led_3", "metron_led_4", "metron_led_5", "metron_led_6", "metron_led_7", "metron_led_8", "metron_led_9", "metron_led_10", "metron_led_11", "metron_led_12", "metron_led_13", "metron_led_14", "metron_led_15", "obj186", "metron_timesig_top", "obj187", "metron_label_timesig", "metron_timesig_bot", "obj188", "metron_btn_tempo", "metron_slider_bpm", "metron_btn_play_stop", "metron_btn_play_stop_label", "metron_set", "metron_set_btn_return", "obj189", "metron_sound", "obj190", "metron_led_ai", "obj191", "obj192", "obj193", "obj194", "obj195", "ai_btn_home", "obj196", "obj197", "ai_context_user0", "ai_context_user0_text", "ai_context_ai0", "obj198", "ai_context_ai0_text", "ai_btn_set", "ai_btn_speak", "ai_set", "ai_set_btn_return", "obj199", "ai_switch_save_text", "obj200", "ai_switch_wake_anywhere", "obj201", "ai_btn_config_reset", "obj202", "obj203", "obj204", "obj205", "obj206", "obj207", "obj208", "clock_btn_home", "clock_btn_set", "clock_panel_clock", "obj209", "clock_clock_12h_label", "clock_clock_bigtime", "clock_clock_date", "clock_clock_lunar", "clock_clock_hitokoto", "clock_panel_weather", "obj210", "obj211", "weather_today_text", "weather_today_text_1", "obj212", "weather_location", "obj213", "weather_temp", "weather_humi", "obj214", "weather_day2", "weather_day2_1", "obj215", "weather_day3", "weather_day3_1", "weatehr_panel_news", "clock_panel_calender", "clock_calender", "obj216", "obj217", "clock_calender_today", "clock_calender_today_week", "clock_calender_progress_bar", "clock_calender_progress_text", "clock_calender_panel_huangli", "clock_calender_huangli_1", "clock_calender_huangli_2", "clock_calender_huangli_3", "clock_calender_huangli_4", "clock_calender_huangli_5", "clock_calender_huangli_6", "clock_panel_timer", "obj218", "clock_timer_bell", "clock_timer_pv", "obj219", "clock_timer_btn_min_1min", "clock_timer_sv", "clock_timer_btn_add_1min", "obj220", "obj221", "clock_timer_quick_1min", "clock_timer_quick_3min", "clock_timer_quick_5min", "clock_timer_quick_10min", "clock_timer_quick_20min", "clock_timer_quick_30min", "clock_timer_quick_40min", "clock_timer_quick_50min", "clock_timer_quick_60min", "clock_timer_reset", "clock_timer_start_pause", "clock_set", "clock_set_btn_return", "obj222", "clock_set_12_24h", "obj223", "obj224", "clock_set_time_font", "obj225", "clock_btn_clock", "clock_btn_calender", "clock_btn_timer", "clock_led_ai", "obj226", "obj227", "obj228", "obj229", "obj230", "obj231", "obj232", "fun_btn_home", "fun_btn_set", "fun_panel_book", "obj233", "obj234", "obj235", "obj236", "panel_book_open", "book_answer_text", "obj237", "book_answer_detail", "panel_book_close", "obj238", "obj239", "obj240", "fun_panel_tarot", "obj241", "obj242", "obj243", "obj244", "obj245", "panel_tarot_open_1", "tarot_card_1", "obj246", "tarot_card_reversed_1", "tarot_card_detail_panel_1", "tarot_card_detail_text_1", "panel_tarot_close_1", "obj247", "panel_tarot_open_2", "obj248", "tarot_card_2", "tarot_card_reversed_2", "tarot_card_detail_panel_2", "tarot_card_detail_text_2", "panel_tarot_close_2", "obj249", "panel_tarot_open_3", "obj250", "tarot_card_3", "tarot_card_reversed_3", "tarot_card_detail_panel_3", "tarot_card_detail_text_3", "panel_tarot_close_3", "fun_tip_label", "fun_set", "fun_btn_book", "fun_btn_tarot", "fun_led_ai", "obj251", "obj252", "obj253", "obj254", "ftp_btn_back2setting", "obj255", "obj256", "ftp_hammy", "obj257", "obj258", "ftp_label_state", "obj259", "obj260", "ftp_label_ip", "obj261", "obj262", "ftp_label_file", "obj263", "ftp_bar_progress", "obj264", "obj265" };

// Global state variables

static lv_anim_t anim;
static bool anim_initialized;

//
// Helper functions
//

lv_anim_t *get_anim() {
    if (!anim_initialized) {
        lv_anim_init(&anim);
        lv_anim_set_delay(&anim, 200);
        anim_initialized = true;
    }
    return &anim;
}

//
// Event handlers
//

lv_obj_t *tick_value_change_obj;

static void event_handler_cb_onboard_step_ob_slide_brightness(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    void *flowState = lv_event_get_user_data(e);
    (void)flowState;
    
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target_obj(e);
        if (tick_value_change_obj != ta) {
            int32_t value = lv_slider_get_value(ta);
            assignIntegerProperty(flowState, 40, 3, value, "Failed to assign Value in Slider widget");
        }
    }
}

static void event_handler_cb_onboard_step_ob_slide_volume(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    void *flowState = lv_event_get_user_data(e);
    (void)flowState;
    
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target_obj(e);
        if (tick_value_change_obj != ta) {
            int32_t value = lv_slider_get_value(ta);
            assignIntegerProperty(flowState, 42, 3, value, "Failed to assign Value in Slider widget");
        }
    }
}

static void event_handler_cb_setting_setting_slide_brightness(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    void *flowState = lv_event_get_user_data(e);
    (void)flowState;
    
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target_obj(e);
        if (tick_value_change_obj != ta) {
            int32_t value = lv_slider_get_value(ta);
            assignIntegerProperty(flowState, 17, 3, value, "Failed to assign Value in Slider widget");
        }
    }
}

static void event_handler_cb_setting_setting_slide_volume(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    void *flowState = lv_event_get_user_data(e);
    (void)flowState;
    
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target_obj(e);
        if (tick_value_change_obj != ta) {
            int32_t value = lv_slider_get_value(ta);
            assignIntegerProperty(flowState, 21, 3, value, "Failed to assign Value in Slider widget");
        }
    }
}

//
// Screens
//

void create_screen_boot() {
    void *flowState = getFlowState(0, 0);
    (void)flowState;
    lv_obj_t *obj = lv_obj_create(0);
    objects.boot = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            // boot_hammy
            lv_obj_t *obj = lv_image_create(parent_obj);
            objects.boot_hammy = obj;
            lv_obj_set_pos(obj, 540, 201);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_image_set_src(obj, "/sys/src/ui_image_sleepy.bin");
            lv_image_set_scale(obj, 400);
        }
        {
            // boot_label_name_en
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.boot_label_name_en = obj;
            lv_obj_set_pos(obj, 523, 449);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "TAB5 Music Box");
        }
        {
            // boot_percent
            lv_obj_t *obj = lv_bar_create(parent_obj);
            objects.boot_percent = obj;
            lv_obj_set_pos(obj, 128, 530);
            lv_obj_set_size(obj, LV_PCT(80), 30);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_anim(obj, get_anim(), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // boot_label_loading
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.boot_label_loading = obj;
            lv_obj_set_pos(obj, 128, 573);
            lv_obj_set_size(obj, LV_PCT(80), LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "喵喵在睡梦中吃瓜子...");
        }
        {
            // boot_version
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.boot_version = obj;
            lv_obj_set_pos(obj, 0, 688);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // boot_label_name
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.boot_label_name = obj;
            lv_obj_set_pos(obj, 525, 382);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "TAB5 音乐盒");
        }
    }
    
    tick_screen_boot();
}

void delete_screen_boot() {
    lv_obj_delete(objects.boot);
    objects.boot = 0;
    objects.boot_hammy = 0;
    objects.boot_label_name_en = 0;
    objects.boot_percent = 0;
    objects.boot_label_loading = 0;
    objects.boot_version = 0;
    objects.boot_label_name = 0;
    deletePageFlowState(0);
}

void tick_screen_boot() {
    void *flowState = getFlowState(0, 0);
    (void)flowState;
    {
        int32_t new_val = evalIntegerProperty(flowState, 3, 3, "Failed to evaluate Value in Bar widget");
        int32_t cur_val = lv_bar_get_value(objects.boot_percent);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.boot_percent;
            lv_bar_set_value(objects.boot_percent, new_val, LV_ANIM_ON);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 5, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.boot_version);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.boot_version;
            lv_label_set_text(objects.boot_version, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

void create_screen_onboard_step() {
    void *flowState = getFlowState(0, 1);
    (void)flowState;
    lv_obj_t *obj = lv_obj_create(0);
    objects.onboard_step = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.obj0 = obj;
            lv_obj_set_pos(obj, LV_PCT(3), LV_PCT(85));
            lv_obj_set_size(obj, LV_PCT(95), LV_PCT(12));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_radius(obj, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(3), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            {
                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // ob_step_prev
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.ob_step_prev = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(20), LV_PCT(100));
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "上一步");
                        }
                    }
                }
                {
                    // ob_step_next
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.ob_step_next = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(20), LV_PCT(100));
                    lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_END, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "下一步");
                        }
                    }
                }
            }
        }
        {
            // step01_welcome
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.step01_welcome = obj;
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, LV_PCT(100), LV_PCT(85));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // ob_hammy
                    lv_obj_t *obj = lv_image_create(parent_obj);
                    objects.ob_hammy = obj;
                    lv_obj_set_pos(obj, 570, 164);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_image_set_src(obj, "/sys/src/ui_image_cheer.bin");
                    lv_image_set_scale(obj, 350);
                }
                {
                    // ob_str1
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.ob_str1 = obj;
                    lv_obj_set_pos(obj, 0, 350);
                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "欢迎来到Tab5 Music Box");
                }
                {
                    // ob_str2
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.ob_str2 = obj;
                    lv_obj_set_pos(obj, 0, 444);
                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "我是喵喵，你的音乐小伙伴");
                }
                {
                    // ob_str3
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.ob_str3 = obj;
                    lv_obj_set_pos(obj, 0, 483);
                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "让我们一起完成几个简单的设置，开启音乐之旅...");
                }
            }
        }
        {
            // step02_datetime
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.step02_datetime = obj;
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, LV_PCT(100), LV_PCT(85));
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj1 = obj;
                    lv_obj_set_pos(obj, 549, 10);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "步骤：2/5");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj2 = obj;
                    lv_obj_set_pos(obj, 420, 45);
                    lv_obj_set_size(obj, 400, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "设置日期和时间");
                }
                {
                    // label_loading_2
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.label_loading_2 = obj;
                    lv_obj_set_pos(obj, 124, 90);
                    lv_obj_set_size(obj, LV_PCT(80), LV_SIZE_CONTENT);
                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "喵喵需要知道正确的时间才能准确陪伴你的练习哦...");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj3 = obj;
                    lv_obj_set_pos(obj, 632, 182);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "小时");
                }
                {
                    // ob_set_hour
                    lv_obj_t *obj = lv_roller_create(parent_obj);
                    objects.ob_set_hour = obj;
                    lv_obj_set_pos(obj, 625, 234);
                    lv_obj_set_size(obj, 80, 207);
                    lv_roller_set_options(obj, "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23", LV_ROLLER_MODE_INFINITE);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj4 = obj;
                    lv_obj_set_pos(obj, 759, 182);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "分钟");
                }
                {
                    // ob_set_minute
                    lv_obj_t *obj = lv_roller_create(parent_obj);
                    objects.ob_set_minute = obj;
                    lv_obj_set_pos(obj, 749, 234);
                    lv_obj_set_size(obj, 80, 204);
                    lv_roller_set_options(obj, "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59", LV_ROLLER_MODE_INFINITE);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj5 = obj;
                    lv_obj_set_pos(obj, 307, 495);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "当前选择：");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj6 = obj;
                    lv_obj_set_pos(obj, 897, 182);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "秒");
                }
                {
                    // ob_set_second
                    lv_obj_t *obj = lv_roller_create(parent_obj);
                    objects.ob_set_second = obj;
                    lv_obj_set_pos(obj, 872, 234);
                    lv_obj_set_size(obj, 80, 204);
                    lv_roller_set_options(obj, "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59", LV_ROLLER_MODE_INFINITE);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // ob_set_time_result
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.ob_set_time_result = obj;
                    lv_obj_set_pos(obj, 479, 495);
                    lv_obj_set_size(obj, 327, LV_SIZE_CONTENT);
                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "0");
                }
                {
                    // ob_set_time_save
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.ob_set_time_save = obj;
                    lv_obj_set_pos(obj, 832, 471);
                    lv_obj_set_size(obj, 120, 80);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj7 = obj;
                            lv_obj_set_pos(obj, 2, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "保存");
                        }
                    }
                }
                {
                    // ob_set_year
                    lv_obj_t *obj = lv_dropdown_create(parent_obj);
                    objects.ob_set_year = obj;
                    lv_obj_set_pos(obj, 212, 182);
                    lv_obj_set_size(obj, 200, LV_SIZE_CONTENT);
                    lv_dropdown_set_options_static(obj, "2026\n2027\n2028\n2029\n2030\n2031\n2032\n2033\n2034\n2035");
                    lv_dropdown_set_dir(obj, LV_DIR_LEFT);
                    lv_dropdown_set_symbol(obj, LV_SYMBOL_LEFT);
                    lv_dropdown_set_selected(obj, 0);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                            objects.obj8 = obj;
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj9 = obj;
                    lv_obj_set_pos(obj, 442, 195);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "年");
                }
                {
                    // ob_set_month
                    lv_obj_t *obj = lv_dropdown_create(parent_obj);
                    objects.ob_set_month = obj;
                    lv_obj_set_pos(obj, 212, 282);
                    lv_obj_set_size(obj, 200, LV_SIZE_CONTENT);
                    lv_dropdown_set_options_static(obj, "01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12");
                    lv_dropdown_set_dir(obj, LV_DIR_LEFT);
                    lv_dropdown_set_symbol(obj, LV_SYMBOL_LEFT);
                    lv_dropdown_set_selected(obj, 0);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                            objects.obj10 = obj;
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj11 = obj;
                    lv_obj_set_pos(obj, 442, 295);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "月");
                }
                {
                    // ob_set_day
                    lv_obj_t *obj = lv_dropdown_create(parent_obj);
                    objects.ob_set_day = obj;
                    lv_obj_set_pos(obj, 212, 381);
                    lv_obj_set_size(obj, 200, LV_SIZE_CONTENT);
                    lv_dropdown_set_options_static(obj, "01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31");
                    lv_dropdown_set_dir(obj, LV_DIR_LEFT);
                    lv_dropdown_set_symbol(obj, LV_SYMBOL_LEFT);
                    lv_dropdown_set_selected(obj, 0);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                            objects.obj12 = obj;
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj13 = obj;
                    lv_obj_set_pos(obj, 442, 394);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "日");
                }
            }
        }
        {
            // step03_bg_vol
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.step03_bg_vol = obj;
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, LV_PCT(100), LV_PCT(85));
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj14 = obj;
                    lv_obj_set_pos(obj, 582, 10);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "步骤：3/5");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj15 = obj;
                    lv_obj_set_pos(obj, 420, 45);
                    lv_obj_set_size(obj, 400, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "设置亮度和音量");
                }
                {
                    // label_loading_3
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.label_loading_3 = obj;
                    lv_obj_set_pos(obj, 124, 90);
                    lv_obj_set_size(obj, LV_PCT(80), LV_SIZE_CONTENT);
                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "让喵喵的表现更适合你的眼睛和耳朵...");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj16 = obj;
                    lv_obj_set_pos(obj, 94, 220);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "屏幕亮度：");
                }
                {
                    // ob_slide_brightness
                    lv_obj_t *obj = lv_slider_create(parent_obj);
                    objects.ob_slide_brightness = obj;
                    lv_obj_set_pos(obj, 93, 260);
                    lv_obj_set_size(obj, LV_PCT(85), 30);
                    lv_slider_set_range(obj, 1, 100);
                    lv_obj_add_event_cb(obj, event_handler_cb_onboard_step_ob_slide_brightness, LV_EVENT_ALL, flowState);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 2, LV_PART_KNOB | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_KNOB | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0xffffff), LV_PART_KNOB | LV_STATE_DEFAULT);
                }
                {
                    // ob_slide_brightness_num
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.ob_slide_brightness_num = obj;
                    lv_obj_set_pos(obj, 1041, 220);
                    lv_obj_set_size(obj, 100, LV_SIZE_CONTENT);
                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj17 = obj;
                    lv_obj_set_pos(obj, 93, 320);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "主音量：");
                }
                {
                    // ob_slide_volume
                    lv_obj_t *obj = lv_slider_create(parent_obj);
                    objects.ob_slide_volume = obj;
                    lv_obj_set_pos(obj, 93, 360);
                    lv_obj_set_size(obj, LV_PCT(85), 30);
                    lv_obj_add_event_cb(obj, event_handler_cb_onboard_step_ob_slide_volume, LV_EVENT_ALL, flowState);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_KNOB | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 2, LV_PART_KNOB | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0xffffff), LV_PART_KNOB | LV_STATE_DEFAULT);
                }
                {
                    // ob_slide_volume_num
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.ob_slide_volume_num = obj;
                    lv_obj_set_pos(obj, 1041, 320);
                    lv_obj_set_size(obj, 100, LV_SIZE_CONTENT);
                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    // ob_key_try_volume
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.ob_key_try_volume = obj;
                    lv_obj_set_pos(obj, 540, 433);
                    lv_obj_set_size(obj, 160, 100);
                    lv_obj_set_style_text_font(obj, ui_font_icon_70, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj18 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "");
                        }
                    }
                }
            }
        }
        {
            // step04_feature
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.step04_feature = obj;
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, LV_PCT(100), LV_PCT(85));
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj19 = obj;
                    lv_obj_set_pos(obj, 549, 10);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "步骤：4/5");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj20 = obj;
                    lv_obj_set_pos(obj, 248, 45);
                    lv_obj_set_size(obj, LV_PCT(60), LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "个性化你的Music Box");
                }
                {
                    // label_loading_5
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.label_loading_5 = obj;
                    lv_obj_set_pos(obj, 124, 90);
                    lv_obj_set_size(obj, LV_PCT(80), LV_SIZE_CONTENT);
                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "让喵喵记住你的偏好设置");
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj21 = obj;
                    lv_obj_set_pos(obj, 124, 194);
                    lv_obj_set_size(obj, LV_PCT(80), 60);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "主题：");
                        }
                        {
                            // ob_setting_theme
                            lv_obj_t *obj = lv_dropdown_create(parent_obj);
                            objects.ob_setting_theme = obj;
                            lv_obj_set_pos(obj, 994, 360);
                            lv_obj_set_size(obj, LV_PCT(20), 50);
                            lv_dropdown_set_options_static(obj, "金丝熊\n星空黑");
                            lv_dropdown_set_selected(obj, 0);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                                    objects.obj22 = obj;
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                                }
                            }
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj23 = obj;
                    lv_obj_set_pos(obj, 124, 281);
                    lv_obj_set_size(obj, LV_PCT(80), 60);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "开机页面：");
                        }
                        {
                            // ob_setting_on_screen
                            lv_obj_t *obj = lv_dropdown_create(parent_obj);
                            objects.ob_setting_on_screen = obj;
                            lv_obj_set_pos(obj, 994, 360);
                            lv_obj_set_size(obj, LV_PCT(20), 50);
                            lv_dropdown_set_options_static(obj, "主选单\n时钟\n小钢琴\nAI导师\n禅模式");
                            lv_dropdown_set_selected(obj, 0);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                                    objects.obj24 = obj;
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                }
                            }
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj25 = obj;
                    lv_obj_set_pos(obj, 124, 368);
                    lv_obj_set_size(obj, LV_PCT(80), 60);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "自动熄屏时间：");
                        }
                        {
                            // ob_setting_time2idle
                            lv_obj_t *obj = lv_dropdown_create(parent_obj);
                            objects.ob_setting_time2idle = obj;
                            lv_obj_set_pos(obj, 994, 360);
                            lv_obj_set_size(obj, LV_PCT(20), 50);
                            lv_dropdown_set_options_static(obj, "15秒\n30秒\n1分钟\n3分钟\n5分钟\n10分钟");
                            lv_dropdown_set_selected(obj, 0);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                                    objects.obj26 = obj;
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                }
                            }
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj27 = obj;
                    lv_obj_set_pos(obj, 124, 455);
                    lv_obj_set_size(obj, LV_PCT(80), 60);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(2), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "自动关机（熄屏后5min触发）：");
                        }
                        {
                            // ob_setting_auto_sleep
                            lv_obj_t *obj = lv_switch_create(parent_obj);
                            objects.ob_setting_auto_sleep = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 100, 45);
                            lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_INDICATOR | LV_STATE_CHECKED);
                        }
                    }
                }
            }
        }
        {
            // step05_online
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.step05_online = obj;
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, LV_PCT(100), LV_PCT(85));
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj28 = obj;
                    lv_obj_set_pos(obj, 549, 10);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "步骤：5/5");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj29 = obj;
                    lv_obj_set_pos(obj, 420, 45);
                    lv_obj_set_size(obj, 400, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "设置网络连接");
                }
                {
                    // label_loading_4
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.label_loading_4 = obj;
                    lv_obj_set_pos(obj, 124, 90);
                    lv_obj_set_size(obj, LV_PCT(80), LV_SIZE_CONTENT);
                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "联网后喵喵会自动帮你调整时间，还可以使用AI超强Buff哦...");
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj30 = obj;
                    lv_obj_set_pos(obj, 125, 140);
                    lv_obj_set_size(obj, LV_PCT(80), 60);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(2), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "Wifi总开关：");
                        }
                        {
                            // ob_setting_wifi_switch
                            lv_obj_t *obj = lv_switch_create(parent_obj);
                            objects.ob_setting_wifi_switch = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 100, 45);
                            lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_INDICATOR | LV_STATE_CHECKED);
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_image_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 220);
                    lv_obj_set_size(obj, LV_PCT(100), 300);
                    lv_image_set_src(obj, "/sys/src/ui_image_wifi_qrcode.bin");
                    lv_image_set_scale(obj, 255);
                }
                {
                    // ob_set_wifi_connect_tip
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.ob_set_wifi_connect_tip = obj;
                    lv_obj_set_pos(obj, 1, 520);
                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "string_wifi_connect_tip");
                }
            }
        }
        {
            // step06_finish_reboot
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.step06_finish_reboot = obj;
            lv_obj_set_pos(obj, 0, 0);
            lv_obj_set_size(obj, LV_PCT(100), LV_PCT(85));
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj31 = obj;
                    lv_obj_set_pos(obj, 420, 150);
                    lv_obj_set_size(obj, 400, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "设置完成啦");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj32 = obj;
                    lv_obj_set_pos(obj, 0, 500);
                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "我们可以正式开启音乐旅途啦");
                }
                {
                    lv_obj_t *obj = lv_image_create(parent_obj);
                    lv_obj_set_pos(obj, 570, 300);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_image_set_src(obj, "/sys/src/ui_image_cheer.bin");
                    lv_image_set_scale(obj, 350);
                }
            }
        }
    }
    
    tick_screen_onboard_step();
}

void delete_screen_onboard_step() {
    lv_obj_delete(objects.onboard_step);
    objects.onboard_step = 0;
    objects.obj0 = 0;
    objects.ob_step_prev = 0;
    objects.ob_step_next = 0;
    objects.step01_welcome = 0;
    objects.ob_hammy = 0;
    objects.ob_str1 = 0;
    objects.ob_str2 = 0;
    objects.ob_str3 = 0;
    objects.step02_datetime = 0;
    objects.obj1 = 0;
    objects.obj2 = 0;
    objects.label_loading_2 = 0;
    objects.obj3 = 0;
    objects.ob_set_hour = 0;
    objects.obj4 = 0;
    objects.ob_set_minute = 0;
    objects.obj5 = 0;
    objects.obj6 = 0;
    objects.ob_set_second = 0;
    objects.ob_set_time_result = 0;
    objects.ob_set_time_save = 0;
    objects.obj7 = 0;
    objects.ob_set_year = 0;
    objects.obj8 = 0;
    objects.obj9 = 0;
    objects.ob_set_month = 0;
    objects.obj10 = 0;
    objects.obj11 = 0;
    objects.ob_set_day = 0;
    objects.obj12 = 0;
    objects.obj13 = 0;
    objects.step03_bg_vol = 0;
    objects.obj14 = 0;
    objects.obj15 = 0;
    objects.label_loading_3 = 0;
    objects.obj16 = 0;
    objects.ob_slide_brightness = 0;
    objects.ob_slide_brightness_num = 0;
    objects.obj17 = 0;
    objects.ob_slide_volume = 0;
    objects.ob_slide_volume_num = 0;
    objects.ob_key_try_volume = 0;
    objects.obj18 = 0;
    objects.step04_feature = 0;
    objects.obj19 = 0;
    objects.obj20 = 0;
    objects.label_loading_5 = 0;
    objects.obj21 = 0;
    objects.ob_setting_theme = 0;
    objects.obj22 = 0;
    objects.obj23 = 0;
    objects.ob_setting_on_screen = 0;
    objects.obj24 = 0;
    objects.obj25 = 0;
    objects.ob_setting_time2idle = 0;
    objects.obj26 = 0;
    objects.obj27 = 0;
    objects.ob_setting_auto_sleep = 0;
    objects.step05_online = 0;
    objects.obj28 = 0;
    objects.obj29 = 0;
    objects.label_loading_4 = 0;
    objects.obj30 = 0;
    objects.ob_setting_wifi_switch = 0;
    objects.ob_set_wifi_connect_tip = 0;
    objects.step06_finish_reboot = 0;
    objects.obj31 = 0;
    objects.obj32 = 0;
    deletePageFlowState(1);
}

void tick_screen_onboard_step() {
    void *flowState = getFlowState(0, 1);
    (void)flowState;
    {
        int32_t new_val = evalIntegerProperty(flowState, 40, 3, "Failed to evaluate Value in Slider widget");
        int32_t cur_val = lv_slider_get_value(objects.ob_slide_brightness);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.ob_slide_brightness;
            lv_slider_set_value(objects.ob_slide_brightness, new_val, LV_ANIM_OFF);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 39, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.ob_slide_brightness_num);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.ob_slide_brightness_num;
            lv_label_set_text(objects.ob_slide_brightness_num, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = evalIntegerProperty(flowState, 42, 3, "Failed to evaluate Value in Slider widget");
        int32_t cur_val = lv_slider_get_value(objects.ob_slide_volume);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.ob_slide_volume;
            lv_slider_set_value(objects.ob_slide_volume, new_val, LV_ANIM_OFF);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 43, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.ob_slide_volume_num);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.ob_slide_volume_num;
            lv_label_set_text(objects.ob_slide_volume_num, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

void create_screen_launcher() {
    void *flowState = getFlowState(0, 2);
    (void)flowState;
    lv_obj_t *obj = lv_obj_create(0);
    objects.launcher = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_src(obj, "/sys/src/ui_image_bg_night.bin", LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj33 = obj;
            lv_obj_set_pos(obj, 440, 10);
            lv_obj_set_size(obj, 400, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, _("主菜单"));
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj47 = obj;
            lv_obj_set_pos(obj, LV_PCT(50), 10);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj48 = obj;
            lv_obj_set_pos(obj, 10, 10);
            lv_obj_set_size(obj, 200, LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.obj34 = obj;
            lv_obj_set_pos(obj, 32, 110);
            lv_obj_set_size(obj, LV_PCT(95), LV_PCT(80));
            lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), 0, LV_GRID_TEMPLATE_LAST};
                lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            lv_obj_set_style_pad_top(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_column(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_row(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // launcher_zen
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.launcher_zen = obj;
                    lv_obj_set_pos(obj, 29, 32);
                    lv_obj_set_size(obj, LV_PCT(22), LV_PCT(28));
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][10]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_x(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_y(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_darken(lv_color_hex(0xffffff), 64), LV_PART_MAIN | LV_STATE_PRESSED);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj35 = obj;
                            lv_obj_set_pos(obj, 12, LV_PCT(0));
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_obj_set_style_text_font(obj, ui_font_icon_70, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][10]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 12, LV_PCT(80));
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_label_set_text_static(obj, "禅模式");
                        }
                    }
                }
                {
                    // launcher_ear
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.launcher_ear = obj;
                    lv_obj_set_pos(obj, 637, 32);
                    lv_obj_set_size(obj, LV_PCT(22), LV_PCT(28));
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][11]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_x(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_y(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_darken(lv_color_hex(0xffffff), 64), LV_PART_MAIN | LV_STATE_PRESSED);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 12, LV_PCT(80));
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_label_set_text_static(obj, "练耳");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj36 = obj;
                            lv_obj_set_pos(obj, 12, LV_PCT(0));
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_obj_set_style_text_font(obj, ui_font_icon_70, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][11]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "");
                        }
                    }
                }
                {
                    // launcher_fifth
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.launcher_fifth = obj;
                    lv_obj_set_pos(obj, 29, 215);
                    lv_obj_set_size(obj, LV_PCT(22), LV_PCT(28));
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][12]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_x(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_y(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_darken(lv_color_hex(0xffffff), 64), LV_PART_MAIN | LV_STATE_PRESSED);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 12, LV_PCT(80));
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_label_set_text_static(obj, "五度圈");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj37 = obj;
                            lv_obj_set_pos(obj, 12, LV_PCT(0));
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_obj_set_style_text_font(obj, ui_font_icon_70, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][12]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "");
                        }
                    }
                }
                {
                    // launcher_chord
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.launcher_chord = obj;
                    lv_obj_set_pos(obj, 333, 215);
                    lv_obj_set_size(obj, LV_PCT(22), LV_PCT(28));
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][12]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_x(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_y(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_darken(lv_color_hex(0xffffff), 64), LV_PART_MAIN | LV_STATE_PRESSED);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 12, LV_PCT(80));
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_label_set_text_static(obj, "和弦练习");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj38 = obj;
                            lv_obj_set_pos(obj, 12, LV_PCT(0));
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_obj_set_style_text_font(obj, ui_font_icon_70, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][12]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "");
                        }
                    }
                }
                {
                    // launcher_midi
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.launcher_midi = obj;
                    lv_obj_set_pos(obj, 637, 215);
                    lv_obj_set_size(obj, LV_PCT(22), LV_PCT(28));
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][13]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_x(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_y(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_darken(lv_color_hex(0xffffff), 64), LV_PART_MAIN | LV_STATE_PRESSED);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 12, LV_PCT(80));
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_label_set_text_static(obj, "Midi");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj39 = obj;
                            lv_obj_set_pos(obj, 12, LV_PCT(0));
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_obj_set_style_text_font(obj, ui_font_icon_70, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][13]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "");
                        }
                    }
                }
                {
                    // launcher_xy
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.launcher_xy = obj;
                    lv_obj_set_pos(obj, 101, 46);
                    lv_obj_set_size(obj, LV_PCT(22), LV_PCT(28));
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][13]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_x(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_y(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_darken(lv_color_hex(0xffffff), 64), LV_PART_MAIN | LV_STATE_PRESSED);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 12, LV_PCT(80));
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_label_set_text_static(obj, "X-Y");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj40 = obj;
                            lv_obj_set_pos(obj, 12, LV_PCT(0));
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_obj_set_style_text_font(obj, ui_font_icon_70, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][13]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "");
                        }
                    }
                }
                {
                    // launcher_drum
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.launcher_drum = obj;
                    lv_obj_set_pos(obj, 941, 215);
                    lv_obj_set_size(obj, LV_PCT(22), LV_PCT(28));
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][13]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_x(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_y(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_darken(lv_color_hex(0xffffff), 64), LV_PART_MAIN | LV_STATE_PRESSED);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 12, LV_PCT(80));
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_label_set_text_static(obj, "组鼓");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj41 = obj;
                            lv_obj_set_pos(obj, 12, LV_PCT(0));
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_obj_set_style_text_font(obj, ui_font_icon_70, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][13]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "");
                        }
                    }
                }
                {
                    // launcher_piano
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.launcher_piano = obj;
                    lv_obj_set_pos(obj, 29, 398);
                    lv_obj_set_size(obj, LV_PCT(22), LV_PCT(28));
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][13]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_x(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_y(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_darken(lv_color_hex(0xffffff), 64), LV_PART_MAIN | LV_STATE_PRESSED);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 12, LV_PCT(80));
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_label_set_text_static(obj, "小钢琴");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj42 = obj;
                            lv_obj_set_pos(obj, 12, LV_PCT(0));
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_obj_set_style_text_font(obj, ui_font_icon_70, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][13]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "");
                        }
                    }
                }
                {
                    // launcher_clock
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.launcher_clock = obj;
                    lv_obj_set_pos(obj, 637, 398);
                    lv_obj_set_size(obj, LV_PCT(22), LV_PCT(28));
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][15]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_x(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_y(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_darken(lv_color_hex(0xffffff), 64), LV_PART_MAIN | LV_STATE_PRESSED);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 12, LV_PCT(80));
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_label_set_text_static(obj, "时钟日历");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj43 = obj;
                            lv_obj_set_pos(obj, 12, LV_PCT(0));
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_obj_set_style_text_font(obj, ui_font_icon_70, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][15]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "");
                        }
                    }
                }
                {
                    // launcher_ai
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.launcher_ai = obj;
                    lv_obj_set_pos(obj, 941, 398);
                    lv_obj_set_size(obj, LV_PCT(22), LV_PCT(28));
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][14]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_x(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_y(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_darken(lv_color_hex(0xffffff), 64), LV_PART_MAIN | LV_STATE_PRESSED);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 12, LV_PCT(80));
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_label_set_text_static(obj, "AI导师");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj44 = obj;
                            lv_obj_set_pos(obj, 12, LV_PCT(0));
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_obj_set_style_text_font(obj, ui_font_icon_70, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][14]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "");
                        }
                    }
                }
                {
                    // launcher_metron
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.launcher_metron = obj;
                    lv_obj_set_pos(obj, 941, 398);
                    lv_obj_set_size(obj, LV_PCT(22), LV_PCT(28));
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][14]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_x(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_y(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_darken(lv_color_hex(0xffffff), 64), LV_PART_MAIN | LV_STATE_PRESSED);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 12, LV_PCT(80));
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_label_set_text_static(obj, "节拍器");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj45 = obj;
                            lv_obj_set_pos(obj, 12, LV_PCT(0));
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_obj_set_style_text_font(obj, ui_font_icon_70, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][14]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "");
                        }
                    }
                }
                {
                    // launcher_fun
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.launcher_fun = obj;
                    lv_obj_set_pos(obj, 637, 398);
                    lv_obj_set_size(obj, LV_PCT(22), LV_PCT(28));
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][15]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_x(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_y(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_darken(lv_color_hex(0xffffff), 64), LV_PART_MAIN | LV_STATE_PRESSED);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 12, LV_PCT(80));
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_label_set_text_static(obj, "答案之书");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj46 = obj;
                            lv_obj_set_pos(obj, 12, LV_PCT(0));
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_obj_set_style_text_font(obj, ui_font_icon_70, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][15]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "");
                        }
                    }
                }
            }
        }
        {
            // launcher_btn_setting
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.launcher_btn_setting = obj;
            lv_obj_set_pos(obj, 1150, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
            }
        }
        {
            // launcher_led_ai
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.launcher_led_ai = obj;
            lv_obj_set_pos(obj, 626, 670);
            lv_obj_set_size(obj, 40, 35);
            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]));
            lv_led_set_brightness(obj, 255);
        }
    }
    
    tick_screen_launcher();
}

void delete_screen_launcher() {
    lv_obj_delete(objects.launcher);
    objects.launcher = 0;
    objects.obj33 = 0;
    objects.obj47 = 0;
    objects.obj48 = 0;
    objects.obj34 = 0;
    objects.launcher_zen = 0;
    objects.obj35 = 0;
    objects.launcher_ear = 0;
    objects.obj36 = 0;
    objects.launcher_fifth = 0;
    objects.obj37 = 0;
    objects.launcher_chord = 0;
    objects.obj38 = 0;
    objects.launcher_midi = 0;
    objects.obj39 = 0;
    objects.launcher_xy = 0;
    objects.obj40 = 0;
    objects.launcher_drum = 0;
    objects.obj41 = 0;
    objects.launcher_piano = 0;
    objects.obj42 = 0;
    objects.launcher_clock = 0;
    objects.obj43 = 0;
    objects.launcher_ai = 0;
    objects.obj44 = 0;
    objects.launcher_metron = 0;
    objects.obj45 = 0;
    objects.launcher_fun = 0;
    objects.obj46 = 0;
    objects.launcher_btn_setting = 0;
    objects.launcher_led_ai = 0;
    deletePageFlowState(2);
}

void tick_screen_launcher() {
    void *flowState = getFlowState(0, 2);
    (void)flowState;
    {
        const char *new_val = evalTextProperty(flowState, 2, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj47);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj47;
            lv_label_set_text(objects.obj47, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 3, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj48);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj48;
            lv_label_set_text(objects.obj48, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

void create_screen_setting() {
    void *flowState = getFlowState(0, 3);
    (void)flowState;
    lv_obj_t *obj = lv_obj_create(0);
    objects.setting = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj68 = obj;
            lv_obj_set_pos(obj, 10, 10);
            lv_obj_set_size(obj, 200, LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj49 = obj;
            lv_obj_set_pos(obj, 440, 10);
            lv_obj_set_size(obj, 400, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, _("设置"));
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj69 = obj;
            lv_obj_set_pos(obj, LV_PCT(50), 10);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj50 = obj;
            lv_obj_set_pos(obj, 320, 55);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // setting_btn_home
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.setting_btn_home = obj;
            lv_obj_set_pos(obj, 30, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
            }
        }
        {
            // setting_btn_about
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.setting_btn_about = obj;
            lv_obj_set_pos(obj, 1150, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
            }
        }
        {
            // setting_tab
            lv_obj_t *obj = lv_tabview_create(parent_obj);
            objects.setting_tab = obj;
            lv_obj_set_pos(obj, 32, 110);
            lv_obj_set_size(obj, LV_PCT(95), LV_PCT(81));
            lv_tabview_set_tab_bar_position(obj, LV_DIR_LEFT);
            lv_tabview_set_tab_bar_size(obj, 120);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // setting_tab_basic
                    lv_obj_t *obj = lv_tabview_add_tab(parent_obj, "基础\n设置");
                    objects.setting_tab_basic = obj;
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_obj_create(parent_obj);
                            objects.obj51 = obj;
                            lv_obj_set_pos(obj, 41, 350);
                            lv_obj_set_size(obj, LV_PCT(100), 60);
                            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                            {
                                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                                lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                            {
                                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                                lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                            lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "语言/Language：");
                                }
                                {
                                    // setting_language
                                    lv_obj_t *obj = lv_dropdown_create(parent_obj);
                                    objects.setting_language = obj;
                                    lv_obj_set_pos(obj, 994, 360);
                                    lv_obj_set_size(obj, LV_PCT(20), 50);
                                    lv_dropdown_set_options_static(obj, "简体中文\nEnglish");
                                    lv_dropdown_set_selected(obj, 0);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(0xf0f4ff), LV_PART_SELECTED | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    {
                                        lv_obj_t *parent_obj = obj;
                                        {
                                            lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                                            objects.obj52 = obj;
                                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                                        }
                                    }
                                }
                            }
                        }
                        {
                            lv_obj_t *obj = lv_obj_create(parent_obj);
                            objects.obj53 = obj;
                            lv_obj_set_pos(obj, 40, 190);
                            lv_obj_set_size(obj, LV_PCT(100), 60);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                            lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                                lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                            {
                                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(2), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                                lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "屏幕亮度：");
                                }
                                {
                                    // setting_slide_brightness
                                    lv_obj_t *obj = lv_slider_create(parent_obj);
                                    objects.setting_slide_brightness = obj;
                                    lv_obj_set_pos(obj, 124, 320);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, 30);
                                    lv_slider_set_range(obj, 1, 100);
                                    lv_obj_add_event_cb(obj, event_handler_cb_setting_setting_slide_brightness, LV_EVENT_ALL, flowState);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_column_span(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(0xf8f8f8), LV_PART_KNOB | LV_STATE_DEFAULT);
                                    lv_obj_set_style_border_width(obj, 2, LV_PART_KNOB | LV_STATE_DEFAULT);
                                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_KNOB | LV_STATE_DEFAULT);
                                }
                                {
                                    // setting_slide_brightness_num
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    objects.setting_slide_brightness_num = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(20), LV_SIZE_CONTENT);
                                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text(obj, "");
                                }
                            }
                        }
                        {
                            lv_obj_t *obj = lv_obj_create(parent_obj);
                            objects.obj54 = obj;
                            lv_obj_set_pos(obj, 40, 270);
                            lv_obj_set_size(obj, LV_PCT(100), 60);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                            lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                                lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                            {
                                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(2), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                                lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "主音量：");
                                }
                                {
                                    // setting_slide_volume
                                    lv_obj_t *obj = lv_slider_create(parent_obj);
                                    objects.setting_slide_volume = obj;
                                    lv_obj_set_pos(obj, 124, 320);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, 30);
                                    lv_obj_add_event_cb(obj, event_handler_cb_setting_setting_slide_volume, LV_EVENT_ALL, flowState);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_column_span(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(0xf8f8f8), LV_PART_KNOB | LV_STATE_DEFAULT);
                                    lv_obj_set_style_border_width(obj, 2, LV_PART_KNOB | LV_STATE_DEFAULT);
                                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_KNOB | LV_STATE_DEFAULT);
                                }
                                {
                                    // setting_slide_volume_num
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    objects.setting_slide_volume_num = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(20), LV_SIZE_CONTENT);
                                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text(obj, "");
                                }
                            }
                        }
                        {
                            lv_obj_t *obj = lv_obj_create(parent_obj);
                            objects.obj55 = obj;
                            lv_obj_set_pos(obj, 41, 350);
                            lv_obj_set_size(obj, LV_PCT(100), 60);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                            {
                                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                                lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                            {
                                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                                lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                            lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "主题：");
                                }
                                {
                                    // setting_theme
                                    lv_obj_t *obj = lv_dropdown_create(parent_obj);
                                    objects.setting_theme = obj;
                                    lv_obj_set_pos(obj, 994, 360);
                                    lv_obj_set_size(obj, LV_PCT(20), 50);
                                    lv_dropdown_set_options_static(obj, "金丝熊\n星空黑");
                                    lv_dropdown_set_selected(obj, 0);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    {
                                        lv_obj_t *parent_obj = obj;
                                        {
                                            lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                                            objects.obj56 = obj;
                                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                                        }
                                    }
                                }
                            }
                        }
                        {
                            lv_obj_t *obj = lv_obj_create(parent_obj);
                            objects.obj57 = obj;
                            lv_obj_set_pos(obj, -18, 11);
                            lv_obj_set_size(obj, LV_PCT(100), 60);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                            {
                                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                                lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                            {
                                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                                lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                            lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "开机页面：");
                                }
                                {
                                    // setting_on_screen
                                    lv_obj_t *obj = lv_dropdown_create(parent_obj);
                                    objects.setting_on_screen = obj;
                                    lv_obj_set_pos(obj, 994, 360);
                                    lv_obj_set_size(obj, LV_PCT(20), 50);
                                    lv_dropdown_set_options_static(obj, "主选单\n时钟\n小钢琴\nAI导师\n禅模式");
                                    lv_dropdown_set_selected(obj, 0);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    {
                                        lv_obj_t *parent_obj = obj;
                                        {
                                            lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                                            objects.obj58 = obj;
                                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                        }
                                    }
                                }
                            }
                        }
                        {
                            lv_obj_t *obj = lv_obj_create(parent_obj);
                            objects.obj59 = obj;
                            lv_obj_set_pos(obj, -18, 11);
                            lv_obj_set_size(obj, LV_PCT(100), 60);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                            {
                                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                                lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                            {
                                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                                lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                            lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "自动熄屏时间：");
                                }
                                {
                                    // setting_time2idle
                                    lv_obj_t *obj = lv_dropdown_create(parent_obj);
                                    objects.setting_time2idle = obj;
                                    lv_obj_set_pos(obj, 994, 360);
                                    lv_obj_set_size(obj, LV_PCT(20), 50);
                                    lv_dropdown_set_options_static(obj, "15秒\n30秒\n1分钟\n3分钟\n5分钟\n10分钟");
                                    lv_dropdown_set_selected(obj, 0);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    {
                                        lv_obj_t *parent_obj = obj;
                                        {
                                            lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                                            objects.obj60 = obj;
                                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                        }
                                    }
                                }
                            }
                        }
                        {
                            lv_obj_t *obj = lv_obj_create(parent_obj);
                            objects.obj61 = obj;
                            lv_obj_set_pos(obj, -18, 11);
                            lv_obj_set_size(obj, LV_PCT(100), 60);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                            {
                                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                                lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                            {
                                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(2), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                                lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                            lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "自动休眠（熄屏后5min触发）：");
                                }
                                {
                                    // setting_auto_sleep
                                    lv_obj_t *obj = lv_switch_create(parent_obj);
                                    objects.setting_auto_sleep = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, 100, 45);
                                    lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_INDICATOR | LV_STATE_CHECKED);
                                }
                            }
                        }
                        {
                            lv_obj_t *obj = lv_obj_create(parent_obj);
                            objects.obj62 = obj;
                            lv_obj_set_pos(obj, -18, 11);
                            lv_obj_set_size(obj, LV_PCT(100), 60);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                            {
                                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                                lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                            {
                                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(2), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                                lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                            lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "倒置显示：");
                                }
                                {
                                    // setting_invert_display
                                    lv_obj_t *obj = lv_switch_create(parent_obj);
                                    objects.setting_invert_display = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, 100, 45);
                                    lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_INDICATOR | LV_STATE_CHECKED);
                                }
                            }
                        }
                    }
                }
                {
                    // setting_tab_advanced
                    lv_obj_t *obj = lv_tabview_add_tab(parent_obj, "高级\n设置");
                    objects.setting_tab_advanced = obj;
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_obj_create(parent_obj);
                            objects.obj63 = obj;
                            lv_obj_set_pos(obj, 41, 443);
                            lv_obj_set_size(obj, LV_PCT(100), 60);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                            lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                                lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                            {
                                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(2), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                                lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "网络设置：");
                                }
                                {
                                    // setting_btn_wifi_detail
                                    lv_obj_t *obj = lv_button_create(parent_obj);
                                    objects.setting_btn_wifi_detail = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, 200, 50);
                                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    {
                                        lv_obj_t *parent_obj = obj;
                                        {
                                            lv_obj_t *obj = lv_label_create(parent_obj);
                                            lv_obj_set_pos(obj, 0, 0);
                                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_label_set_text_static(obj, "");
                                        }
                                    }
                                }
                            }
                        }
                        {
                            lv_obj_t *obj = lv_obj_create(parent_obj);
                            objects.obj64 = obj;
                            lv_obj_set_pos(obj, 41, 443);
                            lv_obj_set_size(obj, LV_PCT(100), 60);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                            lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                                lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                            {
                                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(2), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                                lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "局域网FTP文件管理：");
                                }
                                {
                                    // setting_btn_ftp
                                    lv_obj_t *obj = lv_button_create(parent_obj);
                                    objects.setting_btn_ftp = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, 200, 50);
                                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    {
                                        lv_obj_t *parent_obj = obj;
                                        {
                                            lv_obj_t *obj = lv_label_create(parent_obj);
                                            lv_obj_set_pos(obj, 0, 0);
                                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_label_set_text_static(obj, "");
                                        }
                                    }
                                }
                            }
                        }
                        {
                            lv_obj_t *obj = lv_obj_create(parent_obj);
                            objects.obj65 = obj;
                            lv_obj_set_pos(obj, 41, 443);
                            lv_obj_set_size(obj, LV_PCT(100), 60);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                            lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                                lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                            {
                                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(2), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                                lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][8]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "全局重置：");
                                }
                                {
                                    // setting_btn_system_reset
                                    lv_obj_t *obj = lv_button_create(parent_obj);
                                    objects.setting_btn_system_reset = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, 200, 50);
                                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][8]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    {
                                        lv_obj_t *parent_obj = obj;
                                        {
                                            lv_obj_t *obj = lv_label_create(parent_obj);
                                            lv_obj_set_pos(obj, 0, 0);
                                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_label_set_text_static(obj, "长按重置");
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        {
            // wifi_set_panel
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.wifi_set_panel = obj;
            lv_obj_set_pos(obj, 128, 54);
            lv_obj_set_size(obj, LV_PCT(80), LV_PCT(85));
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // wifi_set_panel_return
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.wifi_set_panel_return = obj;
                    lv_obj_set_pos(obj, 50, 60);
                    lv_obj_set_size(obj, 100, 60);
                    lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj70 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj66 = obj;
                    lv_obj_set_pos(obj, 78, 436);
                    lv_obj_set_size(obj, LV_PCT(100), 60);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(2), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "Wifi总开关：");
                        }
                        {
                            // setting_wifi_switch
                            lv_obj_t *obj = lv_switch_create(parent_obj);
                            objects.setting_wifi_switch = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 100, 45);
                            lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_INDICATOR | LV_STATE_CHECKED);
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_image_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(100), 300);
                    lv_image_set_src(obj, "/sys/src/ui_image_wifi_qrcode.bin");
                }
                {
                    // setting_wifi_connect_tip
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.setting_wifi_connect_tip = obj;
                    lv_obj_set_pos(obj, -1, 0);
                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "string_wifi_connect_tip");
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj67 = obj;
                    lv_obj_set_pos(obj, 41, 443);
                    lv_obj_set_size(obj, LV_PCT(100), 60);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][8]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_flex_track_place(obj, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_flex_main_place(obj, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // setting_btn_wifi_reset
                            lv_obj_t *obj = lv_button_create(parent_obj);
                            objects.setting_btn_wifi_reset = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 200, 50);
                            lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][8]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "长按重置");
                                }
                            }
                        }
                    }
                }
            }
        }
        {
            // setting_led_ai
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.setting_led_ai = obj;
            lv_obj_set_pos(obj, 626, 670);
            lv_obj_set_size(obj, 40, 35);
            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]));
            lv_led_set_brightness(obj, 255);
        }
    }
    
    tick_screen_setting();
}

void delete_screen_setting() {
    lv_obj_delete(objects.setting);
    objects.setting = 0;
    objects.obj68 = 0;
    objects.obj49 = 0;
    objects.obj69 = 0;
    objects.obj50 = 0;
    objects.setting_btn_home = 0;
    objects.setting_btn_about = 0;
    objects.setting_tab = 0;
    objects.setting_tab_basic = 0;
    objects.obj51 = 0;
    objects.setting_language = 0;
    objects.obj52 = 0;
    objects.obj53 = 0;
    objects.setting_slide_brightness = 0;
    objects.setting_slide_brightness_num = 0;
    objects.obj54 = 0;
    objects.setting_slide_volume = 0;
    objects.setting_slide_volume_num = 0;
    objects.obj55 = 0;
    objects.setting_theme = 0;
    objects.obj56 = 0;
    objects.obj57 = 0;
    objects.setting_on_screen = 0;
    objects.obj58 = 0;
    objects.obj59 = 0;
    objects.setting_time2idle = 0;
    objects.obj60 = 0;
    objects.obj61 = 0;
    objects.setting_auto_sleep = 0;
    objects.obj62 = 0;
    objects.setting_invert_display = 0;
    objects.setting_tab_advanced = 0;
    objects.obj63 = 0;
    objects.setting_btn_wifi_detail = 0;
    objects.obj64 = 0;
    objects.setting_btn_ftp = 0;
    objects.obj65 = 0;
    objects.setting_btn_system_reset = 0;
    objects.wifi_set_panel = 0;
    objects.wifi_set_panel_return = 0;
    objects.obj70 = 0;
    objects.obj66 = 0;
    objects.setting_wifi_switch = 0;
    objects.setting_wifi_connect_tip = 0;
    objects.obj67 = 0;
    objects.setting_btn_wifi_reset = 0;
    objects.setting_led_ai = 0;
    deletePageFlowState(3);
}

void tick_screen_setting() {
    void *flowState = getFlowState(0, 3);
    (void)flowState;
    {
        const char *new_val = evalTextProperty(flowState, 0, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj68);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj68;
            lv_label_set_text(objects.obj68, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 3, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj69);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj69;
            lv_label_set_text(objects.obj69, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 4, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj50);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj50;
            lv_label_set_text(objects.obj50, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = evalIntegerProperty(flowState, 17, 3, "Failed to evaluate Value in Slider widget");
        int32_t cur_val = lv_slider_get_value(objects.setting_slide_brightness);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.setting_slide_brightness;
            lv_slider_set_value(objects.setting_slide_brightness, new_val, LV_ANIM_OFF);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 18, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.setting_slide_brightness_num);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.setting_slide_brightness_num;
            lv_label_set_text(objects.setting_slide_brightness_num, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = evalIntegerProperty(flowState, 21, 3, "Failed to evaluate Value in Slider widget");
        int32_t cur_val = lv_slider_get_value(objects.setting_slide_volume);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.setting_slide_volume;
            lv_slider_set_value(objects.setting_slide_volume, new_val, LV_ANIM_OFF);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 22, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.setting_slide_volume_num);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.setting_slide_volume_num;
            lv_label_set_text(objects.setting_slide_volume_num, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 56, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj70);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj70;
            lv_label_set_text(objects.obj70, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

void create_screen_about() {
    void *flowState = getFlowState(0, 4);
    (void)flowState;
    lv_obj_t *obj = lv_obj_create(0);
    objects.about = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj82 = obj;
            lv_obj_set_pos(obj, 10, 10);
            lv_obj_set_size(obj, 200, LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj71 = obj;
            lv_obj_set_pos(obj, 440, 10);
            lv_obj_set_size(obj, 400, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, _("关于"));
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj83 = obj;
            lv_obj_set_pos(obj, LV_PCT(50), 10);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // about_btn_return
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.about_btn_return = obj;
            lv_obj_set_pos(obj, 30, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj84 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
            }
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.obj72 = obj;
            lv_obj_set_pos(obj, 32, 110);
            lv_obj_set_size(obj, LV_PCT(95), LV_PCT(80));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                static lv_coord_t dsc[] = {LV_GRID_FR(6), LV_GRID_FR(2), LV_GRID_FR(2), LV_GRID_FR(2), LV_GRID_FR(2), LV_GRID_FR(2), LV_GRID_FR(2), LV_GRID_TEMPLATE_LAST};
                lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            {
                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_image_create(parent_obj);
                    lv_obj_set_pos(obj, 590, 168);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_image_set_src(obj, "/sys/src/ui_image_sleepy.bin");
                    lv_image_set_scale(obj, 300);
                    lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj73 = obj;
                    lv_obj_set_pos(obj, 474, 357);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_span(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "TAB5 Music Box");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj74 = obj;
                    lv_obj_set_pos(obj, 10, 680);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_span(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_START, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj75 = obj;
                    lv_obj_set_pos(obj, 40, 190);
                    lv_obj_set_size(obj, LV_PCT(100), 55);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(4), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_grid_cell_row_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj76 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "硬件：");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 100, LV_SIZE_CONTENT);
                            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
                            lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "M5Stack TAB5 (ESP32P4 + ESP32C6)");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj77 = obj;
                    lv_obj_set_pos(obj, 40, 190);
                    lv_obj_set_size(obj, LV_PCT(100), 55);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(4), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_grid_cell_row_pos(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj78 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "软件：");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 100, LV_SIZE_CONTENT);
                            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
                            lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "FreeRTOS + LVGL 9.5.0 (EEZ Studio)");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj79 = obj;
                    lv_obj_set_pos(obj, 40, 190);
                    lv_obj_set_size(obj, LV_PCT(100), 55);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(4), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_grid_cell_row_pos(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj80 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "作者：");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 100, LV_SIZE_CONTENT);
                            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
                            lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "FmiL (fmil123@qq.com)");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj81 = obj;
                    lv_obj_set_pos(obj, 40, 190);
                    lv_obj_set_size(obj, LV_PCT(100), 55);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(4), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_grid_cell_row_pos(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, "/sys/src/ui_image_icon.bin");
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            // about_system_monitor_tick
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.about_system_monitor_tick = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 100, LV_SIZE_CONTENT);
                            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
                            lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "-");
                        }
                    }
                }
            }
        }
        {
            // about_led_ai
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.about_led_ai = obj;
            lv_obj_set_pos(obj, 626, 670);
            lv_obj_set_size(obj, 40, 35);
            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]));
            lv_led_set_brightness(obj, 255);
        }
    }
    
    tick_screen_about();
}

void delete_screen_about() {
    lv_obj_delete(objects.about);
    objects.about = 0;
    objects.obj82 = 0;
    objects.obj71 = 0;
    objects.obj83 = 0;
    objects.about_btn_return = 0;
    objects.obj84 = 0;
    objects.obj72 = 0;
    objects.obj73 = 0;
    objects.obj74 = 0;
    objects.obj75 = 0;
    objects.obj76 = 0;
    objects.obj77 = 0;
    objects.obj78 = 0;
    objects.obj79 = 0;
    objects.obj80 = 0;
    objects.obj81 = 0;
    objects.about_system_monitor_tick = 0;
    objects.about_led_ai = 0;
    deletePageFlowState(4);
}

void tick_screen_about() {
    void *flowState = getFlowState(0, 4);
    (void)flowState;
    {
        const char *new_val = evalTextProperty(flowState, 0, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj82);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj82;
            lv_label_set_text(objects.obj82, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 3, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj83);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj83;
            lv_label_set_text(objects.obj83, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 5, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj84);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj84;
            lv_label_set_text(objects.obj84, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 9, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj74);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj74;
            lv_label_set_text(objects.obj74, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

void create_screen_app_zen_mode() {
    void *flowState = getFlowState(0, 5);
    (void)flowState;
    lv_obj_t *obj = lv_obj_create(0);
    objects.app_zen_mode = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj95 = obj;
            lv_obj_set_pos(obj, 10, 10);
            lv_obj_set_size(obj, 200, LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj85 = obj;
            lv_obj_set_pos(obj, 440, 10);
            lv_obj_set_size(obj, 400, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, _("禅模式"));
        }
        {
            // zen_btn_home
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.zen_btn_home = obj;
            lv_obj_set_pos(obj, 30, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_spread(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
            }
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj96 = obj;
            lv_obj_set_pos(obj, LV_PCT(50), 10);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj86 = obj;
            lv_obj_set_pos(obj, 320, 55);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // zen_canvas
            lv_obj_t *obj = lv_canvas_create(parent_obj);
            objects.zen_canvas = obj;
            lv_obj_set_pos(obj, 25, 110);
            lv_obj_set_size(obj, 950, LV_PCT(80));
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
        }
        {
            // zen_ball_0
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.zen_ball_0 = obj;
            lv_obj_set_pos(obj, 30, 656);
            lv_obj_set_size(obj, 30, 30);
            lv_led_set_color(obj, lv_color_hex(0x0000ff));
            lv_led_set_brightness(obj, 255);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        }
        {
            // zen_ball_1
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.zen_ball_1 = obj;
            lv_obj_set_pos(obj, 60, 656);
            lv_obj_set_size(obj, 30, 30);
            lv_led_set_color(obj, lv_color_hex(0x0000ff));
            lv_led_set_brightness(obj, 255);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        }
        {
            // zen_ball_2
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.zen_ball_2 = obj;
            lv_obj_set_pos(obj, 90, 656);
            lv_obj_set_size(obj, 30, 30);
            lv_led_set_color(obj, lv_color_hex(0x0000ff));
            lv_led_set_brightness(obj, 255);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        }
        {
            // zen_ball_3
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.zen_ball_3 = obj;
            lv_obj_set_pos(obj, 120, 657);
            lv_obj_set_size(obj, 30, 30);
            lv_led_set_color(obj, lv_color_hex(0x0000ff));
            lv_led_set_brightness(obj, 255);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        }
        {
            // zen_ball_4
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.zen_ball_4 = obj;
            lv_obj_set_pos(obj, 150, 657);
            lv_obj_set_size(obj, 30, 30);
            lv_led_set_color(obj, lv_color_hex(0x0000ff));
            lv_led_set_brightness(obj, 255);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.obj87 = obj;
            lv_obj_set_pos(obj, 1000, 110);
            lv_obj_set_size(obj, LV_PCT(20), LV_PCT(80));
            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj88 = obj;
                    lv_obj_set_pos(obj, 944, -87);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "模式：");
                }
                {
                    // zen_dropdown_mode
                    lv_obj_t *obj = lv_dropdown_create(parent_obj);
                    objects.zen_dropdown_mode = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(100), 50);
                    lv_dropdown_set_options_static(obj, "弹珠\n雨滴");
                    lv_dropdown_set_selected(obj, 0);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                            objects.obj89 = obj;
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_ITEMS | LV_STATE_DEFAULT);
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj90 = obj;
                    lv_obj_set_pos(obj, 944, -87);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "调式：");
                }
                {
                    // zen_dropdown_key
                    lv_obj_t *obj = lv_dropdown_create(parent_obj);
                    objects.zen_dropdown_key = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(100), 50);
                    lv_dropdown_set_options_static(obj, "大调\n国风五声\n小调\n日式小调");
                    lv_dropdown_set_selected(obj, 1);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                            objects.obj91 = obj;
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_ITEMS | LV_STATE_DEFAULT);
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj92 = obj;
                    lv_obj_set_pos(obj, 944, -87);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "速度：");
                }
                {
                    // zen_dropdown_speed
                    lv_obj_t *obj = lv_dropdown_create(parent_obj);
                    objects.zen_dropdown_speed = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(100), 50);
                    lv_dropdown_set_options_static(obj, "1\n2\n3\n4\n5");
                    lv_dropdown_set_selected(obj, 0);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                            objects.obj93 = obj;
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_ITEMS | LV_STATE_DEFAULT);
                        }
                    }
                }
            }
        }
        {
            // zen_btn_rec
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.zen_btn_rec = obj;
            lv_obj_set_pos(obj, 150, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][8]), LV_PART_MAIN | LV_STATE_CHECKED);
            lv_obj_set_style_bg_opa(obj, 100, LV_PART_MAIN | LV_STATE_CHECKED);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj94 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][8]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
            }
        }
        {
            // zen_led_ai
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.zen_led_ai = obj;
            lv_obj_set_pos(obj, 626, 670);
            lv_obj_set_size(obj, 40, 35);
            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]));
            lv_led_set_brightness(obj, 255);
        }
    }
    
    tick_screen_app_zen_mode();
}

void delete_screen_app_zen_mode() {
    lv_obj_delete(objects.app_zen_mode);
    objects.app_zen_mode = 0;
    objects.obj95 = 0;
    objects.obj85 = 0;
    objects.zen_btn_home = 0;
    objects.obj96 = 0;
    objects.obj86 = 0;
    objects.zen_canvas = 0;
    objects.zen_ball_0 = 0;
    objects.zen_ball_1 = 0;
    objects.zen_ball_2 = 0;
    objects.zen_ball_3 = 0;
    objects.zen_ball_4 = 0;
    objects.obj87 = 0;
    objects.obj88 = 0;
    objects.zen_dropdown_mode = 0;
    objects.obj89 = 0;
    objects.obj90 = 0;
    objects.zen_dropdown_key = 0;
    objects.obj91 = 0;
    objects.obj92 = 0;
    objects.zen_dropdown_speed = 0;
    objects.obj93 = 0;
    objects.zen_btn_rec = 0;
    objects.obj94 = 0;
    objects.zen_led_ai = 0;
    deletePageFlowState(5);
}

void tick_screen_app_zen_mode() {
    void *flowState = getFlowState(0, 5);
    (void)flowState;
    {
        const char *new_val = evalTextProperty(flowState, 0, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj95);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj95;
            lv_label_set_text(objects.obj95, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 5, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj96);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj96;
            lv_label_set_text(objects.obj96, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 6, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj86);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj86;
            lv_label_set_text(objects.obj86, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

void create_screen_app_ear_train() {
    void *flowState = getFlowState(0, 6);
    (void)flowState;
    lv_obj_t *obj = lv_obj_create(0);
    objects.app_ear_train = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj108 = obj;
            lv_obj_set_pos(obj, 10, 10);
            lv_obj_set_size(obj, 200, LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj97 = obj;
            lv_obj_set_pos(obj, 440, 10);
            lv_obj_set_size(obj, 400, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "练耳");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj109 = obj;
            lv_obj_set_pos(obj, LV_PCT(50), 10);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj98 = obj;
            lv_obj_set_pos(obj, 320, 50);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // ear_btn_home
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.ear_btn_home = obj;
            lv_obj_set_pos(obj, 30, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
            }
        }
        {
            // ear_key_try_play
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.ear_key_try_play = obj;
            lv_obj_set_pos(obj, 682, 123);
            lv_obj_set_size(obj, 134, 100);
            lv_obj_set_style_text_font(obj, ui_font_icon_70, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj99 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
            }
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj100 = obj;
            lv_obj_set_pos(obj, 509, 132);
            lv_obj_set_size(obj, 140, LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "点击试听");
        }
        {
            // ear_label_try_count
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.ear_label_try_count = obj;
            lv_obj_set_pos(obj, 529, 169);
            lv_obj_set_size(obj, 100, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "888");
        }
        {
            // ear_score_title
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.ear_score_title = obj;
            lv_obj_set_pos(obj, 317, 131);
            lv_obj_set_size(obj, LV_PCT(12), LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "得分");
        }
        {
            // ear_score
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.ear_score = obj;
            lv_obj_set_pos(obj, 344, 171);
            lv_obj_set_size(obj, 100, LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][13]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "200");
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.obj101 = obj;
            lv_obj_set_pos(obj, 1000, 110);
            lv_obj_set_size(obj, LV_PCT(20), LV_PCT(80));
            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_ofs_x(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_ofs_y(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_spread(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // ear_trainer_test
                    lv_obj_t *obj = lv_dropdown_create(parent_obj);
                    objects.ear_trainer_test = obj;
                    lv_obj_set_pos(obj, 607, -118);
                    lv_obj_set_size(obj, LV_PCT(100), 50);
                    lv_dropdown_set_options_static(obj, "= 挑战 =\n= 练习=");
                    lv_dropdown_set_selected(obj, 0);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                            objects.obj102 = obj;
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj103 = obj;
                    lv_obj_set_pos(obj, 1073, 192);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "模式：");
                }
                {
                    // ear_mode
                    lv_obj_t *obj = lv_dropdown_create(parent_obj);
                    objects.ear_mode = obj;
                    lv_obj_set_pos(obj, 999, 237);
                    lv_obj_set_size(obj, LV_PCT(100), 50);
                    lv_dropdown_set_options_static(obj, "绝对音感\n相对音感");
                    lv_dropdown_set_selected(obj, 0);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                            objects.obj104 = obj;
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj105 = obj;
                    lv_obj_set_pos(obj, 1073, 192);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "难度：");
                }
                {
                    // ear_difficult
                    lv_obj_t *obj = lv_dropdown_create(parent_obj);
                    objects.ear_difficult = obj;
                    lv_obj_set_pos(obj, 999, 237);
                    lv_obj_set_size(obj, LV_PCT(100), 50);
                    lv_dropdown_set_options_static(obj, "初级\n中级\n高级");
                    lv_dropdown_set_selected(obj, 0);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                            objects.obj106 = obj;
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj107 = obj;
                    lv_obj_set_pos(obj, 75, 134);
                    lv_obj_set_size(obj, LV_PCT(100), 80);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "\n历史最高");
                }
                {
                    // ear_best_score
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.ear_best_score = obj;
                    lv_obj_set_pos(obj, 187, 125);
                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][15]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "100");
                }
            }
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 25, 250);
            lv_obj_set_size(obj, 950, LV_PCT(60));
            {
                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            {
                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // ear_key_major
                    lv_obj_t *obj = lv_buttonmatrix_create(parent_obj);
                    objects.ear_key_major = obj;
                    lv_obj_set_pos(obj, 0, LV_PCT(0));
                    lv_obj_set_size(obj, LV_PCT(100), LV_PCT(100));
                    static const char *map[8] = {
                        "\n\n\n\n\n\n\n\nC",
                        "\n\n\n\n\n\n\n\nD",
                        "\n\n\n\n\n\n\n\nE",
                        "\n\n\n\n\n\n\n\nF",
                        "\n\n\n\n\n\n\n\nG",
                        "\n\n\n\n\n\n\n\nA",
                        "\n\n\n\n\n\n\n\nB",
                        NULL,
                    };
                    lv_buttonmatrix_set_map(obj, map);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_ITEMS | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_ITEMS | LV_STATE_DEFAULT);
                }
                {
                    // ear_key_minor2
                    lv_obj_t *obj = lv_buttonmatrix_create(parent_obj);
                    objects.ear_key_minor2 = obj;
                    lv_obj_set_pos(obj, 68, LV_PCT(0));
                    lv_obj_set_size(obj, 300, LV_PCT(55));
                    static const char *map[3] = {
                        "\n\n\nC#",
                        "\n\n\nD#",
                        NULL,
                    };
                    lv_buttonmatrix_set_map(obj, map);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_column(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_ITEMS | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_ITEMS | LV_STATE_DEFAULT);
                }
                {
                    // ear_key_minor3
                    lv_obj_t *obj = lv_buttonmatrix_create(parent_obj);
                    objects.ear_key_minor3 = obj;
                    lv_obj_set_pos(obj, 459, 0);
                    lv_obj_set_size(obj, 435, LV_PCT(55));
                    static const char *map[4] = {
                        "\n\n\nF#",
                        "\n\n\nAb",
                        "\n\n\nBb",
                        NULL,
                    };
                    lv_buttonmatrix_set_map(obj, map);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_column(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_ITEMS | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_ITEMS | LV_STATE_DEFAULT);
                }
            }
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 25, 250);
            lv_obj_set_size(obj, 950, LV_PCT(60));
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            {
                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            {
                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // ear_key_interval
                    lv_obj_t *obj = lv_buttonmatrix_create(parent_obj);
                    objects.ear_key_interval = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(100), LV_PCT(100));
                    static const char *map[15] = {
                        "小二度",
                        "大二度",
                        "小三度",
                        "大三度",
                        "\n",
                        "纯四度",
                        "增四度",
                        "纯五度",
                        "小六度",
                        "\n",
                        "大六度",
                        "小七度",
                        "大七度",
                        "八度",
                        NULL,
                    };
                    lv_buttonmatrix_set_map(obj, map);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_column(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_row(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_ITEMS | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_ITEMS | LV_STATE_DEFAULT);
                }
            }
        }
        {
            // ear_life_panel
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.ear_life_panel = obj;
            lv_obj_set_pos(obj, 97, 132);
            lv_obj_set_size(obj, 239, 78);
            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_main_place(obj, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_column(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 23, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // ear_life1
                    lv_obj_t *obj = lv_led_create(parent_obj);
                    objects.ear_life1 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 32, 32);
                    lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][7]));
                    lv_led_set_brightness(obj, 255);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                }
                {
                    // ear_life2
                    lv_obj_t *obj = lv_led_create(parent_obj);
                    objects.ear_life2 = obj;
                    lv_obj_set_pos(obj, 547, 204);
                    lv_obj_set_size(obj, 32, 32);
                    lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][7]));
                    lv_led_set_brightness(obj, 255);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                }
                {
                    // ear_life3
                    lv_obj_t *obj = lv_led_create(parent_obj);
                    objects.ear_life3 = obj;
                    lv_obj_set_pos(obj, 504, 204);
                    lv_obj_set_size(obj, 32, 32);
                    lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][7]));
                    lv_led_set_brightness(obj, 255);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                }
            }
        }
        {
            // ear_led_ai
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.ear_led_ai = obj;
            lv_obj_set_pos(obj, 626, 670);
            lv_obj_set_size(obj, 40, 35);
            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]));
            lv_led_set_brightness(obj, 255);
        }
    }
    
    tick_screen_app_ear_train();
}

void delete_screen_app_ear_train() {
    lv_obj_delete(objects.app_ear_train);
    objects.app_ear_train = 0;
    objects.obj108 = 0;
    objects.obj97 = 0;
    objects.obj109 = 0;
    objects.obj98 = 0;
    objects.ear_btn_home = 0;
    objects.ear_key_try_play = 0;
    objects.obj99 = 0;
    objects.obj100 = 0;
    objects.ear_label_try_count = 0;
    objects.ear_score_title = 0;
    objects.ear_score = 0;
    objects.obj101 = 0;
    objects.ear_trainer_test = 0;
    objects.obj102 = 0;
    objects.obj103 = 0;
    objects.ear_mode = 0;
    objects.obj104 = 0;
    objects.obj105 = 0;
    objects.ear_difficult = 0;
    objects.obj106 = 0;
    objects.obj107 = 0;
    objects.ear_best_score = 0;
    objects.ear_key_major = 0;
    objects.ear_key_minor2 = 0;
    objects.ear_key_minor3 = 0;
    objects.ear_key_interval = 0;
    objects.ear_life_panel = 0;
    objects.ear_life1 = 0;
    objects.ear_life2 = 0;
    objects.ear_life3 = 0;
    objects.ear_led_ai = 0;
    deletePageFlowState(6);
}

void tick_screen_app_ear_train() {
    void *flowState = getFlowState(0, 6);
    (void)flowState;
    {
        const char *new_val = evalTextProperty(flowState, 0, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj108);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj108;
            lv_label_set_text(objects.obj108, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 3, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj109);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj109;
            lv_label_set_text(objects.obj109, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 4, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj98);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj98;
            lv_label_set_text(objects.obj98, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

void create_screen_app_chord_memory() {
    void *flowState = getFlowState(0, 7);
    (void)flowState;
    lv_obj_t *obj = lv_obj_create(0);
    objects.app_chord_memory = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj114 = obj;
            lv_obj_set_pos(obj, 10, 10);
            lv_obj_set_size(obj, 200, LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj110 = obj;
            lv_obj_set_pos(obj, 440, 10);
            lv_obj_set_size(obj, 400, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "和弦练习");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj115 = obj;
            lv_obj_set_pos(obj, LV_PCT(50), 10);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj111 = obj;
            lv_obj_set_pos(obj, 320, 50);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // chord_btn_home
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.chord_btn_home = obj;
            lv_obj_set_pos(obj, 30, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
            }
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 32, 110);
            lv_obj_set_size(obj, LV_PCT(95), LV_PCT(18));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // chord_key_key
                    lv_obj_t *obj = lv_buttonmatrix_create(parent_obj);
                    objects.chord_key_key = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(100), LV_PCT(100));
                    static const char *map[13] = {
                        "C",
                        "C#",
                        "D",
                        "D#",
                        "E",
                        "F",
                        "F#",
                        "G",
                        "Ab",
                        "A",
                        "Bb",
                        "B",
                        NULL,
                    };
                    static lv_buttonmatrix_ctrl_t ctrl_map[12] = {
                        1 | LV_BUTTONMATRIX_CTRL_CHECKED,
                        1,
                        1,
                        1,
                        1,
                        1,
                        1,
                        1,
                        1,
                        1,
                        1,
                        1,
                    };
                    lv_buttonmatrix_set_map(obj, map);
                    lv_buttonmatrix_set_ctrl_map(obj, ctrl_map);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_left(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_right(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_ITEMS | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_ITEMS | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_ITEMS | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 1, LV_PART_ITEMS | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_ITEMS | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_ITEMS | LV_STATE_CHECKED);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_ITEMS | LV_STATE_CHECKED);
                }
            }
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.obj112 = obj;
            lv_obj_set_pos(obj, 32, 230);
            lv_obj_set_size(obj, LV_PCT(95), LV_PCT(22));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_MOMENTUM|LV_OBJ_FLAG_SCROLL_WITH_ARROW|LV_OBJ_FLAG_SNAPPABLE);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_AUTO, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            {
                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(2), LV_GRID_TEMPLATE_LAST};
                lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj113 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(10), LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_margin_top(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "标识：");
                }
                {
                    // chord_definition
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.chord_definition = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(32), LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_span(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "大三度+小三度+小三度+小三度+小三度+小三度");
                }
                {
                    // chord_panel_type_poll
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.chord_panel_type_poll = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 300, LV_PCT(100));
                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_span(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_left(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_right(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // chord_type_maj
                            lv_obj_t *obj = lv_button_create(parent_obj);
                            objects.chord_type_maj = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 120, LV_PCT(100));
                            lv_obj_add_state(obj, LV_STATE_PRESSED);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_PRESSED);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_PRESSED);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "大三\n");
                                }
                            }
                        }
                    }
                }
            }
        }
        {
            // chord_name
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.chord_name = obj;
            lv_obj_set_pos(obj, 130, 242);
            lv_obj_set_size(obj, LV_PCT(25), LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_AUTO, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "F#7sus4");
        }
        {
            // chord_canvas_piano
            lv_obj_t *obj = lv_canvas_create(parent_obj);
            objects.chord_canvas_piano = obj;
            lv_obj_set_pos(obj, 32, 390);
            lv_obj_set_size(obj, LV_PCT(95), 300);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
        }
        {
            // chord_led_ai
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.chord_led_ai = obj;
            lv_obj_set_pos(obj, 626, 670);
            lv_obj_set_size(obj, 40, 35);
            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]));
            lv_led_set_brightness(obj, 255);
        }
    }
    
    tick_screen_app_chord_memory();
}

void delete_screen_app_chord_memory() {
    lv_obj_delete(objects.app_chord_memory);
    objects.app_chord_memory = 0;
    objects.obj114 = 0;
    objects.obj110 = 0;
    objects.obj115 = 0;
    objects.obj111 = 0;
    objects.chord_btn_home = 0;
    objects.chord_key_key = 0;
    objects.obj112 = 0;
    objects.obj113 = 0;
    objects.chord_definition = 0;
    objects.chord_panel_type_poll = 0;
    objects.chord_type_maj = 0;
    objects.chord_name = 0;
    objects.chord_canvas_piano = 0;
    objects.chord_led_ai = 0;
    deletePageFlowState(7);
}

void tick_screen_app_chord_memory() {
    void *flowState = getFlowState(0, 7);
    (void)flowState;
    {
        const char *new_val = evalTextProperty(flowState, 0, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj114);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj114;
            lv_label_set_text(objects.obj114, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 3, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj115);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj115;
            lv_label_set_text(objects.obj115, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 4, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj111);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj111;
            lv_label_set_text(objects.obj111, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

void create_screen_app_circle_of_fifths() {
    void *flowState = getFlowState(0, 8);
    (void)flowState;
    lv_obj_t *obj = lv_obj_create(0);
    objects.app_circle_of_fifths = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj122 = obj;
            lv_obj_set_pos(obj, 10, 10);
            lv_obj_set_size(obj, 200, LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj116 = obj;
            lv_obj_set_pos(obj, 440, 10);
            lv_obj_set_size(obj, 400, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "五度圈");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj123 = obj;
            lv_obj_set_pos(obj, LV_PCT(50), 10);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // fifth_btn_home
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.fifth_btn_home = obj;
            lv_obj_set_pos(obj, 30, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
            }
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj117 = obj;
            lv_obj_set_pos(obj, 320, 50);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // fifth_canvas_circle
            lv_obj_t *obj = lv_canvas_create(parent_obj);
            objects.fifth_canvas_circle = obj;
            lv_obj_set_pos(obj, 40, 128);
            lv_obj_set_size(obj, 540, 540);
        }
        {
            // fifth_panel_info
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.fifth_panel_info = obj;
            lv_obj_set_pos(obj, 610, 110);
            lv_obj_set_size(obj, LV_PCT(50), LV_PCT(80));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                static lv_coord_t dsc[] = {LV_GRID_FR(2), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(5), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            {
                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // fifth_name
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.fifth_name = obj;
                    lv_obj_set_pos(obj, 944, -87);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_span(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "C大调/Am小调");
                }
                {
                    // fifth_key_sig
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.fifth_key_sig = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "调号：0");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj118 = obj;
                    lv_obj_set_pos(obj, 944, -87);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "音阶：");
                }
                {
                    // fifth_scale
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.fifth_scale = obj;
                    lv_obj_set_pos(obj, 944, -87);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_span(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Cb Db Eb Fb Gb Ab B#");
                }
                {
                    // fifth_canvas_piano
                    lv_obj_t *obj = lv_canvas_create(parent_obj);
                    objects.fifth_canvas_piano = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 180, 100);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_set_style_grid_cell_row_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_span(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj119 = obj;
                    lv_obj_set_pos(obj, 944, -87);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_END, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "属");
                }
                {
                    // fifth_dominant
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.fifth_dominant = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][13]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "G(V)");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj120 = obj;
                    lv_obj_set_pos(obj, 944, -87);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_END, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "平行");
                }
                {
                    // fifth_parallel
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.fifth_parallel = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
                    lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Cm");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj121 = obj;
                    lv_obj_set_pos(obj, 944, -87);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_END, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "下属");
                }
                {
                    // fifth_subdominant
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.fifth_subdominant = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
                    lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][13]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "F(IV)");
                }
            }
        }
        {
            // fifth_led_ai
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.fifth_led_ai = obj;
            lv_obj_set_pos(obj, 626, 670);
            lv_obj_set_size(obj, 40, 35);
            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]));
            lv_led_set_brightness(obj, 255);
        }
    }
    
    tick_screen_app_circle_of_fifths();
}

void delete_screen_app_circle_of_fifths() {
    lv_obj_delete(objects.app_circle_of_fifths);
    objects.app_circle_of_fifths = 0;
    objects.obj122 = 0;
    objects.obj116 = 0;
    objects.obj123 = 0;
    objects.fifth_btn_home = 0;
    objects.obj117 = 0;
    objects.fifth_canvas_circle = 0;
    objects.fifth_panel_info = 0;
    objects.fifth_name = 0;
    objects.fifth_key_sig = 0;
    objects.obj118 = 0;
    objects.fifth_scale = 0;
    objects.fifth_canvas_piano = 0;
    objects.obj119 = 0;
    objects.fifth_dominant = 0;
    objects.obj120 = 0;
    objects.fifth_parallel = 0;
    objects.obj121 = 0;
    objects.fifth_subdominant = 0;
    objects.fifth_led_ai = 0;
    deletePageFlowState(8);
}

void tick_screen_app_circle_of_fifths() {
    void *flowState = getFlowState(0, 8);
    (void)flowState;
    {
        const char *new_val = evalTextProperty(flowState, 0, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj122);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj122;
            lv_label_set_text(objects.obj122, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 3, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj123);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj123;
            lv_label_set_text(objects.obj123, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 6, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj117);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj117;
            lv_label_set_text(objects.obj117, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

void create_screen_app_tiny_piano() {
    void *flowState = getFlowState(0, 9);
    (void)flowState;
    lv_obj_t *obj = lv_obj_create(0);
    objects.app_tiny_piano = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj136 = obj;
            lv_obj_set_pos(obj, 10, 10);
            lv_obj_set_size(obj, 200, LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj124 = obj;
            lv_obj_set_pos(obj, 440, 10);
            lv_obj_set_size(obj, 400, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "小钢琴");
        }
        {
            // piano_btn_home
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.piano_btn_home = obj;
            lv_obj_set_pos(obj, 30, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
            }
        }
        {
            // piano_btn_rec
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.piano_btn_rec = obj;
            lv_obj_set_pos(obj, 150, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][8]), LV_PART_MAIN | LV_STATE_CHECKED);
            lv_obj_set_style_bg_opa(obj, 100, LV_PART_MAIN | LV_STATE_CHECKED);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj125 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][8]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
            }
        }
        {
            // piano_btn_set
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.piano_btn_set = obj;
            lv_obj_set_pos(obj, 1150, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj137 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
            }
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj138 = obj;
            lv_obj_set_pos(obj, LV_PCT(50), 10);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj126 = obj;
            lv_obj_set_pos(obj, 320, 50);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // piano_panel_m
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.piano_panel_m = obj;
            lv_obj_set_pos(obj, 32, 110);
            lv_obj_set_size(obj, LV_PCT(95), LV_PCT(80));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            {
                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_column(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_row(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // piano_pad0
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.piano_pad0 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 170, 170);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // piano_pad1
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.piano_pad1 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 170, 170);
                    lv_obj_set_style_grid_cell_row_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // piano_pad2
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.piano_pad2 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 170, 170);
                    lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // piano_pad3
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.piano_pad3 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 170, 170);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // piano_pad4
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.piano_pad4 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 170, 170);
                    lv_obj_set_style_grid_cell_column_pos(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // piano_pad5
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.piano_pad5 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 170, 170);
                    lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // piano_pad6
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.piano_pad6 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 170, 170);
                    lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // piano_pad7
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.piano_pad7 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 170, 170);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // piano_pad8
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.piano_pad8 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 170, 170);
                    lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // piano_pad9
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.piano_pad9 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 170, 170);
                    lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // piano_pad10
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.piano_pad10 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 170, 170);
                    lv_obj_set_style_grid_cell_row_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // piano_pad11
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.piano_pad11 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 170, 170);
                    lv_obj_set_style_grid_cell_row_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // piano_pad12
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.piano_pad12 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 170, 170);
                    lv_obj_set_style_grid_cell_row_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // piano_pad13
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.piano_pad13 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 170, 170);
                    lv_obj_set_style_grid_cell_row_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // piano_pad14
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.piano_pad14 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 170, 170);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
            }
        }
        {
            // piano_panel_v
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.piano_panel_v = obj;
            lv_obj_set_pos(obj, 32, 110);
            lv_obj_set_size(obj, LV_PCT(95), LV_PCT(80));
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj127 = obj;
                    lv_obj_set_pos(obj, 41, 350);
                    lv_obj_set_size(obj, LV_PCT(100), LV_PCT(30));
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_right(obj, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "根音:");
                        }
                        {
                            // piano_root_v
                            lv_obj_t *obj = lv_roller_create(parent_obj);
                            objects.piano_root_v = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 80, 100);
                            lv_roller_set_options(obj, "C0\nC1\nC2\nC3\nC4\nC5\nC6", LV_ROLLER_MODE_NORMAL);
                            lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_SELECTED | LV_STATE_DEFAULT);
                        }
                    }
                }
                {
                    // piano_canvas_key
                    lv_obj_t *obj = lv_canvas_create(parent_obj);
                    objects.piano_canvas_key = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(100), LV_PCT(65));
                }
            }
        }
        {
            // piano_set
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.piano_set = obj;
            lv_obj_set_pos(obj, 128, 150);
            lv_obj_set_size(obj, LV_PCT(80), LV_PCT(70));
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_row(obj, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // piano_set_btn_return
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.piano_set_btn_return = obj;
                    lv_obj_set_pos(obj, 20, 20);
                    lv_obj_set_size(obj, 100, 60);
                    lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj139 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj128 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(100), 60);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "显示样式：");
                        }
                        {
                            // piano_display_type
                            lv_obj_t *obj = lv_dropdown_create(parent_obj);
                            objects.piano_display_type = obj;
                            lv_obj_set_pos(obj, 994, 360);
                            lv_obj_set_size(obj, 229, 50);
                            lv_dropdown_set_options_static(obj, "按键矩阵\n模拟钢琴");
                            lv_dropdown_set_selected(obj, 0);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                                    objects.obj129 = obj;
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
                                }
                            }
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj130 = obj;
                    lv_obj_set_pos(obj, 41, 350);
                    lv_obj_set_size(obj, LV_PCT(100), 60);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "调式（仅矩阵按键生效）：");
                        }
                        {
                            // piano_scale_type
                            lv_obj_t *obj = lv_dropdown_create(parent_obj);
                            objects.piano_scale_type = obj;
                            lv_obj_set_pos(obj, 994, 360);
                            lv_obj_set_size(obj, 229, 50);
                            lv_dropdown_set_options_static(obj, "大调\n中国五声\n小调\n小调五声音阶\n日本调式");
                            lv_dropdown_set_selected(obj, 0);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                                    objects.obj131 = obj;
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
                                }
                            }
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj132 = obj;
                    lv_obj_set_pos(obj, 79, 362);
                    lv_obj_set_size(obj, LV_PCT(100), 60);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "音调：");
                        }
                        {
                            // piano_pitch
                            lv_obj_t *obj = lv_dropdown_create(parent_obj);
                            objects.piano_pitch = obj;
                            lv_obj_set_pos(obj, 994, 360);
                            lv_obj_set_size(obj, 229, 50);
                            lv_dropdown_set_options_static(obj, "C\nC#\nD\nD#\nE\nF\nGb\nG\nG#\nA\nA#\nB\nC");
                            lv_dropdown_set_selected(obj, 0);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                                    objects.obj133 = obj;
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
                                }
                            }
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj134 = obj;
                    lv_obj_set_pos(obj, 79, 362);
                    lv_obj_set_size(obj, LV_PCT(100), 60);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "音色：");
                        }
                        {
                            // piano_sound_type
                            lv_obj_t *obj = lv_dropdown_create(parent_obj);
                            objects.piano_sound_type = obj;
                            lv_obj_set_pos(obj, 994, 360);
                            lv_obj_set_size(obj, 229, 50);
                            lv_dropdown_set_options_static(obj, "00-大钢琴\n01-明亮钢琴\n02-电三角钢琴\n03-酒吧钢琴\n04-电钢琴1\n05-电钢琴2\n06-羽管键琴\n07-克拉维尼特\n08-钢片琴\n09-钟琴\n10-八音盒\n11-颤音琴\n12-马林巴\n13-木琴\n14-管钟\n15-扬琴");
                            lv_dropdown_set_selected(obj, 0);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                                    objects.obj135 = obj;
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
                                }
                            }
                        }
                    }
                }
            }
        }
        {
            // piano_led_ai
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.piano_led_ai = obj;
            lv_obj_set_pos(obj, 626, 670);
            lv_obj_set_size(obj, 40, 35);
            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]));
            lv_led_set_brightness(obj, 255);
        }
    }
    
    tick_screen_app_tiny_piano();
}

void delete_screen_app_tiny_piano() {
    lv_obj_delete(objects.app_tiny_piano);
    objects.app_tiny_piano = 0;
    objects.obj136 = 0;
    objects.obj124 = 0;
    objects.piano_btn_home = 0;
    objects.piano_btn_rec = 0;
    objects.obj125 = 0;
    objects.piano_btn_set = 0;
    objects.obj137 = 0;
    objects.obj138 = 0;
    objects.obj126 = 0;
    objects.piano_panel_m = 0;
    objects.piano_pad0 = 0;
    objects.piano_pad1 = 0;
    objects.piano_pad2 = 0;
    objects.piano_pad3 = 0;
    objects.piano_pad4 = 0;
    objects.piano_pad5 = 0;
    objects.piano_pad6 = 0;
    objects.piano_pad7 = 0;
    objects.piano_pad8 = 0;
    objects.piano_pad9 = 0;
    objects.piano_pad10 = 0;
    objects.piano_pad11 = 0;
    objects.piano_pad12 = 0;
    objects.piano_pad13 = 0;
    objects.piano_pad14 = 0;
    objects.piano_panel_v = 0;
    objects.obj127 = 0;
    objects.piano_root_v = 0;
    objects.piano_canvas_key = 0;
    objects.piano_set = 0;
    objects.piano_set_btn_return = 0;
    objects.obj139 = 0;
    objects.obj128 = 0;
    objects.piano_display_type = 0;
    objects.obj129 = 0;
    objects.obj130 = 0;
    objects.piano_scale_type = 0;
    objects.obj131 = 0;
    objects.obj132 = 0;
    objects.piano_pitch = 0;
    objects.obj133 = 0;
    objects.obj134 = 0;
    objects.piano_sound_type = 0;
    objects.obj135 = 0;
    objects.piano_led_ai = 0;
    deletePageFlowState(9);
}

void tick_screen_app_tiny_piano() {
    void *flowState = getFlowState(0, 9);
    (void)flowState;
    {
        const char *new_val = evalTextProperty(flowState, 0, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj136);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj136;
            lv_label_set_text(objects.obj136, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 8, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj137);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj137;
            lv_label_set_text(objects.obj137, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 9, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj138);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj138;
            lv_label_set_text(objects.obj138, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 10, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj126);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj126;
            lv_label_set_text(objects.obj126, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 34, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj139);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj139;
            lv_label_set_text(objects.obj139, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

void create_screen_app_drum_pad() {
    void *flowState = getFlowState(0, 10);
    (void)flowState;
    lv_obj_t *obj = lv_obj_create(0);
    objects.app_drum_pad = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj147 = obj;
            lv_obj_set_pos(obj, 10, 10);
            lv_obj_set_size(obj, 200, LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj140 = obj;
            lv_obj_set_pos(obj, 440, 10);
            lv_obj_set_size(obj, 400, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "鼓垫");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj148 = obj;
            lv_obj_set_pos(obj, LV_PCT(50), 10);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj141 = obj;
            lv_obj_set_pos(obj, 320, 50);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // drum_btn_home
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.drum_btn_home = obj;
            lv_obj_set_pos(obj, 30, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
            }
        }
        {
            // drum_btn_rec
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.drum_btn_rec = obj;
            lv_obj_set_pos(obj, 150, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][8]), LV_PART_MAIN | LV_STATE_CHECKED);
            lv_obj_set_style_bg_opa(obj, 100, LV_PART_MAIN | LV_STATE_CHECKED);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj142 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][8]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
            }
        }
        {
            // drum_btn_set
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.drum_btn_set = obj;
            lv_obj_set_pos(obj, 1150, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj149 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
            }
        }
        {
            // drum_panel_v
            lv_obj_t *obj = lv_canvas_create(parent_obj);
            objects.drum_panel_v = obj;
            lv_obj_set_pos(obj, 32, 110);
            lv_obj_set_size(obj, LV_PCT(95), LV_PCT(80));
        }
        {
            // drum_panel_m
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.drum_panel_m = obj;
            lv_obj_set_pos(obj, 32, 110);
            lv_obj_set_size(obj, LV_PCT(95), LV_PCT(80));
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            {
                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_column(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_row(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // drum_crash_m
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.drum_crash_m = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 250, 250);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_PRESSED);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 1307, -203);
                            lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                            lv_label_set_text_static(obj, "Crash");
                        }
                    }
                }
                {
                    // drum_clap_m
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.drum_clap_m = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 250, 250);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_PRESSED);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 1307, -203);
                            lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                            lv_label_set_text_static(obj, "Clap");
                        }
                    }
                }
                {
                    // drum_openhht_m
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.drum_openhht_m = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 250, 250);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_PRESSED);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 1307, -203);
                            lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                            lv_label_set_text_static(obj, "Open HH");
                        }
                    }
                }
                {
                    // drum_closedhh_m
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.drum_closedhh_m = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 250, 250);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_PRESSED);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 1307, -203);
                            lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                            lv_label_set_text_static(obj, "Closed HH");
                        }
                    }
                }
                {
                    // drum_ride_m
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.drum_ride_m = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 250, 250);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_PRESSED);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 1307, -203);
                            lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                            lv_label_set_text_static(obj, "Ride");
                        }
                    }
                }
                {
                    // drum_snare_n
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.drum_snare_n = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 250, 250);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_PRESSED);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 1307, -203);
                            lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                            lv_label_set_text_static(obj, "Snare");
                        }
                    }
                }
                {
                    // drum_kick_m
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.drum_kick_m = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 250, 250);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_PRESSED);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 1307, -203);
                            lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                            lv_label_set_text_static(obj, "Kick");
                        }
                    }
                }
                {
                    // drum_floortom_m
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.drum_floortom_m = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 250, 250);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_PRESSED);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 1307, -203);
                            lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                            lv_label_set_text_static(obj, "Floor Tom");
                        }
                    }
                }
            }
        }
        {
            // drum_pad_set
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.drum_pad_set = obj;
            lv_obj_set_pos(obj, 128, 150);
            lv_obj_set_size(obj, LV_PCT(80), LV_PCT(70));
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 240, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_row(obj, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // drum_set_btn_return
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.drum_set_btn_return = obj;
                    lv_obj_set_pos(obj, 20, 20);
                    lv_obj_set_size(obj, 100, 60);
                    lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj150 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj143 = obj;
                    lv_obj_set_pos(obj, 41, 350);
                    lv_obj_set_size(obj, LV_PCT(100), 60);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "显示样式：");
                        }
                        {
                            // drum_display_type
                            lv_obj_t *obj = lv_dropdown_create(parent_obj);
                            objects.drum_display_type = obj;
                            lv_obj_set_pos(obj, 994, 360);
                            lv_obj_set_size(obj, 229, 50);
                            lv_dropdown_set_options_static(obj, "模拟组鼓\n打击垫");
                            lv_dropdown_set_selected(obj, 0);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                                    objects.obj144 = obj;
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
                                }
                            }
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj145 = obj;
                    lv_obj_set_pos(obj, 41, 350);
                    lv_obj_set_size(obj, LV_PCT(100), 60);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "鼓类型：");
                        }
                        {
                            // drum_sound_type
                            lv_obj_t *obj = lv_dropdown_create(parent_obj);
                            objects.drum_sound_type = obj;
                            lv_obj_set_pos(obj, 994, 360);
                            lv_obj_set_size(obj, 229, 50);
                            lv_dropdown_set_options_static(obj, "000-标准\n008-Room\n016-Power\n024-Electronic\n025-TR-808\n032-Jazz\n040-Brush\n048-Prchestra\n056-SFX");
                            lv_dropdown_set_selected(obj, 0);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                                    objects.obj146 = obj;
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
                                }
                            }
                        }
                    }
                }
            }
        }
        {
            // drum_led_ai
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.drum_led_ai = obj;
            lv_obj_set_pos(obj, 626, 670);
            lv_obj_set_size(obj, 40, 35);
            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]));
            lv_led_set_brightness(obj, 255);
        }
    }
    
    tick_screen_app_drum_pad();
}

void delete_screen_app_drum_pad() {
    lv_obj_delete(objects.app_drum_pad);
    objects.app_drum_pad = 0;
    objects.obj147 = 0;
    objects.obj140 = 0;
    objects.obj148 = 0;
    objects.obj141 = 0;
    objects.drum_btn_home = 0;
    objects.drum_btn_rec = 0;
    objects.obj142 = 0;
    objects.drum_btn_set = 0;
    objects.obj149 = 0;
    objects.drum_panel_v = 0;
    objects.drum_panel_m = 0;
    objects.drum_crash_m = 0;
    objects.drum_clap_m = 0;
    objects.drum_openhht_m = 0;
    objects.drum_closedhh_m = 0;
    objects.drum_ride_m = 0;
    objects.drum_snare_n = 0;
    objects.drum_kick_m = 0;
    objects.drum_floortom_m = 0;
    objects.drum_pad_set = 0;
    objects.drum_set_btn_return = 0;
    objects.obj150 = 0;
    objects.obj143 = 0;
    objects.drum_display_type = 0;
    objects.obj144 = 0;
    objects.obj145 = 0;
    objects.drum_sound_type = 0;
    objects.obj146 = 0;
    objects.drum_led_ai = 0;
    deletePageFlowState(10);
}

void tick_screen_app_drum_pad() {
    void *flowState = getFlowState(0, 10);
    (void)flowState;
    {
        const char *new_val = evalTextProperty(flowState, 0, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj147);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj147;
            lv_label_set_text(objects.obj147, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 3, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj148);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj148;
            lv_label_set_text(objects.obj148, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 4, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj141);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj141;
            lv_label_set_text(objects.obj141, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 10, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj149);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj149;
            lv_label_set_text(objects.obj149, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 31, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj150);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj150;
            lv_label_set_text(objects.obj150, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

void create_screen_app_midi_player() {
    void *flowState = getFlowState(0, 11);
    (void)flowState;
    lv_obj_t *obj = lv_obj_create(0);
    objects.app_midi_player = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj151 = obj;
            lv_obj_set_pos(obj, 440, 10);
            lv_obj_set_size(obj, 400, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "Midi播放器");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj152 = obj;
            lv_obj_set_pos(obj, 320, 50);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj161 = obj;
            lv_obj_set_pos(obj, LV_PCT(50), 10);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj162 = obj;
            lv_obj_set_pos(obj, 10, 10);
            lv_obj_set_size(obj, 200, LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_label_set_text(obj, "");
        }
        {
            // midi_btn_home
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.midi_btn_home = obj;
            lv_obj_set_pos(obj, 30, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
            }
        }
        {
            // midi_btn_set
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.midi_btn_set = obj;
            lv_obj_set_pos(obj, 1150, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj163 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
            }
        }
        {
            // midi_panel_mid_list
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.midi_panel_mid_list = obj;
            lv_obj_set_pos(obj, 32, 110);
            lv_obj_set_size(obj, LV_PCT(47), LV_PCT(80));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 32, 110);
                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Midi歌曲列表");
                }
                {
                    // midi_list_music_file
                    lv_obj_t *obj = lv_list_create(parent_obj);
                    objects.midi_list_music_file = obj;
                    lv_obj_set_pos(obj, 32, 146);
                    lv_obj_set_size(obj, LV_PCT(100), LV_PCT(95));
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_row(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_left(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // midi_file_example
                            lv_obj_t *obj = lv_button_create(parent_obj);
                            objects.midi_file_example = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_PCT(100), 50);
                            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(99), 34);
                                    lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
                                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text(obj, " AbandonAbandonAbandonAbandon.mid");
                                }
                            }
                        }
                    }
                }
            }
        }
        {
            // midi_panel_hmr_list
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.midi_panel_hmr_list = obj;
            lv_obj_set_pos(obj, 32, 110);
            lv_obj_set_size(obj, LV_PCT(47), LV_PCT(80));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 32, 110);
                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "录音列表");
                }
                {
                    // midi_list_record_file
                    lv_obj_t *obj = lv_list_create(parent_obj);
                    objects.midi_list_record_file = obj;
                    lv_obj_set_pos(obj, 32, 146);
                    lv_obj_set_size(obj, LV_PCT(100), LV_PCT(95));
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_row(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_left(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
            }
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.obj153 = obj;
            lv_obj_set_pos(obj, 650, 110);
            lv_obj_set_size(obj, LV_PCT(47), LV_PCT(80));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_row(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj154 = obj;
                    lv_obj_set_pos(obj, -342, 26);
                    lv_obj_set_size(obj, LV_PCT(100), 300);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(2), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(2), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // midi_music_name_label
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.midi_music_name_label = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
                            lv_obj_set_style_grid_cell_column_span(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "Bad Apple(Nodbody Mixed)");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj155 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "文件：");
                        }
                        {
                            // midi_music_path_label
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.midi_music_path_label = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
                            lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_span(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "/sd/midi/Abandon.mid");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj156 = obj;
                            lv_obj_set_pos(obj, -604, 62);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_row_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "BPM：");
                        }
                        {
                            // midi_music_bpm_num
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.midi_music_bpm_num = obj;
                            lv_obj_set_pos(obj, -921, -44);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_row_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "3");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj157 = obj;
                            lv_obj_set_pos(obj, -604, 62);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_row_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "轨道数量: ");
                        }
                        {
                            // midi_music_track_count
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.midi_music_track_count = obj;
                            lv_obj_set_pos(obj, -214, 77);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_row_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "1");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    lv_obj_set_pos(obj, -342, 26);
                    lv_obj_set_size(obj, LV_PCT(100), 130);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_flex_main_place(obj, LV_FLEX_ALIGN_SPACE_AROUND, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // midi_prev
                            lv_obj_t *obj = lv_button_create(parent_obj);
                            objects.midi_prev = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 100, 100);
                            lv_obj_set_style_radius(obj, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    objects.obj164 = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text(obj, "");
                                }
                            }
                        }
                        {
                            // midi_play_stop
                            lv_obj_t *obj = lv_button_create(parent_obj);
                            objects.midi_play_stop = obj;
                            lv_obj_set_pos(obj, -473, -135);
                            lv_obj_set_size(obj, 100, 100);
                            lv_obj_set_style_radius(obj, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    // midi_play_stop_label
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    objects.midi_play_stop_label = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "");
                                }
                            }
                        }
                        {
                            // midi_next
                            lv_obj_t *obj = lv_button_create(parent_obj);
                            objects.midi_next = obj;
                            lv_obj_set_pos(obj, -517, -135);
                            lv_obj_set_size(obj, 100, 100);
                            lv_obj_set_style_radius(obj, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    objects.obj165 = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text(obj, "");
                                }
                            }
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj158 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(100), 100);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(2), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // midi_progress
                            lv_obj_t *obj = lv_slider_create(parent_obj);
                            objects.midi_progress = obj;
                            lv_obj_set_pos(obj, 124, 320);
                            lv_obj_set_size(obj, LV_PCT(100), 20);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_opa(obj, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_span(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(0xf8f8f8), LV_PART_KNOB | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 2, LV_PART_KNOB | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_KNOB | LV_STATE_DEFAULT);
                        }
                        {
                            // midi_play_time_now
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.midi_play_time_now = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "00:25");
                        }
                        {
                            // midi_play_time_total
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.midi_play_time_total = obj;
                            lv_obj_set_pos(obj, -114, 109);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_END, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "03:55");
                        }
                    }
                }
            }
        }
        {
            // midi_set
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.midi_set = obj;
            lv_obj_set_pos(obj, 128, 150);
            lv_obj_set_size(obj, LV_PCT(80), LV_PCT(70));
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_row(obj, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // midi_set_btn_return
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.midi_set_btn_return = obj;
                    lv_obj_set_pos(obj, 20, 20);
                    lv_obj_set_size(obj, 100, 60);
                    lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj166 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj159 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(100), 60);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "播放内容：");
                        }
                        {
                            // midi_play_type
                            lv_obj_t *obj = lv_dropdown_create(parent_obj);
                            objects.midi_play_type = obj;
                            lv_obj_set_pos(obj, 994, 360);
                            lv_obj_set_size(obj, 229, 50);
                            lv_dropdown_set_options_static(obj, "Midi音乐\n录音文件");
                            lv_dropdown_set_selected(obj, 0);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                                    objects.obj160 = obj;
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
                                }
                            }
                        }
                    }
                }
            }
        }
        {
            // midi_led_ai
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.midi_led_ai = obj;
            lv_obj_set_pos(obj, 626, 670);
            lv_obj_set_size(obj, 40, 35);
            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]));
            lv_led_set_brightness(obj, 255);
        }
    }
    
    tick_screen_app_midi_player();
}

void delete_screen_app_midi_player() {
    lv_obj_delete(objects.app_midi_player);
    objects.app_midi_player = 0;
    objects.obj151 = 0;
    objects.obj152 = 0;
    objects.obj161 = 0;
    objects.obj162 = 0;
    objects.midi_btn_home = 0;
    objects.midi_btn_set = 0;
    objects.obj163 = 0;
    objects.midi_panel_mid_list = 0;
    objects.midi_list_music_file = 0;
    objects.midi_file_example = 0;
    objects.midi_panel_hmr_list = 0;
    objects.midi_list_record_file = 0;
    objects.obj153 = 0;
    objects.obj154 = 0;
    objects.midi_music_name_label = 0;
    objects.obj155 = 0;
    objects.midi_music_path_label = 0;
    objects.obj156 = 0;
    objects.midi_music_bpm_num = 0;
    objects.obj157 = 0;
    objects.midi_music_track_count = 0;
    objects.midi_prev = 0;
    objects.obj164 = 0;
    objects.midi_play_stop = 0;
    objects.midi_play_stop_label = 0;
    objects.midi_next = 0;
    objects.obj165 = 0;
    objects.obj158 = 0;
    objects.midi_progress = 0;
    objects.midi_play_time_now = 0;
    objects.midi_play_time_total = 0;
    objects.midi_set = 0;
    objects.midi_set_btn_return = 0;
    objects.obj166 = 0;
    objects.obj159 = 0;
    objects.midi_play_type = 0;
    objects.obj160 = 0;
    objects.midi_led_ai = 0;
    deletePageFlowState(11);
}

void tick_screen_app_midi_player() {
    void *flowState = getFlowState(0, 11);
    (void)flowState;
    {
        const char *new_val = evalTextProperty(flowState, 2, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj152);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj152;
            lv_label_set_text(objects.obj152, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 3, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj161);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj161;
            lv_label_set_text(objects.obj161, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 4, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj162);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj162;
            lv_label_set_text(objects.obj162, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 8, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj163);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj163;
            lv_label_set_text(objects.obj163, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 28, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj164);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj164;
            lv_label_set_text(objects.obj164, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 32, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj165);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj165;
            lv_label_set_text(objects.obj165, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 39, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj166);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj166;
            lv_label_set_text(objects.obj166, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

void create_screen_app_xy_mode() {
    void *flowState = getFlowState(0, 12);
    (void)flowState;
    lv_obj_t *obj = lv_obj_create(0);
    objects.app_xy_mode = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_line_create(parent_obj);
            objects.obj167 = obj;
            lv_obj_set_pos(obj, 40, 410);
            lv_obj_set_size(obj, 1200, LV_SIZE_CONTENT);
            static lv_point_precise_t line_points[] = {
                { 0, 0 },
                { 1200, 0 }
            };
            lv_line_set_points(obj, line_points, 2);
            lv_obj_set_style_line_width(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_line_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_line_rounded(obj, true, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            lv_obj_t *obj = lv_line_create(parent_obj);
            objects.obj168 = obj;
            lv_obj_set_pos(obj, 320, 160);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            static lv_point_precise_t line_points[] = {
                { 0, 0 },
                { 0, 500 }
            };
            lv_line_set_points(obj, line_points, 2);
            lv_obj_set_style_line_width(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_line_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_line_rounded(obj, true, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            lv_obj_t *obj = lv_line_create(parent_obj);
            objects.obj169 = obj;
            lv_obj_set_pos(obj, 640, 160);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            static lv_point_precise_t line_points[] = {
                { 0, 0 },
                { 0, 500 }
            };
            lv_line_set_points(obj, line_points, 2);
            lv_obj_set_style_line_width(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_line_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_line_rounded(obj, true, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            lv_obj_t *obj = lv_line_create(parent_obj);
            objects.obj170 = obj;
            lv_obj_set_pos(obj, 960, 160);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            static lv_point_precise_t line_points[] = {
                { 0, 0 },
                { 0, 500 }
            };
            lv_line_set_points(obj, line_points, 2);
            lv_obj_set_style_line_width(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_line_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_line_rounded(obj, true, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj171 = obj;
            lv_obj_set_pos(obj, 440, 10);
            lv_obj_set_size(obj, 400, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "X-Y模式");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj172 = obj;
            lv_obj_set_pos(obj, 320, 50);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj178 = obj;
            lv_obj_set_pos(obj, LV_PCT(50), 10);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj179 = obj;
            lv_obj_set_pos(obj, 10, 10);
            lv_obj_set_size(obj, 200, LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_label_set_text(obj, "");
        }
        {
            // xy_btn_home
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.xy_btn_home = obj;
            lv_obj_set_pos(obj, 30, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
            }
        }
        {
            // xy_btn_rec
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.xy_btn_rec = obj;
            lv_obj_set_pos(obj, 150, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][8]), LV_PART_MAIN | LV_STATE_CHECKED);
            lv_obj_set_style_bg_opa(obj, 100, LV_PART_MAIN | LV_STATE_CHECKED);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj173 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][8]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
            }
        }
        {
            // xy_btn_set
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.xy_btn_set = obj;
            lv_obj_set_pos(obj, 1150, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj180 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
            }
        }
        {
            // xy_point_1
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.xy_point_1 = obj;
            lv_obj_set_pos(obj, 85, 630);
            lv_obj_set_size(obj, 50, 50);
            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]));
            lv_led_set_brightness(obj, 255);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
        }
        {
            // xy_point_2
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.xy_point_2 = obj;
            lv_obj_set_pos(obj, 135, 630);
            lv_obj_set_size(obj, 50, 50);
            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][13]));
            lv_led_set_brightness(obj, 255);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
        }
        {
            // xy_point_3
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.xy_point_3 = obj;
            lv_obj_set_pos(obj, 35, 630);
            lv_obj_set_size(obj, 50, 50);
            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][14]));
            lv_led_set_brightness(obj, 255);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
        }
        {
            // xy_set
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.xy_set = obj;
            lv_obj_set_pos(obj, 128, 150);
            lv_obj_set_size(obj, LV_PCT(80), LV_PCT(70));
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_CHAIN_VER|LV_OBJ_FLAG_SCROLL_ELASTIC);
            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_row(obj, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // xy_set_btn_return
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.xy_set_btn_return = obj;
                    lv_obj_set_pos(obj, 20, 20);
                    lv_obj_set_size(obj, 100, 60);
                    lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_x(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj181 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj174 = obj;
                    lv_obj_set_pos(obj, 41, 350);
                    lv_obj_set_size(obj, LV_PCT(100), 60);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "音色：");
                        }
                        {
                            // xy_sound
                            lv_obj_t *obj = lv_dropdown_create(parent_obj);
                            objects.xy_sound = obj;
                            lv_obj_set_pos(obj, 994, 360);
                            lv_obj_set_size(obj, 229, 50);
                            lv_dropdown_set_options_static(obj, "40-小提琴\n41-中提琴\n42-大提琴\n43-低音提琴\n44-弦乐震音\n45-弦乐拨奏\n48-弦乐合奏1\n49-弦乐合奏2\n50-合成弦乐1\n51-合成弦乐2\n52-人声合唱Ah\n53-人声Oh\n54-合成人声\n56-小号\n57-长号\n58-大号\n59-弱音小号\n60-圆号\n61-铜管合奏\n62-合成铜管1\n63-合成铜管2\n64-高音萨克斯\n65-中音萨克斯\n66-次中音萨克斯\n67-上低音萨克斯\n68-双簧管\n69-英国管\n70-大管\n71-单簧管");
                            lv_dropdown_set_selected(obj, 0);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                                    objects.obj175 = obj;
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
                                }
                            }
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj176 = obj;
                    lv_obj_set_pos(obj, 41, 350);
                    lv_obj_set_size(obj, LV_PCT(100), 60);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "步长：");
                        }
                        {
                            // xy_step
                            lv_obj_t *obj = lv_dropdown_create(parent_obj);
                            objects.xy_step = obj;
                            lv_obj_set_pos(obj, 994, 360);
                            lv_obj_set_size(obj, 229, 50);
                            lv_dropdown_set_options_static(obj, "乐器\n线性");
                            lv_dropdown_set_selected(obj, 0);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                                    objects.obj177 = obj;
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
                                }
                            }
                        }
                    }
                }
            }
        }
        {
            // xy_led_ai
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.xy_led_ai = obj;
            lv_obj_set_pos(obj, 626, 670);
            lv_obj_set_size(obj, 40, 35);
            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]));
            lv_led_set_brightness(obj, 255);
        }
    }
    
    tick_screen_app_xy_mode();
}

void delete_screen_app_xy_mode() {
    lv_obj_delete(objects.app_xy_mode);
    objects.app_xy_mode = 0;
    objects.obj167 = 0;
    objects.obj168 = 0;
    objects.obj169 = 0;
    objects.obj170 = 0;
    objects.obj171 = 0;
    objects.obj172 = 0;
    objects.obj178 = 0;
    objects.obj179 = 0;
    objects.xy_btn_home = 0;
    objects.xy_btn_rec = 0;
    objects.obj173 = 0;
    objects.xy_btn_set = 0;
    objects.obj180 = 0;
    objects.xy_point_1 = 0;
    objects.xy_point_2 = 0;
    objects.xy_point_3 = 0;
    objects.xy_set = 0;
    objects.xy_set_btn_return = 0;
    objects.obj181 = 0;
    objects.obj174 = 0;
    objects.xy_sound = 0;
    objects.obj175 = 0;
    objects.obj176 = 0;
    objects.xy_step = 0;
    objects.obj177 = 0;
    objects.xy_led_ai = 0;
    deletePageFlowState(12);
}

void tick_screen_app_xy_mode() {
    void *flowState = getFlowState(0, 12);
    (void)flowState;
    {
        const char *new_val = evalTextProperty(flowState, 6, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj172);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj172;
            lv_label_set_text(objects.obj172, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 7, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj178);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj178;
            lv_label_set_text(objects.obj178, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 8, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj179);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj179;
            lv_label_set_text(objects.obj179, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 14, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj180);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj180;
            lv_label_set_text(objects.obj180, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 20, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj181);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj181;
            lv_label_set_text(objects.obj181, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

void create_screen_app_metronome() {
    void *flowState = getFlowState(0, 13);
    (void)flowState;
    lv_obj_t *obj = lv_obj_create(0);
    objects.app_metronome = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj182 = obj;
            lv_obj_set_pos(obj, 440, 10);
            lv_obj_set_size(obj, 400, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "节拍器");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj183 = obj;
            lv_obj_set_pos(obj, 320, 50);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj191 = obj;
            lv_obj_set_pos(obj, LV_PCT(50), 10);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj192 = obj;
            lv_obj_set_pos(obj, 10, 10);
            lv_obj_set_size(obj, 200, LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_label_set_text(obj, "");
        }
        {
            // metron_btn_home
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.metron_btn_home = obj;
            lv_obj_set_pos(obj, 30, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
            }
        }
        {
            // metron_btn_set
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.metron_btn_set = obj;
            lv_obj_set_pos(obj, 1150, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj193 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
            }
        }
        {
            // metron_panel
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.metron_panel = obj;
            lv_obj_set_pos(obj, 32, 110);
            lv_obj_set_size(obj, LV_PCT(95), LV_PCT(80));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                static lv_coord_t dsc[] = {LV_GRID_FR(2), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            {
                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // metron_btn_minus
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.metron_btn_minus = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 150, 140);
                    lv_obj_set_style_text_font(obj, ui_font_icon_70, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "-1");
                        }
                    }
                }
                {
                    // metron_label_bpm
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.metron_label_bpm = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_font(obj, ui_font_icon_70, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_letter_space(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "210");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj184 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_font(obj, ui_font_icon_70, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_letter_space(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "BPM");
                }
                {
                    // metron_btn_plus
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.metron_btn_plus = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 150, 140);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "+1");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj185 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 300, 200);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_span(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_ROW, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_flex_main_place(obj, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_column(obj, 22, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // metron_led_heavy
                            lv_obj_t *obj = lv_led_create(parent_obj);
                            objects.metron_led_heavy = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 50, 50);
                            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][8]));
                            lv_led_set_brightness(obj, 255);
                        }
                        {
                            // metron_led_1
                            lv_obj_t *obj = lv_led_create(parent_obj);
                            objects.metron_led_1 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 50, 50);
                            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][7]));
                            lv_led_set_brightness(obj, 255);
                        }
                        {
                            // metron_led_2
                            lv_obj_t *obj = lv_led_create(parent_obj);
                            objects.metron_led_2 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 50, 50);
                            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][7]));
                            lv_led_set_brightness(obj, 0);
                        }
                        {
                            // metron_led_3
                            lv_obj_t *obj = lv_led_create(parent_obj);
                            objects.metron_led_3 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 50, 50);
                            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][7]));
                            lv_led_set_brightness(obj, 0);
                        }
                        {
                            // metron_led_4
                            lv_obj_t *obj = lv_led_create(parent_obj);
                            objects.metron_led_4 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 50, 50);
                            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][7]));
                            lv_led_set_brightness(obj, 0);
                            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                        }
                        {
                            // metron_led_5
                            lv_obj_t *obj = lv_led_create(parent_obj);
                            objects.metron_led_5 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 50, 50);
                            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][7]));
                            lv_led_set_brightness(obj, 0);
                            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                        }
                        {
                            // metron_led_6
                            lv_obj_t *obj = lv_led_create(parent_obj);
                            objects.metron_led_6 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 50, 50);
                            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][7]));
                            lv_led_set_brightness(obj, 0);
                            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                        }
                        {
                            // metron_led_7
                            lv_obj_t *obj = lv_led_create(parent_obj);
                            objects.metron_led_7 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 50, 50);
                            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][7]));
                            lv_led_set_brightness(obj, 0);
                            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                        }
                        {
                            // metron_led_8
                            lv_obj_t *obj = lv_led_create(parent_obj);
                            objects.metron_led_8 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 50, 50);
                            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][7]));
                            lv_led_set_brightness(obj, 0);
                            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                        }
                        {
                            // metron_led_9
                            lv_obj_t *obj = lv_led_create(parent_obj);
                            objects.metron_led_9 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 50, 50);
                            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][7]));
                            lv_led_set_brightness(obj, 0);
                            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                        }
                        {
                            // metron_led_10
                            lv_obj_t *obj = lv_led_create(parent_obj);
                            objects.metron_led_10 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 50, 50);
                            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][7]));
                            lv_led_set_brightness(obj, 0);
                            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                        }
                        {
                            // metron_led_11
                            lv_obj_t *obj = lv_led_create(parent_obj);
                            objects.metron_led_11 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 50, 50);
                            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][7]));
                            lv_led_set_brightness(obj, 0);
                            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                        }
                        {
                            // metron_led_12
                            lv_obj_t *obj = lv_led_create(parent_obj);
                            objects.metron_led_12 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 50, 50);
                            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][7]));
                            lv_led_set_brightness(obj, 0);
                            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                        }
                        {
                            // metron_led_13
                            lv_obj_t *obj = lv_led_create(parent_obj);
                            objects.metron_led_13 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 50, 50);
                            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][7]));
                            lv_led_set_brightness(obj, 0);
                            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                        }
                        {
                            // metron_led_14
                            lv_obj_t *obj = lv_led_create(parent_obj);
                            objects.metron_led_14 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 50, 50);
                            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][7]));
                            lv_led_set_brightness(obj, 0);
                            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                        }
                        {
                            // metron_led_15
                            lv_obj_t *obj = lv_led_create(parent_obj);
                            objects.metron_led_15 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 50, 50);
                            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][7]));
                            lv_led_set_brightness(obj, 0);
                            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj186 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "拍号:");
                }
                {
                    // metron_timesig_top
                    lv_obj_t *obj = lv_dropdown_create(parent_obj);
                    objects.metron_timesig_top = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(100), 50);
                    lv_dropdown_set_options_static(obj, "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15\n16");
                    lv_dropdown_set_selected(obj, 3);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                            objects.obj187 = obj;
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
                        }
                    }
                }
                {
                    // metron_label_timesig
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.metron_label_timesig = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "4/4");
                }
                {
                    // metron_timesig_bot
                    lv_obj_t *obj = lv_dropdown_create(parent_obj);
                    objects.metron_timesig_bot = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(100), 50);
                    lv_dropdown_set_options_static(obj, "4\n6\n8\n16\n32");
                    lv_dropdown_set_selected(obj, 0);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                            objects.obj188 = obj;
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
                        }
                    }
                }
                {
                    // metron_btn_tempo
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.metron_btn_tempo = obj;
                    lv_obj_set_pos(obj, 595, 326);
                    lv_obj_set_size(obj, 200, 100);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, " Tap");
                        }
                    }
                }
                {
                    // metron_slider_bpm
                    lv_obj_t *obj = lv_slider_create(parent_obj);
                    objects.metron_slider_bpm = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, 30);
                    lv_slider_set_range(obj, 20, 300);
                    lv_slider_set_value(obj, 210, LV_ANIM_ON);
                    lv_obj_set_style_grid_cell_column_span(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0xf8f8f8), LV_PART_KNOB | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 2, LV_PART_KNOB | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_KNOB | LV_STATE_DEFAULT);
                }
                {
                    // metron_btn_play_stop
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.metron_btn_play_stop = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 250, 100);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_icon_70, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_span(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_row_pos(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_x(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // metron_btn_play_stop_label
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.metron_btn_play_stop_label = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "");
                        }
                    }
                }
            }
        }
        {
            // metron_set
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.metron_set = obj;
            lv_obj_set_pos(obj, 128, 150);
            lv_obj_set_size(obj, LV_PCT(80), LV_PCT(70));
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // metron_set_btn_return
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.metron_set_btn_return = obj;
                    lv_obj_set_pos(obj, 20, 20);
                    lv_obj_set_size(obj, 100, 60);
                    lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj194 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj189 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(100), 60);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "节拍音色：");
                        }
                        {
                            // metron_sound
                            lv_obj_t *obj = lv_dropdown_create(parent_obj);
                            objects.metron_sound = obj;
                            lv_obj_set_pos(obj, 994, 360);
                            lv_obj_set_size(obj, 229, 50);
                            lv_dropdown_set_options_static(obj, "标准\n指针式\n木鱼式\n鼓组式\n打击乐式\n体感式");
                            lv_dropdown_set_selected(obj, 0);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                                    objects.obj190 = obj;
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
                                }
                            }
                        }
                    }
                }
            }
        }
        {
            // metron_led_ai
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.metron_led_ai = obj;
            lv_obj_set_pos(obj, 626, 670);
            lv_obj_set_size(obj, 40, 35);
            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]));
            lv_led_set_brightness(obj, 255);
        }
    }
    
    tick_screen_app_metronome();
}

void delete_screen_app_metronome() {
    lv_obj_delete(objects.app_metronome);
    objects.app_metronome = 0;
    objects.obj182 = 0;
    objects.obj183 = 0;
    objects.obj191 = 0;
    objects.obj192 = 0;
    objects.metron_btn_home = 0;
    objects.metron_btn_set = 0;
    objects.obj193 = 0;
    objects.metron_panel = 0;
    objects.metron_btn_minus = 0;
    objects.metron_label_bpm = 0;
    objects.obj184 = 0;
    objects.metron_btn_plus = 0;
    objects.obj185 = 0;
    objects.metron_led_heavy = 0;
    objects.metron_led_1 = 0;
    objects.metron_led_2 = 0;
    objects.metron_led_3 = 0;
    objects.metron_led_4 = 0;
    objects.metron_led_5 = 0;
    objects.metron_led_6 = 0;
    objects.metron_led_7 = 0;
    objects.metron_led_8 = 0;
    objects.metron_led_9 = 0;
    objects.metron_led_10 = 0;
    objects.metron_led_11 = 0;
    objects.metron_led_12 = 0;
    objects.metron_led_13 = 0;
    objects.metron_led_14 = 0;
    objects.metron_led_15 = 0;
    objects.obj186 = 0;
    objects.metron_timesig_top = 0;
    objects.obj187 = 0;
    objects.metron_label_timesig = 0;
    objects.metron_timesig_bot = 0;
    objects.obj188 = 0;
    objects.metron_btn_tempo = 0;
    objects.metron_slider_bpm = 0;
    objects.metron_btn_play_stop = 0;
    objects.metron_btn_play_stop_label = 0;
    objects.metron_set = 0;
    objects.metron_set_btn_return = 0;
    objects.obj194 = 0;
    objects.obj189 = 0;
    objects.metron_sound = 0;
    objects.obj190 = 0;
    objects.metron_led_ai = 0;
    deletePageFlowState(13);
}

void tick_screen_app_metronome() {
    void *flowState = getFlowState(0, 13);
    (void)flowState;
    {
        const char *new_val = evalTextProperty(flowState, 2, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj183);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj183;
            lv_label_set_text(objects.obj183, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 3, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj191);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj191;
            lv_label_set_text(objects.obj191, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 4, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj192);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj192;
            lv_label_set_text(objects.obj192, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 8, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj193);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj193;
            lv_label_set_text(objects.obj193, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 46, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj194);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj194;
            lv_label_set_text(objects.obj194, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

void create_screen_app_ai_agent() {
    void *flowState = getFlowState(0, 14);
    (void)flowState;
    lv_obj_t *obj = lv_obj_create(0);
    objects.app_ai_agent = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj195 = obj;
            lv_obj_set_pos(obj, 440, 10);
            lv_obj_set_size(obj, 400, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "AI 导师");
        }
        {
            // ai_btn_home
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.ai_btn_home = obj;
            lv_obj_set_pos(obj, 30, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj202 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
            }
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj196 = obj;
            lv_obj_set_pos(obj, 320, 50);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj203 = obj;
            lv_obj_set_pos(obj, LV_PCT(50), 10);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj204 = obj;
            lv_obj_set_pos(obj, 10, 10);
            lv_obj_set_size(obj, 200, LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.obj197 = obj;
            lv_obj_set_pos(obj, 32, 110);
            lv_obj_set_size(obj, LV_PCT(80), LV_PCT(80));
            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_cross_place(obj, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_row(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 60, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // ai_context_user0
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.ai_context_user0 = obj;
                    lv_obj_set_pos(obj, 32, 110);
                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        static lv_coord_t dsc[] = {50, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {50, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 60, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 50, 50);
                            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "");
                        }
                        {
                            // ai_context_user0_text
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.ai_context_user0_text = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "Hi，喵喵。嗨黑哼 嘿哈呼呵咯啦嘞喽噜");
                        }
                    }
                }
                {
                    // ai_context_ai0
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.ai_context_ai0 = obj;
                    lv_obj_set_pos(obj, 32, 110);
                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        static lv_coord_t dsc[] = {50, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {50, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 60, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj198 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 50, 50);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "");
                        }
                        {
                            // ai_context_ai0_text
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.ai_context_ai0_text = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "你好，我是喵喵。你的设备还没有绑定，请用设备ID: %6d, 在 xiaozhi.me 完成绑定~");
                        }
                    }
                }
            }
        }
        {
            // ai_btn_set
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.ai_btn_set = obj;
            lv_obj_set_pos(obj, 1150, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj205 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
            }
        }
        {
            // ai_btn_speak
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.ai_btn_speak = obj;
            lv_obj_set_pos(obj, 1090, 536);
            lv_obj_set_size(obj, 150, 150);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
            }
        }
        {
            // ai_set
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.ai_set = obj;
            lv_obj_set_pos(obj, 128, 150);
            lv_obj_set_size(obj, LV_PCT(80), LV_PCT(70));
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_CLICK_FOCUSABLE|LV_OBJ_FLAG_GESTURE_BUBBLE);
            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_row(obj, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // ai_set_btn_return
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.ai_set_btn_return = obj;
                    lv_obj_set_pos(obj, 20, 20);
                    lv_obj_set_size(obj, 100, 60);
                    lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj206 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj199 = obj;
                    lv_obj_set_pos(obj, 41, 350);
                    lv_obj_set_size(obj, LV_PCT(100), 60);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "存储对话（需要使用SD卡）：");
                        }
                        {
                            // ai_switch_save_text
                            lv_obj_t *obj = lv_switch_create(parent_obj);
                            objects.ai_switch_save_text = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 100, 45);
                            lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_INDICATOR | LV_STATE_CHECKED);
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj200 = obj;
                    lv_obj_set_pos(obj, 41, 350);
                    lv_obj_set_size(obj, LV_PCT(100), 60);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "全局唤醒（需要先激活绑定）：");
                        }
                        {
                            // ai_switch_wake_anywhere
                            lv_obj_t *obj = lv_switch_create(parent_obj);
                            objects.ai_switch_wake_anywhere = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 100, 45);
                            lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_INDICATOR | LV_STATE_CHECKED);
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj201 = obj;
                    lv_obj_set_pos(obj, 41, 350);
                    lv_obj_set_size(obj, LV_PCT(100), 60);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "重置AI配置：");
                        }
                        {
                            // ai_btn_config_reset
                            lv_obj_t *obj = lv_button_create(parent_obj);
                            objects.ai_btn_config_reset = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 200, 50);
                            lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][8]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "长按重置");
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    tick_screen_app_ai_agent();
}

void delete_screen_app_ai_agent() {
    lv_obj_delete(objects.app_ai_agent);
    objects.app_ai_agent = 0;
    objects.obj195 = 0;
    objects.ai_btn_home = 0;
    objects.obj202 = 0;
    objects.obj196 = 0;
    objects.obj203 = 0;
    objects.obj204 = 0;
    objects.obj197 = 0;
    objects.ai_context_user0 = 0;
    objects.ai_context_user0_text = 0;
    objects.ai_context_ai0 = 0;
    objects.obj198 = 0;
    objects.ai_context_ai0_text = 0;
    objects.ai_btn_set = 0;
    objects.obj205 = 0;
    objects.ai_btn_speak = 0;
    objects.ai_set = 0;
    objects.ai_set_btn_return = 0;
    objects.obj206 = 0;
    objects.obj199 = 0;
    objects.ai_switch_save_text = 0;
    objects.obj200 = 0;
    objects.ai_switch_wake_anywhere = 0;
    objects.obj201 = 0;
    objects.ai_btn_config_reset = 0;
    deletePageFlowState(14);
}

void tick_screen_app_ai_agent() {
    void *flowState = getFlowState(0, 14);
    (void)flowState;
    {
        const char *new_val = evalTextProperty(flowState, 3, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj202);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj202;
            lv_label_set_text(objects.obj202, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 4, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj196);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj196;
            lv_label_set_text(objects.obj196, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 5, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj203);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj203;
            lv_label_set_text(objects.obj203, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 6, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj204);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj204;
            lv_label_set_text(objects.obj204, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 15, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj205);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj205;
            lv_label_set_text(objects.obj205, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 20, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj206);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj206;
            lv_label_set_text(objects.obj206, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

void create_screen_app_clock() {
    void *flowState = getFlowState(0, 15);
    (void)flowState;
    lv_obj_t *obj = lv_obj_create(0);
    objects.app_clock = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj207 = obj;
            lv_obj_set_pos(obj, 440, 10);
            lv_obj_set_size(obj, 400, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "日历&时钟");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj208 = obj;
            lv_obj_set_pos(obj, 320, 50);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj226 = obj;
            lv_obj_set_pos(obj, LV_PCT(50), 10);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj227 = obj;
            lv_obj_set_pos(obj, 10, 10);
            lv_obj_set_size(obj, 200, LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_label_set_text(obj, "");
        }
        {
            // clock_btn_home
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.clock_btn_home = obj;
            lv_obj_set_pos(obj, 30, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
            }
        }
        {
            // clock_btn_set
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.clock_btn_set = obj;
            lv_obj_set_pos(obj, 1150, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj228 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
            }
        }
        {
            // clock_panel_clock
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.clock_panel_clock = obj;
            lv_obj_set_pos(obj, 32, 110);
            lv_obj_set_size(obj, LV_PCT(95), LV_PCT(80));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_ROW, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_main_place(obj, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj209 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(60), LV_PCT(99));
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_flex_cross_place(obj, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_flex_main_place(obj, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_row(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // clock_clock_12h_label
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.clock_clock_12h_label = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "上午");
                        }
                        {
                            // clock_clock_bigtime
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.clock_clock_bigtime = obj;
                            lv_obj_set_pos(obj, 0, 233);
                            lv_obj_set_size(obj, LV_PCT(100), 180);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, ui_font_clock_150, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "12:34:56");
                        }
                        {
                            // clock_clock_date
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.clock_clock_date = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_PCT(80), LV_SIZE_CONTENT);
                            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "2026-07-21 星期五");
                        }
                        {
                            // clock_clock_lunar
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.clock_clock_lunar = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_PCT(80), LV_SIZE_CONTENT);
                            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "丙午年六月大 初八 · 大暑");
                        }
                        {
                            // clock_clock_hitokoto
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.clock_clock_hitokoto = obj;
                            lv_obj_set_pos(obj, 335, -7);
                            lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
                            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_pad_top(obj, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "https://v1.hitokoto.cn/?c=f&encode=text");
                        }
                    }
                }
                {
                    // clock_panel_weather
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.clock_panel_weather = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(39), LV_PCT(99));
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(3), LV_GRID_FR(2), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_obj_create(parent_obj);
                            objects.obj210 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_PCT(39), LV_PCT(99));
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                static lv_coord_t dsc[] = {LV_GRID_FR(2), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                                lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                            {
                                static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                                lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                            }
                            lv_obj_set_style_grid_cell_column_span(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_obj_create(parent_obj);
                                    objects.obj211 = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, 100, 100);
                                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_column_span(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    {
                                        lv_obj_t *parent_obj = obj;
                                        {
                                            // weather_today_text
                                            lv_obj_t *obj = lv_label_create(parent_obj);
                                            objects.weather_today_text = obj;
                                            lv_obj_set_pos(obj, 0, 0);
                                            lv_obj_set_size(obj, LV_PCT(100), 40);
                                            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
                                            lv_label_set_text_static(obj, "今天 · 星期二");
                                        }
                                        {
                                            // weather_today_text_1
                                            lv_obj_t *obj = lv_label_create(parent_obj);
                                            objects.weather_today_text_1 = obj;
                                            lv_obj_set_pos(obj, 0, 0);
                                            lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_label_set_text_static(obj, "curl -X GET 'https://uapis.cn/api/v1/misc/'");
                                        }
                                    }
                                }
                                {
                                    lv_obj_t *obj = lv_obj_create(parent_obj);
                                    objects.obj212 = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, 100, 100);
                                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    {
                                        lv_obj_t *parent_obj = obj;
                                        {
                                            lv_obj_t *obj = lv_label_create(parent_obj);
                                            objects.obj229 = obj;
                                            lv_obj_set_pos(obj, 0, 0);
                                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                            lv_label_set_text(obj, "");
                                        }
                                        {
                                            // weather_location
                                            lv_obj_t *obj = lv_label_create(parent_obj);
                                            objects.weather_location = obj;
                                            lv_obj_set_pos(obj, 0, 0);
                                            lv_obj_set_size(obj, LV_PCT(75), LV_SIZE_CONTENT);
                                            lv_label_set_text_static(obj, "萨卡班萨卡班");
                                        }
                                    }
                                }
                                {
                                    lv_obj_t *obj = lv_obj_create(parent_obj);
                                    objects.obj213 = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, 100, 100);
                                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                                    lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    {
                                        lv_obj_t *parent_obj = obj;
                                        {
                                            // weather_temp
                                            lv_obj_t *obj = lv_label_create(parent_obj);
                                            objects.weather_temp = obj;
                                            lv_obj_set_pos(obj, 0, 0);
                                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                            lv_label_set_text_static(obj, " 26℃");
                                        }
                                        {
                                            // weather_humi
                                            lv_obj_t *obj = lv_label_create(parent_obj);
                                            objects.weather_humi = obj;
                                            lv_obj_set_pos(obj, 0, 0);
                                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                            lv_label_set_text_static(obj, " 68%");
                                        }
                                    }
                                }
                            }
                        }
                        {
                            lv_obj_t *obj = lv_obj_create(parent_obj);
                            objects.obj214 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 100, 100);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
                                    lv_label_set_text_static(obj, "明天: ");
                                }
                                {
                                    // weather_day2
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    objects.weather_day2 = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "倾盆大雨");
                                }
                                {
                                    // weather_day2_1
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    objects.weather_day2_1 = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "倾盆大雨");
                                }
                            }
                        }
                        {
                            lv_obj_t *obj = lv_obj_create(parent_obj);
                            objects.obj215 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 100, 100);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_flex_cross_place(obj, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
                                    lv_label_set_text_static(obj, "后天: ");
                                }
                                {
                                    // weather_day3
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    objects.weather_day3 = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "鹅毛大雪鹅鹅鹅饿");
                                }
                                {
                                    // weather_day3_1
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    objects.weather_day3_1 = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "鹅毛大雪鹅鹅鹅饿");
                                }
                            }
                        }
                        {
                            // weatehr_panel_news
                            lv_obj_t *obj = lv_obj_create(parent_obj);
                            objects.weatehr_panel_news = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 100, 100);
                            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_span(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                    }
                }
            }
        }
        {
            // clock_panel_calender
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.clock_panel_calender = obj;
            lv_obj_set_pos(obj, 32, 110);
            lv_obj_set_size(obj, LV_PCT(95), LV_PCT(80));
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_ROW, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_main_place(obj, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // clock_calender
                    lv_obj_t *obj = lv_calendar_create(parent_obj);
                    objects.clock_calender = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(68), LV_PCT(99));
                    lv_calendar_add_header_dropdown(obj);
                    lv_calendar_set_today_date(obj, 2026, 7, 24);
                    lv_calendar_set_month_shown(obj, 2026, 7);
                    lv_calendar_set_chinese_mode(obj, true);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_ITEMS | LV_STATE_DEFAULT);
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj216 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(30), LV_PCT(99));
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_row(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_obj_create(parent_obj);
                            objects.obj217 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_PCT(100), LV_PCT(36));
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_flex_main_place(obj, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_pad_bottom(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    // clock_calender_today
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    objects.clock_calender_today = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "2026-01-08");
                                }
                                {
                                    // clock_calender_today_week
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    objects.clock_calender_today_week = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "20xx年第x周，星期九");
                                }
                                {
                                    // clock_calender_progress_bar
                                    lv_obj_t *obj = lv_bar_create(parent_obj);
                                    objects.clock_calender_progress_bar = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), 10);
                                    lv_bar_set_value(obj, 25, LV_ANIM_ON);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
                                }
                                {
                                    // clock_calender_progress_text
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    objects.clock_calender_progress_text = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
                                    lv_label_set_text_static(obj, "今年已过500天，剩余600天。");
                                }
                            }
                        }
                        {
                            // clock_calender_panel_huangli
                            lv_obj_t *obj = lv_obj_create(parent_obj);
                            objects.clock_calender_panel_huangli = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_PCT(100), LV_PCT(60));
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_pad_row(obj, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    // clock_calender_huangli_1
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    objects.clock_calender_huangli_1 = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
                                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "宜");
                                }
                                {
                                    // clock_calender_huangli_2
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    objects.clock_calender_huangli_2 = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
                                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "已过500天");
                                }
                                {
                                    // clock_calender_huangli_3
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    objects.clock_calender_huangli_3 = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
                                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "忌");
                                }
                                {
                                    // clock_calender_huangli_4
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    objects.clock_calender_huangli_4 = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
                                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "已过500天");
                                }
                                {
                                    // clock_calender_huangli_5
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    objects.clock_calender_huangli_5 = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
                                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "已过500天");
                                }
                                {
                                    // clock_calender_huangli_6
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    objects.clock_calender_huangli_6 = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
                                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "已过500天");
                                }
                            }
                        }
                    }
                }
            }
        }
        {
            // clock_panel_timer
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.clock_panel_timer = obj;
            lv_obj_set_pos(obj, 32, 110);
            lv_obj_set_size(obj, LV_PCT(95), LV_PCT(80));
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_ROW, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_main_place(obj, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj218 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(60), LV_PCT(99));
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_flex_cross_place(obj, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_flex_main_place(obj, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_row(obj, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // clock_timer_bell
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.clock_timer_bell = obj;
                            lv_obj_set_pos(obj, 0, 233);
                            lv_obj_set_size(obj, LV_PCT(100), LV_PCT(15));
                            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_pad_top(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "");
                        }
                        {
                            // clock_timer_pv
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.clock_timer_pv = obj;
                            lv_obj_set_pos(obj, 0, 233);
                            lv_obj_set_size(obj, LV_PCT(100), LV_PCT(40));
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, ui_font_clock_150, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "00:06:02");
                        }
                        {
                            lv_obj_t *obj = lv_obj_create(parent_obj);
                            objects.obj219 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_PCT(100), LV_PCT(30));
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_span(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_flex_main_place(obj, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_pad_column(obj, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_flex_cross_place(obj, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    // clock_timer_btn_min_1min
                                    lv_obj_t *obj = lv_button_create(parent_obj);
                                    objects.clock_timer_btn_min_1min = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(30), LV_PCT(100));
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    {
                                        lv_obj_t *parent_obj = obj;
                                        {
                                            lv_obj_t *obj = lv_label_create(parent_obj);
                                            lv_obj_set_pos(obj, 0, 0);
                                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_label_set_text_static(obj, "-1分钟");
                                        }
                                    }
                                }
                                {
                                    // clock_timer_sv
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    objects.clock_timer_sv = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "50min");
                                }
                                {
                                    // clock_timer_btn_add_1min
                                    lv_obj_t *obj = lv_button_create(parent_obj);
                                    objects.clock_timer_btn_add_1min = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(30), LV_PCT(100));
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    {
                                        lv_obj_t *parent_obj = obj;
                                        {
                                            lv_obj_t *obj = lv_label_create(parent_obj);
                                            lv_obj_set_pos(obj, 0, 0);
                                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_label_set_text_static(obj, "+1分钟");
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj220 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(39), LV_PCT(99));
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(2), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_obj_create(parent_obj);
                            objects.obj221 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_PCT(39), LV_PCT(99));
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_span(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    // clock_timer_quick_1min
                                    lv_obj_t *obj = lv_button_create(parent_obj);
                                    objects.clock_timer_quick_1min = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), 100);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    {
                                        lv_obj_t *parent_obj = obj;
                                        {
                                            lv_obj_t *obj = lv_label_create(parent_obj);
                                            lv_obj_set_pos(obj, 0, 0);
                                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_label_set_text_static(obj, "1分钟");
                                        }
                                    }
                                }
                                {
                                    // clock_timer_quick_3min
                                    lv_obj_t *obj = lv_button_create(parent_obj);
                                    objects.clock_timer_quick_3min = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), 100);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    {
                                        lv_obj_t *parent_obj = obj;
                                        {
                                            lv_obj_t *obj = lv_label_create(parent_obj);
                                            lv_obj_set_pos(obj, 0, 0);
                                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_label_set_text_static(obj, "3分钟");
                                        }
                                    }
                                }
                                {
                                    // clock_timer_quick_5min
                                    lv_obj_t *obj = lv_button_create(parent_obj);
                                    objects.clock_timer_quick_5min = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), 100);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    {
                                        lv_obj_t *parent_obj = obj;
                                        {
                                            lv_obj_t *obj = lv_label_create(parent_obj);
                                            lv_obj_set_pos(obj, 0, 0);
                                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_label_set_text_static(obj, "5分钟");
                                        }
                                    }
                                }
                                {
                                    // clock_timer_quick_10min
                                    lv_obj_t *obj = lv_button_create(parent_obj);
                                    objects.clock_timer_quick_10min = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), 100);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    {
                                        lv_obj_t *parent_obj = obj;
                                        {
                                            lv_obj_t *obj = lv_label_create(parent_obj);
                                            lv_obj_set_pos(obj, 0, 0);
                                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_label_set_text_static(obj, "10分钟");
                                        }
                                    }
                                }
                                {
                                    // clock_timer_quick_20min
                                    lv_obj_t *obj = lv_button_create(parent_obj);
                                    objects.clock_timer_quick_20min = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), 100);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    {
                                        lv_obj_t *parent_obj = obj;
                                        {
                                            lv_obj_t *obj = lv_label_create(parent_obj);
                                            lv_obj_set_pos(obj, 0, 0);
                                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_label_set_text_static(obj, "20分钟");
                                        }
                                    }
                                }
                                {
                                    // clock_timer_quick_30min
                                    lv_obj_t *obj = lv_button_create(parent_obj);
                                    objects.clock_timer_quick_30min = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), 100);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    {
                                        lv_obj_t *parent_obj = obj;
                                        {
                                            lv_obj_t *obj = lv_label_create(parent_obj);
                                            lv_obj_set_pos(obj, 0, 0);
                                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_label_set_text_static(obj, "30分钟");
                                        }
                                    }
                                }
                                {
                                    // clock_timer_quick_40min
                                    lv_obj_t *obj = lv_button_create(parent_obj);
                                    objects.clock_timer_quick_40min = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), 100);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    {
                                        lv_obj_t *parent_obj = obj;
                                        {
                                            lv_obj_t *obj = lv_label_create(parent_obj);
                                            lv_obj_set_pos(obj, 0, 0);
                                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_label_set_text_static(obj, "40分钟");
                                        }
                                    }
                                }
                                {
                                    // clock_timer_quick_50min
                                    lv_obj_t *obj = lv_button_create(parent_obj);
                                    objects.clock_timer_quick_50min = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), 100);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    {
                                        lv_obj_t *parent_obj = obj;
                                        {
                                            lv_obj_t *obj = lv_label_create(parent_obj);
                                            lv_obj_set_pos(obj, 0, 0);
                                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_label_set_text_static(obj, "50分钟");
                                        }
                                    }
                                }
                                {
                                    // clock_timer_quick_60min
                                    lv_obj_t *obj = lv_button_create(parent_obj);
                                    objects.clock_timer_quick_60min = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), 100);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    {
                                        lv_obj_t *parent_obj = obj;
                                        {
                                            lv_obj_t *obj = lv_label_create(parent_obj);
                                            lv_obj_set_pos(obj, 0, 0);
                                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                            lv_label_set_text_static(obj, "60分钟");
                                        }
                                    }
                                }
                            }
                        }
                        {
                            // clock_timer_reset
                            lv_obj_t *obj = lv_button_create(parent_obj);
                            objects.clock_timer_reset = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_PCT(100), 100);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "重置");
                                }
                            }
                        }
                        {
                            // clock_timer_start_pause
                            lv_obj_t *obj = lv_button_create(parent_obj);
                            objects.clock_timer_start_pause = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_PCT(100), 100);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_row_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "开始");
                                }
                            }
                        }
                    }
                }
            }
        }
        {
            // clock_set
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.clock_set = obj;
            lv_obj_set_pos(obj, 128, 150);
            lv_obj_set_size(obj, LV_PCT(80), LV_PCT(70));
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_row(obj, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // clock_set_btn_return
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.clock_set_btn_return = obj;
                    lv_obj_set_pos(obj, 20, 20);
                    lv_obj_set_size(obj, 100, 60);
                    lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_x(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj230 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj222 = obj;
                    lv_obj_set_pos(obj, 41, 350);
                    lv_obj_set_size(obj, LV_PCT(100), 60);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "时间显示：");
                        }
                        {
                            // clock_set_12_24h
                            lv_obj_t *obj = lv_dropdown_create(parent_obj);
                            objects.clock_set_12_24h = obj;
                            lv_obj_set_pos(obj, 994, 360);
                            lv_obj_set_size(obj, 229, 50);
                            lv_dropdown_set_options_static(obj, "24小时\n12小时");
                            lv_dropdown_set_selected(obj, 0);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                                    objects.obj223 = obj;
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
                                }
                            }
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj224 = obj;
                    lv_obj_set_pos(obj, 41, 350);
                    lv_obj_set_size(obj, LV_PCT(100), 60);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_row_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    {
                        static lv_coord_t dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
                        lv_obj_set_style_grid_column_dsc_array(obj, dsc, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    lv_obj_set_style_layout(obj, LV_LAYOUT_GRID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "时间数字：");
                        }
                        {
                            // clock_set_time_font
                            lv_obj_t *obj = lv_dropdown_create(parent_obj);
                            objects.clock_set_time_font = obj;
                            lv_obj_set_pos(obj, 994, 360);
                            lv_obj_set_size(obj, 229, 50);
                            lv_dropdown_set_options_static(obj, "字体1\n字体2");
                            lv_dropdown_set_selected(obj, 0);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 3, LV_PART_SELECTED | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_column_pos(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_grid_cell_x_align(obj, LV_GRID_ALIGN_STRETCH, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_dropdown_get_list(parent_obj);
                                    objects.obj225 = obj;
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
                                }
                            }
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_flex_main_place(obj, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_column(obj, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // clock_btn_clock
                            lv_obj_t *obj = lv_button_create(parent_obj);
                            objects.clock_btn_clock = obj;
                            lv_obj_set_pos(obj, -255, 323);
                            lv_obj_set_size(obj, 200, 130);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_MAIN | LV_STATE_PRESSED);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "时钟");
                                }
                            }
                        }
                        {
                            // clock_btn_calender
                            lv_obj_t *obj = lv_button_create(parent_obj);
                            objects.clock_btn_calender = obj;
                            lv_obj_set_pos(obj, -20, 1);
                            lv_obj_set_size(obj, 200, 130);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_MAIN | LV_STATE_PRESSED);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "万年历");
                                }
                            }
                        }
                        {
                            // clock_btn_timer
                            lv_obj_t *obj = lv_button_create(parent_obj);
                            objects.clock_btn_timer = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 200, 130);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_MAIN | LV_STATE_PRESSED);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "计时器");
                                }
                            }
                        }
                    }
                }
            }
        }
        {
            // clock_led_ai
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.clock_led_ai = obj;
            lv_obj_set_pos(obj, 626, 670);
            lv_obj_set_size(obj, 40, 35);
            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]));
            lv_led_set_brightness(obj, 255);
        }
    }
    
    tick_screen_app_clock();
}

void delete_screen_app_clock() {
    lv_obj_delete(objects.app_clock);
    objects.app_clock = 0;
    objects.obj207 = 0;
    objects.obj208 = 0;
    objects.obj226 = 0;
    objects.obj227 = 0;
    objects.clock_btn_home = 0;
    objects.clock_btn_set = 0;
    objects.obj228 = 0;
    objects.clock_panel_clock = 0;
    objects.obj209 = 0;
    objects.clock_clock_12h_label = 0;
    objects.clock_clock_bigtime = 0;
    objects.clock_clock_date = 0;
    objects.clock_clock_lunar = 0;
    objects.clock_clock_hitokoto = 0;
    objects.clock_panel_weather = 0;
    objects.obj210 = 0;
    objects.obj211 = 0;
    objects.weather_today_text = 0;
    objects.weather_today_text_1 = 0;
    objects.obj212 = 0;
    objects.obj229 = 0;
    objects.weather_location = 0;
    objects.obj213 = 0;
    objects.weather_temp = 0;
    objects.weather_humi = 0;
    objects.obj214 = 0;
    objects.weather_day2 = 0;
    objects.weather_day2_1 = 0;
    objects.obj215 = 0;
    objects.weather_day3 = 0;
    objects.weather_day3_1 = 0;
    objects.weatehr_panel_news = 0;
    objects.clock_panel_calender = 0;
    objects.clock_calender = 0;
    objects.obj216 = 0;
    objects.obj217 = 0;
    objects.clock_calender_today = 0;
    objects.clock_calender_today_week = 0;
    objects.clock_calender_progress_bar = 0;
    objects.clock_calender_progress_text = 0;
    objects.clock_calender_panel_huangli = 0;
    objects.clock_calender_huangli_1 = 0;
    objects.clock_calender_huangli_2 = 0;
    objects.clock_calender_huangli_3 = 0;
    objects.clock_calender_huangli_4 = 0;
    objects.clock_calender_huangli_5 = 0;
    objects.clock_calender_huangli_6 = 0;
    objects.clock_panel_timer = 0;
    objects.obj218 = 0;
    objects.clock_timer_bell = 0;
    objects.clock_timer_pv = 0;
    objects.obj219 = 0;
    objects.clock_timer_btn_min_1min = 0;
    objects.clock_timer_sv = 0;
    objects.clock_timer_btn_add_1min = 0;
    objects.obj220 = 0;
    objects.obj221 = 0;
    objects.clock_timer_quick_1min = 0;
    objects.clock_timer_quick_3min = 0;
    objects.clock_timer_quick_5min = 0;
    objects.clock_timer_quick_10min = 0;
    objects.clock_timer_quick_20min = 0;
    objects.clock_timer_quick_30min = 0;
    objects.clock_timer_quick_40min = 0;
    objects.clock_timer_quick_50min = 0;
    objects.clock_timer_quick_60min = 0;
    objects.clock_timer_reset = 0;
    objects.clock_timer_start_pause = 0;
    objects.clock_set = 0;
    objects.clock_set_btn_return = 0;
    objects.obj230 = 0;
    objects.obj222 = 0;
    objects.clock_set_12_24h = 0;
    objects.obj223 = 0;
    objects.obj224 = 0;
    objects.clock_set_time_font = 0;
    objects.obj225 = 0;
    objects.clock_btn_clock = 0;
    objects.clock_btn_calender = 0;
    objects.clock_btn_timer = 0;
    objects.clock_led_ai = 0;
    deletePageFlowState(15);
}

void tick_screen_app_clock() {
    void *flowState = getFlowState(0, 15);
    (void)flowState;
    {
        const char *new_val = evalTextProperty(flowState, 2, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj208);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj208;
            lv_label_set_text(objects.obj208, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 3, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj226);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj226;
            lv_label_set_text(objects.obj226, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 4, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj227);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj227;
            lv_label_set_text(objects.obj227, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 8, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj228);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj228;
            lv_label_set_text(objects.obj228, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 22, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj229);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj229;
            lv_label_set_text(objects.obj229, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 87, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj230);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj230;
            lv_label_set_text(objects.obj230, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

void create_screen_app_fun() {
    void *flowState = getFlowState(0, 16);
    (void)flowState;
    lv_obj_t *obj = lv_obj_create(0);
    objects.app_fun = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj231 = obj;
            lv_obj_set_pos(obj, 440, 10);
            lv_obj_set_size(obj, 400, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "答案之书&塔罗牌");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj232 = obj;
            lv_obj_set_pos(obj, 128, 50);
            lv_obj_set_size(obj, LV_PCT(80), LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(0x6495ed), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj251 = obj;
            lv_obj_set_pos(obj, LV_PCT(50), 10);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj252 = obj;
            lv_obj_set_pos(obj, 10, 10);
            lv_obj_set_size(obj, 200, LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_label_set_text(obj, "");
        }
        {
            // fun_btn_home
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.fun_btn_home = obj;
            lv_obj_set_pos(obj, 30, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
            }
        }
        {
            // fun_btn_set
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.fun_btn_set = obj;
            lv_obj_set_pos(obj, 1150, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj253 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "");
                }
            }
        }
        {
            // fun_panel_book
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.fun_panel_book = obj;
            lv_obj_set_pos(obj, 32, 110);
            lv_obj_set_size(obj, LV_PCT(95), LV_PCT(80));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj233 = obj;
                    lv_obj_set_pos(obj, 20, 20);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "?");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj234 = obj;
                    lv_obj_set_pos(obj, 1179, 20);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "?");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj235 = obj;
                    lv_obj_set_pos(obj, 1179, 511);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "?");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj236 = obj;
                    lv_obj_set_pos(obj, 20, 510);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "?");
                }
                {
                    // panel_book_open
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.panel_book_open = obj;
                    lv_obj_set_pos(obj, 433, LV_PCT(6));
                    lv_obj_set_size(obj, 350, 450);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_x(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_y(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(0xdaa520), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // book_answer_text
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.book_answer_text = obj;
                            lv_obj_set_pos(obj, 0, LV_PCT(10));
                            lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "\"Consider a different perspective\"");
                        }
                        {
                            lv_obj_t *obj = lv_line_create(parent_obj);
                            objects.obj237 = obj;
                            lv_obj_set_pos(obj, 3, LV_PCT(38));
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            static lv_point_precise_t line_points[] = {
                                { 0, 0 },
                                { 300, 0 }
                            };
                            lv_line_set_points(obj, line_points, 2);
                            lv_obj_set_style_line_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_line_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            // book_answer_detail
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.book_answer_detail = obj;
                            lv_obj_set_pos(obj, 0, LV_PCT(42));
                            lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "It is suggested to view the issue from a different way or perspective, which may lead to new findings or solutions.");
                        }
                    }
                }
                {
                    // panel_book_close
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.panel_book_close = obj;
                    lv_obj_set_pos(obj, 433, LV_PCT(6));
                    lv_obj_set_size(obj, 350, 450);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_x(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_y(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(0xffd700), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj238 = obj;
                            lv_obj_set_pos(obj, 64, LV_PCT(15));
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "The Book of");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj239 = obj;
                            lv_obj_set_pos(obj, 57, LV_PCT(25));
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "ANSWERS");
                        }
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj240 = obj;
                            lv_obj_set_pos(obj, 1, 181);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_letter_space(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "= 答案之书 =");
                        }
                    }
                }
            }
        }
        {
            // fun_panel_tarot
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.fun_panel_tarot = obj;
            lv_obj_set_pos(obj, 32, 110);
            lv_obj_set_size(obj, LV_PCT(95), LV_PCT(80));
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj241 = obj;
                    lv_obj_set_pos(obj, 20, 20);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj242 = obj;
                    lv_obj_set_pos(obj, 1160, 20);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj243 = obj;
                    lv_obj_set_pos(obj, 1160, 511);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj244 = obj;
                    lv_obj_set_pos(obj, 20, 510);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj245 = obj;
                    lv_obj_set_pos(obj, 152, LV_PCT(10));
                    lv_obj_set_size(obj, LV_PCT(20), LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "过去");
                }
                {
                    // panel_tarot_open_1
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.panel_tarot_open_1 = obj;
                    lv_obj_set_pos(obj, 152, LV_PCT(20));
                    lv_obj_set_size(obj, LV_PCT(20), LV_PCT(60));
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_x(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_y(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // tarot_card_1
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.tarot_card_1 = obj;
                            lv_obj_set_pos(obj, 0, LV_PCT(20));
                            lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                            lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "审判");
                        }
                        {
                            lv_obj_t *obj = lv_line_create(parent_obj);
                            objects.obj246 = obj;
                            lv_obj_set_pos(obj, 0, LV_PCT(40));
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            static lv_point_precise_t line_points[] = {
                                { 0, 0 },
                                { 200, 0 }
                            };
                            lv_line_set_points(obj, line_points, 2);
                            lv_obj_set_style_line_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_line_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            // tarot_card_reversed_1
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.tarot_card_reversed_1 = obj;
                            lv_obj_set_pos(obj, 0, LV_PCT(50));
                            lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                            lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "正位");
                        }
                        {
                            // tarot_card_detail_panel_1
                            lv_obj_t *obj = lv_obj_create(parent_obj);
                            objects.tarot_card_detail_panel_1 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_PCT(100), LV_PCT(100));
                            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    // tarot_card_detail_text_1
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    objects.tarot_card_detail_text_1 = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "象征思想力量的觉醒与真理的锐利揭示，代表清晰洞察与果断决策的开端，蕴含理智与正义的纯粹能量。象征思想力量的觉醒与真理的锐利揭示，代表清晰洞察与果断决策的开端，蕴含理智与正义的纯粹能量。");
                                }
                            }
                        }
                    }
                }
                {
                    // panel_tarot_close_1
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.panel_tarot_close_1 = obj;
                    lv_obj_set_pos(obj, 152, LV_PCT(20));
                    lv_obj_set_size(obj, LV_PCT(20), LV_PCT(60));
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_x(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_y(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, LV_PCT(20));
                            lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, ui_font_icon_70, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj247 = obj;
                    lv_obj_set_pos(obj, 487, LV_PCT(10));
                    lv_obj_set_size(obj, LV_PCT(20), LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "现在");
                }
                {
                    // panel_tarot_open_2
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.panel_tarot_open_2 = obj;
                    lv_obj_set_pos(obj, 487, LV_PCT(20));
                    lv_obj_set_size(obj, LV_PCT(20), LV_PCT(60));
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_x(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_y(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_line_create(parent_obj);
                            objects.obj248 = obj;
                            lv_obj_set_pos(obj, 0, LV_PCT(40));
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            static lv_point_precise_t line_points[] = {
                                { 0, 0 },
                                { 200, 0 }
                            };
                            lv_line_set_points(obj, line_points, 2);
                            lv_obj_set_style_line_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_line_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            // tarot_card_2
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.tarot_card_2 = obj;
                            lv_obj_set_pos(obj, 0, LV_PCT(20));
                            lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                            lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "权杖骑士");
                        }
                        {
                            // tarot_card_reversed_2
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.tarot_card_reversed_2 = obj;
                            lv_obj_set_pos(obj, 0, LV_PCT(50));
                            lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                            lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "逆位");
                        }
                        {
                            // tarot_card_detail_panel_2
                            lv_obj_t *obj = lv_obj_create(parent_obj);
                            objects.tarot_card_detail_panel_2 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_PCT(100), LV_PCT(100));
                            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    // tarot_card_detail_text_2
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    objects.tarot_card_detail_text_2 = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "象征思想力量的觉醒与真理的锐利揭示，代表清晰洞察与果断决策的开端，蕴含理智与正义的纯粹能量。象征思想力量的觉醒与真理的锐利揭示，代表清晰洞察与果断决策的开端，蕴含理智与正义的纯粹能量。");
                                }
                            }
                        }
                    }
                }
                {
                    // panel_tarot_close_2
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.panel_tarot_close_2 = obj;
                    lv_obj_set_pos(obj, 487, LV_PCT(20));
                    lv_obj_set_size(obj, LV_PCT(20), LV_PCT(60));
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_x(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_y(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, LV_PCT(20));
                            lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, ui_font_icon_70, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj249 = obj;
                    lv_obj_set_pos(obj, 822, LV_PCT(10));
                    lv_obj_set_size(obj, LV_PCT(20), LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "未来");
                }
                {
                    // panel_tarot_open_3
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.panel_tarot_open_3 = obj;
                    lv_obj_set_pos(obj, 822, LV_PCT(20));
                    lv_obj_set_size(obj, LV_PCT(20), LV_PCT(60));
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_x(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_y(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_line_create(parent_obj);
                            objects.obj250 = obj;
                            lv_obj_set_pos(obj, 0, LV_PCT(40));
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            static lv_point_precise_t line_points[] = {
                                { 0, 0 },
                                { 200, 0 }
                            };
                            lv_line_set_points(obj, line_points, 2);
                            lv_obj_set_style_line_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_line_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        {
                            // tarot_card_3
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.tarot_card_3 = obj;
                            lv_obj_set_pos(obj, 0, LV_PCT(20));
                            lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                            lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "星币十");
                        }
                        {
                            // tarot_card_reversed_3
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.tarot_card_reversed_3 = obj;
                            lv_obj_set_pos(obj, 0, LV_PCT(50));
                            lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                            lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "逆位");
                        }
                        {
                            // tarot_card_detail_panel_3
                            lv_obj_t *obj = lv_obj_create(parent_obj);
                            objects.tarot_card_detail_panel_3 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_PCT(100), LV_PCT(100));
                            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
                            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    // tarot_card_detail_text_3
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    objects.tarot_card_detail_text_3 = obj;
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "象征思想力量的觉醒与真理的锐利揭示，代表清晰洞察与果断决策的开端，蕴含理智与正义的纯粹能量。象征思想力量的觉醒与真理的锐利揭示，代表清晰洞察与果断决策的开端，蕴含理智与正义的纯粹能量。");
                                }
                            }
                        }
                    }
                }
                {
                    // panel_tarot_close_3
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.panel_tarot_close_3 = obj;
                    lv_obj_set_pos(obj, 822, LV_PCT(20));
                    lv_obj_set_size(obj, LV_PCT(20), LV_PCT(60));
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_spread(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_x(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_ofs_y(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_shadow_width(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, LV_PCT(20));
                            lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, ui_font_icon_70, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "");
                        }
                    }
                }
            }
        }
        {
            // fun_tip_label
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.fun_tip_label = obj;
            lv_obj_set_pos(obj, 128, 620);
            lv_obj_set_size(obj, LV_PCT(80), LV_PCT(10));
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, " 请将设备翻转，心中默念你的疑问之后，翻开设备 ");
        }
        {
            // fun_set
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.fun_set = obj;
            lv_obj_set_pos(obj, 128, 150);
            lv_obj_set_size(obj, LV_PCT(80), LV_PCT(70));
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_row(obj, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_flex_main_place(obj, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_column(obj, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // fun_btn_book
                            lv_obj_t *obj = lv_button_create(parent_obj);
                            objects.fun_btn_book = obj;
                            lv_obj_set_pos(obj, -255, 323);
                            lv_obj_set_size(obj, 300, 300);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_MAIN | LV_STATE_PRESSED);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "答案之书");
                                }
                            }
                        }
                        {
                            // fun_btn_tarot
                            lv_obj_t *obj = lv_button_create(parent_obj);
                            objects.fun_btn_tarot = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, 300, 300);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_MAIN | LV_STATE_PRESSED);
                            {
                                lv_obj_t *parent_obj = obj;
                                {
                                    lv_obj_t *obj = lv_label_create(parent_obj);
                                    lv_obj_set_pos(obj, 0, 0);
                                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                                    lv_label_set_text_static(obj, "塔罗牌");
                                }
                            }
                        }
                    }
                }
            }
        }
        {
            // fun_led_ai
            lv_obj_t *obj = lv_led_create(parent_obj);
            objects.fun_led_ai = obj;
            lv_obj_set_pos(obj, 626, 670);
            lv_obj_set_size(obj, 40, 35);
            lv_led_set_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][4]));
            lv_led_set_brightness(obj, 255);
        }
    }
    
    tick_screen_app_fun();
}

void delete_screen_app_fun() {
    lv_obj_delete(objects.app_fun);
    objects.app_fun = 0;
    objects.obj231 = 0;
    objects.obj232 = 0;
    objects.obj251 = 0;
    objects.obj252 = 0;
    objects.fun_btn_home = 0;
    objects.fun_btn_set = 0;
    objects.obj253 = 0;
    objects.fun_panel_book = 0;
    objects.obj233 = 0;
    objects.obj234 = 0;
    objects.obj235 = 0;
    objects.obj236 = 0;
    objects.panel_book_open = 0;
    objects.book_answer_text = 0;
    objects.obj237 = 0;
    objects.book_answer_detail = 0;
    objects.panel_book_close = 0;
    objects.obj238 = 0;
    objects.obj239 = 0;
    objects.obj240 = 0;
    objects.fun_panel_tarot = 0;
    objects.obj241 = 0;
    objects.obj242 = 0;
    objects.obj243 = 0;
    objects.obj244 = 0;
    objects.obj245 = 0;
    objects.panel_tarot_open_1 = 0;
    objects.tarot_card_1 = 0;
    objects.obj246 = 0;
    objects.tarot_card_reversed_1 = 0;
    objects.tarot_card_detail_panel_1 = 0;
    objects.tarot_card_detail_text_1 = 0;
    objects.panel_tarot_close_1 = 0;
    objects.obj247 = 0;
    objects.panel_tarot_open_2 = 0;
    objects.obj248 = 0;
    objects.tarot_card_2 = 0;
    objects.tarot_card_reversed_2 = 0;
    objects.tarot_card_detail_panel_2 = 0;
    objects.tarot_card_detail_text_2 = 0;
    objects.panel_tarot_close_2 = 0;
    objects.obj249 = 0;
    objects.panel_tarot_open_3 = 0;
    objects.obj250 = 0;
    objects.tarot_card_3 = 0;
    objects.tarot_card_reversed_3 = 0;
    objects.tarot_card_detail_panel_3 = 0;
    objects.tarot_card_detail_text_3 = 0;
    objects.panel_tarot_close_3 = 0;
    objects.fun_tip_label = 0;
    objects.fun_set = 0;
    objects.fun_btn_book = 0;
    objects.fun_btn_tarot = 0;
    objects.fun_led_ai = 0;
    deletePageFlowState(16);
}

void tick_screen_app_fun() {
    void *flowState = getFlowState(0, 16);
    (void)flowState;
    {
        const char *new_val = evalTextProperty(flowState, 2, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj232);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj232;
            lv_label_set_text(objects.obj232, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 3, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj251);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj251;
            lv_label_set_text(objects.obj251, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 4, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj252);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj252;
            lv_label_set_text(objects.obj252, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 8, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj253);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj253;
            lv_label_set_text(objects.obj253, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

void create_screen_ftp() {
    void *flowState = getFlowState(0, 17);
    (void)flowState;
    lv_obj_t *obj = lv_obj_create(0);
    objects.ftp = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(obj, ui_font_chinese_30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj254 = obj;
            lv_obj_set_pos(obj, 440, 10);
            lv_obj_set_size(obj, 400, LV_SIZE_CONTENT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, ui_font_chinese_40, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "FTP文件管理");
        }
        {
            // ftp_btn_back2setting
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.ftp_btn_back2setting = obj;
            lv_obj_set_pos(obj, 30, 45);
            lv_obj_set_size(obj, 100, 60);
            lv_obj_set_style_radius(obj, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_column_pos(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_grid_cell_y_align(obj, LV_GRID_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "");
                }
            }
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj264 = obj;
            lv_obj_set_pos(obj, 10, 10);
            lv_obj_set_size(obj, 200, LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj265 = obj;
            lv_obj_set_pos(obj, LV_PCT(50), 10);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj255 = obj;
            lv_obj_set_pos(obj, 320, 50);
            lv_obj_set_size(obj, LV_PCT(50), LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.obj256 = obj;
            lv_obj_set_pos(obj, 32, 110);
            lv_obj_set_size(obj, LV_PCT(95), LV_PCT(80));
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_flex_flow(obj, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // ftp_hammy
                    lv_obj_t *obj = lv_image_create(parent_obj);
                    objects.ftp_hammy = obj;
                    lv_obj_set_pos(obj, 540, 201);
                    lv_obj_set_size(obj, LV_PCT(100), 150);
                    lv_image_set_src(obj, "/sys/src/ui_image_sleepy.bin");
                    lv_image_set_scale(obj, 255);
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj257 = obj;
                    lv_obj_set_pos(obj, 40, 190);
                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_flex_cross_place(obj, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj258 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_PCT(20), LV_SIZE_CONTENT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "状态：");
                        }
                        {
                            // ftp_label_state
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.ftp_label_state = obj;
                            lv_obj_set_pos(obj, 128, 573);
                            lv_obj_set_size(obj, LV_PCT(80), LV_SIZE_CONTENT);
                            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "等待连接");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj259 = obj;
                    lv_obj_set_pos(obj, 40, 190);
                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_flex_cross_place(obj, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj260 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_PCT(20), LV_SIZE_CONTENT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "IP：");
                        }
                        {
                            // ftp_label_ip
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.ftp_label_ip = obj;
                            lv_obj_set_pos(obj, 128, 573);
                            lv_obj_set_size(obj, LV_PCT(80), LV_SIZE_CONTENT);
                            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "ftp://123.456.789.222等待连接\nWifi为案松手的撒网数大");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.obj261 = obj;
                    lv_obj_set_pos(obj, 40, 190);
                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_layout(obj, LV_LAYOUT_FLEX, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_flex_cross_place(obj, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj262 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_PCT(20), LV_SIZE_CONTENT);
                            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "文件：");
                        }
                        {
                            // ftp_label_file
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.ftp_label_file = obj;
                            lv_obj_set_pos(obj, 128, 573);
                            lv_obj_set_size(obj, LV_PCT(80), LV_SIZE_CONTENT);
                            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL);
                            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "xxx.mid");
                        }
                    }
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.obj263 = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "进度：");
                }
                {
                    // ftp_bar_progress
                    lv_obj_t *obj = lv_bar_create(parent_obj);
                    objects.ftp_bar_progress = obj;
                    lv_obj_set_pos(obj, 128, 530);
                    lv_obj_set_size(obj, LV_PCT(100), 30);
                    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][3]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[eez_flow_get_selected_theme_index()][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_anim(obj, get_anim(), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
            }
        }
    }
    
    tick_screen_ftp();
}

void delete_screen_ftp() {
    lv_obj_delete(objects.ftp);
    objects.ftp = 0;
    objects.obj254 = 0;
    objects.ftp_btn_back2setting = 0;
    objects.obj264 = 0;
    objects.obj265 = 0;
    objects.obj255 = 0;
    objects.obj256 = 0;
    objects.ftp_hammy = 0;
    objects.obj257 = 0;
    objects.obj258 = 0;
    objects.ftp_label_state = 0;
    objects.obj259 = 0;
    objects.obj260 = 0;
    objects.ftp_label_ip = 0;
    objects.obj261 = 0;
    objects.obj262 = 0;
    objects.ftp_label_file = 0;
    objects.obj263 = 0;
    objects.ftp_bar_progress = 0;
    deletePageFlowState(17);
}

void tick_screen_ftp() {
    void *flowState = getFlowState(0, 17);
    (void)flowState;
    {
        const char *new_val = evalTextProperty(flowState, 4, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj264);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj264;
            lv_label_set_text(objects.obj264, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 5, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj265);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj265;
            lv_label_set_text(objects.obj265, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = evalTextProperty(flowState, 6, 3, "Failed to evaluate Text in Label widget");
        const char *cur_val = lv_label_get_text(objects.obj255);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj255;
            lv_label_set_text(objects.obj255, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

typedef void (*create_screen_func_t)();
create_screen_func_t create_screen_funcs[] = {
    create_screen_boot,
    create_screen_onboard_step,
    create_screen_launcher,
    create_screen_setting,
    create_screen_about,
    create_screen_app_zen_mode,
    create_screen_app_ear_train,
    create_screen_app_chord_memory,
    create_screen_app_circle_of_fifths,
    create_screen_app_tiny_piano,
    create_screen_app_drum_pad,
    create_screen_app_midi_player,
    create_screen_app_xy_mode,
    create_screen_app_metronome,
    create_screen_app_ai_agent,
    create_screen_app_clock,
    create_screen_app_fun,
    create_screen_ftp,
};
void create_screen(int screen_index) {
    create_screen_funcs[screen_index]();
}
void create_screen_by_id(enum ScreensEnum screenId) {
    create_screen_funcs[screenId - 1]();
}

typedef void (*delete_screen_func_t)();
delete_screen_func_t delete_screen_funcs[] = {
    delete_screen_boot,
    delete_screen_onboard_step,
    delete_screen_launcher,
    delete_screen_setting,
    delete_screen_about,
    delete_screen_app_zen_mode,
    delete_screen_app_ear_train,
    delete_screen_app_chord_memory,
    delete_screen_app_circle_of_fifths,
    delete_screen_app_tiny_piano,
    delete_screen_app_drum_pad,
    delete_screen_app_midi_player,
    delete_screen_app_xy_mode,
    delete_screen_app_metronome,
    delete_screen_app_ai_agent,
    delete_screen_app_clock,
    delete_screen_app_fun,
    delete_screen_ftp,
};
void delete_screen(int screen_index) {
    delete_screen_funcs[screen_index]();
}
void delete_screen_by_id(enum ScreensEnum screenId) {
    delete_screen_funcs[screenId - 1]();
}

typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_boot,
    tick_screen_onboard_step,
    tick_screen_launcher,
    tick_screen_setting,
    tick_screen_about,
    tick_screen_app_zen_mode,
    tick_screen_app_ear_train,
    tick_screen_app_chord_memory,
    tick_screen_app_circle_of_fifths,
    tick_screen_app_tiny_piano,
    tick_screen_app_drum_pad,
    tick_screen_app_midi_player,
    tick_screen_app_xy_mode,
    tick_screen_app_metronome,
    tick_screen_app_ai_agent,
    tick_screen_app_clock,
    tick_screen_app_fun,
    tick_screen_ftp,
};
void tick_screen(int screen_index) {
    if (screen_index >= 0 && screen_index < 18) {
        tick_screen_funcs[screen_index]();
    }
}
void tick_screen_by_id(enum ScreensEnum screenId) {
    tick_screen(screenId - 1);
}

//
// Fonts
//

lv_font_t *ui_font_icon_70;
lv_font_t *ui_font_chinese_30;
lv_font_t *ui_font_chinese_40;
lv_font_t *ui_font_clock_150;
lv_font_t *ui_font_clock_150_a;

ext_font_desc_t fonts[] = {
    { "icon_70", NULL },
    { "chinese_30", NULL },
    { "chinese_40", NULL },
    { "clock_150", NULL },
    { "clock_150_a", NULL },
#if LV_FONT_MONTSERRAT_8
    { "MONTSERRAT_8", &lv_font_montserrat_8 },
#endif
#if LV_FONT_MONTSERRAT_10
    { "MONTSERRAT_10", &lv_font_montserrat_10 },
#endif
#if LV_FONT_MONTSERRAT_12
    { "MONTSERRAT_12", &lv_font_montserrat_12 },
#endif
#if LV_FONT_MONTSERRAT_14
    { "MONTSERRAT_14", &lv_font_montserrat_14 },
#endif
#if LV_FONT_MONTSERRAT_16
    { "MONTSERRAT_16", &lv_font_montserrat_16 },
#endif
#if LV_FONT_MONTSERRAT_18
    { "MONTSERRAT_18", &lv_font_montserrat_18 },
#endif
#if LV_FONT_MONTSERRAT_20
    { "MONTSERRAT_20", &lv_font_montserrat_20 },
#endif
#if LV_FONT_MONTSERRAT_22
    { "MONTSERRAT_22", &lv_font_montserrat_22 },
#endif
#if LV_FONT_MONTSERRAT_24
    { "MONTSERRAT_24", &lv_font_montserrat_24 },
#endif
#if LV_FONT_MONTSERRAT_26
    { "MONTSERRAT_26", &lv_font_montserrat_26 },
#endif
#if LV_FONT_MONTSERRAT_28
    { "MONTSERRAT_28", &lv_font_montserrat_28 },
#endif
#if LV_FONT_MONTSERRAT_30
    { "MONTSERRAT_30", &lv_font_montserrat_30 },
#endif
#if LV_FONT_MONTSERRAT_32
    { "MONTSERRAT_32", &lv_font_montserrat_32 },
#endif
#if LV_FONT_MONTSERRAT_34
    { "MONTSERRAT_34", &lv_font_montserrat_34 },
#endif
#if LV_FONT_MONTSERRAT_36
    { "MONTSERRAT_36", &lv_font_montserrat_36 },
#endif
#if LV_FONT_MONTSERRAT_38
    { "MONTSERRAT_38", &lv_font_montserrat_38 },
#endif
#if LV_FONT_MONTSERRAT_40
    { "MONTSERRAT_40", &lv_font_montserrat_40 },
#endif
#if LV_FONT_MONTSERRAT_42
    { "MONTSERRAT_42", &lv_font_montserrat_42 },
#endif
#if LV_FONT_MONTSERRAT_44
    { "MONTSERRAT_44", &lv_font_montserrat_44 },
#endif
#if LV_FONT_MONTSERRAT_46
    { "MONTSERRAT_46", &lv_font_montserrat_46 },
#endif
#if LV_FONT_MONTSERRAT_48
    { "MONTSERRAT_48", &lv_font_montserrat_48 },
#endif
};

//
// Color themes
//

void change_color_theme(uint32_t theme_index) {
    {
        if (objects.boot) lv_obj_set_style_bg_color(objects.boot, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.boot) lv_obj_set_style_text_color(objects.boot, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.boot_label_name_en) lv_obj_set_style_text_color(objects.boot_label_name_en, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.boot_percent) lv_obj_set_style_bg_color(objects.boot_percent, lv_color_hex(theme_colors[theme_index][3]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        if (objects.boot_percent) lv_obj_set_style_bg_color(objects.boot_percent, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.boot_label_name) lv_obj_set_style_text_color(objects.boot_label_name, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    {
        if (objects.onboard_step) lv_obj_set_style_bg_color(objects.onboard_step, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.onboard_step) lv_obj_set_style_text_color(objects.onboard_step, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj0) lv_obj_set_style_bg_color(objects.obj0, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_step_prev) lv_obj_set_style_text_color(objects.ob_step_prev, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_step_prev) lv_obj_set_style_bg_color(objects.ob_step_prev, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_step_next) lv_obj_set_style_text_color(objects.ob_step_next, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_step_next) lv_obj_set_style_bg_color(objects.ob_step_next, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_str1) lv_obj_set_style_text_color(objects.ob_str1, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_str2) lv_obj_set_style_text_color(objects.ob_str2, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_str3) lv_obj_set_style_text_color(objects.ob_str3, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj1) lv_obj_set_style_text_color(objects.obj1, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj2) lv_obj_set_style_text_color(objects.obj2, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.label_loading_2) lv_obj_set_style_text_color(objects.label_loading_2, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj3) lv_obj_set_style_text_color(objects.obj3, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_set_hour) lv_obj_set_style_bg_color(objects.ob_set_hour, lv_color_hex(theme_colors[theme_index][2]), LV_PART_SELECTED | LV_STATE_DEFAULT);
        if (objects.ob_set_hour) lv_obj_set_style_text_color(objects.ob_set_hour, lv_color_hex(theme_colors[theme_index][5]), LV_PART_SELECTED | LV_STATE_DEFAULT);
        if (objects.ob_set_hour) lv_obj_set_style_bg_color(objects.ob_set_hour, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_set_hour) lv_obj_set_style_text_color(objects.ob_set_hour, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj4) lv_obj_set_style_text_color(objects.obj4, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_set_minute) lv_obj_set_style_bg_color(objects.ob_set_minute, lv_color_hex(theme_colors[theme_index][2]), LV_PART_SELECTED | LV_STATE_DEFAULT);
        if (objects.ob_set_minute) lv_obj_set_style_text_color(objects.ob_set_minute, lv_color_hex(theme_colors[theme_index][5]), LV_PART_SELECTED | LV_STATE_DEFAULT);
        if (objects.ob_set_minute) lv_obj_set_style_bg_color(objects.ob_set_minute, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_set_minute) lv_obj_set_style_text_color(objects.ob_set_minute, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj5) lv_obj_set_style_text_color(objects.obj5, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj6) lv_obj_set_style_text_color(objects.obj6, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_set_second) lv_obj_set_style_bg_color(objects.ob_set_second, lv_color_hex(theme_colors[theme_index][2]), LV_PART_SELECTED | LV_STATE_DEFAULT);
        if (objects.ob_set_second) lv_obj_set_style_text_color(objects.ob_set_second, lv_color_hex(theme_colors[theme_index][5]), LV_PART_SELECTED | LV_STATE_DEFAULT);
        if (objects.ob_set_second) lv_obj_set_style_bg_color(objects.ob_set_second, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_set_second) lv_obj_set_style_text_color(objects.ob_set_second, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_set_time_result) lv_obj_set_style_text_color(objects.ob_set_time_result, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_set_time_save) lv_obj_set_style_bg_color(objects.ob_set_time_save, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj7) lv_obj_set_style_text_color(objects.obj7, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_set_year) lv_obj_set_style_bg_color(objects.ob_set_year, lv_color_hex(theme_colors[theme_index][2]), LV_PART_SELECTED | LV_STATE_DEFAULT);
        if (objects.ob_set_year) lv_obj_set_style_text_color(objects.ob_set_year, lv_color_hex(theme_colors[theme_index][5]), LV_PART_SELECTED | LV_STATE_DEFAULT);
        if (objects.ob_set_year) lv_obj_set_style_bg_color(objects.ob_set_year, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_set_year) lv_obj_set_style_text_color(objects.ob_set_year, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_set_year) lv_obj_set_style_bg_color(objects.ob_set_year, lv_color_hex(theme_colors[theme_index][0]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        if (objects.ob_set_year) lv_obj_set_style_text_color(objects.ob_set_year, lv_color_hex(theme_colors[theme_index][6]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        if (objects.obj8) lv_obj_set_style_bg_color(objects.obj8, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj8) lv_obj_set_style_bg_color(objects.obj8, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj8) lv_obj_set_style_bg_color(objects.obj8, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        if (objects.obj9) lv_obj_set_style_text_color(objects.obj9, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_set_month) lv_obj_set_style_bg_color(objects.ob_set_month, lv_color_hex(theme_colors[theme_index][2]), LV_PART_SELECTED | LV_STATE_DEFAULT);
        if (objects.ob_set_month) lv_obj_set_style_text_color(objects.ob_set_month, lv_color_hex(theme_colors[theme_index][5]), LV_PART_SELECTED | LV_STATE_DEFAULT);
        if (objects.ob_set_month) lv_obj_set_style_bg_color(objects.ob_set_month, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_set_month) lv_obj_set_style_text_color(objects.ob_set_month, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_set_month) lv_obj_set_style_bg_color(objects.ob_set_month, lv_color_hex(theme_colors[theme_index][0]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        if (objects.ob_set_month) lv_obj_set_style_text_color(objects.ob_set_month, lv_color_hex(theme_colors[theme_index][6]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        if (objects.obj10) lv_obj_set_style_bg_color(objects.obj10, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj10) lv_obj_set_style_bg_color(objects.obj10, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj10) lv_obj_set_style_bg_color(objects.obj10, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        if (objects.obj11) lv_obj_set_style_text_color(objects.obj11, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_set_day) lv_obj_set_style_bg_color(objects.ob_set_day, lv_color_hex(theme_colors[theme_index][2]), LV_PART_SELECTED | LV_STATE_DEFAULT);
        if (objects.ob_set_day) lv_obj_set_style_text_color(objects.ob_set_day, lv_color_hex(theme_colors[theme_index][5]), LV_PART_SELECTED | LV_STATE_DEFAULT);
        if (objects.ob_set_day) lv_obj_set_style_bg_color(objects.ob_set_day, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_set_day) lv_obj_set_style_text_color(objects.ob_set_day, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_set_day) lv_obj_set_style_bg_color(objects.ob_set_day, lv_color_hex(theme_colors[theme_index][0]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        if (objects.ob_set_day) lv_obj_set_style_text_color(objects.ob_set_day, lv_color_hex(theme_colors[theme_index][6]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        if (objects.obj12) lv_obj_set_style_bg_color(objects.obj12, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj12) lv_obj_set_style_bg_color(objects.obj12, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj12) lv_obj_set_style_bg_color(objects.obj12, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        if (objects.obj13) lv_obj_set_style_text_color(objects.obj13, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj14) lv_obj_set_style_text_color(objects.obj14, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj15) lv_obj_set_style_text_color(objects.obj15, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.label_loading_3) lv_obj_set_style_text_color(objects.label_loading_3, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj16) lv_obj_set_style_text_color(objects.obj16, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_slide_brightness) lv_obj_set_style_bg_color(objects.ob_slide_brightness, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_slide_brightness) lv_obj_set_style_bg_color(objects.ob_slide_brightness, lv_color_hex(theme_colors[theme_index][3]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        if (objects.ob_slide_brightness) lv_obj_set_style_border_color(objects.ob_slide_brightness, lv_color_hex(theme_colors[theme_index][3]), LV_PART_KNOB | LV_STATE_DEFAULT);
        if (objects.ob_slide_brightness_num) lv_obj_set_style_text_color(objects.ob_slide_brightness_num, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj17) lv_obj_set_style_text_color(objects.obj17, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_slide_volume) lv_obj_set_style_bg_color(objects.ob_slide_volume, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_slide_volume) lv_obj_set_style_bg_color(objects.ob_slide_volume, lv_color_hex(theme_colors[theme_index][3]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        if (objects.ob_slide_volume) lv_obj_set_style_border_color(objects.ob_slide_volume, lv_color_hex(theme_colors[theme_index][3]), LV_PART_KNOB | LV_STATE_DEFAULT);
        if (objects.ob_slide_volume_num) lv_obj_set_style_text_color(objects.ob_slide_volume_num, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_key_try_volume) lv_obj_set_style_bg_color(objects.ob_key_try_volume, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_key_try_volume) lv_obj_set_style_text_color(objects.ob_key_try_volume, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj18) lv_obj_set_style_text_color(objects.obj18, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj19) lv_obj_set_style_text_color(objects.obj19, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj20) lv_obj_set_style_text_color(objects.obj20, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.label_loading_5) lv_obj_set_style_text_color(objects.label_loading_5, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj21) lv_obj_set_style_text_color(objects.obj21, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj21) lv_obj_set_style_bg_color(objects.obj21, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_setting_theme) lv_obj_set_style_text_color(objects.ob_setting_theme, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_setting_theme) lv_obj_set_style_border_color(objects.ob_setting_theme, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_setting_theme) lv_obj_set_style_bg_color(objects.ob_setting_theme, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj22) lv_obj_set_style_bg_color(objects.obj22, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj22) lv_obj_set_style_text_color(objects.obj22, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj22) lv_obj_set_style_bg_color(objects.obj22, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj23) lv_obj_set_style_text_color(objects.obj23, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj23) lv_obj_set_style_bg_color(objects.obj23, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_setting_on_screen) lv_obj_set_style_text_color(objects.ob_setting_on_screen, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_setting_on_screen) lv_obj_set_style_border_color(objects.ob_setting_on_screen, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_setting_on_screen) lv_obj_set_style_bg_color(objects.ob_setting_on_screen, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj24) lv_obj_set_style_bg_color(objects.obj24, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj24) lv_obj_set_style_bg_color(objects.obj24, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj24) lv_obj_set_style_text_color(objects.obj24, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj25) lv_obj_set_style_text_color(objects.obj25, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj25) lv_obj_set_style_bg_color(objects.obj25, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_setting_time2idle) lv_obj_set_style_text_color(objects.ob_setting_time2idle, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_setting_time2idle) lv_obj_set_style_border_color(objects.ob_setting_time2idle, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_setting_time2idle) lv_obj_set_style_bg_color(objects.ob_setting_time2idle, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj26) lv_obj_set_style_bg_color(objects.obj26, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj26) lv_obj_set_style_bg_color(objects.obj26, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj26) lv_obj_set_style_text_color(objects.obj26, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj27) lv_obj_set_style_text_color(objects.obj27, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj27) lv_obj_set_style_bg_color(objects.obj27, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_setting_auto_sleep) lv_obj_set_style_bg_color(objects.ob_setting_auto_sleep, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_setting_auto_sleep) lv_obj_set_style_bg_color(objects.ob_setting_auto_sleep, lv_color_hex(theme_colors[theme_index][3]), LV_PART_INDICATOR | LV_STATE_CHECKED);
        if (objects.obj28) lv_obj_set_style_text_color(objects.obj28, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj29) lv_obj_set_style_text_color(objects.obj29, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.label_loading_4) lv_obj_set_style_text_color(objects.label_loading_4, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj30) lv_obj_set_style_text_color(objects.obj30, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj30) lv_obj_set_style_bg_color(objects.obj30, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_setting_wifi_switch) lv_obj_set_style_bg_color(objects.ob_setting_wifi_switch, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ob_setting_wifi_switch) lv_obj_set_style_bg_color(objects.ob_setting_wifi_switch, lv_color_hex(theme_colors[theme_index][3]), LV_PART_INDICATOR | LV_STATE_CHECKED);
        if (objects.ob_set_wifi_connect_tip) lv_obj_set_style_text_color(objects.ob_set_wifi_connect_tip, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj31) lv_obj_set_style_text_color(objects.obj31, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj32) lv_obj_set_style_text_color(objects.obj32, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    {
        if (objects.launcher) lv_obj_set_style_bg_color(objects.launcher, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher) lv_obj_set_style_text_color(objects.launcher, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj33) lv_obj_set_style_text_color(objects.obj33, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj34) lv_obj_set_style_bg_color(objects.obj34, lv_color_hex(theme_colors[theme_index][4]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        if (objects.launcher_zen) lv_obj_set_style_text_color(objects.launcher_zen, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_zen) lv_obj_set_style_shadow_color(objects.launcher_zen, lv_color_hex(theme_colors[theme_index][10]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_zen) lv_obj_set_style_bg_color(objects.launcher_zen, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj35) lv_obj_set_style_text_color(objects.obj35, lv_color_hex(theme_colors[theme_index][10]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_ear) lv_obj_set_style_text_color(objects.launcher_ear, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_ear) lv_obj_set_style_shadow_color(objects.launcher_ear, lv_color_hex(theme_colors[theme_index][11]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_ear) lv_obj_set_style_bg_color(objects.launcher_ear, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj36) lv_obj_set_style_text_color(objects.obj36, lv_color_hex(theme_colors[theme_index][11]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_fifth) lv_obj_set_style_text_color(objects.launcher_fifth, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_fifth) lv_obj_set_style_shadow_color(objects.launcher_fifth, lv_color_hex(theme_colors[theme_index][12]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_fifth) lv_obj_set_style_bg_color(objects.launcher_fifth, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj37) lv_obj_set_style_text_color(objects.obj37, lv_color_hex(theme_colors[theme_index][12]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_chord) lv_obj_set_style_text_color(objects.launcher_chord, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_chord) lv_obj_set_style_shadow_color(objects.launcher_chord, lv_color_hex(theme_colors[theme_index][12]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_chord) lv_obj_set_style_bg_color(objects.launcher_chord, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj38) lv_obj_set_style_text_color(objects.obj38, lv_color_hex(theme_colors[theme_index][12]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_midi) lv_obj_set_style_text_color(objects.launcher_midi, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_midi) lv_obj_set_style_shadow_color(objects.launcher_midi, lv_color_hex(theme_colors[theme_index][13]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_midi) lv_obj_set_style_bg_color(objects.launcher_midi, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj39) lv_obj_set_style_text_color(objects.obj39, lv_color_hex(theme_colors[theme_index][13]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_xy) lv_obj_set_style_text_color(objects.launcher_xy, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_xy) lv_obj_set_style_shadow_color(objects.launcher_xy, lv_color_hex(theme_colors[theme_index][13]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_xy) lv_obj_set_style_bg_color(objects.launcher_xy, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj40) lv_obj_set_style_text_color(objects.obj40, lv_color_hex(theme_colors[theme_index][13]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_drum) lv_obj_set_style_text_color(objects.launcher_drum, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_drum) lv_obj_set_style_shadow_color(objects.launcher_drum, lv_color_hex(theme_colors[theme_index][13]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_drum) lv_obj_set_style_bg_color(objects.launcher_drum, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj41) lv_obj_set_style_text_color(objects.obj41, lv_color_hex(theme_colors[theme_index][13]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_piano) lv_obj_set_style_text_color(objects.launcher_piano, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_piano) lv_obj_set_style_shadow_color(objects.launcher_piano, lv_color_hex(theme_colors[theme_index][13]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_piano) lv_obj_set_style_bg_color(objects.launcher_piano, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj42) lv_obj_set_style_text_color(objects.obj42, lv_color_hex(theme_colors[theme_index][13]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_clock) lv_obj_set_style_text_color(objects.launcher_clock, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_clock) lv_obj_set_style_shadow_color(objects.launcher_clock, lv_color_hex(theme_colors[theme_index][15]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_clock) lv_obj_set_style_bg_color(objects.launcher_clock, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj43) lv_obj_set_style_text_color(objects.obj43, lv_color_hex(theme_colors[theme_index][15]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_ai) lv_obj_set_style_text_color(objects.launcher_ai, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_ai) lv_obj_set_style_shadow_color(objects.launcher_ai, lv_color_hex(theme_colors[theme_index][14]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_ai) lv_obj_set_style_bg_color(objects.launcher_ai, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj44) lv_obj_set_style_text_color(objects.obj44, lv_color_hex(theme_colors[theme_index][14]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_metron) lv_obj_set_style_text_color(objects.launcher_metron, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_metron) lv_obj_set_style_shadow_color(objects.launcher_metron, lv_color_hex(theme_colors[theme_index][14]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_metron) lv_obj_set_style_bg_color(objects.launcher_metron, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj45) lv_obj_set_style_text_color(objects.obj45, lv_color_hex(theme_colors[theme_index][14]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_fun) lv_obj_set_style_text_color(objects.launcher_fun, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_fun) lv_obj_set_style_shadow_color(objects.launcher_fun, lv_color_hex(theme_colors[theme_index][15]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_fun) lv_obj_set_style_bg_color(objects.launcher_fun, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj46) lv_obj_set_style_text_color(objects.obj46, lv_color_hex(theme_colors[theme_index][15]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_btn_setting) lv_obj_set_style_text_color(objects.launcher_btn_setting, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_btn_setting) lv_obj_set_style_bg_color(objects.launcher_btn_setting, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.launcher_led_ai) lv_led_set_color(objects.launcher_led_ai, lv_color_hex(theme_colors[theme_index][4]));
    }
    {
        if (objects.setting) lv_obj_set_style_bg_color(objects.setting, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting) lv_obj_set_style_text_color(objects.setting, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj49) lv_obj_set_style_text_color(objects.obj49, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj50) lv_obj_set_style_text_color(objects.obj50, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_btn_home) lv_obj_set_style_bg_color(objects.setting_btn_home, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_btn_home) lv_obj_set_style_text_color(objects.setting_btn_home, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_btn_about) lv_obj_set_style_text_color(objects.setting_btn_about, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_btn_about) lv_obj_set_style_bg_color(objects.setting_btn_about, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_tab) lv_obj_set_style_bg_color(objects.setting_tab, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_tab) lv_obj_set_style_text_color(objects.setting_tab, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_tab_basic) lv_obj_set_style_text_color(objects.setting_tab_basic, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj51) lv_obj_set_style_text_color(objects.obj51, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj51) lv_obj_set_style_bg_color(objects.obj51, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_language) lv_obj_set_style_text_color(objects.setting_language, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_language) lv_obj_set_style_border_color(objects.setting_language, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_language) lv_obj_set_style_bg_color(objects.setting_language, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj52) lv_obj_set_style_bg_color(objects.obj52, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj52) lv_obj_set_style_text_color(objects.obj52, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj52) lv_obj_set_style_bg_color(objects.obj52, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj53) lv_obj_set_style_text_color(objects.obj53, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj53) lv_obj_set_style_bg_color(objects.obj53, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_slide_brightness) lv_obj_set_style_bg_color(objects.setting_slide_brightness, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_slide_brightness) lv_obj_set_style_bg_color(objects.setting_slide_brightness, lv_color_hex(theme_colors[theme_index][3]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        if (objects.setting_slide_brightness) lv_obj_set_style_border_color(objects.setting_slide_brightness, lv_color_hex(theme_colors[theme_index][3]), LV_PART_KNOB | LV_STATE_DEFAULT);
        if (objects.obj54) lv_obj_set_style_text_color(objects.obj54, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj54) lv_obj_set_style_bg_color(objects.obj54, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_slide_volume) lv_obj_set_style_bg_color(objects.setting_slide_volume, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_slide_volume) lv_obj_set_style_bg_color(objects.setting_slide_volume, lv_color_hex(theme_colors[theme_index][3]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        if (objects.setting_slide_volume) lv_obj_set_style_border_color(objects.setting_slide_volume, lv_color_hex(theme_colors[theme_index][3]), LV_PART_KNOB | LV_STATE_DEFAULT);
        if (objects.obj55) lv_obj_set_style_text_color(objects.obj55, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj55) lv_obj_set_style_bg_color(objects.obj55, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_theme) lv_obj_set_style_text_color(objects.setting_theme, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_theme) lv_obj_set_style_border_color(objects.setting_theme, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_theme) lv_obj_set_style_bg_color(objects.setting_theme, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj56) lv_obj_set_style_bg_color(objects.obj56, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj56) lv_obj_set_style_text_color(objects.obj56, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj56) lv_obj_set_style_bg_color(objects.obj56, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj57) lv_obj_set_style_text_color(objects.obj57, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj57) lv_obj_set_style_bg_color(objects.obj57, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_on_screen) lv_obj_set_style_text_color(objects.setting_on_screen, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_on_screen) lv_obj_set_style_border_color(objects.setting_on_screen, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_on_screen) lv_obj_set_style_bg_color(objects.setting_on_screen, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj58) lv_obj_set_style_bg_color(objects.obj58, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj58) lv_obj_set_style_bg_color(objects.obj58, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj58) lv_obj_set_style_text_color(objects.obj58, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj59) lv_obj_set_style_text_color(objects.obj59, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj59) lv_obj_set_style_bg_color(objects.obj59, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_time2idle) lv_obj_set_style_text_color(objects.setting_time2idle, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_time2idle) lv_obj_set_style_border_color(objects.setting_time2idle, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_time2idle) lv_obj_set_style_bg_color(objects.setting_time2idle, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj60) lv_obj_set_style_bg_color(objects.obj60, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj60) lv_obj_set_style_bg_color(objects.obj60, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj60) lv_obj_set_style_text_color(objects.obj60, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj61) lv_obj_set_style_text_color(objects.obj61, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj61) lv_obj_set_style_bg_color(objects.obj61, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_auto_sleep) lv_obj_set_style_bg_color(objects.setting_auto_sleep, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_auto_sleep) lv_obj_set_style_bg_color(objects.setting_auto_sleep, lv_color_hex(theme_colors[theme_index][3]), LV_PART_INDICATOR | LV_STATE_CHECKED);
        if (objects.obj62) lv_obj_set_style_text_color(objects.obj62, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj62) lv_obj_set_style_bg_color(objects.obj62, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_invert_display) lv_obj_set_style_bg_color(objects.setting_invert_display, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_invert_display) lv_obj_set_style_bg_color(objects.setting_invert_display, lv_color_hex(theme_colors[theme_index][3]), LV_PART_INDICATOR | LV_STATE_CHECKED);
        if (objects.setting_tab_advanced) lv_obj_set_style_text_color(objects.setting_tab_advanced, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj63) lv_obj_set_style_text_color(objects.obj63, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj63) lv_obj_set_style_bg_color(objects.obj63, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_btn_wifi_detail) lv_obj_set_style_text_color(objects.setting_btn_wifi_detail, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_btn_wifi_detail) lv_obj_set_style_bg_color(objects.setting_btn_wifi_detail, lv_color_hex(theme_colors[theme_index][3]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj64) lv_obj_set_style_text_color(objects.obj64, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj64) lv_obj_set_style_bg_color(objects.obj64, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_btn_ftp) lv_obj_set_style_text_color(objects.setting_btn_ftp, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_btn_ftp) lv_obj_set_style_bg_color(objects.setting_btn_ftp, lv_color_hex(theme_colors[theme_index][3]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj65) lv_obj_set_style_border_color(objects.obj65, lv_color_hex(theme_colors[theme_index][8]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj65) lv_obj_set_style_text_color(objects.obj65, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj65) lv_obj_set_style_bg_color(objects.obj65, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_btn_system_reset) lv_obj_set_style_text_color(objects.setting_btn_system_reset, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_btn_system_reset) lv_obj_set_style_bg_color(objects.setting_btn_system_reset, lv_color_hex(theme_colors[theme_index][8]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.wifi_set_panel) lv_obj_set_style_bg_color(objects.wifi_set_panel, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.wifi_set_panel) lv_obj_set_style_text_color(objects.wifi_set_panel, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.wifi_set_panel_return) lv_obj_set_style_bg_color(objects.wifi_set_panel_return, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.wifi_set_panel_return) lv_obj_set_style_text_color(objects.wifi_set_panel_return, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj66) lv_obj_set_style_text_color(objects.obj66, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj66) lv_obj_set_style_bg_color(objects.obj66, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_wifi_switch) lv_obj_set_style_bg_color(objects.setting_wifi_switch, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_wifi_switch) lv_obj_set_style_bg_color(objects.setting_wifi_switch, lv_color_hex(theme_colors[theme_index][3]), LV_PART_INDICATOR | LV_STATE_CHECKED);
        if (objects.setting_wifi_connect_tip) lv_obj_set_style_text_color(objects.setting_wifi_connect_tip, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj67) lv_obj_set_style_border_color(objects.obj67, lv_color_hex(theme_colors[theme_index][8]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj67) lv_obj_set_style_text_color(objects.obj67, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj67) lv_obj_set_style_bg_color(objects.obj67, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_btn_wifi_reset) lv_obj_set_style_text_color(objects.setting_btn_wifi_reset, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_btn_wifi_reset) lv_obj_set_style_bg_color(objects.setting_btn_wifi_reset, lv_color_hex(theme_colors[theme_index][8]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.setting_led_ai) lv_led_set_color(objects.setting_led_ai, lv_color_hex(theme_colors[theme_index][4]));
    }
    {
        if (objects.about) lv_obj_set_style_text_color(objects.about, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.about) lv_obj_set_style_bg_color(objects.about, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj71) lv_obj_set_style_text_color(objects.obj71, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.about_btn_return) lv_obj_set_style_text_color(objects.about_btn_return, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.about_btn_return) lv_obj_set_style_bg_color(objects.about_btn_return, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj72) lv_obj_set_style_bg_color(objects.obj72, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj73) lv_obj_set_style_text_color(objects.obj73, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj74) lv_obj_set_style_text_color(objects.obj74, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj75) lv_obj_set_style_text_color(objects.obj75, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj75) lv_obj_set_style_bg_color(objects.obj75, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj76) lv_obj_set_style_text_color(objects.obj76, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj77) lv_obj_set_style_text_color(objects.obj77, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj77) lv_obj_set_style_bg_color(objects.obj77, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj78) lv_obj_set_style_text_color(objects.obj78, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj79) lv_obj_set_style_text_color(objects.obj79, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj79) lv_obj_set_style_bg_color(objects.obj79, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj80) lv_obj_set_style_text_color(objects.obj80, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj81) lv_obj_set_style_text_color(objects.obj81, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj81) lv_obj_set_style_bg_color(objects.obj81, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.about_system_monitor_tick) lv_obj_set_style_text_color(objects.about_system_monitor_tick, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.about_led_ai) lv_led_set_color(objects.about_led_ai, lv_color_hex(theme_colors[theme_index][4]));
    }
    {
        if (objects.app_zen_mode) lv_obj_set_style_text_color(objects.app_zen_mode, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.app_zen_mode) lv_obj_set_style_bg_color(objects.app_zen_mode, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj85) lv_obj_set_style_text_color(objects.obj85, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.zen_btn_home) lv_obj_set_style_bg_color(objects.zen_btn_home, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.zen_btn_home) lv_obj_set_style_text_color(objects.zen_btn_home, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.zen_btn_home) lv_obj_set_style_shadow_color(objects.zen_btn_home, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj86) lv_obj_set_style_text_color(objects.obj86, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj87) lv_obj_set_style_bg_color(objects.obj87, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj88) lv_obj_set_style_text_color(objects.obj88, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.zen_dropdown_mode) lv_obj_set_style_text_color(objects.zen_dropdown_mode, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.zen_dropdown_mode) lv_obj_set_style_bg_color(objects.zen_dropdown_mode, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj89) lv_obj_set_style_bg_color(objects.obj89, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj89) lv_obj_set_style_bg_color(objects.obj89, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj89) lv_obj_set_style_text_color(objects.obj89, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj89) lv_obj_set_style_bg_color(objects.obj89, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        if (objects.obj89) lv_obj_set_style_text_color(objects.obj89, lv_color_hex(theme_colors[theme_index][6]), LV_PART_ITEMS | LV_STATE_DEFAULT);
        if (objects.obj90) lv_obj_set_style_text_color(objects.obj90, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.zen_dropdown_key) lv_obj_set_style_text_color(objects.zen_dropdown_key, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.zen_dropdown_key) lv_obj_set_style_bg_color(objects.zen_dropdown_key, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj91) lv_obj_set_style_bg_color(objects.obj91, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj91) lv_obj_set_style_bg_color(objects.obj91, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj91) lv_obj_set_style_text_color(objects.obj91, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj91) lv_obj_set_style_bg_color(objects.obj91, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        if (objects.obj91) lv_obj_set_style_text_color(objects.obj91, lv_color_hex(theme_colors[theme_index][6]), LV_PART_ITEMS | LV_STATE_DEFAULT);
        if (objects.obj92) lv_obj_set_style_text_color(objects.obj92, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.zen_dropdown_speed) lv_obj_set_style_text_color(objects.zen_dropdown_speed, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.zen_dropdown_speed) lv_obj_set_style_bg_color(objects.zen_dropdown_speed, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj93) lv_obj_set_style_bg_color(objects.obj93, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj93) lv_obj_set_style_bg_color(objects.obj93, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj93) lv_obj_set_style_text_color(objects.obj93, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj93) lv_obj_set_style_bg_color(objects.obj93, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        if (objects.obj93) lv_obj_set_style_text_color(objects.obj93, lv_color_hex(theme_colors[theme_index][6]), LV_PART_ITEMS | LV_STATE_DEFAULT);
        if (objects.zen_btn_rec) lv_obj_set_style_text_color(objects.zen_btn_rec, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.zen_btn_rec) lv_obj_set_style_bg_color(objects.zen_btn_rec, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.zen_btn_rec) lv_obj_set_style_bg_color(objects.zen_btn_rec, lv_color_hex(theme_colors[theme_index][8]), LV_PART_MAIN | LV_STATE_CHECKED);
        if (objects.obj94) lv_obj_set_style_text_color(objects.obj94, lv_color_hex(theme_colors[theme_index][8]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.zen_led_ai) lv_led_set_color(objects.zen_led_ai, lv_color_hex(theme_colors[theme_index][4]));
    }
    {
        if (objects.app_ear_train) lv_obj_set_style_text_color(objects.app_ear_train, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.app_ear_train) lv_obj_set_style_bg_color(objects.app_ear_train, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj97) lv_obj_set_style_text_color(objects.obj97, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj98) lv_obj_set_style_text_color(objects.obj98, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ear_btn_home) lv_obj_set_style_text_color(objects.ear_btn_home, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ear_btn_home) lv_obj_set_style_bg_color(objects.ear_btn_home, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ear_key_try_play) lv_obj_set_style_bg_color(objects.ear_key_try_play, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ear_key_try_play) lv_obj_set_style_text_color(objects.ear_key_try_play, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj99) lv_obj_set_style_text_color(objects.obj99, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj100) lv_obj_set_style_text_color(objects.obj100, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ear_score_title) lv_obj_set_style_text_color(objects.ear_score_title, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ear_score) lv_obj_set_style_text_color(objects.ear_score, lv_color_hex(theme_colors[theme_index][13]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj101) lv_obj_set_style_shadow_color(objects.obj101, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj101) lv_obj_set_style_bg_color(objects.obj101, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ear_trainer_test) lv_obj_set_style_text_color(objects.ear_trainer_test, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ear_trainer_test) lv_obj_set_style_bg_color(objects.ear_trainer_test, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj102) lv_obj_set_style_bg_color(objects.obj102, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj102) lv_obj_set_style_bg_color(objects.obj102, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj102) lv_obj_set_style_text_color(objects.obj102, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj103) lv_obj_set_style_text_color(objects.obj103, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ear_mode) lv_obj_set_style_text_color(objects.ear_mode, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ear_mode) lv_obj_set_style_bg_color(objects.ear_mode, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj104) lv_obj_set_style_bg_color(objects.obj104, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj104) lv_obj_set_style_bg_color(objects.obj104, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj104) lv_obj_set_style_text_color(objects.obj104, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj105) lv_obj_set_style_text_color(objects.obj105, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ear_difficult) lv_obj_set_style_text_color(objects.ear_difficult, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ear_difficult) lv_obj_set_style_bg_color(objects.ear_difficult, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj106) lv_obj_set_style_bg_color(objects.obj106, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj106) lv_obj_set_style_bg_color(objects.obj106, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj106) lv_obj_set_style_text_color(objects.obj106, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj107) lv_obj_set_style_text_color(objects.obj107, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ear_best_score) lv_obj_set_style_text_color(objects.ear_best_score, lv_color_hex(theme_colors[theme_index][15]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ear_key_major) lv_obj_set_style_bg_color(objects.ear_key_major, lv_color_hex(theme_colors[theme_index][2]), LV_PART_ITEMS | LV_STATE_DEFAULT);
        if (objects.ear_key_major) lv_obj_set_style_text_color(objects.ear_key_major, lv_color_hex(theme_colors[theme_index][5]), LV_PART_ITEMS | LV_STATE_DEFAULT);
        if (objects.ear_key_minor2) lv_obj_set_style_bg_color(objects.ear_key_minor2, lv_color_hex(theme_colors[theme_index][6]), LV_PART_ITEMS | LV_STATE_DEFAULT);
        if (objects.ear_key_minor2) lv_obj_set_style_text_color(objects.ear_key_minor2, lv_color_hex(theme_colors[theme_index][5]), LV_PART_ITEMS | LV_STATE_DEFAULT);
        if (objects.ear_key_minor3) lv_obj_set_style_bg_color(objects.ear_key_minor3, lv_color_hex(theme_colors[theme_index][6]), LV_PART_ITEMS | LV_STATE_DEFAULT);
        if (objects.ear_key_minor3) lv_obj_set_style_text_color(objects.ear_key_minor3, lv_color_hex(theme_colors[theme_index][5]), LV_PART_ITEMS | LV_STATE_DEFAULT);
        if (objects.ear_key_interval) lv_obj_set_style_bg_color(objects.ear_key_interval, lv_color_hex(theme_colors[theme_index][2]), LV_PART_ITEMS | LV_STATE_DEFAULT);
        if (objects.ear_key_interval) lv_obj_set_style_text_color(objects.ear_key_interval, lv_color_hex(theme_colors[theme_index][5]), LV_PART_ITEMS | LV_STATE_DEFAULT);
        if (objects.ear_life_panel) lv_obj_set_style_bg_color(objects.ear_life_panel, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ear_life1) lv_led_set_color(objects.ear_life1, lv_color_hex(theme_colors[theme_index][7]));
        if (objects.ear_life2) lv_led_set_color(objects.ear_life2, lv_color_hex(theme_colors[theme_index][7]));
        if (objects.ear_life3) lv_led_set_color(objects.ear_life3, lv_color_hex(theme_colors[theme_index][7]));
        if (objects.ear_led_ai) lv_led_set_color(objects.ear_led_ai, lv_color_hex(theme_colors[theme_index][4]));
    }
    {
        if (objects.app_chord_memory) lv_obj_set_style_text_color(objects.app_chord_memory, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.app_chord_memory) lv_obj_set_style_bg_color(objects.app_chord_memory, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj110) lv_obj_set_style_text_color(objects.obj110, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj111) lv_obj_set_style_text_color(objects.obj111, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.chord_btn_home) lv_obj_set_style_text_color(objects.chord_btn_home, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.chord_btn_home) lv_obj_set_style_bg_color(objects.chord_btn_home, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.chord_key_key) lv_obj_set_style_bg_color(objects.chord_key_key, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.chord_key_key) lv_obj_set_style_text_color(objects.chord_key_key, lv_color_hex(theme_colors[theme_index][5]), LV_PART_ITEMS | LV_STATE_DEFAULT);
        if (objects.chord_key_key) lv_obj_set_style_bg_color(objects.chord_key_key, lv_color_hex(theme_colors[theme_index][0]), LV_PART_ITEMS | LV_STATE_DEFAULT);
        if (objects.chord_key_key) lv_obj_set_style_border_color(objects.chord_key_key, lv_color_hex(theme_colors[theme_index][16]), LV_PART_ITEMS | LV_STATE_DEFAULT);
        if (objects.chord_key_key) lv_obj_set_style_bg_color(objects.chord_key_key, lv_color_hex(theme_colors[theme_index][1]), LV_PART_ITEMS | LV_STATE_CHECKED);
        if (objects.chord_key_key) lv_obj_set_style_text_color(objects.chord_key_key, lv_color_hex(theme_colors[theme_index][5]), LV_PART_ITEMS | LV_STATE_CHECKED);
        if (objects.obj112) lv_obj_set_style_text_color(objects.obj112, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj113) lv_obj_set_style_text_color(objects.obj113, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.chord_definition) lv_obj_set_style_text_color(objects.chord_definition, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.chord_panel_type_poll) lv_obj_set_style_bg_color(objects.chord_panel_type_poll, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.chord_panel_type_poll) lv_obj_set_style_border_color(objects.chord_panel_type_poll, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.chord_type_maj) lv_obj_set_style_bg_color(objects.chord_type_maj, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.chord_type_maj) lv_obj_set_style_text_color(objects.chord_type_maj, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.chord_type_maj) lv_obj_set_style_border_color(objects.chord_type_maj, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.chord_type_maj) lv_obj_set_style_bg_color(objects.chord_type_maj, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_PRESSED);
        if (objects.chord_type_maj) lv_obj_set_style_text_color(objects.chord_type_maj, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_PRESSED);
        if (objects.chord_name) lv_obj_set_style_text_color(objects.chord_name, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.chord_led_ai) lv_led_set_color(objects.chord_led_ai, lv_color_hex(theme_colors[theme_index][4]));
    }
    {
        if (objects.app_circle_of_fifths) lv_obj_set_style_text_color(objects.app_circle_of_fifths, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.app_circle_of_fifths) lv_obj_set_style_bg_color(objects.app_circle_of_fifths, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj116) lv_obj_set_style_text_color(objects.obj116, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fifth_btn_home) lv_obj_set_style_text_color(objects.fifth_btn_home, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fifth_btn_home) lv_obj_set_style_bg_color(objects.fifth_btn_home, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj117) lv_obj_set_style_text_color(objects.obj117, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fifth_panel_info) lv_obj_set_style_text_color(objects.fifth_panel_info, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fifth_panel_info) lv_obj_set_style_bg_color(objects.fifth_panel_info, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fifth_name) lv_obj_set_style_text_color(objects.fifth_name, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj118) lv_obj_set_style_text_color(objects.obj118, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fifth_scale) lv_obj_set_style_text_color(objects.fifth_scale, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fifth_scale) lv_obj_set_style_bg_color(objects.fifth_scale, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj119) lv_obj_set_style_text_color(objects.obj119, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fifth_dominant) lv_obj_set_style_text_color(objects.fifth_dominant, lv_color_hex(theme_colors[theme_index][13]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj120) lv_obj_set_style_text_color(objects.obj120, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fifth_parallel) lv_obj_set_style_text_color(objects.fifth_parallel, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj121) lv_obj_set_style_text_color(objects.obj121, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fifth_subdominant) lv_obj_set_style_text_color(objects.fifth_subdominant, lv_color_hex(theme_colors[theme_index][13]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fifth_led_ai) lv_led_set_color(objects.fifth_led_ai, lv_color_hex(theme_colors[theme_index][4]));
    }
    {
        if (objects.app_tiny_piano) lv_obj_set_style_bg_color(objects.app_tiny_piano, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.app_tiny_piano) lv_obj_set_style_text_color(objects.app_tiny_piano, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj124) lv_obj_set_style_text_color(objects.obj124, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_btn_home) lv_obj_set_style_text_color(objects.piano_btn_home, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_btn_home) lv_obj_set_style_bg_color(objects.piano_btn_home, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_btn_rec) lv_obj_set_style_text_color(objects.piano_btn_rec, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_btn_rec) lv_obj_set_style_bg_color(objects.piano_btn_rec, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_btn_rec) lv_obj_set_style_bg_color(objects.piano_btn_rec, lv_color_hex(theme_colors[theme_index][8]), LV_PART_MAIN | LV_STATE_CHECKED);
        if (objects.obj125) lv_obj_set_style_text_color(objects.obj125, lv_color_hex(theme_colors[theme_index][8]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_btn_set) lv_obj_set_style_text_color(objects.piano_btn_set, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_btn_set) lv_obj_set_style_bg_color(objects.piano_btn_set, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj126) lv_obj_set_style_text_color(objects.obj126, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad0) lv_obj_set_style_bg_color(objects.piano_pad0, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad0) lv_obj_set_style_shadow_color(objects.piano_pad0, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad1) lv_obj_set_style_bg_color(objects.piano_pad1, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad1) lv_obj_set_style_shadow_color(objects.piano_pad1, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad2) lv_obj_set_style_bg_color(objects.piano_pad2, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad2) lv_obj_set_style_shadow_color(objects.piano_pad2, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad3) lv_obj_set_style_bg_color(objects.piano_pad3, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad3) lv_obj_set_style_shadow_color(objects.piano_pad3, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad4) lv_obj_set_style_bg_color(objects.piano_pad4, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad4) lv_obj_set_style_shadow_color(objects.piano_pad4, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad5) lv_obj_set_style_bg_color(objects.piano_pad5, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad5) lv_obj_set_style_shadow_color(objects.piano_pad5, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad6) lv_obj_set_style_bg_color(objects.piano_pad6, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad6) lv_obj_set_style_shadow_color(objects.piano_pad6, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad7) lv_obj_set_style_bg_color(objects.piano_pad7, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad7) lv_obj_set_style_shadow_color(objects.piano_pad7, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad8) lv_obj_set_style_bg_color(objects.piano_pad8, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad8) lv_obj_set_style_shadow_color(objects.piano_pad8, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad9) lv_obj_set_style_bg_color(objects.piano_pad9, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad9) lv_obj_set_style_shadow_color(objects.piano_pad9, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad10) lv_obj_set_style_bg_color(objects.piano_pad10, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad10) lv_obj_set_style_shadow_color(objects.piano_pad10, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad11) lv_obj_set_style_bg_color(objects.piano_pad11, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad11) lv_obj_set_style_shadow_color(objects.piano_pad11, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad12) lv_obj_set_style_bg_color(objects.piano_pad12, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad12) lv_obj_set_style_shadow_color(objects.piano_pad12, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad13) lv_obj_set_style_bg_color(objects.piano_pad13, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad13) lv_obj_set_style_shadow_color(objects.piano_pad13, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad14) lv_obj_set_style_bg_color(objects.piano_pad14, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pad14) lv_obj_set_style_shadow_color(objects.piano_pad14, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj127) lv_obj_set_style_text_color(objects.obj127, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_root_v) lv_obj_set_style_text_color(objects.piano_root_v, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_root_v) lv_obj_set_style_bg_color(objects.piano_root_v, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_root_v) lv_obj_set_style_border_color(objects.piano_root_v, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_root_v) lv_obj_set_style_text_color(objects.piano_root_v, lv_color_hex(theme_colors[theme_index][5]), LV_PART_SELECTED | LV_STATE_DEFAULT);
        if (objects.piano_root_v) lv_obj_set_style_bg_color(objects.piano_root_v, lv_color_hex(theme_colors[theme_index][2]), LV_PART_SELECTED | LV_STATE_DEFAULT);
        if (objects.piano_set) lv_obj_set_style_bg_color(objects.piano_set, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_set) lv_obj_set_style_border_color(objects.piano_set, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_set_btn_return) lv_obj_set_style_text_color(objects.piano_set_btn_return, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_set_btn_return) lv_obj_set_style_bg_color(objects.piano_set_btn_return, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj128) lv_obj_set_style_text_color(objects.obj128, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj128) lv_obj_set_style_bg_color(objects.obj128, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_display_type) lv_obj_set_style_text_color(objects.piano_display_type, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_display_type) lv_obj_set_style_bg_color(objects.piano_display_type, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj129) lv_obj_set_style_bg_color(objects.obj129, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj129) lv_obj_set_style_bg_color(objects.obj129, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj129) lv_obj_set_style_text_color(objects.obj129, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj129) lv_obj_set_style_bg_color(objects.obj129, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        if (objects.obj130) lv_obj_set_style_text_color(objects.obj130, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj130) lv_obj_set_style_bg_color(objects.obj130, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_scale_type) lv_obj_set_style_text_color(objects.piano_scale_type, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_scale_type) lv_obj_set_style_bg_color(objects.piano_scale_type, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj131) lv_obj_set_style_bg_color(objects.obj131, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj131) lv_obj_set_style_bg_color(objects.obj131, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj131) lv_obj_set_style_text_color(objects.obj131, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj131) lv_obj_set_style_bg_color(objects.obj131, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        if (objects.obj132) lv_obj_set_style_text_color(objects.obj132, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj132) lv_obj_set_style_bg_color(objects.obj132, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pitch) lv_obj_set_style_text_color(objects.piano_pitch, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_pitch) lv_obj_set_style_bg_color(objects.piano_pitch, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj133) lv_obj_set_style_bg_color(objects.obj133, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj133) lv_obj_set_style_bg_color(objects.obj133, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj133) lv_obj_set_style_text_color(objects.obj133, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj133) lv_obj_set_style_bg_color(objects.obj133, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        if (objects.obj134) lv_obj_set_style_text_color(objects.obj134, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj134) lv_obj_set_style_bg_color(objects.obj134, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_sound_type) lv_obj_set_style_text_color(objects.piano_sound_type, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.piano_sound_type) lv_obj_set_style_bg_color(objects.piano_sound_type, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj135) lv_obj_set_style_bg_color(objects.obj135, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj135) lv_obj_set_style_bg_color(objects.obj135, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj135) lv_obj_set_style_text_color(objects.obj135, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj135) lv_obj_set_style_bg_color(objects.obj135, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        if (objects.piano_led_ai) lv_led_set_color(objects.piano_led_ai, lv_color_hex(theme_colors[theme_index][4]));
    }
    {
        if (objects.app_drum_pad) lv_obj_set_style_text_color(objects.app_drum_pad, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.app_drum_pad) lv_obj_set_style_bg_color(objects.app_drum_pad, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj140) lv_obj_set_style_text_color(objects.obj140, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj141) lv_obj_set_style_text_color(objects.obj141, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_btn_home) lv_obj_set_style_text_color(objects.drum_btn_home, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_btn_home) lv_obj_set_style_bg_color(objects.drum_btn_home, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_btn_rec) lv_obj_set_style_text_color(objects.drum_btn_rec, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_btn_rec) lv_obj_set_style_bg_color(objects.drum_btn_rec, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_btn_rec) lv_obj_set_style_bg_color(objects.drum_btn_rec, lv_color_hex(theme_colors[theme_index][8]), LV_PART_MAIN | LV_STATE_CHECKED);
        if (objects.obj142) lv_obj_set_style_text_color(objects.obj142, lv_color_hex(theme_colors[theme_index][8]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_btn_set) lv_obj_set_style_text_color(objects.drum_btn_set, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_btn_set) lv_obj_set_style_bg_color(objects.drum_btn_set, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_crash_m) lv_obj_set_style_text_color(objects.drum_crash_m, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_crash_m) lv_obj_set_style_bg_color(objects.drum_crash_m, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_crash_m) lv_obj_set_style_border_color(objects.drum_crash_m, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_crash_m) lv_obj_set_style_bg_color(objects.drum_crash_m, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_PRESSED);
        if (objects.drum_clap_m) lv_obj_set_style_bg_color(objects.drum_clap_m, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_PRESSED);
        if (objects.drum_clap_m) lv_obj_set_style_text_color(objects.drum_clap_m, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_clap_m) lv_obj_set_style_bg_color(objects.drum_clap_m, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_clap_m) lv_obj_set_style_border_color(objects.drum_clap_m, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_openhht_m) lv_obj_set_style_bg_color(objects.drum_openhht_m, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_PRESSED);
        if (objects.drum_openhht_m) lv_obj_set_style_text_color(objects.drum_openhht_m, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_openhht_m) lv_obj_set_style_bg_color(objects.drum_openhht_m, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_openhht_m) lv_obj_set_style_border_color(objects.drum_openhht_m, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_closedhh_m) lv_obj_set_style_bg_color(objects.drum_closedhh_m, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_PRESSED);
        if (objects.drum_closedhh_m) lv_obj_set_style_text_color(objects.drum_closedhh_m, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_closedhh_m) lv_obj_set_style_bg_color(objects.drum_closedhh_m, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_closedhh_m) lv_obj_set_style_border_color(objects.drum_closedhh_m, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_ride_m) lv_obj_set_style_bg_color(objects.drum_ride_m, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_PRESSED);
        if (objects.drum_ride_m) lv_obj_set_style_text_color(objects.drum_ride_m, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_ride_m) lv_obj_set_style_bg_color(objects.drum_ride_m, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_ride_m) lv_obj_set_style_border_color(objects.drum_ride_m, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_snare_n) lv_obj_set_style_bg_color(objects.drum_snare_n, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_PRESSED);
        if (objects.drum_snare_n) lv_obj_set_style_text_color(objects.drum_snare_n, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_snare_n) lv_obj_set_style_bg_color(objects.drum_snare_n, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_snare_n) lv_obj_set_style_border_color(objects.drum_snare_n, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_kick_m) lv_obj_set_style_text_color(objects.drum_kick_m, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_kick_m) lv_obj_set_style_bg_color(objects.drum_kick_m, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_kick_m) lv_obj_set_style_border_color(objects.drum_kick_m, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_kick_m) lv_obj_set_style_bg_color(objects.drum_kick_m, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_PRESSED);
        if (objects.drum_floortom_m) lv_obj_set_style_bg_color(objects.drum_floortom_m, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_PRESSED);
        if (objects.drum_floortom_m) lv_obj_set_style_text_color(objects.drum_floortom_m, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_floortom_m) lv_obj_set_style_bg_color(objects.drum_floortom_m, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_floortom_m) lv_obj_set_style_border_color(objects.drum_floortom_m, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_pad_set) lv_obj_set_style_text_color(objects.drum_pad_set, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_pad_set) lv_obj_set_style_bg_color(objects.drum_pad_set, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_pad_set) lv_obj_set_style_border_color(objects.drum_pad_set, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_set_btn_return) lv_obj_set_style_text_color(objects.drum_set_btn_return, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_set_btn_return) lv_obj_set_style_bg_color(objects.drum_set_btn_return, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj143) lv_obj_set_style_text_color(objects.obj143, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj143) lv_obj_set_style_bg_color(objects.obj143, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_display_type) lv_obj_set_style_text_color(objects.drum_display_type, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_display_type) lv_obj_set_style_bg_color(objects.drum_display_type, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj144) lv_obj_set_style_bg_color(objects.obj144, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj144) lv_obj_set_style_bg_color(objects.obj144, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj144) lv_obj_set_style_text_color(objects.obj144, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj144) lv_obj_set_style_bg_color(objects.obj144, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        if (objects.obj145) lv_obj_set_style_text_color(objects.obj145, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj145) lv_obj_set_style_bg_color(objects.obj145, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_sound_type) lv_obj_set_style_text_color(objects.drum_sound_type, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.drum_sound_type) lv_obj_set_style_bg_color(objects.drum_sound_type, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj146) lv_obj_set_style_bg_color(objects.obj146, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj146) lv_obj_set_style_bg_color(objects.obj146, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj146) lv_obj_set_style_text_color(objects.obj146, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj146) lv_obj_set_style_bg_color(objects.obj146, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        if (objects.drum_led_ai) lv_led_set_color(objects.drum_led_ai, lv_color_hex(theme_colors[theme_index][4]));
    }
    {
        if (objects.app_midi_player) lv_obj_set_style_text_color(objects.app_midi_player, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.app_midi_player) lv_obj_set_style_bg_color(objects.app_midi_player, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj151) lv_obj_set_style_text_color(objects.obj151, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj152) lv_obj_set_style_text_color(objects.obj152, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_btn_home) lv_obj_set_style_text_color(objects.midi_btn_home, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_btn_home) lv_obj_set_style_bg_color(objects.midi_btn_home, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_btn_set) lv_obj_set_style_text_color(objects.midi_btn_set, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_btn_set) lv_obj_set_style_bg_color(objects.midi_btn_set, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_panel_mid_list) lv_obj_set_style_text_color(objects.midi_panel_mid_list, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_panel_mid_list) lv_obj_set_style_bg_color(objects.midi_panel_mid_list, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_list_music_file) lv_obj_set_style_text_color(objects.midi_list_music_file, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_list_music_file) lv_obj_set_style_bg_color(objects.midi_list_music_file, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_file_example) lv_obj_set_style_text_color(objects.midi_file_example, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_file_example) lv_obj_set_style_bg_color(objects.midi_file_example, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_panel_hmr_list) lv_obj_set_style_text_color(objects.midi_panel_hmr_list, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_panel_hmr_list) lv_obj_set_style_bg_color(objects.midi_panel_hmr_list, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_list_record_file) lv_obj_set_style_text_color(objects.midi_list_record_file, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_list_record_file) lv_obj_set_style_bg_color(objects.midi_list_record_file, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj153) lv_obj_set_style_text_color(objects.obj153, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj153) lv_obj_set_style_bg_color(objects.obj153, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj154) lv_obj_set_style_text_color(objects.obj154, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj155) lv_obj_set_style_text_color(objects.obj155, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_music_path_label) lv_obj_set_style_text_color(objects.midi_music_path_label, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj156) lv_obj_set_style_text_color(objects.obj156, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_music_bpm_num) lv_obj_set_style_text_color(objects.midi_music_bpm_num, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj157) lv_obj_set_style_text_color(objects.obj157, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_music_track_count) lv_obj_set_style_text_color(objects.midi_music_track_count, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_prev) lv_obj_set_style_text_color(objects.midi_prev, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_prev) lv_obj_set_style_bg_color(objects.midi_prev, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_play_stop) lv_obj_set_style_text_color(objects.midi_play_stop, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_play_stop) lv_obj_set_style_bg_color(objects.midi_play_stop, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_next) lv_obj_set_style_text_color(objects.midi_next, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_next) lv_obj_set_style_bg_color(objects.midi_next, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj158) lv_obj_set_style_text_color(objects.obj158, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_progress) lv_obj_set_style_border_color(objects.midi_progress, lv_color_hex(theme_colors[theme_index][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_progress) lv_obj_set_style_bg_color(objects.midi_progress, lv_color_hex(theme_colors[theme_index][3]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        if (objects.midi_progress) lv_obj_set_style_border_color(objects.midi_progress, lv_color_hex(theme_colors[theme_index][3]), LV_PART_KNOB | LV_STATE_DEFAULT);
        if (objects.midi_set) lv_obj_set_style_bg_color(objects.midi_set, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_set) lv_obj_set_style_border_color(objects.midi_set, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_set_btn_return) lv_obj_set_style_text_color(objects.midi_set_btn_return, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_set_btn_return) lv_obj_set_style_bg_color(objects.midi_set_btn_return, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj159) lv_obj_set_style_text_color(objects.obj159, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj159) lv_obj_set_style_bg_color(objects.obj159, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_play_type) lv_obj_set_style_text_color(objects.midi_play_type, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.midi_play_type) lv_obj_set_style_bg_color(objects.midi_play_type, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj160) lv_obj_set_style_bg_color(objects.obj160, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj160) lv_obj_set_style_bg_color(objects.obj160, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj160) lv_obj_set_style_text_color(objects.obj160, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj160) lv_obj_set_style_bg_color(objects.obj160, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        if (objects.midi_led_ai) lv_led_set_color(objects.midi_led_ai, lv_color_hex(theme_colors[theme_index][4]));
    }
    {
        if (objects.app_xy_mode) lv_obj_set_style_text_color(objects.app_xy_mode, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.app_xy_mode) lv_obj_set_style_bg_color(objects.app_xy_mode, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj167) lv_obj_set_style_line_color(objects.obj167, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj168) lv_obj_set_style_line_color(objects.obj168, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj169) lv_obj_set_style_line_color(objects.obj169, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj170) lv_obj_set_style_line_color(objects.obj170, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj171) lv_obj_set_style_text_color(objects.obj171, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj172) lv_obj_set_style_text_color(objects.obj172, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.xy_btn_home) lv_obj_set_style_text_color(objects.xy_btn_home, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.xy_btn_home) lv_obj_set_style_bg_color(objects.xy_btn_home, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.xy_btn_rec) lv_obj_set_style_text_color(objects.xy_btn_rec, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.xy_btn_rec) lv_obj_set_style_bg_color(objects.xy_btn_rec, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.xy_btn_rec) lv_obj_set_style_bg_color(objects.xy_btn_rec, lv_color_hex(theme_colors[theme_index][8]), LV_PART_MAIN | LV_STATE_CHECKED);
        if (objects.obj173) lv_obj_set_style_text_color(objects.obj173, lv_color_hex(theme_colors[theme_index][8]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.xy_btn_set) lv_obj_set_style_text_color(objects.xy_btn_set, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.xy_btn_set) lv_obj_set_style_bg_color(objects.xy_btn_set, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.xy_point_1) lv_led_set_color(objects.xy_point_1, lv_color_hex(theme_colors[theme_index][4]));
        if (objects.xy_point_2) lv_led_set_color(objects.xy_point_2, lv_color_hex(theme_colors[theme_index][13]));
        if (objects.xy_point_3) lv_led_set_color(objects.xy_point_3, lv_color_hex(theme_colors[theme_index][14]));
        if (objects.xy_set) lv_obj_set_style_text_color(objects.xy_set, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.xy_set) lv_obj_set_style_bg_color(objects.xy_set, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.xy_set) lv_obj_set_style_border_color(objects.xy_set, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.xy_set_btn_return) lv_obj_set_style_shadow_color(objects.xy_set_btn_return, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.xy_set_btn_return) lv_obj_set_style_text_color(objects.xy_set_btn_return, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.xy_set_btn_return) lv_obj_set_style_bg_color(objects.xy_set_btn_return, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj174) lv_obj_set_style_text_color(objects.obj174, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj174) lv_obj_set_style_bg_color(objects.obj174, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.xy_sound) lv_obj_set_style_text_color(objects.xy_sound, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.xy_sound) lv_obj_set_style_bg_color(objects.xy_sound, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj175) lv_obj_set_style_bg_color(objects.obj175, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj175) lv_obj_set_style_bg_color(objects.obj175, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj175) lv_obj_set_style_text_color(objects.obj175, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj175) lv_obj_set_style_bg_color(objects.obj175, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        if (objects.obj176) lv_obj_set_style_text_color(objects.obj176, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj176) lv_obj_set_style_bg_color(objects.obj176, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.xy_step) lv_obj_set_style_text_color(objects.xy_step, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.xy_step) lv_obj_set_style_bg_color(objects.xy_step, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj177) lv_obj_set_style_bg_color(objects.obj177, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj177) lv_obj_set_style_bg_color(objects.obj177, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj177) lv_obj_set_style_text_color(objects.obj177, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj177) lv_obj_set_style_bg_color(objects.obj177, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        if (objects.xy_led_ai) lv_led_set_color(objects.xy_led_ai, lv_color_hex(theme_colors[theme_index][4]));
    }
    {
        if (objects.app_metronome) lv_obj_set_style_text_color(objects.app_metronome, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.app_metronome) lv_obj_set_style_bg_color(objects.app_metronome, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj182) lv_obj_set_style_text_color(objects.obj182, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj183) lv_obj_set_style_text_color(objects.obj183, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.metron_btn_home) lv_obj_set_style_text_color(objects.metron_btn_home, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.metron_btn_home) lv_obj_set_style_bg_color(objects.metron_btn_home, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.metron_btn_set) lv_obj_set_style_text_color(objects.metron_btn_set, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.metron_btn_set) lv_obj_set_style_bg_color(objects.metron_btn_set, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.metron_panel) lv_obj_set_style_text_color(objects.metron_panel, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.metron_btn_minus) lv_obj_set_style_text_color(objects.metron_btn_minus, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.metron_btn_minus) lv_obj_set_style_bg_color(objects.metron_btn_minus, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj184) lv_obj_set_style_text_color(objects.obj184, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.metron_btn_plus) lv_obj_set_style_text_color(objects.metron_btn_plus, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.metron_btn_plus) lv_obj_set_style_bg_color(objects.metron_btn_plus, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj185) lv_obj_set_style_text_color(objects.obj185, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.metron_led_heavy) lv_led_set_color(objects.metron_led_heavy, lv_color_hex(theme_colors[theme_index][8]));
        if (objects.metron_led_1) lv_led_set_color(objects.metron_led_1, lv_color_hex(theme_colors[theme_index][7]));
        if (objects.metron_led_2) lv_led_set_color(objects.metron_led_2, lv_color_hex(theme_colors[theme_index][7]));
        if (objects.metron_led_3) lv_led_set_color(objects.metron_led_3, lv_color_hex(theme_colors[theme_index][7]));
        if (objects.metron_led_4) lv_led_set_color(objects.metron_led_4, lv_color_hex(theme_colors[theme_index][7]));
        if (objects.metron_led_5) lv_led_set_color(objects.metron_led_5, lv_color_hex(theme_colors[theme_index][7]));
        if (objects.metron_led_6) lv_led_set_color(objects.metron_led_6, lv_color_hex(theme_colors[theme_index][7]));
        if (objects.metron_led_7) lv_led_set_color(objects.metron_led_7, lv_color_hex(theme_colors[theme_index][7]));
        if (objects.metron_led_8) lv_led_set_color(objects.metron_led_8, lv_color_hex(theme_colors[theme_index][7]));
        if (objects.metron_led_9) lv_led_set_color(objects.metron_led_9, lv_color_hex(theme_colors[theme_index][7]));
        if (objects.metron_led_10) lv_led_set_color(objects.metron_led_10, lv_color_hex(theme_colors[theme_index][7]));
        if (objects.metron_led_11) lv_led_set_color(objects.metron_led_11, lv_color_hex(theme_colors[theme_index][7]));
        if (objects.metron_led_12) lv_led_set_color(objects.metron_led_12, lv_color_hex(theme_colors[theme_index][7]));
        if (objects.metron_led_13) lv_led_set_color(objects.metron_led_13, lv_color_hex(theme_colors[theme_index][7]));
        if (objects.metron_led_14) lv_led_set_color(objects.metron_led_14, lv_color_hex(theme_colors[theme_index][7]));
        if (objects.metron_led_15) lv_led_set_color(objects.metron_led_15, lv_color_hex(theme_colors[theme_index][7]));
        if (objects.obj186) lv_obj_set_style_text_color(objects.obj186, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.metron_timesig_top) lv_obj_set_style_text_color(objects.metron_timesig_top, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.metron_timesig_top) lv_obj_set_style_bg_color(objects.metron_timesig_top, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj187) lv_obj_set_style_bg_color(objects.obj187, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj187) lv_obj_set_style_bg_color(objects.obj187, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj187) lv_obj_set_style_bg_color(objects.obj187, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        if (objects.metron_timesig_bot) lv_obj_set_style_text_color(objects.metron_timesig_bot, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.metron_timesig_bot) lv_obj_set_style_bg_color(objects.metron_timesig_bot, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj188) lv_obj_set_style_bg_color(objects.obj188, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj188) lv_obj_set_style_bg_color(objects.obj188, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj188) lv_obj_set_style_bg_color(objects.obj188, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        if (objects.metron_btn_tempo) lv_obj_set_style_text_color(objects.metron_btn_tempo, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.metron_btn_tempo) lv_obj_set_style_bg_color(objects.metron_btn_tempo, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.metron_slider_bpm) lv_obj_set_style_bg_color(objects.metron_slider_bpm, lv_color_hex(theme_colors[theme_index][3]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.metron_slider_bpm) lv_obj_set_style_bg_color(objects.metron_slider_bpm, lv_color_hex(theme_colors[theme_index][3]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        if (objects.metron_slider_bpm) lv_obj_set_style_border_color(objects.metron_slider_bpm, lv_color_hex(theme_colors[theme_index][3]), LV_PART_KNOB | LV_STATE_DEFAULT);
        if (objects.metron_btn_play_stop) lv_obj_set_style_bg_color(objects.metron_btn_play_stop, lv_color_hex(theme_colors[theme_index][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.metron_btn_play_stop) lv_obj_set_style_shadow_color(objects.metron_btn_play_stop, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.metron_btn_play_stop_label) lv_obj_set_style_text_color(objects.metron_btn_play_stop_label, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.metron_set) lv_obj_set_style_bg_color(objects.metron_set, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.metron_set_btn_return) lv_obj_set_style_text_color(objects.metron_set_btn_return, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.metron_set_btn_return) lv_obj_set_style_bg_color(objects.metron_set_btn_return, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj189) lv_obj_set_style_bg_color(objects.obj189, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj189) lv_obj_set_style_text_color(objects.obj189, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.metron_sound) lv_obj_set_style_bg_color(objects.metron_sound, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.metron_sound) lv_obj_set_style_text_color(objects.metron_sound, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj190) lv_obj_set_style_bg_color(objects.obj190, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj190) lv_obj_set_style_bg_color(objects.obj190, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj190) lv_obj_set_style_text_color(objects.obj190, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj190) lv_obj_set_style_bg_color(objects.obj190, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        if (objects.metron_led_ai) lv_led_set_color(objects.metron_led_ai, lv_color_hex(theme_colors[theme_index][4]));
    }
    {
        if (objects.app_ai_agent) lv_obj_set_style_text_color(objects.app_ai_agent, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.app_ai_agent) lv_obj_set_style_bg_color(objects.app_ai_agent, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj195) lv_obj_set_style_text_color(objects.obj195, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ai_btn_home) lv_obj_set_style_bg_color(objects.ai_btn_home, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ai_btn_home) lv_obj_set_style_text_color(objects.ai_btn_home, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj196) lv_obj_set_style_text_color(objects.obj196, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj197) lv_obj_set_style_text_color(objects.obj197, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj197) lv_obj_set_style_border_color(objects.obj197, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ai_context_user0) lv_obj_set_style_text_color(objects.ai_context_user0, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ai_context_ai0) lv_obj_set_style_text_color(objects.ai_context_ai0, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj198) lv_obj_set_style_text_color(objects.obj198, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ai_context_ai0_text) lv_obj_set_style_text_color(objects.ai_context_ai0_text, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ai_btn_set) lv_obj_set_style_text_color(objects.ai_btn_set, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ai_btn_set) lv_obj_set_style_bg_color(objects.ai_btn_set, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ai_btn_speak) lv_obj_set_style_text_color(objects.ai_btn_speak, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ai_btn_speak) lv_obj_set_style_bg_color(objects.ai_btn_speak, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ai_set) lv_obj_set_style_text_color(objects.ai_set, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ai_set) lv_obj_set_style_bg_color(objects.ai_set, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ai_set) lv_obj_set_style_border_color(objects.ai_set, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ai_set_btn_return) lv_obj_set_style_text_color(objects.ai_set_btn_return, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ai_set_btn_return) lv_obj_set_style_bg_color(objects.ai_set_btn_return, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj199) lv_obj_set_style_text_color(objects.obj199, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj199) lv_obj_set_style_bg_color(objects.obj199, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ai_switch_save_text) lv_obj_set_style_bg_color(objects.ai_switch_save_text, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ai_switch_save_text) lv_obj_set_style_bg_color(objects.ai_switch_save_text, lv_color_hex(theme_colors[theme_index][3]), LV_PART_INDICATOR | LV_STATE_CHECKED);
        if (objects.obj200) lv_obj_set_style_text_color(objects.obj200, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj200) lv_obj_set_style_bg_color(objects.obj200, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ai_switch_wake_anywhere) lv_obj_set_style_bg_color(objects.ai_switch_wake_anywhere, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ai_switch_wake_anywhere) lv_obj_set_style_bg_color(objects.ai_switch_wake_anywhere, lv_color_hex(theme_colors[theme_index][3]), LV_PART_INDICATOR | LV_STATE_CHECKED);
        if (objects.obj201) lv_obj_set_style_text_color(objects.obj201, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj201) lv_obj_set_style_bg_color(objects.obj201, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ai_btn_config_reset) lv_obj_set_style_text_color(objects.ai_btn_config_reset, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ai_btn_config_reset) lv_obj_set_style_bg_color(objects.ai_btn_config_reset, lv_color_hex(theme_colors[theme_index][8]), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    {
        if (objects.app_clock) lv_obj_set_style_text_color(objects.app_clock, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.app_clock) lv_obj_set_style_bg_color(objects.app_clock, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj207) lv_obj_set_style_text_color(objects.obj207, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj208) lv_obj_set_style_text_color(objects.obj208, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_btn_home) lv_obj_set_style_bg_color(objects.clock_btn_home, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_btn_home) lv_obj_set_style_text_color(objects.clock_btn_home, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_btn_set) lv_obj_set_style_text_color(objects.clock_btn_set, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_btn_set) lv_obj_set_style_bg_color(objects.clock_btn_set, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_panel_clock) lv_obj_set_style_text_color(objects.clock_panel_clock, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_panel_clock) lv_obj_set_style_bg_color(objects.clock_panel_clock, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj209) lv_obj_set_style_text_color(objects.obj209, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj209) lv_obj_set_style_bg_color(objects.obj209, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_clock_lunar) lv_obj_set_style_text_color(objects.clock_clock_lunar, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_clock_hitokoto) lv_obj_set_style_text_color(objects.clock_clock_hitokoto, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_panel_weather) lv_obj_set_style_bg_color(objects.clock_panel_weather, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj210) lv_obj_set_style_bg_color(objects.obj210, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj211) lv_obj_set_style_text_color(objects.obj211, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj211) lv_obj_set_style_bg_color(objects.obj211, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.weather_today_text_1) lv_obj_set_style_text_color(objects.weather_today_text_1, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj212) lv_obj_set_style_text_color(objects.obj212, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj212) lv_obj_set_style_bg_color(objects.obj212, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj213) lv_obj_set_style_text_color(objects.obj213, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj213) lv_obj_set_style_bg_color(objects.obj213, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj214) lv_obj_set_style_text_color(objects.obj214, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj214) lv_obj_set_style_bg_color(objects.obj214, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.weather_day2) lv_obj_set_style_text_color(objects.weather_day2, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.weather_day2_1) lv_obj_set_style_text_color(objects.weather_day2_1, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj215) lv_obj_set_style_text_color(objects.obj215, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj215) lv_obj_set_style_bg_color(objects.obj215, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.weather_day3) lv_obj_set_style_text_color(objects.weather_day3, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.weather_day3_1) lv_obj_set_style_text_color(objects.weather_day3_1, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.weatehr_panel_news) lv_obj_set_style_text_color(objects.weatehr_panel_news, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.weatehr_panel_news) lv_obj_set_style_bg_color(objects.weatehr_panel_news, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_panel_calender) lv_obj_set_style_text_color(objects.clock_panel_calender, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_panel_calender) lv_obj_set_style_bg_color(objects.clock_panel_calender, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_calender) lv_obj_set_style_text_color(objects.clock_calender, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_calender) lv_obj_set_style_bg_color(objects.clock_calender, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_calender) lv_obj_set_style_text_color(objects.clock_calender, lv_color_hex(theme_colors[theme_index][5]), LV_PART_ITEMS | LV_STATE_DEFAULT);
        if (objects.obj216) lv_obj_set_style_bg_color(objects.obj216, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj217) lv_obj_set_style_text_color(objects.obj217, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj217) lv_obj_set_style_bg_color(objects.obj217, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_calender_progress_bar) lv_obj_set_style_bg_color(objects.clock_calender_progress_bar, lv_color_hex(theme_colors[theme_index][3]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_calender_progress_bar) lv_obj_set_style_bg_color(objects.clock_calender_progress_bar, lv_color_hex(theme_colors[theme_index][4]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        if (objects.clock_calender_panel_huangli) lv_obj_set_style_text_color(objects.clock_calender_panel_huangli, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_calender_panel_huangli) lv_obj_set_style_bg_color(objects.clock_calender_panel_huangli, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_calender_huangli_1) lv_obj_set_style_text_color(objects.clock_calender_huangli_1, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_calender_huangli_2) lv_obj_set_style_text_color(objects.clock_calender_huangli_2, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_calender_huangli_3) lv_obj_set_style_text_color(objects.clock_calender_huangli_3, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_calender_huangli_4) lv_obj_set_style_text_color(objects.clock_calender_huangli_4, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_calender_huangli_5) lv_obj_set_style_text_color(objects.clock_calender_huangli_5, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_calender_huangli_6) lv_obj_set_style_text_color(objects.clock_calender_huangli_6, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_panel_timer) lv_obj_set_style_text_color(objects.clock_panel_timer, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_panel_timer) lv_obj_set_style_bg_color(objects.clock_panel_timer, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj218) lv_obj_set_style_text_color(objects.obj218, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_bell) lv_obj_set_style_text_color(objects.clock_timer_bell, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj219) lv_obj_set_style_bg_color(objects.obj219, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj219) lv_obj_set_style_text_color(objects.obj219, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_btn_min_1min) lv_obj_set_style_bg_color(objects.clock_timer_btn_min_1min, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_btn_min_1min) lv_obj_set_style_text_color(objects.clock_timer_btn_min_1min, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_btn_add_1min) lv_obj_set_style_bg_color(objects.clock_timer_btn_add_1min, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_btn_add_1min) lv_obj_set_style_text_color(objects.clock_timer_btn_add_1min, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj220) lv_obj_set_style_bg_color(objects.obj220, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj221) lv_obj_set_style_bg_color(objects.obj221, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_quick_1min) lv_obj_set_style_bg_color(objects.clock_timer_quick_1min, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_quick_1min) lv_obj_set_style_text_color(objects.clock_timer_quick_1min, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_quick_3min) lv_obj_set_style_bg_color(objects.clock_timer_quick_3min, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_quick_3min) lv_obj_set_style_text_color(objects.clock_timer_quick_3min, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_quick_5min) lv_obj_set_style_bg_color(objects.clock_timer_quick_5min, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_quick_5min) lv_obj_set_style_text_color(objects.clock_timer_quick_5min, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_quick_10min) lv_obj_set_style_bg_color(objects.clock_timer_quick_10min, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_quick_10min) lv_obj_set_style_text_color(objects.clock_timer_quick_10min, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_quick_20min) lv_obj_set_style_bg_color(objects.clock_timer_quick_20min, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_quick_20min) lv_obj_set_style_text_color(objects.clock_timer_quick_20min, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_quick_30min) lv_obj_set_style_bg_color(objects.clock_timer_quick_30min, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_quick_30min) lv_obj_set_style_text_color(objects.clock_timer_quick_30min, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_quick_40min) lv_obj_set_style_bg_color(objects.clock_timer_quick_40min, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_quick_40min) lv_obj_set_style_text_color(objects.clock_timer_quick_40min, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_quick_50min) lv_obj_set_style_bg_color(objects.clock_timer_quick_50min, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_quick_50min) lv_obj_set_style_text_color(objects.clock_timer_quick_50min, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_quick_60min) lv_obj_set_style_bg_color(objects.clock_timer_quick_60min, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_quick_60min) lv_obj_set_style_text_color(objects.clock_timer_quick_60min, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_reset) lv_obj_set_style_bg_color(objects.clock_timer_reset, lv_color_hex(theme_colors[theme_index][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_reset) lv_obj_set_style_text_color(objects.clock_timer_reset, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_start_pause) lv_obj_set_style_bg_color(objects.clock_timer_start_pause, lv_color_hex(theme_colors[theme_index][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_timer_start_pause) lv_obj_set_style_text_color(objects.clock_timer_start_pause, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_set) lv_obj_set_style_bg_color(objects.clock_set, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_set) lv_obj_set_style_border_color(objects.clock_set, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_set_btn_return) lv_obj_set_style_shadow_color(objects.clock_set_btn_return, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_set_btn_return) lv_obj_set_style_text_color(objects.clock_set_btn_return, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_set_btn_return) lv_obj_set_style_bg_color(objects.clock_set_btn_return, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj222) lv_obj_set_style_text_color(objects.obj222, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj222) lv_obj_set_style_bg_color(objects.obj222, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_set_12_24h) lv_obj_set_style_text_color(objects.clock_set_12_24h, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_set_12_24h) lv_obj_set_style_bg_color(objects.clock_set_12_24h, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj223) lv_obj_set_style_bg_color(objects.obj223, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj223) lv_obj_set_style_bg_color(objects.obj223, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj223) lv_obj_set_style_text_color(objects.obj223, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj223) lv_obj_set_style_bg_color(objects.obj223, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        if (objects.obj224) lv_obj_set_style_text_color(objects.obj224, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj224) lv_obj_set_style_bg_color(objects.obj224, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_set_time_font) lv_obj_set_style_text_color(objects.clock_set_time_font, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_set_time_font) lv_obj_set_style_bg_color(objects.clock_set_time_font, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj225) lv_obj_set_style_bg_color(objects.obj225, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SELECTED | LV_STATE_CHECKED);
        if (objects.obj225) lv_obj_set_style_bg_color(objects.obj225, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj225) lv_obj_set_style_text_color(objects.obj225, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj225) lv_obj_set_style_bg_color(objects.obj225, lv_color_hex(theme_colors[theme_index][3]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        if (objects.clock_btn_clock) lv_obj_set_style_text_color(objects.clock_btn_clock, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_btn_clock) lv_obj_set_style_bg_color(objects.clock_btn_clock, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_btn_clock) lv_obj_set_style_bg_color(objects.clock_btn_clock, lv_color_hex(theme_colors[theme_index][3]), LV_PART_MAIN | LV_STATE_PRESSED);
        if (objects.clock_btn_calender) lv_obj_set_style_text_color(objects.clock_btn_calender, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_btn_calender) lv_obj_set_style_bg_color(objects.clock_btn_calender, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_btn_calender) lv_obj_set_style_bg_color(objects.clock_btn_calender, lv_color_hex(theme_colors[theme_index][3]), LV_PART_MAIN | LV_STATE_PRESSED);
        if (objects.clock_btn_timer) lv_obj_set_style_text_color(objects.clock_btn_timer, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_btn_timer) lv_obj_set_style_bg_color(objects.clock_btn_timer, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.clock_btn_timer) lv_obj_set_style_bg_color(objects.clock_btn_timer, lv_color_hex(theme_colors[theme_index][3]), LV_PART_MAIN | LV_STATE_PRESSED);
        if (objects.clock_led_ai) lv_led_set_color(objects.clock_led_ai, lv_color_hex(theme_colors[theme_index][4]));
    }
    {
        if (objects.app_fun) lv_obj_set_style_text_color(objects.app_fun, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.app_fun) lv_obj_set_style_bg_color(objects.app_fun, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj231) lv_obj_set_style_text_color(objects.obj231, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fun_btn_home) lv_obj_set_style_bg_color(objects.fun_btn_home, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fun_btn_home) lv_obj_set_style_text_color(objects.fun_btn_home, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fun_btn_set) lv_obj_set_style_text_color(objects.fun_btn_set, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fun_btn_set) lv_obj_set_style_bg_color(objects.fun_btn_set, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fun_panel_book) lv_obj_set_style_text_color(objects.fun_panel_book, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fun_panel_book) lv_obj_set_style_bg_color(objects.fun_panel_book, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj233) lv_obj_set_style_text_color(objects.obj233, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj234) lv_obj_set_style_text_color(objects.obj234, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj235) lv_obj_set_style_text_color(objects.obj235, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj236) lv_obj_set_style_text_color(objects.obj236, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_book_open) lv_obj_set_style_text_color(objects.panel_book_open, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_book_open) lv_obj_set_style_bg_color(objects.panel_book_open, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_book_open) lv_obj_set_style_shadow_color(objects.panel_book_open, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.book_answer_text) lv_obj_set_style_text_color(objects.book_answer_text, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj237) lv_obj_set_style_line_color(objects.obj237, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.book_answer_detail) lv_obj_set_style_text_color(objects.book_answer_detail, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_book_close) lv_obj_set_style_text_color(objects.panel_book_close, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_book_close) lv_obj_set_style_bg_color(objects.panel_book_close, lv_color_hex(theme_colors[theme_index][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_book_close) lv_obj_set_style_shadow_color(objects.panel_book_close, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj238) lv_obj_set_style_text_color(objects.obj238, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj239) lv_obj_set_style_text_color(objects.obj239, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj240) lv_obj_set_style_text_color(objects.obj240, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fun_panel_tarot) lv_obj_set_style_text_color(objects.fun_panel_tarot, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fun_panel_tarot) lv_obj_set_style_bg_color(objects.fun_panel_tarot, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj241) lv_obj_set_style_text_color(objects.obj241, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj242) lv_obj_set_style_text_color(objects.obj242, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj243) lv_obj_set_style_text_color(objects.obj243, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj244) lv_obj_set_style_text_color(objects.obj244, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj245) lv_obj_set_style_text_color(objects.obj245, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_tarot_open_1) lv_obj_set_style_text_color(objects.panel_tarot_open_1, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_tarot_open_1) lv_obj_set_style_bg_color(objects.panel_tarot_open_1, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_tarot_open_1) lv_obj_set_style_shadow_color(objects.panel_tarot_open_1, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_tarot_open_1) lv_obj_set_style_border_color(objects.panel_tarot_open_1, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj246) lv_obj_set_style_line_color(objects.obj246, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.tarot_card_reversed_1) lv_obj_set_style_text_color(objects.tarot_card_reversed_1, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.tarot_card_detail_panel_1) lv_obj_set_style_bg_color(objects.tarot_card_detail_panel_1, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.tarot_card_detail_text_1) lv_obj_set_style_text_color(objects.tarot_card_detail_text_1, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_tarot_close_1) lv_obj_set_style_bg_color(objects.panel_tarot_close_1, lv_color_hex(theme_colors[theme_index][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_tarot_close_1) lv_obj_set_style_shadow_color(objects.panel_tarot_close_1, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_tarot_close_1) lv_obj_set_style_text_color(objects.panel_tarot_close_1, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj247) lv_obj_set_style_text_color(objects.obj247, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_tarot_open_2) lv_obj_set_style_text_color(objects.panel_tarot_open_2, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_tarot_open_2) lv_obj_set_style_bg_color(objects.panel_tarot_open_2, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_tarot_open_2) lv_obj_set_style_shadow_color(objects.panel_tarot_open_2, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_tarot_open_2) lv_obj_set_style_border_color(objects.panel_tarot_open_2, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj248) lv_obj_set_style_line_color(objects.obj248, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.tarot_card_reversed_2) lv_obj_set_style_text_color(objects.tarot_card_reversed_2, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.tarot_card_detail_panel_2) lv_obj_set_style_bg_color(objects.tarot_card_detail_panel_2, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.tarot_card_detail_text_2) lv_obj_set_style_text_color(objects.tarot_card_detail_text_2, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_tarot_close_2) lv_obj_set_style_bg_color(objects.panel_tarot_close_2, lv_color_hex(theme_colors[theme_index][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_tarot_close_2) lv_obj_set_style_shadow_color(objects.panel_tarot_close_2, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_tarot_close_2) lv_obj_set_style_text_color(objects.panel_tarot_close_2, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj249) lv_obj_set_style_text_color(objects.obj249, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_tarot_open_3) lv_obj_set_style_text_color(objects.panel_tarot_open_3, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_tarot_open_3) lv_obj_set_style_bg_color(objects.panel_tarot_open_3, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_tarot_open_3) lv_obj_set_style_shadow_color(objects.panel_tarot_open_3, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_tarot_open_3) lv_obj_set_style_border_color(objects.panel_tarot_open_3, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj250) lv_obj_set_style_line_color(objects.obj250, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.tarot_card_reversed_3) lv_obj_set_style_text_color(objects.tarot_card_reversed_3, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.tarot_card_detail_panel_3) lv_obj_set_style_bg_color(objects.tarot_card_detail_panel_3, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.tarot_card_detail_text_3) lv_obj_set_style_text_color(objects.tarot_card_detail_text_3, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_tarot_close_3) lv_obj_set_style_bg_color(objects.panel_tarot_close_3, lv_color_hex(theme_colors[theme_index][4]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_tarot_close_3) lv_obj_set_style_shadow_color(objects.panel_tarot_close_3, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.panel_tarot_close_3) lv_obj_set_style_text_color(objects.panel_tarot_close_3, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fun_tip_label) lv_obj_set_style_text_color(objects.fun_tip_label, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fun_set) lv_obj_set_style_text_color(objects.fun_set, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fun_set) lv_obj_set_style_bg_color(objects.fun_set, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fun_set) lv_obj_set_style_border_color(objects.fun_set, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fun_btn_book) lv_obj_set_style_text_color(objects.fun_btn_book, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fun_btn_book) lv_obj_set_style_bg_color(objects.fun_btn_book, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fun_btn_book) lv_obj_set_style_bg_color(objects.fun_btn_book, lv_color_hex(theme_colors[theme_index][3]), LV_PART_MAIN | LV_STATE_PRESSED);
        if (objects.fun_btn_tarot) lv_obj_set_style_text_color(objects.fun_btn_tarot, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fun_btn_tarot) lv_obj_set_style_bg_color(objects.fun_btn_tarot, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.fun_btn_tarot) lv_obj_set_style_bg_color(objects.fun_btn_tarot, lv_color_hex(theme_colors[theme_index][3]), LV_PART_MAIN | LV_STATE_PRESSED);
        if (objects.fun_led_ai) lv_led_set_color(objects.fun_led_ai, lv_color_hex(theme_colors[theme_index][4]));
    }
    {
        if (objects.ftp) lv_obj_set_style_bg_color(objects.ftp, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ftp) lv_obj_set_style_text_color(objects.ftp, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj254) lv_obj_set_style_text_color(objects.obj254, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ftp_btn_back2setting) lv_obj_set_style_bg_color(objects.ftp_btn_back2setting, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ftp_btn_back2setting) lv_obj_set_style_text_color(objects.ftp_btn_back2setting, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj255) lv_obj_set_style_text_color(objects.obj255, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj256) lv_obj_set_style_bg_color(objects.obj256, lv_color_hex(theme_colors[theme_index][0]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj256) lv_obj_set_style_text_color(objects.obj256, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj257) lv_obj_set_style_text_color(objects.obj257, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj257) lv_obj_set_style_bg_color(objects.obj257, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj258) lv_obj_set_style_text_color(objects.obj258, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj259) lv_obj_set_style_text_color(objects.obj259, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj259) lv_obj_set_style_bg_color(objects.obj259, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj260) lv_obj_set_style_text_color(objects.obj260, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj261) lv_obj_set_style_text_color(objects.obj261, lv_color_hex(theme_colors[theme_index][5]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj261) lv_obj_set_style_bg_color(objects.obj261, lv_color_hex(theme_colors[theme_index][2]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj262) lv_obj_set_style_text_color(objects.obj262, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.obj263) lv_obj_set_style_text_color(objects.obj263, lv_color_hex(theme_colors[theme_index][6]), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (objects.ftp_bar_progress) lv_obj_set_style_bg_color(objects.ftp_bar_progress, lv_color_hex(theme_colors[theme_index][3]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        if (objects.ftp_bar_progress) lv_obj_set_style_bg_color(objects.ftp_bar_progress, lv_color_hex(theme_colors[theme_index][1]), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (objects.boot) lv_obj_invalidate(objects.boot);
    if (objects.onboard_step) lv_obj_invalidate(objects.onboard_step);
    if (objects.launcher) lv_obj_invalidate(objects.launcher);
    if (objects.setting) lv_obj_invalidate(objects.setting);
    if (objects.about) lv_obj_invalidate(objects.about);
    if (objects.app_zen_mode) lv_obj_invalidate(objects.app_zen_mode);
    if (objects.app_ear_train) lv_obj_invalidate(objects.app_ear_train);
    if (objects.app_chord_memory) lv_obj_invalidate(objects.app_chord_memory);
    if (objects.app_circle_of_fifths) lv_obj_invalidate(objects.app_circle_of_fifths);
    if (objects.app_tiny_piano) lv_obj_invalidate(objects.app_tiny_piano);
    if (objects.app_drum_pad) lv_obj_invalidate(objects.app_drum_pad);
    if (objects.app_midi_player) lv_obj_invalidate(objects.app_midi_player);
    if (objects.app_xy_mode) lv_obj_invalidate(objects.app_xy_mode);
    if (objects.app_metronome) lv_obj_invalidate(objects.app_metronome);
    if (objects.app_ai_agent) lv_obj_invalidate(objects.app_ai_agent);
    if (objects.app_clock) lv_obj_invalidate(objects.app_clock);
    if (objects.app_fun) lv_obj_invalidate(objects.app_fun);
    if (objects.ftp) lv_obj_invalidate(objects.ftp);
}
static const char *theme_names[] = { "hammyorange", "starrynight" };
uint32_t theme_colors[2][17] = {
    { 0xfffff8e7, 0xfffcfff5, 0xfffdeec9, 0xfff4a261, 0xffe9c46a, 0xff4a403a, 0xff8c8179, 0xff2a9d8f, 0xffe76f51, 0xffd3c8bc, 0xff90e0ef, 0xffffb7b2, 0xffb8b8d1, 0xfff4a261, 0xffa8d5ba, 0xff68d46a, 0xff9e9e9e },
    { 0xff0a0c14, 0xff374049, 0xff1a2030, 0xff217091, 0xff9a4c71, 0xfff0f4ff, 0xffa0a8c2, 0xff9fc1b6, 0xfff87171, 0xff444a60, 0xff948e51, 0xffba5eb7, 0xff7209b7, 0xff4576e8, 0xff429e84, 0xffa26825, 0xff616161 },
};

//
//
//

void create_screens() {
    // Load external fonts
    {
        ui_font_icon_70 = lv_binfont_create("/sys/src/ui_font_icon_70.bin");
        if (ui_font_icon_70) {
            fonts[0].font_ptr = ui_font_icon_70;
        } else {
            LV_LOG_ERROR("font create failed: ui_font_icon_70");
        }
    }
    {
        ui_font_chinese_30 = lv_binfont_create("/sys/src/ui_font_chinese_30.bin");
        if (ui_font_chinese_30) {
            fonts[1].font_ptr = ui_font_chinese_30;
        } else {
            LV_LOG_ERROR("font create failed: ui_font_chinese_30");
        }
    }
    {
        ui_font_chinese_40 = lv_binfont_create("/sys/src/ui_font_chinese_40.bin");
        if (ui_font_chinese_40) {
            fonts[2].font_ptr = ui_font_chinese_40;
        } else {
            LV_LOG_ERROR("font create failed: ui_font_chinese_40");
        }
    }
    {
        ui_font_clock_150 = lv_binfont_create("/sys/src/ui_font_clock_150.bin");
        if (ui_font_clock_150) {
            fonts[3].font_ptr = ui_font_clock_150;
        } else {
            LV_LOG_ERROR("font create failed: ui_font_clock_150");
        }
    }
    {
        ui_font_clock_150_a = lv_binfont_create("/sys/src/ui_font_clock_150_a.bin");
        if (ui_font_clock_150_a) {
            fonts[4].font_ptr = ui_font_clock_150_a;
        } else {
            LV_LOG_ERROR("font create failed: ui_font_clock_150_a");
        }
    }
    
    eez_flow_init_fonts(fonts, sizeof(fonts) / sizeof(ext_font_desc_t));
    
    eez_flow_init_themes(theme_names, sizeof(theme_names) / sizeof(const char *), change_color_theme, &theme_colors[0][0], sizeof(theme_colors[0]) / sizeof(uint32_t));

// Set default LVGL theme
    lv_display_t *dispp = lv_display_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), false, LV_FONT_DEFAULT);
    lv_display_set_theme(dispp, theme);
    
    // Initialize screens
    eez_flow_init_screen_names(screen_names, sizeof(screen_names) / sizeof(const char *));
    eez_flow_init_object_names(object_names, sizeof(object_names) / sizeof(const char *));
    
    eez_flow_set_create_screen_func(create_screen);
    eez_flow_set_delete_screen_func(delete_screen);
    
    // Create screens
    create_screen_boot();
    create_screen_onboard_step();
    create_screen_launcher();
    create_screen_setting();
    create_screen_about();
    create_screen_app_zen_mode();
    create_screen_app_ear_train();
    create_screen_app_chord_memory();
    create_screen_app_circle_of_fifths();
    create_screen_app_tiny_piano();
    create_screen_app_drum_pad();
    create_screen_app_midi_player();
    create_screen_app_xy_mode();
    create_screen_app_metronome();
    create_screen_app_ai_agent();
    create_screen_app_clock();
    create_screen_app_fun();
    create_screen_ftp();
}