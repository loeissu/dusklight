#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把 tools/zh_cn.json 词典应用到当前工作区，生成汉化文件。

用法：
    python tools/apply-zh.py            # 应用并写回文件
    python tools/apply-zh.py --check    # 只检查：列出未匹配的英文条目

用途：同步上游后，上游的新版英文源码 + 词典 = 最新汉化文件。
词典中没有对应翻译的英文会原样保留（界面显示英文，便于发现漏翻）。
"""

import argparse
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DICT_PATH = ROOT / "tools" / "zh_cn.json"


def apply_file(rel: str, spec: dict, dry_run: bool):
    path = ROOT / rel
    if not path.exists():
        print(f"[skip] {rel}: 文件不存在（上游可能已删除该文件）")
        return
    text = path.read_text(encoding="utf-8")
    original = text
    missed = []

    for entry in sorted(spec.get("replace", []), key=lambda e: -len(e["key"])):
        key, value = entry["key"], entry["value"]
        if key in text:
            text = text.replace(key, value)
        else:
            missed.append(key.splitlines()[0][:80])

    for entry in spec.get("insert", []):
        anchor, value = entry["key"], entry["value"]
        if anchor in text and value not in text:
            text = text.replace(anchor, anchor + "\n" + value, 1)
        elif anchor not in text:
            missed.append("anchor: " + anchor[:80])

    if text != original:
        if dry_run:
            print(f"[diff] {rel}")
        else:
            path.write_text(text, encoding="utf-8", newline="\n")
            print(f"[updated] {rel}")
    if missed:
        print(f"[missed {rel}]")
        for m in missed:
            print("   ", m)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="只检查不写回")
    args = parser.parse_args()

    payload = json.loads(DICT_PATH.read_text(encoding="utf-8"))
    print(f"词典基准: {payload.get('base')}")
    for rel, spec in payload["files"].items():
        apply_file(rel, spec, dry_run=args.check)


if __name__ == "__main__":
    main()
