/**
 * @file cnlunar.h
 * @brief 农历/黄历算法库 —— cnlunar (Python) 的 ESP-IDF C 移植版
 *
 * 算法与数据忠实移植自 https://github.com/OPN48/cnlunar (MIT License)
 * 支持公历 1901-01-01 ~ 2100-02-08（农历月数据表实际覆盖 1901-2099 年，
 * 原版在 2100 年春节及之后会抛出 IndexError，本库返回 -1）。
 *
 * 特点：零动态内存、零浮点运算、零依赖（仅需 libc），适合 ESP32 全系列。
 */
#ifndef CNLUNAR_H
#define CNLUNAR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CNLUNAR_START_YEAR   1901
#define CNLUNAR_END_YEAR     2100
#define CNLUNAR_MAX_THINGS   96   /**< 宜/忌事项列表最大条目 */
#define CNLUNAR_MAX_GODS     80   /**< 吉神/凶神列表最大条目 */

/* 宜忌"诸事不宜"优化等级：改下面一个数字即可
 * 0 = 与 cnlunar Python 原版完全一致
 * 1 = 空的宜列表用通用宜回填（默认）
 * 2 = 同时放宽等第过滤，进一步减少"诸事不宜" */
#define CNLUNAR_YIJI_OPTIMIZE_LEVEL 1

#if CNLUNAR_YIJI_OPTIMIZE_LEVEL >= 1
/* 通用宜列表：等级 1/2 用来回填空宜列表。可自由增删。 */
#define CNLUNAR_COMMON_YI "祭祀", "祈福", "沐浴", "扫舍宇", "入学", "裁制", "会亲友", "出行"
#endif

/** 神煞计算模式，对应原版 Lunar(godType=...) */
typedef enum {
    CNLUNAR_GOD_TYPE_8CHAR = 0,   /**< 默认：用八字月柱算神煞（辨方书配图） */
    CNLUNAR_GOD_TYPE_LUNAR_MONTH, /**< 'cnlunar'：用农历月份算神煞（辨方书文字） */
} cnlunar_god_type_t;

/** 年干支模式，对应原版 Lunar(year8Char=...) */
typedef enum {
    CNLUNAR_YEAR_GANZHI_LUNAR_YEAR = 0, /**< 默认 'year'：按农历年 */
    CNLUNAR_YEAR_GANZHI_LICHUN,         /**< 'beginningOfSpring'：严格按立春切换 */
} cnlunar_year_ganzhi_t;

typedef struct {
    cnlunar_god_type_t god_type;       /**< 默认 CNLUNAR_GOD_TYPE_8CHAR */
    cnlunar_year_ganzhi_t year_ganzhi; /**< 默认 CNLUNAR_YEAR_GANZHI_LUNAR_YEAR */
    bool yeargod_duty;                 /**< 默认 true（'duty'）：含岁德/岁破等年神 */
} cnlunar_config_t;

/** 计算结果。所有 const char* 均指向静态存储，无需释放。 */
typedef struct {
    /* ---- 输入回显 ---- */
    int32_t year, month, day, hour;

    /* ---- 农历 ---- */
    int32_t lunar_year, lunar_month, lunar_day;
    bool is_lunar_leap_month;        /**< 是否闰月 */
    bool lunar_month_long;           /**< true=大月(30天) false=小月(29天) */
    int32_t span_days;               /**< 距当年春节天数（负值表示在春节前） */
    char lunar_year_cn[20];          /**< 二零二六 */
    char lunar_month_cn[16];         /**< 六月大 / 闰腊月小 */
    const char *lunar_day_cn;        /**< 初七 */

    /* ---- 月相/星期/星座/星次 ---- */
    const char *phase_of_moon;       /**< 朔/望/上弦/下弦，无则 "" */
    const char *weekday_cn;          /**< 星期一 */
    const char *star_zodiac;         /**< 西方星座，如 巨蟹座 */
    const char *east_zodiac;         /**< 十二星次，如 鹑火 */

    /* ---- 节气 ---- */
    const char *today_solar_term;    /**< 今日节气名，非节气日则为 "无" */
    const char *next_solar_term;     /**< 下一个节气名 */
    int32_t next_solar_term_month;   /**< 下一节气日期（月/日/年） */
    int32_t next_solar_term_day;
    int32_t next_solar_term_year;
    int32_t next_solar_num;          /**< 下一节气序号 0-23（0=小寒） */

    /* ---- 干支（八字） ---- */
    char year_ganzhi[8];             /**< 丙午 */
    char month_ganzhi[8];            /**< 乙未 */
    char day_ganzhi[8];              /**< 乙未（23点后算次日，与原版一致） */
    char hour_ganzhi[8];             /**< 时柱 */
    char hour_ganzhi_list[13][8];    /**< 当日13个时辰干支 */
    int32_t twohour_num;             /**< (hour+1)/2 */
    int32_t year_stem_num;           /**< 年天干序号 0-9 */
    int32_t year_branch_num;         /**< 年地支序号 0-11 */
    int32_t month_branch_num;
    int32_t day_stem_num;
    int32_t day_branch_num;
    int32_t day_ganzhi_num;          /**< 日柱在60甲子中的序号 0-59 */

    /* ---- 生肖/冲煞 ---- */
    const char *zodiac;              /**< 年生肖，如 马 */
    char zodiac_clash[16];           /**< 羊日冲牛 */
    const char *zodiac_mark6;        /**< 六合生肖 */
    const char *zodiac_mark3[2];     /**< 三合生肖 */
    const char *zodiac_win;          /**< 日生肖 */
    const char *zodiac_lose;         /**< 被冲生肖 */

    /* ---- 季节 ---- */
    char season_name[8];             /**< 孟春/仲夏/季秋... */
    int32_t lunar_season_num;        /**< 0春 1夏 2秋 3冬 */

    /* ---- 二十八宿/建除十二神/黄黑道 ---- */
    const char *star28;              /**< 张月鹿 */
    const char *day_officer;         /**< 建/除/满/平/定/执/破/危/成/收/开/闭 */
    const char *day_god;             /**< 青龙/明堂/天刑/... */
    bool is_yellow_day;              /**< true=黄道日 false=黑道日 */

    /* ---- 神煞与宜忌（核心输出） ---- */
    const char *good_gods[CNLUNAR_MAX_GODS];
    int32_t good_gods_num;
    const char *bad_gods[CNLUNAR_MAX_GODS];
    int32_t bad_gods_num;
    const char *good_thing[CNLUNAR_MAX_THINGS];  /**< 宜：已按《辨方书》规则清洗排序 */
    int32_t good_thing_num;
    const char *bad_thing[CNLUNAR_MAX_THINGS];   /**< 忌 */
    int32_t bad_thing_num;
    int32_t today_level;             /**< 宜忌等第 -1(无) 0(上) 1(上次) 2(中) 3(中次) 4(下) 5(下下) */
    const char *today_level_name;    /**< 等第说明文字 */
    const char *thing_level_name;    /**< 从宜不从忌/从宜亦从忌/从忌不从宜/诸事皆忌 */
    bool is_de;                      /**< 是否遇德（岁德/月德/天德及合） */

    /* ---- 杂项 ---- */
    const char *meridians;           /**< 子午流注经络：胆/肝/肺... */
    const char *fetal_god;           /**< 胎神占方 */
    const char *nayin;               /**< 日纳音，如 砂中金 */
    const char *stem_element;        /**< 日天干五行 */
    const char *branch_element;      /**< 日地支五行 */
    char nine_fly_star[12];          /**< 九宫飞星，9位数字 */
    char lucky_direction[5][16];     /**< 喜神/财神/福神/阳贵/阴贵 方位 */
    const char *peng_taboo_stem;     /**< 彭祖百忌（干），如 甲不开仓 财物耗散 */
    const char *peng_taboo_branch;   /**< 彭祖百忌（支） */
    bool twohour_lucky[13];          /**< 当日13时辰吉凶，true=吉 */
} cnlunar_t;

/** 填充默认配置（与 Python 版默认参数一致） */
void cnlunar_config_default(cnlunar_config_t *cfg);

/**
 * 计算某日全部历法信息。
 * @param out   输出结构体（调用者提供，可用静态或栈内存）
 * @param year  公历年，1901-2100
 * @param month 公历月 1-12
 * @param day   公历日 1-31
 * @param hour  小时 0-23（影响时柱与23点后日柱进位；不需要可传12）
 * @param cfg   配置，传 NULL 使用默认
 * @return 0 成功；-1 日期超出支持范围
 */
int cnlunar_compute(cnlunar_t *out, int32_t year, int32_t month, int32_t day, int32_t hour,
                    const cnlunar_config_t *cfg);

/**
 * 节日查询（需在 cnlunar_compute 之后调用）。
 * 将命中的节日名以 ',' 连接写入 buf，返回写入的节日个数，无节日则 buf 为空串。
 */
int cnlunar_get_legal_holidays(const cnlunar_t *l, char *buf, size_t len);        /**< 法定节假日 */
int cnlunar_get_other_holidays(const cnlunar_t *l, char *buf, size_t len);        /**< 其他公历节日（含母亲节/父亲节） */
int cnlunar_get_other_lunar_holidays(const cnlunar_t *l, char *buf, size_t len);  /**< 农历神诞节日 */

/** 工具：把 good_thing / bad_thing 列表拼接成 "、" 分隔的字符串，便于直接喂给 LVGL label */
void cnlunar_join_things(const cnlunar_t *l, bool good, char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* CNLUNAR_H */
