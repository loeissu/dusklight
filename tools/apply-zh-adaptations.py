#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把汉化适配以幂等方式合并进当前工作区（上游同步后调用）。

不整文件覆盖上游源码，只做小补丁：
- 中文字体 fallback
- 更新检查指向本 fork
- Android versionCode 自动推导
- 窄屏/长文本 CSS
- 说明区触摸滚动（若尚未存在）
"""

from __future__ import annotations

import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent.parent


def patch_file(rel: str, patches: list[tuple[str, str]]) -> None:
    path = ROOT / rel
    if not path.exists():
        print(f"[skip] {rel}: 文件不存在")
        return
    text = path.read_text(encoding="utf-8")
    original = text
    for old, new in patches:
        if new in text:
            continue
        if old not in text:
            print(f"[miss] {rel}: 未找到锚点 -> {old[:60]!r}")
            continue
        text = text.replace(old, new, 1)
    if text != original:
        path.write_text(text, encoding="utf-8", newline="\n")
        print(f"[updated] {rel}")
    else:
        print(f"[ok] {rel}: 无需修改")


def main() -> None:
    patch_file(
        "src/dusk/app_info.hpp",
        [
            (
                '.githubOwner = "TwilitRealm",\n        .githubRepo = "dusklight",',
                '// 汉化版：更新检查指向本 fork 的 Releases\n'
                '        .githubOwner = "loeissu",\n'
                '        .githubRepo = "dusklight",',
            )
        ],
    )

    patch_file(
        "src/dusk/ui/ui.cpp",
        [
            (
                'load_font("NotoMono-Regular.ttf");',
                'load_font("NotoMono-Regular.ttf");\n'
                "    // 中文字体 fallback：界面汉化后，Fira Sans 等字体缺少的汉字由此字体渲染。\n"
                '    load_font("NotoSansCJKsc-Regular.otf", true);',
            )
        ],
    )

    # Android versionCode：若尚无 zhVersionCode 块则追加
    gradle = ROOT / "platforms/android/app/build.gradle"
    if gradle.exists():
        gtext = gradle.read_text(encoding="utf-8")
        if "zhVersionCode" not in gtext:
            gtext = gtext.replace(
                "dependencies {\n    implementation fileTree(dir: 'libs', include: ['*.jar'])\n}",
                """// 汉化版：从版本号推导 Android versionCode，保证可覆盖安装
def zhVersionProps = new Properties()
file(ext.borealisAndroid.propertiesFile).withInputStream { zhVersionProps.load(it) }
def zhVersionName = (zhVersionProps.getProperty('borealis.versionName') ?: '0.0.0').replaceFirst('^v', '')
def zhVersionParts = zhVersionName.split('\\\\.')
def zhVersionCode = 1
if (zhVersionParts.length >= 3) {
    zhVersionCode = zhVersionParts[0].toInteger() * 10000 + zhVersionParts[1].toInteger() * 100 + zhVersionParts[2].toInteger()
}
android {
    defaultConfig {
        versionCode zhVersionCode
    }
}

dependencies {
    implementation fileTree(dir: 'libs', include: ['*.jar'])
}""",
            )
            gradle.write_text(gtext, encoding="utf-8", newline="\n")
            print("[updated] platforms/android/app/build.gradle")
        else:
            print("[ok] platforms/android/app/build.gradle")

    # window.rcss 窄屏/长文本适配
    rcss = ROOT / "res/rml/window.rcss"
    if rcss.exists():
        rtext = rcss.read_text(encoding="utf-8")
        if "word-break: break-word" not in rtext:
            rtext += """

/* 汉化：说明/详情文本按边框宽度自动换行 + 弹窗可滚动 */
window content pane div {
    width: 100%;
    word-break: break-word;
}

window.small,
window.modal {
    max-height: 100%;
    overflow: hidden auto;
}

@media (max-width: 720dp) {
    body { padding: 8dp; }
    window {
        max-width: none;
        max-height: none;
        border-radius: 0;
        border-left: none;
        border-right: none;
    }
    window.small,
    window.modal {
        width: 100%;
        max-height: 100%;
        overflow: hidden auto;
    }
    window content { flex-flow: column; }
    window content pane { padding: 16dp; font-size: 16dp; }
    window content pane:not(:last-of-type) {
        border-right: none;
        border-bottom: 1dp var(--color-border);
    }
}
"""
            rcss.write_text(rtext, encoding="utf-8", newline="\n")
            print("[updated] res/rml/window.rcss")
        else:
            print("[ok] res/rml/window.rcss")

    # input.cpp 触摸滚动：若尚未包含则提示（代码补丁复杂，需人工合入）
    input_cpp = ROOT / "src/dusk/ui/input.cpp"
    if input_cpp.exists():
        itext = input_cpp.read_text(encoding="utf-8")
        if "handle_touch_scroll_motion" not in itext:
            print("[warn] src/dusk/ui/input.cpp: 缺少触摸滚动实现，需人工合入适配")
        else:
            print("[ok] src/dusk/ui/input.cpp")

    print("适配补丁完成")


if __name__ == "__main__":
    main()
