#!/usr/bin/env python3
"""把模拟器导出的 PPM(P6) 转成 PNG，并可选放大。

用法：
    python3 sim/tools/ppm2png.py sim/out/menu.ppm              # 只转 PNG
    python3 sim/tools/ppm2png.py sim/out/menu.ppm --zoom 3     # 同时出 3 倍放大图
C 端不引入任何图片编码库，只写 PPM；转换这一步在 Mac 上用 PIL 完成。
"""
import argparse
import pathlib
import sys

from PIL import Image


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("ppm", help="模拟器导出的 PPM 文件")
    ap.add_argument("--zoom", type=int, default=0,
                    help="额外输出一张放大图（最近邻，保持像素锐利）")
    args = ap.parse_args()

    src = pathlib.Path(args.ppm)
    if not src.exists():
        print(f"找不到文件: {src}", file=sys.stderr)
        return 1

    with Image.open(src) as im:
        im.load()
        png = src.with_suffix(".png")
        im.save(png)
        print(f"{png}  ({im.width}x{im.height})")

        if args.zoom > 0:
            big = im.resize((im.width * args.zoom, im.height * args.zoom),
                            Image.NEAREST)
            zoomed = src.with_name(f"{src.stem}_{args.zoom}x.png")
            big.save(zoomed)
            print(f"{zoomed}  ({big.width}x{big.height})")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
