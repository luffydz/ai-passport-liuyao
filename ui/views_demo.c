// ui/views_demo.c —— 三个演示页的实现。
//
// 注意：本文件只 include lvgl.h / ui_pixel.h / app_port.h，
//      不含任何 ESP-IDF 头文件，所以固件与模拟器都能直接编译。
// 文字一律用 ASCII —— 真机与模拟器当前都没有中文字库（Montserrat 只有拉丁字形）。
#include "views_demo.h"
#include "ui_pixel.h"
#include "lvgl.h"

#include <stdio.h>

// ---------------------------------------------------------------------------
// 页面公共骨架
// ---------------------------------------------------------------------------
static lv_obj_t *s_scr;      // 当前页的根屏
static lv_obj_t *s_panel;    // 主面板（Display 页要改它的底色）
static lv_obj_t *s_body;     // 面板里的说明文字
static lv_obj_t *s_mascot;

static lv_obj_t *page_open(const char *title, uint32_t panel_color, uint32_t text_color)
{
    s_scr = ui_pixel_screen_create(title);
    s_panel = ui_pixel_panel_create(s_scr, 18, 58, 204, 188, panel_color);
    s_body = ui_pixel_label(s_panel, "", &lv_font_montserrat_14, text_color);
    lv_obj_set_style_text_align(s_body, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_body);
    s_mascot = ui_pixel_mascot_create(s_scr, 101, 238);
    return s_panel;
}

static void page_close(void)
{
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
    }
    s_panel = s_body = s_mascot = NULL;
}

// ===========================================================================
// Display —— 色块循环 + 背光档位
// ===========================================================================
static const uint32_t DISPLAY_COLORS[] = { 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF, 0x000000 };
static const char    *DISPLAY_NAMES[]  = { "RED", "GREEN", "BLUE", "WHITE", "BLACK" };
static const int      BACKLIGHT_LEVELS[] = { 100, 50, 10 };
#define DISPLAY_COLOR_COUNT ((int)(sizeof(DISPLAY_COLORS) / sizeof(DISPLAY_COLORS[0])))
#define BACKLIGHT_COUNT     ((int)(sizeof(BACKLIGHT_LEVELS) / sizeof(BACKLIGHT_LEVELS[0])))

static int s_color_idx;
static int s_bl_idx;

static void display_refresh(void)
{
    lv_obj_set_style_bg_color(s_panel, lv_color_hex(DISPLAY_COLORS[s_color_idx]), 0);
    // 深底用白字、浅底用墨字，保证任何色块上都看得见
    bool dark_bg = (DISPLAY_COLORS[s_color_idx] == 0x0000FF || DISPLAY_COLORS[s_color_idx] == 0x000000);
    lv_obj_set_style_text_color(s_body,
        dark_bg ? lv_color_white() : lv_color_hex(UI_INK), 0);
    lv_label_set_text_fmt(s_body, "%s\n\n\nBACKLIGHT %d%%\n\n\nOK: NEXT COLOR\nUP/DOWN: LIGHT",
                          DISPLAY_NAMES[s_color_idx], BACKLIGHT_LEVELS[s_bl_idx]);
}

void view_display_enter(void)
{
    s_color_idx = 0;
    s_bl_idx = 0;
    app_port_backlight_set(BACKLIGHT_LEVELS[s_bl_idx]);
    page_open("DISPLAY", DISPLAY_COLORS[s_color_idx], UI_INK);
    display_refresh();
    lv_screen_load(s_scr);
}

void view_display_exit(void)
{
    app_port_backlight_set(100);   // 退出恢复全亮，免得菜单看不见
    page_close();
}

void view_display_key(app_key_t key, app_key_ev_t ev)
{
    if (ev != APP_KEY_CLICK) return;
    if (key == APP_KEY_OK) {
        s_color_idx = (s_color_idx + 1) % DISPLAY_COLOR_COUNT;
        ui_pixel_mascot_jump(s_mascot);
    } else {
        s_bl_idx = (key == APP_KEY_UP) ? (s_bl_idx + BACKLIGHT_COUNT - 1) % BACKLIGHT_COUNT
                                       : (s_bl_idx + 1) % BACKLIGHT_COUNT;
        app_port_backlight_set(BACKLIGHT_LEVELS[s_bl_idx]);
    }
    display_refresh();
}

// ===========================================================================
// Battery —— 电量读数
// ===========================================================================
static void battery_refresh(void)
{
    int pct = app_port_battery_percent();
    if (pct < 0) {
        // 读数不可用时优雅降级，不画数字
        lv_label_set_text(s_body, "BATTERY\n\n\nN/A\n\n\nOK: REFRESH");
    } else {
        lv_label_set_text_fmt(s_body, "BATTERY\n\n\n%d%%\n\n\nOK: REFRESH", pct);
    }
}

void view_battery_enter(void)
{
    page_open("BATTERY", UI_PAPER, UI_INK);
    battery_refresh();
    lv_screen_load(s_scr);
}

void view_battery_exit(void) { page_close(); }

void view_battery_key(app_key_t key, app_key_ev_t ev)
{
    if (ev == APP_KEY_CLICK && key == APP_KEY_OK) {
        ui_pixel_mascot_jump(s_mascot);
        battery_refresh();
    }
}

// ===========================================================================
// Button —— 显示最后一次按键事件（验证输入链路）
// ===========================================================================
static void button_refresh(app_key_t key, app_key_ev_t ev, bool first)
{
    if (first) {
        lv_label_set_text(s_body, "BUTTON\n\n\nLAST: -\n\n\nUP/DOWN/OK\nto test");
        return;
    }
    const char *kname = (key == APP_KEY_UP) ? "UP" : (key == APP_KEY_DOWN) ? "DOWN" : "OK";
    const char *ename = (ev == APP_KEY_LONG) ? "LONG" : "CLICK";
    lv_label_set_text_fmt(s_body, "BUTTON\n\n\nLAST: %s\n%s\n\n\nOK LONG = back", kname, ename);
}

void view_button_enter(void)
{
    page_open("BUTTON", UI_PAPER, UI_INK);
    button_refresh(APP_KEY_OK, APP_KEY_CLICK, true);
    lv_screen_load(s_scr);
}

void view_button_exit(void) { page_close(); }

void view_button_key(app_key_t key, app_key_ev_t ev)
{
    ui_pixel_mascot_jump(s_mascot);
    button_refresh(key, ev, false);
}
