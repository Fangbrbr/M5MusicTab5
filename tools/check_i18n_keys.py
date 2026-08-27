#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
@brief 校验 EEZ 生成代码中的 _() 翻译 key 是否全部被 translations.tsv 覆盖

扫描 components/engine_gui/src/ui/*.c 中的 `_("...")` 调用，C 反转义后按
TSV 约定（字面 \\n 表示换行）与 translations.tsv 首列对比，列出缺失词条。

用法：python tools/check_i18n_keys.py   # 有缺失时退出码 1
"""

import csv
import re
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
UI_SRC_DIR = PROJECT_ROOT / "components/engine_gui/src/ui"
COMPONENTS_DIR = PROJECT_ROOT / "components"
TSV = PROJECT_ROOT / "components/service_i18n/translations.tsv"

RE_I18N_CALL = re.compile(r'_\("((?:[^"\\]|\\.)*)"\)')


def c_unescape(token: str) -> str:
    """C 字符串字面量反转义（覆盖生成代码出现的转义子集）。"""
    out = []
    i = 0
    while i < len(token):
        if token[i] == "\\" and i + 1 < len(token):
            nxt = token[i + 1]
            out.append({"n": "\n", "r": "\r", "t": "\t",
                        "\\": "\\", '"': '"'}.get(nxt, "\\" + nxt))
            i += 2
        else:
            out.append(token[i])
            i += 1
    return "".join(out)


def main() -> int:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    keys = set()
    # EEZ 生成代码 + 全部业务组件源码（runtime _() 调用点）
    sources = sorted(UI_SRC_DIR.glob("*.c")) + [
        p for p in sorted(COMPONENTS_DIR.rglob("*.c")) + sorted(COMPONENTS_DIR.rglob("*.cpp"))
        if UI_SRC_DIR not in p.parents
    ]
    for src_path in sources:
        try:
            src = src_path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        for m in RE_I18N_CALL.finditer(src):
            keys.add(c_unescape(m.group(1)))

    with TSV.open(encoding="utf-8", newline="") as f:
        tsv_keys = {row[0] for row in csv.reader(f, delimiter="\t") if row}

    # TSV 约定：字面 \n 表示换行；TSV key 先还原为真实 key 再比对
    tsv_keys_real = {k.replace("\\n", "\n") for k in tsv_keys}

    missing = sorted(k for k in keys if k not in tsv_keys_real)
    print(f"_() key 总数: {len(keys)}，TSV 覆盖: {len(keys) - len(missing)}，"
          f"缺失: {len(missing)}")
    for k in missing:
        print(f"  MISSING: {k.replace(chr(10), chr(92) + 'n')}")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
