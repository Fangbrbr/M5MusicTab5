#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
@brief 由 translations.tsv 生成 i18n 静态 C 数组

输入：UTF-8 TSV，第一行为表头，首列为 key，后续每列一种语言。
输出：
  - service_i18n_generated_enum.h：语言枚举
  - service_i18n_generated.h：条目结构声明
  - service_i18n_generated.c：排序后的翻译表与语言 ID 数组
"""

import argparse
import csv
import os
import re
import sys
from pathlib import Path


def to_enum_name(lang_id: str) -> str:
    """把语言 ID 转换为 C 枚举名，例如 zh-CN -> I18N_LANG_ZH_CN。"""
    safe = re.sub(r"[^a-zA-Z0-9]", "_", lang_id).upper()
    # 去掉首尾下划线
    safe = safe.strip("_")
    if not safe:
        safe = "LANG"
    return f"I18N_LANG_{safe}"


def unescape_tsv_field(s: str) -> str:
    """TSV 字段转义约定：字面 \\n 表示真实换行（dropdown options 等多行 key）。"""
    return s.replace("\\n", "\n")


def escape_c_string(s: str) -> str:
    """转义 C 字符串中的特殊字符。"""
    s = s.replace("\\", "\\\\")
    s = s.replace('"', '\\"')
    s = s.replace("\n", "\\n")
    s = s.replace("\r", "\\r")
    s = s.replace("\t", "\\t")
    return s


def generate(tsv_path: Path, out_dir: Path) -> int:
    if not tsv_path.exists():
        print(f"error: translation file not found: {tsv_path}", file=sys.stderr)
        return 1

    out_dir.mkdir(parents=True, exist_ok=True)

    with tsv_path.open("r", encoding="utf-8", newline="") as f:
        reader = csv.reader(f, delimiter="\t")
        rows = list(reader)

    if len(rows) < 1:
        print("error: empty translation file", file=sys.stderr)
        return 1

    header = rows[0]
    if len(header) < 2:
        print("error: header needs at least key + one language", file=sys.stderr)
        return 1

    languages = header[1:]  # 去掉 key 列
    data_rows = rows[1:]

    # 过滤空行，并确保每行至少与表头等长；字段按约定把字面 \n 还原为真实换行
    entries = []
    for row in data_rows:
        if not row or (len(row) == 1 and row[0].strip() == ""):
            continue
        key = unescape_tsv_field(row[0])
        if not key.strip():
            continue
        translations = [unescape_tsv_field(row[i]) if i < len(row) else "" for i in range(1, len(header))]
        entries.append((key, translations))

    if not entries:
        print("warning: no translation entries found", file=sys.stderr)

    # 按 key 的 UTF-8 字节序排序：运行时 bsearch 用 strcmp（字节序比较）。
    # UTF-8 字节序与码点序一致，显式按字节排序确保与 strcmp 绝对对齐，
    # 避免任何隐式假设导致 bsearch 查错条目。
    entries.sort(key=lambda x: x[0].encode("utf-8"))

    enum_names = [to_enum_name(lang) for lang in languages]

    # 生成 enum 头文件
    enum_h = out_dir / "service_i18n_generated_enum.h"
    with enum_h.open("w", encoding="utf-8") as f:
        f.write("#ifndef SERVICE_I18N_GENERATED_ENUM_H\n")
        f.write("#define SERVICE_I18N_GENERATED_ENUM_H\n\n")
        f.write("typedef enum {\n")
        for i, name in enumerate(enum_names):
            f.write(f"    {name} = {i},\n")
        f.write(f"    I18N_LANG_COUNT = {len(enum_names)}\n")
        f.write("} i18n_language_t;\n\n")
        f.write(f"#define I18N_ENTRY_COUNT {len(entries)}\n\n")
        f.write("#endif /* SERVICE_I18N_GENERATED_ENUM_H */\n")

    # 生成内部头文件
    gen_h = out_dir / "service_i18n_generated.h"
    with gen_h.open("w", encoding="utf-8") as f:
        f.write("#ifndef SERVICE_I18N_GENERATED_H\n")
        f.write("#define SERVICE_I18N_GENERATED_H\n\n")
        f.write('#include "service_i18n_generated_enum.h"\n')
        f.write('#include <stddef.h>\n')
        f.write('#include <stdint.h>\n\n')
        f.write("typedef struct {\n")
        f.write("    const char *key;\n")
        f.write(f"    const char *translations[I18N_LANG_COUNT];\n")
        f.write("} i18n_entry_t;\n\n")
        f.write("/* 反向索引项：某语言译文 -> 词条序号（按译文排序，供双向查表） */\n")
        f.write("typedef struct {\n")
        f.write("    const char *text;\n")
        f.write("    uint16_t entry_idx;\n")
        f.write("} i18n_rev_entry_t;\n\n")
        f.write("extern const i18n_entry_t g_i18n_entries[I18N_ENTRY_COUNT];\n")
        f.write("extern const size_t g_i18n_entry_count;\n")
        f.write("extern const char * const g_i18n_language_ids[I18N_LANG_COUNT];\n\n")
        f.write("/* 反向索引注册表（列 1..N-1；第 0 列即 key 列，正向已覆盖） */\n")
        f.write("extern const i18n_rev_entry_t * const g_i18n_rev_tables[];\n")
        f.write("extern const size_t g_i18n_rev_counts[];\n")
        f.write("extern const size_t g_i18n_rev_table_count;\n")
        f.write("\n#endif /* SERVICE_I18N_GENERATED_H */\n")

    # 生成 C 文件
    gen_c = out_dir / "service_i18n_generated.c"
    with gen_c.open("w", encoding="utf-8") as f:
        f.write('#include "service_i18n_generated.h"\n\n')
        f.write("const i18n_entry_t g_i18n_entries[I18N_ENTRY_COUNT] = {\n")
        for key, translations in entries:
            escaped_key = escape_c_string(key)
            escaped_translations = [escape_c_string(t) for t in translations]
            trans_str = ", ".join(f'"{t}"' for t in escaped_translations)
            f.write(f'    {{ "{escaped_key}", {{ {trans_str} }} }},\n')
        f.write("};\n\n")
        f.write("const size_t g_i18n_entry_count = I18N_ENTRY_COUNT;\n\n")
        f.write("const char * const g_i18n_language_ids[I18N_LANG_COUNT] = {\n")
        for lang in languages:
            f.write(f'    "{escape_c_string(lang)}",\n')
        f.write("};\n\n")
        # 每种语言（除 key 列）的反向索引（译文非空才收录；按译文 UTF-8 字节序排序）
        rev_table_names = []
        for li in range(1, len(languages)):
            safe = languages[li].replace("-", "_")
            rev = sorted(
                ((e[1][li], i) for i, e in enumerate(entries) if e[1][li]),
                key=lambda x: x[0].encode("utf-8"),
            )
            name = f"g_i18n_rev_{safe}"
            rev_table_names.append(name)
            f.write(f"static const i18n_rev_entry_t {name}[] = {{\n")
            for text, i in rev:
                f.write(f'    {{ "{escape_c_string(text)}", {i} }},\n')
            f.write("};\n\n")
        f.write("const i18n_rev_entry_t * const g_i18n_rev_tables[] = {\n")
        for name in rev_table_names:
            f.write(f"    {name},\n")
        f.write("};\n\n")
        f.write("const size_t g_i18n_rev_counts[] = {\n")
        for name in rev_table_names:
            f.write(f"    sizeof({name}) / sizeof({name}[0]),\n")
        f.write("};\n\n")
        f.write(f"const size_t g_i18n_rev_table_count = {len(rev_table_names)};\n")

    print(f"generated: {enum_h}, {gen_h}, {gen_c}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate i18n C arrays from TSV")
    parser.add_argument("tsv", type=Path, help="input translations.tsv")
    parser.add_argument("out_dir", type=Path, help="output directory")
    args = parser.parse_args()
    return generate(args.tsv, args.out_dir)


if __name__ == "__main__":
    sys.exit(main())
