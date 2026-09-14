// ui/app_ui.h —— 应用外壳：入口 + 按键分发。
//
// 顶层是一个【功能选择启动页】，之后才是六爻 / 每日穿衣两个 App。
//
// 平台层（真机按键任务 / 模拟器 SDL 事件循环）只需要做两件事：
//   1. 等 LVGL 初始化完成后调用 app_ui_start()
//   2. 收到按键就调用 app_ui_key()
#pragma once

#include "app_port.h"

// 启动 App（必须在 LVGL 初始化之后调用）
void app_ui_start(void);

// 送一个按键事件进来（内部会自行加解锁 LVGL）
void app_ui_key(app_key_t key, app_key_ev_t ev);

// 启动页选择后进入对应 App（内部置模式 + 构造页面）
void app_ui_launch_liuyao(void);
void app_ui_launch_dressing(void);

// 任一 App 首页长按 OK → 回到功能选择启动页
void app_ui_goto_launcher(void);
