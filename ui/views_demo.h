// ui/views_demo.h —— 模拟器自带的三个演示页（Display / Battery / Button）。
// 它们的作用是验证管线：面板+文字渲染是否正常、按键事件链路是否通。
// 我们做自己的页面时，可以照这三个页面的写法（同构于基线 main/demo_*.c）。
#pragma once

#include "app_port.h"

// Display：色块循环 + 背光档位（对应基线 demo_display.c，去掉真实 LEDC 调用）
void view_display_enter(void);
void view_display_exit(void);
void view_display_key(app_key_t key, app_key_ev_t ev);

// Battery：读 app_port_battery_percent() 并显示
void view_battery_enter(void);
void view_battery_exit(void);
void view_battery_key(app_key_t key, app_key_ev_t ev);

// Button：显示最后一次收到的按键事件（验证输入链路）
void view_button_enter(void);
void view_button_exit(void);
void view_button_key(app_key_t key, app_key_ev_t ev);
