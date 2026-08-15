/**
 * @file cnlunar_level.c
 * @brief cnlunar (https://github.com/OPN48/cnlunar, MIT) 的 C/ESP-IDF 移植 —— 宜忌等第与《协纪辨方书》清洗规则
 */
#include "cnlunar_internal.h"

/* ============================== 宜忌等第（getTodayThingLevel） ============================== */

typedef struct {
    const char *branches;      /* 月支匹配串 */
    const char *gods[3];       /* 相关神煞名 */
    int8_t god_n;
    int8_t level;
} lvl_item_t;

typedef struct {
    const char *name;
    const lvl_item_t *items;
    int8_t n;
} lvl_group_t;

static const lvl_item_t LVL_PING[] = {
    {"亥", {"相日", "时德", "六合"}, 3, 0}, {"巳", {"相日", "六合", "月刑"}, 3, 1},
    {"申", {"相日", "月害"}, 2, 2}, {"寅", {"相日", "月害", "月刑"}, 3, 3},
    {"卯午酉", {"天吏"}, 1, 3}, {"辰戌丑未", {"月煞"}, 1, 4}, {"子", {"天吏", "月刑"}, 2, 4}};
static const lvl_item_t LVL_SHOU[] = {
    {"寅申", {"长生", "六合", "劫煞"}, 3, 0}, {"巳亥", {"长生", "劫煞"}, 2, 2},
    {"辰未", {"月害"}, 1, 2}, {"子午酉", {"大时"}, 1, 3}, {"丑戌", {"月刑"}, 1, 3},
    {"卯", {"大时"}, 1, 4}};
static const lvl_item_t LVL_BI[] = {
    {"子午卯酉", {"王日"}, 1, 3}, {"辰戌丑未", {"官日", "天吏"}, 2, 3}, {"寅申巳亥", {"月煞"}, 1, 4}};
static const lvl_item_t LVL_JIESHA[] = {
    {"寅申", {"长生", "六合"}, 2, 0}, {"辰戌丑未", {"除日", "相日"}, 2, 1},
    {"巳亥", {"长生", "月害"}, 2, 2}, {"子午卯酉", {"执日"}, 1, 3}};
static const lvl_item_t LVL_ZAISHA[] = {
    {"寅申巳亥", {"开日"}, 1, 1}, {"辰戌丑未", {"满日", "民日"}, 2, 2},
    {"子午", {"月破"}, 1, 4}, {"卯酉", {"月破", "月厌"}, 2, 5}};
static const lvl_item_t LVL_YUESHA[] = {
    {"卯酉", {"六合", "危日"}, 2, 1}, {"子午", {"月害", "危日"}, 2, 3}};
static const lvl_item_t LVL_YUEXING[] = {
    {"巳", {"平日", "六合", "相日"}, 3, 1}, {"寅", {"相日", "月害", "平日"}, 3, 3},
    {"辰酉亥", {"建日"}, 1, 3}, {"子", {"平日", "天吏"}, 2, 4},
    {"卯", {"收日", "大时", "天破"}, 3, 4}, {"未申", {"月破"}, 1, 4},
    {"午", {"月建", "月厌", "德大会"}, 3, 4}};
static const lvl_item_t LVL_YUEHAI[] = {
    {"卯酉", {"守日", "除日"}, 2, 2}, {"丑未", {"执日", "大时"}, 2, 2},
    {"巳亥", {"长生", "劫煞"}, 2, 2}, {"申", {"相日", "平日"}, 2, 2},
    {"子午", {"月煞"}, 1, 3}, {"辰戌", {"官日", "闭日", "天吏"}, 3, 3},
    {"寅", {"相日", "平日", "月刑"}, 3, 3}};
static const lvl_item_t LVL_YUEYAN[] = {
    {"寅申", {"成日"}, 1, 2}, {"丑未", {"开日"}, 1, 2}, {"辰戌", {"定日"}, 1, 3},
    {"巳亥", {"满日"}, 1, 3}, {"子", {"月建", "德大会"}, 2, 4},
    {"午", {"月建", "月刑", "德大会"}, 3, 4}, {"卯酉", {"月破", "灾煞"}, 2, 5}};
static const lvl_item_t LVL_DASHI[] = {
    {"寅申巳亥", {"除日", "官日"}, 2, 0}, {"辰戌", {"执日", "六合"}, 2, 0},
    {"丑未", {"执日", "月害"}, 2, 2}, {"子午酉", {"收日"}, 1, 3}, {"卯", {"收日", "月刑"}, 2, 4}};
static const lvl_item_t LVL_TIANLI[] = {
    {"寅申巳亥", {"危日"}, 1, 2}, {"辰戌丑未", {"闭日"}, 1, 3},
    {"卯午酉", {"平日"}, 1, 3}, {"子", {"平日", "月刑"}, 2, 4}};

static const lvl_group_t LVL_GROUPS[] = {
    {"平日", LVL_PING, N_(LVL_PING)},   {"收日", LVL_SHOU, N_(LVL_SHOU)},
    {"闭日", LVL_BI, N_(LVL_BI)},       {"劫煞", LVL_JIESHA, N_(LVL_JIESHA)},
    {"灾煞", LVL_ZAISHA, N_(LVL_ZAISHA)}, {"月煞", LVL_YUESHA, N_(LVL_YUESHA)},
    {"月刑", LVL_YUEXING, N_(LVL_YUEXING)}, {"月害", LVL_YUEHAI, N_(LVL_YUEHAI)},
    {"月厌", LVL_YUEYAN, N_(LVL_YUEYAN)}, {"大时", LVL_DASHI, N_(LVL_DASHI)},
    {"天吏", LVL_TIANLI, N_(LVL_TIANLI)}};

static const char *const LEVEL_NAME[7] = {
    "上：吉足胜凶，从宜不从忌。",
    "上次：吉足抵凶，遇德从宜不从忌，不遇从宜亦从忌。",
    "中：吉不抵凶，遇德从宜不从忌，不遇从忌不从宜。",
    "中次：凶胜于吉，遇德从宜亦从忌，不遇从忌不从宜。",
    "下:凶又逢凶，遇德从忌不从宜，不遇诸事皆忌。",
    "下下：凶叠大凶，遇德亦诸事皆忌。（卯酉月，灾煞遇月破、月厌，月厌遇灾煞、月破）",
    "无"};

static const char *const THING_LEVEL_NAME[4] = {"从宜不从忌", "从宜亦从忌", "从忌不从宜", "诸事皆忌"};

static bool god_name_in(const ctx_t *c, const char *name, const char *officer_day)
{
    if (strcmp(name, officer_day) == 0) return true;
    for (int32_t i = 0; i < c->good_gods.n; i++)
        if (strcmp(c->good_gods.items[i], name) == 0) return true;
    for (int32_t i = 0; i < c->bad_gods.n; i++)
        if (strcmp(c->bad_gods.items[i], name) == 0) return true;
    return false;
}

void compute_thing_level(ctx_t *c)
{
    cnlunar_t *o = c->o;
    char officer_day[8];
    snprintf(officer_day, sizeof(officer_day), "%s日", o->day_officer);

    int32_t l = -1;
    for (int g = 0; g < (int)(sizeof(LVL_GROUPS) / sizeof(LVL_GROUPS[0])); g++) {
        const lvl_group_t *grp = &LVL_GROUPS[g];
        if (!god_name_in(c, grp->name, officer_day)) continue;
        for (int i = 0; i < grp->n; i++) {
            const lvl_item_t *it = &grp->items[i];
            if (!ch_in(it->branches, o->month_ganzhi + 3)) continue;
            for (int k = 0; k < it->god_n; k++) {
                if (god_name_in(c, it->gods[k], officer_day) && it->level > l) {
                    l = it->level;
                    break;
                }
            }
        }
    }

    /* 是否遇德 */
    static const char *const DE_GODS[6] = {"岁德", "岁德合", "月德", "月德合", "天德", "天德合"};
    c->is_de = false;
    for (int32_t i = 0; i < c->good_gods.n; i++) {
        for (int j = 0; j < 6; j++) {
            if (strcmp(c->good_gods.items[i], DE_GODS[j]) == 0) { c->is_de = true; break; }
        }
        if (c->is_de) break;
    }

    int32_t tl;
    if (l == 5) {
        tl = 3;
    } else if (l == 4) {
#if CNLUNAR_YIJI_OPTIMIZE_LEVEL >= 2
        /* 等级 2：下等日不再直接判定为"诸事皆忌"，降级为"从忌不从宜" */
        tl = 2;
#else
        tl = c->is_de ? 2 : 3;
#endif
    } else if (l == 3) {
#if CNLUNAR_YIJI_OPTIMIZE_LEVEL >= 2
        /* 等级 2：中次等日统一按"从宜亦从忌"处理 */
        tl = 1;
#else
        tl = c->is_de ? 1 : 2;
#endif
    } else if (l == 2) {
        tl = c->is_de ? 0 : 2;
    } else if (l == 1) {
        tl = c->is_de ? 0 : 1;
    } else if (l == 0) {
        tl = 0;
    } else {
        tl = 1;
    }

    c->today_level = l;
    c->thing_level = tl;
    o->today_level = l;
    o->today_level_name = l < 0 ? LEVEL_NAME[6] : LEVEL_NAME[l];
    o->thing_level_name = THING_LEVEL_NAME[tl];
    o->is_de = c->is_de;
}

/* ============================== 宜忌清洗（《协纪辨方书》规则） ============================== */

/* 从 a 中删除同时存在于 b 的项 */
static void sl_remove_intersection(slist_t *a, const slist_t *b)
{
    for (int32_t i = 0; i < a->n;) {
        bool found = false;
        for (int32_t j = 0; j < b->n; j++)
            if (strcmp(a->items[i], b->items[j]) == 0) { found = true; break; }
        if (found) {
            memmove(&a->items[i], &a->items[i + 1], (size_t)(a->n - i - 1) * sizeof(char *));
            a->n--;
        } else {
            i++;
        }
    }
}

void cleanup_things(ctx_t *c)
{
    cnlunar_t *o = c->o;
    slist_t *gt = &c->good_things, *bt = &c->bad_things;
    const char *d = o->day_ganzhi;

#if CNLUNAR_YIJI_OPTIMIZE_LEVEL >= 1
    static const char *const COMMON_YI[] = { CNLUNAR_COMMON_YI };
#endif

    if (c->thing_level == 3) {          /* 诸事不宜 */
#if CNLUNAR_YIJI_OPTIMIZE_LEVEL >= 1
        /* 等级 1/2 显示优化：把"诸事不宜"处理成"通用宜 + 诸事皆宜"，
         * 避免"宜：通用宜"与"忌：诸事不宜"自相矛盾。 */
        for (int i = 0; i < N_(COMMON_YI); i++) sl_add(gt, COMMON_YI[i]);
        sl_set1(bt, "诸事皆宜");
#else
        sl_set1(gt, "诸事不宜");
        sl_set1(bt, "诸事不宜");
#endif
    } else if (c->thing_level == 2) {   /* 从忌不从宜 */
        sl_remove_intersection(gt, bt);
    } else if (c->thing_level == 1) {   /* 从宜亦从忌：交集两边都删 */
        slist_t tmp = *gt;
        const char *copy[CNLUNAR_MAX_THINGS];
        tmp.items = copy;
        memcpy(copy, gt->items, (size_t)gt->n * sizeof(char *));
        for (int32_t i = 0; i < tmp.n; i++) {
            if (sl_contains(bt, tmp.items[i])) {
                sl_remove(gt, tmp.items[i]);
                sl_remove(bt, tmp.items[i]);
            }
        }
    } else {                            /* 从宜不从忌 */
        sl_remove_intersection(bt, gt);
    }

    /* 遇德犹忌之事（岁德/岁德合无，月德/月德合/天德/天德合为畋猎、取鱼） */
    static const char *const DE_BAD[] = {"畋猎", "取鱼"};
    slist_t de_bad;
    const char *de_bad_buf[4];
    sl_init(&de_bad, de_bad_buf, 4);
    if (c->is_de) {
        static const char *const DE_WITH_BAD[4] = {"月德", "月德合", "天德", "天德合"};
        for (int32_t i = 0; i < c->good_gods.n; i++)
            for (int j = 0; j < 4; j++)
                if (strcmp(c->good_gods.items[i], DE_WITH_BAD[j]) == 0)
                    sl_addn(&de_bad, DE_BAD, 2);
    }

    if (c->thing_level != 3) {
        if (sl_contains(gt, "宣政事") && sl_contains(gt, "布政事")) sl_remove(gt, "布政事");
        if (sl_contains(gt, "营建宫室") && sl_contains(gt, "修宫室")) sl_remove(gt, "修宫室");

        /* 凡德合、赦愿、月恩、四相、时德等日不注忌 */
        static const char *const MAXPOWER[7] = {"月德合", "天德合", "天赦", "天愿", "月恩", "四相", "时德"};
        bool is_maxpower = false;
        for (int32_t i = 0; i < c->good_gods.n; i++) {
            for (int j = 0; j < 7; j++)
                if (strcmp(c->good_gods.items[i], MAXPOWER[j]) == 0) is_maxpower = true;
            if (c->cfg.yeargod_duty && strcmp(c->good_gods.items[i], "岁德合") == 0) is_maxpower = true;
        }
        if (is_maxpower && c->thing_level != 2) {
            static const char *const RM_FULL[9] = {"进人口", "安床", "经络", "酝酿", "开市", "立券交易",
                                                   "纳财", "开仓库", "出货财"};
            sl_removen(bt, RM_FULL, 9);
            sl_addn(bt, de_bad.items, de_bad.n);
        }

        if (sl_contains(&c->bad_gods, "天狗") || ch_in(d, "寅")) {
            sl_add(bt, "祭祀");
            sl_remove(gt, "祭祀");
            static const char *const RM1[2] = {"求福", "祈嗣"};
            sl_removen(gt, RM1, 2);
        }
        if (ch_in(d, "卯")) {
            sl_add(bt, "穿井");
            sl_remove(gt, "穿井");
            sl_remove(gt, "开渠");
        }
        if (ch_in(d, "壬")) {
            sl_add(bt, "开渠");
            sl_remove(gt, "开渠");
            sl_remove(gt, "穿井");
        }
        if (ch_in(d, "巳")) {
            sl_add(bt, "出行");
            sl_remove(gt, "出行");
            static const char *const RM2[2] = {"出师", "遣使"};
            sl_removen(gt, RM2, 2);
        }
        if (ch_in(d, "酉")) {
            sl_add(bt, "宴会");
            sl_remove(gt, "宴会");
            static const char *const RM3[2] = {"庆赐", "赏贺"};
            sl_removen(gt, RM3, 2);
        }
        if (ch_in(d, "丁")) {
            sl_add(bt, "剃头");
            sl_remove(gt, "剃头");
            sl_remove(gt, "整容");
        }
        if (c->today_level == 0 && c->thing_level == 0)
            sl_addn(bt, de_bad.items, de_bad.n);
        if (c->today_level == 1) {
            sl_addn(bt, de_bad.items, de_bad.n);
            if (!sl_contains(bt, "祈福")) sl_remove(bt, "求嗣");
            if (!sl_contains(bt, "结婚姻") && !c->is_de) {
                static const char *const RM4[4] = {"冠带", "纳采问名", "嫁娶", "进人口"};
                sl_removen(bt, RM4, 4);
            }
            if (!sl_contains(bt, "嫁娶") && !c->is_de && !sl_contains(&c->good_gods, "不将")) {
                static const char *const RM5[6] = {"冠带", "纳采问名", "结婚姻", "进人口", "搬移", "安床"};
                sl_removen(bt, RM5, 6);
            }
        }
        if (ch_in(d, "亥")) sl_add(bt, "嫁娶");

        if (c->today_level == 1 && !c->is_de) {
            if (!sl_contains(bt, "搬移")) sl_remove(bt, "安床");
            if (!sl_contains(bt, "安床")) sl_remove(bt, "搬移");
            if (!sl_contains(bt, "解除")) {
                static const char *const RM6[3] = {"整容", "剃头", "整手足甲"};
                sl_removen(bt, RM6, 3);
            }
            if (!sl_contains(bt, "修造") || !sl_contains(bt, "竖柱上梁")) {
                static const char *const RM7[14] = {"修宫室", "缮城郭", "整手足甲", "筑堤防", "修仓库", "鼓铸",
                                                    "苫盖", "修置产室", "开渠穿井", "安碓硙", "补垣塞穴",
                                                    "修饰垣墙", "平治道涂", "破屋坏垣"};
                sl_removen(bt, RM7, 14);
            }
        }
        if (c->today_level == 1) {
            if (!sl_contains(bt, "开市")) {
                static const char *const RM8[4] = {"立券交易", "纳财", "开仓库", "出货财"};
                sl_removen(bt, RM8, 4);
            }
            if (!sl_contains(bt, "纳财")) {
                static const char *const RM9[2] = {"立券交易", "开市"};
                sl_removen(bt, RM9, 2);
            }
            if (!sl_contains(bt, "立券交易")) {
                static const char *const RM10[4] = {"纳财", "开市", "开仓库", "出货财"};
                sl_removen(bt, RM10, 4);
            }
        }
        if (c->today_level == 1) {
            if (!sl_contains(bt, "牧养")) sl_remove(bt, "纳畜");
            if (!sl_contains(bt, "纳畜")) sl_remove(bt, "牧养");
            if (sl_contains(gt, "安葬")) sl_remove(bt, "启攒");
            if (sl_contains(gt, "启攒")) sl_remove(bt, "安葬");
        }
        if (sl_contains(bt, "诏命公卿") || sl_contains(bt, "招贤")) {
            static const char *const RM11[2] = {"施恩", "举正直"};
            sl_removen(gt, RM11, 2);
        }
        if (sl_contains(bt, "施恩") || sl_contains(bt, "举正直")) {
            static const char *const RM12[2] = {"诏命公卿", "招贤"};
            sl_removen(gt, RM12, 2);
        }
        if (sl_contains(gt, "宣政事") && sl_contains(&c->bad_gods, "往亡")) {
            sl_remove(gt, "宣政事");
            sl_add(gt, "布政事");
        }
        if (sl_contains(&c->bad_gods, "月厌")) {
            static const char *const RM13[5] = {"颁诏", "施恩", "招贤", "举正直", "宣政事"};
            sl_removen(gt, RM13, 5);
            sl_add(gt, "布政事");
            sl_add(bt, "补垣");
            if (sl_contains(&c->bad_gods, "土府") || sl_contains(&c->bad_gods, "土符") ||
                sl_contains(&c->bad_gods, "地囊"))
                sl_remove(gt, "塞穴");
        }
        if (strcmp(o->day_officer, "开") == 0) {
            static const char *const RM14[3] = {"破土", "安葬", "启攒"};
            sl_removen(gt, RM14, 3);
        }
        if (sl_contains(&c->bad_gods, "四忌") || sl_contains(&c->bad_gods, "四穷")) {
            sl_add(bt, "安葬");
            static const char *const RM15[2] = {"破土", "启攒"};
            sl_removen(gt, RM15, 2);
        }
        if (sl_contains(&c->good_gods, "鸣吠") || sl_contains(&c->good_gods, "鸣吠对")) {
            static const char *const RM16[2] = {"破土", "启攒"};
            sl_removen(gt, RM16, 2);
        }
        {
            static const char *const SPECIAL[12] = {"空", "甲戌", "空", "丙申", "空", "甲子",
                                                    "戊申", "庚辰", "辛卯", "甲子", "空", "甲子"};
            const char *sp = SPECIAL[o->lunar_month - 1];
            if (strcmp(sp, "空") != 0 && gz_in(d, sp)) sl_set1(bt, "诸事不忌");
        }
        {
            bool has_de3 = sl_contains(&c->good_gods, "岁德合") || sl_contains(&c->good_gods, "月德合") ||
                           sl_contains(&c->good_gods, "天德合");
            bool has_sheyuan = sl_contains(&c->good_gods, "天赦") || sl_contains(&c->good_gods, "天愿");
            if (has_de3 && has_sheyuan) sl_set1(bt, "诸事不忌");
        }
    }

    /* 通用宜回填（等级 1/2）：避免经过等第清洗后"宜"列表为空。
     * 注意：thing_level==3 的"诸事不宜"已在前面单独处理，这里不再重复回填。 */
#if CNLUNAR_YIJI_OPTIMIZE_LEVEL >= 1
    if (gt->n == 0 && c->thing_level != 3) {
        for (int i = 0; i < N_(COMMON_YI); i++) sl_add(gt, COMMON_YI[i]);
    }
#endif

    /* 书中未明注忌不注宜 */
    {
        const char *rm[CNLUNAR_MAX_THINGS];
        int32_t rn = 0;
        for (int32_t i = 0; i < bt->n; i++)
            if (sl_contains(gt, bt->items[i])) rm[rn++] = bt->items[i];
        if (rn == 1 && strstr(rm[0], "诸事") != NULL) {
            /* 保留 */
        } else {
            for (int32_t i = 0; i < rn; i++) sl_remove(gt, rm[i]);
        }
    }

    if (bt->n == 0) sl_add(bt, "诸事不忌");
#if CNLUNAR_YIJI_OPTIMIZE_LEVEL >= 1
    /* 若通用宜被冲突清洗后又空了，兜底再填一次，避免用户看到"诸事不宜"。
     * 等级 2 因等第映射更宽松，走到这里的概率更低。 */
    if (gt->n == 0) {
        for (int i = 0; i < N_(COMMON_YI); i++) sl_add(gt, COMMON_YI[i]);
    }
#else
    if (gt->n == 0) sl_add(gt, "诸事不宜");
#endif

    sl_sort_things(bt);
    sl_sort_things(gt);

    o->good_gods_num = c->good_gods.n;
    o->bad_gods_num = c->bad_gods.n;
    o->good_thing_num = gt->n;
    o->bad_thing_num = bt->n;
}
