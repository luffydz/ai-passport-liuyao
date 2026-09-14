// ui/view.h —— 每个页面的统一接口（与基线 main/demo.h 的 demo_entry_t 同构）。
//
// 新增一个页面 = 实现 enter/exit/key 三个函数，再在 app_ui.c 的 VIEWS[] 里加一行。
#pragma once

#include "app_port.h"

typedef struct {
    const char *name;                             // 菜单里显示的名字（ASCII）
    bool        enabled;                          // false = 菜单里灰显、不可进入
    void (*enter)(void);                          // 建自己的屏并载入
    void (*exit)(void);                           // 删屏、停定时器、释放资源
    void (*key)(app_key_t key, app_key_ev_t ev);  // 收按键（OK 长按已被 app_ui 拦截）
} app_view_t;
