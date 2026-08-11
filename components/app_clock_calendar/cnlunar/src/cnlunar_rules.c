/**
 * @file cnlunar_rules.c
 * @brief cnlunar (https://github.com/OPN48/cnlunar, MIT) 的 C/ESP-IDF 移植 —— 神煞规则（get_AngelDemon）
 */
#include "cnlunar_internal.h"

/* ============================== 神煞规则（get_AngelDemon） ============================== */

extern const char *const ARR_DE_GOOD[37];
extern const char *const ARR_DE_BAD[2];
extern const char *const ARR_TIANSHE[36];
extern const char *const ARR_SIXIANG[24];
extern const char *const ARR_TIANYUAN[40];
extern const char *const ARR_WANGRI[16];
extern const char *const ARR_SANHE[18];
extern const char *const ARR_MINRI[11];
extern const char *const ARR_TIANXI[10];
extern const char *const ARR_BAD_YUEPO[57];
extern const char *const ARR_BAD_YUESHA[57];
extern const char *const ARR_BAD_60[59];
extern const char *const ARR_BAD_YUEYAN[62];
extern const char *const ARR_BAD_TIANLI[37];
extern const char *const ARR_BAD_YUEJIAN[25];
extern const char *const ARR_BAD_YUEHAI[27];
extern const char *const ARR_BAD_WUMU[30];
extern const char *const ARR_BAD_SIFEI[55];
extern const char *const ARR_BAD_TUFU[16];
extern const char *const ARR_BAD_WANGWANG[18];
extern const char *const ARR_BAD_SISHEN[10];
extern const char *const ARR_BAD_SIQI[7];
extern const char *const ARR_BAD_CHUAN2[3];
extern const char *const ARR_BAD_XIU2[3];
extern const char *const ARR_BAD_KU4[5];
extern const char *const ARR_BAD_KU2[2];
extern const char *const ARR_BAD_KU9[6];
extern const char *const ARR_BAD_MARRY3[6];
extern const char *const ARR_BAD_ZANG3[3];

#define L(...) (const char *const[]){__VA_ARGS__}, \
               (int)(sizeof((const char *const[]){__VA_ARGS__}) / sizeof(const char *))
#define L0 NULL, 0

static void god_hit(ctx_t *c, bool angel, const char *name,
                    const char *const *good, int ng, const char *const *bad, int nb)
{
    if (!c->cfg.yeargod_duty && strstr(name, "岁")) return;
    sl_add(angel ? &c->good_gods : &c->bad_gods, name);
    sl_addn(&c->good_things, good, ng);
    sl_addn(&c->bad_things, bad, nb);
}

#define ANGEL(name, cond, ...) do { if (cond) god_hit(c, true, name, __VA_ARGS__); } while (0)
#define DEMON(name, cond, ...) do { if (cond) god_hit(c, false, name, __VA_ARGS__); } while (0)

void compute_angel_demon(ctx_t *c)
{
    cnlunar_t *o = c->o;
    const char *d = o->day_ganzhi;
    const char *d0 = d, *d1 = d + 3;
    int32_t den = o->day_branch_num, dhen = o->day_ganzhi_num;
    int32_t sn = o->lunar_season_num, yhn = o->year_stem_num, yen = o->year_branch_num;
    int32_t ldn = o->lunar_day, lmn = o->lunar_month, men = c->men;
    const char *s = o->star28;
    int32_t officer_idx = pmod32(den - men, 12);
    const char *officer = o->day_officer;

    /* 基础：建除十二神宜忌 */
    sl_addn(&c->good_things, CNL_OFFICER_GOOD[officer_idx].items, CNL_OFFICER_GOOD[officer_idx].n);
    sl_addn(&c->bad_things, CNL_OFFICER_BAD[officer_idx].items, CNL_OFFICER_BAD[officer_idx].n);

    /* 日干/日支宜忌 */
    for (int i = 0; i < CNL_DAY8CHAR_THING_NUM; i++) {
        if (ch_in(d, CNL_DAY8CHAR_KEY[i])) {
            sl_addn(&c->good_things, CNL_DAY8CHAR_GOOD[i].items, CNL_DAY8CHAR_GOOD[i].n);
            sl_addn(&c->bad_things, CNL_DAY8CHAR_BAD[i].items, CNL_DAY8CHAR_BAD[i].n);
        }
    }

    /* 节气间差类增补（保留原版行为：伐木规则中 d in ['午','申'] 恒为假） */
    bool officer_in_zws = strcmp(officer, "执") == 0 || strcmp(officer, "危") == 0 || strcmp(officer, "收") == 0;
    if (o->next_solar_num >= 4 && o->next_solar_num <= 8 && officer_in_zws)
        sl_add(&c->good_things, "取鱼");
    if (((o->next_solar_num >= 20 && o->next_solar_num <= 23) || o->next_solar_num <= 2) && officer_in_zws)
        sl_add(&c->good_things, "畋猎");
    if (((o->next_solar_num >= 21 && o->next_solar_num <= 23) || o->next_solar_num <= 2) &&
        strcmp(officer, "危") == 0)
        sl_add(&c->good_things, "伐木");
    static const int32_t LDN_JIA[] = {1, 6, 15, 19, 21, 23};
    if (int_in(ldn, LDN_JIA, N_(LDN_JIA))) sl_add(&c->bad_things, "整手足甲");
    if (ldn == 12 || ldn == 15) { sl_add(&c->bad_things, "整容"); sl_add(&c->bad_things, "剃头"); }
    if (ldn == 15 || o->phase_of_moon[0] != '\0') sl_add(&c->bad_things, "求医疗病");

    /* 明天 (m,d) 与 四立/二分二至 */
    static const uint8_t MDAYS[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int32_t tm = o->month, td = o->day + 1;
    int32_t mlen = MDAYS[tm - 1] + (tm == 2 && (o->year % 4 == 0 && (o->year % 100 != 0 || o->year % 400 == 0)));
    if (td > mlen) { td = 1; tm++; }
    md_t tmd = {tm, td};
    md_t t4l[4] = {{3, c->terms[5]}, {6, c->terms[11]}, {9, c->terms[17]}, {12, c->terms[23]}}; /* 春分夏至秋分冬至 */
    md_t t4j[4] = {{2, c->terms[2]}, {5, c->terms[8]}, {8, c->terms[14]}, {11, c->terms[20]}};  /* 立春立夏立秋立冬 */
    int32_t cnt_less = 0;
    for (int i = 0; i < 4; i++)
        if (t4j[i].m < tmd.m || (t4j[i].m == tmd.m && t4j[i].d < tmd.d)) cnt_less++;
    md_t twys = t4j[cnt_less % 4];
    /* 原版用 datetime 相减：(目标日0点 - 当日hour点).days，hour>0 时会少一天 */
    int64_t tuwang_days = days_from_civil(o->next_solar_term_year, (unsigned)twys.m, (unsigned)twys.d) -
                          c->days - (o->hour > 0 ? 1 : 0);

    /* ==================== 吉神 ==================== */
    ANGEL("岁德", ch_in(d, CH("甲庚丙壬戊甲庚丙壬戊", yhn)), L("修造"), L0);
    ANGEL("岁德合", ch_in(d, CH("己乙辛丁癸己乙辛丁癸", yhn)), L("修造"), L0);
    ANGEL("月德", memcmp(CH("壬庚丙甲壬庚丙甲壬庚丙甲", men), d0, 3) == 0,
          ARR_DE_GOOD, N_(ARR_DE_GOOD), ARR_DE_BAD, N_(ARR_DE_BAD));
    ANGEL("月德合", memcmp(CH("丁乙辛己丁乙辛己丁乙辛己", men), d0, 3) == 0,
          ARR_DE_GOOD, N_(ARR_DE_GOOD), ARR_DE_BAD, N_(ARR_DE_BAD));
    {
        /* 天德：仲月看日支，其余看日干 */
        static const char *const TIANDE_SET[12] = {"巳辰", "庚", "丁", "申未", "壬", "辛",
                                                   "亥戌", "甲", "癸", "寅丑", "丙", "乙"};
        bool is_zhong = (o->month_branch_num % 3) == 0; /* 仲季孟[0]='仲' */
        const char *v = is_zhong ? d1 : d0;
        ANGEL("天德", ch_in(TIANDE_SET[men], v),
              ARR_DE_GOOD, N_(ARR_DE_GOOD), ARR_DE_BAD, N_(ARR_DE_BAD));
    }
    ANGEL("天德合", ch_in(d, CH("空乙壬空丁丙空己戊空辛庚", men)),
          ARR_DE_GOOD, N_(ARR_DE_GOOD), ARR_DE_BAD, N_(ARR_DE_BAD));
    ANGEL("凤凰日", memcmp(s, CH("危昴胃毕", sn), 3) == 0, L("嫁娶"), L0);
    ANGEL("麒麟日", memcmp(s, CH("井尾牛壁", sn), 3) == 0, L("嫁娶"), L0);
    ANGEL("三合", (den - men) % 4 == 0, ARR_SANHE, N_(ARR_SANHE), L0);
    {
        static const char *const SIXIANG_SET[4] = {"丙丁", "戊己", "壬癸", "甲乙"};
        ANGEL("四相", ch_in(SIXIANG_SET[sn], d0), ARR_SIXIANG, N_(ARR_SIXIANG), L0);
    }
    ANGEL("五合", ch_in("寅卯", d1), L("宴会", "结婚姻", "立券交易"), L0);
    ANGEL("五富", ch_in(d, CH("巳申亥寅巳申亥寅巳申亥寅", men)),
          L("经络", "酝酿", "开市", "立券交易", "纳财", "开仓", "栽种", "牧养", "纳畜"), L0);
    ANGEL("六合", ch_in(d, CH("丑子亥戌酉申未午巳辰卯寅", men)),
          L("宴会", "结婚姻", "嫁娶", "进人口", "经络", "酝酿", "立券交易", "纳财", "纳畜", "安葬"), L0);
    ANGEL("六仪", ch_in(d, CH("午巳辰卯寅丑子亥戌酉申未", men)), L("临政"), L0);
    ANGEL("不将", gz_in(CNL_BUJIANG[men], d), L("嫁娶"), L0);
    ANGEL("时德", memcmp(CH("午辰子寅", sn), d1, 3) == 0, ARR_SIXIANG, N_(ARR_SIXIANG), L0);
    ANGEL("大葬", gz_in("壬申癸酉壬午甲申乙酉丙申丁酉壬寅丙午己酉庚申辛酉", d), L("安葬"), L0);
    ANGEL("鸣吠", gz_in("庚午壬申癸酉壬午甲申乙酉己酉丙申丁酉壬寅丙午庚寅庚申辛酉", d),
          L("破土", "安葬"), L0);
    ANGEL("小葬", gz_in("庚午壬辰甲辰乙巳甲寅丙辰庚寅", d), L("安葬"), L0);
    ANGEL("鸣吠对", gz_in("丙寅丁卯丙子辛卯甲午庚子癸卯壬子甲寅乙卯", d), L("破土", "启攒"), L0);
    ANGEL("不守塚", gz_in("庚午辛未壬申癸酉戊寅己卯壬午癸未甲申乙酉丁未甲午乙未丙申丁酉壬寅癸卯丙午戊申己酉庚申辛酉", d),
          L("破土"), L0);
    ANGEL("王日", memcmp(CH("寅巳申亥", sn), d1, 3) == 0, ARR_WANGRI, N_(ARR_WANGRI), L0);
    ANGEL("官日", memcmp(CH("卯午酉子", sn), d1, 3) == 0, L("上官", "临政"), L0);
    ANGEL("守日", memcmp(CH("酉子卯午", sn), d1, 3) == 0, L("安抚边境", "上官", "临政"), L0);
    ANGEL("相日", memcmp(CH("巳申亥寅", sn), d1, 3) == 0, L("上官", "临政"), L0);
    ANGEL("民日", memcmp(CH("午酉子卯", sn), d1, 3) == 0, ARR_MINRI, N_(ARR_MINRI), L0);
    ANGEL("临日", ch_in(d, CH("辰酉午亥申丑戌卯子巳寅未", men)), L("上册", "上表章", "上官", "临政"), L0);
    {
        static const char *const TIANGUI_SET[4] = {"甲乙", "丙丁", "庚辛", "壬癸"};
        ANGEL("天贵", ch_in(TIANGUI_SET[sn], d0), L0, L0);
    }
    ANGEL("天喜", memcmp(CH("申酉戌亥子丑寅卯辰巳午未", men), d1, 3) == 0, ARR_TIANXI, N_(ARR_TIANXI), L0);
    ANGEL("天富", ch_in(d, CH("寅卯辰巳午未申酉戌亥子丑", men)), L("安葬", "修仓库"), L0);
    ANGEL("天恩", dhen % 15 < 5 && dhen / 15 != 2, L("覃恩", "恤孤茕", "布政事", "雪冤", "庆赐", "宴会"), L0);
    ANGEL("月恩", ch_in(d, CH("甲辛丙丁庚己戊辛壬癸庚乙", men)), ARR_SIXIANG, N_(ARR_SIXIANG), L0);
    {
        static const char *const TIANHE[12] = {"甲子", "甲子", "戊寅", "戊寅", "戊寅", "甲午",
                                               "甲午", "甲午", "戊申", "戊申", "戊申", "甲子"};
        ANGEL("天赦", strcmp(TIANHE[men], d) == 0,
              ARR_TIANSHE, N_(ARR_TIANSHE), ARR_DE_BAD, N_(ARR_DE_BAD));
    }
    {
        static const char *const TIANYUAN_SET[12] = {"甲子", "癸未", "甲午", "甲戌", "乙酉", "丙子",
                                                     "丁丑", "戊午", "甲寅", "丙辰", "辛卯", "戊辰"};
        ANGEL("天愿", strcmp(TIANYUAN_SET[men], d) == 0, ARR_TIANYUAN, N_(ARR_TIANYUAN), L0);
    }
    ANGEL("天成", ch_in(d, CH("卯巳未酉亥丑卯巳未酉亥丑", men)), L0, L0);
    ANGEL("天官", ch_in(d, CH("午申戌子寅辰午申戌子寅辰", men)), L0, L0);
    ANGEL("天医", ch_in(d, CH("亥子丑寅卯辰巳午未申酉戌", men)), L("求医疗病"), L0);
    ANGEL("天马", ch_in(d, CH("寅辰午申戌子寅辰午申戌子", men)), L("出行", "搬移"), L0);
    ANGEL("驿马", ch_in(d, CH("寅亥申巳寅亥申巳寅亥申巳", men)), L("出行", "搬移"), L0);
    ANGEL("天财", ch_in(d, CH("子寅辰午申戌子寅辰午申戌", men)), L0, L0);
    ANGEL("福生", ch_in(d, CH("寅申酉卯戌辰亥巳子午丑未", men)), L("祭祀", "祈福"), L0);
    ANGEL("福厚", ch_in(d, CH("寅巳申亥", sn)), L0, L0);
    ANGEL("福德", ch_in(d, CH("寅卯辰巳午未申酉戌亥子丑", men)), L("上册", "上表章", "庆赐", "宴会", "修宫室", "缮城郭"), L0);
    ANGEL("天巫", ch_in(d, CH("寅卯辰巳午未申酉戌亥子丑", men)), L("求医疗病"), L0);
    ANGEL("地财", ch_in(d, CH("丑卯巳未酉亥丑卯巳未酉亥", men)), L0, L0);
    ANGEL("月财", ch_in(d, CH("酉亥午巳巳未酉亥午巳巳未", men)), L0, L0);
    ANGEL("月空", ch_in(d, CH("丙甲壬庚丙甲壬庚丙甲壬庚", men)), L("上表章"), L0);
    {
        static const char *const MUCANG_SET[4] = {"亥子", "寅卯", "辰丑戌未", "申酉"};
        ANGEL("母仓", ch_in(MUCANG_SET[sn], d1), L("纳财", "栽种", "牧养", "纳畜"), L0);
    }
    ANGEL("明星", ch_in(d, CH("辰午甲戌子寅辰午甲戌子寅", men)), L("赴任", "诉讼", "安葬"), L0);
    ANGEL("圣心", ch_in(d, CH("辰戌亥巳子午丑未寅申卯酉", men)), L("祭祀", "祈福"), L0);
    ANGEL("禄库", ch_in(d, CH("寅卯辰巳午未申酉戌亥子丑", men)), L("纳财"), L0);
    ANGEL("吉庆", ch_in(d, CH("未子酉寅亥辰丑午卯申巳戌", men)), L0, L0);
    ANGEL("阴德", ch_in(d, CH("丑亥酉未巳卯丑亥酉未巳卯", men)), L("恤孤茕", "雪冤"), L0);
    ANGEL("活曜", ch_in(d, CH("卯申巳戌未子酉寅亥辰丑午", men)), L0, L0);
    ANGEL("除神", ch_in("申酉", d1), L("解除", "沐浴", "整容", "剃头", "整手足甲", "求医疗病", "扫舍宇"), L0);
    ANGEL("解神", ch_in(d, CH("午午申申戌戌子子寅寅辰辰", men)),
          L("上表章", "解除", "沐浴", "整容", "剃头", "整手足甲", "求医疗病"), L0);
    ANGEL("生气", ch_in(d, CH("戌亥子丑寅卯辰巳午未申酉", men)), L0, L("伐木", "畋猎", "取鱼"));
    ANGEL("普护", ch_in(d, CH("丑卯申寅酉卯戌辰亥巳子午", men)), L("祭祀", "祈福"), L0);
    ANGEL("益后", ch_in(d, CH("巳亥子午丑未寅申卯酉辰戌", men)), L("祭祀", "祈福", "求嗣"), L0);
    ANGEL("续世", ch_in(d, CH("午子丑未寅申卯酉辰戌巳亥", men)), L("祭祀", "祈福", "求嗣"), L0);
    ANGEL("要安", ch_in(d, CH("未丑寅申卯酉辰戌巳亥午子", men)), L0, L0);
    ANGEL("天后", ch_in(d, CH("寅亥申巳寅亥申巳寅亥申巳", men)), L("求医疗病"), L0);
    ANGEL("天仓", ch_in(d, CH("辰卯寅丑子亥戌酉申未午巳", men)), L("进人口", "纳财", "纳畜"), L0);
    ANGEL("敬安", ch_in(d, CH("子午未丑申寅酉卯戌辰亥巳", men)), L0, L0);
    ANGEL("玉宇", ch_in(d, CH("申寅卯酉辰戌巳亥午子未丑", men)), L0, L0);
    ANGEL("金堂", ch_in(d, CH("酉卯辰戌巳亥午子未丑申寅", men)), L0, L0);
    ANGEL("吉期", ch_in(d, CH("丑寅卯辰巳午未申酉戌亥子", men)), L("施恩", "举正直", "出行", "上官", "临政"), L0);
    ANGEL("小时", ch_in(d, CH("子丑寅卯辰巳午未申酉戌亥", men)), L0, L0);
    ANGEL("兵福", ch_in(d, CH("子丑寅卯辰巳午未申酉戌亥", men)), L("安抚边境", "选将", "出师"), L0);
    ANGEL("兵宝", ch_in(d, CH("丑寅卯辰巳午未申酉戌亥子", men)), L("安抚边境", "选将", "出师"), L0);
    {
        static const char *const BINGJI_SET[12] = {
            "寅卯辰巳", "丑寅卯辰", "子丑寅卯", "亥子丑寅", "戌亥子丑", "酉戌亥子",
            "申酉戌亥", "未申酉戌", "午未申酉", "巳午未申", "辰巳午未", "卯辰巳午"};
        ANGEL("兵吉", ch_in(BINGJI_SET[men], d1), L("安抚边境", "选将", "出师"), L0);
    }

    /* ==================== 凶神 ==================== */
    DEMON("岁破", den == (yen + 6) % 12, L0, L("修造", "搬移", "嫁娶", "出行"));
    DEMON("天罡", ch_in(d, CH("卯戌巳子未寅酉辰亥午丑申", men)), L0, L("安葬"));
    DEMON("河魁", ch_in(d, CH("酉辰亥午丑申卯戌巳子未寅", men)), L0, L("安葬"));
    DEMON("死神", ch_in(d, CH("卯辰巳午未申酉戌亥子丑寅", men)), L0, ARR_BAD_SISHEN, N_(ARR_BAD_SISHEN));
    DEMON("死气", ch_in(d, CH("辰巳午未申酉戌亥子丑寅卯", men)), L0, ARR_BAD_SIQI, N_(ARR_BAD_SIQI));
    DEMON("伏兵", memcmp(CH("丙甲壬庚", yen % 4), d0, 3) == 0, L0, ARR_BAD_XIU2, N_(ARR_BAD_XIU2));
    DEMON("官符", ch_in(d, CH("辰巳午未申酉戌亥子丑寅卯", men)), L0, L("上表章", "上册"));
    DEMON("月建", ch_in(d, CH("子丑寅卯辰巳午未申酉戌亥", men)), L0, ARR_BAD_YUEJIAN, N_(ARR_BAD_YUEJIAN));
    DEMON("月破", ch_in(d, CH("午未申酉戌亥子丑寅卯辰巳", men)), L("破屋坏垣"), ARR_BAD_YUEPO, N_(ARR_BAD_YUEPO));
    DEMON("月煞", ch_in(d, CH("未辰丑戌未辰丑戌未辰丑戌", men)), L0, ARR_BAD_YUESHA, N_(ARR_BAD_YUESHA));
    DEMON("月害", ch_in(d, CH("未午巳辰卯寅丑子亥戌酉申", men)), L0, ARR_BAD_YUEHAI, N_(ARR_BAD_YUEHAI));
    DEMON("月刑", ch_in(d, CH("卯戌巳子辰申午丑寅酉未亥", men)), L0, ARR_BAD_60, N_(ARR_BAD_60));
    DEMON("月厌", ch_in(d, CH("子亥戌酉申未午巳辰卯寅丑", men)), L0, ARR_BAD_YUEYAN, N_(ARR_BAD_YUEYAN));
    {
        static const int32_t YUEJI_D[] = {5, 14, 23};
        DEMON("月忌", int_in(ldn, YUEJI_D, 3), L0, L("出行", "乘船渡水"));
    }
    DEMON("月虚", ch_in(d, CH("未辰丑戌未辰丑戌未辰丑戌", men)), L0, L("修仓库", "纳财", "开仓"));
    DEMON("灾煞", ch_in(d, CH("午卯子酉午卯子酉午卯子酉", men)), L0, ARR_BAD_60, N_(ARR_BAD_60));
    DEMON("劫煞", ch_in(d, CH("巳寅亥申巳寅亥申巳寅亥申", men)), L0, ARR_BAD_60, N_(ARR_BAD_60));
    DEMON("厌对", ch_in(d, CH("午巳辰卯寅丑子亥戌酉申未", men)), L0, L("嫁娶"));
    DEMON("招摇", ch_in(d, CH("午巳辰卯寅丑子亥戌酉申未", men)), L0, L("取鱼", "乘船渡水"));
    DEMON("小红砂", ch_in(d, CH("酉丑巳酉丑巳酉丑巳酉丑巳", men)), L0, L("嫁娶"));
    DEMON("往亡", ch_in(d, CH("戌丑寅巳申亥卯午酉子辰未", men)), L0, ARR_BAD_WANGWANG, N_(ARR_BAD_WANGWANG));
    DEMON("重丧", ch_in(d, CH("癸己甲乙己丙丁己庚辛己壬", men)), L0, L("嫁娶", "安葬"));
    DEMON("重复", ch_in(d, CH("癸己庚辛己壬癸戊甲乙己壬", men)), L0, L("嫁娶", "安葬"));
    {
        static const md_t YANGGONG[13] = {{1, 13}, {2, 11}, {3, 9}, {4, 7}, {5, 5}, {6, 2}, {7, 1},
                                          {7, 29}, {8, 27}, {9, 25}, {10, 23}, {11, 21}, {12, 19}};
        md_t ld = {lmn, ldn};
        DEMON("杨公忌", md_in(ld, YANGGONG, 13), L0, L("开张", "修造", "嫁娶", "立券"));
    }
    DEMON("神号", ch_in(d, CH("申酉戌亥子丑寅卯辰巳午未", men)), L0, L0);
    DEMON("妨择", ch_in(d, CH("辰辰午午申申戌戌子子寅寅", men)), L0, L0);
    DEMON("披麻", ch_in(d, CH("午卯子酉午卯子酉午卯子酉", men)), L0, L("嫁娶", "入宅"));
    DEMON("大耗", ch_in(d, CH("辰巳午未申酉戌亥子丑寅卯", men)), L0, ARR_BAD_KU4, N_(ARR_BAD_KU4));
    DEMON("大祸", memcmp(CH("丁乙癸辛", yen % 4), d0, 3) == 0, L0, ARR_BAD_XIU2, N_(ARR_BAD_XIU2));
    DEMON("天吏", ch_in(d, CH("卯子酉午卯子酉午卯子酉午", men)), L0, ARR_BAD_TIANLI, N_(ARR_BAD_TIANLI));
    DEMON("天瘟", ch_in(d, CH("丑卯未戌辰寅午子酉申巳亥", men)), L0, L("修造", "求医疗病", "纳畜"));
    DEMON("天狱", ch_in(d, CH("午酉子卯午酉子卯午酉子卯", men)), L0, L0);
    DEMON("天火", ch_in(d, CH("午酉子卯午酉子卯午酉子卯", men)), L0, L("苫盖"));
    DEMON("天棒", ch_in(d, CH("寅辰午申戌子寅辰午申戌子", men)), L0, L0);
    DEMON("天狗", ch_in(d, CH("寅卯辰巳午未申酉戌亥子丑", men)), L0, L("祭祀"));
    DEMON("天狗下食", ch_in(d, CH("戌亥子丑寅卯辰巳午未申酉", men)), L0, L("祭祀"));
    DEMON("天贼", ch_in(d, CH("卯寅丑子亥戌酉申未午巳辰", men)), L0, L("出行", "修仓库", "开仓"));
    {
        static const char *const DINANG[12] = {"辛未辛酉", "乙酉乙未", "庚子庚午", "癸未癸丑",
                                               "甲子甲寅", "己卯己丑", "戊辰戊午", "癸未癸巳",
                                               "丙寅丙申", "丁卯丁巳", "戊辰戊子", "庚戌庚子"};
        DEMON("地囊", gz_in(DINANG[men], d), L0, ARR_BAD_TUFU, N_(ARR_BAD_TUFU));
    }
    DEMON("地火", ch_in(d, CH("子亥戌酉申未午巳辰卯寅丑", men)), L0, L("栽种"));
    DEMON("独火", ch_in(d, CH("未午巳辰卯寅丑子亥戌酉申", men)), L0, L("修造"));
    DEMON("受死", ch_in(d, CH("卯酉戌辰亥巳子午丑未寅申", men)), L0, L("畋猎"));
    DEMON("黄沙", ch_in(d, CH("寅子午寅子午寅子午寅子午", men)), L0, L("出行"));
    DEMON("六不成", ch_in(d, CH("卯未寅午戌巳酉丑申子辰亥", men)), L0, L("修造"));
    DEMON("小耗", ch_in(d, CH("卯辰巳午未申酉戌亥子丑寅", men)), L0, ARR_BAD_KU4, N_(ARR_BAD_KU4));
    DEMON("神隔", ch_in(d, CH("酉未巳卯丑亥酉未巳卯丑亥", men)), L0, L("祭祀", "祈福"));
    DEMON("朱雀", ch_in(d, CH("亥丑卯巳未酉亥丑卯巳未酉", men)), L0, L("嫁娶"));
    DEMON("白虎", ch_in(d, CH("寅辰午申戌子寅辰午申戌子", men)), L0, L("安葬"));
    DEMON("玄武", ch_in(d, CH("巳未酉亥丑卯巳未酉亥丑卯", men)), L0, L("安葬"));
    DEMON("勾陈", ch_in(d, CH("未酉亥丑卯巳未酉亥丑卯巳", men)), L0, L0);
    DEMON("木马", ch_in(d, CH("辰午巳未酉申戌子亥丑卯寅", men)), L0, L0);
    DEMON("破败", ch_in(d, CH("辰午申戌子寅辰午申戌子寅", men)), L0, L0);
    DEMON("殃败", ch_in(d, CH("巳辰卯寅丑子亥戌酉申未午", men)), L0, L0);
    DEMON("雷公", ch_in(d, CH("巳申寅亥巳申寅亥巳申寅亥", men)), L0, L0);
    DEMON("飞廉", ch_in(d, CH("申酉戌巳午未寅卯辰亥子丑", men)), L0, L("纳畜", "修造", "搬移", "嫁娶"));
    DEMON("大煞", ch_in(d, CH("申酉戌巳午未寅卯辰亥子丑", men)), L0, ARR_BAD_CHUAN2, N_(ARR_BAD_CHUAN2));
    DEMON("枯鱼", ch_in(d, CH("申巳辰丑戌未卯子酉午寅亥", men)), L0, L("栽种"));
    DEMON("九空", ch_in(d, CH("申巳辰丑戌未卯子酉午寅亥", men)), L0, ARR_BAD_KU9, N_(ARR_BAD_KU9));
    DEMON("八座", ch_in(d, CH("酉戌亥子丑寅卯辰巳午未申", men)), L0, L0);
    {
        static const char *const BAFENG[4] = {"丁丑己酉", "甲申甲辰", "辛未丁未", "甲戌甲寅"};
        DEMON("八风触水龙", gz_in(BAFENG[sn], d), L0, L("取鱼", "乘船渡水"));
    }
    DEMON("血忌", ch_in(d, CH("午子丑未寅申卯酉辰戌巳亥", men)), L0, L("针刺"));
    DEMON("阴错", gz_eq(CH("壬子癸丑庚寅辛卯庚辰丁巳丙午丁未甲申乙酉甲戌癸亥", men * 2), d), L0, L0);
    {
        static const int32_t SANNANG[] = {3, 7, 13, 18, 22, 27};
        DEMON("三娘煞", int_in(ldn, SANNANG, 6), L0, L("嫁娶", "结婚姻"));
    }
    DEMON("四绝", md_in(tmd, t4j, 4), L0, L("出行", "上官", "嫁娶", "进人口", "搬移", "开市", "立券交易", "祭祀"));
    DEMON("四离", md_in(tmd, t4l, 4), L0, L("出行", "嫁娶"));
    DEMON("四击", ch_in(d, CH("未未戌戌戌丑丑丑辰辰辰未", men)), L0, ARR_BAD_CHUAN2, N_(ARR_BAD_CHUAN2));
    {
        static const char *const SIHAO[4] = {"壬子", "乙卯", "戊午", "辛酉"};
        DEMON("四耗", strcmp(SIHAO[sn], d) == 0, L0,
              L("安抚边境", "选将", "出师", "修仓库", "开市", "立券交易", "纳财", "开仓"));
    }
    {
        static const char *const SIQIONG[4] = {"乙亥", "丁亥", "辛亥", "癸亥"};
        DEMON("四穷", strcmp(SIQIONG[sn], d) == 0, L0,
              L("安抚边境", "选将", "出师", "结婚姻", "纳采", "嫁娶", "进人口", "修仓库",
                "开市", "立券交易", "纳财", "开仓", "安葬"));
    }
    {
        static const char *const SIJI[4] = {"甲子", "丙子", "庚子", "壬子"};
        DEMON("四忌", strcmp(SIJI[sn], d) == 0, L0,
              L("安抚边境", "选将", "出师", "结婚姻", "纳采", "嫁娶", "安葬"));
    }
    {
        static const char *const SIFEI[4] = {"庚申辛酉", "壬子癸亥", "甲寅乙卯", "丁巳丙午"};
        DEMON("四废", gz_in(SIFEI[sn], d), L0, ARR_BAD_SIFEI, N_(ARR_BAD_SIFEI));
    }
    {
        static const char *const WUMU[12] = {"壬辰", "戊辰", "乙未", "乙未", "戊辰", "丙戌",
                                             "丙戌", "戊辰", "辛丑", "辛丑", "戊辰", "壬辰"};
        DEMON("五墓", strcmp(WUMU[men], d) == 0, L0, ARR_BAD_WUMU, N_(ARR_BAD_WUMU));
    }
    {
        static const char *const WUXU[4] = {"巳酉丑", "申子辰", "亥卯未", "寅午戌"};
        DEMON("五虚", ch_in(WUXU[sn], d1), L0, ARR_BAD_KU2, N_(ARR_BAD_KU2));
    }
    DEMON("五离", ch_in("申酉", d1), L("沐浴"), L("庆赐", "宴会", "结婚姻", "纳采", "立券交易"));
    DEMON("五鬼", ch_in(d, CH("未戌午寅辰酉卯申丑巳子亥", men)), L0, L("出行"));
    {
        static const char *const BAZHUAN[5] = {"丁未", "己未", "庚申", "甲寅", "癸丑"};
        bool hit = false;
        for (int i = 0; i < 5; i++) hit |= strcmp(BAZHUAN[i], d) == 0;
        DEMON("八专", hit, L0, ARR_BAD_MARRY3, N_(ARR_BAD_MARRY3));
    }
    DEMON("九坎", ch_in(d, CH("申巳辰丑戌未卯子酉午寅亥", men)), L0, L("塞穴", "补垣", "取鱼", "乘船渡水"));
    DEMON("九焦", ch_in(d, CH("申巳辰丑戌未卯子酉午寅亥", men)), L0, L("鼓铸", "栽种"));
    DEMON("天转", gz_eq(CH("乙卯丙午辛酉壬子", sn * 2), d), L0, L("修造", "搬移", "嫁娶"));
    DEMON("地转", gz_eq(CH("辛卯戊午癸酉丙子", sn * 2), d), L0, L("修造", "搬移", "嫁娶"));
    DEMON("月建转杀", ch_in(d, CH("卯午酉子", sn)), L0, L("修造"));
    DEMON("荒芜", ch_in_n(CH("巳酉丑申子辰亥卯未寅午戌", sn * 3), 3, d1), L0, L0);
    DEMON("蚩尤", ch_in(d, CH("戌子寅辰午申", men % 6)), L0, L0);
    DEMON("大时", ch_in(d, CH("酉午卯子酉午卯子酉午卯子", men)), L0, ARR_BAD_TIANLI, N_(ARR_BAD_TIANLI));
    DEMON("大败", ch_in(d, CH("酉午卯子酉午卯子酉午卯子", men)), L0, L0);
    DEMON("咸池", ch_in(d, CH("酉午卯子酉午卯子酉午卯子", men)), L0, L("嫁娶", "取鱼", "乘船渡水"));
    DEMON("土符", ch_in(d, CH("申子丑巳酉寅午戌卯未亥辰", men)), L0, ARR_BAD_TUFU, N_(ARR_BAD_TUFU));
    DEMON("土府", ch_in(d, CH("子丑寅卯辰巳午未申酉戌亥", men)), L0, ARR_BAD_TUFU, N_(ARR_BAD_TUFU));
    DEMON("土王用事", tuwang_days >= 0 && tuwang_days < 18, L0, ARR_BAD_TUFU, N_(ARR_BAD_TUFU));
    DEMON("血支", ch_in(d, CH("亥子丑寅卯辰巳午未申酉戌", men)), L0, L("针刺"));
    DEMON("游祸", ch_in(d, CH("亥申巳寅亥申巳寅亥申巳寅", men)), L0, L("祈福", "求嗣", "解除", "求医疗病"));
    DEMON("归忌", ch_in(d, CH("寅子丑寅子丑寅子丑寅子丑", men)), L0, L("搬移", "远回"));
    DEMON("岁薄", (lmn == 4 && (gz_eq(d, "戊午") || gz_eq(d, "丙午"))) ||
                  (lmn == 10 && (gz_eq(d, "壬子") || gz_eq(d, "戊子"))), L0, L0);
    DEMON("逐阵", (lmn == 6 && (gz_eq(d, "戊午") || gz_eq(d, "丙午"))) ||
                  (lmn == 12 && (gz_eq(d, "壬子") || gz_eq(d, "戊子"))), L0, L0);
    DEMON("阴阳交破", lmn == 10 && gz_eq(d, "丁巳"), L0, L0);
    {
        static const char *const BAORI[12] = {"丁未", "丁丑", "丙戌", "甲午", "庚子", "壬寅",
                                              "癸卯", "乙巳", "戊申", "己酉", "辛亥", "丙辰"};
        bool hit = false;
        for (int i = 0; i < 12; i++) hit |= strcmp(BAORI[i], d) == 0;
        DEMON("宝日", hit, L0, L0);
    }
    {
        static const char *const YIRI[12] = {"甲子", "丙寅", "丁卯", "己巳", "辛未", "壬申",
                                             "癸酉", "乙亥", "庚辰", "辛丑", "庚戌", "戊午"};
        bool hit = false;
        for (int i = 0; i < 12; i++) hit |= strcmp(YIRI[i], d) == 0;
        DEMON("义日", hit, L0, L0);
    }
    {
        static const char *const ZHIRI[12] = {"乙丑", "甲戌", "壬午", "戊子", "庚寅", "辛卯",
                                              "癸巳", "乙未", "丙申", "丁酉", "己亥", "甲辰"};
        bool hit = false;
        for (int i = 0; i < 12; i++) hit |= strcmp(ZHIRI[i], d) == 0;
        DEMON("制日", hit, L0, L0);
    }
    {
        static const char *const FARI[12] = {"庚午", "辛巳", "丙子", "戊寅", "己卯", "癸未",
                                             "癸丑", "甲申", "乙酉", "丁亥", "壬辰", "壬戌"};
        bool hit = false;
        for (int i = 0; i < 12; i++) hit |= strcmp(FARI[i], d) == 0;
        DEMON("伐日", hit, L0, ARR_BAD_CHUAN2, N_(ARR_BAD_CHUAN2));
    }
    {
        static const char *const ZHUANRI[12] = {"甲寅", "乙卯", "丁巳", "丙午", "庚申", "辛酉",
                                                "癸亥", "壬子", "戊辰", "戊戌", "己丑", "己未"};
        bool hit = false;
        for (int i = 0; i < 12; i++) hit |= strcmp(ZHUANRI[i], d) == 0;
        DEMON("专日", hit, L0, ARR_BAD_CHUAN2, N_(ARR_BAD_CHUAN2));
    }
    DEMON("重日", ch_in("巳亥", d1), L0, L("破土", "安葬", "启攒"));
    DEMON("复日", ch_in(d, CH("癸巳甲乙戊丙丁巳庚辛戊壬", men)), L("裁制"), ARR_BAD_ZANG3, N_(ARR_BAD_ZANG3));
}
