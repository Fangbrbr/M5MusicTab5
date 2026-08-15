#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_BOOT = 1,
    SCREEN_ID_ONBOARD_STEP = 2,
    SCREEN_ID_LAUNCHER = 3,
    SCREEN_ID_SETTING = 4,
    SCREEN_ID_ABOUT = 5,
    SCREEN_ID_APP_ZEN_MODE = 6,
    SCREEN_ID_APP_EAR_TRAIN = 7,
    SCREEN_ID_APP_CHORD_MEMORY = 8,
    SCREEN_ID_APP_CIRCLE_OF_FIFTHS = 9,
    SCREEN_ID_APP_TINY_PIANO = 10,
    SCREEN_ID_APP_DRUM_PAD = 11,
    SCREEN_ID_APP_MIDI_PLAYER = 12,
    SCREEN_ID_APP_XY_MODE = 13,
    SCREEN_ID_APP_METRONOME = 14,
    SCREEN_ID_APP_AI_AGENT = 15,
    SCREEN_ID_APP_CLOCK = 16,
    SCREEN_ID_APP_FUN = 17,
    SCREEN_ID_FTP = 18,
    _SCREEN_ID_LAST = 18
};

typedef struct _objects_t {
    lv_obj_t *boot;
    lv_obj_t *onboard_step;
    lv_obj_t *launcher;
    lv_obj_t *setting;
    lv_obj_t *about;
    lv_obj_t *app_zen_mode;
    lv_obj_t *app_ear_train;
    lv_obj_t *app_chord_memory;
    lv_obj_t *app_circle_of_fifths;
    lv_obj_t *app_tiny_piano;
    lv_obj_t *app_drum_pad;
    lv_obj_t *app_midi_player;
    lv_obj_t *app_xy_mode;
    lv_obj_t *app_metronome;
    lv_obj_t *app_ai_agent;
    lv_obj_t *app_clock;
    lv_obj_t *app_fun;
    lv_obj_t *ftp;
    lv_obj_t *boot_hammy;
    lv_obj_t *boot_label_name_en;
    lv_obj_t *boot_percent;
    lv_obj_t *boot_label_loading;
    lv_obj_t *boot_version;
    lv_obj_t *boot_label_name;
    lv_obj_t *obj0;
    lv_obj_t *ob_step_prev;
    lv_obj_t *ob_step_next;
    lv_obj_t *step01_welcome;
    lv_obj_t *ob_hammy;
    lv_obj_t *ob_str1;
    lv_obj_t *ob_str2;
    lv_obj_t *ob_str3;
    lv_obj_t *step02_datetime;
    lv_obj_t *obj1;
    lv_obj_t *obj2;
    lv_obj_t *label_loading_2;
    lv_obj_t *obj3;
    lv_obj_t *ob_set_hour;
    lv_obj_t *obj4;
    lv_obj_t *ob_set_minute;
    lv_obj_t *obj5;
    lv_obj_t *obj6;
    lv_obj_t *ob_set_second;
    lv_obj_t *ob_set_time_result;
    lv_obj_t *ob_set_time_save;
    lv_obj_t *obj7;
    lv_obj_t *ob_set_year;
    lv_obj_t *obj8;
    lv_obj_t *obj9;
    lv_obj_t *ob_set_month;
    lv_obj_t *obj10;
    lv_obj_t *obj11;
    lv_obj_t *ob_set_day;
    lv_obj_t *obj12;
    lv_obj_t *obj13;
    lv_obj_t *step03_bg_vol;
    lv_obj_t *obj14;
    lv_obj_t *obj15;
    lv_obj_t *label_loading_3;
    lv_obj_t *obj16;
    lv_obj_t *ob_slide_brightness;
    lv_obj_t *ob_slide_brightness_num;
    lv_obj_t *obj17;
    lv_obj_t *ob_slide_volume;
    lv_obj_t *ob_slide_volume_num;
    lv_obj_t *ob_key_try_volume;
    lv_obj_t *obj18;
    lv_obj_t *step04_feature;
    lv_obj_t *obj19;
    lv_obj_t *obj20;
    lv_obj_t *label_loading_5;
    lv_obj_t *obj21;
    lv_obj_t *ob_setting_theme;
    lv_obj_t *obj22;
    lv_obj_t *obj23;
    lv_obj_t *ob_setting_on_screen;
    lv_obj_t *obj24;
    lv_obj_t *obj25;
    lv_obj_t *ob_setting_time2idle;
    lv_obj_t *obj26;
    lv_obj_t *obj27;
    lv_obj_t *ob_setting_auto_sleep;
    lv_obj_t *step05_online;
    lv_obj_t *obj28;
    lv_obj_t *obj29;
    lv_obj_t *label_loading_4;
    lv_obj_t *obj30;
    lv_obj_t *ob_setting_wifi_switch;
    lv_obj_t *ob_set_wifi_connect_tip;
    lv_obj_t *step06_finish_reboot;
    lv_obj_t *obj31;
    lv_obj_t *obj32;
    lv_obj_t *obj33;
    lv_obj_t *obj34;
    lv_obj_t *launcher_zen;
    lv_obj_t *obj35;
    lv_obj_t *launcher_ear;
    lv_obj_t *obj36;
    lv_obj_t *launcher_fifth;
    lv_obj_t *obj37;
    lv_obj_t *launcher_chord;
    lv_obj_t *obj38;
    lv_obj_t *launcher_midi;
    lv_obj_t *obj39;
    lv_obj_t *launcher_xy;
    lv_obj_t *obj40;
    lv_obj_t *launcher_drum;
    lv_obj_t *obj41;
    lv_obj_t *launcher_piano;
    lv_obj_t *obj42;
    lv_obj_t *launcher_clock;
    lv_obj_t *obj43;
    lv_obj_t *launcher_ai;
    lv_obj_t *obj44;
    lv_obj_t *launcher_metron;
    lv_obj_t *obj45;
    lv_obj_t *launcher_fun;
    lv_obj_t *obj46;
    lv_obj_t *launcher_btn_setting;
    lv_obj_t *launcher_led_ai;
    lv_obj_t *obj47;
    lv_obj_t *obj48;
    lv_obj_t *obj49;
    lv_obj_t *obj50;
    lv_obj_t *setting_btn_home;
    lv_obj_t *setting_btn_about;
    lv_obj_t *setting_tab;
    lv_obj_t *setting_tab_basic;
    lv_obj_t *obj51;
    lv_obj_t *setting_language;
    lv_obj_t *obj52;
    lv_obj_t *obj53;
    lv_obj_t *setting_slide_brightness;
    lv_obj_t *setting_slide_brightness_num;
    lv_obj_t *obj54;
    lv_obj_t *setting_slide_volume;
    lv_obj_t *setting_slide_volume_num;
    lv_obj_t *obj55;
    lv_obj_t *setting_theme;
    lv_obj_t *obj56;
    lv_obj_t *obj57;
    lv_obj_t *setting_on_screen;
    lv_obj_t *obj58;
    lv_obj_t *obj59;
    lv_obj_t *setting_time2idle;
    lv_obj_t *obj60;
    lv_obj_t *obj61;
    lv_obj_t *setting_auto_sleep;
    lv_obj_t *obj62;
    lv_obj_t *setting_invert_display;
    lv_obj_t *setting_tab_advanced;
    lv_obj_t *obj63;
    lv_obj_t *setting_btn_wifi_detail;
    lv_obj_t *obj64;
    lv_obj_t *setting_btn_ftp;
    lv_obj_t *obj65;
    lv_obj_t *setting_sf2_source;
    lv_obj_t *obj66;
    lv_obj_t *obj67;
    lv_obj_t *setting_btn_system_reset;
    lv_obj_t *wifi_set_panel;
    lv_obj_t *wifi_set_panel_return;
    lv_obj_t *obj68;
    lv_obj_t *setting_wifi_switch;
    lv_obj_t *setting_wifi_connect_tip;
    lv_obj_t *obj69;
    lv_obj_t *setting_btn_wifi_reset;
    lv_obj_t *setting_led_ai;
    lv_obj_t *obj70;
    lv_obj_t *obj71;
    lv_obj_t *obj72;
    lv_obj_t *obj73;
    lv_obj_t *about_btn_return;
    lv_obj_t *obj74;
    lv_obj_t *obj75;
    lv_obj_t *obj76;
    lv_obj_t *obj77;
    lv_obj_t *obj78;
    lv_obj_t *obj79;
    lv_obj_t *obj80;
    lv_obj_t *obj81;
    lv_obj_t *obj82;
    lv_obj_t *obj83;
    lv_obj_t *about_system_monitor_tick;
    lv_obj_t *about_led_ai;
    lv_obj_t *obj84;
    lv_obj_t *obj85;
    lv_obj_t *obj86;
    lv_obj_t *obj87;
    lv_obj_t *zen_btn_home;
    lv_obj_t *obj88;
    lv_obj_t *zen_canvas;
    lv_obj_t *zen_ball_0;
    lv_obj_t *zen_ball_1;
    lv_obj_t *zen_ball_2;
    lv_obj_t *zen_ball_3;
    lv_obj_t *zen_ball_4;
    lv_obj_t *obj89;
    lv_obj_t *obj90;
    lv_obj_t *zen_dropdown_mode;
    lv_obj_t *obj91;
    lv_obj_t *obj92;
    lv_obj_t *zen_dropdown_key;
    lv_obj_t *obj93;
    lv_obj_t *obj94;
    lv_obj_t *zen_dropdown_speed;
    lv_obj_t *obj95;
    lv_obj_t *zen_btn_rec;
    lv_obj_t *obj96;
    lv_obj_t *zen_led_ai;
    lv_obj_t *obj97;
    lv_obj_t *obj98;
    lv_obj_t *obj99;
    lv_obj_t *obj100;
    lv_obj_t *ear_btn_home;
    lv_obj_t *ear_key_try_play;
    lv_obj_t *obj101;
    lv_obj_t *obj102;
    lv_obj_t *ear_label_try_count;
    lv_obj_t *ear_score_title;
    lv_obj_t *ear_score;
    lv_obj_t *obj103;
    lv_obj_t *ear_trainer_test;
    lv_obj_t *obj104;
    lv_obj_t *obj105;
    lv_obj_t *ear_mode;
    lv_obj_t *obj106;
    lv_obj_t *obj107;
    lv_obj_t *ear_difficult;
    lv_obj_t *obj108;
    lv_obj_t *obj109;
    lv_obj_t *ear_best_score;
    lv_obj_t *ear_key_major;
    lv_obj_t *ear_key_minor2;
    lv_obj_t *ear_key_minor3;
    lv_obj_t *ear_key_interval;
    lv_obj_t *ear_life_panel;
    lv_obj_t *ear_life1;
    lv_obj_t *ear_life2;
    lv_obj_t *ear_life3;
    lv_obj_t *ear_led_ai;
    lv_obj_t *obj110;
    lv_obj_t *obj111;
    lv_obj_t *obj112;
    lv_obj_t *obj113;
    lv_obj_t *chord_btn_home;
    lv_obj_t *chord_key_key;
    lv_obj_t *obj114;
    lv_obj_t *obj115;
    lv_obj_t *chord_definition;
    lv_obj_t *chord_panel_type_poll;
    lv_obj_t *chord_type_maj;
    lv_obj_t *chord_name;
    lv_obj_t *chord_canvas_piano;
    lv_obj_t *chord_led_ai;
    lv_obj_t *obj116;
    lv_obj_t *obj117;
    lv_obj_t *obj118;
    lv_obj_t *fifth_btn_home;
    lv_obj_t *obj119;
    lv_obj_t *fifth_canvas_circle;
    lv_obj_t *fifth_panel_info;
    lv_obj_t *fifth_name;
    lv_obj_t *fifth_key_sig;
    lv_obj_t *obj120;
    lv_obj_t *fifth_scale;
    lv_obj_t *fifth_canvas_piano;
    lv_obj_t *obj121;
    lv_obj_t *fifth_dominant;
    lv_obj_t *obj122;
    lv_obj_t *fifth_parallel;
    lv_obj_t *obj123;
    lv_obj_t *fifth_subdominant;
    lv_obj_t *fifth_led_ai;
    lv_obj_t *obj124;
    lv_obj_t *obj125;
    lv_obj_t *obj126;
    lv_obj_t *piano_btn_home;
    lv_obj_t *piano_btn_rec;
    lv_obj_t *obj127;
    lv_obj_t *piano_btn_set;
    lv_obj_t *obj128;
    lv_obj_t *piano_panel_m;
    lv_obj_t *piano_pad0;
    lv_obj_t *piano_pad1;
    lv_obj_t *piano_pad2;
    lv_obj_t *piano_pad3;
    lv_obj_t *piano_pad4;
    lv_obj_t *piano_pad5;
    lv_obj_t *piano_pad6;
    lv_obj_t *piano_pad7;
    lv_obj_t *piano_pad8;
    lv_obj_t *piano_pad9;
    lv_obj_t *piano_pad10;
    lv_obj_t *piano_pad11;
    lv_obj_t *piano_pad12;
    lv_obj_t *piano_pad13;
    lv_obj_t *piano_pad14;
    lv_obj_t *piano_panel_v;
    lv_obj_t *obj129;
    lv_obj_t *piano_root_v;
    lv_obj_t *piano_canvas_key;
    lv_obj_t *piano_set;
    lv_obj_t *piano_set_btn_return;
    lv_obj_t *obj130;
    lv_obj_t *piano_display_type;
    lv_obj_t *obj131;
    lv_obj_t *obj132;
    lv_obj_t *piano_scale_type;
    lv_obj_t *obj133;
    lv_obj_t *obj134;
    lv_obj_t *piano_pitch;
    lv_obj_t *obj135;
    lv_obj_t *obj136;
    lv_obj_t *piano_sound_type;
    lv_obj_t *obj137;
    lv_obj_t *piano_led_ai;
    lv_obj_t *obj138;
    lv_obj_t *obj139;
    lv_obj_t *obj140;
    lv_obj_t *obj141;
    lv_obj_t *obj142;
    lv_obj_t *obj143;
    lv_obj_t *drum_btn_home;
    lv_obj_t *drum_btn_rec;
    lv_obj_t *obj144;
    lv_obj_t *drum_btn_set;
    lv_obj_t *drum_panel_v;
    lv_obj_t *drum_panel_m;
    lv_obj_t *drum_crash_m;
    lv_obj_t *drum_clap_m;
    lv_obj_t *drum_openhht_m;
    lv_obj_t *drum_closedhh_m;
    lv_obj_t *drum_ride_m;
    lv_obj_t *drum_snare_n;
    lv_obj_t *drum_kick_m;
    lv_obj_t *drum_floortom_m;
    lv_obj_t *drum_pad_set;
    lv_obj_t *drum_set_btn_return;
    lv_obj_t *obj145;
    lv_obj_t *drum_display_type;
    lv_obj_t *obj146;
    lv_obj_t *obj147;
    lv_obj_t *drum_sound_type;
    lv_obj_t *obj148;
    lv_obj_t *drum_led_ai;
    lv_obj_t *obj149;
    lv_obj_t *obj150;
    lv_obj_t *obj151;
    lv_obj_t *obj152;
    lv_obj_t *obj153;
    lv_obj_t *obj154;
    lv_obj_t *midi_btn_home;
    lv_obj_t *midi_btn_set;
    lv_obj_t *midi_panel_music_list;
    lv_obj_t *midi_list_music_file;
    lv_obj_t *midi_panel_mid_list;
    lv_obj_t *midi_list_mid_file;
    lv_obj_t *midi_file_example;
    lv_obj_t *midi_panel_hmr_list;
    lv_obj_t *midi_list_record_file;
    lv_obj_t *obj155;
    lv_obj_t *obj156;
    lv_obj_t *midi_music_name_label;
    lv_obj_t *obj157;
    lv_obj_t *midi_music_path_label;
    lv_obj_t *obj158;
    lv_obj_t *midi_music_bpm_num;
    lv_obj_t *obj159;
    lv_obj_t *midi_music_track_count;
    lv_obj_t *midi_prev;
    lv_obj_t *midi_play_stop;
    lv_obj_t *midi_play_stop_label;
    lv_obj_t *midi_next;
    lv_obj_t *obj160;
    lv_obj_t *midi_progress;
    lv_obj_t *midi_play_time_now;
    lv_obj_t *midi_play_time_total;
    lv_obj_t *midi_set;
    lv_obj_t *midi_set_btn_return;
    lv_obj_t *midi_btn_music;
    lv_obj_t *midi_btn_mid;
    lv_obj_t *midi_btn_hmr;
    lv_obj_t *midi_led_ai;
    lv_obj_t *obj161;
    lv_obj_t *obj162;
    lv_obj_t *obj163;
    lv_obj_t *obj164;
    lv_obj_t *obj165;
    lv_obj_t *obj166;
    lv_obj_t *obj167;
    lv_obj_t *obj168;
    lv_obj_t *obj169;
    lv_obj_t *obj170;
    lv_obj_t *obj171;
    lv_obj_t *obj172;
    lv_obj_t *xy_btn_home;
    lv_obj_t *xy_btn_rec;
    lv_obj_t *obj173;
    lv_obj_t *xy_btn_set;
    lv_obj_t *xy_point_1;
    lv_obj_t *xy_point_2;
    lv_obj_t *xy_point_3;
    lv_obj_t *xy_set;
    lv_obj_t *xy_set_btn_return;
    lv_obj_t *obj174;
    lv_obj_t *xy_sound;
    lv_obj_t *obj175;
    lv_obj_t *obj176;
    lv_obj_t *xy_step;
    lv_obj_t *obj177;
    lv_obj_t *xy_led_ai;
    lv_obj_t *obj178;
    lv_obj_t *obj179;
    lv_obj_t *obj180;
    lv_obj_t *obj181;
    lv_obj_t *obj182;
    lv_obj_t *obj183;
    lv_obj_t *metron_btn_home;
    lv_obj_t *metron_btn_set;
    lv_obj_t *metron_panel;
    lv_obj_t *metron_btn_minus;
    lv_obj_t *metron_label_bpm;
    lv_obj_t *obj184;
    lv_obj_t *metron_btn_plus;
    lv_obj_t *obj185;
    lv_obj_t *metron_led_heavy;
    lv_obj_t *metron_led_1;
    lv_obj_t *metron_led_2;
    lv_obj_t *metron_led_3;
    lv_obj_t *metron_led_4;
    lv_obj_t *metron_led_5;
    lv_obj_t *metron_led_6;
    lv_obj_t *metron_led_7;
    lv_obj_t *metron_led_8;
    lv_obj_t *metron_led_9;
    lv_obj_t *metron_led_10;
    lv_obj_t *metron_led_11;
    lv_obj_t *metron_led_12;
    lv_obj_t *metron_led_13;
    lv_obj_t *metron_led_14;
    lv_obj_t *metron_led_15;
    lv_obj_t *obj186;
    lv_obj_t *metron_timesig_top;
    lv_obj_t *obj187;
    lv_obj_t *metron_label_timesig;
    lv_obj_t *metron_timesig_bot;
    lv_obj_t *obj188;
    lv_obj_t *metron_btn_tempo;
    lv_obj_t *metron_slider_bpm;
    lv_obj_t *metron_btn_play_stop;
    lv_obj_t *metron_btn_play_stop_label;
    lv_obj_t *metron_set;
    lv_obj_t *metron_set_btn_return;
    lv_obj_t *obj189;
    lv_obj_t *metron_sound;
    lv_obj_t *obj190;
    lv_obj_t *metron_led_ai;
    lv_obj_t *obj191;
    lv_obj_t *obj192;
    lv_obj_t *obj193;
    lv_obj_t *obj194;
    lv_obj_t *obj195;
    lv_obj_t *ai_btn_home;
    lv_obj_t *obj196;
    lv_obj_t *obj197;
    lv_obj_t *ai_context_user0;
    lv_obj_t *ai_context_user0_text;
    lv_obj_t *ai_context_ai0;
    lv_obj_t *obj198;
    lv_obj_t *ai_context_ai0_text;
    lv_obj_t *ai_btn_set;
    lv_obj_t *ai_btn_speak;
    lv_obj_t *ai_set;
    lv_obj_t *ai_set_btn_return;
    lv_obj_t *obj199;
    lv_obj_t *ai_switch_save_text;
    lv_obj_t *obj200;
    lv_obj_t *ai_switch_wake_anywhere;
    lv_obj_t *obj201;
    lv_obj_t *ai_btn_config_reset;
    lv_obj_t *obj202;
    lv_obj_t *obj203;
    lv_obj_t *obj204;
    lv_obj_t *obj205;
    lv_obj_t *obj206;
    lv_obj_t *obj207;
    lv_obj_t *obj208;
    lv_obj_t *clock_btn_home;
    lv_obj_t *clock_btn_set;
    lv_obj_t *clock_panel_clock;
    lv_obj_t *obj209;
    lv_obj_t *clock_clock_12h_label;
    lv_obj_t *clock_clock_bigtime;
    lv_obj_t *clock_clock_date;
    lv_obj_t *clock_clock_lunar;
    lv_obj_t *clock_clock_hitokoto;
    lv_obj_t *clock_panel_weather;
    lv_obj_t *obj210;
    lv_obj_t *obj211;
    lv_obj_t *weather_today_text;
    lv_obj_t *weather_today_text_1;
    lv_obj_t *obj212;
    lv_obj_t *weather_location;
    lv_obj_t *obj213;
    lv_obj_t *weather_temp;
    lv_obj_t *weather_humi;
    lv_obj_t *obj214;
    lv_obj_t *weather_day2;
    lv_obj_t *weather_day2_1;
    lv_obj_t *obj215;
    lv_obj_t *weather_day3;
    lv_obj_t *weather_day3_1;
    lv_obj_t *weatehr_panel_news;
    lv_obj_t *clock_panel_calender;
    lv_obj_t *clock_calender;
    lv_obj_t *obj216;
    lv_obj_t *obj217;
    lv_obj_t *clock_calender_today;
    lv_obj_t *clock_calender_today_week;
    lv_obj_t *clock_calender_progress_bar;
    lv_obj_t *clock_calender_progress_text;
    lv_obj_t *clock_calender_panel_huangli;
    lv_obj_t *clock_calender_huangli_1;
    lv_obj_t *clock_calender_huangli_2;
    lv_obj_t *clock_calender_huangli_3;
    lv_obj_t *clock_calender_huangli_4;
    lv_obj_t *clock_calender_huangli_5;
    lv_obj_t *clock_calender_huangli_6;
    lv_obj_t *clock_panel_timer;
    lv_obj_t *obj218;
    lv_obj_t *clock_timer_bell;
    lv_obj_t *clock_timer_pv;
    lv_obj_t *obj219;
    lv_obj_t *clock_timer_btn_min_1min;
    lv_obj_t *clock_timer_sv;
    lv_obj_t *clock_timer_btn_add_1min;
    lv_obj_t *obj220;
    lv_obj_t *obj221;
    lv_obj_t *clock_timer_quick_1min;
    lv_obj_t *clock_timer_quick_3min;
    lv_obj_t *clock_timer_quick_5min;
    lv_obj_t *clock_timer_quick_10min;
    lv_obj_t *clock_timer_quick_20min;
    lv_obj_t *clock_timer_quick_30min;
    lv_obj_t *clock_timer_quick_40min;
    lv_obj_t *clock_timer_quick_50min;
    lv_obj_t *clock_timer_quick_60min;
    lv_obj_t *clock_timer_reset;
    lv_obj_t *clock_timer_start_pause;
    lv_obj_t *clock_set;
    lv_obj_t *clock_set_btn_return;
    lv_obj_t *obj222;
    lv_obj_t *clock_set_12_24h;
    lv_obj_t *obj223;
    lv_obj_t *obj224;
    lv_obj_t *clock_set_time_font;
    lv_obj_t *obj225;
    lv_obj_t *clock_btn_clock;
    lv_obj_t *clock_btn_calender;
    lv_obj_t *clock_btn_timer;
    lv_obj_t *clock_led_ai;
    lv_obj_t *obj226;
    lv_obj_t *obj227;
    lv_obj_t *obj228;
    lv_obj_t *obj229;
    lv_obj_t *obj230;
    lv_obj_t *obj231;
    lv_obj_t *obj232;
    lv_obj_t *fun_btn_home;
    lv_obj_t *fun_btn_set;
    lv_obj_t *fun_panel_book;
    lv_obj_t *obj233;
    lv_obj_t *obj234;
    lv_obj_t *obj235;
    lv_obj_t *obj236;
    lv_obj_t *panel_book_open;
    lv_obj_t *book_answer_text;
    lv_obj_t *obj237;
    lv_obj_t *book_answer_detail;
    lv_obj_t *panel_book_close;
    lv_obj_t *obj238;
    lv_obj_t *obj239;
    lv_obj_t *obj240;
    lv_obj_t *fun_panel_tarot;
    lv_obj_t *obj241;
    lv_obj_t *obj242;
    lv_obj_t *obj243;
    lv_obj_t *obj244;
    lv_obj_t *obj245;
    lv_obj_t *panel_tarot_open_1;
    lv_obj_t *tarot_card_1;
    lv_obj_t *obj246;
    lv_obj_t *tarot_card_reversed_1;
    lv_obj_t *tarot_card_detail_panel_1;
    lv_obj_t *tarot_card_detail_text_1;
    lv_obj_t *panel_tarot_close_1;
    lv_obj_t *obj247;
    lv_obj_t *panel_tarot_open_2;
    lv_obj_t *obj248;
    lv_obj_t *tarot_card_2;
    lv_obj_t *tarot_card_reversed_2;
    lv_obj_t *tarot_card_detail_panel_2;
    lv_obj_t *tarot_card_detail_text_2;
    lv_obj_t *panel_tarot_close_2;
    lv_obj_t *obj249;
    lv_obj_t *panel_tarot_open_3;
    lv_obj_t *obj250;
    lv_obj_t *tarot_card_3;
    lv_obj_t *tarot_card_reversed_3;
    lv_obj_t *tarot_card_detail_panel_3;
    lv_obj_t *tarot_card_detail_text_3;
    lv_obj_t *panel_tarot_close_3;
    lv_obj_t *fun_tip_label;
    lv_obj_t *fun_set;
    lv_obj_t *fun_btn_book;
    lv_obj_t *fun_btn_tarot;
    lv_obj_t *fun_led_ai;
    lv_obj_t *obj251;
    lv_obj_t *obj252;
    lv_obj_t *obj253;
    lv_obj_t *obj254;
    lv_obj_t *ftp_btn_back2setting;
    lv_obj_t *obj255;
    lv_obj_t *obj256;
    lv_obj_t *ftp_hammy;
    lv_obj_t *obj257;
    lv_obj_t *obj258;
    lv_obj_t *ftp_label_state;
    lv_obj_t *obj259;
    lv_obj_t *obj260;
    lv_obj_t *ftp_label_ip;
    lv_obj_t *obj261;
    lv_obj_t *obj262;
    lv_obj_t *ftp_label_file;
    lv_obj_t *obj263;
    lv_obj_t *ftp_bar_progress;
    lv_obj_t *obj264;
    lv_obj_t *obj265;
} objects_t;

extern objects_t objects;

void create_screen_boot();
void delete_screen_boot();
void tick_screen_boot();

void create_screen_onboard_step();
void delete_screen_onboard_step();
void tick_screen_onboard_step();

void create_screen_launcher();
void delete_screen_launcher();
void tick_screen_launcher();

void create_screen_setting();
void delete_screen_setting();
void tick_screen_setting();

void create_screen_about();
void delete_screen_about();
void tick_screen_about();

void create_screen_app_zen_mode();
void delete_screen_app_zen_mode();
void tick_screen_app_zen_mode();

void create_screen_app_ear_train();
void delete_screen_app_ear_train();
void tick_screen_app_ear_train();

void create_screen_app_chord_memory();
void delete_screen_app_chord_memory();
void tick_screen_app_chord_memory();

void create_screen_app_circle_of_fifths();
void delete_screen_app_circle_of_fifths();
void tick_screen_app_circle_of_fifths();

void create_screen_app_tiny_piano();
void delete_screen_app_tiny_piano();
void tick_screen_app_tiny_piano();

void create_screen_app_drum_pad();
void delete_screen_app_drum_pad();
void tick_screen_app_drum_pad();

void create_screen_app_midi_player();
void delete_screen_app_midi_player();
void tick_screen_app_midi_player();

void create_screen_app_xy_mode();
void delete_screen_app_xy_mode();
void tick_screen_app_xy_mode();

void create_screen_app_metronome();
void delete_screen_app_metronome();
void tick_screen_app_metronome();

void create_screen_app_ai_agent();
void delete_screen_app_ai_agent();
void tick_screen_app_ai_agent();

void create_screen_app_clock();
void delete_screen_app_clock();
void tick_screen_app_clock();

void create_screen_app_fun();
void delete_screen_app_fun();
void tick_screen_app_fun();

void create_screen_ftp();
void delete_screen_ftp();
void tick_screen_ftp();

void create_screen_by_id(enum ScreensEnum screenId);
void delete_screen_by_id(enum ScreensEnum screenId);
void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

// Color themes

enum Themes {
    THEME_ID_HAMMYORANGE,
    THEME_ID_STARRYNIGHT,
};
enum Colors {
    COLOR_ID_BG_PRIMARY,
    COLOR_ID_BG_SECONDARY,
    COLOR_ID_CARD,
    COLOR_ID_PRIMARY,
    COLOR_ID_SECONDARY,
    COLOR_ID_TEXT_PRIMARY,
    COLOR_ID_TEXT_SECONDARY,
    COLOR_ID_SUCCESS,
    COLOR_ID_ERROR,
    COLOR_ID_DISABLE,
    COLOR_ID_M1_PERCEIVE,
    COLOR_ID_M2_DEFINE,
    COLOR_ID_M3_BUILD,
    COLOR_ID_M4_PERFORM,
    COLOR_ID_M5_EXTEND,
    COLOR_ID_TOOL,
    COLOR_ID_SHADOW,
};
void change_color_theme(uint32_t themeIndex);
extern uint32_t theme_colors[2][17];

//
// Helper functions
//

lv_anim_t *get_anim();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/