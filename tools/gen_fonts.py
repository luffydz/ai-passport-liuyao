#!/usr/bin/env python3
"""按「实际用到的字符」生成 LVGL v9 静态字库（子集），供固件编进 flash。

为什么必须做子集：
  真机没有 SD 卡、没有 PSRAM，且 LVGL 自带的 Montserrat 没有任何中文字形
  （实测 lv_font_montserrat_20.c 里 CJK 命中数 = 0），所以中文只能编进 flash。
  整套中文字库太大，但本项目实际用到的字很有限 —— 按用量裁剪后只有几百 KB。

字符集怎么来的（不靠手工罗列，避免漏字导致真机出现方块）：
  - UI 文案   ：扫描 ui/*.c、ui/*.h 里出现的所有可显示字符（hexagram.c 除外，单独处理）
  - 算法文案  ：扫描 algo/*.c —— 每日穿衣的颜色词、五行名、干支名都在那里，界面拿返回值渲染
  - 卦理数据  ：扫描 ui/hexagram.c（64 卦名/卦辞/爻辞/白话/建议/装卦表）
  - 卦名单独取：只把 HEX_NAME 表里的字给楷体大字号（kai_34）用
  - 运行时串  ：hexagram.c 里以函数返回值形式出现的字（六亲/卦宫/纳甲/六神等）

字号分档与用途见下表；改文案后重跑本脚本即可。
"""
import argparse
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

FONTS = {
    "kai":  ROOT / "assets/fonts/ZiHunTaiAKaiShu.ttf",              # 标题用楷书
    "sans": ROOT / "assets/fonts/SourceHanSansCN-Medium.otf",       # 正文用思源黑体
}
OUT_DIR = ROOT / "ui" / "fonts"

# 规划：(输出名, 字体, 字号, bpp, 字符集键)
PLAN = [
    ("lv_font_kai_48",  "kai",  48, 4, "hero"),
    ("lv_font_kai_34",  "kai",  34, 4, "names"),
    ("lv_font_sans_18", "sans", 18, 4, "body"),
    ("lv_font_sans_16", "sans", 16, 4, "ui"),
    ("lv_font_sans_14", "sans", 14, 4, "ui"),
]


def is_allowed(ch: str) -> bool:
    """是否收进字符集。

    刻意不手工划分 Unicode 区间 —— 曾经因为只覆盖 CJK/全角/ASCII，
    漏掉了提示文案里的间隔号「·」(U+00B7)，导致真机出现方块字。
    字符集来源就是源码文本本身，所以"只要是可打印字符就收"才是安全的。
    """
    if ch == " ":
        return True
    if ch in "\n\r\t":
        return False
    return ch.isprintable()


def strip_comments(text: str) -> str:
    """去掉注释 —— 注释里的汉字永远不会被渲染，不该进字库"""
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    return text


def chars_of(text: str) -> set:
    return {c for c in text if is_allowed(c)}


def read(p: pathlib.Path) -> str:
    return p.read_text(encoding="utf-8")


def read_code(p: pathlib.Path) -> str:
    """读源码并去注释（用于收集字符集）"""
    return strip_comments(read(p))


def collect_ui_chars() -> set:
    """ui/ 下除 hexagram.c 以外的所有源文件 + algo/ 全部源文件。

    - hexagram.c 单独用下方收集器处理（卦名/卦辞/爻辞/装卦表，避免与 UI 字符混在一起）；
    - algo/ 全扫：每日穿衣的颜色词、五行名、干支名都定义在 algo/dressing.c，
      界面是拿函数返回值拼文案渲染的，漏了就是真机方块字。
    这个集合同时用于 kai_34（names）：保证「六爻安静」这类用楷体大字号渲染的
    UI 字符串也不会出方块（见 view_liuyao.c 结果页的「六爻安静」）。
    """
    out = set()
    for pat in ("ui/*.c", "ui/*.h", "algo/*.c", "algo/*.h"):
        for p in sorted(ROOT.glob(pat)):
            if p.name == "hexagram.c":
                continue
            out |= chars_of(read_code(p))
    return out


def collect_data_chars() -> set:
    """hexagram.c 的全部字面量（64 卦名/卦辞/爻辞/白话/建议/装卦表）。"""
    return chars_of(read_code(ROOT / "ui" / "hexagram.c"))


def collect_hexname_chars() -> set:
    """只取 HEX_NAME 表（64 卦名）里的字 → 给楷体大字号 kai_34 用。"""
    src = read_code(ROOT / "ui" / "hexagram.c")
    m = re.search(r"const char HEX_NAME\[8\]\[8\]\[16\]\s*=\s*\{(.*?)\n\};", src, re.S)
    if not m:
        sys.exit("找不到 HEX_NAME 表，hexagram.c 结构变了？")
    return chars_of(m.group(1))


def collect_runtime_strings() -> set:
    """收集【运行时才产生】的文字。

    这些字不在 ui/*.c 的字符串字面量里，而是通过函数返回值拼进文案，例如
    getCategoryName() 返回的「事业前程」，用小字号渲染在摇卦页顶部。
    漏掉它们就会出现真机方块字 —— 而且模拟器用 FreeType 看不出来。
    """
    src = read_code(ROOT / "ui" / "hexagram.c")
    out = set()
    for pat in [
        r"CATEGORY_NAMES\[CATEGORY_COUNT\]\s*=\s*\{(.*?)\};",
        r'TRIGRAM_NAMES\[8\]\s*=\s*\{(.*?)\};',
        r'LINE_POS_NAMES\[6\]\s*=\s*\{(.*?)\};',
        r'YANG_STR\[\]\s*=\s*"(.*?)"',
        r'YIN_STR\[\]\s*=\s*"(.*?)"',
        # 断卦页扩充维度（16px 也渲染它们，所以必须收进 ui 字库）
        r'LINE_POS_MEANING\[6\]\s*=\s*\{(.*?)\};',
        r'ELEM_RELATION_TEXT\[5\]\s*=\s*\{(.*?)\};',
        # 装卦层：卦盘页会用 16px 渲染天干地支、六亲、卦宫名
        r'NAJIA_GAN_IN\[8\]\s*=\s*\{(.*?)\};',
        r'NAJIA_GAN_OUT\[8\]\s*=\s*\{(.*?)\};',
        r'NAJIA_ZHI_IN\[8\]\[3\]\s*=\s*\{(.*?)\};',
        r'NAJIA_ZHI_OUT\[8\]\[3\]\s*=\s*\{(.*?)\};',
        r'LIUQIN_NAME\[LIUQIN_COUNT\]\s*=\s*\{(.*?)\};',
        r'PALACE_NAME\[8\]\s*=\s*\{(.*?)\};',
    ]:
        m = re.search(pat, src, re.S)
        if not m:
            sys.exit(f"运行时字符串提取失败（hexagram.c 结构变了？）: {pat}")
        out |= chars_of(m.group(1))
    return out


def build_sets() -> dict:
    ui = collect_ui_chars()
    data = collect_data_chars()
    names = collect_hexname_chars()
    runtime = collect_runtime_strings()
    misc = chars_of("上下动")                     # 拼接用的连接字
    digits = chars_of("0123456789")

    # hero（kai_48）：六爻首页「六  爻」+ 每日穿衣首页「每日穿衣」
    hero = chars_of("六爻每日穿衣 ")

    return {
        "hero":  hero,
        # names（kai_34）：卦名 + 运行时串 + 全部 UI 字符 + 拼接字 + 数字。
        #   并上 ui 是为了"六爻安静"等用楷体大字号渲染的 UI 串不出方块。
        "names": names | runtime | ui | misc | digits,
        "body":  data | ui | runtime | misc | digits,
        "ui":    ui | runtime | misc | digits,
    }


def fmt_chars(cs: set) -> str:
    # 按 codepoint 排序，保证每次生成结果一致（可复现）
    return "".join(sorted(cs, key=ord))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true", help="只打印字符集统计，不生成")
    args = ap.parse_args()

    sets = build_sets()
    for k, v in sets.items():
        cjk = sum(1 for c in v if ord(c) > 0x2E80)
        print(f"字符集 {k:<6} 共 {len(v):>4} 字（其中汉字/全角 {cjk}）")

    if args.dry_run:
        return 0

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    print()
    total = 0
    for name, fam, size, bpp, key in PLAN:
        chars = sets[key]
        out = OUT_DIR / f"{name}.c"
        cmd = [
            "lv_font_conv",
            "--font", str(FONTS[fam]),
            "--size", str(size),
            "--bpp", str(bpp),
            "--format", "lvgl",
            "--no-compress",
            "--lv-font-name", name,
            "--lv-include", "lvgl.h",
            "--force-fast-kern-format",
            # 只给实际用到的字符：字符集里已经包含需要的 ASCII（来自字符串字面量
            # 和显式加入的数字），所以不要再无条件加整段 ASCII —— 那会白白多出几十 KB
            "--symbols", fmt_chars(chars),
            "-o", str(out),
        ]
        print(f"生成 {name}: {size}px/{bpp}bpp ← {len(chars)} 字")
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stdout[-2000:])
            print(r.stderr[-2000:], file=sys.stderr)
            return 1
        kb = out.stat().st_size / 1024
        total += kb
        print(f"        -> {out.relative_to(ROOT)}  {kb:.0f} KB")

    print(f"\n五档合计（C 源码体积，编译进 flash 后为二进制像素数据，更小）：{total:.0f} KB")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
