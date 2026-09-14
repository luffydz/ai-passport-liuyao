#!/usr/bin/env python3
"""生成"十二节"表（六爻取月建用），输出 C 头文件供固件查表。

为什么需要
    六爻的月建既不是公历月、也不是农历月，而是【节气月】：
    立春起寅、惊蛰起卯、清明起辰…… 所以必须知道某个公历日期落在哪个"节"之后。

为什么离线预生成
    固件端算节气要浮点天文学（迭代求太阳视黄经），对一个 240×320 的小设备
    是纯浪费。这里离线算好，固件只做一次查表比较。

算法
    Meeus《Astronomical Algorithms》第 25 章：太阳视黄经低精度公式。
    精度约 0.01°（对应时间约 15 分钟），判"落在哪一天"绰绰有余。
    时间用北京时（UTC+8）—— 节气属于中国历法，必须用东八区日期。

用法
    python3 tools/gen_solar_terms.py            # 生成 ui/solar_terms.h
    python3 tools/gen_solar_terms.py --check    # 只打印几个年份供人工核对
"""
import argparse
import math
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "ui" / "solar_terms.h"

YEAR_FROM, YEAR_TO = 2020, 2040

# 十二"节"：黄经 → 月建地支。按公历年内的先后排序（小寒在年初）。
# (黄经, 月支)
JIE = [
    (285.0, "丑"),  # 小寒（1月）
    (315.0, "寅"),  # 立春（2月）
    (345.0, "卯"),  # 惊蛰（3月）
    ( 15.0, "辰"),  # 清明（4月）
    ( 45.0, "巳"),  # 立夏（5月）
    ( 75.0, "午"),  # 芒种（6月）
    (105.0, "未"),  # 小暑（7月）
    (135.0, "申"),  # 立秋（8月）
    (165.0, "酉"),  # 白露（9月）
    (195.0, "戌"),  # 寒露（10月）
    (225.0, "亥"),  # 立冬（11月）
    (255.0, "子"),  # 大雪（12月）
]

ZHI_ORDER = ["子", "丑", "寅", "卯", "辰", "巳", "午", "未", "申", "酉", "戌", "亥"]


# ---------------------------------------------------------------------------
# 儒略日 <-> 公历（Meeus 第 7 章）
# ---------------------------------------------------------------------------
def jd_from_gregorian(y: int, m: int, d: float) -> float:
    if m <= 2:
        y -= 1
        m += 12
    a = y // 100
    b = 2 - a + a // 4
    return math.floor(365.25 * (y + 4716)) + math.floor(30.6001 * (m + 1)) + d + b - 1524.5


def gregorian_from_jd(jd: float):
    z = math.floor(jd + 0.5)
    f = (jd + 0.5) - z
    if z < 2299161:
        a = z
    else:
        alpha = math.floor((z - 1867216.25) / 36524.25)
        a = z + 1 + alpha - math.floor(alpha / 4)
    b = a + 1524
    c = math.floor((b - 122.1) / 365.25)
    d = math.floor(365.25 * c)
    e = math.floor((b - d) / 30.6001)
    day = b - d - math.floor(30.6001 * e) + f
    month = e - 1 if e < 14 else e - 13
    year = c - 4716 if month > 2 else c - 4715
    di = int(math.floor(day))
    frac = day - di
    return year, month, di, frac


# ---------------------------------------------------------------------------
# 太阳视黄经（Meeus 第 25 章低精度）
# ---------------------------------------------------------------------------
def sun_apparent_longitude(jd: float) -> float:
    t = (jd - 2451545.0) / 36525.0
    l0 = 280.46646 + 36000.76983 * t + 0.0003032 * t * t          # 平黄经
    m = 357.52911 + 35999.05029 * t - 0.0001537 * t * t           # 平近点角
    mr = math.radians(m)
    c = ((1.914602 - 0.004817 * t - 0.000014 * t * t) * math.sin(mr)
         + (0.019993 - 0.000101 * t) * math.sin(2 * mr)
         + 0.000289 * math.sin(3 * mr))                           # 中心差
    true_long = l0 + c
    omega = 125.04 - 1934.136 * t
    lam = true_long - 0.00569 - 0.00478 * math.sin(math.radians(omega))
    return lam % 360.0


def find_jie_jd(year: int, target_deg: float) -> float:
    """求某年太阳视黄经到达 target_deg 的时刻（返回 UT 儒略日）。"""
    jd0 = jd_from_gregorian(year, 1, 1) - 2
    prev = sun_apparent_longitude(jd0) - target_deg
    prev_n = (prev + 180.0) % 360.0 - 180.0
    for k in range(1, 370):
        jd = jd0 + k
        cur = sun_apparent_longitude(jd) - target_deg
        cur_n = (cur + 180.0) % 360.0 - 180.0
        if prev_n < 0.0 <= cur_n:            # 本步内跨越目标黄经
            frac = -prev_n / (cur_n - prev_n)
            return jd - 1 + frac
        prev_n = cur_n
    raise RuntimeError(f"{year} 年黄经 {target_deg}° 未找到")


def jie_date_bj(year: int, target_deg: float):
    """节气时刻转北京时日期 (月, 日)。"""
    jd_ut = find_jie_jd(year, target_deg)
    jd_bj = jd_ut + 8.0 / 24.0               # 东八区
    y, m, d, _ = gregorian_from_jd(jd_bj)
    assert y == year, f"{year} 算出 {y}-{m}-{d}，年份不符"
    return m, d


def table_for_year(year: int):
    return [jie_date_bj(year, deg) for deg, _ in JIE]


def check():
    print("人工核对用：各年十二节日期（北京时）\n")
    print("年份  " + "  ".join(f"{z}节" for _, z in JIE))
    for y in (2024, 2025, 2026, 2027, 2030):
        row = table_for_year(y)
        print(f"{y}  " + "  ".join(f"{m:02d}-{d:02d}" for m, d in row))
    print("\n参考：立春应恒在 2/3-2/5；小寒 1/5-1/7；清明 4/4-4/6；冬至不在本表（那是中气）")


def generate():
    rows = []
    for y in range(YEAR_FROM, YEAR_TO + 1):
        rows.append(table_for_year(y))

    lines = []
    lines.append("// ui/solar_terms.h —— 【自动生成，勿手改】")
    lines.append("// 由 tools/gen_solar_terms.py 生成：十二节日期（北京时）+ 月建地支。")
    lines.append("//")
    lines.append("// 用途：六爻取月建。月建是【节气月】——立春起寅、惊蛰起卯……")
    lines.append("//       所以要知道某公历日期落在哪个「节」之后。")
    lines.append("// 算法：Meeus《Astronomical Algorithms》第 25 章 太阳视黄经低精度公式。")
    lines.append("// 重生成：python3 tools/gen_solar_terms.py")
    lines.append("#pragma once")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append(f"#define SOLAR_TERM_YEAR_FROM {YEAR_FROM}")
    lines.append(f"#define SOLAR_TERM_YEAR_TO   {YEAR_TO}")
    lines.append("")
    lines.append("// 每个月建地支（索引与上表列一一对应，从「小寒→丑月」开始）")
    lines.append("static const char *const SOLAR_TERM_ZHI[12] = {")
    lines.append("    " + ", ".join(f'"{z}"' for _, z in JIE))
    lines.append("};")
    lines.append("")
    lines.append("// 每年 12 个「节」的日期，编码 = 月 * 32 + 日（月<=12、日<=31，够塞进 5 位）")
    lines.append("static const uint16_t SOLAR_TERM_DAY[][12] = {")
    for y, row in zip(range(YEAR_FROM, YEAR_TO + 1), rows):
        enc = ", ".join(f"{m * 32 + d:#06x}" for m, d in row)
        lines.append(f"    {{ {enc} }},   // {y}")
    lines.append("};")
    lines.append("")

    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"已生成 {OUT.relative_to(ROOT)}：{YEAR_TO - YEAR_FROM + 1} 年 × 12 节 = "
          f"{(YEAR_TO - YEAR_FROM + 1) * 12 * 2} 字节数据")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="只打印几个年份供人工核对")
    args = ap.parse_args()
    if args.check:
        check()
        return 0
    generate()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
