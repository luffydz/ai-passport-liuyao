// sim/host/capture.h —— 无头截图：不建窗口，把 LVGL 渲染结果导出为图片。
//
// 用途：一条命令就能拿到「真 LVGL + 真字体」渲染出的界面图，
//       不依赖 SDL 窗口，方便脚本化与批量出图。
#pragma once

#include <stdbool.h>

// 创建一个 240x320 的无头 display，渲染结果会累积到内部全屏帧缓冲
void capture_init(void);

// 把当前帧缓冲写成 PPM(P6) 文件；失败返回 false
bool capture_write_ppm(const char *path);
