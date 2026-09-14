// ui/app_ui.c —— 应用外壳实现。
//
// 两层结构：
//   ① 六爻 App（默认界面）      ui/view_liuyao.c
//   ② 硬件演示菜单（开发用）    本文件，藏着 7 个基线演示页
//
// 顶层用 s_mode 分流按键；演示菜单里的页面逻辑与基线 main/main.c 的菜单一致。
#include "app_ui.h"
#include "view.h"
#include "ui_pixel.h"
#include "views_demo.h"
#include "view_liuyao.h"
#include "standby.h"
#include "lvgl.h"

#include <stddef.h>

// ---------------------------------------------------------------------------
// 演示菜单的页面表（与基线 main.c 的 DEMOS[] 同序）
// ---------------------------------------------------------------------------
static const app_view_t VIEWS[] = {
    { "Display",   true,  view_display_enter, view_display_exit, view_display_key },
    { "Button",    true,  view_button_enter,  view_button_exit,  view_button_key  },
    { "Audio",     false, NULL,               NULL,               NULL             },
    { "Battery",   true,  view_battery_enter, view_battery_exit, view_battery_key  },
    { "Wi-Fi",     false, NULL,               NULL,               NULL             },
    { "BLE",       false, NULL,               NULL,               NULL             },
    { "Low Power", false, NULL,               NULL,               NULL             },
};
#define VIEW_COUNT ((int)(sizeof(VIEWS) / sizeof(VIEWS[0])))

typedef enum {
    APP_MODE_LIUYAO = 0,   // 六爻 App（默认）
    APP_MODE_MENU,         // 硬件演示菜单（开发用）
} app_mode_t;

static app_mode_t s_mode = APP_MODE_LIUYAO;

static lv_obj_t *s_menu_scr;
static lv_obj_t *s_cards[VIEW_COUNT];
static lv_obj_t *s_rows[VIEW_COUNT];
static lv_obj_t *s_mascot;
static int s_sel;         // 菜单当前选中项
static int s_active = -1; // 菜单里当前所在演示页；-1 = 在菜单

// ---------------------------------------------------------------------------
// 演示菜单
// ---------------------------------------------------------------------------
static void menu_refresh(void)
{
    for (int i = 0; i < VIEW_COUNT; i++) {
        const bool ok = VIEWS[i].enabled;
        lv_label_set_text(s_rows[i], VIEWS[i].name);
        ui_pixel_set_selected(s_cards[i], i == s_sel, ok);
        lv_obj_set_style_text_color(s_rows[i],
            ok ? lv_color_hex(UI_INK) : lv_color_hex(0x546E7A), 0);
    }
}

static void menu_build(void)
{
    s_menu_scr = ui_pixel_screen_create("FoloToy");

    for (int i = 0; i < VIEW_COUNT; i++) {
        const int x = 11 + (i % 2) * 112;
        const int y = 52 + (i / 2) * 47;
        s_cards[i] = ui_pixel_panel_create(s_menu_scr, x, y, 102, 40, UI_PAPER);
        s_rows[i] = lv_label_create(s_cards[i]);
        lv_obj_set_style_text_font(s_rows[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_align(s_rows[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(s_rows[i]);
    }

    s_mascot = ui_pixel_mascot_create(s_menu_scr, 101, 242);
    menu_refresh();
    lv_screen_load(s_menu_scr);
}

static void menu_teardown(void)
{
    if (s_menu_scr) {
        lv_obj_delete(s_menu_scr);
        s_menu_scr = NULL;
    }
    s_mascot = NULL;
}

// ---------------------------------------------------------------------------
// 对外接口
// ---------------------------------------------------------------------------
void app_ui_start(void)
{
    if (!app_port_lvgl_lock(1000)) return;
    liuyao_enter();          // 直接进六爻 App
    standby_init();          // 待机图层：建在最上层，跨页面存活
    app_port_lvgl_unlock();
}

void app_ui_enter_menu(void)
{
    // 调用方已持有 LVGL 锁
    s_mode = APP_MODE_MENU;
    s_active = -1;
    menu_build();
}

void app_ui_key(app_key_t key, app_key_ev_t ev)
{
    if (!app_port_lvgl_lock(500)) return;

    // 待机中：这一次按键只用来唤醒，直接丢掉不再往下分发。
    // 不分发是因为从待机按下的键，用户本意是"先看一眼"，不是"按这个键"——
    // 否则在解卦页按一下 OK 会顺手翻页，在摇卦页更会直接开始蓄力。
    if (standby_wake()) {
        app_port_lvgl_unlock();
        return;
    }
    standby_note_key();      // 有按键 = 有人，重置空闲计时

    if (s_mode == APP_MODE_LIUYAO) {
        liuyao_key(key, ev);
        app_port_lvgl_unlock();
        return;
    }

    // ---------------- 演示菜单模式 ----------------
    if (s_active >= 0) {
        // 演示页内：OK 长按 = 返回菜单
        if (key == APP_KEY_OK && ev == APP_KEY_LONG) {
            if (VIEWS[s_active].exit) VIEWS[s_active].exit();
            s_active = -1;
            menu_build();
        } else if (VIEWS[s_active].key) {
            VIEWS[s_active].key(key, ev);
        }
    } else if (key == APP_KEY_OK && ev == APP_KEY_LONG) {
        // 菜单内 OK 长按 = 回到六爻 App
        menu_teardown();
        s_mode = APP_MODE_LIUYAO;
        liuyao_enter();
    } else if (ev == APP_KEY_CLICK) {
        if (key == APP_KEY_UP) {
            s_sel = (s_sel + VIEW_COUNT - 1) % VIEW_COUNT;
            menu_refresh();
            ui_pixel_mascot_jump(s_mascot);
        } else if (key == APP_KEY_DOWN) {
            s_sel = (s_sel + 1) % VIEW_COUNT;
            menu_refresh();
            ui_pixel_mascot_jump(s_mascot);
        } else if (key == APP_KEY_OK && VIEWS[s_sel].enabled) {
            s_active = s_sel;
            ui_pixel_mascot_jump(s_mascot);
            lv_obj_delete(s_menu_scr);
            s_menu_scr = NULL;
            s_mascot = NULL;
            VIEWS[s_active].enter();
        }
    }

    app_port_lvgl_unlock();
}
