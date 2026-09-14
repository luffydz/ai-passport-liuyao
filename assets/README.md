# assets/ —— 字库与音效的源文件

这个目录放**生成字库所需的源字体**和**音效原始文件**。它们**不参与固件编译** ——
编译用的是已经生成好、放在 `ui/fonts/` 里的静态字库，那部分在库内。

## 为什么这里默认是空的

源字体与音效的**授权范围尚未逐一确认**，因此暂未纳入公开仓库。要重新生成字库，
请自行准备下列文件放回本目录（文件名需一致）：

```
assets/fonts/SourceHanSansCN-Medium.otf   正文·思源黑体（SIL OFL 1.1，可自由分发）
assets/fonts/ZiHunTaiAKaiShu.ttf          标题·楷体（授权范围请自行确认）
assets/sounds/c_inhand3.mp3               铜钱音效
```

思源黑体可从 [adobe-fonts/source-han-sans](https://github.com/adobe-fonts/source-han-sans)
取得。另两个请向原作者确认授权后再对外分发。

## 放好之后

```bash
cd .. && python3 tools/gen_fonts.py       # 重新生成 ui/fonts/ 下的静态字库
python3 tools/check_font_coverage.py      # 检查漏字（真机漏字会显示方块）
```

`tools/gen_fonts.py` 用 `Path(__file__).parent.parent` 定位项目根，因此只要
`assets/` 与 `ui/` 的相对位置不变，脚本就能找到这些文件。
