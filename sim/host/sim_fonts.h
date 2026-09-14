// sim/host/sim_fonts.h —— 模拟器侧的字体实现（FreeType 动态加载 OTF/TTF）。
#pragma once

// 必须在 lv_init() 之后调用（lv_init 会注册 FreeType 与 FS 驱动）
void sim_fonts_init(void);
