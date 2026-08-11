#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从 cnlunar 的 config.py / holidays.py 自动生成 C 数据表，避免手工转录出错。"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..'))  # 指向仓库根（含 cnlunar 包）
from cnlunar import config as C
from cnlunar import holidays as H

OUT_H = os.path.join(os.path.dirname(__file__), '..', 'src', 'cnlunar_data.h')
OUT_C = os.path.join(os.path.dirname(__file__), '..', 'src', 'cnlunar_data.c')

def emit_str_array(f, name, items, ctype='const char *const'):
    f.write(f'{ctype} {name}[{len(items)}] = {{\n')
    for i in range(0, len(items), 6):
        chunk = items[i:i+6]
        f.write('    ' + ', '.join(f'"{s}"' for s in chunk) + ',\n')
    f.write('};\n\n')

def str_list_structs(f, pyname, cname, lists):
    """lists: list of tuple-of-str -> 生成数组 + {ptr,n} 索引表"""
    f.write(f'static const char *const {cname}_buf[] = {{\n')
    counts = []
    for lst in lists:
        for s in lst:
            f.write(f'    "{s}",\n')
        counts.append(len(lst))
    f.write('};\n')
    f.write(f'const cnl_strlist_t {cname}[{len(lists)}] = {{\n')
    off = 0
    for i, n in enumerate(counts):
        f.write(f'    {{ {cname}_buf + {off}, {n} }},\n')
        off += n
    f.write('};\n\n')

header_guard = '''/* Auto-generated from cnlunar config.py/holidays.py. DO NOT EDIT. */
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

'''

with open(OUT_H, 'w', encoding='utf-8') as h, open(OUT_C, 'w', encoding='utf-8') as c:
    h.write(header_guard)
    c.write('/* Auto-generated from cnlunar config.py/holidays.py. DO NOT EDIT. */\n')
    c.write('#include "cnlunar_data.h"\n\n')

    decl = lambda s: h.write(s + '\n')

    # ---- 简单字符串数组 ----
    simple_tables = [
        ('CNL_STAR_ZODIAC_NAME', list(C.STAR_ZODIAC_NAME)),
        ('CNL_SOLAR_TERMS_NAME', list(C.SOLAR_TERMS_NAME_LIST)),
        ('CNL_EAST_ZODIAC', list(C.EAST_ZODIAC_LIST)),
        ('CNL_STEMS', list(C.the10HeavenlyStems)),
        ('CNL_STEM_ELEMENTS', list(C.the10HeavenlyStems5ElementsList)),
        ('CNL_BRANCHES', list(C.the12EarthlyBranches)),
        ('CNL_BRANCH_ELEMENTS', list(C.the12EarthlyBranches5ElementsList)),
        ('CNL_NAYIN', list(C.theHalf60HeavenlyEarth5ElementsList)),
        ('CNL_STARS28', list(C.the28StarsList)),
        ('CNL_PENG_TABOO', list(C.pengTatooList)),
        ('CNL_ZODIAC', list(C.chineseZodiacNameList)),
        ('CNL_12_DAY_GODS', list(C.chinese12DayGods)),
        ('CNL_DIRECTIONS', list(C.directionList)),
        ('CNL_FETAL_GOD', list(C.fetalGodList)),
        ('CNL_MERIDIANS', list(C.meridiansName)),
        ('CNL_LUNAR_MONTH_NAMES', list(C.lunarMonthNameList)),
        ('CNL_LUNAR_DAY_NAMES', list(C.lunarDayNameList)),
        ('CNL_UPPER_NUM', list(C.upperNum)),
        ('CNL_WEEKDAY', list(C.weekDay)),
        ('CNL_THINGS_SORT', list(C.thingsSort)),
        ('CNL_BUJIANG', list(C.bujiang)),
    ]
    for name, items in simple_tables:
        decl(f'extern const char *const {name}[{len(items)}];')
        emit_str_array(c, name, items)

    # ---- 单字符串（按字索引用）----
    single_strings = [
        ('CNL_12_OFFICERS', C.chinese12DayOfficers),
        ('CNL_8TRIGRAMS', C.chinese8Trigrams),
        ('CNL_LUCKY_GOD_DIR', C.luckyGodDirection),
        ('CNL_WEALTH_GOD_DIR', C.wealthGodDirection),
        ('CNL_MASCOT_GOD_DIR', C.mascotGodDirection),
        ('CNL_SUN_NOBLE_DIR', C.sunNobleDirection),
        ('CNL_MOON_NOBLE_DIR', C.moonNobleDirection),
    ]
    for name, s in single_strings:
        decl(f'extern const char {name}[];')
        c.write(f'const char {name}[] = "{s}";\n\n')

    # ---- 星座日期表 ----
    decl('extern const uint8_t CNL_STAR_ZODIAC_DATE[12][2];')
    c.write('const uint8_t CNL_STAR_ZODIAC_DATE[12][2] = {\n')
    for m, d in C.STAR_ZODIAC_DATE:
        c.write(f'    {{{m}, {d}}},\n')
    c.write('};\n\n')

    # ---- 节气编码向量与数据 ----
    decl('extern const uint8_t CNL_ENC_VECTOR[24];')
    c.write('const uint8_t CNL_ENC_VECTOR[24] = {\n    ')
    c.write(', '.join(str(x) for x in C.ENC_VECTOR_LIST))
    c.write('\n};\n\n')
    decl('extern const uint64_t CNL_SOLAR_TERMS_DATA[200];')
    c.write('const uint64_t CNL_SOLAR_TERMS_DATA[200] = {\n')
    for i in range(0, 200, 5):
        chunk = C.SOLAR_TERMS_DATA_LIST[i:i+5]
        c.write('    ' + ', '.join(f'0x{x:012x}ULL' for x in chunk) + ',\n')
    c.write('};\n\n')

    # ---- 农历数据 ----
    decl('extern const uint32_t CNL_LUNAR_MONTH_DATA[200];')
    c.write('const uint32_t CNL_LUNAR_MONTH_DATA[200] = {\n')
    for i in range(0, 200, 8):
        chunk = C.lunarMonthData[i:i+8]
        c.write('    ' + ', '.join(f'0x{x:05x}u' for x in chunk) + ',\n')
    c.write('};\n\n')
    decl('extern const uint8_t CNL_LUNAR_NEW_YEAR[200];')
    c.write('const uint8_t CNL_LUNAR_NEW_YEAR[200] = {\n')
    for i in range(0, 200, 12):
        chunk = C.lunarNewYearList[i:i+12]
        c.write('    ' + ', '.join(f'0x{x:02x}' for x in chunk) + ',\n')
    c.write('};\n\n')

    # ---- 时辰吉凶 ----
    decl('extern const uint16_t CNL_TWOHOUR_LUCKY[60];')
    c.write('const uint16_t CNL_TWOHOUR_LUCKY[60] = {\n')
    for i in range(0, 60, 6):
        chunk = C.twohourLuckyTimeList[i:i+6]
        c.write('    ' + ', '.join(f'0x{x:03x}' for x in chunk) + ',\n')
    c.write('};\n\n')

    # ---- 建除十二神宜忌 ----
    officers = list(C.chinese12DayOfficers)
    decl(f'extern const cnl_strlist_t CNL_OFFICER_GOOD[12];')
    decl(f'extern const cnl_strlist_t CNL_OFFICER_BAD[12];')
    str_list_structs(c, 'officerGood', 'CNL_OFFICER_GOOD', [list(C.officerThings[o][0]) for o in officers])
    str_list_structs(c, 'officerBad', 'CNL_OFFICER_BAD', [list(C.officerThings[o][1]) for o in officers])

    # ---- 日干/日支宜忌 ----
    keys = list(C.day8CharThing.keys())
    decl(f'#define CNL_DAY8CHAR_THING_NUM {len(keys)}')
    decl('extern const char *const CNL_DAY8CHAR_KEY[];')
    decl('extern const cnl_strlist_t CNL_DAY8CHAR_GOOD[];')
    decl('extern const cnl_strlist_t CNL_DAY8CHAR_BAD[];')
    c.write('const char *const CNL_DAY8CHAR_KEY[] = {\n')
    for k in keys:
        c.write(f'    "{k}",\n')
    c.write('};\n\n')
    str_list_structs(c, 'd8cGood', 'CNL_DAY8CHAR_GOOD', [list(C.day8CharThing[k][0]) for k in keys])
    str_list_structs(c, 'd8cBad', 'CNL_DAY8CHAR_BAD', [list(C.day8CharThing[k][1]) for k in keys])

    # ---- 节假日表 ----
    def md_table(pyname, cname, pairs):
        decl(f'extern const cnl_md_name_t {cname}[{len(pairs)}];')
        c.write(f'const cnl_md_name_t {cname}[{len(pairs)}] = {{\n')
        for (m, d), name in pairs:
            c.write(f'    {{{m}, {d}, "{name}"}},\n')
        c.write('};\n\n')

    md_table('legalHolidaysDic', 'CNL_LEGAL_HOLIDAYS', sorted(C_legal := list(H.legalHolidaysDic.items())))
    md_table('legalLunarHolidaysDic', 'CNL_LEGAL_LUNAR_HOLIDAYS', sorted(H.legalLunarHolidaysDic.items()))

    other = []
    for mi, dic in enumerate(H.otherHolidaysList):
        for d, name in sorted(dic.items()):
            other.append(((mi + 1, d), name))
    md_table('otherHolidaysList', 'CNL_OTHER_HOLIDAYS', other)

    other_lunar = []
    for mi, dic in enumerate(H.otherLunarHolidaysList):
        for d, name in sorted(dic.items()):
            other_lunar.append(((mi + 1, d), name))
    md_table('otherLunarHolidaysList', 'CNL_OTHER_LUNAR_HOLIDAYS', other_lunar)

    h.write('\n#endif /* CNLUNAR_DATA_H */\n')

print('generated', OUT_H, OUT_C)
