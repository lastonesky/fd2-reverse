# EXTERNAL_ARTIFACTS — 仓库外关键产物登记（可追溯性）

> 本仓库（port3）只存**手工维护的还原源码、引擎/工具、文档，以及游戏运行时必需的小体积数据镜像**。
> 原版程序的**代码段**字节、反编译/反汇编、IDA·Ghidra 导出的大表都**不在 git 内**
> （可随时从下面的 IDB/原程序重新导出，见“已移出 git”一节）。以下外部产物记录哈希/时间来
> 锁定“这一版分析基于什么”，改动需同步本文件。

| 产物 | 路径 | 大小 | MD5 | 说明 |
|---|---|---|---|---|
| 原程序 | `E:\FD2\FD2.EXE` | 509158 | `a6e341a8decc6ebf7f4872076d9cf161` | 逆向目标（DOS/4GW LE）。任何外部资料若哈希不同即为**另一 build**（见 RE_MAP §7.6） |
| IDA 数据库 | `E:\FD2\FD2.EXE.i64` | 13410681 | `f949fe9e46fb37b880cc6bca2bcbc796` | **含本轮全部原型修复/边界修复/命名**（1124 处原型回写 + 0x367D1 重建）。`analysis/*` 由此导出 |
| Ghidra 工程 | `E:\FD2\FlameDragon2.rep` | — | — | 独立第二意见（LE 加载逐字节一致，见 RE_MAP §1）；本轮未依赖 |
| 段镜像源 | `analysis/seg_*.bin` | — | 由 IDB 导出 | **已移出 git**（可从 IDB 重新导出，见下）。其中只 dseg02/dseg03 用于生成运行时数据镜像；cseg01 代码段已弃用 |

## 已移出 git（体积大 / 可由原版程序或 IDA·Ghidra 重新获得）

这些路径已从**工作区与全部历史**中删除（2026-10-11 的 `git filter-branch` 历史改写），
并由 `.gitignore` 阻止再次入库。它们都能从外部 IDB / 原程序重新生成：

| 路径 | 来源 | 如何重获 |
|---|---|---|
| `analysis/seg_cseg01.bin` | IDB 代码段导出 | 保留原因已消失（`fd2_code` 死代码），无需重获 |
| `analysis/seg_dseg02.bin`、`seg_dseg03.bin` | IDB 数据段导出 | 仅在需要**重新生成** `src/fd2_image.c` 时用 IDA 导出两段 |
| `analysis/decomp/**`（1366 个 `.c`） | Hex-Rays | IDA 全量反编译导出 |
| `analysis/disasm_fallback/**`、`analysis/cseg01_disasm.txt` | 反汇编 | objdump/IDA 重新导出 |
| `analysis/arity.json`、`callgraph.csv`、`data_items.csv`、`data_type_overrides.csv`、`decomp_failures.csv`、`dispatch_tables.csv`、`funcmap.csv`、`names.csv`、`prototype_heal_log.json`、`prototypes.csv`、`reloc_suspects.csv`、`strings.csv`、`compile_errors.txt` | IDA/脚本导出 | `tools/*.py` + IDB 重新导出 |
| `src/fd2_image.c`（**已改回入库**） | `gen_headers.py` 由 `seg_dseg0{2,3}.bin` 生成，**只含数据段**（约 197 KB） | 运行时必需（`fd2_data_init` 拷进镜像）；若要重生成，先恢复 `seg_dseg0{2,3}.bin` 再跑脚本 |
| `function-map.csv` | `gen_function_map.py` 生成 | 重跑该脚本（依赖外部的 `port/re/*.csv`） |
| `build/**` | 编译产物 | `pwsh -File build.ps1` |

保留在仓库内的手工/小体积文件：`analysis/data_type_overrides.manual.csv`、
`analysis/prototype_overrides.csv`、`src/include/*.h`（生成但小且编译必需）、
`src/fd2_relocate.c`（生成但极小）、`src/fd2_image.c`（生成但只含数据镜像，约 197 KB）。

> 说明：`gen_image()` 不再生成 **cseg01 代码段**（原 258 KB；51 个引用 `fd2_code` 的宏
> 无一被 `src/game`/`host` 使用，纯死数据）。它曾把 `fd2_image.c` 撑到 1.5 MB。
> 因此**当前检出可直接 `build.ps1` 编译**，无需先重建任何外部产物。

## 版本锚点

- `v0.1-phase1-static`：第一阶段（纯静态逆向）完成快照。包含：
  - 1366/1369 函数反编译导出、1114 个实参个数推导、1041 个数据符号宏（语料 100% 覆盖）
  - 五份知识库文档（overview / function-map / data-layout / module-map / uncertainties）
  - 双 profile 状态模型 + 2 个手工还原模块（res/boot）+ 5 项 gcc 静态验证全绿

## 重建路径（他人/未来自己）

1. 用 `E:\FD2\FD2.EXE`（哈希须匹配）重建 IDA 库 → 若需重生成数据镜像，导出
   `analysis/seg_dseg02.bin`、`seg_dseg03.bin`（cseg01 已不需要）；若要重生成
   `analysis/` 下的表，也一并重新导出。
2. `python tools/gen_headers.py` → 重新生成 `src/include/*.h`、`src/fd2_image.c`（数据段）、
   `src/fd2_relocate.c`。
3. `python tools/gen_function_map.py` → 重新生成 `function-map.csv`（需 `port/re/*.csv`，在 port/ 仓库）。
