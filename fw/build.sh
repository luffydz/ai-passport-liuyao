#!/usr/bin/env bash
# fw/build.sh —— 编译 → 合并刷机包 → 官方布局校验，一条命令跑完。
#
# 用法（先激活 ESP-IDF v5.5.3）：
#   source ~/esp/esp-idf-v5.5.3/export.sh
#   cd "/Users/luffydz/Desktop/ai passport/liuyao/fw" && ./build.sh
set -euo pipefail
cd "$(dirname "$0")"

if ! command -v idf.py >/dev/null 2>&1; then
    echo "错误：idf.py 不可用。请先执行：source ~/esp/esp-idf-v5.5.3/export.sh" >&2
    exit 1
fi

# 组件下载走中国区镜像：默认 CDN 拉 LVGL 会中途截断（is not a zip file）
export IDF_COMPONENT_STORAGE_URL="${IDF_COMPONENT_STORAGE_URL:-https://components-file.espressif.cn}"

# 应用 bin 的名字由 CMakeLists.txt 的 project() 决定，勿改（官方校验脚本依赖它）
APP_BIN="FoloToy-AI-Passport.bin"
# 我们交付的刷机包：名字里带 liuyao，便于与基线固件区分
FULL_BIN="FoloToy-AI-Passport-liuyao-full.bin"

idf.py build
idf.py merge-bin -o "$PWD/build/$FULL_BIN"

# 官方校验脚本内部写死了合并包名 FoloToy-AI-Passport-full.bin，
# 而我们的包叫 ...-liuyao-full.bin。为了继续让官方脚本把守
# 「受保护 cardid 分区必须全是 0xFF（不得覆盖设备身份）」这条底线，
# 临时建一个同名软链指向我们的包（只是链接，不复制内容）。
ln -sf "$FULL_BIN" "build/FoloToy-AI-Passport-full.bin"
# 官方校验脚本住在【上层】的 ai-passport 克隆里（那是官方原仓库，不属于本项目）。
# 本项目已收进 liuyao/，从 liuyao/fw 往上是两层，所以路径是 ../../。
python3 ../../ai-passport/tools/verify_firmware.py build

# ---------------------------------------------------------------------------
# 归档本次刷机包
#   idf.py 每次都覆盖 build/ 下的同名文件 —— 不归档就永远只有一个版本，
#   想回退也找不到（这个坑踩过一次，当时把烧进设备的包直接覆盖没了）。
# ---------------------------------------------------------------------------
ARCHIVE_DIR="$PWD/dist"
mkdir -p "$ARCHIVE_DIR"
STAMP="$(date +%Y%m%d-%H%M%S)"
ARCHIVE_FILE="$ARCHIVE_DIR/FoloToy-AI-Passport-liuyao-$STAMP.bin"
cp "$PWD/build/$FULL_BIN" "$ARCHIVE_FILE"

echo
echo "完成。刷机包：fw/build/$FULL_BIN"
echo "      已归档：fw/dist/$(basename "$ARCHIVE_FILE")"
echo "网页刷机：https://ai-passport.folotoy.cn/tools/web-flasher/  → 起始地址 0x0，波特率 460800"
