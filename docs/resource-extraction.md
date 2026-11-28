# FD2 资源提取器（`tools/fd2_extract.py`）

统一把原版数据目录里的资源导出成通用格式。纯标准库实现，不读 `FD2.SAV`，
不会修改游戏数据；输出目录默认 `./fd2-extracted`，可单独指到隔离目录。

```bash
# 全部资源（从仓库根目录运行；--data 指向含 FDTXT.DAT 的游戏数据目录）
python3 tools/fd2_extract.py --data /path/to/FD2 --out /path/to/out

# 只提一类
python3 tools/fd2_extract.py --data /path/to/FD2 --out /path/to/out music
python3 tools/fd2_extract.py --data /path/to/FD2 --out /path/to/out maps

# 额外逐帧导出小图；未识别资源保留原始 .bin
python3 tools/fd2_extract.py --data /path/to/FD2 --out /path/to/out --split --raw
```

可选子命令：`all`（默认）、`text`、`font`、`images`、`portraits`、`battle`、
`maps`、`music`。`--scale N` 控制图集放大倍数（默认 2；不影响整屏图与地图）。

## 输出结构

| 路径 | 内容 |
|---|---|
| `text/fdtxt.txt`、`text/fdtxt_resNNN_subNNN.txt` | FDTXT 全部文本流，按资源/子流拆文件；opcode 保留为 `{頭像:id}` / `{單位:slot}` / `{子文本}` / `{num}` |
| `text/glyphs.json` | 本次实际使用的字形号→汉字映射（来自 `tools/fd2data/`） |
| `images/font/glyph_atlas.png` | FDOTHER 资源 4 的 1824 个 16×16 字形，32 列网格 |
| `images/fdother/` | FDOTHER 中的调色板、`LMI1`/偏移表图片集；`images/ui/` 为 DATO 对话框帧；`images/ani/` 为 ANI 逐帧 |
| `portraits/unit_NNN_atlas.png` | FDICON.B24 每个单位 12 帧 24×24 头像（4×3 图集） |
| `battle/backgrounds/bg_NNN.png` | BG.DAT 的 320×100 卷动背景条（游戏里画到 y=50） |
| `battle/figures/fig_NNN_atlas.png` + `.json` | FIGANI.DAT 三人组动作图集，JSON 记录每子图的 x/y、姿态、SFX、帧数 |
| `battle/portraits/tai_NNN.png` | TAI.DAT 战斗肖像条 |
| `maps/state_NN_map.png` + `.json` | 每章地图（tile 24×24 拼接）与事件表；`maps/tilesets/ts_NN.png` 是 tile 图集 |
| `music/track_NN.xmi` / `.mid` | FDMUS.DAT 的 IFF XMID 原样导出 + 转换后的标准 MIDI（SMF format 0） |
| `music/sample_opl.json` | SAMPLE.OPL（或 SAMPLE.AD）162 个 AIL 两算子音色寄存器 |
| `manifest.json` | 各类别计数、耗时、数据目录与已知限制 |

PNG 全部是 8-bit 索引色 + PLTE。主调色板取 FDOTHER 资源 0 的 768 字节 6-bit DAC
值，按 `(v<<2)|(v>>4)` 转 8-bit；ANI 每帧使用帧内当前调色板。精灵图把索引 0
标为透明（`tRNS`），背景与地图不透明。

## 解码依据与边界

- **容器**：`LLLLLL` + u32 头长 + u32 **END** 偏移表；资源 `i` 是
  `[i==0 ? header : ends[i-1], ends[i])`。实现见 `src/game/res.c`。
- **大图 RLE**：`u16 w,u16 h` + 2-bit token（solid / odd / literal / skip），
  对应 `src/game/rle.c`。odd token 只写隔列像素，所以部分精灵放大后呈网点状，
  这是原版语义，不是解码错误。
- **DATO 框**：`rle2`，`c <= 0xC0` 为字面像素，`c > 0xC0` 后跟重复值与次数；
  `src/game/rle2.c`。
- **24×24 精灵**：FDICON.B24（13×u32/条目的 12 帧表）与 FDSHAP 形状库用
  `sprite24_plain`；见 `src/game/unit_load.c`、`src/game/map_shape.c`。
- **偏移表推断**：FDOTHER/FIGANI 的子图表分别可能在 +0/+6/+8；工具用首个子图
  偏移反推表长并验证子图头，避免把像素数据误当偏移。
- **地图**：FDFIELD 每状态三资源（地图/状态头/摆放），tile 低 10 位索引
  FDSHAP 精灵。静态渲染只画 base tile；运行期换色带、镜像/闪烁与事件触发
  不在导出图里（JSON 保留事件表）。
- **ANI**：0xAD 容器头，帧数在 +0xA5；每帧 `{u16 size, i16 cmd_count}` +
  命令流，10 个 opcode 逐条执行并保存当前调色板/帧缓冲；见 `src/game/ani.c`。
- **音乐**：FDMUS 曲目是 IFF XMID/XDIR。XMI 的 delta 是"连续 <0x80 字节求和"
  （不是 SMF VLQ），每个事件都写 status，Note-On 后跟 VLQ 音长；工具解析
  EVNT、补 Note-Off、按 tick 稳定排序后写 SMF。AIL 固定 120 ticks/s 与
  OPL3 音色的听感仍由引擎决定，MIDI 文件只保留事件序列，不渲染 WAV。

验证（2026-11-28，Arch Linux）：一次 `all` 约 8 秒，输出 926 条文本流、
1824 字形、140 单位头像、56 张战斗背景、264 组战斗动画、33 张地图、
73 组 FDOTHER 图集、136 组 DATO 图集、9 段 ANI、20 个音乐条目
（15 首转 MIDI）、161 个音色。
