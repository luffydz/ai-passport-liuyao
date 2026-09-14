// sim/host/sim_fonts.c —— ui_theme.h 里声明的字体在【模拟器侧】的实现。
//
// 真机侧由固件提供同名函数，返回编译进 flash 的静态字库（走 XIP，不占 RAM）。
// 模拟器这里用 FreeType 直接加载 OTF/TTF —— 优点是改字号/换字体零成本，
// 适合快速迭代布局；定稿后再用 lv_font_conv 生成静态字库给固件。
#include "sim_fonts.h"
#include "ui_theme.h"

#include "lvgl.h"
#include "src/libs/freetype/lv_freetype.h"   // LVGL 的 FreeType 封装

#include <stdio.h>

// 相对路径 —— FreeType 直接 fopen，相对于运行目录（sim/）
#define FONT_BODY_PATH  "../assets/fonts/SourceHanSansCN-Medium.otf"
#define FONT_TITLE_PATH "../assets/fonts/ZiHunTaiAKaiShu.ttf"

// 字号选择依据：经验上中文在 16px 以下笔画会开始糊（楷体更明显），
// 所以正文/提示尽量不小于 16px；仅副标题这类装饰性文字用到 14px。
// 240x320 竖屏比原 240x135 宽裕得多，主标题可以做到 48px。
#define SIZE_HERO  48
#define SIZE_TITLE 34
#define SIZE_BODY  18
#define SIZE_SMALL 16
#define SIZE_TINY  14

static lv_font_t *s_hero;
static lv_font_t *s_title;
static lv_font_t *s_body;
static lv_font_t *s_small;
static lv_font_t *s_tiny;

static lv_font_t *mk(const char *path, int size)
{
    return lv_freetype_font_create(path, LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
                                   (uint32_t)size, LV_FREETYPE_FONT_STYLE_NORMAL);
}

void sim_fonts_init(void)
{
    s_hero  = mk(FONT_TITLE_PATH, SIZE_HERO);   // 大标题用楷书
    s_title = mk(FONT_TITLE_PATH, SIZE_TITLE);
    s_body  = mk(FONT_BODY_PATH,  SIZE_BODY);
    s_small = mk(FONT_BODY_PATH,  SIZE_SMALL);
    s_tiny  = mk(FONT_BODY_PATH,  SIZE_TINY);

    printf("  [font] 48px=%s 34px=%s 18px=%s 16px=%s 14px=%s\n",
           s_hero ? "OK" : "失败", s_title ? "OK" : "失败",
           s_body ? "OK" : "失败", s_small ? "OK" : "失败", s_tiny ? "OK" : "失败");
    if (!s_hero || !s_title || !s_body || !s_small || !s_tiny) {
        fprintf(stderr,
                "  [font] 字体加载失败！检查：assets/fonts/ 是否存在、"
                "LV_USE_FREETYPE 是否打开、运行目录是否为 sim/\n");
    }
}

const lv_font_t *th_font_hero(void)  { return s_hero; }
const lv_font_t *th_font_title(void) { return s_title; }
const lv_font_t *th_font_body(void)  { return s_body; }
const lv_font_t *th_font_small(void) { return s_small; }
const lv_font_t *th_font_tiny(void)  { return s_tiny; }
