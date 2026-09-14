// algo/dressing.c —— 每日穿衣算法实现（规则来源与推导见 dressing.h 的说明）
#include "dressing.h"

static const char *GAN[10] = { "甲","乙","丙","丁","戊","己","庚","辛","壬","癸" };
static const char *ZHI[12] = { "子","丑","寅","卯","辰","巳","午","未","申","酉","戌","亥" };
static const char *WX[5]   = { "木","火","土","金","水" };

// 五行 → 颜色词。沿用原站用词；原站每天把顺序换一次（大概是求个多样），
// 这里固定一种写法。核对时按【集合】比，不按顺序比。
static const char *WX_COLOR[5] = {
    "翠绿、绿色、青绿、青色",        // 木
    "紫色、橙红、红色、粉色",        // 火
    "橙黄、黄色、褐色、棕色、咖色",  // 土
    "银色、灰色、米白、白色",        // 金
    "黑色、蓝色",                    // 水
};

// 地支 → 五行：子水 丑土 寅木 卯木 辰土 巳火 午火 未土 申金 酉金 戌土 亥水
static const int ZHI_WX[12] = {
    WX_SHUI, WX_TU,  WX_MU,  WX_MU,  WX_TU,  WX_HUO,
    WX_HUO,  WX_TU,  WX_JIN, WX_JIN, WX_TU,  WX_SHUI
};

static bool is_leap(int y)
{
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static int month_days(int y, int m)
{
    static const int M[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    return M[m - 1] + ((m == 2 && is_leap(y)) ? 1 : 0);
}

// 距 2000-01-01 的天数（当天取 0）。写成逐年累加而不是闭式公式 ——
// 这段是全链路的起点，宁可慢也要一眼能看懂、能手工核对。
static long days_from_2000(int y, int m, int d)
{
    long n = 0;
    if (y >= 2000) {
        for (int yy = 2000; yy < y; yy++) n += is_leap(yy) ? 366 : 365;
    } else {
        for (int yy = y; yy < 2000; yy++) n -= is_leap(yy) ? 366 : 365;
    }
    for (int mm = 1; mm < m; mm++) n += month_days(y, mm);
    return n + (d - 1);
}

bool dressing_for_date(int y, int m, int d, day_gz_t *gz, dressing_t *out)
{
    if (m < 1 || m > 12 || d < 1 || d > month_days(y, m)) return false;

    const long n = days_from_2000(y, m, d);
    // 基准：2000-01-01 是【戊午】日。
    // 戊 = 天干第 4（甲0）、午 = 地支第 6（子0）→ 序数须同时满足 mod10=4、mod12=6，
    // 在 0..59 内唯一解是 54（54%10=4、54%12=6）✓ 故 序数 = (54 + N) mod 60。
    //
    // 注：网上流传的同一公式常写成 55 —— 那是错的（55%10=5=己、55%12=7=未，成了己未），
    // 会让整条链每天偏一天。这里是用原站 31 天数据回代才发现并纠正的，见 test_dressing.c。
    const int seq = (int)(((54 + n) % 60 + 60) % 60);   // 再取一次模，兼容 2000 年之前
    const int gan = seq % 10;
    const int zhi = seq % 12;

    // ② 「我」取【日支】五行，不是日干 —— 依据见 dressing.h 的反证。
    const int me = ZHI_WX[zhi];

    if (gz) {
        gz->gan = gan;
        gz->zhi = zhi;
        gz->zhi_wuxing = me;
    }
    if (out) {
        // 五行下标按相生链排（木火土金水），所以两个关系都退化成加法：
        //   我生 = +1（木生火…）   我克 = +2（木克土、土克水…）
        out->daji = WX_COLOR[(me + 1) % WX_COUNT];
        out->ciji = WX_COLOR[me];
        out->buyi = WX_COLOR[(me + 2) % WX_COUNT];
        out->me   = WX[me];
    }
    return true;
}

const char *wuxing_name(int wuxing)
{
    return (wuxing < 0 || wuxing >= WX_COUNT) ? "" : WX[wuxing];
}

const char *gan_name(int i) { return (i < 0 || i > 9)  ? "" : GAN[i]; }
const char *zhi_name(int i) { return (i < 0 || i > 11) ? "" : ZHI[i]; }

const char *wuxing_colors(int wuxing)
{
    return (wuxing < 0 || wuxing >= WX_COUNT) ? "" : WX_COLOR[wuxing];
}
