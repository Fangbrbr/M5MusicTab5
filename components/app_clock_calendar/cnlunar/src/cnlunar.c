/**
 * @file cnlunar.c
 * @brief cnlunar (https://github.com/OPN48/cnlunar, MIT) 的 C/ESP-IDF 移植 —— 农历/节气/干支/节日与公共入口
 */
#include "cnlunar_internal.h"

/* ============================== 农历转换 ============================== */

static void get_month_leap_days(ctx_t *c, int32_t lunar_year, int32_t lunar_month)
{
    /* 农历月数据表实际只有 199 项(1901-2099)；Python 负下标回绕，此处取模保持一致 */
    int32_t idx = pmod32(lunar_year - CNLUNAR_START_YEAR, 199);
    uint32_t tmp = CNL_LUNAR_MONTH_DATA[idx];
    c->month_days = (tmp & (1u << (lunar_month - 1))) ? 30 : 29;
    c->leap_month = (int32_t)((tmp >> 13) & 0xf);
    c->leap_days = 0;
    if (c->leap_month)
        c->leap_days = (tmp & (1u << 12)) ? 30 : 29;
}

static void compute_lunar_date(ctx_t *c)
{
    cnlunar_t *o = c->o;
    int32_t year = o->year;
    uint8_t code = CNL_LUNAR_NEW_YEAR[year - CNLUNAR_START_YEAR];
    int64_t span = c->days - days_from_civil(year, (code >> 5) & 0x3, code & 0x1f);
    o->span_days = (int32_t)span;

    int32_t ly = year, lm = 1, ld = 1;
    bool is_leap = false;

    if (span >= 0) {
        get_month_leap_days(c, ly, lm);
        int32_t mdays = c->month_days;
        while (span >= mdays) {
            span -= mdays;
            if (lm == c->leap_month) {
                mdays = c->leap_days;
                if (span < mdays) {
                    is_leap = true;
                    break;
                }
                span -= mdays;
            }
            lm++;
            get_month_leap_days(c, ly, lm);
            mdays = c->month_days;
        }
        ld += (int32_t)span;
    } else {
        lm = 12;
        ly -= 1;
        get_month_leap_days(c, ly, lm);
        int32_t mdays = c->month_days;
        while ((-span) > mdays) {
            span += mdays;
            lm--;
            if (lm == c->leap_month) {
                mdays = c->leap_days;
                if ((-span) <= mdays) {
                    is_leap = true;
                    break;
                }
                span += mdays;
            }
            get_month_leap_days(c, ly, lm);
            mdays = c->month_days;
        }
        ld += (int32_t)(mdays + span);
    }

    o->lunar_year = ly;
    o->lunar_month = lm;
    o->lunar_day = ld;
    o->is_lunar_leap_month = is_leap;

    /* 中文表示 */
    char *p = o->lunar_year_cn;
    int32_t ytmp = ly;
    char digits[5];
    snprintf(digits, sizeof(digits), "%d", (int)ytmp);
    for (char *s = digits; *s; s++) {
        const char *cn = CNL_UPPER_NUM[*s - '0'];
        memcpy(p, cn, 3);
        p += 3;
    }
    *p = '\0';

    int32_t mdays = is_leap ? c->leap_days : c->month_days;
    o->lunar_month_long = mdays >= 30;
    snprintf(o->lunar_month_cn, sizeof(o->lunar_month_cn), "%s%s%s",
             is_leap ? "闰" : "", CNL_LUNAR_MONTH_NAMES[(lm - 1) % 12],
             o->lunar_month_long ? "大" : "小");
    o->lunar_day_cn = CNL_LUNAR_DAY_NAMES[(ld - 1) % 30];

    /* 月相（依赖 lunar_month_long，须在月名之后） */
    if (ld - (o->lunar_month_long ? 1 : 0) == 15) o->phase_of_moon = "望";
    else if (ld == 1)                            o->phase_of_moon = "朔";
    else if (ld >= 7 && ld <= 8)               o->phase_of_moon = "上弦";
    else if (ld >= 22 && ld <= 23)             o->phase_of_moon = "下弦";
    else                                       o->phase_of_moon = "";
}

/* ============================== 节气 ============================== */

static void year_solar_term_days(int32_t year, uint8_t out[24])
{
    uint64_t data = CNL_SOLAR_TERMS_DATA[year - CNLUNAR_START_YEAR];
    for (int j = 0; j < 24; j++)
        out[j] = (uint8_t)(CNL_ENC_VECTOR[j] + ((data >> (2 * j)) & 3u));
}

static void compute_solar_terms(ctx_t *c)
{
    cnlunar_t *o = c->o;
    year_solar_term_days(o->year, c->terms);

    /* nextSolarNum：小于等于今天的节气个数 % 24 */
    int32_t num = 0;
    for (int j = 0; j < 24; j++) {
        int32_t tm = j / 2 + 1;
        if (tm < o->month || (tm == o->month && c->terms[j] <= o->day)) num++;
    }
    o->next_solar_num = num % 24;

    /* 今日节气 */
    o->today_solar_term = "无";
    for (int j = 0; j < 24; j++) {
        if (j / 2 + 1 == o->month && c->terms[j] == o->day) {
            o->today_solar_term = CNL_SOLAR_TERMS_NAME[j];
            break;
        }
    }

    /* 下一节气（跨年处理） */
    int32_t term_year = o->year;
    const uint8_t *list = c->terms;
    uint8_t next_terms[24];
    if (o->month == 12 && o->day >= c->terms[23]) {
        term_year = o->year + 1;
        if (term_year <= CNLUNAR_END_YEAR) {
            year_solar_term_days(term_year, next_terms);
            list = next_terms;
        }
    }
    o->next_solar_term = CNL_SOLAR_TERMS_NAME[o->next_solar_num];
    o->next_solar_term_month = o->next_solar_num / 2 + 1;
    o->next_solar_term_day = list[o->next_solar_num];
    o->next_solar_term_year = term_year;
}

/* ============================== 干支/生肖/杂项 ============================== */

static void compose_ganzhi(char out[8], int32_t idx60)
{
    const char *s = CNL_STEMS[idx60 % 10];
    const char *b = CNL_BRANCHES[idx60 % 12];
    memcpy(out, s, 3);
    memcpy(out + 3, b, 3);
    out[6] = '\0';
}

static void compute_ganzhi(ctx_t *c)
{
    cnlunar_t *o = c->o;

    /* 立春修正因子 _x */
    c->x_lichun = 0;
    if (c->cfg.year_ganzhi == CNLUNAR_YEAR_GANZHI_LICHUN) {
        bool before_lichun = o->next_solar_num < 3;
        bool before_lunar_year = o->span_days < 0;
        if (before_lunar_year) {
            if (!before_lichun) c->x_lichun = -1;
        } else {
            if (before_lichun) c->x_lichun = 1;
        }
    }

    int32_t yi = pmod32(pmod32(o->lunar_year - 4, 60) - c->x_lichun, 60);
    compose_ganzhi(o->year_ganzhi, yi);

    int32_t next_num = o->next_solar_num;
    if (next_num == 0 && o->month == 12) next_num = 24;
    int32_t apart_num = (next_num + 1) / 2;
    compose_ganzhi(o->month_ganzhi, pmod32((int64_t)(o->year - 2019) * 12 + apart_num, 60));

    int64_t apart = c->days - days_from_civil(2019, 1, 29);
    int32_t base = 2; /* 丙寅 */
    if (c->twohour_num == 12) base += 1; /* 23点后算次日 */
    o->day_ganzhi_num = pmod32(apart + base, 60);
    compose_ganzhi(o->day_ganzhi, o->day_ganzhi_num);

    /* 时辰干支 */
    int32_t begin = pmod32((int64_t)o->day_ganzhi_num * 12, 60);
    for (int k = 0; k < 13; k++)
        compose_ganzhi(o->hour_ganzhi_list[k], (begin + k) % 60);
    memcpy(o->hour_ganzhi, o->hour_ganzhi_list[c->twohour_num % 12], 8);

    o->year_stem_num = yi % 10;
    o->year_branch_num = yi % 12;
    o->month_branch_num = pmod32((int64_t)(o->year - 2019) * 12 + apart_num, 60) % 12;
    o->day_stem_num = o->day_ganzhi_num % 10;
    o->day_branch_num = o->day_ganzhi_num % 12;

    /* 季节：仲季孟 + 春夏秋冬 */
    int32_t season_type = o->month_branch_num % 3;
    o->lunar_season_num = pmod32(o->month_branch_num - 2, 12) / 3;
    static const char *const MT[3] = {"仲", "季", "孟"};
    static const char *const SS[4] = {"春", "夏", "秋", "冬"};
    snprintf(o->season_name, sizeof(o->season_name), "%s%s", MT[season_type], SS[o->lunar_season_num]);

    /* 生肖与冲煞 */
    o->zodiac = CNL_ZODIAC[pmod32(pmod32(o->lunar_year - 4, 12) - c->x_lichun, 12)];
    int32_t zn = o->day_branch_num;
    int32_t clash = (zn + 6) % 12;
    o->zodiac_mark6 = CNL_ZODIAC[(25 - zn) % 12];
    o->zodiac_mark3[0] = CNL_ZODIAC[(zn + 4) % 12];
    o->zodiac_mark3[1] = CNL_ZODIAC[(zn + 8) % 12];
    o->zodiac_win = CNL_ZODIAC[zn];
    o->zodiac_lose = CNL_ZODIAC[clash];
    snprintf(o->zodiac_clash, sizeof(o->zodiac_clash), "%s日冲%s", o->zodiac_win, o->zodiac_lose);
}

static void compute_misc(ctx_t *c)
{
    cnlunar_t *o = c->o;

    o->weekday_cn = CNL_WEEKDAY[py_weekday(c->days)];

    /* 星座 */
    int32_t cnt = 0;
    for (int i = 0; i < 12; i++) {
        if (CNL_STAR_ZODIAC_DATE[i][0] < o->month ||
            (CNL_STAR_ZODIAC_DATE[i][0] == o->month && CNL_STAR_ZODIAC_DATE[i][1] <= o->day))
            cnt++;
    }
    o->star_zodiac = CNL_STAR_ZODIAC_NAME[cnt % 12];

    /* 星次 */
    o->east_zodiac = CNL_EAST_ZODIAC[pmod32(o->next_solar_num - 1, 24) / 2];

    /* 二十八宿 */
    o->star28 = CNL_STARS28[pmod32(c->days - days_from_civil(2019, 1, 17), 28)];

    /* 建除十二神与黄黑道 */
    if (c->cfg.god_type == CNLUNAR_GOD_TYPE_LUNAR_MONTH)
        c->men = (o->lunar_month - 1 + 2) % 12;
    else
        c->men = o->month_branch_num;
    static const char *const OFFICER[12] = {"建", "除", "满", "平", "定", "执",
                                            "破", "危", "成", "收", "开", "闭"};
    o->day_officer = OFFICER[pmod32(o->day_branch_num - c->men, 12)];

    static const int32_t GOD_START[12] = {8, 10, 0, 2, 4, 6, 8, 10, 0, 2, 4, 6};
    int32_t god_num = pmod32(o->day_branch_num - GOD_START[c->men], 12);
    o->day_god = CNL_12_DAY_GODS[god_num];
    o->is_yellow_day = god_num == 0 || god_num == 1 || god_num == 4 ||
                       god_num == 5 || god_num == 7 || god_num == 10;

    /* 纳音/五行/胎神/彭祖 */
    o->nayin = CNL_NAYIN[o->day_ganzhi_num / 2];
    o->stem_element = CNL_STEM_ELEMENTS[o->day_stem_num];
    o->branch_element = CNL_BRANCH_ELEMENTS[o->day_branch_num];
    o->fetal_god = CNL_FETAL_GOD[o->day_ganzhi_num];
    o->peng_taboo_stem = CNL_PENG_TABOO[o->day_stem_num];
    o->peng_taboo_branch = CNL_PENG_TABOO[o->day_branch_num + 10];

    /* 九宫飞星 */
    static const int32_t START_NUM[9] = {7, 3, 5, 6, 8, 1, 2, 4, 9};
    int64_t apart = c->days - days_from_civil(2019, 1, 17);
    for (int i = 0; i < 9; i++)
        o->nine_fly_star[i] = (char)('0' + pmod32((int64_t)START_NUM[i] - 1 - apart, 9) + 1);
    o->nine_fly_star[9] = '\0';

    /* 吉神方位 */
    static const char *const PREFIX[5] = {"喜神", "财神", "福神", "阳贵", "阴贵"};
    const char *tabs[5] = {CNL_LUCKY_GOD_DIR, CNL_WEALTH_GOD_DIR, CNL_MASCOT_GOD_DIR,
                           CNL_SUN_NOBLE_DIR, CNL_MOON_NOBLE_DIR};
    for (int i = 0; i < 5; i++) {
        const char *tri = CH(tabs[i], o->day_stem_num);
        int idx = -1;
        for (int t = 0; t < 8; t++) {
            if (memcmp(CH(CNL_8TRIGRAMS, t), tri, 3) == 0) { idx = t; break; }
        }
        snprintf(o->lucky_direction[i], sizeof(o->lucky_direction[i]), "%s%s",
                 PREFIX[i], idx >= 0 ? CNL_DIRECTIONS[idx] : "");
    }

    /* 时辰吉凶 */
    uint16_t today = CNL_TWOHOUR_LUCKY[o->day_ganzhi_num];
    uint16_t tomorrow = CNL_TWOHOUR_LUCKY[(o->day_ganzhi_num + 1) % 60];
    for (int i = 0; i < 12; i++)
        o->twohour_lucky[i] = !(today & (1u << (12 - 1 - i)));
    o->twohour_lucky[12] = !(tomorrow & (1u << 11));

    o->meridians = CNL_MERIDIANS[c->twohour_num % 12];
}

/* ============================== 节日 ============================== */

#define ARR_LEN(a) (int)(sizeof(a) / sizeof((a)[0]))

int cnlunar_get_legal_holidays(const cnlunar_t *l, char *buf, size_t len)
{
    buf[0] = '\0';
    size_t off = 0;
    int count = 0;
    if (strcmp(l->today_solar_term, "清明") == 0) {
        off += (size_t)snprintf(buf + off, len - off, "清明节");
        count++;
    }
    static const struct { uint8_t m, d; const char *name; } LH[3] = {
        {1, 1, "元旦节"}, {5, 1, "国际劳动节"}, {10, 1, "国庆节"}};
    for (int i = 0; i < 3; i++) {
        if (LH[i].m == l->month && LH[i].d == l->day) {
            off += (size_t)snprintf(buf + off, len - off, "%s%s", count ? "," : "", LH[i].name);
            count++;
        }
    }
    static const struct { uint8_t m, d; const char *name; } LLH[3] = {
        {1, 1, "春节"}, {5, 5, "端午节"}, {8, 15, "中秋节"}};
    for (int i = 0; i < 3; i++) {
        if (LLH[i].m == l->lunar_month && LLH[i].d == l->lunar_day) {
            off += (size_t)snprintf(buf + off, len - off, "%s%s", count ? "," : "", LLH[i].name);
            count++;
        }
    }
    (void)off;
    return count;
}

int cnlunar_get_other_holidays(const cnlunar_t *l, char *buf, size_t len)
{
    buf[0] = '\0';
    size_t off = 0;
    int count = 0;
    /* 母亲节(5月第2个星期日) / 父亲节(6月第3个星期日)，用 ISO 周数计算，与原版一致 */
    if (l->month == 5 || l->month == 6) {
        int iso_wd;
        int wn = iso_week(l->year, l->month, l->day, &iso_wd);
        int dummy;
        int t1dwn = iso_week(l->year, l->month, 1, &dummy);
        int want_week = l->month == 5 ? 2 : 3;
        if (wn - t1dwn + 1 == want_week && iso_wd == 7) {
            const char *name = l->month == 5 ? "母亲节" : "父亲节";
            off += (size_t)snprintf(buf + off, len - off, "%s", name);
            count++;
        }
    }
    for (int i = 0; i < ARR_LEN(CNL_OTHER_HOLIDAYS); i++) {
        if (CNL_OTHER_HOLIDAYS[i].m == l->month && CNL_OTHER_HOLIDAYS[i].d == l->day) {
            off += (size_t)snprintf(buf + off, len - off, "%s%s", count ? "," : "", CNL_OTHER_HOLIDAYS[i].name);
            count++;
        }
    }
    (void)off;
    return count;
}

int cnlunar_get_other_lunar_holidays(const cnlunar_t *l, char *buf, size_t len)
{
    buf[0] = '\0';
    size_t off = 0;
    int count = 0;
    for (int i = 0; i < ARR_LEN(CNL_OTHER_LUNAR_HOLIDAYS); i++) {
        if (CNL_OTHER_LUNAR_HOLIDAYS[i].m == l->lunar_month &&
            CNL_OTHER_LUNAR_HOLIDAYS[i].d == l->lunar_day) {
            off += (size_t)snprintf(buf + off, len - off, "%s%s", count ? "," : "",
                                    CNL_OTHER_LUNAR_HOLIDAYS[i].name);
            count++;
        }
    }
    (void)off;
    return count;
}

void cnlunar_join_things(const cnlunar_t *l, bool good, char *buf, size_t len)
{
    const char *const *items = good ? l->good_thing : l->bad_thing;
    int32_t n = good ? l->good_thing_num : l->bad_thing_num;
    size_t off = 0;
    buf[0] = '\0';
    for (int32_t i = 0; i < n; i++)
        off += (size_t)snprintf(buf + off, off < len ? len - off : 0, "%s%s", i ? "、" : "", items[i]);
}

/* ============================== 公共入口 ============================== */

void cnlunar_config_default(cnlunar_config_t *cfg)
{
    cfg->god_type = CNLUNAR_GOD_TYPE_8CHAR;
    cfg->year_ganzhi = CNLUNAR_YEAR_GANZHI_LUNAR_YEAR;
    cfg->yeargod_duty = true;
}

int cnlunar_compute(cnlunar_t *out, int32_t year, int32_t month, int32_t day, int32_t hour,
                    const cnlunar_config_t *cfg)
{
    if (year < CNLUNAR_START_YEAR || year > CNLUNAR_END_YEAR ||
        month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23)
        return -1;
    static const uint8_t MDAYS[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int32_t mlen = MDAYS[month - 1] + (month == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)));
    if (day > mlen) return -1;
    /* 农历月数据表仅覆盖到 2099 年，Python 版在 2100 年春节(含)之后必崩，此处明确拒绝 */
    if (year == CNLUNAR_END_YEAR) {
        uint8_t code = CNL_LUNAR_NEW_YEAR[199];
        int32_t cny_m = (code >> 5) & 0x3, cny_d = code & 0x1f;
        if (month > cny_m || (month == cny_m && day >= cny_d)) return -1;
    }

    memset(out, 0, sizeof(*out));
    ctx_t c;
    memset(&c, 0, sizeof(c));
    c.o = out;
    if (cfg) c.cfg = *cfg; else cnlunar_config_default(&c.cfg);

    out->year = year;
    out->month = month;
    out->day = day;
    out->hour = hour;
    c.twohour_num = (hour + 1) / 2;
    out->twohour_num = c.twohour_num;
    c.days = days_from_civil(year, (unsigned)month, (unsigned)day);

    sl_init(&c.good_gods, out->good_gods, CNLUNAR_MAX_GODS);
    sl_init(&c.bad_gods, out->bad_gods, CNLUNAR_MAX_GODS);
    sl_init(&c.good_things, out->good_thing, CNLUNAR_MAX_THINGS);
    sl_init(&c.bad_things, out->bad_thing, CNLUNAR_MAX_THINGS);

    compute_lunar_date(&c);
    compute_solar_terms(&c);
    compute_ganzhi(&c);
    compute_misc(&c);
    compute_angel_demon(&c);
    compute_thing_level(&c);
    cleanup_things(&c);
    return 0;
}
