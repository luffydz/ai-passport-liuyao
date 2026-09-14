// ui/view_launcher.c —— 功能选择启动页（黑金国风，与两个 App 同一套主题）。
//
// 两选项上下排列，选中项金框高亮；OK 进对应 App。
// 屏幕由 app_ui 持有并切换，本文件只负责把这一页画出来、把按键喂给选择逻辑。
#include "view_launcher.h"
#include "app_ui.h"
#include "ui_theme.h"
#include "view_liuyao.h"
#include "view_dressing.h"
#include "lvgl.h"

#include <string.h>

#define SEL_COUNT 2
static const char *SEL_NAMES[SEL_COUNT] = { "六  爻", "每 日 穿 衣" };

static lv_obj_t *s_scr;
static int       s_sel;
static lv_obj_t *s_frames[SEL_COUNT];

static void launcher_refresh(void)
{
    for (int i = 0; i < SEL_COUNT; i++) {
        th_set_selected(s_frames[i], i == s_sel);
    }
}

void launcher_enter(void)
{
    s_sel = 0;
    s_scr = th_screen_create();

    lv_obj_t *hdr = th_label(s_scr, "选 择 功 能", th_font_small(), TH_DIM);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 28);

    for (int i = 0; i < SEL_COUNT; i++) {
        const int x = 28;
        const int w = APP_SCREEN_W - 2 * 28;
        const int y = 88 + i * 84;
        const int h = 60;
        s_frames[i] = th_frame_create(s_scr, x, y, w, h);

        lv_obj_t *lb = th_label(s_frames[i], SEL_NAMES[i], th_font_title(), TH_TEXT);
        lv_obj_set_width(lb, lv_pct(100));
        lv_obj_set_style_text_align(lb, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(lb, LV_ALIGN_CENTER, 0, -th_vshift(th_font_title(), 0x56FD));
    }

    launcher_refresh();
    th_hint_create(s_scr, "上下选择 · OK 进入");
    lv_screen_load(s_scr);
}

void launcher_exit(void)
{
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
    }
}

void launcher_key(app_key_t key, app_key_ev_t ev)
{
    if (ev == APP_KEY_CLICK) {
        if (key == APP_KEY_UP) {
            s_sel = (s_sel + SEL_COUNT - 1) % SEL_COUNT;
            launcher_refresh();
        } else if (key == APP_KEY_DOWN) {
            s_sel = (s_sel + 1) % SEL_COUNT;
            launcher_refresh();
        } else if (key == APP_KEY_OK) {
            launcher_exit();
            if (s_sel == 0) app_ui_launch_liuyao();
            else            app_ui_launch_dressing();
        }
    }
}
