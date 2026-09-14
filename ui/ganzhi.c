// ui/ganzhi.c —— 历法层实现（详见 ganzhi.h）
#include "ganzhi.h"
#include "solar_terms.h"

#include <string.h>   // strcmp：地支名 -> 地支序

// 编译期守住：ganzhi.h 声明的年份范围必须跟节气表实际范围一致，
// 否则查表会越界（这是最不该出现的错，所以用断言而不是注释约束）。
_Static_assert(GANZHI_YEAR_FROM == SOLAR_TERM_YEAR_FROM, "ganzhi.h 年份下界与节气表不一致");
_Static_assert(GANZHI_YEAR_TO   == SOLAR_TERM_YEAR_TO,   "ganzhi.h 年份上界与节气表不一致");

// ---------------------------------------------------------------------------
// 共享名表
// ---------------------------------------------------------------------------
const char *const GZ_GAN[10] = { "甲","乙","丙","丁","戊","己","庚","辛","壬","癸" };
const char *const GZ_ZHI[12] = { "子","丑","寅","卯","辰","巳","午","未","申","酉","戌","亥" };

// 地支 → 五行：子水 丑土 寅木 卯木 辰土 巳火 午火 未土 申金 酉金 戌土 亥水
const uint8_t GZ_ZHI_WUXING[12] = { 4, 2, 0, 0, 2, 1, 1, 2, 3, 3, 2, 4 };

// 五行名（下标即相生顺序：木→火→土→金→水→木）
const char *const GZ_WUXING[5] = { "木","火","土","金","水" };

// 六神（自初爻起按顺序排）
static const char *LIU_SHEN[6] = { "青龙","朱雀","勾陈","螣蛇","白虎","玄武" };

// 十二支对应的"季节五行"（定旺衰用）：辰未戌丑为四季土月
static const uint8_t MONTH_SEASON_WX[12] = {
    4,  // 子 → 水
    2,  // 丑 → 土
    0,  // 寅 → 木
    0,  // 卯 → 木
    2,  // 辰 → 土
    1,  // 巳 → 火
    1,  // 午 → 火
    2,  // 未 → 土
    3,  // 申 → 金
    3,  // 酉 → 金
    2,  // 戌 → 土
    4,  // 亥 → 水
};

// ---------------------------------------------------------------------------
// 儒略日（Meeus《Astronomical Algorithms》第 7 章）
// 本模块只用 2020-2040，各数均为正，故 (int) 截断等价于 floor。
// ---------------------------------------------------------------------------
static long jdn_from_ymd(int y, int m, int d)
{
    if (m <= 2) { y -= 1; m += 12; }
    const int a = y / 100;
    const int b = 2 - a + a / 4;
    const long jd = (long)(365.25 * (y + 4716))
                  + (long)(30.6001 * (m + 1))
                  + d + b - 1524;
    return jd;   // 返回整数儒略日数（JDN）
}

// ---------------------------------------------------------------------------
// 月建：找该日期落在哪个「节」之后
// 表内 12 个节按公历年内先后排列（小寒在年初），编码 = 月*32+日，单调递增。
// 落在本年小寒之前 → 属于上一年的大雪，即子月（下标 11）。
//
// ⚠ 这里必须把"节在表里的下标"翻译成【地支序】：
//     表下标 0=小寒、1=立春…；地支序 0=子、1=丑…
//   两者差一格（小寒对丑、立春对寅）。直接拿下标当地支用会整体错一个月 ——
//   实测就是这个坑：白露后本该酉月，却算成了申月。
// ---------------------------------------------------------------------------
static int zhi_seq_of_name(const char *z)
{
    for (int i = 0; i < 12; i++) {
        if (!strcmp(GZ_ZHI[i], z)) return i;
    }
    return -1;
}

static int month_zhi_seq(int y, int m, int d)
{
    const uint16_t *row = SOLAR_TERM_DAY[y - SOLAR_TERM_YEAR_FROM];
    const int today = m * 32 + d;
    int found = -1;
    for (int i = 0; i < 12; i++) {
        if ((int)row[i] <= today) found = i;
        else break;
    }
    if (found < 0) found = 11;                 // 小寒前 → 上一年大雪
    const int seq = zhi_seq_of_name(SOLAR_TERM_ZHI[found]);
    return (seq >= 0) ? seq : 0;
}

// 该年份的立春（下标 1）编码
static int lichun_code(int y)
{
    return (int)SOLAR_TERM_DAY[y - SOLAR_TERM_YEAR_FROM][1];
}

// ---------------------------------------------------------------------------
// 对外接口
// ---------------------------------------------------------------------------
bool ganzhi_from_date(int y, int m, int d, ganzhi_t *out)
{
    if (!out) return false;
    if (y < SOLAR_TERM_YEAR_FROM || y > SOLAR_TERM_YEAR_TO) return false;
    if (m < 1 || m > 12 || d < 1 || d > 31) return false;

    out->year = y; out->month = m; out->day = d;

    // ---- 年柱：以立春为年界 ----
    const int gz_year = (m * 32 + d < lichun_code(y)) ? (y - 1) : y;
    // 1984 年为甲子年（通用锚点）
    int yi = ((gz_year - 1984) % 60 + 60) % 60;
    out->year_gan = yi % 10;
    out->year_zhi = yi % 12;

    // ---- 月柱：月建取节气月，月干用五虎遁 ----
    out->month_zhi = month_zhi_seq(y, m, d);   // 地支序 0子…11亥（已由节气名翻译）
    // 五虎遁：甲己之年丙作首 —— 寅月天干 = (年干 % 5) * 2 + 2
    const int yin_gan = ((out->year_gan % 5) * 2 + 2) % 10;
    // 从寅月（地支序 2）推到当月
    out->month_gan = (yin_gan + ((out->month_zhi - 2) % 12 + 12) % 12) % 10;

    // ---- 日柱：JDN 推算 ----
    // 锚点：1949-10-01（开国大典）= 甲子日（通用校核点）
    const long jdn = jdn_from_ymd(y, m, d);
    const int di = (int)((jdn + 49) % 60);
    out->day_gan = di % 10;
    out->day_zhi = di % 12;

    // ---- 旬空：旬首之后两位地支 ----
    const int xun_shou = di - (di % 10);
    out->kong_zhi[0] = (xun_shou + 10) % 12;
    out->kong_zhi[1] = (xun_shou + 11) % 12;

    return true;
}

const char *ganzhi_liu_shen(const ganzhi_t *g, int lineIdx)
{
    if (!g || lineIdx < 0 || lineIdx > 5) return "";
    // 日干起六神：甲乙起青龙、丙丁起朱雀、戊起勾陈、己起螣蛇、庚辛起白虎、壬癸起玄武
    static const uint8_t START[10] = { 0, 0, 1, 1, 2, 3, 4, 4, 5, 5 };
    return LIU_SHEN[(START[g->day_gan] + lineIdx) % 6];
}

// 旺衰/日辰都先算成"下标"，再取术语名或白话名 ——
// 这样术语与否只有一处逻辑，不会两套判断走偏。
//   旺衰下标：0旺 1相 2休 3囚 4死（力量由强到弱）
static int wang_shuai_index(int wuxing, int month_zhi)
{
    if (wuxing < 0 || wuxing > 4 || month_zhi < 0 || month_zhi > 11) return -1;
    const int s = MONTH_SEASON_WX[month_zhi];   // 当令五行
    if (wuxing == s)            return 0;       // 当令者旺
    if ((s + 1) % 5 == wuxing)  return 1;       // 当令者所生为相
    if ((wuxing + 1) % 5 == s)  return 2;       // 生当令者为休
    if ((wuxing + 2) % 5 == s)  return 3;       // 克当令者为囚
    return 4;                                    // 当令者所克为死
}

//   日辰下标：0比和 1生扶 2泄 3克害 4耗
static int day_effect_index(int wuxing, const ganzhi_t *g)
{
    if (!g || wuxing < 0 || wuxing > 4) return -1;
    const int dw = GZ_ZHI_WUXING[g->day_zhi];   // 日支五行
    if (wuxing == dw)            return 0;      // 同气
    if ((dw + 1) % 5 == wuxing)  return 1;      // 日辰生我
    if ((wuxing + 1) % 5 == dw)  return 2;      // 我生日辰，泄气
    if ((dw + 2) % 5 == wuxing)  return 3;      // 日辰克我
    return 4;                                    // 我克日辰，耗力
}

const char *ganzhi_wang_shuai(int wuxing, int month_zhi)
{
    // 旺相休囚死，力量由强到弱：当令者旺、当令所生为相、生当令者休、
    // 克当令者囚、被当令所克者死。
    static const char *N[5] = { "旺", "相", "休", "囚", "死" };
    const int i = wang_shuai_index(wuxing, month_zhi);
    return (i < 0) ? "" : N[i];
}

const char *ganzhi_day_effect(int wuxing, const ganzhi_t *g)
{
    static const char *N[5] = { "比和", "生扶", "泄", "克害", "耗" };
    const int i = day_effect_index(wuxing, g);
    return (i < 0) ? "" : N[i];
}

// ---------------------------------------------------------------------------
// 白话版：给普通用户看（详见 ganzhi.h 的说明）
//   "底子很足…" 描述的是【力量强弱】，不是吉凶
//   "今日有助…" 描述的是【当天条件】，同样不是吉凶
// ---------------------------------------------------------------------------
const char *ganzhi_wang_shuai_plain(int wuxing, int month_zhi)
{
    static const char *N[5] = { "底子很足", "底子不错", "势头偏弱", "施展不开", "几乎没力" };
    const int i = wang_shuai_index(wuxing, month_zhi);
    return (i < 0) ? "" : N[i];
}

const char *ganzhi_day_effect_plain(int wuxing, const ganzhi_t *g)
{
    static const char *N[5] = { "今日平顺", "今日有助", "今日耗力", "今日受制", "今日费劲" };
    const int i = day_effect_index(wuxing, g);
    return (i < 0) ? "" : N[i];
}

int ganzhi_wuxing_index(const char *name)
{
    if (!name) return -1;
    for (int i = 0; i < 5; i++) {
        if (!strcmp(GZ_WUXING[i], name)) return i;
    }
    return -1;
}
