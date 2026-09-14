# fw/main —— 应用层
#
# ★ 关键：UI 源码在工程外（../../ui），与 macOS 模拟器共用同一份文件。
#   不复制、不软链，直接用绝对路径登记 —— 保证"模拟器跑的就是真机代码"。
set(UI_DIR "${CMAKE_CURRENT_LIST_DIR}/../../ui")

idf_component_register(
    SRCS
        "main.c"                     # 固件入口：初始化 + 按键桥接
        "app_port_impl.c"            # app_port.h 的真机实现（转调 BSP）

        # ---- 双端共用 UI ----
        "${UI_DIR}/ui_pixel.c"
        "${UI_DIR}/ui_theme.c"
        "${UI_DIR}/app_ui.c"
        "${UI_DIR}/views_demo.c"
        "${UI_DIR}/view_liuyao.c"
        "${UI_DIR}/standby.c"         # 待机画面（八卦环 + 背光压暗）
        "${UI_DIR}/hexagram.c"        # 64 卦数据与算法（纯计算）
        "${UI_DIR}/ganzhi.c"          # 历法层：公历 → 干支/月建/旬空/六神/旺衰
        "${UI_DIR}/liuyao_sound.c"
        "${UI_DIR}/th_fonts_static.c" # 静态字库入口（真机用）

        # ---- 静态中文字库（tools/gen_fonts.py 生成，勿手改）----
        "${UI_DIR}/fonts/lv_font_kai_48.c"
        "${UI_DIR}/fonts/lv_font_kai_34.c"
        "${UI_DIR}/fonts/lv_font_sans_18.c"
        "${UI_DIR}/fonts/lv_font_sans_16.c"
        "${UI_DIR}/fonts/lv_font_sans_14.c"

    INCLUDE_DIRS "." "${UI_DIR}"
    # nvs_flash：存起卦日期（真机无 RTC，日期靠用户输入并记住）
    REQUIRES bsp lvgl esp_timer nvs_flash
)
