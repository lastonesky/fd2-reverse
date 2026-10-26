---
name: ghidra-fd2
description: 用 Ghidra MCP 桥给 FD2.EXE 做第二意见：按地址反编译、查 xref/调用图、找自动分析空洞、批量导出函数清单或反编译文本，并与 IDA 的 FD2.EXE.i64 对表。当任务需要核对某个函数的反编译结构、确认某段内存/数据表布局、或批量产出 analysis/*.csv 时使用。
---

# Ghidra MCP × FD2.EXE

Ghidra 在 port3 里的定位是**第二意见**：权威仍是 `FD2.EXE.i64`（README §1）。
Ghidra 的价值在于独立反编译器、以及可以脚本化批量导出。**不要**用 Ghidra 的产物覆盖
`analysis/` 里来自 IDA 的表，除非已在回复里注明来源和差异。

## 0. 铁律（先读）

- 任何会写文件的动作之前，先看 AGENTS.md §0：`FD2.SAV` 是用户存档，本 skill 全程不需要碰它。
- **不要**在分析会话里调 `save_program` / `save_all_programs` / `exit_ghidra`。
  工程里那份 `FD2.EXE` 是 versioned 但未 checkout 的文件，保存会被拒；而
  `exit_ghidra` 会尝试保存所有打开的程序。实验结束时用
  `close_program(name="FD2.EXE", save=false)` 丢弃临时副本。
- 只看不改：`rename_function` / `set_function_prototype` / `create_function` 都只落在内存里，
  关掉就没了。这类操作只用于"先建个函数看看反编译长什么样"，不要当作分析成果。

## 1. 开工前自检（30 秒）

```
mcp__ghidra_mcp__list_instances        # 有没有 gui/headless 实例、pid、socket
mcp__ghidra_mcp__list_open_programs    # 当前打开了哪些程序
mcp__ghidra_mcp__load_tool_group({"group":"all"})   # 关键：见 §1.1
```

### 1.1 默认只加载 3 个工具组

`function` / `listing` / `program` 是默认组（≈68 个工具），**其余 14 组要显式加载**：

| 需要什么 | 先 `load_tool_group(...)` |
|---|---|
| 打开工程 `open_project`、`get_project_info` | `headless`、`project` |
| 建结构体/枚举、改原型类型 | `datatype` |
| 加注释 | `comment` |
| `find_code_gaps`、`find_next_undefined_function`、`inspect_memory_content`、`run_analysis` | `analysis` |
| xref 批量、symbol 重命名、服务器 checkout | `xref`、`symbol`、`server` |

漏了这步会误以为 `open_project` 不存在（它被归在 `headless` 组，只有 1 个工具）。
`all` 一次性加载 205 个，工具列表会很长但最省事。

### 1.2 脚本通道必须有 Java 侧环境变量

`run_script_inline` / `run_ghidra_script` 的开关在 **Ghidra JVM** 里读
（`com.xebyte.core.SecurityConfig` → `System.getenv("GHIDRA_MCP_ALLOW_SCRIPTS")`，
接受 `1` / `true` / `yes`）。写在 `~/.pi/agent/mcp.json` 的 `env` 只到 Python 桥，
**到不了 Ghidra 进程**。必须在启动 Ghidra 的那个环境里：

```bash
# ~/.local/share/applications/ghidra.desktop
Exec=env GHIDRA_MCP_ALLOW_SCRIPTS=1 GHIDRA_MCP_FILE_ROOT=/mnt/MyData/FD2 ghidra %F
```

验证（返回 `ok` 才可用）：

```
mcp__ghidra_mcp__run_script_inline({"code":"public class V extends GhidraScript { public void run() throws Exception { println(System.getenv(\"GHIDRA_MCP_ALLOW_SCRIPTS\")); } }"})
```

同族变量：`GHIDRA_MCP_FILE_ROOT`（限制文件读写根目录，不设则可能拒绝工程目录外的路径）、
`GHIDRA_MCP_PROJECT_FOLDER`、`GHIDRA_MCP_AUTH_TOKEN`（非 loopback 才需要）。

### 1.3 工程属主必须等于 JVM 的 `user.name`

```
NotOwnerException: Project is owned by Ted      ← 而 user.name=ted
```
修法二选一：Ghidra 启动参数加 `-Duser.name=Ted`，或把
`FlameDragon2.rep/project.prp` 里 `<STATE NAME="OWNER" ... VALUE="Ted" />` 改成当前用户名
（Ghidra 保存时会自己重写这个字段，改动无害；先备份）。

## 2. GUI 模式的已知坑：project 视图错位

`/open_project` 在 GUI 里 `{"headless":"true"}` 会返回 `success`，但随后
`get_project_info` 仍是 `{"has_project":false}`，`list_project_files` / `open_program`
继续报 "No project is currently open"，而且**没有 `close_project` 端点**能解除这个状态。
同时 `import_file` 却能成功——它走的是内存 program manager，生成一份**未分析的临时副本**。

后果：想用工程里那份已分析的 `FD2.EXE` 只能
①在 GUI 里手动双击打开 CodeBrowser，或 ②改跑 headless server。
在此之前把 `import_file` + `reanalyze` 当作"另起一份干净分析"用，别指望拿到既有标注。

两种副本要分清：

| 副本 | 函数数 | 能否保存 |
|---|---|---|
| 工程里的 `FD2.EXE`（`/FD2.EXE`，versioned） | 多（含历史标注） | 需 checkout |
| `import_file` 的临时副本 | 分析后 ≈706 | 否（未 checkout） |

## 3. 地址空间事实（与 IDA 逐项一致，实测）

- 加载器 `watcom:LE:32:default`（Ghidra 自带，DOS/4GW LE 直接开），`image_base = 0`
- **地址与 `.i64` 1:1**：无需任何 rebase，IDA 里叫 `0x2C67D` 这里也是 `0x2c67d`
- 布局：`cseg01 ≈ 0x10000–0x4Fxxx`（代码）；`dseg02 = 0x50000`（+0x56B0）；
  `dseg03 = 0x60000`（+0x4000）；另有 1 个 overlay 空间

自检三地址（都应返回有意义内容）：

```
read_memory 0x1DA16 → 6828000000e8...    代码序言 push 0x28; call chkstk
read_memory 0x50000 → "Out of Memory"     dseg02 字符串表
read_memory 0x2C67D → 686c000000e8...    已知函数的序言
```

## 4. Ghidra 自动分析会漏函数（必须知道）

`reanalyze` 约 7 s 完成，只得到 **≈706** 个函数，IDA 是 **1369**。
典型空洞（实测）：`0x2b843 → 0x2cf30` 整块没建函数，而 IDA 的 `0x2C67D` 就在其中。
根因是 Watcom 的 `chkstk` 序言 + 跳转表让递归下降断链。

处理顺序：

1. `load_tool_group("analysis")`，用 `find_code_gaps` / `find_next_undefined_function` 列出空洞
   （比手算地址差可靠；本 skill 的 `scripts/DumpBlocksAndGaps.java` 也能离线跑）。
2. 对 IDA 里已知的函数入口 `create_function(address=...)`，然后立刻
   `get_functions(address=..., fields="signature,decompiled_code")` 验证。
   补建后 body 大小合理（`0x2C67D` = 1151 B）、反编译里的常量/结构能与 IDA 对上，才说明建对了。
3. 比对结果写进回复或 `PROGRESS.md`，注明"Ghidra 手工补建后一致"。

判据：Ghidra 反编译对 `chkstk` 后的栈帧处理与 IDA 不同（`in_stack_00000000` 之类），
**结构/常量/分支一致**即可，不要追求逐字相同。

## 5. 常用配方

```js
// 反编译一个函数（注意：decompile_function 已并入 get_functions）
get_functions({ address:"0x2c67d", fields:"signature,decompiled_code" })
get_functions({ address:"0x2c67d", fields:"callers,callees,labels" })   // 不解编译，毫秒级
get_functions({ functions:"0x2c67d,0x1da16", fields:"signature" })      // 批量，上限 20

find_functions({ program:"FD2.EXE", offset:690, limit:20 })            // 分页列函数
read_memory({ address:"0x53a45", length:64 })                          // 看数据表
list_strings / search_strings                                          // 找字符串与引用
disassemble_function({ address:"0x2c67d" })                            // 汇编清单
```

批量导出（脚本启用后首选，比逐条 MCP 调用快一个数量级）：
`scripts/ExportInventory.java`（清单 + 可选反编译全文）、
`scripts/DumpBlocksAndGaps.java`（内存块 + 代码空洞）。
用 `run_ghidra_script({script_name:"ExportInventory.java", args:"...", timeout_seconds:600})`；
脚本按 Ghidra 惯例放进 `~/ghidra_scripts`（`run_script_inline` 会自动落盘编译）。

产物位置约定：导出物写 `analysis/` 之外的临时目录，**不要**直接覆盖
`analysis/*.csv`（那些来自 IDA，且多数在 `.gitignore` 里）。

## 6. 收尾

```
close_program({ name:"FD2.EXE", save:false })   // 丢弃临时副本
```

工程 `project.prp` 若被改过，确认值仍合法即可，不必手工回滚（Ghidra 会重写）。

## 7. 故障速查

| 现象 | 原因 / 修法 |
|---|---|
| `env must map names to strings`（pi 启动即报） | `mcp.json` 的 `env` 要写成对象：`{"X":"1"}`，不是 `"X=1"` |
| `Script execution disabled` | §1.2：环境变量没进 Ghidra JVM |
| `NotOwnerException: Project is owned by X` | §1.3 |
| `No project open` / `get_project_info` 说 false 却又不让开 | §2，GUI 错位；换 headless 或手动开 CodeBrowser |
| `Function not found` / `No function at address` | 自动分析空洞，§4 先 `create_function` |
| 工具"不存在"（如 `open_project`、`create_struct`） | §1.1 先 `load_tool_group` |
| `dry_run is not supported by /xxx` | program/project 类端点不支持预演，直接正式调用 |
| 反编译超时 / 结果残缺 | `force_decompile` 重试；或先 `disassemble_function` 确认指令边界 |
