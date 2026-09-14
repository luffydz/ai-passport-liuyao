// ui/app_ui.h —— 应用外壳：入口 + 按键分发。
//
// 本 App 的直接界面是六爻页面流（ui/view_liuyao.c）；
// 基线的 7 个硬件演示页保留为「开发用演示菜单」，藏在这之后。
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

// 开发用：切到硬件演示菜单。
// ⚠️ 调用时必须【已持有 LVGL 锁】（它由页面内部按键回调触发，外层已加锁）
void app_ui_enter_menu(void);
