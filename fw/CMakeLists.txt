# AI Passport · 六爻 App —— ESP-IDF 工程入口
#
# 结构与基线一致：板级能力在 components/bsp（从基线复制，未改动），
# 应用代码在 main/。UI 代码在工程外的 ../ui（与模拟器共用同一份源码）。
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 工程名决定【应用 bin】的文件名，这里刻意保持与基线一致（FoloToy-AI-Passport）：
# 官方校验脚本 tools/verify_firmware.py 是按这个名字硬编码找文件的，改名会让
# 「受保护 cardid 分区必须是 0xFF 空白」这条底线失去自动把关。
#
# 我们要区分自己固件的那份【刷机包】，由 fw/build.sh 用 -o 指定成：
#   FoloToy-AI-Passport-liuyao-full.bin     ← 网页刷机页选它，起始地址 0x0
project(FoloToy-AI-Passport)
