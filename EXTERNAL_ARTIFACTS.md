# EXTERNAL_ARTIFACTS — 仓库外关键产物登记（可追溯性）

> 本仓库（port3）只存还原源码与静态分析导出。以下产物在 `E:\FD2\` 下、**不在 git 内**，
> 记录哈希/时间来锁定"这一版分析基于什么"。改动这些产物需要同步更新本文件。

| 产物 | 路径 | 大小 | MD5 | 说明 |
|---|---|---|---|---|
| 原程序 | `E:\FD2\FD2.EXE` | 509158 | `a6e341a8decc6ebf7f4872076d9cf161` | 逆向目标（DOS/4GW LE）。任何外部资料若哈希不同即为**另一 build**（见 RE_MAP §7.6） |
| IDA 数据库 | `E:\FD2\FD2.EXE.i64` | 13410681 | `f949fe9e46fb37b880cc6bca2bcbc796` | **含本轮全部原型修复/边界修复/命名**（1124 处原型回写 + 0x367D1 重建）。`analysis/*` 由此导出 |
| Ghidra 工程 | `E:\FD2\FlameDragon2.rep` | — | — | 独立第二意见（LE 加载逐字节一致，见 RE_MAP §1）；本轮未依赖 |
| 段镜像源 | `analysis/seg_*.bin` | — | 见 git | 由 IDB 导出，随仓库版本化 ✅ |

## 版本锚点

- `v0.1-phase1-static`：第一阶段（纯静态逆向）完成快照。包含：
  - 1366/1369 函数反编译导出、1114 个实参个数推导、1041 个数据符号宏（语料 100% 覆盖）
  - 五份知识库文档（overview / function-map / data-layout / module-map / uncertainties）
  - 双 profile 状态模型 + 2 个手工还原模块（res/boot）+ 5 项 gcc 静态验证全绿

## 重建路径（他人/未来自己）

1. 用 `E:\FD2\FD2.EXE`（哈希须匹配）重建 IDA 库 → 重跑导出脚本（`tools/gen_headers.py` 依赖
   `analysis/*.csv|bin`，这些已在 git 内；`analysis/prototypes.csv` 等由 IDB 导出，也已入库）。
2. `python tools/gen_headers.py` → 重新生成 `src/include/*.h`、`src/fd2_image.c`、`src/fd2_relocate.c`。
3. `python tools/gen_function_map.py` → 重新生成 `function-map.csv`（需 `port/re/*.csv`，在 port/ 仓库）。
