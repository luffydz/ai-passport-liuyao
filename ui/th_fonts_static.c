// ui/th_fonts_static.c —— ui_theme.h 里声明的字体在【固件侧】的实现。
//
// 返回编译进 flash 的静态字库（由 tools/gen_fonts.py 生成，见 ui/fonts/）。
// 只读数据走 XIP，不占 RAM —— 真机无 PSRAM，这是唯一可行的方案。
//
// 模拟器默认不用本文件（它用 FreeType 动态加载 OTF，方便改字号迭代）；
// 想验证静态字库，编译时加 -DSIM_USE_STATIC_FONTS=1 即可切到这里。
#include "ui_theme.h"

// 生成的字库文件（ui/fonts/*.c）各自定义这些符号
extern const lv_font_t lv_font_kai_48;    // 欢迎页大标题（楷书）
extern const lv_font_t lv_font_kai_34;    // 次要大字（楷书，日期/结果用）
extern const lv_font_t lv_font_sans_18;   // 正文（黑体）
extern const lv_font_t lv_font_sans_16;   // 底部提示条（黑体）
extern const lv_font_t lv_font_sans_14;   // 次要说明（黑体）

const lv_font_t *th_font_hero(void)  { return &lv_font_kai_48; }
const lv_font_t *th_font_title(void) { return &lv_font_kai_34; }
const lv_font_t *th_font_body(void)  { return &lv_font_sans_18; }
const lv_font_t *th_font_small(void) { return &lv_font_sans_16; }
const lv_font_t *th_font_tiny(void)  { return &lv_font_sans_14; }
