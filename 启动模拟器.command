#!/bin/bash
# 双击本文件即可启动「每日穿衣」UI 模拟器（macOS）
#
# 它会做三件事：
#   1. 关掉已在运行的旧实例（避免开两个窗口）
#   2. 如果还没编译过，自动编译（首次约 1-3 分钟）
#   3. 打开模拟器窗口
#
# 窗口里的按键对应设备上的三个键：
#   ↑ / W / K        = UP（上）
#   ↓ / S / J        = DOWN（下）
#   Enter / 空格      = OK（确定）
#   按住 0.6 秒以上再松 = 长按（界面里表示"返回/蓄力"）
#   ESC              = 退出模拟器

cd "$(dirname "$0")/sim" || exit 1

# 1) 关掉已在运行的实例
if pgrep -f "build/sim" >/dev/null 2>&1; then
    echo "检测到已有模拟器在运行，先关闭它..."
    pkill -f "build/sim" >/dev/null 2>&1
    sleep 1
fi

# 2) 没编译过就先编译
if [ ! -x "build/sim" ]; then
    echo "首次运行：正在编译模拟器（LVGL 全量编译，约 1-3 分钟）..."
    cmake -S . -B build || exit 1
    cmake --build build -j8 || exit 1
    echo "编译完成。"
fi

# 3) 启动
echo "启动模拟器窗口..."
echo "  ↑/W/K = 上    ↓/S/J = 下    Enter/空格 = 确定    按住 = 长按    ESC = 退出"
echo
./build/sim
