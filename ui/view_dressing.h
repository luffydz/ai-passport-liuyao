// ui/view_dressing.h —— 每日穿衣 App 的页面流。
//
// 流程：欢迎 → 取日期 → 结果（日柱 + 三档穿衣色）
//
// 日期页整页借无 RTC 设备的通用做法：本机没有 RTC、也不联网，日期只能手输；
// 输一次存 NVS，下次开机默认填上次那个。三键设备上没有比这更省按键的方式。
//
// 页面内部自己管理状态，app_ui 只负责把按键喂进来。
#pragma once

#include "app_port.h"

void dressing_enter(void);
void dressing_exit(void);
void dressing_key(app_key_t key, app_key_ev_t ev);
