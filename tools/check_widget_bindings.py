#!/usr/bin/env python3
"""
校验 App 的 WIDGET_BIND 名称是否全部存在于 EEZ Studio 工程中。

在前端修改、重命名或误删控件后，后端绑定表若未同步，会在运行时因访问
NULL 控件指针而死机。此脚本在构建阶段扫描所有 App 的 WIDGET_BIND 调用，
并与 components/engine_gui/TAB_MusicBox.eez-project 中的控件 identifier
比对，发现缺失立即报错退出，使绑定错误在编译/构建期暴露。
"""

import json
import re
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
EEZ_PROJECT = PROJECT_ROOT / "components" / "engine_gui" / "TAB_MusicBox.eez-project"
APP_DIR = PROJECT_ROOT / "components"

# 匹配单行 WIDGET_BIND(type, field, "widget_name", kind)
WIDGET_BIND_RE = re.compile(
    r'WIDGET_BIND\s*\(\s*[^,]+\s*,\s*[^,]+\s*,\s*"([^"]+)"\s*,[^)]*\)'
)


def extract_identifiers(obj):
    """递归提取 JSON 中所有 identifier 字段的字符串值。"""
    ids = set()
    if isinstance(obj, dict):
        for key, value in obj.items():
            if key == "identifier" and isinstance(value, str) and value:
                ids.add(value)
            else:
                ids.update(extract_identifiers(value))
    elif isinstance(obj, list):
        for item in obj:
            ids.update(extract_identifiers(item))
    return ids


def collect_app_bindings():
    """扫描 components/app_* 下的 app_*.c，返回 {文件: [widget_name]}。"""
    bindings = {}
    for app_c in sorted(APP_DIR.glob("app_*/app_*.c")):
        text = app_c.read_text(encoding="utf-8")
        names = WIDGET_BIND_RE.findall(text)
        if names:
            bindings[str(app_c.relative_to(PROJECT_ROOT))] = names
    return bindings


def main():
    if not EEZ_PROJECT.exists():
        print(f"check_widget_bindings: EEZ project not found: {EEZ_PROJECT}",
              file=sys.stderr)
        sys.exit(1)

    try:
        eez = json.loads(EEZ_PROJECT.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        print(f"check_widget_bindings: failed to parse EEZ project: {exc}",
              file=sys.stderr)
        sys.exit(1)

    valid_ids = extract_identifiers(eez)
    bindings = collect_app_bindings()

    missing = []
    for path, names in bindings.items():
        for name in names:
            if name not in valid_ids:
                missing.append((path, name))

    if missing:
        print("check_widget_bindings: widget binding mismatch detected",
              file=sys.stderr)
        print("The following widget names are used in WIDGET_BIND but do not exist "
              "in the EEZ Studio project:", file=sys.stderr)
        for path, name in missing:
            print(f"  {path}: '{name}'", file=sys.stderr)
        print("Please fix the EEZ Studio project or update the binding table.",
              file=sys.stderr)
        sys.exit(1)

    total = sum(len(v) for v in bindings.values())
    print(f"check_widget_bindings: {total} binding(s) verified across "
          f"{len(bindings)} app(s)")
    sys.exit(0)


if __name__ == "__main__":
    main()
