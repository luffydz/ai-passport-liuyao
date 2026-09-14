// ui/view_liuyao.h —— 六爻 App 的页面流。
//
// 流程：欢迎 → 择事（8 类）→ 取三位数 → 摇卦显爻 → 解卦
// 页面内部自己管理状态，app_ui 只负责把按键喂进来。
#pragma once

#include "app_port.h"

void liuyao_enter(void);
void liuyao_exit(void);
void liuyao_key(app_key_t key, app_key_ev_t ev);
