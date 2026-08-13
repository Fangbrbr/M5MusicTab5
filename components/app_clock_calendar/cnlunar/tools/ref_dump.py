#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Python 版参考输出，格式与 host_test/dump.c 完全一致。"""
import sys, datetime
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..'))  # 指向仓库根（含 cnlunar 包）
import cnlunar

y0, y1, h = 1901, 2100, 12
if len(sys.argv) > 2:
    y0, y1 = int(sys.argv[1]), int(sys.argv[2])
if len(sys.argv) > 3:
    h = int(sys.argv[3])
god_type = 'cnlunar' if len(sys.argv) > 4 and sys.argv[4] == '1' else '8char'
year8char = 'beginningOfSpring' if len(sys.argv) > 5 and sys.argv[5] == '1' else 'year'
yeargod = 'notDuty' if len(sys.argv) > 6 and sys.argv[6] == '0' else 'duty'

GOD_START = [8, 10, 0, 2, 4, 6, 8, 10, 0, 2, 4, 6]

def is_leap(y):
    return y % 4 == 0 and (y % 100 != 0 or y % 400 == 0)

MDAYS = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]

out = sys.stdout
d0 = datetime.date(y0, 1, 1)
d1 = datetime.date(y1, 12, 31)
day = d0
one = datetime.timedelta(days=1)
while day <= d1:
    y, m, dd = day.year, day.month, day.day
    day += one
    try:
        a = cnlunar.Lunar(datetime.datetime(y, m, dd, h), godType=god_type, year8Char=year8char, yeargod=yeargod)
    except IndexError:
        continue
    men = (a.lunarMonth - 1 + 2) % 12 if god_type == 'cnlunar' else a.monthEarthNum
    ecl = (a.dayEarthNum - GOD_START[men]) % 12
    yellow = 1 if ecl in (0, 1, 4, 5, 7, 10) else 0
    lucky = ''.join('1' if x == '吉' else '0' for x in a.get_twohourLuckyList())
    p = []
    p.append(f"{y}-{m:02d}-{dd:02d}")
    p.append(f"{a.lunarYear},{a.lunarMonth},{a.lunarDay},{1 if a.isLunarLeapMonth else 0},{1 if a.lunarMonthLong else 0},{a.spanDays}")
    p.append(f"{a.lunarYearCn},{a.lunarMonthCn},{a.lunarDayCn}")
    p.append(a.phaseOfMoon)
    p.append(a.weekDayCn)
    p.append(a.starZodiac)
    p.append(a.todayEastZodiac)
    p.append(a.todaySolarTerms)
    p.append(f"{a.nextSolarTerm},{a.nextSolarTermDate[0]},{a.nextSolarTermDate[1]},{a.nextSolarTermYear},{a.nextSolarNum}")
    p.append(f"{a.year8Char},{a.month8Char},{a.day8Char},{a.twohour8Char}")
    p.append(','.join(a.twohour8CharList))
    p.append(str(a.twohourNum))
    p.append(f"{a.chineseYearZodiac},{a.chineseZodiacClash},{a.zodiacMark6},{a.zodiacMark3List[0]},{a.zodiacMark3List[1]}")
    p.append(f"{a.lunarSeasonName},{a.lunarSeasonNum}")
    p.append(f"{a.today28Star},{a.today12DayOfficer},{a.today12DayGod},{yellow}")
    p.append(f"{a.get_nayin()},{'木木火火土土金金水水'[a.dayHeavenNum]},{'水土木木土火火土金金土水'[a.dayEarthNum]},{a.get_fetalGod()}")
    p.append(a.get_the9FlyStar())
    p.append(','.join(a.get_luckyGodsDirection()))
    p.append(a.meridians)
    p.append(lucky)
    p.append(f"{a.todayLevel},{a.thingLevelName},{1 if a.isDe else 0}")
    p.append(','.join(sorted(a.goodGodName)))
    p.append(','.join(sorted(a.badGodName)))
    p.append(','.join(sorted(a.goodThing)))
    p.append(','.join(sorted(a.badThing)))
    p.append(a.get_legalHolidays())
    p.append(a.get_otherHolidays())
    p.append(a.get_otherLunarHolidays())
    from cnlunar.config import pengTatooList
    p.append(f"{pengTatooList[a.dayHeavenNum]},{pengTatooList[a.dayEarthNum + 10]}")
    p.append(a.todayLevelName)
    out.write('|'.join(p) + '\n')
