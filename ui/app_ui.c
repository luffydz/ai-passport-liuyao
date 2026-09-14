// ui/app_ui.c —— 应用外壳实现。
//
// 结构（启动页在最前）：
//   ① 功能选择启动页          ui/view_launcher.c
//   ② 六爻 App               ui/view_liuyao.c
//   ③ 每日穿衣 App           ui/view_dressing.c
//
// 顶层用 s_mode 分流按键；各 App 页面内部自己管理状态，本文件只负责把按键喂进去。
#include "app_ui.h"
#include "view.h"
#include "view_liuyao.h"
#include "view_dressing.h"
#include "view_launcher.h"
#include "standby.h"
#include "lvgl.h"

#include <stddef.h>

typedef enum {
    APP_MODE_LAUNCHER = 0,   // 功能选择启动页
    APP_MODE_LIUYAO,         // 六爻 App
    APP_MODE_DRESSING,       // 每日穿衣 App
} app_mode_t;

static app_mode_t s_mode = APP_MODE_LAUNCHER;

// ---------------------------------------------------------------------------
// 对外接口
// ---------------------------------------------------------------------------
void app_ui_start(void)
{
    if (!app_port_lvgl_lock(1000)) return;
    launcher_enter();        // 先进功能选择启动页
    standby_init();          // 待机图层：建在最上层，跨页面存活
    standby_set_app(STANDBY_APP_NONE); // 启动页待机：仅调暗背光，不显示待机图
    app_port_lvgl_unlock();
}

// 启动页选六爻 → 进入六爻 App
void app_ui_launch_liuyao(void)
{
    s_mode = APP_MODE_LIUYAO;
    standby_set_app(STANDBY_APP_LIUYAO); // 六爻待机图=八卦环
    liuyao_enter();
}

// 启动页选每日穿衣 → 进入每日穿衣 App
void app_ui_launch_dressing(void)
{
    s_mode = APP_MODE_DRESSING;
    standby_set_app(STANDBY_APP_DRESSING); // 穿衣待机图=五行色环
    dressing_enter();
}

// 任一 App 首页长按 OK → 回到功能选择启动页
void app_ui_goto_launcher(void)
{
    if (s_mode == APP_MODE_LIUYAO)        liuyao_exit();
    else if (s_mode == APP_MODE_DRESSING) dressing_exit();
    s_mode = APP_MODE_LAUNCHER;
    standby_set_app(STANDBY_APP_NONE); // 回启动页待机：仅调暗背光，不显示待机图
    launcher_enter();
}

void app_ui_key(app_key_t key, app_key_ev_t ev)
{
    if (!app_port_lvgl_lock(500)) return;

    // 待机中：这一次按键只用来唤醒，直接丢掉不再往下分发。
    // 不分发是因为从待机按下的键，用户本意是"先看一眼"，不是"按这个键"——
    // 否则在解卦页按一下 OK 会顺手翻页，在结果页按一下 OK 会跳回日期页。
    if (standby_wake()) {
        app_port_lvgl_unlock();
        return;
    }
    standby_note_key();      // 有按键 = 有人，重置空闲计时

    switch (s_mode) {
    case APP_MODE_LAUNCHER:
        launcher_key(key, ev);
        break;
    case APP_MODE_LIUYAO:
        liuyao_key(key, ev);
        break;
    case APP_MODE_DRESSING:
        dressing_key(key, ev);
        break;
    }

    app_port_lvgl_unlock();
}
