/* 宿主机对拍工具：输出与 Python 版 ref_dump.py 完全一致的逐日文本 */
#include "cnlunar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cmp_str(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void print_sorted(const char *const *items, int32_t n)
{
    const char *tmp[CNLUNAR_MAX_THINGS];
    memcpy(tmp, items, (size_t)n * sizeof(char *));
    qsort(tmp, (size_t)n, sizeof(char *), cmp_str);
    for (int32_t i = 0; i < n; i++)
        printf("%s%s", i ? "," : "", tmp[i]);
}

int main(int argc, char **argv)
{
    int y0 = 1901, y1 = 2100, h = 12;
    if (argc > 2) { y0 = atoi(argv[1]); y1 = atoi(argv[2]); }
    if (argc > 3) h = atoi(argv[3]);
    cnlunar_config_t cfg;
    cnlunar_config_default(&cfg);
    if (argc > 4) cfg.god_type = atoi(argv[4]) ? CNLUNAR_GOD_TYPE_LUNAR_MONTH : CNLUNAR_GOD_TYPE_8CHAR;
    if (argc > 5) cfg.year_ganzhi = atoi(argv[5]) ? CNLUNAR_YEAR_GANZHI_LICHUN : CNLUNAR_YEAR_GANZHI_LUNAR_YEAR;
    if (argc > 6) cfg.yeargod_duty = atoi(argv[6]) != 0;

    static cnlunar_t L;
    char legal[256], other[256], other_lunar[256];

    for (int y = y0; y <= y1; y++) {
        for (int m = 1; m <= 12; m++) {
            for (int d = 1; d <= 31; d++) {
                if (cnlunar_compute(&L, y, m, d, h, &cfg) != 0) continue;

                printf("%d-%02d-%02d|", y, m, d);
                printf("%d,%d,%d,%d,%d,%d|", L.lunar_year, L.lunar_month, L.lunar_day,
                       L.is_lunar_leap_month, L.lunar_month_long, L.span_days);
                printf("%s,%s,%s|", L.lunar_year_cn, L.lunar_month_cn, L.lunar_day_cn);
                printf("%s|%s|%s|%s|", L.phase_of_moon, L.weekday_cn, L.star_zodiac, L.east_zodiac);
                printf("%s|", L.today_solar_term);
                printf("%s,%d,%d,%d,%d|", L.next_solar_term, L.next_solar_term_month,
                       L.next_solar_term_day, L.next_solar_term_year, L.next_solar_num);
                printf("%s,%s,%s,%s|", L.year_ganzhi, L.month_ganzhi, L.day_ganzhi, L.hour_ganzhi);
                for (int i = 0; i < 13; i++) printf("%s%s", i ? "," : "", L.hour_ganzhi_list[i]);
                printf("|%d|", L.twohour_num);
                printf("%s,%s,%s,%s,%s|", L.zodiac, L.zodiac_clash, L.zodiac_mark6,
                       L.zodiac_mark3[0], L.zodiac_mark3[1]);
                printf("%s,%d|", L.season_name, L.lunar_season_num);
                printf("%s,%s,%s,%d|", L.star28, L.day_officer, L.day_god, L.is_yellow_day);
                printf("%s,%s,%s,%s|", L.nayin, L.stem_element, L.branch_element, L.fetal_god);
                printf("%s|", L.nine_fly_star);
                printf("%s,%s,%s,%s,%s|", L.lucky_direction[0], L.lucky_direction[1],
                       L.lucky_direction[2], L.lucky_direction[3], L.lucky_direction[4]);
                printf("%s|", L.meridians);
                for (int i = 0; i < 13; i++) putchar(L.twohour_lucky[i] ? '1' : '0');
                printf("|%d,%s,%d|", L.today_level, L.thing_level_name, L.is_de);
                print_sorted(L.good_gods, L.good_gods_num); printf("|");
                print_sorted(L.bad_gods, L.bad_gods_num);   printf("|");
                print_sorted(L.good_thing, L.good_thing_num); printf("|");
                print_sorted(L.bad_thing, L.bad_thing_num);   printf("|");
                cnlunar_get_legal_holidays(&L, legal, sizeof(legal));
                cnlunar_get_other_holidays(&L, other, sizeof(other));
                cnlunar_get_other_lunar_holidays(&L, other_lunar, sizeof(other_lunar));
                printf("%s|%s|%s|", legal, other, other_lunar);
                printf("%s,%s|", L.peng_taboo_stem, L.peng_taboo_branch);
                printf("%s\n", L.today_level_name);
            }
        }
    }
    return 0;
}
