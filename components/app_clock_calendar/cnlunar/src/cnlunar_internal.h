/* 内部共享头：仅被本组件的 .c 文件包含 */
#ifndef CNLUNAR_INTERNAL_H
#define CNLUNAR_INTERNAL_H

#include "cnlunar.h"
#include "cnlunar_data.h"
#include <string.h>
#include <stdio.h>


/* ============================== 基础工具 ============================== */

/* Howard Hinnant days_from_civil：公历日期 -> 自 1970-01-01 的天数 */
static inline int64_t days_from_civil(int64_t y, unsigned m, unsigned d)
{
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

/* Python 语义的正取模 */
static inline int32_t pmod32(int64_t a, int32_t b)
{
    int64_t r = a % b;
    return (int32_t)(r < 0 ? r + b : r);
}

/* Python date.weekday()：周一=0 ... 周日=6 */
static inline int py_weekday(int64_t z)
{
    return (int)pmod32(z + 3, 7);
}

/* ISO 8601 周数（isocalendar()[1]），iso_wday 返回 1=周一..7=周日 */
static inline int iso_week(int32_t y, int32_t m, int32_t d, int *iso_wday)
{
    int64_t z = days_from_civil(y, (unsigned)m, (unsigned)d);
    int wd = (int)pmod32(z + 3, 7) + 1;
    *iso_wday = wd;
    int64_t thu = z + 4 - wd;                 /* 本周周四 */
    /* 周四所在年即 ISO 年；用 civil 反推周四的年份：年份至多差 1，直接试探 */
    int32_t iy = y;
    int64_t jan4 = days_from_civil(iy, 1, 4);
    if (thu < jan4 - (pmod32(jan4 + 3, 7))) {
        iy--;
    } else {
        int64_t jan4_next = days_from_civil(iy + 1, 1, 4);
        int64_t w1_next = jan4_next - pmod32(jan4_next + 3, 7);
        if (thu >= w1_next) iy++;
    }
    jan4 = days_from_civil(iy, 1, 4);
    int64_t week1_mon = jan4 - pmod32(jan4 + 3, 7);
    return (int)((thu - week1_mon) / 7) + 1;
}

/* 指向 UTF-8 CJK 字符串中第 i 个字符（每字 3 字节） */
#define CH(s, i) ((s) + 3 * (i))

/* 单 CJK 字符（3字节，非NUL结尾）是否在字符串中 */
static inline bool ch_in(const char *hay, const char *ch_ptr)
{
    char tmp[4];
    memcpy(tmp, ch_ptr, 3);
    tmp[3] = '\0';
    return strstr(hay, tmp) != NULL;
}

/* 单 CJK 字符是否出现在 hay 的前 n_chars 个字符内（Python 切片语义） */
static inline bool ch_in_n(const char *hay, int n_chars, const char *ch_ptr)
{
    for (int i = 0; i < n_chars; i++)
        if (memcmp(hay + 3 * i, ch_ptr, 3) == 0) return true;
    return false;
}

/* 2 字干支（6字节，非NUL结尾）是否等于/在字符串中 */
static inline bool gz_in(const char *hay, const char *gz_ptr)
{
    char tmp[7];
    memcpy(tmp, gz_ptr, 6);
    tmp[6] = '\0';
    return strstr(hay, tmp) != NULL;
}

static inline bool gz_eq(const char *a, const char *b)  /* 两个 6 字节干支比较 */
{
    return memcmp(a, b, 6) == 0;
}

static inline bool int_in(int32_t v, const int32_t *arr, int n)
{
    for (int i = 0; i < n; i++)
        if (arr[i] == v) return true;
    return false;
}

/* ============================== 字符串列表（去重集合） ============================== */

typedef struct {
    const char **items;
    int32_t n;
    int32_t cap;
} slist_t;

static inline void sl_init(slist_t *l, const char **buf, int32_t cap)
{
    l->items = buf;
    l->n = 0;
    l->cap = cap;
}

static inline bool sl_contains(const slist_t *l, const char *s)
{
    for (int32_t i = 0; i < l->n; i++)
        if (strcmp(l->items[i], s) == 0) return true;
    return false;
}

static inline void sl_add(slist_t *l, const char *s)
{
    if (l->n >= l->cap || sl_contains(l, s)) return;
    l->items[l->n++] = s;
}

static inline void sl_addn(slist_t *l, const char *const *arr, int n)
{
    for (int i = 0; i < n; i++) sl_add(l, arr[i]);
}

static inline void sl_remove(slist_t *l, const char *s)
{
    for (int32_t i = 0; i < l->n; i++) {
        if (strcmp(l->items[i], s) == 0) {
            memmove(&l->items[i], &l->items[i + 1], (size_t)(l->n - i - 1) * sizeof(char *));
            l->n--;
            return;
        }
    }
}

static inline void sl_removen(slist_t *l, const char *const *arr, int n)
{
    for (int i = 0; i < n; i++) sl_remove(l, arr[i]);
}

/* 用另一张表整体替换 */
static inline void sl_set1(slist_t *l, const char *s)
{
    l->n = 0;
    sl_add(l, s);
}

/* sortCollation：按 thingsSort 表排序（稳定插入排序），不在表中的排最后 */
static inline int thing_sort_key(const char *s)
{
    for (int i = 0; i < 38; i++)
        if (strcmp(CNL_THINGS_SORT[i], s) == 0) return i;
    return 39;
}

static inline void sl_sort_things(slist_t *l)
{
    for (int32_t i = 1; i < l->n; i++) {
        const char *key = l->items[i];
        int k = thing_sort_key(key);
        int32_t j = i - 1;
        while (j >= 0 && thing_sort_key(l->items[j]) > k) {
            l->items[j + 1] = l->items[j];
            j--;
        }
        l->items[j + 1] = key;
    }
}

/* ============================== 内部上下文 ============================== */

typedef struct {
    cnlunar_t *o;
    cnlunar_config_t cfg;
    int64_t days;              /* 当天距 1970 天数 */
    int32_t twohour_num;
    /* 农历中间量 */
    int32_t month_days, leap_month, leap_days;
    int32_t x_lichun;          /* 立春修正 _x */
    /* 节气 */
    uint8_t terms[24];         /* 当年24节气日 */
    /* 神煞 */
    slist_t good_gods, bad_gods, good_things, bad_things;
    int32_t men;               /* 月支序（建除用神煞月序） */
    int32_t today_level;
    int32_t thing_level;
    bool is_de;
} ctx_t;


#define N_(a) (int)(sizeof(a) / sizeof((a)[0]))

/* 四绝四离等需要的 (月,日) 对 */
typedef struct { int32_t m, d; } md_t;

static inline bool md_in(md_t v, const md_t *arr, int n)
{
    for (int i = 0; i < n; i++)
        if (arr[i].m == v.m && arr[i].d == v.d) return true;
    return false;
}


/* 跨翻译单元函数 */
void compute_angel_demon(ctx_t *c);
void compute_thing_level(ctx_t *c);
void cleanup_things(ctx_t *c);

#endif /* CNLUNAR_INTERNAL_H */
