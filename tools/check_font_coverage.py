#!/usr/bin/env python3
"""检查渲染文案用到的字是否都在对应字库里 —— 防止真机出现方块字。

背景（这个坑已经踩过一次）
    字库是按"实际用到的字符"裁剪的，分五档：
        kai_48 / kai_34  → 只给欢迎页大标题 / 次要大字
        sans_18          → 正文
        sans_16 / sans_14→ 提示条与次要说明，**字符集小得多**
    如果一个字符串用 16px 渲染，但它的字只收进了 18px 那一档，真机就会显示
    方块 —— 而模拟器用 FreeType 动态加载，完全看不出来。
    （旧项目实测翻过车：某页文案从 18px 改成 16px 后，白话里的字不在 16px
      字库里，真机变方块。）

做法
    取出 16px 字库的字符集（它是两档小字库的上界），再把 ui/*.c、ui/*.h 与
    algo/*.c 里所有字符串字面量逐个比对，列出**不在其中**的字符串。
    列出来的每一条，都必须人工确认它只在 18px 及以上渲染 —— 否则就是方块。

    为什么连 algo/ 也要扫：算法的输出就是界面要显示的字（颜色词、五行名），
    它们定义在 algo/dressing.c，界面拿返回值渲染。

用法
    python3 tools/check_font_coverage.py
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
UI = ROOT / "ui"
SMALL_FONTS = ["lv_font_sans_16", "lv_font_sans_14"]


def charset_of(stem: str) -> set:
    """从生成的字库文件头注释里取回当时的 --symbols 字符集。"""
    p = UI / "fonts" / f"{stem}.c"
    if not p.is_file():
        sys.exit(f"找不到字库文件 {p}（先跑 tools/gen_fonts.py）")
    m = re.search(r"--symbols\s+(\S+)", p.read_text(encoding="utf-8"))
    if not m:
        sys.exit(f"{p} 里没有 --symbols 记录，无法判定字符集")
    return set(m.group(1))


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    return text


def strings_in(path: pathlib.Path):
    """源码里的字符串字面量（已去注释）。"""
    src = strip_comments(path.read_text(encoding="utf-8"))
    return re.findall(r'"((?:[^"\\]|\\.)*)"', src)


def main() -> int:
    small = charset_of(SMALL_FONTS[0])
    for f in SMALL_FONTS[1:]:
        assert charset_of(f) == small, f"{f} 的字符集与 {SMALL_FONTS[0]} 不一致"

    files = sorted(UI.glob("*.c")) + sorted(UI.glob("*.h")) + sorted((ROOT / "algo").glob("*.c"))
    risky = []
    for p in files:
        for s in strings_in(p):
            if not s.strip():
                continue
            missing = {c for c in s if c not in small and c not in " \n\r\t"}
            if missing:
                risky.append((p.name, s, "".join(sorted(missing))))

    print(f"16/14px 字库字符集：{len(small)} 字")
    print(f"扫描 {len(files)} 个源文件，以下文案含有小字库没有的字：\n")
    for name, s, miss in risky:
        print(f"  {name:<18} {s[:34]:<36} 缺: {miss}")
    print(f"\n共 {len(risky)} 条。")
    print("→ 每一条都必须确认【只在大字号（18px 及以上）渲染】。")
    print("  只要有一条会被 16px/14px 画出来，真机就是方块字。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
