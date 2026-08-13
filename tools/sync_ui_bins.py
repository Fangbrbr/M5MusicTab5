#!/usr/bin/env python3
"""
自动扫描 engine_gui/src/ui 下的 .bin 文件，同步到：
1. CMakeLists.txt 的 EMBED_FILES（所有 .bin 都内嵌进固件）
2. engine_gui_res_vfs.c 的内嵌资源表（所有 .bin）
3. prepare_spiffs.py 的 EMBEDDED_RESOURCES 集合（所有 .bin）

用法：python tools/sync_ui_bins.py
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UI_DIR = os.path.join(ROOT, "components", "engine_gui", "src", "ui")
CMAKE_FILE = os.path.join(ROOT, "components", "engine_gui", "CMakeLists.txt")
RES_VFS_FILE = os.path.join(ROOT, "components", "engine_gui", "src", "engine_gui_res_vfs.c")
SPIFFS_PY = os.path.join(ROOT, "tools", "prepare_spiffs.py")


def get_bin_files():
    """扫描 src/ui 下所有 .bin 文件，返回排序后的相对路径列表"""
    if not os.path.isdir(UI_DIR):
        print(f"ERROR: UI directory not found: {UI_DIR}")
        sys.exit(1)
    bins = []
    for f in sorted(os.listdir(UI_DIR)):
        if f.endswith(".bin"):
            bins.append(f"src/ui/{f}")
    return bins


def path_to_symbol(path):
    """将 src/ui/ui_font_chinese_30.bin 转换为 ui_font_chinese_30"""
    return os.path.basename(path).replace(".bin", "")


def update_cmake(bins):
    """更新 CMakeLists.txt 的 EMBED_FILES 段（所有 bin）"""
    if not bins:
        print("WARNING: no bin files found")
        return True

    with open(CMAKE_FILE, "r", encoding="utf-8") as f:
        content = f.read()

    # 匹配 EMBED_FILES 段：首行 EMBED_FILES "first.bin"，后续每行一个 .bin
    pattern = r'(EMBED_FILES\s+"[^"]*\.bin")(\s*\n)((?:\s+"[^"]*\.bin"\s*\n)*)'
    m = re.search(pattern, content)
    if not m:
        print("ERROR: EMBED_FILES section not found in CMakeLists.txt")
        return False

    indent = "                                  "
    first_line = m.group(1)
    rest_block = "".join(f'{indent}"{b}"\n' for b in bins[1:])
    new_content = content[:m.start(1)] + first_line + m.group(2) + rest_block + content[m.end(3):]

    with open(CMAKE_FILE, "w", encoding="utf-8") as f:
        f.write(new_content)

    print(f"[CMakeLists.txt] EMBED_FILES updated: {len(bins)} bins")
    return True


def update_res_vfs(bins):
    """更新 engine_gui_res_vfs.c 中的 extern 声明和内嵌表（所有 bin）"""
    if not bins:
        return

    with open(RES_VFS_FILE, "r", encoding="utf-8") as f:
        content = f.read()

    # 1. 更新 extern 声明块
    extern_start = content.find("/* 字体资源经 EMBED_FILES 内嵌进固件")
    if extern_start >= 0:
        # 找到 extern 块的结束（第一个 _binary_..._end 后的分号）
        extern_end_pos = content.find("_binary_", extern_start)
        if extern_end_pos >= 0:
            # 找到这行的行尾
            line_end = content.find(";", extern_end_pos)
            if line_end >= 0:
                # 继续找所有连续的 extern 行
                pos = line_end + 1
                while True:
                    next_line = content.find("\nextern const uint8_t _binary_", pos)
                    if next_line == pos:
                        line_end = content.find(";", next_line)
                        if line_end >= 0:
                            pos = line_end + 1
                        else:
                            break
                    else:
                        break
                extern_end = pos

                extern_block = "\n".join(
                    f"extern const uint8_t _binary_{path_to_symbol(b)}_bin_start[];\n"
                    f"extern const uint8_t _binary_{path_to_symbol(b)}_bin_end[];"
                    for b in bins
                )
                content = content[:extern_start] + extern_block + "\n\n" + content[extern_end:]

    # 2. 更新 s_embedded 表
    table_pattern = r'(static const res_embedded_t s_embedded\[\] = \{)(.*?)(\n\};)'
    m = re.search(table_pattern, content, re.DOTALL)
    if m:
        rows = "".join(
            f'    {{ "/{path_to_symbol(b)}.bin", _binary_{path_to_symbol(b)}_bin_start, _binary_{path_to_symbol(b)}_bin_end }},\n'
            for b in bins
        )
        content = content[:m.start(2)] + "\n" + rows + content[m.end(2):]

    with open(RES_VFS_FILE, "w", encoding="utf-8") as f:
        f.write(content)

    print(f"[engine_gui_res_vfs.c] embedded table updated: {len(bins)} bins")


def update_prepare_spiffs(bins):
    """更新 prepare_spiffs.py 的 EMBEDDED_RESOURCES 集合（所有 bin）"""
    if not bins:
        return

    with open(SPIFFS_PY, "r", encoding="utf-8") as f:
        content = f.read()

    # 匹配 EMBEDDED_RESOURCES = { ... }
    pattern = r'(EMBEDDED_RESOURCES\s*=\s*\{)(.*?)(\})'
    m = re.search(pattern, content, re.DOTALL)
    if not m:
        print("WARNING: EMBEDDED_RESOURCES not found in prepare_spiffs.py")
        return

    entries = "".join(f'    "{path_to_symbol(b)}.bin",\n' for b in bins)
    new_content = content[:m.start(2)] + "\n" + entries + content[m.end(2):]

    with open(SPIFFS_PY, "w", encoding="utf-8") as f:
        f.write(content)

    print(f"[prepare_spiffs.py] EMBEDDED_RESOURCES updated: {len(bins)} bins")


def main():
    bins = get_bin_files()
    if not bins:
        print("WARNING: no .bin files found in", UI_DIR)
        sys.exit(0)

    print(f"Found {len(bins)} bin files:")
    for b in bins:
        print(f"  - {b}")

    ok = update_cmake(bins)
    if ok:
        update_res_vfs(bins)
        update_prepare_spiffs(bins)
        print("\nDone. Run 'idf.py fullclean' and rebuild.")
    else:
        sys.exit(1)


if __name__ == "__main__":
    main()
