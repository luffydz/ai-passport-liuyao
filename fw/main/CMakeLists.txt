# fw/main —— 应用层
#
# ★ 关键：UI 源码在工程外（../../ui），与 macOS 模拟器共用同一份文件。
#   不复制、不软链，直接用绝对路径登记 —— 保证"模拟器跑的就是真机代码"。
set(UI_DIR   "${CMAKE_CURRENT_LIST_DIR}/../../ui")
# 算法单独放在 algo/：它是纯计算、零依赖的一层，带自己的回代验证程序
# （algo/test_dressing.c + 顶层 run_tests.sh），刻意不混进 UI 目录。
set(ALGO_DIR "${CMAKE_CURRENT_LIST_DIR}/../../algo")

idf_component_register(
    SRCS
        "main.c"                     # 固件入口：初始化 + 按键桥接
        "app_port_impl.c"            # app_port.h 的真机实现（转调 BSP）

        # ---- 双端共用 UI ----
        "${UI_DIR}/ui_theme.c"
        "${UI_DIR}/app_ui.c"
        "${UI_DIR}/view_dressing.c"   # 每日穿衣：欢迎 → 取日期 → 结果
        "${UI_DIR}/view_liuyao.c"      # 六爻：欢迎 → 择事 → 取日期 → 摇卦 → 解卦
        "${UI_DIR}/hexagram.c"         # 六爻卦数据与装卦算法（纯计算）
        "${UI_DIR}/liuyao_sound.c"     # 六爻铜钱音效
        "${UI_DIR}/view_launcher.c"    # 功能选择启动页（六爻 / 每日穿衣）
        "${UI_DIR}/standby.c"         # 待机画面（八卦环 + 背光压暗）
        "${UI_DIR}/ganzhi.c"          # 干支层：公历 → 年/月/日三柱
        "${UI_DIR}/th_fonts_static.c" # 静态字库入口（真机用）

        # ---- 算法层（纯计算）----
        "${ALGO_DIR}/dressing.c"      # 日柱 → 三档穿衣色

        # ---- 静态中文字库（tools/gen_fonts.py 生成，勿手改）----
        "${UI_DIR}/fonts/lv_font_kai_48.c"
        "${UI_DIR}/fonts/lv_font_kai_34.c"
        "${UI_DIR}/fonts/lv_font_sans_18.c"
        "${UI_DIR}/fonts/lv_font_sans_16.c"
        "${UI_DIR}/fonts/lv_font_sans_14.c"

    INCLUDE_DIRS "." "${UI_DIR}" "${ALGO_DIR}"
    # nvs_flash：存日期（真机无 RTC，日期靠用户输入并记住）
    REQUIRES bsp lvgl esp_timer nvs_flash
)
