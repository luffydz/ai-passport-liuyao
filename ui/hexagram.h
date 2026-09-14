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

// ---------------------------------------------------------------------------
// 动爻用 6 位【掩码】表示
//
// 为什么不是单个下标：传统掷币法六爻都可能发动，也可能一个都不动
//（"六爻安静"约每 5~6 次遇到一次），所以动爻是一组、不是一个。
//   bit0 = 初爻 … bit5 = 上爻；0 表示六爻安静。
// ---------------------------------------------------------------------------
typedef uint8_t liuyao_moving_t;
#define LIUYAO_MOV(i)  ((liuyao_moving_t)(1u << (i)))

// 变卦：把动爻逐爻取反（阳变阴、阴变阳），不动的爻原样保留。
void getChangedHexagram(uint8_t originalUpper, uint8_t originalLower,
                        liuyao_moving_t moving,
                        uint8_t *outUpper, uint8_t *outLower);

// 由六爻反查上下卦（lines[0]=初 … lines[5]=上，1=阳）。
// 摇卦是逐爻掷出来的，攒满六爻后靠它还原出是哪一卦。
void getHexagramFromLines(const uint8_t lines[6], uint8_t *outUpper, uint8_t *outLower);

// 主爻：动爻里位置最高的那一个（变占法「以上爻为主」）。返回 -1 = 无动爻。
int getPrimaryMovingLine(liuyao_moving_t moving);

// ---------------------------------------------------------------------------
// 变占规则（朱熹《易学启蒙》）：动爻个数决定该读哪段文字
//
//   0 动 → 本卦卦辞
//   1 动 → 该动爻的爻辞
//   2 动 → 两个动爻的爻辞，以上爻为主
//   3 动 → 本卦与变卦的卦辞，以本卦为主
//   4 动 → 变卦中两个"不动之爻"的爻辞
//   5 动 → 变卦中那个"不动之爻"的爻辞
//   6 动 → 乾坤看用九/用六，其余看变卦卦辞
// ---------------------------------------------------------------------------
typedef enum {
    BIAN_ZHAN_JING = 0,      // 0 动
    BIAN_ZHAN_ONE_YAO,       // 1 动
    BIAN_ZHAN_TWO_YAO,       // 2 动
    BIAN_ZHAN_THREE_GUA,     // 3 动
    BIAN_ZHAN_FOUR,          // 4 动
    BIAN_ZHAN_FIVE,          // 5 动
    BIAN_ZHAN_SIX,           // 6 动
} bian_zhan_t;

bian_zhan_t getBianZhanRule(liuyao_moving_t moving);

// 这一卦按变占法该看什么，一句白话（给"断卦"页那一行用，≤14 字）
const char *getBianZhanText(uint8_t upper, uint8_t lower, liuyao_moving_t moving);

// 六爻皆动时，乾坤两卦有专用辞（用九 / 用六）；其余卦返回空串
const char *getYongJiuYongLiu(uint8_t upper, uint8_t lower);
const char *getYongJiuYongLiuPlain(uint8_t upper, uint8_t lower);   // 它的白话

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

// ---------------------------------------------------------------------------
// 事由建议 —— 按【用神旺衰档位】选句
//
// 早先这行按「事由 × 动爻阴阳」查表，8×2 = 16 条。两个毛病：
//   ① 阴阳与"该进该守"没有必然关系 —— 用神明明休囚无力，只要动爻是阳，
//      照样输出"宜积极进取"，跟上面几行打架；
//   ② 锁定一个事由后只有 2 种说法，连起几次就撞同一句。
// 现在改按旺衰档位取句，8×5 = 40 条：旺则放手、死则停手，
// 与「力量」那行同源，不会再打架，重复率也降下来。
//
// level: 0旺 1相 2休 3囚 4死（与 ganzhi_wang_shuai_level 同序）；
//        传 -1（算不出旺衰）时按 2休 处理。
// ---------------------------------------------------------------------------
const char *getCategoryAdvice(uint8_t cat, int level);

// 动爻爻位含义（六爻常法：爻位象征事情所处阶段）。lineIdx 0=初 … 5=上。
const char *getLinePosMeaning(uint8_t lineIdx);

// 动爻所在卦的五行走向（本卦 → 变卦），归为 5 类：
//   比和 / 卦生变（顺势）/ 变生卦（得助）/ 卦克变（可制）/ 变克卦（受阻）
// 说明：这是"卦级五行"的简化判断（真六爻看爻级纳甲五行），
//       但逻辑自洽、结果随卦而变，不会反复撞同一句。
const char *getElemRelationText(uint8_t upper, uint8_t lower, liuyao_moving_t moving);

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
void getLiuyaoChart(uint8_t upper, uint8_t lower, liuyao_moving_t moving,
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
                                 liuyao_moving_t moving);

// 世应关系的白话（世 = 你自己，应 = 对方）。
// 取世、应两爻的五行论生克，描述【双方力量对比】；同样不下吉凶断语。
// 卦里找不到世或应时返回空串。返回例："你方占上风，"
const char *getShiYingPlain(const liuyao_chart_t *chart);
