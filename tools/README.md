# tools 速查

所有命令在**仓库根目录**执行。游戏数据默认在 `E:/FD2/`（可用参数改）。

## 1. 存档（玩家用）

| 工具 | 作用 | 用法 |
|---|---|---|
| `FD2SAV.BAT` | 命令行入口（双击=交互菜单） | `FD2SAV.BAT info` |
| `FD2GUI.BAT` | 图形界面入口（双击即开） | — |
| `fd2sav.py` | 存档读写/校验/满血/改钱/备份回滚 | `python tools/fd2sav.py info`<br>`… heal`（默认只治"继续游戏"）<br>`… heal --all`（含 4 个存档槽）<br>`… money 5000` / `… money +1000`<br>`… set 0 level=40 hp=999`<br>`… get continue 0 hp hp_max`<br>`… check` / `… restore` |
| `fd2sav_gui.py` | 同上，tkinter 界面（`--selftest` 自检） | `python tools/fd2sav_gui.py` |
| `sav_diff.py` | 两个存档解密后逐字节对比 | `python tools/sav_diff.py a.sav b.sav` |

## 2. 文本与字形

| 工具 | 作用 | 用法 |
|---|---|---|
| `fd2_fonttool.py` | 字库/文本底层工具：列资源、导字形表、解文本流、繁→简重生成字库 | `… text --text E:/FD2/FDTXT.DAT --out t.txt`<br>`… decode --text … --res 3 --sub 0`<br>`… items` / `… items --id 12`<br>`… sheet --lo 0 --hi 256 --out s.png` |
| `fd2_dialogue.py` | 全对白导出：`docs/dialogue.md` + 可搜索 `docs/dialogue.html` | `python tools/fd2_dialogue.py` |
| `glyph_view.py` | 按字形号导出放大图（目检用） | `python tools/glyph_view.py 160-179 out.png 12` |
| `glyph_atlas.py` | 全字库「位图 + 识别字」对照表（人工核对） | `python tools/glyph_atlas.py`（16 页）<br>`… --lo 640 --hi 960` |
| `glyph_dupcheck.py` | 查「一字多号」并给文本语境（找错认） | `python tools/glyph_dupcheck.py --only-diff` |
| `fd2data/_rebuild.py` | 由 ROWS 表 + OVERRIDE 重建 `glyphs.json` | `python tools/fd2data/_rebuild.py` |

字形表改动流程：改 `_rebuild.py` → 跑 `_rebuild.py` → `fd2_fonttool.py text` 导出全文 grep 复核 → `glyph_view.py` 目检 → `glyph_atlas.py` 重建对照图。详见 `tools/fd2data/README.md`。

## 3. 隐藏内容

| 工具 | 作用 | 用法 |
|---|---|---|
| `fd2secrets.py` | 扫描 FDFIELD/FDSHAP 里的隐藏事件、道具事件，并能标出存档里已拿过的 | `python tools/fd2secrets.py E:/FD2 --items`<br>`… --all` / `… --state 3 --save FD2.SAV` |

## 4. 移植/构建（开发者）

| 工具 | 作用 | 用法 |
|---|---|---|
| `gen_headers.py` | 由 `analysis/` 生成 `fd2_globals.h`/`fd2_funcs.h`/`fd2_image.c`/`fd2_relocate.c` | `python tools/gen_headers.py` |
| `gen_function_map.py` | 合并各表生成 `analysis/function-map.csv`（函数状态总表） | `python tools/gen_function_map.py` |
| `gen_dispatch.py` | 生成 64 位可用的函数派发表 `src/fd2_dispatch.c` | `python tools/gen_dispatch.py` |
| `extract_data_types.py` | 从 `port/` 源码抽数据符号真实类型 → `analysis/data_type_overrides.csv` | `python tools/extract_data_types.py` |
| `migrate_port_assets.py` | 把 `port/` 里已验证的 309 个翻译模块搬进 `port3/src/` | `python tools/migrate_port_assets.py` |
| `prepare_image.py` | 从原版 FD2.EXE 重建数据段镜像（含 LE fixup；版权数据不入库） | `python tools/prepare_image.py` |
| `check_compile.sh` | 轻量编译校验（WSL） | `wsl bash tools/check_compile.sh [--full]` |
| `scan64.sh` | 64 位可移植性扫描（WSL） | `wsl bash tools/scan64.sh all` |
| `_fix_fd.py` | 一次性补丁脚本（修复 `gen_headers.py` 正则），**勿日常使用** | — |

## 5. 数据

| 路径 | 内容 |
|---|---|
| `fd2data/glyphs.json` | 1824 字形号 → 汉字（生成物，别手改） |
| `fd2data/_rebuild.py` | 字形表源数据（ROWS 每行 16 字 + OVERRIDE 补丁） |
| `fd2data/glyph_map.seed.json` | 早期种子映射（内部用） |
| `fd2data/items.json` | 道具号 → 名称 |
