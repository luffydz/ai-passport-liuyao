// fw/main/app_port_impl.c —— ui/app_port.h 在【真机侧】的实现。
//
// 这一层是三行转调，但有两处必须小心：
//   1. 音频必须异步 —— bsp_audio_write() 是阻塞的，1.3 秒的音效足以卡死
//      按键任务（按键回调跑在 button 组件的任务里）。所以这里用队列 + worker。
//   2. bsp_audio_set_format() 在采样率变化时会 close 再 reopen codec
//      （bsp_audio.h 里标注的坑），这个动作也必须在 worker 里做。
#include "app_port.h"
#include "app_port_impl.h"

#include "bsp_display.h"
#include "bsp_battery.h"
#include "bsp_audio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_random.h"     // esp_random()：随机源（掷币/随机用）
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "app_port";

// ---------------------------------------------------------------------------
// LVGL 锁：直接转 BSP（BSP 用的是 esp_lvgl_port 的递归互斥量）
// ---------------------------------------------------------------------------
bool app_port_lvgl_lock(int timeout_ms)
{
    return bsp_lvgl_lock(timeout_ms);
}

void app_port_lvgl_unlock(void)
{
    bsp_lvgl_unlock();
}

// ---------------------------------------------------------------------------
// 随机数：直接取芯片硬件随机源
// ---------------------------------------------------------------------------
uint32_t app_port_random(void)
{
    return esp_random();
}

// ---------------------------------------------------------------------------
// 电量 / 背光
// ---------------------------------------------------------------------------
int app_port_battery_percent(void)
{
    const int soc = bsp_battery_soc();
    // 读数不可用（器件缺失/未就绪）时返回 -1，UI 会优雅降级不画数字
    return (soc < 0 || soc > 100) ? -1 : soc;
}

void app_port_backlight_set(int percent)
{
    if (percent < 0)   percent = 0;
    if (percent > 100) percent = 100;
    bsp_display_backlight((uint8_t)percent);
}

// ---------------------------------------------------------------------------
// 音频：队列 + worker，保证 app_port_play_pcm() 非阻塞
// ---------------------------------------------------------------------------
typedef struct {
    const int16_t *samples;
    int            count;
    int            rate;
} pcm_job_t;

static QueueHandle_t s_audio_q;

static void audio_worker(void *arg)
{
    (void)arg;
    pcm_job_t job;

    while (1) {
        if (xQueueReceive(s_audio_q, &job, portMAX_DELAY) != pdTRUE) continue;
        if (job.samples == NULL || job.count <= 0) continue;

        // 采样率与当前格式不同时，BSP 会 close/reopen codec —— 只能在 worker 里做
        if (bsp_audio_set_format((uint32_t)job.rate, 16, 1) != ESP_OK) {
            ESP_LOGW(TAG, "音频格式切换失败(%d Hz)，跳过本次音效", job.rate);
            continue;
        }
        bsp_audio_write(job.samples, (size_t)job.count * 2);   // 16bit 单声道
    }
}

void app_port_impl_audio_start(void)
{
    // 队列深度 2：连按两次也不会堆积太多音效
    s_audio_q = xQueueCreate(2, sizeof(pcm_job_t));
    if (s_audio_q == NULL) {
        ESP_LOGW(TAG, "音频队列创建失败，音效将静音");
        return;
    }
    // 栈 4096：只做一次格式设置 + 一次 I2S 写，不需要更大
    if (xTaskCreate(audio_worker, "dressing_audio", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGW(TAG, "音频任务创建失败，音效将静音");
    }
}

void app_port_play_pcm(const int16_t *samples, int count, int sample_rate)
{
    if (s_audio_q == NULL || samples == NULL || count <= 0) return;

    const pcm_job_t job = { samples, count, sample_rate };
    // 超时 0：队列满就丢弃这次音效。宁可丢一声，也绝不能在按键任务里等。
    if (xQueueSend(s_audio_q, &job, 0) != pdTRUE) {
        ESP_LOGD(TAG, "音频队列已满，丢弃本次音效");
    }
}

// ---------------------------------------------------------------------------
// 选定日期持久化：写 NVS。
// 真机没有 RTC（断电即丢时间），本 App 要的"今天"只能靠用户输入并记住。
// 打包成一个 int 存取：y*10000 + m*100 + d。
// ---------------------------------------------------------------------------
#define DATE_NVS_NS  "dressing"
#define DATE_NVS_KEY "date"

void app_port_date_save(int year, int month, int day)
{
    nvs_handle_t h;
    if (nvs_open(DATE_NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGW(TAG, "NVS 打开失败，日期未保存");
        return;
    }
    nvs_set_i32(h, DATE_NVS_KEY, (int32_t)(year * 10000 + month * 100 + day));
    nvs_commit(h);
    nvs_close(h);
}

bool app_port_date_load(int *year, int *month, int *day)
{
    if (!year || !month || !day) return false;
    nvs_handle_t h;
    if (nvs_open(DATE_NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;
    int32_t v = 0;
    const bool ok = (nvs_get_i32(h, DATE_NVS_KEY, &v) == ESP_OK && v > 0);
    nvs_close(h);
    if (!ok) return false;
    *year  = (int)(v / 10000);
    *month = (int)((v / 100) % 100);
    *day   = (int)(v % 100);
    return true;
}

// ---------------------------------------------------------------------------
// 音效响度：写 codec + 存 NVS
//
// 为什么"写硬件"放在 get() 里：音量必须在开机后第一声【之前】生效，而 UI 只在
// 建首页时读一次。把这一步和"读入持久化值"绑在一起，就不会有谁忘了应用。
// ---------------------------------------------------------------------------
#define VOL_NVS_NS   "dressing"    // 与日期共用命名空间
#define VOL_NVS_KEY  "vol"
#define VOL_DEFAULT  100

static int s_volume = -1;          // -1 = 还没从 NVS 读过

int app_port_volume_get(void)
{
    if (s_volume >= 0) return s_volume;

    s_volume = VOL_DEFAULT;
    nvs_handle_t h;
    if (nvs_open(VOL_NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        int32_t v = -1;
        if (nvs_get_i32(h, VOL_NVS_KEY, &v) == ESP_OK && v >= 0 && v <= 100) {
            s_volume = (int)v;
        }
        nvs_close(h);
    }
    bsp_audio_set_volume((uint8_t)s_volume);
    ESP_LOGI(TAG, "音效响度恢复为 %d%%", s_volume);
    return s_volume;
}

void app_port_volume_set(int percent)
{
    if (percent < 0)   percent = 0;
    if (percent > 100) percent = 100;
    s_volume = percent;
    bsp_audio_set_volume((uint8_t)percent);

    nvs_handle_t h;
    if (nvs_open(VOL_NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGW(TAG, "NVS 打开失败，响度未保存");
        return;
    }
    nvs_set_i32(h, VOL_NVS_KEY, (int32_t)percent);
    nvs_commit(h);
    nvs_close(h);
}
