// sim/host/sim_port.c —— app_port.h 的「模拟器侧」实现。
//
// 真机侧由 BSP 实现（bsp_battery_percent / bsp_display_backlight / bsp_lvgl_lock），
// 这里用假数据和空操作代替。UI 代码感知不到区别 —— 这正是抽象层的意义。
#include "app_port.h"

#include <stdio.h>

// 模拟器固定假数据：真机读 CW2017，这里返回一个好看的数值
int app_port_battery_percent(void)
{
    return 87;
}

void app_port_backlight_set(int percent)
{
    printf("[port] backlight -> %d%%\n", percent);
    fflush(stdout);
}

// ---------------------------------------------------------------------------
// 起卦日期持久化：模拟器落一个本地文件（真机对应 NVS）
// 放 /tmp 是为了每次出图互不干扰；想复现"首次开机"删掉它即可。
// ---------------------------------------------------------------------------
#define SIM_DATE_FILE "/tmp/ai-passport-sim-date.txt"

void app_port_date_save(int year, int month, int day)
{
    FILE *f = fopen(SIM_DATE_FILE, "w");
    if (!f) return;
    fprintf(f, "%d %d %d\n", year, month, day);
    fclose(f);
}

bool app_port_date_load(int *year, int *month, int *day)
{
    if (!year || !month || !day) return false;
    FILE *f = fopen(SIM_DATE_FILE, "r");
    if (!f) return false;
    int y = 0, m = 0, d = 0;
    const int n = fscanf(f, "%d %d %d", &y, &m, &d);
    fclose(f);
    if (n != 3 || y <= 0) return false;
    *year = y; *month = m; *day = d;
    return true;
}

// 模拟器全程单线程，不存在 LVGL 并发访问，加锁是空操作。
// 真机是多任务（按键任务 + LVGL 任务），必须真的加锁。
bool app_port_lvgl_lock(int timeout_ms)
{
    (void)timeout_ms;
    return true;
}

// ---------------------------------------------------------------------------
// 铜钱音效响度：模拟器落一个小文件（真机对应 NVS），并打印出来
// 想复现"首次开机默认 100%"删掉这个文件即可。
// ---------------------------------------------------------------------------
#define SIM_VOL_FILE "/tmp/ai-passport-sim-volume.txt"

static int s_volume = -1;          // -1 = 还没读过

int app_port_volume_get(void)
{
    if (s_volume >= 0) return s_volume;

    s_volume = 100;
    FILE *f = fopen(SIM_VOL_FILE, "r");
    if (f) {
        int v = -1;
        if (fscanf(f, "%d", &v) == 1 && v >= 0 && v <= 100) s_volume = v;
        fclose(f);
    }
    printf("[port] volume -> %d%%\n", s_volume);
    fflush(stdout);
    return s_volume;
}

void app_port_volume_set(int percent)
{
    if (percent < 0)   percent = 0;
    if (percent > 100) percent = 100;
    s_volume = percent;
    printf("[port] volume -> %d%%\n", percent);
    fflush(stdout);
    FILE *f = fopen(SIM_VOL_FILE, "w");
    if (!f) return;
    fprintf(f, "%d\n", percent);
    fclose(f);
}

void app_port_lvgl_unlock(void)
{
}
