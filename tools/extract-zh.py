#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从“上游英文版”与“汉化版”的差异中提取英→中对照词典。

用法：
    python tools/extract-zh.py --base <commit> [--translated <commit|worktree>]

默认 --translated 为 worktree（当前工作区）。生成的词典写入 tools/zh_cn.json。
词典同时记录：
  - replace：整行/整块英→中替换（key 为英文原文，value 为中文译文）
  - insert ：纯新增的行（key 为锚点行，value 为要插入的行列表）
"""

import argparse
import difflib
import json
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DICT_PATH = ROOT / "tools" / "zh_cn.json"

# 参与汉化生成的文件（与上游同步后由 apply-zh.py 重新生成）
FILES = [
    "platforms/android/app/src/main/res/values/strings.xml",
    "src/dusk/achievements.cpp",
    "src/dusk/map_loader_definitions.h",
    "src/dusk/mods/loader/loader.cpp",
    "src/dusk/ui/achievements.cpp",
    "src/dusk/ui/bool_button.cpp",
    "src/dusk/ui/controller_config.cpp",
    "src/dusk/ui/editor.cpp",
    "src/dusk/ui/graphics_tuner.cpp",
    "src/dusk/ui/logs_window.cpp",
    "src/dusk/ui/menu_bar.cpp",
    "src/dusk/ui/mods_window.cpp",
    "src/dusk/ui/overlay.cpp",
    "src/dusk/ui/prelaunch.cpp",
    "src/dusk/ui/preset.cpp",
    "src/dusk/ui/reporting.cpp",
    "src/dusk/ui/settings.cpp",
    "src/dusk/ui/touch_controls_editor.cpp",
    "src/dusk/ui/ui.cpp",
    "src/dusk/ui/warp.cpp",
]


def has_cjk(s: str) -> bool:
    return any(
        ("\u4e00" <= ch <= "\u9fff") or ("\u3000" <= ch <= "\u303f")
        or ("\uff00" <= ch <= "\uffef")
        for ch in s
    )


def git_show(ref: str, path: str) -> str:
    proc = subprocess.run(
        ["git", "-C", str(ROOT), "show", f"{ref}:{path}"],
        capture_output=True,
        encoding="utf-8",
        errors="replace",
    )
    if proc.returncode != 0:
        return ""
    return proc.stdout


def extract_file(base_text: str, new_text: str):
    base_lines = base_text.splitlines()
    new_lines = new_text.splitlines()
    matcher = difflib.SequenceMatcher(None, base_lines, new_lines)
    replaces = []
    inserts = []
    for tag, i1, i2, j1, j2 in matcher.get_opcodes():
        if tag == "replace":
            key = "\n".join(base_lines[i1:i2])
            value = "\n".join(new_lines[j1:j2])
            if key and value and key != value and has_cjk(value):
                replaces.append({"key": key, "value": value})
        elif tag == "insert":
            # 纯新增行：以新增块前一行（上游也有的行）作为锚点
            anchor = base_lines[i1 - 1] if i1 > 0 else None
            value = "\n".join(new_lines[j1:j2])
            if anchor is not None and has_cjk(value):
                inserts.append({"key": anchor, "value": value})
    return replaces, inserts


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", required=True, help="上游基准提交（英文版）")
    parser.add_argument(
        "--translated",
        default="worktree",
        help="汉化版本：提交号或 worktree（默认 worktree）",
    )
    args = parser.parse_args()

    files_out = {}
    for rel in FILES:
        base_text = git_show(args.base, rel)
        if not base_text:
            print(f"[skip] {rel}: 基准中不存在", file=sys.stderr)
            continue
        if args.translated == "worktree":
            new_text = (ROOT / rel).read_text(encoding="utf-8")
        else:
            new_text = git_show(args.translated, rel)
        replaces, inserts = extract_file(base_text, new_text)
        if replaces or inserts:
            files_out[rel] = {"replace": replaces, "insert": inserts}
            print(f"{rel}: {len(replaces)} replace, {len(inserts)} insert")

    payload = {
        "format": 1,
        "base": args.base,
        "files": files_out,
    }
    DICT_PATH.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    print(f"wrote {DICT_PATH}")


if __name__ == "__main__":
    main()
