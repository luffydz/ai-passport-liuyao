// ui/view_launcher.h —— 功能选择启动页。
//
// 在六爻 / 每日穿衣两个 App 之前，先让用户选要进哪个：
//   上下键移动选中项，OK 确认进入。
// 两个 App 各自的首页长按 OK 都会回到这里（见 app_ui_goto_launcher）。
#pragma once

#include "app_port.h"

void launcher_enter(void);
void launcher_exit(void);
void launcher_key(app_key_t key, app_key_ev_t ev);
