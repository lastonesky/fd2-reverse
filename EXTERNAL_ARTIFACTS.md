# EXTERNAL_ARTIFACTS — 仓库外关键产物登记（可追溯性）

> **port3 仓库不含原版程序的任何字节。** 手工维护的还原源码、引擎/工具/文档都入库；
> 运行必需的原版数据段镜像在构建时由 `tools/prepare_image.py` 从**本地的** `FD2.EXE`
> 生成（`analysis/seg_dseg0{2,3}.bin` + `src/fd2_image.c`），它们都被 `.gitignore`
> 忽略，永不提交。改动下列外部产物需同步本文件。

## 外部产物

| 产物 | 路径 | 大小 | MD5 | 说明 |
|---|---|---|---|---|
| 原程序 | `E:\FD2\FD2.EXE` | 509158 | `a6e341a8decc6ebf7f4872076d9cf161` | 逆向目标（DOS/4GW LE）。**构建时放到仓库根目录**（gitignored）或设 `FD2_EXE` |
| IDA 数据库 | `E:\FD2\FD2.EXE.i64` | 13410681 | `f949fe9e46fb37b880cc6bca2bcbc796` | 含全部原型/边界修复与命名；`analysis/*` 由此导出 |
| Ghidra 工程 | `E:\FD2\FlameDragon2.rep` | — | — | 独立第二意见；本轮未依赖 |

## 构建前置：数据段镜像（`tools/prepare_image.py`）

原生构建在运行时要读游戏数据段的静态数据（文件名串、每场景 BGM 表、道具/地形等表）。
这些是**受版权保护的游戏数据**，所以不入库。`tools/prepare_image.py` 的逻辑：

1. 若 `analysis/seg_dseg02.bin` + `seg_dseg03.bin` 已存在 → 直接复用；
2. 否则从 `FD2.EXE` 提取：解析 DOS/4GW LE 的对象页、**应用 LE fixups**（指针表里
   存的是重定位前的裸偏移，loader 会改写成绝对地址），写出两个数据段；
3. 若既无段数据又无 `FD2.EXE` → **打印提示并 `exit(1)`，停止构建**；
4. 由数据段生成 `src/fd2_image.c`（只含数据段；死的 cseg01 代码段不再生成）。

`FD2.EXE` 放仓库根目录即可（`.gitignore` 已忽略 `/FD2.EXE`），或用 `--exe PATH` /
`$FD2_EXE` 指定。`build.ps1`、`tools/check_compile.sh --full`、`host/Makefile engine`
都会先调用它。

> **已核对**：从本仓库锚定的 `FD2.EXE` 提取出的 dseg02/dseg03 与 IDA 导出的
> `analysis/seg_dseg0{2,3}.bin` **逐字节一致**（dseg02 前 0x4000、dseg03 全 0x34D2，
> 0 差异；仅 dseg02 的 BSS 尾部 IDA 填 0xFF、本工具填 0x00，功能等价）。fixups 应用
> 7948 条、跨页伪记录跳过 11 条。

## 已移出 git（体积大 / 可由原版程序或 IDA·Ghidra 重新获得）

这些路径已从**工作区与全部历史**中删除（`git filter-branch` 历史改写），并由
`.gitignore` 阻止再次入库：

| 路径 | 来源 | 如何重获 |
|---|---|---|
| `analysis/seg_dseg02.bin`、`seg_dseg03.bin` | 从 `FD2.EXE` 提取 | `python tools/prepare_image.py` |
| `src/fd2_image.c` | `prepare_image.py` 生成 | 同上 |
| `analysis/seg_cseg01.bin` | IDB 代码段导出 | 保留原因已消失（`fd2_code` 死代码），无需重获 |
| `analysis/decomp/**`（1366 个 `.c`） | Hex-Rays | IDA 全量反编译导出 |
| `analysis/disasm_fallback/**`、`analysis/cseg01_disasm.txt` | 反汇编 | objdump/IDA 重新导出 |
| `analysis/arity.json`、`callgraph.csv`、`data_items.csv`、`data_type_overrides.csv`、`decomp_failures.csv`、`dispatch_tables.csv`、`funcmap.csv`、`names.csv`、`prototype_heal_log.json`、`prototypes.csv`、`reloc_suspects.csv`、`strings.csv`、`compile_errors.txt` | IDA/脚本导出 | `tools/*.py` + IDB 重新导出 |
| `function-map.csv` | `gen_function_map.py` 生成 | 重跑该脚本（依赖外部的 `port/re/*.csv`） |
| `build/**` | 编译产物 | `pwsh -File build.ps1` |

保留在仓库内的手工/小体积文件：`analysis/data_type_overrides.manual.csv`、
`analysis/prototype_overrides.csv`、`src/include/*.h`（生成但小且编译必需）、
`src/fd2_relocate.c`（生成但极小）。

## 版本锚点

- `v0.1-phase1-static`：第一阶段（纯静态逆向）完成快照。包含：
  - 1366/1369 函数反编译导出、1114 个实参个数推导、1041 个数据符号宏（语料 100% 覆盖）
  - 五份知识库文档（overview / function-map / data-layout / module-map / uncertainties）
  - 双 profile 状态模型 + 2 个手工还原模块（res/boot）+ 5 项 gcc 静态验证全绿

## 重建路径（他人/未来自己）

1. 拿到 `E:\FD2\FD2.EXE`（MD5 须匹配），放到仓库根目录。
2. `pwsh -File build.ps1`（或 `python tools/prepare_image.py` 后再 `make -C host`）——
   会自动生成数据段与 `src/fd2_image.c`。
3. 若要重生成头文件/全量表：用 IDB 重新导出 `analysis/` 下的 `seg_*.bin`、`decomp/`、
   `*.csv|json`，再跑 `python tools/gen_headers.py` 与 `python tools/gen_function_map.py`。
