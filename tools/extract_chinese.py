#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
中文词条提取工具

扫描项目代码与文档文件，提取其中所有中文字符（CJK 统一表意文字），
去重后输出为字体生成工具可用的语料文件。

用法：
    python tools/extract_chinese.py
    python tools/extract_chinese.py --dirs components/main doc --output assets/chars.txt
    python tools/extract_chinese.py --strings-only
"""

import argparse
import json
import os
import re
import sys
from pathlib import Path
from typing import List, Set, Tuple

# 默认扫描目录（相对于项目根目录）
DEFAULT_DIRS = ["components", "main"]

# 默认扫描的文件扩展名
DEFAULT_EXTENSIONS = {".c", ".cpp", ".h", ".hpp", ".eez-project"}

# 需要排除的目录名
EXCLUDE_DIRS = {
    "build",
    "managed_components",
    ".git",
    ".vscode",
    ".devcontainer",
    "__pycache__",
    "M5Tab5-UserDemo",   # 官方 Demo，非本项目代码
    "src",               # engine_gui 下的 EEZ 生成代码通常不含业务中文
}

# 中文字符 Unicode 范围：CJK 统一表意文字
CHINESE_RE = re.compile(r"[\u4e00-\u9fff]")

# C/C++ 字符串字面量匹配（简化版，支持 "..." 和 '...'）
STRING_RE = re.compile(r'"(?:[^"\\]|\\.)*"|\'(?:[^\'\\]|\\.)*\'')


def is_excluded(path: Path) -> bool:
    """判断路径是否在被排除的目录中"""
    for part in path.parts:
        if part in EXCLUDE_DIRS:
            return True
    return False


def find_source_files(root: Path, dirs: List[str], extensions: Set[str]) -> List[Path]:
    """在指定目录下查找符合条件的源文件"""
    files: List[Path] = []
    for d in dirs:
        base = root / d
        if not base.exists():
            print(f"警告：目录不存在，跳过: {base}")
            continue
        for path in base.rglob("*"):
            if not path.is_file():
                continue
            if path.suffix.lower() not in extensions:
                continue
            if is_excluded(path):
                continue
            files.append(path)
    return sorted(files)


def extract_chinese_from_text(text: str, strings_only: bool) -> List[str]:
    """从文本中提取中文字符"""
    if strings_only:
        # 只扫描字符串字面量
        chars: List[str] = []
        for match in STRING_RE.finditer(text):
            chars.extend(CHINESE_RE.findall(match.group()))
        return chars
    else:
        # 扫描全部文本（注释、字符串、标识符等）
        return CHINESE_RE.findall(text)


def extract_chinese(
    root: Path, dirs: List[str], extensions: Set[str], strings_only: bool
) -> Tuple[List[str], int, List[Tuple[str, int]]]:
    """
    提取中文字符

    返回：
        ordered_chars: 按首次出现顺序排列的去重字符列表
        total_chars: 扫描到的中文总字符数（含重复）
        file_counts: 每个文件提取到的字符数列表
    """
    files = find_source_files(root, dirs, extensions)
    seen: Set[str] = set()
    ordered: List[str] = []
    total = 0
    file_counts: List[Tuple[str, int]] = []

    for f in files:
        try:
            text = f.read_text(encoding="utf-8", errors="ignore")
        except Exception as e:
            print(f"读取失败: {f} - {e}")
            continue

        chars = extract_chinese_from_text(text, strings_only)
        unique_in_file = set(chars)
        for ch in chars:
            total += 1
            if ch not in seen:
                seen.add(ch)
                ordered.append(ch)

        file_counts.append((str(f.relative_to(root)), len(unique_in_file)))

    return ordered, total, file_counts


def main() -> int:
    parser = argparse.ArgumentParser(
        description="从项目代码和文档中提取中文字符，用于生成字体包"
    )
    parser.add_argument(
        "--dirs",
        nargs="+",
        default=DEFAULT_DIRS,
        help=f"扫描目录，默认: {DEFAULT_DIRS}",
    )
    parser.add_argument(
        "--include-doc",
        action="store_true",
        help="同时扫描 doc/ 目录中的 Markdown 文档",
    )
    parser.add_argument(
        "--output",
        default="tools/chinese_chars.txt",
        help="输出语料文件路径，默认: tools/chinese_chars.txt",
    )
    parser.add_argument(
        "--report",
        default="tools/chinese_chars_report.json",
        help="输出统计报告路径，默认: tools/chinese_chars_report.json",
    )
    parser.add_argument(
        "--strings-only",
        action="store_true",
        help="仅提取字符串字面量中的中文",
    )
    parser.add_argument(
        "--sort",
        action="store_true",
        help="按 Unicode 码点排序输出",
    )
    parser.add_argument(
        "--exts",
        nargs="+",
        default=None,
        help=f"指定扫描扩展名，默认: {sorted(DEFAULT_EXTENSIONS)}",
    )
    args = parser.parse_args()

    root = Path(__file__).resolve().parent.parent
    dirs = list(args.dirs)
    if args.include_doc and "doc" not in dirs:
        dirs.append("doc")

    extensions = set(args.exts) if args.exts else DEFAULT_EXTENSIONS
    extensions = {e.lower() if e.startswith(".") else f".{e.lower()}" for e in extensions}

    ordered, total, file_counts = extract_chinese(
        root, dirs, extensions, args.strings_only
    )

    if args.sort:
        ordered = sorted(set(ordered))

    output_path = root / args.output
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("".join(ordered), encoding="utf-8")

    report_path = root / args.report
    report = {
        "total_scanned_chars": total,
        "unique_chars": len(ordered),
        "output_file": args.output,
        "strings_only": args.strings_only,
        "sorted": args.sort,
        "scan_dirs": args.dirs,
        "scan_extensions": sorted(extensions),
        "top_files": sorted(file_counts, key=lambda x: x[1], reverse=True)[:20],
        "chars": "".join(ordered),
    }
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")

    print(f"扫描完成：")
    print(f"  文件数: {len(file_counts)}")
    print(f"  中文字符总数（含重复）: {total}")
    print(f"  去重后字符数: {len(ordered)}")
    print(f"  语料文件: {output_path}")
    print(f"  报告文件: {report_path}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
