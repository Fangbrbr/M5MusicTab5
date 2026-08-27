#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
@brief EEZ 工程多语言批处理：label/dropdown/roller/tab 的 literal 文本转 translated-literal

直接按行处理 TAB_MusicBox.eez-project（EEZ 工程是逐行 JSON，"text"/"textType"、
"options"/"optionsType"、"tabName"/"tabNameType" 恒为相邻键值对，已全局验证）。

跳过规则（保持 literal 不动）：
  1. 空串 / 纯空白
  2. 不含任何字母或 CJK/假名/谚文（纯数字、标点、占位符、图标字体）
  3. setting_language 下拉框的 options（语言名本身不翻译）

同时把转换出的翻译 key 归一化（字面 \\n → 真实换行）写入 key 清单文件，
供 translations.tsv 同步使用。TSV 约定：字段内字面 \\n 表示换行。

用法：
  python tools/eez_i18n_prepare.py            # 执行转换并输出报告
  python tools/eez_i18n_prepare.py --dry-run  # 只报告不修改
"""

import argparse
import json
import re
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
EEZ_PROJECT = PROJECT_ROOT / "components/engine_gui/TAB_MusicBox.eez-project"
KEYS_OUT = PROJECT_ROOT / "build" / "i18n_eez_keys.txt"

# "text" -> "textType" 等属性对；相邻出现
PROP_PAIRS = {"text": "textType", "options": "optionsType", "tabName": "tabNameType"}

RE_PROP = re.compile(
    r'^\s*"(text|options|tabName)": ("(?:[^"\\]|\\.)*"),?$'
)
RE_TYPE = re.compile(
    r'^\s*"(textType|optionsType|tabNameType)": "(literal|translated-literal)",?$'
)
RE_IDENTIFIER = re.compile(r'^\s*"identifier": ("(?:[^"\\]|\\.)*"),?$')

# 含字母 / CJK / 假名 / 谚文 才值得翻译
RE_TRANSLATABLE = re.compile(
    r"[A-Za-z一-鿿぀-ヿ가-힯]"
)

# 语言名下拉框永不翻译
SKIP_IDENTIFIERS = {"setting_language"}


def decode_json_string(token: str) -> str:
    """解析 JSON 字符串 token（含外层引号）为原始文本。"""
    return json.loads(token)


def normalize_key(text: str) -> str:
    """EEZ 代码生成把字面 \\n 转成真实换行，key 与运行时保持一致。"""
    return text.replace("\\n", "\n")


def is_translatable(decoded: str) -> bool:
    """判定文本是否值得翻译。先归一化 \\n，否则字面 \\n 里的字母 n 会误判。"""
    s = normalize_key(decoded)
    if not s.strip():
        return False
    return RE_TRANSLATABLE.search(s) is not None


def key_to_tsv(key: str) -> str:
    """TSV 字段不能含真实换行，用字面 \\n 表示。"""
    return key.replace("\n", "\\n")


def main() -> int:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true", help="只报告不修改文件")
    args = parser.parse_args()

    lines = EEZ_PROJECT.read_text(encoding="utf-8").splitlines(keepends=True)

    converted = {"text": 0, "options": 0, "tabName": 0}
    skipped = []
    keys = set()
    last_identifier = None
    pending_prop = None  # (prop_name, raw_token, line_no)

    for idx, line in enumerate(lines):
        m_ident = RE_IDENTIFIER.match(line)
        if m_ident:
            last_identifier = decode_json_string(m_ident.group(1))
            pending_prop = None
            continue

        m_prop = RE_PROP.match(line)
        if m_prop:
            pending_prop = (m_prop.group(1), m_prop.group(2), idx + 1)
            continue

        m_type = RE_TYPE.match(line)
        if m_type:
            type_name = m_type.group(1)
            type_value = m_type.group(2)
            # 必须与上一行的 text/options/tabName 配对，否则结构漂移，拒绝猜测
            if pending_prop is None or PROP_PAIRS[pending_prop[0]] != type_name:
                print(f"warning: 第 {idx + 1} 行 {type_name} 未找到相邻属性行，跳过",
                      file=sys.stderr)
                pending_prop = None
                continue

            prop_name, raw_token, prop_line_no = pending_prop
            pending_prop = None

            decoded = decode_json_string(raw_token)
            is_lang_dropdown = prop_name == "options" and (
                last_identifier in SKIP_IDENTIFIERS or "English" in decoded
            )

            # 自纠错：已被转成 translated-literal 但内容不可翻译的，回退 literal
            if type_value == "translated-literal":
                if not is_translatable(decoded) or is_lang_dropdown:
                    lines[idx] = line.replace('"translated-literal"', '"literal"')
                    skipped.append((prop_line_no, prop_name, decoded[:40],
                                    "回退（不可翻译）"))
                else:
                    keys.add(normalize_key(decoded))
                continue

            if not is_translatable(decoded):
                skipped.append((prop_line_no, prop_name, decoded[:40],
                                "空串或无文字字符"))
                continue
            if is_lang_dropdown:
                skipped.append((prop_line_no, prop_name, decoded[:40], "语言名下拉框"))
                continue

            lines[idx] = line.replace('"literal"', '"translated-literal"')
            converted[prop_name] += 1
            keys.add(normalize_key(decoded))
            continue

        # 其它行不打破配对状态的情况：属性行与 Type 行之间不允许夹任何内容
        if line.strip() and pending_prop is not None:
            pending_prop = None

    total = sum(converted.values())
    print(f"转换: label text={converted['text']}  options={converted['options']}  "
          f"tabName={converted['tabName']}  合计={total}")
    print(f"跳过: {len(skipped)} 处")
    for line_no, prop, text, reason in skipped:
        print(f"  L{line_no} [{prop}] {text} -- {reason}")

    if args.dry_run:
        print("dry-run，未修改文件")
        return 0

    EEZ_PROJECT.write_text("".join(lines), encoding="utf-8")

    KEYS_OUT.parent.mkdir(parents=True, exist_ok=True)
    with KEYS_OUT.open("w", encoding="utf-8", newline="\n") as f:
        for key in sorted(keys):
            f.write(key_to_tsv(key) + "\n")
    print(f"key 清单: {KEYS_OUT} ({len(keys)} 条，TSV 转义约定)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
