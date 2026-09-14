cmake_minimum_required(VERSION 3.16)
project(ai_passport_sim C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(SIM_ROOT  "${CMAKE_CURRENT_SOURCE_DIR}")
set(REPO_ROOT "${SIM_ROOT}/..")
set(UI_DIR    "${REPO_ROOT}/ui")
# LVGL 取自固件工程的托管组件目录 —— 和真机编译用的【同一份源码、同一版本】。
# 原本指向 ../ai-passport/managed_components（官方克隆），那样本项目就离不开那个
# 克隆了；改用 fw/ 里这份之后，liuyao/ 自成一体，模拟器与固件仍严格同源。
set(LVGL_DIR  "${REPO_ROOT}/fw/managed_components/lvgl__lvgl")

# ---------------------------------------------------------------------------
# 前置检查：LVGL 源码必须与真机同源同版本
# ---------------------------------------------------------------------------
if(NOT EXISTS "${LVGL_DIR}/lvgl.h")
    message(FATAL_ERROR "找不到 LVGL 源码: ${LVGL_DIR}\n固件工程的 managed_components 是否完整？")
endif()
if(NOT EXISTS "${SIM_ROOT}/lv_conf.h")
    message(FATAL_ERROR "找不到 ${SIM_ROOT}/lv_conf.h")
endif()

# ---------------------------------------------------------------------------
# LVGL：编译真源码（不做裁剪，与官方 PC 模拟器做法一致）
#   配置走 sim/lv_conf.h（LV_CONF_INCLUDE_SIMPLE + 把 sim/ 加进 include 路径）
# ---------------------------------------------------------------------------
file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS "${LVGL_DIR}/src/*.c")
list(LENGTH LVGL_SOURCES LVGL_SOURCE_COUNT)
message(STATUS "LVGL 源码文件数: ${LVGL_SOURCE_COUNT}")

add_library(lvgl STATIC ${LVGL_SOURCES})
target_include_directories(lvgl PUBLIC "${LVGL_DIR}" "${SIM_ROOT}")
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)
target_compile_options(lvgl PRIVATE -w)   # LVGL 自身源码不参与本项目的严格告警

# 可选：切回 LVGL 内置静态池模式，复现官方实录里的「池耗尽 -> 白屏」场景。
#   cmake -S . -B build -DSIM_LV_BUILTIN_POOL=1
# 不传则用 CLIB 分配器（对应真机 CONFIG_LV_USE_CLIB_MALLOC=y，官方推荐配置）。
if(DEFINED SIM_LV_BUILTIN_POOL)
    message(STATUS "LVGL 内置静态池模式: ${SIM_LV_BUILTIN_POOL}（1 = 复现真机 24KB 约束）")
    target_compile_definitions(lvgl PUBLIC SIM_LV_BUILTIN_POOL=${SIM_LV_BUILTIN_POOL})
endif()

# 可选：临时改内置池大小做实验（例如 -DLV_MEM_SIZE=65536）。仅在池模式下有效。
if(DEFINED LV_MEM_SIZE)
    message(STATUS "LVGL 内存池被覆盖为 ${LV_MEM_SIZE} 字节（真机值 24576）")
    target_compile_definitions(lvgl PUBLIC LV_MEM_SIZE=${LV_MEM_SIZE})
endif()

# ---------------------------------------------------------------------------
# SDL2（sdl2-compat 提供了 SDL2Config.cmake）
# ---------------------------------------------------------------------------
find_package(SDL2 CONFIG REQUIRED)
# 宿主 FreeType：模拟器用它加载 OTF/TTF 渲染中文
find_package(Freetype REQUIRED)
target_link_libraries(lvgl PUBLIC SDL2::SDL2 Freetype::Freetype)

# ---------------------------------------------------------------------------
# 模拟器可执行文件：ui/ 下的文件是「双端共用」的，这里直接编原始文件
# ---------------------------------------------------------------------------
add_executable(sim
    "${UI_DIR}/ui_pixel.c"
    "${UI_DIR}/ui_theme.c"
    "${UI_DIR}/hexagram.c"
    "${UI_DIR}/ganzhi.c"
    "${UI_DIR}/liuyao_sound.c"
    "${UI_DIR}/app_ui.c"
    "${UI_DIR}/views_demo.c"
    "${UI_DIR}/view_liuyao.c"
    "${UI_DIR}/standby.c"
    host/main.c
    host/sim_port.c
    host/sim_audio.c
    host/capture.c
)

# 可选：把"待机空闲时间"改短，方便自动回归。
#   cmake -S . -B build -DSIM_STANDBY_IDLE_MS=6000
# 为什么要这个口子：稳到待机需要"最后 60 秒没按键"，而脚本模式下按键都排在开头，
# 没法构造"待机之后再按键"的场景，除非把空闲时间压到比按键间隔还短。
# 不传则用固件的 60 秒（ui/standby.c 的默认值）。
if(DEFINED SIM_STANDBY_IDLE_MS)
    target_compile_definitions(sim PRIVATE STANDBY_IDLE_MS=${SIM_STANDBY_IDLE_MS})
endif()

# ---------------------------------------------------------------------------
# 字体来源二选一：
#   默认      FreeType 动态加载 OTF（改字号/换字体零成本，适合迭代 UI）
#   静态模式  编进 flash 的静态字库（与固件完全一致，用于验证字库本身）
#   用法：cmake -S . -B build -DSIM_USE_STATIC_FONTS=1
# ---------------------------------------------------------------------------
if(SIM_USE_STATIC_FONTS)
    message(STATUS "字体来源：静态字库（固件同款，ui/fonts/）")
    file(GLOB STATIC_FONT_SOURCES "${UI_DIR}/fonts/*.c")
    target_sources(sim PRIVATE
        "${UI_DIR}/th_fonts_static.c"
        ${STATIC_FONT_SOURCES}
    )
    target_compile_definitions(sim PRIVATE SIM_USE_STATIC_FONTS=1)
else()
    message(STATUS "字体来源：FreeType 动态加载（迭代用）")
    target_sources(sim PRIVATE host/sim_fonts.c)
endif()

# 计数分配器【只在非池模式】下编入。
#
# ⚠️ 这里踩过坑：它实现了 lv_malloc_core / lv_mem_monitor_core / lv_mem_init 等
# LVGL 分配器核心符号，而内置池模式由 LVGL 自己的 lv_mem_core_builtin.c 提供同名
# 符号。两者同时存在时，可执行文件里的定义会覆盖静态库里的 —— 结果"池模式"
# 根本没走池子（表现为 monitor 报出的 total 超过 LV_MEM_SIZE 配置值），数字不可信。
if(NOT SIM_LV_BUILTIN_POOL)
    target_sources(sim PRIVATE host/lv_alloc_counter.c)
endif()
target_include_directories(sim PRIVATE "${UI_DIR}")
target_link_libraries(sim PRIVATE lvgl SDL2::SDL2)
target_compile_options(sim PRIVATE -Wall -Wextra)
