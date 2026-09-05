#!/usr/bin/env python3
"""
校验 App 的 WIDGET_BIND 名称是否全部存在于 EEZ 生成代码中。

App 后端实际绑定的是生成代码里的屏幕结构体字段（ui_screen_*_t，见
components/engine_gui/src/ui/screens.h），因此校验目标就是生成代码本身，
不依赖 EEZ 工程文件。前端重命名/误删控件后若后端未同步，运行时会访问
NULL 控件指针死机；此脚本在构建阶段比对，发现缺失立即报错退出。
"""

import re
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
SCREENS_H = PROJECT_ROOT / "components" / "engine_gui" / "src" / "ui" / "screens.h"
APP_DIR = PROJECT_ROOT / "components"

# 匹配单行 WIDGET_BIND(type, field, "widget_name", kind)
WIDGET_BIND_RE = re.compile(
    r'WIDGET_BIND\s*\(\s*[^,]+\s*,\s*[^,]+\s*,\s*"([^"]+)"\s*,[^)]*\)'
)

# 生成代码中屏幕结构体的控件字段：lv_obj_t *widget_name;
WIDGET_FIELD_RE = re.compile(r'^\s*lv_obj_t\s*\*\s*([A-Za-z_][A-Za-z0-9_]*)\s*;', re.M)

# engine_gui.c 的 AI LED 映射表（按名字符串经 EEZ flow 查找控件）
ENGINE_GUI_C = PROJECT_ROOT / "components" / "engine_gui" / "engine_gui.c"
AI_LED_MAP_RE = re.compile(r's_ai_led_map\[\]\s*=\s*\{(.*?)\};', re.S)
AI_LED_NAME_RE = re.compile(r'"([^"]+)"')


def collect_generated_widgets():
    """从 screens.h 提取全部控件字段名。"""
    text = SCREENS_H.read_text(encoding="utf-8")
    return set(WIDGET_FIELD_RE.findall(text))


def collect_app_bindings():
    """扫描 components/app_* 下的 app_*.c，返回 {文件: [widget_name]}。"""
    bindings = {}
    for app_c in sorted(APP_DIR.glob("app_*/app_*.c")):
        text = app_c.read_text(encoding="utf-8")
        names = WIDGET_BIND_RE.findall(text)
        if names:
            bindings[str(app_c.relative_to(PROJECT_ROOT))] = names
    return bindings


def collect_ai_led_names():
    """提取 engine_gui.c 的 s_ai_led_map 控件名。

    Why: 该表经 EEZ flow 按名查找控件；生成代码把 sizeof(objects)（字节数）
    误作对象个数，名字 miss 会越界扫表直至 strcmp 空指针崩溃（真机教训），
    因此映射名必须在构建期硬校验，不允许运行时 miss。
    """
    if not ENGINE_GUI_C.exists():
        return []
    text = ENGINE_GUI_C.read_text(encoding="utf-8")
    m = AI_LED_MAP_RE.search(text)
    if not m:
        return []
    return AI_LED_NAME_RE.findall(m.group(1))


def main():
    if not SCREENS_H.exists():
        print(f"check_widget_bindings: generated screens.h not found: {SCREENS_H}",
              file=sys.stderr)
        sys.exit(1)

    valid_ids = collect_generated_widgets()
    bindings = collect_app_bindings()

    missing = []
    for path, names in bindings.items():
        for name in names:
            if name not in valid_ids:
                missing.append((path, name))
    for name in collect_ai_led_names():
        if name not in valid_ids:
            missing.append(("components/engine_gui/engine_gui.c (s_ai_led_map)", name))

    if missing:
        print("check_widget_bindings: widget binding mismatch detected",
              file=sys.stderr)
        print("The following widget names are used in WIDGET_BIND but do not exist "
              "in the generated UI code (src/ui/screens.h):", file=sys.stderr)
        for path, name in missing:
            print(f"  {path}: '{name}'", file=sys.stderr)
        print("Fix the binding table, or re-export the EEZ Studio project if the "
              "generated code is stale.", file=sys.stderr)
        sys.exit(1)

    total = sum(len(v) for v in bindings.values())
    print(f"check_widget_bindings: {total} binding(s) verified across "
          f"{len(bindings)} app(s) against generated UI code")
    sys.exit(0)


if __name__ == "__main__":
    main()
