# cnlunar ESP-IDF C 移植版

[OPN48/cnlunar](https://github.com/OPN48/cnlunar)（Python，MIT License）的忠实 C 语言移植，
面向 ESP32 / ESP-IDF（同样适用于任意 C99 环境）。

- **零动态内存**：所有结果写入调用者提供的 `cnlunar_t` 结构体，字符串均为静态常量指针
- **零浮点运算**：全部为整数查表与规则匹配
- **零依赖**：仅需 libc（string.h / stdio.h），不依赖 ESP-IDF 特有 API
- **已验证**：与 Python 原版在 **1901-01-01 ~ 2100-02-08 全量 72723 天** 上逐字段对拍一致
  （含 hour=0/12/23、godType、year8Char、yeargod 各配置组合）

## 支持范围与已知边界

- 公历 **1901-01-01 ~ 2100-02-08**。农历月数据表实际仅覆盖 1901-2099 年，
  Python 原版在 2100 年春节及之后会抛 `IndexError`，本库对超出范围的日期返回 `-1`
- 移植完全复刻原版行为，包括：`hour>0` 时土王用事窗口偏移一天、
  伐木增补规则中 `d in ['午','申']` 恒为假等原版既有特性
- 输出字段与原版一致：农历日期/闰月/大小月、八字干支（年/月/日/时）、生肖、冲煞、
  二十四节气、二十八宿、建除十二神、黄黑道、宜忌（含《协纪辨方书》清洗规则）、
  吉神凶神、宜忌等第、月相、星座、星次、纳音、胎神、彭祖百忌、九宫飞星、
  吉神方位、时辰吉凶、子午流注、法定/公历/农历节日（含母亲节父亲节）

## 目录结构

```
├── CMakeLists.txt          # ESP-IDF 组件
├── include/cnlunar.h       # 公共 API
├── src/
│   ├── cnlunar.c           # 核心算法（手写移植）
│   ├── cnlunar_data.c/h    # 数据表（由 tools/gen_tables.py 自动生成，勿手改）
├── host_test/              # 宿主机对拍工具
└── tools/                  # gen_tables.py / ref_dump.py
```

## 快速上手

将本目录复制到工程的 `components/` 下（或加入 `EXTRA_COMPONENT_DIRS`），然后：

```c
#include "cnlunar.h"

static cnlunar_t lunar;   // 约 2KB，建议静态或堆上分配

if (cnlunar_compute(&lunar, 2026, 7, 20, 12, NULL) == 0) {
    // 农历：二零二六年 六月大 初七
    printf("%s年 %s %s\n", lunar.lunar_year_cn, lunar.lunar_month_cn, lunar.lunar_day_cn);
    // 干支：丙午年 乙未月 乙未日
    printf("%s %s %s\n", lunar.year_ganzhi, lunar.month_ganzhi, lunar.day_ganzhi);
    // 宜 / 忌
    char yi[512], ji[512];
    cnlunar_join_things(&lunar, true, yi, sizeof(yi));
    cnlunar_join_things(&lunar, false, ji, sizeof(ji));
    printf("宜 %s\n忌 %s\n", yi, ji);
    // 节气、生肖、冲煞、28宿、黄道
    printf("节气:%s 下一节气:%s %d月%d日\n", lunar.today_solar_term,
           lunar.next_solar_term, lunar.next_solar_term_month, lunar.next_solar_term_day);
    printf("%s %s %s宿 %s日\n", lunar.zodiac, lunar.zodiac_clash,
           lunar.star28, lunar.is_yellow_day ? "黄道" : "黑道");
}

// 节日（可选）
char buf[128];
cnlunar_get_legal_holidays(&lunar, buf, sizeof(buf));
```

### LVGL 提示

`good_thing` / `bad_thing` 等字段是指向静态字符串的指针数组，直接喂给
`lv_label_set_text` / `cnlunar_join_things` 拼接即可；注意需要加载 CJK 字体。

### 配置项（对应原版构造参数）

```c
cnlunar_config_t cfg;
cnlunar_config_default(&cfg);           // godType='8char', year8Char='year', yeargod='duty'
cfg.god_type   = CNLUNAR_GOD_TYPE_LUNAR_MONTH;  // godType='cnlunar'
cfg.year_ganzhi= CNLUNAR_YEAR_GANZHI_LICHUN;    // year8Char='beginningOfSpring'
cfg.yeargod_duty = false;                       // yeargod='notDuty'
cnlunar_compute(&lunar, y, m, d, hour, &cfg);
```

## 对拍验证方法

```bash
cd host_test && cmake -B build && cmake --build build
./build/dump 1901 2100 12 > /tmp/c.txt
python3 ../tools/ref_dump.py 1901 2100 12 > /tmp/py.txt   # 需仓库根部的 cnlunar Python 包
diff /tmp/c.txt /tmp/py.txt                               # 应为空
# 配置组合：dump <y0> <y1> <hour> <god_type> <lichun> <yeargod_duty>
./build/dump 1997 1998 23 1 0 0 > /tmp/c2.txt
python3 ../tools/ref_dump.py 1997 1998 23 1 0 0 > /tmp/py2.txt
diff /tmp/c2.txt /tmp/py2.txt
```

修改 `src/cnlunar_data.*` 请通过 `python3 tools/gen_tables.py` 重新生成。

## 许可证

MIT。算法与数据来自 [OPN48/cnlunar](https://github.com/OPN48/cnlunar)，
遵循其 MIT License；宜忌规则源自《协纪辨方书》，干支宜忌参考《盘古历》。
使用请保留出处标注。
