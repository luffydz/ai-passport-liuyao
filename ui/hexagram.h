// ui/hexagram.h —— 六爻/周易数据与算法层（纯计算，无任何硬件依赖）。
//
// 由原 M5Cardputer 项目（src/hexagram.h + hexagram.cpp）移植而来：
//   - 只把 C++ 引用改成指针、Arduino.h 换成 stdint/stdbool，数据表逐字保留
//   - 不依赖 lvgl / ESP-IDF，因此固件与模拟器都能直接编译
//
// 数据规模：64 卦名 + 64 卦辞 + 64×6 爻辞 + 64 条白话解读 + 8 类事由建议
#pragma once

#include <stdbool.h>
#include <stdint.h>

// 八卦三爻位数据（定义在 hexagram.c）
extern const uint8_t TRIGRAM_LINES[8][3];

// 八卦索引
// 0=乾☰, 1=兑☱, 2=离☲, 3=震☳, 4=巽☴, 5=坎☵, 6=艮☶, 7=坤☷
#define TRIGRAM_COUNT 8
#define CATEGORY_COUNT 8

// 一卦的完整数据
typedef struct {
    const char *name;      // 卦名，如"乾为天"
    const char *judgment;  // 卦辞，如"元亨利贞"
} HexagramData;

// 查询一个 (上卦, 下卦) 组合对应的卦
void getHexagram(uint8_t upperIdx, uint8_t lowerIdx, HexagramData *out);

// 三位数 → 上卦/下卦/动爻
//   百位 % 8 → 上卦（0 视作 7=坤）
//   十位 % 8 → 下卦（同上）
//   个位 % 6 → 动爻（0 视作 6，再转 0..5）
void generateHexagramFromDigits(const uint8_t digits[3],
                                uint8_t *outUpper, uint8_t *outLower,
                                uint8_t *outMovingLine);

// 获取变卦（翻转动爻后重新解析上下卦）
void getChangedHexagram(uint8_t originalUpper, uint8_t originalLower,
                        uint8_t movingLine,
                        uint8_t *outUpper, uint8_t *outLower);

// 动爻爻辞
const char *getLineJudgment(uint8_t upper, uint8_t lower, uint8_t lineIdx);

// 同一爻的【白话】翻译（原文是古文，普通人看不懂）。
// 每条都 ≤12 汉字，刚好一行放得下 —— 见 hexagram.c 里 LINE_JUDGES_PLAIN 的说明。
const char *getLineJudgmentPlain(uint8_t upper, uint8_t lower, uint8_t lineIdx);

// 爻位名（如"初九"、"六二"）
const char *getLineName(uint8_t lineIdx, bool isYang);

// 八卦辅助
const char *getTrigramName(uint8_t idx);
const char *getTrigramSymbol(uint8_t idx);
const char *getTrigramElement(uint8_t idx);

// 类别辅助
const char *getCategoryName(uint8_t catIdx);
const char *getCategoryYongShen(uint8_t catIdx);

// 白话解释与类别建议
const char *getHexagramInterp(uint8_t upper, uint8_t lower);
const char *getCategorySuggestion(uint8_t cat, bool yang);

// ---------------------------------------------------------------------------
// 断卦扩充维度
//
// 为什么需要：只用 getCategorySuggestion() 的话，锁定一个事由后整张表
// 只有 2 条（阴阳各一），连起 7、8 次会反复看到同样的话。
// 下面两个维度与「事由类别」彼此独立，乘起来把断语变体从 2 提到 60（单类别）。
// ---------------------------------------------------------------------------

// 动爻爻位含义（六爻常法：爻位象征事情所处阶段）。lineIdx 0=初 … 5=上。
const char *getLinePosMeaning(uint8_t lineIdx);

// 动爻所在卦的五行走向（本卦 → 变卦），归为 5 类：
//   比和 / 卦生变（顺势）/ 变生卦（得助）/ 卦克变（可制）/ 变克卦（受阻）
// 说明：这是"卦级五行"的简化判断（真六爻看爻级纳甲五行），
//       但逻辑自洽、结果随卦而变，不会反复撞同一句。
const char *getElemRelationText(uint8_t upper, uint8_t lower, uint8_t movingLine);

// ---------------------------------------------------------------------------
// 六爻【装卦】层（纳甲六爻的正统算法，纯规则推导）
// ---------------------------------------------------------------------------
// 这一层没有任何"文学创作"，全是规则：
//   定卦宫 → 安世应 → 纳甲（给六爻配干支）→ 地支定五行 → 按卦宫五行配六亲
//   → 按事由取用神
// 因此可以用主机测试穷举 64 卦逐条核对，做到"真"而不是"像"。
//
// ⚠ 尚未包含：月建 / 日辰 / 旺衰 / 旬空 / 六神。
//   原因是它们都要"今天是哪天"，而本设备没有 RTC 时钟源（断电即丢时间），
//   不引入这个假前提。等确定了取时间的方式再加。

// 六亲（按"我"即卦宫五行定：同我者兄弟）
typedef enum {
    LIUQIN_XIONGDI = 0,  // 兄弟（同我）
    LIUQIN_FUMU,         // 父母（生我）
    LIUQIN_ZISUN,        // 子孙（我生）
    LIUQIN_QICAI,        // 妻财（我克）
    LIUQIN_GUANGUI,      // 官鬼（克我）
    LIUQIN_COUNT
} liuqin_t;

// 一爻的装卦结果
typedef struct {
    char        ganzhi[8];  // 纳甲干支，如 "乙未"
    const char *wuxing;     // 地支五行，如 "土"
    const char *liuqin;     // 六亲，如 "父母"
    bool        is_yang;    // true=阳爻(━━━)，false=阴爻(━ ━)
    uint8_t     shiying;    // 0=无 1=世 2=应
    bool        is_moving;  // 是否动爻
} liuyao_line_t;

// 整卦的装卦结果
typedef struct {
    uint8_t       palace;         // 卦宫：沿用八卦索引 0乾1兑2离3震4巽5坎6艮7坤
    const char   *palace_name;    // 如 "坎宫"
    const char   *palace_wuxing;  // 卦宫五行，如 "水"
    liuyao_line_t lines[6];       // [0]=初爻 … [5]=上爻
} liuyao_chart_t;

// 装卦：由 (上卦, 下卦, 动爻) 得到完整卦盘
void getLiuyaoChart(uint8_t upper, uint8_t lower, uint8_t movingLine,
                    liuyao_chart_t *out);

// 取用神：按事由类别返回卦盘中作为用神的爻下标（0..5）。
// 返回 -1 = 该事由对应的六亲不在此卦中（六爻术语："用神不上卦"，需取伏神）。
int getYongShenLine(uint8_t catIdx, const liuyao_chart_t *chart);

// ---------------------------------------------------------------------------
// 断语白话映射 —— 给普通用户看
//
// 六爻术语（"用神官鬼土休、日辰克害"）对普通用户是天书，断卦页改用白话。
// 措辞只描述【事情处在什么阶段、趋势顺不顺】，不下"吉/凶"断语 ——
// 这与通行说法一致：旺衰只计力量，不直接判吉凶。
// ---------------------------------------------------------------------------
const char *getLinePosPlain(uint8_t lineIdx);                       // 爻位白话
const char *getElemRelationPlain(uint8_t upper, uint8_t lower,       // 五行走向白话
                                 uint8_t movingLine);

// 世应关系的白话（世 = 你自己，应 = 对方）。
// 取世、应两爻的五行论生克，描述【双方力量对比】；同样不下吉凶断语。
// 卦里找不到世或应时返回空串。返回例："你方占上风，"
const char *getShiYingPlain(const liuyao_chart_t *chart);
