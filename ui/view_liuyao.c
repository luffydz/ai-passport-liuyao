// ui/view_liuyao.c —— 六爻 App 页面实现（黑金国风，240x320 竖屏）。
//
// 流程：欢迎 → 择事（8 类）→ 取三位数 → 摇卦显爻 → 解卦（4 页）
//
// 按键约定（三键）：
//   UP / DOWN   移动选中项、调节数字、解卦翻页
//   OK 短按     确认 / 进下一位 / 解卦翻页 / 看解卦
//   OK 长按     返回上一层（取数页 = 回退一格）；欢迎页 = 进演示菜单（开发用）
//   OK 按住     摇卦页"蓄力"，松手才摇（对应真机 BSP_BTN_PRESS + 抬起事件）
//
// 注意：所有"文字要在一个框里视觉居中"的地方都用 th_vshift() 做垂直补偿。
#include "view_liuyao.h"
#include "app_ui.h"
#include "ui_theme.h"
#include "hexagram.h"
#include "ganzhi.h"      // 历法层：起卦日期 → 月建/日辰/旬空/六神/旺衰
#include "liuyao_sound.h"

#include <math.h>
#include <stdio.h>       // sscanf：解析 __DATE__ 作首次开机的日期兜底
#include <string.h>      // strstr：判断某爻的纳甲地支是否落在旬空里

#define CAT_COUNT 8
static const char *CAT_NAMES[CAT_COUNT] = {
    "事业", "财运", "感情", "健康", "学业", "出行", "家宅", "其他"
};

typedef enum {
    PAGE_WELCOME = 0,
    PAGE_CATEGORY,
    PAGE_DATE,      // 起卦日期（六爻要"哪天"，本机无 RTC，由用户输入）
    PAGE_CAST,      // 摇卦显爻
    PAGE_RESULT,    // 解卦（5 页）
} page_t;

// ---- 摇卦页布局 ----
#define COIN_D        48
#define COIN_Y        44
#define COIN_CX0      64
#define COIN_CXSTEP   56
#define CHARGE_X      40
#define CHARGE_Y      104
#define CHARGE_W      160
#define CHARGE_H      8
#define LINE_H        14
#define LINE_STEP     20      // 爻间距。原为 26，收紧是为了在底部腾出一行提醒；
                              // 顶爻仍锚在 142（贴着上面的掷币结果），于是省下的
                              // 空间全落在最下面那条爻之下（见 build_cast 底部）。
#define LINE_X        70
#define LINE_W        100
#define LINE_Y0       242     // 第 1 爻（最下面那条）的 y：顶爻 = 242 - 5*20 = 142
#define LINE_RX       126     // 阴爻右半条的 x
#define LINE_RW       44

// 摇卦页最底下两行（16px，字库行高 19）
#define CAST_HINT_Y   260     // "按住 OK 蓄力 · 松手掷币"
#define CAST_TIP_Y    285     // "蓄力大小会影响摇卦解卦"

#define CHARGE_FULL_MS 1200   // 蓄力到头的时间（再按也不会更"满"）
#define FLIP_MS         700   // 摇卦动画时长

// ---- 解卦页 ----
#define RESULT_PAGES   6
#define RESULT_MINI_H  10
#define RESULT_MINI_STEP 18
#define RESULT_MINI_W  80
#define RESULT_MINI_X  80
#define RESULT_MINI_Y0 212    // 最下面一条的 y（往上第 6 条落在 122，避开上/下卦那行字）

// ---- 卦盘页（装卦结果六行表）----
#define CHART_Y0      74      // 上爻那一行的 y（往下每行 +CHART_ROW_H 到初爻）
#define CHART_ROW_H   30
#define CHART_BAR_X   130     // 爻象横条的左端
#define CHART_BAR_W   68
#define CHART_SY_X    204     // 世/应 字的位置
// 左起第一列（六神）原来放在 x=4，压到屏幕圆角金线上了，整体右移 6px
#define CHART_X_SHEN   10
#define CHART_X_QIN    40
#define CHART_X_GZ     76

static lv_obj_t *s_scr;
static page_t    s_page;

// 择事页
static int       s_cat;
static lv_obj_t *s_cat_frames[CAT_COUNT];

// 取日期页
//   5 个"位"，但只画 3 个框：年 1 位（整值）、月 2 位、日 2 位。
//   s_dpos: 0=年 1=月十位 2=月个位 3=日十位 4=日个位
static int       s_dpos;
static int       s_date[3];                       // 年 月 日
static lv_obj_t *s_date_frames[3];                // 年 月 日 三个框
static lv_obj_t *s_date_digit[5];                 // 5 个数字标签（月/日各拆成两位）
static lv_obj_t *s_date_gz;                       // 干支预览（随日期实时更新）
static lv_obj_t *s_date_mark;                     // 选中那一位下面的小金杠
static ganzhi_t  s_gz;                            // 摇卦时锁定的当日干支
static bool      s_gz_ok;                         // 干支是否算得出来（岁超出表范围）

// 摇卦页
//   s_lines / s_moving_mask 是摇卦的【产出】：每次蓄力松手掷三枚铜钱，数出几个
//   背面就定下这一爻（单/拆/重/交），六次攒成一卦。s_upper/s_lower 要等第六次
//   掷完才能由这六爻反查出来 —— 卦是摇出来的，不是提前算好的。
static uint8_t   s_upper, s_lower;
static liuyao_moving_t s_moving_mask; // 动爻掩码：bit0=初爻 … bit5=上爻
static uint8_t   s_lines[6];          // 0=阴 1=阳
static uint8_t   s_coin_back[3];      // 本次掷出的三枚：1=背面 0=字面
static int       s_revealed;          // 已显化爻数 0..6
static bool      s_charging;
static bool      s_flipping;
static uint32_t  s_charge_ms;
static uint32_t  s_flip_ms;
static lv_timer_t *s_timer;
static lv_obj_t *s_hint;
static lv_obj_t *s_cast_txt;          // 摇卦页那行"三背 · 重 · 阳爻发动"
static lv_obj_t *s_vol_label;         // 首页的"音效 xx%"
static int       s_volume = 100;      // 铜钱音效响度 0..100，建首页时从 app_port 读入
static lv_obj_t *s_charge_fill;
static lv_obj_t *s_line_l[6];
static lv_obj_t *s_line_r[6];
static lv_obj_t *s_move_mark[6];      // 每个动爻左边的小方块（现在可能有多个动爻）
// 铜钱：body 是外圈、hole 是方孔、face_l/face_r 是"字面"的两个字。
// 背面是光面（两个字藏起来）—— 数背面就靠这一眼。
struct coin { lv_obj_t *body; lv_obj_t *hole; lv_obj_t *face_l; lv_obj_t *face_r; int cx; };
static struct coin s_coins[3];

// 解卦页
static int       s_result_page;

// 页面构建函数之间有互相调用，先统一声明
static void build_welcome(void);
static void build_category(void);
static void build_date(void);
static void build_cast(void);

// ---------------------------------------------------------------------------
// 小工具
// ---------------------------------------------------------------------------
static lv_obj_t *solid(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    return o;
}

static lv_obj_t *center_label(lv_obj_t *parent, const char *text,
                              const lv_font_t *font, uint32_t color, int y)
{
    lv_obj_t *l = th_label(parent, text, font, color);
    lv_obj_set_width(l, lv_pct(100));
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(l, 0, y - th_vshift(font, 0x56FD /* 国 */));
    return l;
}

// ---------------------------------------------------------------------------
// 欢迎页
// ---------------------------------------------------------------------------
// 铜钱音效响度（0 = 静音），上下键调，百分比显示。
//
// 为什么放在首页：摇卦有时在安静场合（深夜、哄睡、图书馆、开会间隙），需要
// 不出声或很轻，而设备没有别的静音途径。首页是进六爻后的第一屏，一进来就能
// 按上下调好，不必先钻进某页再退出来。
//
// 为什么 10% 一档：三键设备上，100% 共 11 档，从 100 按到 0 是 10 次 —— 可接受；
// 再细（5%）就要按 20 次，反而烦。
#define VOLUME_STEP 10

static void welcome_vol_refresh(void)
{
    if (!s_vol_label) return;
    static char buf[8];
    lv_snprintf(buf, sizeof(buf), "%d%%", s_volume);
    lv_label_set_text(s_vol_label, buf);
}

static void welcome_vol_step(int dir)
{
    // 两端夹住【不回绕】：安静场合手滑从 0 跳到 100 会很尴尬，到顶就停。
    int v = s_volume + dir * VOLUME_STEP;
    if (v < 0)   v = 0;
    if (v > 100) v = 100;
    if (v == s_volume) return;          // 已到端点，不重复写 NVS
    s_volume = v;
    app_port_volume_set(v);             // 立即生效 + 持久化（下次开机保持）
    welcome_vol_refresh();
}

static void build_welcome(void)
{
    s_page = PAGE_WELCOME;
    s_scr = th_screen_create();

    lv_obj_t *title = th_label(s_scr, "六  爻", th_font_hero(), TH_GOLD);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -28 - th_vshift(th_font_hero(), 0x56FD));

    lv_obj_t *sub = th_label(s_scr, "周  易  占  卜", th_font_tiny(), TH_DIM);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 40 - th_vshift(th_font_tiny(), 0x56FD));

    // 响度显示：顶部居中一行小字，只写数字（如 "100%"）。
    // 字体/颜色刻意与底部提示条一致（16px + TH_DIM）——它是"状态读数"，
    // 不该抢标题的注意力，所以放在最上面、做小做暗。
    // 读一次持久化值。真机在这一步就把值写进 codec，
    // 所以开机后第一声摇卦就已经是上次设定的响度。
    s_volume = app_port_volume_get();
    s_vol_label = th_label(s_scr, "", th_font_small(), TH_DIM);
    lv_obj_set_width(s_vol_label, APP_SCREEN_W - 2 * TH_FRAME_INSET - 8);
    lv_obj_set_style_text_align(s_vol_label, LV_TEXT_ALIGN_CENTER, 0);
    // 与底部提示条对称：都让开内缩的金线（TH_FRAME_INSET + 6）
    lv_obj_align(s_vol_label, LV_ALIGN_TOP_MID, 0, TH_FRAME_INSET + 6);
    welcome_vol_refresh();

    th_hint_create(s_scr, "按 OK 起卦  ·  上下调音效");
    lv_screen_load(s_scr);
}

// ---------------------------------------------------------------------------
// 择事页
// ---------------------------------------------------------------------------
static void category_refresh(void)
{
    for (int i = 0; i < CAT_COUNT; i++) {
        th_set_selected(s_cat_frames[i], i == s_cat);
    }
}

static void build_category(void)
{
    s_page = PAGE_CATEGORY;
    // 刻意不重置 s_cat：保留上一次选的事由。
    // 原本每次起卦都悄悄回到「事业」，这也是"每次解卦看起来都差不多"的一部分原因。
    s_scr = th_screen_create();

    lv_obj_t *hdr = th_label(s_scr, "择  事", th_font_body(), TH_GOLD);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 24);

    // 两列横向居中：内容区宽 236，卡片 100x44、列间距 8 → 左右各留 14
    for (int i = 0; i < CAT_COUNT; i++) {
        const int x = 14 + (i % 2) * 108;
        const int y = 64 + (i / 2) * 52;
        s_cat_frames[i] = th_frame_create(s_scr, x, y, 100, 44);

        lv_obj_t *lb = th_label(s_cat_frames[i], CAT_NAMES[i], th_font_body(), TH_TEXT);
        lv_obj_set_width(lb, lv_pct(100));
        lv_obj_set_style_text_align(lb, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(lb, LV_ALIGN_CENTER, 0, -th_vshift(th_font_body(), 0x56FD));
    }
    category_refresh();
    th_hint_create(s_scr, "上下选择   ·   OK 确认");
    lv_screen_load(s_scr);
}

// ---------------------------------------------------------------------------
// 取日期页（六爻要"哪天"：月建定旺衰、日辰定生克）
// ---------------------------------------------------------------------------
// 为什么不用时钟：本机没有 RTC，断电即丢时间，也没有联网对时。所以日期由用户
// 输入，并用 app_port_date_save() 记住 —— 下次开机默认填上次那个，同一天起卦
// 基本只需按 OK 通过。
//
// 输入方式：逐位调数，且每一位的取值范围随上位联动 ——
// 三键设备上没有比这更省按键的做法，同时天然调不出非法日期
// （2月30、4月31 根本到不了），所以不需要"日期无效"的校验提示。
#define DATE_BOX_Y   108
#define DATE_BOX_H    54
#define DATE_YEAR_X   10
#define DATE_YEAR_W   76
#define DATE_MON_X    92
#define DATE_MON_W    62
#define DATE_DAY_X   160
#define DATE_DAY_W    62
#define DATE_MARK_W   16      // 选中位小金杠的宽
#define DATE_MARK_Y  151      // 小金杠的 y（框体 108..162，压在数字下方、仍在框内）

// 该年该月有几天（含闰年判断）
static int days_in_month(int y, int m)
{
    static const int D[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    if (m < 1)  m = 1;
    if (m > 12) m = 12;
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return D[m - 1];
}

static void date_clamp(void)
{
    if (s_date[1] < 1)  s_date[1] = 1;
    if (s_date[1] > 12) s_date[1] = 12;
    const int mx = days_in_month(s_date[0], s_date[1]);
    if (s_date[2] < 1)  s_date[2] = 1;
    if (s_date[2] > mx) s_date[2] = mx;
}

// 取/设第 pos 位（0=年 1=月十 2=月个 3=日十 4=日个）
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

// 当前这一位的取值范围（含两端）—— 范围随上位联动，天然排除非法日期
static void date_digit_range(int pos, int *lo, int *hi)
{
    switch (pos) {
    case 0:                                  // 年：整值 ±1，限制在可算范围内
        *lo = GANZHI_YEAR_FROM; *hi = GANZHI_YEAR_TO;
        break;
    case 1:                                  // 月十位：只能是 0 或 1
        *lo = 0; *hi = 1;
        break;
    case 2:                                  // 月个位：看十位
        if (s_date[1] / 10 == 0) { *lo = 1; *hi = 9; }   // 01–09
        else                     { *lo = 0; *hi = 2; }   // 10–12
        break;
    case 3:                                  // 日十位：0..(当月最大日/10)
        *lo = 0; *hi = days_in_month(s_date[0], s_date[1]) / 10;
        break;
    default: {                               // 日个位：看十位与当月最大日
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
    // 当前编辑的那个框高亮（金框）
    const int box = (s_dpos == 0) ? 0 : (s_dpos <= 2 ? 1 : 2);
    for (int i = 0; i < 3; i++) {
        const bool act = (i == box);
        lv_obj_set_style_border_color(s_date_frames[i],
            lv_color_hex(act ? TH_GOLD : TH_DIM), 0);
        lv_obj_set_style_border_width(s_date_frames[i], act ? 3 : 2, 0);
    }

    lv_label_set_text_fmt(s_date_digit[0], "%d", s_date[0]);
    lv_label_set_text_fmt(s_date_digit[1], "%d", s_date[1] / 10);
    lv_label_set_text_fmt(s_date_digit[2], "%d", s_date[1] % 10);
    lv_label_set_text_fmt(s_date_digit[3], "%d", s_date[2] / 10);
    lv_label_set_text_fmt(s_date_digit[4], "%d", s_date[2] % 10);
    for (int i = 0; i < 5; i++) {
        lv_obj_set_style_text_color(s_date_digit[i],
            lv_color_hex(i == s_dpos ? TH_GOLD : TH_TEXT), 0);
    }

    // 选中那一位再加一条小金杠。
    // 只靠文字颜色区分（金 / 米金）在 240 宽的小屏上太弱 —— 用户反馈"看不出
    // 当前选的是哪一位"。位置全部由框常量算出，不写死坐标：月/日框里两个数字
    // 标签分别偏 ±9（见 build_date 里的 align），所以选中位 = 框中心 ± 9。
    if (s_date_mark) {
        const int cx[3] = { DATE_YEAR_X + DATE_YEAR_W / 2,
                            DATE_MON_X  + DATE_MON_W  / 2,
                            DATE_DAY_X  + DATE_DAY_W  / 2 };
        const int box = (s_dpos == 0) ? 0 : (s_dpos <= 2 ? 1 : 2);
        const int off = (s_dpos == 0) ? 0 : ((s_dpos % 2) ? -9 : 9);
        lv_obj_set_pos(s_date_mark, cx[box] + off - DATE_MARK_W / 2, DATE_MARK_Y);
    }

    // 干支预览：让用户当场就能跟万年历比对，不用等起完卦
    ganzhi_t g;
    if (ganzhi_from_date(s_date[0], s_date[1], s_date[2], &g)) {
        lv_label_set_text_fmt(s_date_gz, "%s%s年 %s%s月 %s%s日",
            GZ_GAN[g.year_gan], GZ_ZHI[g.year_zhi],
            GZ_GAN[g.month_gan], GZ_ZHI[g.month_zhi],
            GZ_GAN[g.day_gan],   GZ_ZHI[g.day_zhi]);
        lv_obj_set_style_text_color(s_date_gz, lv_color_hex(TH_GOLD), 0);
    } else {
        lv_label_set_text(s_date_gz, "年份超出可算范围");
        lv_obj_set_style_text_color(s_date_gz, lv_color_hex(TH_RED), 0);
    }
}

static void build_date(void)
{
    s_page = PAGE_DATE;
    s_dpos = 0;

    // 先取上次存下的日期；没有存过，就用固件编译日期兜底
    // （首次烧录当天使用，约等于"今天"，用户微调即可）
    if (!app_port_date_load(&s_date[0], &s_date[1], &s_date[2])) {
        static const char *MON[12] = {
            "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
        };
        char mon[4] = { 0 };
        int dd = 1, yy = GANZHI_YEAR_FROM;
        if (sscanf(__DATE__, "%3s %d %d", mon, &dd, &yy) == 3) {
            int mi = 0;
            for (int i = 0; i < 12; i++) if (MON[i][0] == mon[0] && MON[i][1] == mon[1]) { mi = i; break; }
            s_date[0] = yy; s_date[1] = mi + 1; s_date[2] = dd;
        } else {
            s_date[0] = GANZHI_YEAR_FROM; s_date[1] = 1; s_date[2] = 1;
        }
    }
    date_clamp();

    s_scr = th_screen_create();

    lv_obj_t *hdr = th_label(s_scr, "取 当 期 日 期", th_font_small(), TH_DIM);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 32);

    s_date_gz = th_label(s_scr, "", th_font_tiny(), TH_DIM);
    lv_obj_align(s_date_gz, LV_ALIGN_TOP_MID, 0, 60);

    const int   xs[3] = { DATE_YEAR_X, DATE_MON_X, DATE_DAY_X };
    const int   ws[3] = { DATE_YEAR_W, DATE_MON_W, DATE_DAY_W };
    const char *nm[3] = { "年", "月", "日" };

    for (int i = 0; i < 3; i++) {
        s_date_frames[i] = th_frame_create(s_scr, xs[i], DATE_BOX_Y, ws[i], DATE_BOX_H);
        lv_obj_t *cap = th_label(s_scr, nm[i], th_font_tiny(), TH_DIM);
        lv_obj_align(cap, LV_ALIGN_TOP_LEFT, xs[i] + ws[i] / 2 - 7,
                     DATE_BOX_Y + DATE_BOX_H + 8);
    }

    // 三个框里的数字统一用 18px：原来 月/日 用 34px，在 240 宽的小屏上
    // 显得又粗又挤，和 年 那一格也不搭。
    s_date_digit[0] = th_label(s_date_frames[0], "2026", th_font_body(), TH_TEXT);
    lv_obj_align(s_date_digit[0], LV_ALIGN_CENTER, 0, -th_vshift(th_font_body(), '8'));

    // 月 / 日：各两个数字标签并排，哪一位在编辑就点亮哪一位
    for (int k = 0; k < 2; k++) {
        for (int d = 0; d < 2; d++) {
            lv_obj_t *lb = th_label(s_date_frames[k + 1], "0", th_font_body(), TH_TEXT);
            lv_obj_align(lb, LV_ALIGN_CENTER, d == 0 ? -9 : 9,
                         -th_vshift(th_font_body(), '8'));
            s_date_digit[1 + k * 2 + d] = lb;
        }
    }

    // 选中指示杠：先建出来（位置由 date_refresh 每次刷新时算）
    s_date_mark = solid(s_scr, 0, DATE_MARK_Y, DATE_MARK_W, 2, TH_GOLD);

    date_refresh();
    th_hint_create(s_scr, "上下调数 · OK 下一位\n长按 OK 回退");
    lv_screen_load(s_scr);
}

// 注：原「取三位数」页已删除。
// 那三个数字本是"让用户有点参与感"，实际却偷偷决定了卦象；现在卦完全由
// 摇卦产生，参与感交给蓄力时长，这一页就没有存在的必要了。

// ---------------------------------------------------------------------------
// 摇卦页
// ---------------------------------------------------------------------------
// 铜钱的两面 —— 摇卦时数"几个背面"就是靠这个分辨：
//   背面 = 光面（只留金圈和方孔，字都藏起来）
//   字面 = 孔左右各一个字（"通""宝"）
static void coin_face_apply(int i, bool back)
{
    if (!s_coins[i].face_l) return;
    if (back) {
        lv_obj_add_flag(s_coins[i].face_l, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_coins[i].face_r, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(s_coins[i].face_l, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_coins[i].face_r, LV_OBJ_FLAG_HIDDEN);
    }
}

static void coin_apply(int i, float factor)
{
    if (factor < 0.12f) factor = 0.12f;      // 侧立时留一点宽度，别完全消失
    const int w = (int)(COIN_D * factor);
    lv_obj_set_size(s_coins[i].body, w, COIN_D);
    lv_obj_set_pos(s_coins[i].body, s_coins[i].cx - w / 2, COIN_Y);
    lv_obj_set_style_radius(s_coins[i].body, w / 2, 0);

    const int hs = (int)(16 * factor);
    lv_obj_set_size(s_coins[i].hole, hs < 3 ? 3 : hs, hs < 3 ? 3 : hs);
    lv_obj_center(s_coins[i].hole);

    // 转到快侧立时把字藏掉 —— 那个角度本来也看不见字面。
    // 只在接近正对观众时才按背面/字面显示，落定后就是最终结果。
    if (factor < 0.55f) {
        lv_obj_add_flag(s_coins[i].face_l, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_coins[i].face_r, LV_OBJ_FLAG_HIDDEN);
    } else {
        coin_face_apply(i, s_coin_back[i] != 0);
    }
}

static void hint_set(const char *txt)
{
    if (s_hint) lv_label_set_text(s_hint, txt);
}

// 画第 i 条爻（i=0 是最下面那条）
static void line_apply(int i)
{
    const int y = LINE_Y0 - i * LINE_STEP;
    const bool revealed = (i < s_revealed);

    lv_obj_set_pos(s_line_l[i], LINE_X, y);
    lv_obj_set_size(s_line_l[i], LINE_W, LINE_H);

    if (!revealed) {
        lv_obj_set_style_bg_color(s_line_l[i], lv_color_hex(0x1A1408), 0);  // 暗金空槽
        lv_obj_set_style_border_color(s_line_l[i], lv_color_hex(TH_DIM), 0);
        lv_obj_set_style_border_width(s_line_l[i], 1, 0);
        lv_obj_add_flag(s_line_r[i], LV_OBJ_FLAG_HIDDEN);
        return;
    }

    // 动爻用亮金，其余用米金
    const bool moving = (s_moving_mask & LIUYAO_MOV(i)) != 0;
    const uint32_t col = moving ? TH_GOLD : TH_TEXT;
    lv_obj_set_style_bg_color(s_line_l[i], lv_color_hex(col), 0);
    lv_obj_set_style_border_width(s_line_l[i], 0, 0);
    lv_obj_set_style_bg_color(s_line_r[i], lv_color_hex(col), 0);
    lv_obj_set_style_border_width(s_line_r[i], 0, 0);

    if (s_lines[i]) {                     // 阳爻：一整条
        lv_obj_set_size(s_line_l[i], LINE_W, LINE_H);
        lv_obj_add_flag(s_line_r[i], LV_OBJ_FLAG_HIDDEN);
    } else {                              // 阴爻：断开成两条
        lv_obj_set_size(s_line_l[i], LINE_RW, LINE_H);
        lv_obj_set_pos(s_line_r[i], LINE_RX, y);
        lv_obj_set_size(s_line_r[i], LINE_RW, LINE_H);
        lv_obj_remove_flag(s_line_r[i], LV_OBJ_FLAG_HIDDEN);
    }

    // 动爻标记（左侧小方块）：这一爻显化出来且确实是动爻时才显示
    if (moving) lv_obj_set_pos(s_move_mark[i], 56, y + (LINE_H - 8) / 2);
}

static void cast_refresh_all(void)
{
    for (int i = 0; i < 6; i++) {
        line_apply(i);
        const bool moving = (s_moving_mask & LIUYAO_MOV(i)) != 0;
        if (moving && i < s_revealed) lv_obj_remove_flag(s_move_mark[i], LV_OBJ_FLAG_HIDDEN);
        else                          lv_obj_add_flag(s_move_mark[i], LV_OBJ_FLAG_HIDDEN);
    }
}


// 33ms 定时器：同时驱动"蓄力"与"摇卦动画"
static void cast_timer_cb(lv_timer_t *t)
{
    (void)t;

    if (s_charging) {
        s_charge_ms += 33;
        if (s_charge_ms > CHARGE_FULL_MS) s_charge_ms = CHARGE_FULL_MS;
        lv_obj_set_width(s_charge_fill, CHARGE_W * (int)s_charge_ms / CHARGE_FULL_MS);

        // 铜钱轻微"发抖"，表示正在蓄力
        const float ph = (float)s_charge_ms / 90.0f;
        for (int i = 0; i < 3; i++) {
            coin_apply(i, 0.94f + 0.06f * sinf(ph + (float)i * 0.9f));
        }
    } else if (s_flipping) {
        s_flip_ms += 33;
        if (s_flip_ms >= FLIP_MS) {
            s_flipping = false;
            for (int i = 0; i < 3; i++) coin_apply(i, 1.0f);

            if (s_revealed < 6) {
                s_revealed++;
                cast_refresh_all();
            }
            if (s_revealed >= 6) {
                // 六爻攒齐 —— 此刻才由这六爻反查出是哪一卦。卦没有别的来源。
                getHexagramFromLines(s_lines, &s_upper, &s_lower);
                hint_set("按 OK 解卦");
            } else {
                hint_set("按住 OK 蓄力 · 松手掷币");
            }
        } else {
            const float t01 = (float)s_flip_ms / (float)FLIP_MS;
            for (int i = 0; i < 3; i++) {
                // 三枚铜钱错开相位，转 3 圈
                const float ph = t01 * 6.2831853f * 3.0f + (float)i * 0.7f;
                coin_apply(i, fabsf(cosf(ph)));
            }
        }
    }
}

// 掷一次三枚铜钱，定下这一爻。
//
// 随机数 = 硬件随机 ^ 本次蓄力时长 —— 既守住传统（三枚铜钱掷一次），
// 又让"手上使了多大劲"真的参与进来：同一个人按两次，蓄力时长不同，卦就不同。
//
// 数出几个背面：
//   一背 = 单（阳爻）     两背 = 拆（阴爻）
//   三背 = 重（阳·发动）  无背 = 交（阴·发动）
static void cast_do_toss(void)
{
    uint32_t r = app_port_random() ^ (s_charge_ms * 2654435761u);
    int backs = 0;
    for (int i = 0; i < 3; i++) {
        r = r * 1103515245u + 12345u;                 // 逐枚演进
        s_coin_back[i] = (uint8_t)((r >> 16) & 1u);   // 1 = 背面
        if (s_coin_back[i]) backs++;
    }

    const int  idx    = s_revealed;
    const bool yang   = (backs == 1 || backs == 3);
    const bool moving = (backs == 0 || backs == 3);   // 交/重 = 老阴/老阳 = 动爻
    s_lines[idx] = yang ? 1 : 0;
    if (moving) s_moving_mask |= LIUYAO_MOV(idx);

    // 结果文字：不必记规则也看得懂这一掷
    static const char *BEI[4]  = { "无背", "一背", "两背", "三背" };
    static const char *NAME[4] = { "交",   "单",   "拆",   "重"   };
    static const char *NOTE[4] = { "阴爻发动", "阳爻", "阴爻", "阳爻发动" };
    if (s_cast_txt) {
        static char buf[40];
        lv_snprintf(buf, sizeof(buf), "%s · %s · %s",
                    BEI[backs], NAME[backs], NOTE[backs]);
        lv_label_set_text(s_cast_txt, buf);
    }
}

static void cast_start_charge(void)
{
    // 摇卦动画还在播时忽略新的按下，否则动画永远走不完
    if (s_revealed >= 6 || s_flipping) return;
    s_charging = true;
    s_charge_ms = 0;
    lv_obj_set_width(s_charge_fill, 0);
    hint_set("松手掷币");
}

static void cast_release(void)
{
    if (!s_charging) return;
    s_charging = false;
    s_flipping = true;
    s_flip_ms = 0;
    lv_obj_set_width(s_charge_fill, 0);   // 蓄力条清空
    hint_set("摇卦中…");
    cast_do_toss();                       // 松手即定这一爻（蓄力时长参与其中）
    liuyao_sound_coin();                  // 铜钱声（非阻塞）
}

static void build_cast(void)
{
    s_page = PAGE_CAST;
    s_revealed = 0;
    s_charging = false;
    s_flipping = false;
    s_charge_ms = 0;
    s_flip_ms = 0;
    s_moving_mask = 0;
    for (int i = 0; i < 6; i++) s_lines[i] = 0;
    for (int i = 0; i < 3; i++) s_coin_back[i] = 0;   // 开局三枚都朝字面

    s_scr = th_screen_create();

    lv_obj_t *hdr = th_label(s_scr, getCategoryName((uint8_t)s_cat), th_font_small(), TH_DIM);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 10);

    // 三枚铜钱：外圈 + 方孔 + "字面"的两个字
    for (int i = 0; i < 3; i++) {
        s_coins[i].cx = COIN_CX0 + i * COIN_CXSTEP;
        s_coins[i].body = lv_obj_create(s_scr);
        lv_obj_remove_flag(s_coins[i].body, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(s_coins[i].body, lv_color_hex(TH_BG), 0);
        lv_obj_set_style_bg_opa(s_coins[i].body, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(s_coins[i].body, lv_color_hex(TH_GOLD), 0);
        lv_obj_set_style_border_width(s_coins[i].body, 2, 0);
        lv_obj_set_style_pad_all(s_coins[i].body, 0, 0);

        s_coins[i].hole = lv_obj_create(s_coins[i].body);
        lv_obj_remove_flag(s_coins[i].hole, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(s_coins[i].hole, lv_color_hex(TH_BG), 0);
        lv_obj_set_style_bg_opa(s_coins[i].hole, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(s_coins[i].hole, lv_color_hex(TH_GOLD), 0);
        lv_obj_set_style_border_width(s_coins[i].hole, 1, 0);
        lv_obj_set_style_radius(s_coins[i].hole, 0, 0);
        lv_obj_set_style_pad_all(s_coins[i].hole, 0, 0);

        // 字面：方孔左右各一个字。背面时这两个字藏起来，
        // 于是"光面 = 背"一眼可辨 —— 数背面不用记规则。
        s_coins[i].face_l = th_label(s_coins[i].body, "通", th_font_tiny(), TH_GOLD);
        lv_obj_align(s_coins[i].face_l, LV_ALIGN_LEFT_MID, 1, 0);
        s_coins[i].face_r = th_label(s_coins[i].body, "宝", th_font_tiny(), TH_GOLD);
        lv_obj_align(s_coins[i].face_r, LV_ALIGN_RIGHT_MID, -1, 0);

        coin_apply(i, 1.0f);
    }

    // 蓄力条
    solid(s_scr, CHARGE_X, CHARGE_Y, CHARGE_W, CHARGE_H, 0x1A1408);
    s_charge_fill = solid(s_scr, CHARGE_X, CHARGE_Y, 0, CHARGE_H, TH_GOLD);

    // 本次掷币的结果（如"三背 · 重 · 阳爻发动"）
    s_cast_txt = th_label(s_scr, "", th_font_tiny(), TH_TEXT);
    lv_obj_set_width(s_cast_txt, 220);
    lv_obj_set_style_text_align(s_cast_txt, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_cast_txt, LV_ALIGN_TOP_MID, 0, 122);

    // 六条爻（从下往上）
    for (int i = 0; i < 6; i++) {
        s_line_l[i] = solid(s_scr, LINE_X, LINE_Y0 - i * LINE_STEP, LINE_W, LINE_H, 0x1A1408);
        s_line_r[i] = solid(s_scr, LINE_RX, LINE_Y0 - i * LINE_STEP, LINE_RW, LINE_H, TH_TEXT);
        lv_obj_add_flag(s_line_r[i], LV_OBJ_FLAG_HIDDEN);
    }
    // 每个动爻左边一个小方块 —— 现在可能有多个动爻
    for (int i = 0; i < 6; i++) {
        s_move_mark[i] = solid(s_scr, 56, 0, 8, 8, TH_GOLD);
        lv_obj_add_flag(s_move_mark[i], LV_OBJ_FLAG_HIDDEN);
    }

    cast_refresh_all();

    // 底部两行。原来只有一行提示、贴屏幕最下沿；现在最下面还要写一句
    // "蓄力会影响卦象"，所以提示条改为按固定 y 排，六爻也顺势收紧（见 LINE_STEP）
    // —— 腾出来的正是这两行的位置。
    //
    // 提示条本身是底部对齐的（th_hint_create 是给"只有一行提示"的页面用的），
    // 这里建完再改对齐，不动那个函数的语义。
    s_hint = th_hint_create(s_scr, "按住 OK 蓄力 · 松手掷币");
    lv_obj_align(s_hint, LV_ALIGN_TOP_MID, 0, CAST_HINT_Y);

    // 提醒：蓄力的轻重会 XOR 进随机数（见 cast_do_toss），它真的会改变卦与解卦，
    // 不是"手感"而已。所以用米金而非提示条的暗色 —— 这条信息要看得清。
    lv_obj_t *tip = th_label(s_scr, "蓄力大小会影响摇卦解卦",
                             th_font_small(), TH_TEXT);
    lv_obj_set_width(tip, APP_SCREEN_W - 2 * TH_FRAME_INSET - 8);
    lv_obj_set_style_text_align(tip, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(tip, LV_ALIGN_TOP_MID, 0, CAST_TIP_Y);

    s_timer = lv_timer_create(cast_timer_cb, 33, NULL);
    lv_screen_load(s_scr);
}

// ---------------------------------------------------------------------------
// 解卦页（5 页）
// ---------------------------------------------------------------------------
// 这一爻是否落在本旬的【旬空】里。
//
// 纳甲字符串形如 "己丑"：第一个汉字是天干、第二个是地支（UTF-8 各占 3 字节），
// 所以拿干支表里的地支去子串匹配即可。
// 旬空需要起卦日期才能算，没设日期时一律返回 false。
static bool line_is_kong(const liuyao_line_t *ln)
{
    if (!ln || !s_gz_ok) return false;
    for (int i = 0; i < 2; i++) {
        if (strstr(ln->ganzhi, GZ_ZHI[s_gz.kong_zhi[i]])) return true;
    }
    return false;
}

// 卦盘页：正统装卦的六行表。
//   每行 = 用神标记 / 六亲 / 纳甲干支+五行 / 爻象 / 世应
//   上爻排最上面，初爻排最下面（传统卦盘画法）。
static void result_build_chart(lv_obj_t *parent, const liuyao_chart_t *ch, int ys)
{
    for (int i = 5; i >= 0; i--) {
        const int y = CHART_Y0 + (5 - i) * CHART_ROW_H;
        const liuyao_line_t *ln = &ch->lines[i];
        const bool is_yong = (i == ys);
        // 动爻用亮金，与本卦页的约定一致
        const uint32_t col = ln->is_moving ? TH_GOLD : TH_TEXT;

        // 六神（按日干起）。没设日期就算不出来，此时整列不画。
        if (s_gz_ok) {
            lv_obj_t *shen = th_label(parent, ganzhi_liu_shen(&s_gz, i),
                                      th_font_tiny(), TH_DIM);
            lv_obj_set_pos(shen, CHART_X_SHEN, y + 1);
        }

        // 用神那一爻的六亲用金色标出（头部已写明用神是谁，靠颜色对号即可）
        lv_obj_t *q = th_label(parent, ln->liuqin, th_font_small(),
                               is_yong ? TH_GOLD : TH_TEXT);
        lv_obj_set_pos(q, CHART_X_QIN, y);

        static char gz[12];
        lv_snprintf(gz, sizeof(gz), "%s%s", ln->ganzhi, ln->wuxing);
        lv_obj_t *g = th_label(parent, gz, th_font_small(),
                               ln->is_moving ? TH_GOLD : TH_DIM);
        lv_obj_set_pos(g, CHART_X_GZ, y);

        if (ln->is_yang) {
            solid(parent, CHART_BAR_X, y + 7, CHART_BAR_W, 8, col);
        } else {
            const int half = (CHART_BAR_W - 12) / 2;
            solid(parent, CHART_BAR_X, y + 7, half, 8, col);
            solid(parent, CHART_BAR_X + half + 12, y + 7, half, 8, col);
        }

        if (ln->shiying) {
            lv_obj_t *s = th_label(parent, ln->shiying == 1 ? "世" : "应",
                                   th_font_small(), TH_GOLD);
            lv_obj_set_pos(s, CHART_SY_X, y);
        }
    }
}

// 把 6 条爻画成小尺寸卦象（解卦页用）
static void result_build_mini(lv_obj_t *parent, const uint8_t lines[6], liuyao_moving_t moving)
{
    for (int i = 0; i < 6; i++) {
        const int y = RESULT_MINI_Y0 - i * RESULT_MINI_STEP;
        const uint32_t col = (moving & LIUYAO_MOV(i)) ? TH_GOLD : TH_TEXT;
        if (lines[i]) {
            solid(parent, RESULT_MINI_X, y, RESULT_MINI_W, RESULT_MINI_H, col);
        } else {
            solid(parent, RESULT_MINI_X, y, (RESULT_MINI_W - 10) / 2, RESULT_MINI_H, col);
            solid(parent, RESULT_MINI_X + (RESULT_MINI_W + 10) / 2, y,
                  (RESULT_MINI_W - 10) / 2, RESULT_MINI_H, col);
        }
    }
}

// 解卦页翻页：整页重建（内容少，重建比增量更新简单可靠）
static void result_show(void)
{
    s_page = PAGE_RESULT;
    HexagramData ben;
    getHexagram(s_upper, s_lower, &ben);
    uint8_t cu = s_upper, cl = s_lower;
    getChangedHexagram(s_upper, s_lower, s_moving_mask, &cu, &cl);
    HexagramData bian;
    getHexagram(cu, cl, &bian);

    // 主爻 = 最高的动爻（变占法多处"以上爻为主"）。六爻安静时为 -1。
    const int mv_main = getPrimaryMovingLine(s_moving_mask);

    // 清掉旧内容（保留外框与提示条由重建统一处理）
    if (s_scr) { lv_obj_delete(s_scr); s_scr = NULL; }

    // 第 5 页"断卦"讲卦说了什么（爻辞 + 本卦/变卦），第 6 页"断语"给四段结论。
    // 拆成两页是因为挤在一页要压到 22px 行距，读起来太密。
    static const char *TITLES[RESULT_PAGES] = { "本 卦", "卦 盘", "卦 辞", "变 卦",
                                                "爻 辞", "断 卦" };
    s_scr = th_screen_create();
    lv_obj_t *hdr = th_label(s_scr, TITLES[s_result_page], th_font_small(), TH_DIM);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 12);

    switch (s_result_page) {
    case 0: {   // 本卦：卦名 + 上下卦 + 卦象
        center_label(s_scr, ben.name, th_font_title(), TH_GOLD, 44);
        static char tri[48];
        lv_snprintf(tri, sizeof(tri), "上 %s   下 %s",
                    getTrigramName(s_upper), getTrigramName(s_lower));
        center_label(s_scr, tri, th_font_small(), TH_TEXT, 90);
        result_build_mini(s_scr, s_lines, s_moving_mask);
        lv_obj_t *note = th_label(s_scr, "亮金为动爻", th_font_tiny(), TH_DIM);
        lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 244);
        break;
    }
    case 1: {   // 卦盘：正统装卦（卦宫 / 世应 / 纳甲 / 六亲 / 用神 / 六神 / 旺衰）
        liuyao_chart_t ch;
        getLiuyaoChart(s_upper, s_lower, s_moving_mask, &ch);
        const int ys = getYongShenLine((uint8_t)s_cat, &ch);

        // 第一行：起卦当天的年月日柱 + 旬空
        static char line1[96];
        if (s_gz_ok) {
            lv_snprintf(line1, sizeof(line1), "%s%s %s%s %s%s · 旬空%s%s",
                        GZ_GAN[s_gz.year_gan],  GZ_ZHI[s_gz.year_zhi],
                        GZ_GAN[s_gz.month_gan], GZ_ZHI[s_gz.month_zhi],
                        GZ_GAN[s_gz.day_gan],   GZ_ZHI[s_gz.day_zhi],
                        GZ_ZHI[s_gz.kong_zhi[0]], GZ_ZHI[s_gz.kong_zhi[1]]);
        } else {
            lv_snprintf(line1, sizeof(line1), "未设起卦日期");
        }
        lv_obj_t *h1 = th_label(s_scr, line1, th_font_tiny(), TH_DIM);
        lv_obj_align(h1, LV_ALIGN_TOP_MID, 0, 34);

        // 第二行：卦宫 + 用神及其旺衰（按【月建】定旺相休囚死）
        static char line2[96];
        if (ys >= 0) {
            const int wx = ganzhi_wuxing_index(ch.lines[ys].wuxing);
            if (s_gz_ok && wx >= 0) {
                lv_snprintf(line2, sizeof(line2), "%s%s 用神%s%s%s",
                            ch.palace_name, ch.palace_wuxing,
                            ch.lines[ys].liuqin, ch.lines[ys].wuxing,
                            ganzhi_wang_shuai(wx, s_gz.month_zhi));
            } else {
                lv_snprintf(line2, sizeof(line2), "%s%s 用神%s",
                            ch.palace_name, ch.palace_wuxing, ch.lines[ys].liuqin);
            }
        } else {
            // 六爻术语：该事由的用神不在此卦中（需另取伏神，本版未实现）
            lv_snprintf(line2, sizeof(line2), "%s%s 用神不上卦",
                        ch.palace_name, ch.palace_wuxing);
        }
        lv_obj_t *h2 = th_label(s_scr, line2, th_font_tiny(), TH_DIM);
        lv_obj_align(h2, LV_ALIGN_TOP_MID, 0, 54);

        result_build_chart(s_scr, &ch, ys);
        break;
    }
    case 2: {   // 卦辞 + 白话
        center_label(s_scr, ben.name, th_font_body(), TH_GOLD, 40);
        lv_obj_t *lab = th_label(s_scr, ben.judgment, th_font_body(), TH_TEXT);
        lv_obj_set_width(lab, 200);
        lv_obj_set_style_text_align(lab, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(lab, LV_ALIGN_TOP_MID, 0, 86);

        lv_obj_t *t2 = th_label(s_scr, "白话", th_font_tiny(), TH_DIM);
        lv_obj_align(t2, LV_ALIGN_TOP_MID, 0, 150);

        lv_obj_t *lab2 = th_label(s_scr, getHexagramInterp(s_upper, s_lower),
                                  th_font_body(), TH_GOLD);
        lv_obj_set_width(lab2, 200);
        lv_obj_set_style_text_align(lab2, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(lab2, LV_ALIGN_TOP_MID, 0, 178);
        break;
    }
    case 3: {   // 变卦
        if (cu == s_upper && cl == s_lower) {
            center_label(s_scr, "六爻安静", th_font_title(), TH_GOLD, 60);
            center_label(s_scr, "本卦即结果\n无变卦", th_font_small(), TH_TEXT, 150);
        } else {
            center_label(s_scr, bian.name, th_font_title(), TH_GOLD, 44);
            lv_obj_t *lab = th_label(s_scr, bian.judgment, th_font_body(), TH_TEXT);
            lv_obj_set_width(lab, 200);
            lv_obj_set_style_text_align(lab, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(lab, LV_ALIGN_TOP_MID, 0, 92);
            lv_obj_t *t = th_label(s_scr, "动爻使本卦变为此卦", th_font_tiny(), TH_DIM);
            lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 140);
        }
        break;
    }
    case 4: {   // 爻辞：按变占法决定读几条 —— 0 条、1 条或 2 条
        // 一爻动读该爻；两爻动两爻都读（以上爻为主，所以从上往下排）；
        // 三爻以上传统就不看爻辞了，转看卦辞。
        int y = 58;
        int shown = 0;
        for (int i = 5; i >= 0 && shown < 2; i--) {
            if (!(s_moving_mask & LIUYAO_MOV(i))) continue;
            shown++;

            static char pos[24];
            lv_snprintf(pos, sizeof(pos), "%s 动",
                        getLineName((uint8_t)i, s_lines[i] != 0));
            center_label(s_scr, pos, th_font_body(), TH_GOLD, y);
            y += 28;

            lv_obj_t *lab = th_label(s_scr, getLineJudgment(s_upper, s_lower, (uint8_t)i),
                                     th_font_body(), TH_TEXT);
            lv_obj_set_width(lab, 224);
            lv_obj_set_style_text_align(lab, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(lab, LV_ALIGN_TOP_MID, 0, y);
            y += 28;

            // 爻辞白话：原文（米金，偏暗）下面紧跟一句人话（亮金）。
            // 宽度 224px ÷ 18px = 12 字，所以每条白话都 ≤12 字。
            lv_obj_t *pl = th_label(s_scr, getLineJudgmentPlain(s_upper, s_lower, (uint8_t)i),
                                    th_font_body(), TH_GOLD);
            lv_obj_set_width(pl, 224);
            lv_obj_set_style_text_align(pl, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(pl, LV_ALIGN_TOP_MID, 0, y);
            y += 40;                      // 两组之间留一点空
        }

        const bian_zhan_t rule = getBianZhanRule(s_moving_mask);
        if (shown == 0) {
            // 本页没有爻辞可列 —— 用【本页自己的话】讲清"为什么空"。
            //
            // 刻意不搬"断卦"页那句读法规则（如"六爻安静，看本卦卦辞"）：两页紧挨着，
            // 同一句话读起来就是重复。读法的权威说明统一放末页"断卦"，因为那一页
            // 在 0~6 动【任何】情况下都会显示，是唯一的归纳位；本页只管自己。
            center_label(s_scr, "六爻安静，没有动爻",
                         th_font_body(), TH_GOLD, 96);
        } else if (rule != BIAN_ZHAN_ONE_YAO && rule != BIAN_ZHAN_TWO_YAO) {
            // 三爻以上：上面那个循环最多只列两条（shown < 2），所以这里必须说明
            // "没列全"，否则用户会以为动爻只有两个。该读什么由末页"断卦"统一讲。
            //
            // 注意这必须写成 else if —— 三爻以上时 shown 恒为 2，永远进不了上面
            // 那个 shown == 0 分支，写成那里就是死代码（漏过一版，已修）。
            center_label(s_scr, "动爻较多，此处只列两爻",
                         th_font_body(), TH_DIM, y + 6);
        }

        // 六爻皆动时，乾、坤有专用辞（用九 / 用六）
        if (rule == BIAN_ZHAN_SIX && getYongJiuYongLiu(s_upper, s_lower)[0] != 0) {
            center_label(s_scr, getYongJiuYongLiu(s_upper, s_lower),
                         th_font_body(), TH_TEXT, y + 30);
            center_label(s_scr, getYongJiuYongLiuPlain(s_upper, s_lower),
                         th_font_body(), TH_GOLD, y + 58);
        }
        break;
    }
    default: {  // 断卦：变占说明 + 现状/趋势 + 力量/虚实/双方/建议（"答案"在这一页）
        // 开头一行把"这一卦该看哪儿"讲明白 —— 是传统规则，也省得用户猜。
        //
        // 用【米金 TH_TEXT】：这一行是"读法指示"，得一眼读到 —— 原来的 TH_DIM
        // (橄榄棕) 明度只有米金一半、几乎无彩，在纯黑底上像脏灰。米金又与下面
        // 暖金色的结论（TH_GOLD）分得开：一眼能分出"这行是说明、下面是结论"。
        center_label(s_scr, getBianZhanText(s_upper, s_lower, s_moving_mask),
                     th_font_body(), TH_TEXT, 40);

        // 现状 / 趋势：由【主爻】决定；六爻安静时没有"走向"可说，留空
        static char v1[64], v2[80], v3[64], v4[64], v5[64];
        v1[0] = 0;
        if (mv_main >= 0) {
            lv_snprintf(v1, sizeof(v1), "%s，%s，",
                        getLinePosPlain((uint8_t)mv_main),
                        getElemRelationPlain(s_upper, s_lower, s_moving_mask));
        }

        // ★ 每行都随卦/随日/随事由变，而且都要说人话。
        //   力量：旺衰(5) × 日辰作用(5)，按【月建/日辰】算   = 25 种
        //   虚实：用神是否落旬空                            = 2 种
        //   双方：世（自己）与应（对方）的五行生克           = 5 种
        //   建议：事由(8) × 用神旺衰档(5)                   = 40 种
        liuyao_chart_t ch;
        getLiuyaoChart(s_upper, s_lower, s_moving_mask, &ch);
        const int ys = getYongShenLine((uint8_t)s_cat, &ch);
        int str_level = -1;                  // 用神旺衰档位：0旺 1相 2休 3囚 4死
        v2[0] = 0;
        if (s_gz_ok && ys >= 0) {
            const int wx = ganzhi_wuxing_index(ch.lines[ys].wuxing);
            if (wx >= 0) {
                // 例：势头偏弱，今日受制，
                lv_snprintf(v2, sizeof(v2), "%s，%s，",
                            ganzhi_wang_shuai_plain(wx, s_gz.month_zhi),
                            ganzhi_day_effect_plain(wx, &s_gz));
                str_level = ganzhi_wang_shuai_level(wx, s_gz.month_zhi);
            }
        }

        // 虚实：用神是否落在【旬空】里。六爻规矩「动不为空」—— 用神自己发动时
        // 不当空论，所以先排除动爻。白话只说"悬着 / 有着落"，不写成凶或吉。
        v3[0] = 0;
        if (s_gz_ok && ys >= 0) {
            const bool kong = !ch.lines[ys].is_moving && line_is_kong(&ch.lines[ys]);
            lv_snprintf(v3, sizeof(v3), "%s",
                        kong ? "此事悬空，暂难落实，" : "此事有着落，");
        }

        // 双方：世（你自己）与应（对方）的五行生克
        lv_snprintf(v4, sizeof(v4), "%s", getShiYingPlain(&ch));

        // 建议：按【用神旺衰】取句 —— 旺则放手、死则停手，与"力量"那行同源
        lv_snprintf(v5, sizeof(v5), "%s",
                    getCategoryAdvice((uint8_t)s_cat, str_level));

        const char *lines[5] = { v1, v2, v3, v4, v5 };
        for (int i = 0; i < 5; i++) {
            if (!lines[i][0]) continue;
            lv_obj_t *t = th_label(s_scr, lines[i], th_font_body(), TH_GOLD);
            lv_obj_set_width(t, 220);
            lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
            // 五行、行距 30px：从 74 排到 194，这一页位置宽裕
            lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 74 + i * 30);
        }
        break;
    }
    }

    static char hint[40];
    lv_snprintf(hint, sizeof(hint), "%d/%d · 上下翻页 · 长按回首页",
                s_result_page + 1, RESULT_PAGES);
    th_hint_create(s_scr, hint);

    lv_screen_load(s_scr);
}

// ---------------------------------------------------------------------------
// 对外接口
// ---------------------------------------------------------------------------
void liuyao_enter(void)
{
    build_welcome();
}

void liuyao_exit(void)
{
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
    }
    s_hint = NULL;
    s_charge_fill = NULL;
    s_cast_txt = NULL;
    for (int i = 0; i < 6; i++) s_move_mark[i] = NULL;
    s_date_gz = NULL;
    for (int i = 0; i < 6; i++) { s_line_l[i] = s_line_r[i] = NULL; }
    for (int i = 0; i < 5; i++) { s_date_digit[i] = NULL; }
    for (int i = 0; i < 3; i++) {
        s_coins[i].body = s_coins[i].hole = NULL;
        s_date_frames[i] = NULL;
    }
}

void liuyao_key(app_key_t key, app_key_ev_t ev)
{
    switch (s_page) {
    // ---------------- 欢迎页 ----------------
    case PAGE_WELCOME:
        if (ev == APP_KEY_CLICK) {
            if (key == APP_KEY_UP) {
                welcome_vol_step(+1);        // 调响，10% 一档
            } else if (key == APP_KEY_DOWN) {
                welcome_vol_step(-1);        // 调轻，按到 0 就是静音
            } else if (key == APP_KEY_OK) {
                liuyao_exit();
                build_category();
            }
        } else if (key == APP_KEY_OK && ev == APP_KEY_LONG) {
            liuyao_exit();
            app_ui_enter_menu();
        }
        break;

    // ---------------- 择事页 ----------------
    case PAGE_CATEGORY:
        if (ev == APP_KEY_CLICK) {
            if (key == APP_KEY_UP) {
                s_cat = (s_cat + CAT_COUNT - 1) % CAT_COUNT;
                category_refresh();
            } else if (key == APP_KEY_DOWN) {
                s_cat = (s_cat + 1) % CAT_COUNT;
                category_refresh();
            } else if (key == APP_KEY_OK) {
                liuyao_exit();
                build_date();          // 流程：择事 → 取日期 → 取数 → 摇卦
            }
        } else if (key == APP_KEY_OK && ev == APP_KEY_LONG) {
            liuyao_exit();
            build_welcome();
        }
        break;

    // ---------------- 取日期页 ----------------
    case PAGE_DATE:
        if (ev == APP_KEY_CLICK) {
            if (key == APP_KEY_OK) {
                if (s_dpos < 4) {              // 还有下一位
                    s_dpos++;
                    date_refresh();
                } else {                        // 最后一位按 OK = 确认
                    app_port_date_save(s_date[0], s_date[1], s_date[2]);   // 记住，下次默认
                    s_gz_ok = ganzhi_from_date(s_date[0], s_date[1], s_date[2], &s_gz);
                    liuyao_exit();
                    build_cast();               // 日期定了就直接摇卦（不再填数字）
                }
            } else {
                // 上下调数：取值范围随上位联动（见 date_digit_range）
                int lo = 0, hi = 0;
                date_digit_range(s_dpos, &lo, &hi);
                int v = date_digit_get(s_dpos) + (key == APP_KEY_UP ? 1 : -1);
                if (v > hi) v = lo;                // 越界即回绕，转起来顺手
                if (v < lo) v = hi;
                date_digit_set(s_dpos, v);
                date_clamp();                      // 月/日变化后保证仍是合法日期
                date_refresh();
            }
        } else if (key == APP_KEY_OK && ev == APP_KEY_LONG) {
            if (s_dpos > 0) {                      // 退回上一位
                s_dpos--;
                date_refresh();
            } else {
                liuyao_exit();
                build_category();                  // 退回上一页：择事
            }
        }
        break;

    // ---------------- 摇卦页 ----------------
    case PAGE_CAST:
        if (key == APP_KEY_OK && ev == APP_KEY_PRESS) {
            cast_start_charge();
        } else if (key == APP_KEY_OK && (ev == APP_KEY_CLICK || ev == APP_KEY_LONG)) {
            if (s_revealed >= 6 && !s_flipping) {
                liuyao_exit();           // 停定时器 + 删屏
                s_result_page = 0;
                result_show();
            } else {
                cast_release();
            }
        }
        break;

    // ---------------- 解卦页 ----------------
    case PAGE_RESULT:
        if (ev == APP_KEY_CLICK) {
            if (key == APP_KEY_UP) {
                s_result_page = (s_result_page + RESULT_PAGES - 1) % RESULT_PAGES;
                result_show();
            } else if (key == APP_KEY_DOWN) {
                s_result_page = (s_result_page + 1) % RESULT_PAGES;
                result_show();
            } else if (key == APP_KEY_OK) {
                s_result_page = (s_result_page + 1) % RESULT_PAGES;
                result_show();
            }
        } else if (key == APP_KEY_OK && ev == APP_KEY_LONG) {
            liuyao_exit();
            build_welcome();             // 长按回首页（原来退回"择事"，够不到首页）
        }
        break;
    }
}
