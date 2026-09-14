// sim/host/capture.c —— 无头截图实现。
#include "capture.h"
#include "app_port.h"
#include "lvgl.h"

#include <stdio.h>
#include <string.h>

// 全屏帧缓冲（RGB565）。真机上不存在这东西 —— 真机是直接把 dirty area 推给 SPI 面板。
static uint16_t s_fb[APP_SCREEN_W * APP_SCREEN_H];

// LVGL 的局部渲染缓冲。**尺寸与真机一致：240 × 20 RGB565 ≈ 9.6 KB**
//（官方硬件指南 §5：The LVGL DMA buffer is one 240 × 20 RGB565 buffer,
//  about 9.6 KB）。缓冲小意味着 LVGL 要分多次局部刷新 —— 这正是真机的渲染路径，
// 也让 flush_cb 被调用的次数、以及每次的裁剪形态与真机一致。
static uint8_t s_draw_buf[APP_SCREEN_W * 20 * 2];

// LVGL 每渲染完一块就回调这里，我们把这块贴进全屏帧缓冲
static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;
    const uint16_t *src = (const uint16_t *)px_map;

    for (int32_t y = 0; y < h; y++) {
        const int32_t dy = area->y1 + y;
        if (dy < 0 || dy >= APP_SCREEN_H) continue;
        memcpy(&s_fb[dy * APP_SCREEN_W + area->x1], src + (size_t)y * w, (size_t)w * 2);
    }
    lv_display_flush_ready(disp);
}

void capture_init(void)
{
    lv_display_t *disp = lv_display_create(APP_SCREEN_W, APP_SCREEN_H);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_set_buffers(disp, s_draw_buf, NULL, sizeof(s_draw_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
}

bool capture_write_ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "无法写入 %s\n", path);
        return false;
    }
    fprintf(f, "P6\n%d %d\n255\n", APP_SCREEN_W, APP_SCREEN_H);

    for (int i = 0; i < APP_SCREEN_W * APP_SCREEN_H; i++) {
        const uint16_t v = s_fb[i];
        const uint8_t r = (uint8_t)(((v >> 11) & 0x1F) * 255 / 31);
        const uint8_t g = (uint8_t)(((v >> 5) & 0x3F) * 255 / 63);
        const uint8_t b = (uint8_t)((v & 0x1F) * 255 / 31);
        const uint8_t px[3] = { r, g, b };
        fwrite(px, 1, 3, f);
    }
    fclose(f);
    return true;
}
