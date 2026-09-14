// ui/ui_theme.c —— 六爻黑金主题实现（纯 LVGL，双端可编）。
#include "ui_theme.h"
#include "app_port.h"

// 原设计稿是 240x135 的横屏，线宽 1px；放到 240x320 竖屏上要加粗到 2px，
// 否则细线在大屏上会显得发虚、发空。
#define TH_BORDER_W 2

lv_obj_t *th_screen_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(TH_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    // 纯黑满屏底：金线以外的区域全是黑的。屏幕四角那点直角黑区在设备上
    // 正好被物理边框盖住，看不到 —— 所以不需要把底色也做成圆角。
    lv_obj_set_style_bg_color(scr, lv_color_hex(TH_BG), 0);
    lv_obj_set_style_radius(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);

    // 圆角金线：一个内缩的透明矩形，只画边框（不填充）。
    // 最先创建 → z 序最低，不会挡住后续添加的页面内容。
    lv_obj_t *frame = lv_obj_create(scr);
    lv_obj_remove_flag(frame, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(frame, TH_FRAME_INSET, TH_FRAME_INSET + TH_FRAME_DY);
    lv_obj_set_size(frame,
                    APP_SCREEN_W - 2 * TH_FRAME_INSET,
                    APP_SCREEN_H - 2 * TH_FRAME_INSET);
    lv_obj_set_style_bg_opa(frame, LV_OPA_TRANSP, 0);      // 纯描边，不遮底
    lv_obj_set_style_border_color(frame, lv_color_hex(TH_FRAME), 0);
    lv_obj_set_style_border_width(frame, TH_FRAME_LINE_W, 0);
    lv_obj_set_style_border_opa(frame, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(frame, TH_SCREEN_RADIUS, 0);
    lv_obj_set_style_pad_all(frame, 0, 0);

    return scr;
}

lv_obj_t *th_frame_create(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(TH_BG), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, 6, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(TH_FRAME), 0);
    lv_obj_set_style_border_width(obj, TH_BORDER_W, 0);
    lv_obj_set_style_pad_all(obj, 6, 0);
    return obj;
}

lv_obj_t *th_label(lv_obj_t *parent, const char *text,
                   const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

void th_set_selected(lv_obj_t *obj, bool selected)
{
    lv_obj_set_style_border_color(obj,
        lv_color_hex(selected ? TH_GOLD : TH_DIM), 0);
    lv_obj_set_style_border_width(obj, selected ? TH_BORDER_W + 1 : TH_BORDER_W, 0);
}

int th_vshift(const lv_font_t *font, uint32_t ref_char)
{
    if (!font) return 0;

    lv_font_glyph_dsc_t g;
    if (!lv_font_get_glyph_dsc(font, &g, ref_char, 0)) return 0;

    // 字形盒顶 = label顶 + (line_height - base_line) - box_h - ofs_y
    // → 墨迹中心（相对 label 顶）= (line_height - base_line) - box_h/2 - ofs_y
    // → 与盒中心 line_height/2 的差就是需要向上补偿的量
    const int shift = (int)font->line_height / 2
                    - (int)font->base_line
                    - (int)g.box_h / 2
                    - (int)g.ofs_y;
    return shift;
}

lv_obj_t *th_hint_create(lv_obj_t *parent, const char *text)
{
    lv_obj_t *label = th_label(parent, text, th_font_small(), TH_DIM);
    // 宽度让开两侧金边，避免长提示文字压到金色留边上
    lv_obj_set_width(label, APP_SCREEN_W - 2 * TH_FRAME_INSET - 8);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    // 贴底提示条要让开下方金边：内容区下沿在 APP_SCREEN_H - TH_FRAME_INSET - TH_FRAME_DY，
    // 这里(屏幕底 -TH_FRAME_INSET-6) 落在那之上，留出几像素干净间距。
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -(TH_FRAME_INSET + 6));
    return label;
}
