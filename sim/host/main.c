// sim/host/main.c —— 模拟器入口。
//
// 两种模式：
//   交互模式（默认）  SDL 窗口 + 键盘当三键，可手按
//   出图模式 --shot  不开窗口，把渲染结果导出成 PPM，用于脚本化/批量出图
//
// 按键映射（交互模式）：
//   ↑ / W / K        = UP
//   ↓ / S / J        = DOWN
//   Enter / Space    = OK
//   按住 ≥600ms 松手  = 长按（页面里 OK 长按 = 返回主菜单）
//   ESC 或关闭窗口    = 退出
#include "app_ui.h"
#include "app_port.h"
#include "capture.h"
#ifndef SIM_USE_STATIC_FONTS
#include "sim_fonts.h"
#endif
#include "sim_audio.h"
#include "lv_alloc_counter.h"   // CUSTOM 模式下的 LVGL 精确占用统计
#include "lvgl.h"   // 内含 src/drivers/lv_drivers.h，LV_USE_SDL=1 时已声明 SDL 窗口 API

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ---------------------------------------------------------------------------
// 编译期护栏：证明 sim/lv_conf.h 真的生效（否则 LVGL 会静默用默认值）
// ---------------------------------------------------------------------------
_Static_assert(LV_COLOR_DEPTH == 16, "lv_conf.h 未生效：真机是 16bit 色深");
#if LV_FONT_MONTSERRAT_20 != 1
#error "lv_conf.h 未生效：真机启用了 Montserrat 20"
#endif

// 真机 LVGL 内置池大小（Kconfig LV_MEM_SIZE_KILOBYTES=24）。
// 二进制已证实：固件里 work_mem_int.0 = 0x6000 = 24576 字节。
#define DEVICE_LV_MEM_SIZE (24 * 1024)

#define LONG_PRESS_MS 600
#define MAX_KEYS      64

typedef struct {
    app_key_t    key;
    app_key_ev_t ev;
} key_step_t;

// ---------------------------------------------------------------------------
// 时基：用 CLOCK_MONOTONIC，不依赖 SDL（出图模式下不初始化 SDL）
// ---------------------------------------------------------------------------
static uint32_t tick_cb(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL);
}

// 跑 n 个 LVGL 周期，让渲染/动画有机会完成
static void pump(int n)
{
    for (int i = 0; i < n; i++) {
        lv_timer_handler();
        struct timespec ts = { 0, 16 * 1000 * 1000 };   // 16ms
        nanosleep(&ts, NULL);
    }
}

// ---------------------------------------------------------------------------
// 内存报告
//
// 两种分配器模式，报告方式必须不同 —— 这点要诚实：
//
//   CUSTOM 计数分配器（默认；语义等同真机的 CONFIG_LV_USE_CLIB_MALLOC=y）
//       LVGL 直接走 malloc/free，没有独立池；每次申请释放都经过
//       sim/host/lv_alloc_counter.c 的包装，所以能精确给出
//       「LVGL 当前占用 / 峰值 / 相对首次的增量」—— 这正是我们关心的数字。
//       （曾试过 CLIB + 宿主 malloc_zone_statistics，但那台机器上
//        max_size_in_use 读出 0、display 阶段报 +70KB，与内置池模式的
//        6.1KB 自相矛盾，不可信，所以换成自己计数。）
//
//   BUILTIN（真机默认的 24KB 静态池；-DSIM_LV_BUILTIN_POOL=1）
//       lv_mem_monitor() 可用，能精确看到池内占用、峰值与碎片率 ——
//       要复现官方实录里「池耗尽 -> 白屏」的场景就用这个模式。
// ---------------------------------------------------------------------------
#if LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN
static void report_mem(const char *tag)
{
    lv_mem_monitor_t m;
    lv_mem_monitor(&m);
    printf("  [mem] %-12s LVGL池 total=%u used=%u free=%u 峰值=%u(%u%%) 碎片=%u%%\n",
           tag,
           (unsigned)m.total_size,
           (unsigned)(m.total_size - m.free_size),
           (unsigned)m.free_size,
           (unsigned)m.max_used, (unsigned)m.used_pct, (unsigned)m.frag_pct);
    fflush(stdout);
}
#else
static size_t s_alloc_first;

static void report_mem(const char *tag)
{
    size_t cur = 0, peak = 0, live = 0, total = 0;
    sim_lv_alloc_stats(&cur, &peak, &live, &total);
    if (s_alloc_first == 0) s_alloc_first = cur;

    const long delta_kb = ((long)cur - (long)s_alloc_first) / 1024;
    printf("  [mem] %-12s LVGL占用=%zuKB 峰值=%zuKB 较首次=%+ldKB 存活块=%zu 累计申请=%zu\n",
           tag, cur / 1024, peak / 1024, delta_kb, live, total);
    fflush(stdout);
}
#endif

// --keys 脚本：U=上 D=下 K=确定(短按) L=确定(长按)
static int parse_keys(const char *spec, key_step_t *out, int max)
{
    int n = 0;
    for (const char *p = spec; *p && n < max; p++) {
        switch (*p) {
        case 'U': case 'u': out[n].key = APP_KEY_UP;   out[n].ev = APP_KEY_CLICK; n++; break;
        case 'D': case 'd': out[n].key = APP_KEY_DOWN; out[n].ev = APP_KEY_CLICK; n++; break;
        case 'K': case 'k': out[n].key = APP_KEY_OK;   out[n].ev = APP_KEY_CLICK; n++; break;
        case 'L': case 'l': out[n].key = APP_KEY_OK;   out[n].ev = APP_KEY_LONG;  n++; break;
        case 'P': case 'p': out[n].key = APP_KEY_OK;   out[n].ev = APP_KEY_PRESS; n++; break;
        case ',': case ' ': case '-': break;
        default:
            fprintf(stderr, "警告：忽略无法识别的按键符号 '%c'（可用 U/D/K/L）\n", *p);
            break;
        }
    }
    return n;
}

static bool key_from_sdl(SDL_Keycode sym, app_key_t *out)
{
    switch (sym) {
    case SDLK_UP:    case SDLK_w: case SDLK_k:             *out = APP_KEY_UP;   return true;
    case SDLK_DOWN:  case SDLK_s: case SDLK_j:             *out = APP_KEY_DOWN; return true;
    case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: *out = APP_KEY_OK;   return true;
    default: return false;
    }
}

static const char *key_name(app_key_t k)
{
    return (k == APP_KEY_UP) ? "UP" : (k == APP_KEY_DOWN) ? "DOWN" : "OK";
}

static void usage(const char *argv0)
{
    printf("用法:\n"
           "  %s                              交互模式（SDL 窗口，键盘当三键）\n"
           "  %s --shot out.ppm [--keys K,D,K] [--frames N]\n"
           "      脚本按键: U=上 D=下 K=确定(短按) L=确定(长按) P=确定(按住未放)\n"
           "                                    出图模式（无窗口，导出 PPM）\n"
           "\n按键映射:  ↑/W/K = UP    ↓/S/J = DOWN    Enter/Space = OK\n"
           "           按住 >=600ms 松手 = 长按（页面里 OK 长按返回菜单）\n"
           "           ESC 或关窗 = 退出\n"
           "\n编译期开关（默认是 CLIB 分配器，与真机推荐配置一致）:\n"
           "  -DSIM_LV_BUILTIN_POOL=1   切回 24KB 内置池，复现「池耗尽 -> 白屏」\n"
           "  -DLV_MEM_SIZE=<字节>      改内置池大小（仅池模式有效）\n",
           argv0, argv0);
}

int main(int argc, char **argv)
{
    const char *shot_path = NULL;
    const char *keys_spec = NULL;
    int frames = 40;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--shot") && i + 1 < argc)        shot_path = argv[++i];
        else if (!strcmp(argv[i], "--keys") && i + 1 < argc)   keys_spec = argv[++i];
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) { usage(argv[0]); return 0; }
        else { fprintf(stderr, "未知参数: %s\n", argv[i]); usage(argv[0]); return 2; }
    }

    key_step_t seq[MAX_KEYS];
    const int seq_n = keys_spec ? parse_keys(keys_spec, seq, MAX_KEYS) : 0;

    lv_init();
    lv_tick_set_cb(tick_cb);
    report_mem("lv_init 后");
#ifndef SIM_USE_STATIC_FONTS
    sim_fonts_init();   // 中文字体（FreeType），必须在 lv_init 之后
#endif

    printf("FOLOTOY AI PASSPORT · UI 模拟器\n");
    printf("  屏幕 %dx%d   LVGL %d.%d.%d   绘制缓冲 %d 行(%.1fKB)\n",
           APP_SCREEN_W, APP_SCREEN_H,
           LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH,
           20, (double)(APP_SCREEN_W * 20 * 2) / 1024.0);

#if LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN
    if ((int)LV_MEM_SIZE != DEVICE_LV_MEM_SIZE) {
        printf("  分配器: LVGL 内置静态池 %u 字节 —— 与真机 %d 字节不一致，"
               "结果不代表真机内存表现\n", (unsigned)LV_MEM_SIZE, DEVICE_LV_MEM_SIZE);
    } else {
        printf("  分配器: LVGL 内置静态池 %u 字节（与真机一致）\n", (unsigned)LV_MEM_SIZE);
    }
#else
    printf("  分配器: 自定义计数分配器（语义等同真机 CONFIG_LV_USE_CLIB_MALLOC=y）\n");
#endif

    if (shot_path) {
        capture_init();
        printf("  模式: 出图 -> %s\n", shot_path);
    } else {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
            fprintf(stderr, "SDL_Init 失败: %s\n", SDL_GetError());
            return 1;
        }
        lv_display_t *disp = lv_sdl_window_create(APP_SCREEN_W, APP_SCREEN_H);
        if (!disp) {
            fprintf(stderr, "创建 SDL 窗口失败\n");
            return 1;
        }
        lv_sdl_window_set_title(disp, "FOLOTOY AI PASSPORT - UI SIM");
        sim_audio_init();   // 出图模式不初始化音频（脚本跑不出声）
        lv_sdl_window_set_zoom(disp, 2.0f);   // 240x320 太小，放大 2 倍好看
        printf("  模式: 交互窗口（放大 2 倍）\n"
               "  ↑/W/K = UP   ↓/S/J = DOWN   Enter/Space = OK   按住=长按   ESC = 退出\n  （按下即上报 PRESS，抬起再报 CLICK/LONG —— 与真机 BSP 事件一致）\n");
    }

    report_mem("display 后");
    app_ui_start();
    report_mem("建界面后");

    // ---------------- 出图模式 ----------------
    if (shot_path) {
        for (int i = 0; i < seq_n; i++) {
            printf("  按键脚本 %d/%d: %s %s\n", i + 1, seq_n,
                   key_name(seq[i].key), seq[i].ev == APP_KEY_LONG ? "LONG" : "CLICK");
            app_ui_key(seq[i].key, seq[i].ev);
            pump(frames / 4 + 1);   // 每个按键后让界面刷新完成
        }
        pump(frames);
        report_mem("渲染完成后");
        if (!capture_write_ppm(shot_path)) return 1;
        printf("  已导出: %s\n", shot_path);
        return 0;
    }

    // ---------------- 交互模式 ----------------
    bool quit = false;
    bool holding = false;
    app_key_t held = APP_KEY_OK;
    uint32_t down_ms = 0;

    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_KEYDOWN && e.key.repeat == 0) {
                if (e.key.keysym.sym == SDLK_ESCAPE) { quit = true; break; }
                app_key_t k;
                if (key_from_sdl(e.key.keysym.sym, &k)) {
                    held = k;
                    holding = true;
                    down_ms = SDL_GetTicks();
                    // 按下瞬间立刻上报：真机 BSP_BTN_PRESS 也是这个语义，
                    // "按住蓄力"这类交互需要它（只在抬起时才拿到事件就晚了）
                    app_ui_key(k, APP_KEY_PRESS);
                }
            } else if (e.type == SDL_KEYUP) {
                app_key_t k;
                if (holding && key_from_sdl(e.key.keysym.sym, &k) && k == held) {
                    const uint32_t dt = SDL_GetTicks() - down_ms;
                    const app_key_ev_t ev = (dt >= LONG_PRESS_MS) ? APP_KEY_LONG : APP_KEY_CLICK;
                    printf("[key] %s %s (%ums)\n", key_name(held),
                           ev == APP_KEY_LONG ? "LONG" : "CLICK", (unsigned)dt);
                    fflush(stdout);
                    app_ui_key(held, ev);
                    holding = false;
                }
            }
        }

        const uint32_t idle = lv_timer_handler();
        SDL_Delay(idle > 10 ? 10 : (idle ? idle : 1));
    }

    report_mem("退出时");
    printf("退出。\n");
    return 0;
}
