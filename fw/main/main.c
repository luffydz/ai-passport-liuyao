// fw/main/main.c —— AI Passport · 每日穿衣 App 的固件入口。
//
// 本文件只做两件事，刻意保持很薄：
//   1. 按顺序初始化板级外设（沿用基线验证过的初始化序列）
//   2. 把 BSP 的按键事件翻译成 UI 层事件，交给 app_ui
//
// 所有界面与交互逻辑都在 UI 层（../ui），与 macOS 模拟器共用同一份源码。
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_pins.h"       // 错误日志里要打印 LCD 引脚号

#include "app_port.h"
#include "app_ui.h"
#include "app_port_impl.h"

#include "esp_log.h"
#include "esp_sleep.h"
#include "nvs_flash.h"

static const char *TAG = "apps";

// ---------------------------------------------------------------------------
// 按键桥接：BSP 事件 → UI 事件
//
// 一一对应关系（三个事件同名同义）：
//   BSP_BTN_PRESS → APP_KEY_PRESS   按下瞬间（交互开始，必须转发）
//   BSP_BTN_CLICK → APP_KEY_CLICK   短按抬起
//   BSP_BTN_LONG  → APP_KEY_LONG    长按抬起
//   BSP_BTN_DOUBLE                       本 App 用不到，按短按处理
// ---------------------------------------------------------------------------
static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user)
{
    (void)user;

    app_key_t k;
    switch (btn) {
    case BSP_BTN_UP:   k = APP_KEY_UP;   break;
    case BSP_BTN_DOWN: k = APP_KEY_DOWN; break;
    default:           k = APP_KEY_OK;   break;
    }

    app_key_ev_t e;
    switch (ev) {
    case BSP_BTN_PRESS: e = APP_KEY_PRESS; break;
    case BSP_BTN_LONG:  e = APP_KEY_LONG;  break;
    default:            e = APP_KEY_CLICK; break;
    }

    app_ui_key(k, e);   // 内部自行加解锁 LVGL
}

void app_main(void)
{
    ESP_LOGI(TAG, "AI Passport 启动（六爻 / 每日穿衣）");

    esp_sleep_wakeup_cause_t wakeup = esp_sleep_get_wakeup_cause();
    if (wakeup != ESP_SLEEP_WAKEUP_UNDEFINED) {
        ESP_LOGI(TAG, "唤醒原因: %d", wakeup);
    }

    // NVS：存用户选定的日期用（真机没有 RTC，日期只能靠用户输入并记住）
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_ret = nvs_flash_init();
    }
    if (nvs_ret != ESP_OK) {
        ESP_LOGW(TAG, "NVS 初始化失败，选定日期将无法保存");
    }

    bsp_i2c_init();
    bsp_i2c_scan();

    // 屏幕是本 App 的载体，失败就没有界面可言 —— 打清楚日志后返回。
    if (bsp_display_init() != ESP_OK || bsp_lvgl_init() == NULL) {
        ESP_LOGE(TAG, "显示/LVGL 初始化失败。检查 SPI 接线"
                      "(MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    // 按键失败等于 App 完全不可操作，必须明确报错
    if (bsp_button_init(on_key, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "按键初始化失败，界面将无法操作");
    }

    // 音频失败只丢音效，不影响主流程 —— 降级不阻塞
    if (bsp_audio_init() == ESP_OK) {
        app_port_impl_audio_start();    // 起音频 worker，播放走非阻塞队列
        bsp_audio_set_volume(85);
    } else {
        ESP_LOGW(TAG, "音频初始化失败，音效将静音");
    }

    // 电量计失败时界面自动不显示电量（app_port_battery_percent 返回 -1）
    if (bsp_battery_init() != ESP_OK) {
        ESP_LOGW(TAG, "电量计初始化失败，界面不显示电量");
    }

    app_ui_start();   // 进入每日穿衣界面（内部自行加锁）

    ESP_LOGI(TAG, "就绪");
}
