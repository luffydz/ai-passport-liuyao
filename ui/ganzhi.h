// ui/ganzhi.h —— 历法层：公历日期 → 干支 / 月建 / 旬空 / 六神 / 旺衰
//
// 真六爻断卦要"今天是哪天"：
//   月建 → 定用神旺衰（旺相休囚死）
//   日辰 → 定生克（生扶 / 克害）
//   日干 → 起六神（青龙朱雀……）
//   日干支 → 定旬空（空亡）
// 这一层就是把它们算出来。
//
// 依赖 ui/solar_terms.h（由 tools/gen_solar_terms.py 生成），纯计算、无硬件依赖，
// 所以固件与模拟器共用同一份，并且能用主机测试穷举核对。
#pragma once

#include <stdbool.h>
#include <stdint.h>

// 可算年份范围 —— 与 ui/solar_terms.h 里的节气表一致。
// 那两张表是 static const 数据，只让 ganzhi.c 包含，避免被多处包含而复制多份；
// ganzhi.c 里有编译期断言守住这两个宏跟表一致。
#define GANZHI_YEAR_FROM 2020
#define GANZHI_YEAR_TO   2040

// ---------------------------------------------------------------------------
// 共享名表（hexagram.c 的装卦层也用这几个，避免两处各写一份表）
// ---------------------------------------------------------------------------
extern const char *const GZ_GAN[10];       // 0甲 1乙 2丙 3丁 4戊 5己 6庚 7辛 8壬 9癸
extern const char *const GZ_ZHI[12];       // 0子 1丑 2寅 3卯 4辰 5巳 6午 7未 8申 9酉 10戌 11亥

// 地支 → 五行，编码 0木 1火 2土 3金 4水（即相生顺序：木→火→土→金→水→木）
extern const uint8_t     GZ_ZHI_WUXING[12];
extern const char *const GZ_WUXING[5];

// ---------------------------------------------------------------------------
// 一日的干支信息
// ---------------------------------------------------------------------------
typedef struct {
    int year, month, day;      // 公历（北京时）

    int year_gan, year_zhi;    // 年柱（以【立春】为年界，不是元旦）
    int month_gan, month_zhi;  // 月柱（月建 = 节气月地支）
    int day_gan, day_zhi;      // 日柱

    int kong_zhi[2];           // 旬空（空亡）的两个地支
} ganzhi_t;

// 由公历日期算干支。超出 ui/solar_terms.h 的年份范围时返回 false。
bool ganzhi_from_date(int y, int m, int d, ganzhi_t *out);

// 六神（按日干起，初爻起）：lineIdx 0=初爻 … 5=上爻
const char *ganzhi_liu_shen(const ganzhi_t *g, int lineIdx);

// 五行旺衰 —— 按【月建】定。wuxing 用 0木 1火 2土 3金 4水
// 返回 "旺" / "相" / "休" / "囚" / "死"
const char *ganzhi_wang_shuai(int wuxing, int month_zhi);

// 日辰对某五行的作用：比和 / 生扶 / 泄 / 克害 / 耗
const char *ganzhi_day_effect(int wuxing, const ganzhi_t *g);

// ---------------------------------------------------------------------------
// 上面两项的【白话版】—— 给普通用户看
//
// 术语（"用神官鬼土休、日辰克害"）只有懂六爻的人才看得懂，所以断卦页用白话。
// 措辞刻意只描述【力量与条件】，不写"吉/凶"，这与通行说法一致：
//   · 月建如季节（定爻的旺衰底色），日辰如当天天气（定当下的助力/阻力）
//   · 旺衰只计力量，不直接判吉凶；见生不必急着叫好，见克不必急着叫坏
// ---------------------------------------------------------------------------
const char *ganzhi_wang_shuai_plain(int wuxing, int month_zhi);
const char *ganzhi_day_effect_plain(int wuxing, const ganzhi_t *g);

// 旺衰的【档位下标】：0旺 1相 2休 3囚 4死（力量由强到弱）；无法判定返回 -1。
// 需要"档位"而不是"名字"时用它 —— 例如按旺衰挑建议文案。
int ganzhi_wang_shuai_level(int wuxing, int month_zhi);

// 五行名（"木"/"火"/"土"/"金"/"水"）→ 序号 0..4；认不出返回 -1
int ganzhi_wuxing_index(const char *name);
