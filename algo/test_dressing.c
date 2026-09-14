// algo/test_dressing.c —— 拿原站公布的 31 天数据回代，验证算法是否一致
//
// 【验证集】3g.d5168.com/wuhang/（2026-09-14 抓取）公布的 2026-09-14 ~ 2026-10-14
// 共 31 天的「大吉色」。该站不公开算法，所以只有拿结果反证一条路。
//
// 【为什么这一列就够验全链路】大吉色 = 日支所生，而日支由日期唯一决定。
// 所以「日期 → 日柱 → 日支五行 → 大吉色」四步里只要有一步错了，31 天里必然对不上。
// 另外 9-14 那天原站还给了完整三档与日柱，单独再核一遍。
//
// 【比较方式】原站每天把颜色词的顺序换一次（大概是求多样），所以按【集合】比，
// 不按顺序比。比较直接拿输出字符串对页面字符串 —— 不用内部变量，端到端。
#include "dressing.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

// ---------------------------------------------------------------------------
// 原站数据（逐日大吉色，原文照抄）
// ---------------------------------------------------------------------------
typedef struct { int y, m, d; const char *daji; } CASE;

static const CASE CASES[] = {
    { 2026, 9, 14, "粉色、紫色、橙红、红色" },
    { 2026, 9, 15, "灰色、米白、银色、白色" },
    { 2026, 9, 16, "橙黄、黄色、褐色、棕色、咖色" },
    { 2026, 9, 17, "褐色、橙黄、咖色、黄色、棕色" },
    { 2026, 9, 18, "灰色、白色、银色、米白" },
    { 2026, 9, 19, "黑色、蓝色" },
    { 2026, 9, 20, "蓝色、黑色" },
    { 2026, 9, 21, "银色、灰色、米白、白色" },
    { 2026, 9, 22, "绿色、青色、翠绿、青绿" },
    { 2026, 9, 23, "绿色、青色、青绿、翠绿" },
    { 2026, 9, 24, "米白、灰色、白色、银色" },
    { 2026, 9, 25, "紫色、橙红、粉色、红色" },
    { 2026, 9, 26, "橙红、红色、紫色、粉色" },
    { 2026, 9, 27, "灰色、银色、白色、米白" },
    { 2026, 9, 28, "橙黄、棕色、黄色、咖色、褐色" },
    { 2026, 9, 29, "橙黄、褐色、咖色、黄色、棕色" },
    { 2026, 9, 30, "银色、米白、白色、灰色" },
    { 2026, 10, 1, "黑色、蓝色" },
    { 2026, 10, 2, "黑色、蓝色" },
    { 2026, 10, 3, "银色、白色、灰色、米白" },
    { 2026, 10, 4, "翠绿、绿色、青绿、青色" },
    { 2026, 10, 5, "绿色、青绿、翠绿、青色" },
    { 2026, 10, 6, "灰色、白色、米白、银色" },
    { 2026, 10, 7, "紫色、橙红、红色、粉色" },
    { 2026, 10, 8, "红色、紫色、橙红、粉色" },
    { 2026, 10, 9, "银色、白色、灰色、米白" },
    { 2026, 10, 10, "褐色、咖色、棕色、黄色、橙黄" },
    { 2026, 10, 11, "黄色、咖色、褐色、橙黄、棕色" },
    { 2026, 10, 12, "白色、银色、米白、灰色" },
    { 2026, 10, 13, "蓝色、黑色" },
    { 2026, 10, 14, "蓝色、黑色" },
};
#define CASE_N ((int)(sizeof(CASES) / sizeof(CASES[0])))

// ---------------------------------------------------------------------------
// 「甲、乙、丙」按顿号拆开，判断两串是不是同一个集合（忽略顺序）
// ---------------------------------------------------------------------------
static bool same_set(const char *a, const char *b)
{
    char ba[128], bb[128];
    snprintf(ba, sizeof(ba), "%s", a);
    snprintf(bb, sizeof(bb), "%s", b);

    char *wa[12], *wb[12];
    int na = 0, nb = 0;
    for (char *p = strtok(ba, "、"); p && na < 12; p = strtok(NULL, "、")) wa[na++] = p;
    for (char *p = strtok(bb, "、"); p && nb < 12; p = strtok(NULL, "、")) wb[nb++] = p;

    if (na != nb) return false;
    bool used[12] = { false };
    for (int i = 0; i < na; i++) {
        bool hit = false;
        for (int j = 0; j < nb; j++) {
            if (!used[j] && strcmp(wa[i], wb[j]) == 0) { used[j] = true; hit = true; break; }
        }
        if (!hit) return false;
    }
    return true;
}

int main(void)
{
    int pass = 0, fail = 0;

    printf("=== ① 逐日回代：大吉色（原站 31 天）===\n");
    printf("  %-12s %-6s %-4s %s\n", "日期", "日柱", "我", "结果");
    for (int i = 0; i < CASE_N; i++) {
        day_gz_t gz;
        dressing_t d;
        const CASE *c = &CASES[i];
        if (!dressing_for_date(c->y, c->m, c->d, &gz, &d)) {
            printf("  %d-%02d-%02d   非法日期 ✗\n", c->y, c->m, c->d);
            fail++;
            continue;
        }
        char gzs[16];
        snprintf(gzs, sizeof(gzs), "%s%s", gan_name(gz.gan), zhi_name(gz.zhi));
        const bool ok = same_set(c->daji, d.daji);
        ok ? pass++ : fail++;
        printf("  %d-%02d-%02d    %-6s %-4s %s\n", c->y, c->m, c->d, gzs, d.me,
               ok ? "✓" : "✗ 对不上");
        if (!ok) printf("      页面: %s\n      算出: %s\n", c->daji, d.daji);
    }

    printf("\n=== ② 2026-09-14 完整三档（原站唯一给了全部三档的一天）===\n");
    {
        day_gz_t gz;
        dressing_t d;
        dressing_for_date(2026, 9, 14, &gz, &d);
        char gzs[16];
        snprintf(gzs, sizeof(gzs), "%s%s", gan_name(gz.gan), zhi_name(gz.zhi));
        const bool gz_ok = (strcmp(gzs, "辛卯") == 0);
        const bool a = same_set(d.daji, "粉色、紫色、橙红、红色");
        const bool b = same_set(d.ciji, "翠绿、青色、青绿、绿色");
        const bool c = same_set(d.buyi, "黄色、咖色、橙黄、褐色、棕色");
        printf("  日柱   页面 辛卯   算出 %-4s  %s\n", gzs, gz_ok ? "✓" : "✗");
        printf("  大吉   页面 粉/紫/橙红/红(火)   算出 %-22s %s\n", d.daji, a ? "✓" : "✗");
        printf("  次吉   页面 翠绿/青/青绿/绿(木) 算出 %-22s %s\n", d.ciji, b ? "✓" : "✗");
        printf("  不宜   页面 黄/咖/橙黄/褐/棕(土)算出 %-22s %s\n", d.buyi, c ? "✓" : "✗");
        (gz_ok && a && b && c) ? pass++ : fail++;
    }

    printf("\n=== ③ 内部一致性：日支应当每天前进一位 ===\n");
    {
        int bad = 0, prev = -1;
        for (int i = 0; i < CASE_N; i++) {
            day_gz_t gz;
            dressing_for_date(CASES[i].y, CASES[i].m, CASES[i].d, &gz, NULL);
            if (prev >= 0 && gz.zhi != (prev + 1) % 12) bad++;
            prev = gz.zhi;
        }
        printf("  31 天连续，日支跳变 %d 处 %s\n", bad, bad == 0 ? "✓" : "✗");
        bad == 0 ? pass++ : fail++;
    }

    printf("\n=== 汇总：通过 %d，失败 %d ===\n", pass, fail);
    printf("%s\n", fail == 0 ? "与原站 31 天数据完全一致 ✓" : "有对不上的地方，需排查 ✗");
    return fail == 0 ? 0 : 1;
}
