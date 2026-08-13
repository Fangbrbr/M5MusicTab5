/* Auto-generated from cnlunar config.py/holidays.py. DO NOT EDIT. */
#ifndef CNLUNAR_DATA_H
#define CNLUNAR_DATA_H

#include <stdint.h>

typedef struct {
    const char *const *items;
    uint8_t n;
} cnl_strlist_t;

typedef struct {
    uint8_t m, d;
    const char *name;
} cnl_md_name_t;

extern const char *const CNL_STAR_ZODIAC_NAME[12];
extern const char *const CNL_SOLAR_TERMS_NAME[24];
extern const char *const CNL_EAST_ZODIAC[12];
extern const char *const CNL_STEMS[10];
extern const char *const CNL_STEM_ELEMENTS[10];
extern const char *const CNL_BRANCHES[12];
extern const char *const CNL_BRANCH_ELEMENTS[12];
extern const char *const CNL_NAYIN[30];
extern const char *const CNL_STARS28[28];
extern const char *const CNL_PENG_TABOO[22];
extern const char *const CNL_ZODIAC[12];
extern const char *const CNL_12_DAY_GODS[12];
extern const char *const CNL_DIRECTIONS[8];
extern const char *const CNL_FETAL_GOD[60];
extern const char *const CNL_MERIDIANS[12];
extern const char *const CNL_LUNAR_MONTH_NAMES[12];
extern const char *const CNL_LUNAR_DAY_NAMES[30];
extern const char *const CNL_UPPER_NUM[10];
extern const char *const CNL_WEEKDAY[7];
extern const char *const CNL_THINGS_SORT[38];
extern const char *const CNL_BUJIANG[12];
extern const char CNL_12_OFFICERS[];
extern const char CNL_8TRIGRAMS[];
extern const char CNL_LUCKY_GOD_DIR[];
extern const char CNL_WEALTH_GOD_DIR[];
extern const char CNL_MASCOT_GOD_DIR[];
extern const char CNL_SUN_NOBLE_DIR[];
extern const char CNL_MOON_NOBLE_DIR[];
extern const uint8_t CNL_STAR_ZODIAC_DATE[12][2];
extern const uint8_t CNL_ENC_VECTOR[24];
extern const uint64_t CNL_SOLAR_TERMS_DATA[200];
extern const uint32_t CNL_LUNAR_MONTH_DATA[200];
extern const uint8_t CNL_LUNAR_NEW_YEAR[200];
extern const uint16_t CNL_TWOHOUR_LUCKY[60];
extern const cnl_strlist_t CNL_OFFICER_GOOD[12];
extern const cnl_strlist_t CNL_OFFICER_BAD[12];
#define CNL_DAY8CHAR_THING_NUM 16
extern const char *const CNL_DAY8CHAR_KEY[];
extern const cnl_strlist_t CNL_DAY8CHAR_GOOD[];
extern const cnl_strlist_t CNL_DAY8CHAR_BAD[];
extern const cnl_md_name_t CNL_LEGAL_HOLIDAYS[3];
extern const cnl_md_name_t CNL_LEGAL_LUNAR_HOLIDAYS[3];
extern const cnl_md_name_t CNL_OTHER_HOLIDAYS[75];
extern const cnl_md_name_t CNL_OTHER_LUNAR_HOLIDAYS[85];

#endif /* CNLUNAR_DATA_H */
