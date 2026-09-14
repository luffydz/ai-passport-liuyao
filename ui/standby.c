// ui/standby.c —— 待机画面（详见 standby.h 的说明）
#include "standby.h"
#include "app_port.h"
#include "ui_theme.h"
#include "lvgl.h"

#include <stddef.h>

// 无操作多久进待机（60 秒）。
// 可在编译期覆盖 —— 模拟器做待机回归时改短它，否则一次测试要真等 60 秒。
// 见 sim/CMakeLists.txt 的 SIM_STANDBY_IDLE_MS。
#ifndef STANDBY_IDLE_MS
#define STANDBY_IDLE_MS   60000
#endif
#define STANDBY_BL_PCT    25      // 待机时背光降到多少
#define AWAKE_BL_PCT      100     // 唤醒后恢复的亮度，与开机一致（main.c 里就是 100）

static lv_obj_t *s_layer;         // 全屏黑底 + 八卦环，建在 lv_layer_top() 上
static uint32_t  s_last_key;      // 上次按键的时刻（lv_tick）
static bool      s_on;            // 是否正处于待机

// ---------------------------------------------------------------------------
// 先天八卦环
//
// 8 个方位 × 3 爻，由内到外依次是【初爻→上爻】（八卦图里爻是往外长的）。
// 1 = 阳爻（一整段弧），0 = 阴爻（断成两段）。
//
// 排列按【先天方位】顺时针，从正上方开始：
//     乾(南/上) 兑(东南) 离(东) 震(东北) 坤(北/下) 艮(西北) 坎(西) 巽(西南)
// ---------------------------------------------------------------------------
static const uint8_t BAGUA[8][3] = {
    { 1, 1, 1 },   // 乾 ☰
    { 1, 1, 0 },   // 兑 ☱
    { 1, 0, 1 },   // 离 ☲
    { 1, 0, 0 },   // 震 ☳
    { 0, 0, 0 },   // 坤 ☷
    { 0, 0, 1 },   // 艮 ☶
    { 0, 1, 0 },   // 坎 ☵
    { 0, 1, 1 },   // 巽 ☴
};

// 三条爻所在的直径（由内到外）—— 30px 宽的环带
static const int RING_DIA[3] = { 104, 118, 132 };

// 一格 45°，这里只画中间 26°，留出的缝就是每卦之间的分界
#define SEG_HALF   13     // 单卦半张角（整段 26°）
#define SEG_GAP     4     // 阴爻中间断开处，距中心 ±4°（所以每段 9°）

// 只留"轨道那段弧"：底色/边框/指示弧/旋钮全部去掉，否则 lv_arc 会多画东西
static void arc_add(lv_obj_t *parent, int dia, int a_from, int a_to)
{
    lv_obj_t *a = lv_arc_create(parent);
    lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(a, dia, dia);
    lv_obj_center(a);
    lv_arc_set_bg_angles(a, a_from, a_to);

    lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(a, 0, 0);
    lv_obj_set_style_pad_all(a, 0, 0);

    lv_obj_set_style_arc_width(a, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_color(a, lv_color_hex(TH_DIM), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_remove_style(a, NULL, LV_PART_KNOB);
}

static void standby_build(void)
{
    // 铺在 lv_layer_top() 上：那一层永远盖在活动屏幕之上，而且换页不会被删，
    // 所以一幅图就能覆盖所有页面。
    s_layer = lv_obj_create(lv_layer_top());
    lv_obj_remove_flag(s_layer, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(s_layer, APP_SCREEN_W, APP_SCREEN_H);
    lv_obj_set_pos(s_layer, 0, 0);
    lv_obj_set_style_bg_color(s_layer, lv_color_hex(TH_BG), 0);
    lv_obj_set_style_bg_opa(s_layer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_layer, 0, 0);
    lv_obj_set_style_radius(s_layer, 0, 0);
    lv_obj_set_style_pad_all(s_layer, 0, 0);

    for (int t = 0; t < 8; t++) {
        // 0° 在 3 点钟方向、顺时针增长，所以正上方是 270°
        const int c = 270 + t * 45;
        for (int r = 0; r < 3; r++) {
            if (BAGUA[t][r]) {
                arc_add(s_layer, RING_DIA[r], c - SEG_HALF, c + SEG_HALF);
            } else {
                arc_add(s_layer, RING_DIA[r], c - SEG_HALF, c - SEG_GAP);
                arc_add(s_layer, RING_DIA[r], c + SEG_GAP,  c + SEG_HALF);
            }
        }
    }

    lv_obj_add_flag(s_layer, LV_OBJ_FLAG_HIDDEN);
}

static void standby_enter(void)
{
    if (s_on || !s_layer) return;
    s_on = true;
    lv_obj_remove_flag(s_layer, LV_OBJ_FLAG_HIDDEN);
    app_port_backlight_set(STANDBY_BL_PCT);
}

// 空闲计时：只判断"距上次按键过了多久"，不关心当前在哪一页
static void idle_cb(lv_timer_t *t)
{
    (void)t;
    if (s_on) return;
    if ((uint32_t)(lv_tick_get() - s_last_key) < STANDBY_IDLE_MS) return;
    standby_enter();
}

void standby_init(void)
{
    standby_build();
    s_last_key = lv_tick_get();
    // 1 秒查一次就够 —— 进待机差一秒无所谓，但定时器频繁唤醒反而费电
    lv_timer_create(idle_cb, 1000, NULL);
}

void standby_note_key(void)
{
    s_last_key = lv_tick_get();
}

bool standby_wake(void)
{
    if (!s_on) return false;

    s_on = false;
    s_last_key = lv_tick_get();
    if (s_layer) lv_obj_add_flag(s_layer, LV_OBJ_FLAG_HIDDEN);
    app_port_backlight_set(AWAKE_BL_PCT);
    return true;
}
