#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""汉化版适配补丁（幂等）。

同步上游（`git checkout -B main <上游tag>`）会把这些文件整体覆盖成官方版本，
导致汉化版特有的适配丢失。本脚本在同步后重新打上这些小补丁；
重复执行不会产生额外改动。

用法：
    python tools/apply-zh-adaptations.py

当前包含的适配：
1. `src/dusk/app_info.hpp`：应用内“检查更新”指向本 fork 的 Releases
   （否则会去查官方仓库，Android 端还会因为签名不同无法覆盖安装）
2. `platforms/android/app/build.gradle`：从 `CMakePresets.json` 的版本号
   推导 versionCode（1.4.1 -> 10401），保证汉化版新版本可直接覆盖安装
"""

import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent.parent

# —— 适配 1：应用内“检查更新”指向本 fork ——
FORK_OWNER = "loeissu"
FORK_REPO = "dusklight"


def patch_app_info() -> None:
    path = ROOT / "src" / "dusk" / "app_info.hpp"
    if not path.exists():
        print("[skip] src/dusk/app_info.hpp 不存在")
        return
    text = path.read_text(encoding="utf-8")
    new = re.sub(
        r'\.githubOwner\s*=\s*"[^"]*"',
        '.githubOwner = "%s"' % FORK_OWNER,
        text,
    )
    new = re.sub(
        r'\.githubRepo\s*=\s*"[^"]*"',
        '.githubRepo = "%s"' % FORK_REPO,
        new,
    )
    if new != text:
        path.write_text(new, encoding="utf-8", newline="\n")
        print("[patched] app_info.hpp -> %s/%s" % (FORK_OWNER, FORK_REPO))
    else:
        print("[ok] app_info.hpp 已指向 %s/%s" % (FORK_OWNER, FORK_REPO))


# —— 适配 2：Android versionCode 从版本号推导 ——
GRADLE_MARKER = "// === 汉化版适配：versionCode 由版本号推导（tools/apply-zh-adaptations.py）==="
GRADLE_BLOCK = """
// === 汉化版适配：versionCode 由版本号推导（tools/apply-zh-adaptations.py）===
// 1.4.1 -> 10401，保证新版本 APK 能覆盖安装旧版汉化版（不需要卸载）。
def zhPresetsFile = new File(duskRepoDir, 'CMakePresets.json')
def zhVersionName = '0.0.0'
if (zhPresetsFile.exists()) {
    def zhMatcher = (zhPresetsFile.text =~ /"BOREALIS_APP_VERSION_OVERRIDE"\\s*:\\s*"([^"]+)"/)
    if (zhMatcher.find()) {
        zhVersionName = zhMatcher.group(1)
    }
}
def zhSegments = zhVersionName.tokenize('.')
while (zhSegments.size() < 3) {
    zhSegments << '0'
}
def zhVersionCode = (zhSegments[0] as int) * 10000 + (zhSegments[1] as int) * 100 + (zhSegments[2] as int)
try {
    android {
        defaultConfig {
            versionName zhVersionName
            versionCode zhVersionCode
        }
    }
    logger.lifecycle("汉化版 versionName=" + zhVersionName + " versionCode=" + zhVersionCode)
} catch (Exception e) {
    logger.lifecycle("汉化版 versionCode 适配失败（继续构建）：" + e.message)
}
"""


def patch_gradle() -> None:
    path = ROOT / "platforms" / "android" / "app" / "build.gradle"
    if not path.exists():
        print("[skip] platforms/android/app/build.gradle 不存在")
        return
    text = path.read_text(encoding="utf-8")
    if GRADLE_MARKER in text:
        print("[ok] build.gradle 已包含 versionCode 适配")
        return
    if not text.endswith("\n"):
        text += "\n"
    path.write_text(text + GRADLE_BLOCK, encoding="utf-8", newline="\n")
    print("[patched] build.gradle -> versionCode 由版本号推导")


def main() -> None:
    print("应用汉化版适配补丁：", ROOT)
    patch_app_info()
    patch_gradle()


if __name__ == "__main__":
    main()
