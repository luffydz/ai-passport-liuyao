// ui/view_dressing.c —— 每日穿衣 App 的页面流（说明见 view_dressing.h）
#include "view_dressing.h"
#include "app_ui.h"        // 首页长按 OK 返回功能选择启动页
#include "dressing.h"      // algo/：日柱 → 三档穿衣色（纯计算）
#include "ganzhi.h"        // 干支层：日期上下限 + 干支预览
#include "ui_theme.h"
#include "lvgl.h"

#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// 状态
// ---------------------------------------------------------------------------
typedef enum { PAGE_WELCOME, PAGE_DATE, PAGE_RESULT } page_t;

static page_t    s_page;
static lv_obj_t *s_scr;

// 日期页
static int       s_date[3];              // 年 月 日
static int       s_dpos;                 // 正在改第几位（0 年 / 1-2 月 / 3-4 日）
static lv_obj_t *s_frames[3];
static lv_obj_t *s_digit[5];
static lv_obj_t *s_gz;                   // 干支预览
static lv_obj_t *s_mark;                 // 选中位下面的小金杠

// 结果页
static day_gz_t    s_day;
static dressing_t  s_dress;

// ---------------------------------------------------------------------------
// 布局（数字取自同款三键设备的取日期页 —— 那一页已经在真机上跑顺了，尺寸照搬省得重调）
// ---------------------------------------------------------------------------
#define DATE_BOX_Y   108
#define DATE_BOX_H    54
#define DATE_YEAR_X   10
#define DATE_YEAR_W   76
#define DATE_MON_X    92
#define DATE_MON_W    62
#define DATE_DAY_X   160
#define DATE_DAY_W    62
#define DATE_MARK_W   16
#define DATE_MARK_Y  151

// 结果页三档的 y（每档三行：档名 / 色块 / 色名）
#define TIER_Y0       86
#define TIER_STEP     66
#define DOT_D         16
#define DOT_GAP        6

// ---------------------------------------------------------------------------
// 颜色词 → 实际色值
//
// 算法返回的是【词】（「翠绿、绿色、青绿、青色」），屏幕上得给出真正的颜色 ——
// 穿衣这件事，色块比字直观。
// 取色只是一家之言：同一个「粉色」各人理解不同，这里取的是常见认知里偏正的值。
// ---------------------------------------------------------------------------
static const struct { const char *name; uint32_t rgb; } COLOR_RGB[] = {
    { "粉色", 0xE9A0B4 }, { "紫色", 0x8B5CC7 }, { "橙红", 0xE2542C }, { "红色", 0xD32F2F },
    { "翠绿", 0x2FA84F }, { "绿色", 0x43A047 }, { "青绿", 0x1FA08C }, { "青色", 0x2E9BC4 },
    { "橙黄", 0xE8A33D }, { "黄色", 0xE8C93D }, { "褐色", 0x7A4B2A }, { "棕色", 0x8B5A2B },
    { "咖色", 0x5E4632 }, { "银色", 0xC3C6CC }, { "灰色", 0x9A9A9A }, { "米白", 0xEFE8D6 },
    { "白色", 0xF7F7F2 }, { "黑色", 0x141414 }, { "蓝色", 0x2E5FA8 },
};
#define COLOR_N ((int)(sizeof(COLOR_RGB) / sizeof(COLOR_RGB[0])))

static uint32_t color_rgb_of(const char *name)
{
    for (int i = 0; i < COLOR_N; i++) {
        if (strcmp(COLOR_RGB[i].name, name) == 0) return COLOR_RGB[i].rgb;
    }
    return 0x808080;                 // 表里没有就用中性灰，不猜
}

// ---------------------------------------------------------------------------
// 小工具
// ---------------------------------------------------------------------------
static lv_obj_t *dot_create(int x, int y, int d, uint32_t rgb)
{
    lv_obj_t *o = lv_obj_create(s_scr);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, d, d);
    lv_obj_set_style_radius(o, d / 2, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(rgb), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    // 描一圈暗金边：不然「黑色」那块在纯黑底上根本看不见
    lv_obj_set_style_border_color(o, lv_color_hex(TH_DIM), 0);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    return o;
}

static void center_label(const char *txt, const lv_font_t *font, uint32_t color, int y)
{
    lv_obj_t *l = th_label(s_scr, txt, font, color);
    lv_obj_set_width(l, APP_SCREEN_W - 2 * TH_FRAME_INSET - 8);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, y);
}

// ---------------------------------------------------------------------------
// 日期页：日期合法性与逐位编辑
//（这一段与取日期页同源：三键设备上逐位调，天然调不出非法日期）
// ---------------------------------------------------------------------------
static bool is_leap(int y)
{
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static int days_in_month(int y, int m)
{
    static const int D[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    return D[m - 1] + ((m == 2 && is_leap(y)) ? 1 : 0);
}

static void date_clamp(void)
{
    if (s_date[1] < 1)  s_date[1] = 1;
    if (s_date[1] > 12) s_date[1] = 12;
    const int mx = days_in_month(s_date[0], s_date[1]);
    if (s_date[2] < 1)  s_date[2] = 1;
    if (s_date[2] > mx) s_date[2] = mx;
}

static int date_digit_get(int pos)
{
    switch (pos) {
    case 0:  return s_date[0];
    case 1:  return s_date[1] / 10;
    case 2:  return s_date[1] % 10;
    case 3:  return s_date[2] / 10;
    default: return s_date[2] % 10;
    }
}

static void date_digit_set(int pos, int v)
{
    switch (pos) {
    case 0:  s_date[0] = v; break;
    case 1:  s_date[1] = v * 10 + (s_date[1] % 10); break;
    case 2:  s_date[1] = (s_date[1] / 10) * 10 + v; break;
    case 3:  s_date[2] = v * 10 + (s_date[2] % 10); break;
    default: s_date[2] = (s_date[2] / 10) * 10 + v; break;
    }
}

// 当前这一位的取值范围（含两端）—— 范围随上位联动，自然排除非法日期
static void date_digit_range(int pos, int *lo, int *hi)
{
    switch (pos) {
    case 0:                                  // 年：整值
        *lo = GANZHI_YEAR_FROM; *hi = GANZHI_YEAR_TO;
        break;
    case 1:
        *lo = 0; *hi = 1;                    // 月十位只能是 0 或 1
        break;
    case 2:
        if (s_date[1] / 10 == 0) { *lo = 1; *hi = 9; }   // 01–09
        else                     { *lo = 0; *hi = 2; }   // 10–12
        break;
    case 3:
        *lo = 0; *hi = days_in_month(s_date[0], s_date[1]) / 10;
        break;
    default: {
        const int mx   = days_in_month(s_date[0], s_date[1]);
        const int tens = s_date[2] / 10;
        *lo = (tens == 0) ? 1 : 0;
        *hi = mx - tens * 10;
        if (*hi < *lo) *hi = *lo;
        break;
    }
    }
}

static void date_refresh(void)
{
    const int box = (s_dpos == 0) ? 0 : (s_dpos <= 2 ? 1 : 2);
    for (int i = 0; i < 3; i++) {
        const bool act = (i == box);
        lv_obj_set_style_border_color(s_frames[i], lv_color_hex(act ? TH_GOLD : TH_DIM), 0);
        lv_obj_set_style_border_width(s_frames[i], act ? 3 : 2, 0);
    }

    lv_label_set_text_fmt(s_digit[0], "%d", s_date[0]);
    lv_label_set_text_fmt(s_digit[1], "%d", s_date[1] / 10);
    lv_label_set_text_fmt(s_digit[2], "%d", s_date[1] % 10);
    lv_label_set_text_fmt(s_digit[3], "%d", s_date[2] / 10);
    lv_label_set_text_fmt(s_digit[4], "%d", s_date[2] % 10);
    for (int i = 0; i < 5; i++) {
        lv_obj_set_style_text_color(s_digit[i],
            lv_color_hex(i == s_dpos ? TH_GOLD : TH_TEXT), 0);
    }

    // 选中位下面一条小金杠（只靠文字颜色区分，小屏上太弱）
    if (s_mark) {
        const int cx[3] = { DATE_YEAR_X + DATE_YEAR_W / 2,
                            DATE_MON_X  + DATE_MON_W  / 2,
                            DATE_DAY_X  + DATE_DAY_W  / 2 };
        const int off = (s_dpos == 0) ? 0 : ((s_dpos % 2) ? -9 : 9);
        lv_obj_set_pos(s_mark, cx[box] + off - DATE_MARK_W / 2, DATE_MARK_Y);
    }

    // 干支预览：让用户当场能跟万年历比对，不用等出结果
    ganzhi_t g;
    if (ganzhi_from_date(s_date[0], s_date[1], s_date[2], &g)) {
        lv_label_set_text_fmt(s_gz, "%s%s年 %s%s月 %s%s日",
            GZ_GAN[g.year_gan], GZ_ZHI[g.year_zhi],
            GZ_GAN[g.month_gan], GZ_ZHI[g.month_zhi],
            GZ_GAN[g.day_gan],   GZ_ZHI[g.day_zhi]);
        lv_obj_set_style_text_color(s_gz, lv_color_hex(TH_GOLD), 0);
    } else {
        lv_label_set_text(s_gz, "年份超出可算范围");
        lv_obj_set_style_text_color(s_gz, lv_color_hex(TH_RED), 0);
    }
}

// ---------------------------------------------------------------------------
// 结果页：一档 = 档名 + 色块 + 色名
// ---------------------------------------------------------------------------
static void tier_show(int y, const char *title, uint32_t title_rgb, const char *colors)
{
    center_label(title, th_font_tiny(), title_rgb, y);

    // 色块：把「甲、乙、丙」按顿号切开，一个词一块
    char buf[80];
    snprintf(buf, sizeof(buf), "%s", colors);
    const char *w[6];
    int n = 0;
    for (char *p = strtok(buf, "、"); p && n < 6; p = strtok(NULL, "、")) w[n++] = p;
    if (n == 0) return;

    const int total = n * DOT_D + (n - 1) * DOT_GAP;
    const int x0 = (APP_SCREEN_W - total) / 2;
    for (int i = 0; i < n; i++) {
        dot_create(x0 + i * (DOT_D + DOT_GAP), y + 20, DOT_D, color_rgb_of(w[i]));
    }

    center_label(colors, th_font_tiny(), TH_TEXT, y + 42);
}

// ---------------------------------------------------------------------------
// 各页
// ---------------------------------------------------------------------------
static void build_welcome(void)
{
    s_page = PAGE_WELCOME;
    s_scr = th_screen_create();

    lv_obj_t *title = th_label(s_scr, "每日穿衣", th_font_hero(), TH_GOLD);
    // 用空格作 ref_char：同字体字形度量统一，空格必在字库里，能取到正确垂直基准
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -28 - th_vshift(th_font_hero(), ' '));

    lv_obj_t *sub = th_label(s_scr, "五  行  择  色", th_font_tiny(), TH_DIM);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 40 - th_vshift(th_font_tiny(), ' '));

    th_hint_create(s_scr, "按 OK 选日期\n长按 OK 切换功能");
    lv_screen_load(s_scr);
}

static void build_date(void)
{
    s_page = PAGE_DATE;
    s_dpos = 0;

    // 先取上次存下的日期；没有存过就用固件编译日期兜底
    if (!app_port_date_load(&s_date[0], &s_date[1], &s_date[2])) {
        static const char *MON[12] = {
            "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
        };
        char mon[4] = { 0 };
        int dd = 1, yy = GANZHI_YEAR_FROM;
        if (sscanf(__DATE__, "%3s %d %d", mon, &dd, &yy) == 3) {
            int mi = 0;
            for (int i = 0; i < 12; i++) {
                if (MON[i][0] == mon[0] && MON[i][1] == mon[1]) { mi = i; break; }
            }
            s_date[0] = yy; s_date[1] = mi + 1; s_date[2] = dd;
        } else {
            s_date[0] = GANZHI_YEAR_FROM; s_date[1] = 1; s_date[2] = 1;
        }
    }
    date_clamp();

    s_scr = th_screen_create();

    lv_obj_t *hdr = th_label(s_scr, "取 当 期 日 期", th_font_small(), TH_DIM);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 32);

    s_gz = th_label(s_scr, "", th_font_tiny(), TH_DIM);
    lv_obj_align(s_gz, LV_ALIGN_TOP_MID, 0, 60);

    const int   xs[3] = { DATE_YEAR_X, DATE_MON_X, DATE_DAY_X };
    const int   ws[3] = { DATE_YEAR_W, DATE_MON_W, DATE_DAY_W };
    const char *nm[3] = { "年", "月", "日" };

    for (int i = 0; i < 3; i++) {
        s_frames[i] = th_frame_create(s_scr, xs[i], DATE_BOX_Y, ws[i], DATE_BOX_H);
        lv_obj_t *cap = th_label(s_scr, nm[i], th_font_tiny(), TH_DIM);
        lv_obj_align(cap, LV_ALIGN_TOP_LEFT, xs[i] + ws[i] / 2 - 7,
                     DATE_BOX_Y + DATE_BOX_H + 8);
    }

    s_digit[0] = th_label(s_frames[0], "2000", th_font_body(), TH_TEXT);
    lv_obj_align(s_digit[0], LV_ALIGN_CENTER, 0, -th_vshift(th_font_body(), '8'));

    // 月 / 日：各两个数字并排，哪一位在编辑就点亮哪一位
    for (int k = 0; k < 2; k++) {
        for (int d = 0; d < 2; d++) {
            lv_obj_t *lb = th_label(s_frames[k + 1], "0", th_font_body(), TH_TEXT);
            lv_obj_align(lb, LV_ALIGN_CENTER, d == 0 ? -9 : 9,
                         -th_vshift(th_font_body(), '8'));
            s_digit[1 + k * 2 + d] = lb;
        }
    }

    s_mark = dot_create(0, DATE_MARK_Y, DATE_MARK_W, TH_GOLD);
    lv_obj_set_size(s_mark, DATE_MARK_W, 2);
    lv_obj_set_style_radius(s_mark, 0, 0);

    date_refresh();
    th_hint_create(s_scr, "上下调数 · OK 下一位\n长按 OK 回退");
    lv_screen_load(s_scr);
}

static void build_result(void)
{
    s_page = PAGE_RESULT;

    if (!dressing_for_date(s_date[0], s_date[1], s_date[2], &s_day, &s_dress)) {
        // 日期页已经拦住非法日期，正常到不了这里；真到了就退回日期页
        build_date();
        return;
    }

    s_scr = th_screen_create();

    lv_obj_t *hdr = th_label(s_scr, "每 日 穿 衣", th_font_small(), TH_DIM);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 12);

    // 日柱 + 「我」的五行 —— 结果页的"依据"要摆在最上面，不然颜色像凭空来的
    static char gzs[32];
    snprintf(gzs, sizeof(gzs), "%s%s日 · 属%s",
             gan_name(s_day.gan), zhi_name(s_day.zhi), s_dress.me);
    center_label(gzs, th_font_body(), TH_GOLD, 40);

    tier_show(TIER_Y0 + 0 * TIER_STEP, "大吉 · 相生助运", TH_GOLD, s_dress.daji);
    tier_show(TIER_Y0 + 1 * TIER_STEP, "次吉 · 比肩共利", TH_TEXT, s_dress.ciji);
    tier_show(TIER_Y0 + 2 * TIER_STEP, "不宜 · 所克难成", TH_RED,  s_dress.buyi);

    th_hint_create(s_scr, "OK 改日期 · 长按回首页");
    lv_screen_load(s_scr);
}

// ---------------------------------------------------------------------------
// 对外接口
// ---------------------------------------------------------------------------
void dressing_enter(void)
{
    s_mark = NULL;
    s_gz = NULL;
    build_welcome();
}

void dressing_exit(void)
{
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
    }
    s_mark = NULL;
    s_gz = NULL;
}

void dressing_key(app_key_t key, app_key_ev_t ev)
{
    switch (s_page) {
    // ---------------- 欢迎页 ----------------
    case PAGE_WELCOME:
        if (key == APP_KEY_OK && ev == APP_KEY_CLICK) {
            dressing_exit();
            build_date();
        } else if (key == APP_KEY_OK && ev == APP_KEY_LONG) {
            dressing_exit();
            app_ui_goto_launcher();
        }
        break;

    // ---------------- 取日期页 ----------------
    case PAGE_DATE:
        if (ev == APP_KEY_CLICK) {
            if (key == APP_KEY_OK) {
                if (s_dpos < 4) {
                    s_dpos++;
                    date_refresh();
                } else {
                    app_port_date_save(s_date[0], s_date[1], s_date[2]);
                    dressing_exit();
                    build_result();
                }
            } else {
                int lo = 0, hi = 0;
                date_digit_range(s_dpos, &lo, &hi);
                int v = date_digit_get(s_dpos) + (key == APP_KEY_UP ? 1 : -1);
                if (v > hi) v = lo;                 // 越界即回绕，转起来顺手
                if (v < lo) v = hi;
                date_digit_set(s_dpos, v);
                date_clamp();
                date_refresh();
            }
        } else if (key == APP_KEY_OK && ev == APP_KEY_LONG) {
            if (s_dpos > 0) {
                s_dpos--;
                date_refresh();
            } else {
                dressing_exit();
                build_welcome();
            }
        }
        break;

    // ---------------- 结果页 ----------------
    case PAGE_RESULT:
        if (key == APP_KEY_OK && ev == APP_KEY_CLICK) {
            dressing_exit();
            build_date();                            // 回去改日期
        } else if (key == APP_KEY_OK && ev == APP_KEY_LONG) {
            dressing_exit();
            build_welcome();
        }
        break;
    }
}
