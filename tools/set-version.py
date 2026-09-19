#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把汉化版构建版本号写入 CMakePresets.json 的三个 CI 预设。

用法：
    python tools/set-version.py 1.4.2

同步上游正式版后由 sync-upstream.yml 自动调用，保证汉化版版本号与
上游正式版一致。只修改 x-android-ci / x-linux-ci / x-windows-ci 三个
预设里的 BOREALIS_APP_VERSION_OVERRIDE，不重排文件其余部分。
"""

import re
import sys
from pathlib import Path

PRESETS = ("x-android-ci", "x-linux-ci", "x-windows-ci")
KEY = "BOREALIS_APP_VERSION_OVERRIDE"
ROOT = Path(__file__).resolve().parent.parent


def main() -> int:
    if len(sys.argv) != 2:
        print("用法: python tools/set-version.py <版本号，如 1.4.2>")
        return 2
    version = sys.argv[1].lstrip("v")
    path = ROOT / "CMakePresets.json"
    lines = path.read_text(encoding="utf-8").splitlines()

    current_preset = None
    changed = 0
    for i, line in enumerate(lines):
        m = re.search(r'"name"\s*:\s*"(' + "|".join(PRESETS) + r')"', line)
        if m:
            current_preset = m.group(1)
            continue
        if current_preset and '"cacheVariables"' in line and "{" in line:
            # 找到该预设的 cacheVariables 块
            indent = line[: len(line) - len(line.lstrip())]
            child_indent = indent + "  "
            override_line = child_indent + f'"{KEY}": "{version}",'
            # 检查块内是否已有 override
            j = i + 1
            replaced = False
            while j < len(lines) and "}" not in lines[j]:
                if KEY in lines[j]:
                    lines[j] = re.sub(
                        r':\s*"[^"]*"', f': "{version}"', lines[j], count=1
                    )
                    replaced = True
                    break
                j += 1
            if not replaced:
                lines.insert(i + 1, override_line)
            changed += 1
            current_preset = None

    if changed != len(PRESETS):
        print(f"警告：只更新了 {changed}/{len(PRESETS)} 个预设，请检查 CMakePresets.json")
        return 1

    path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    print(f"已将 {len(PRESETS)} 个 CI 预设的版本号更新为 {version}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
