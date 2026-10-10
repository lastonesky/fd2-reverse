# reverse-engineering-overview.md — FD2.EXE 逆向总体架构

> 第一阶段交付物。全部内容来自既有静态分析（IDA 库 `E:\FD2\FD2.EXE.i64`、
> `port3/analysis/*`、`port/re/RE_MAP.md`、`port/docs/*`），**未重新扫描**。
> 引用格式：`[来源]`；推断与事实分离见各条目标注。

## 1. 程序概况

| 事实 | 值 | 证据 |
|---|---|---|
| 格式 | DOS/4GW LE（Linear Executable），WATCOM C 编译 | `[BINARY-FACTS §3.1]`，LE 头 @文件 0x27ACC（**实测偏移，与标准 LE 不同**） |
| 代码段 | `cseg01` 0x10000..0x4F000（258048 B） | IDA segments `[analysis/funcmap.csv 所在库]` |
| 数据段 | `dseg02` 0x50000..0x556B0（22192 B，初始化数据+字符串+分派表）；`dseg03` 0x60000..0x64000（vsize 0x34D2） | 同上；段字节 `analysis/seg_*.bin` |
| 入口 | `start` 0x3CCB4（obj0+0x2CCB4），CRT 启动 `start→__CMain→main(0x25BF4)` | `[RE_MAP §1]` |
| 函数总数 | 1369（游戏 573 + 工具库 54 + CRT/AIL 库区 743） | `function-map.csv`（zone 列） |
| 资源 | FDTXT/FDOTHER/FDFIELD/FDSHAP/DATO/FDMUS/BG/FIGANI/TAI/ANI.DAT、FDICON.B24、FD2.SAV/TMP | `[FD2_analysis.md]`（容器格式：`'LLLL'`+偏移表/AFM V1.00 工具生成） |
| 音频 | Miles Sound System AIL V3.02 静态链接（51 个命名入口） | `[RE_MAP §3]` |
| 显示 | VGA Mode 13h，320×200×256，直接写 0xA0000 显存 | `[PROGRESS/BINARY-FACTS]` |

## 2. 入口 / 初始化 / 主循环 / 退出

以下流程由 `src/game/boot.c`（本轮手工还原，逐指令地址注释）与 `analysis/decomp/25BF4_main.c` 给出：

**初始化（main 前半 0x25BF4..0x25D8F）**
1. `AIL_startup → AIL_install_MDI_INI → AIL_allocate_sequence_handle`（句柄存 `dword_53ED8/53ED0`，失败旗标 `byte_53EF0`）；
2. `AIL_install_DIG_INI → AIL_allocate_sample_handle ×2`（`dword_53EDC/53EE4/53EE8`，旗标 `byte_53EF1`）；
3. **资源装载**：8 次 `res_load(file, oldbuf, idx)`（FDOTHER.DAT idx 31/1/2/3/4/5/6、FDTXT.DAT idx 0），写入 `dword_53EEC/53A4D/53A89/53A6D/53A75/53A81/53A7D/53AD1`；
4. 堆：`malloc(0x20)→53AD5`、`malloc(0x25680)→53A49`、`malloc(0xA00)→53BF7`（=80×32 条持久记录）；
5. `word_53A8D=0x13; int386(0x10,…)` 进 Mode 13h（同一 REGS 单元退出时置 3 回文本模式）；
6. BIOS tick 快照 `dword_53C0F = movsx word[0x46C]`（BDA 40:6C 低字，**符号扩展**）；
7. `rand()%256` 次 `sub_4EBE3()` 洗随机表（`ROL16(word_627B8-28652)` `[RE_MAP]`）。

**主循环（0x25DB1..0x25E91，两张分派表）**
```
外层: sub_25977(0x12,0)      播 BGM 18
      v = sub_25EBB()        进入场景；v==0 走内层，v==-1 回外层，其它退出
内层: r = sub_117E7()        一个事件 tick
      dword_53ECC==1 → sub_22E5C()，强制 r=1（回外层）
      dword_53ECC==2 → 状态切换: funcs_25E23[state](); r=sub_26152();
                        r==0 → funcs_25E3A[state]() + sub_25977(byte_51E63[state],0)
                        r≠0 → 退出
                        之后 byte_51AAC=1、dword_53ECC=0、sub_4E381()（清键缓冲 BDA 40:1C/40:1A）
      r==0 续内层；r==-1 退出；r>0 回外层
退出: AIL_shutdown → word_53A8D=3 → int386(0x10) → 共享尾声 0x16F04（return）
```
主循环语义已按分支地址逐条核对（boot.c 注释含 0x25DC8/0x25E76/0x25E7C/0x25E88/0x25E8F 等）。

**子系统入口**：`funcs_25E23`（0x51DE9，离开态，约 25 项）/ `funcs_25E3A`（0x51D71，进入态，30 项）；事件层 `funcs_1199C`（0x51B91，91~92 项）、`funcs_1197B`（0x51B19，30 项）、`funcs_1541F`（0x51D01，28 项）；效果动画 `funcs_30469`（0x524C6，10 项）。逐槽数据见 `analysis/dispatch_tables.csv`。

## 3. 子系统与依赖（详表在 module-map.md）

| 子系统 | 职责 | 关键依赖 |
|---|---|---|
| res/资源 | DAT/LMI 容器解包、FDICON 加载、存档 IO | CRT(stdio/malloc)、`dword_53BFF` 发布 |
| gfx/显示 | blit、RLE、24×24 精灵、调色板淡变、0xA0000 | 工具库、`dword_53A65` 调色板副本 |
| map/地图 | 地图视图渲染、揭示、寻路（BFS 洪泛） | gfx、记录表、`dword_53A51` 格子 |
| rec/unit | 80 字节记录表（角色/队伍/物品槽） | 表访问器、持久表 `53BF7` |
| vm/脚本 | 0x15F84 词流解释器（9 栈参，EDI=当前记录） | 记录表、事件层 |
| ev/事件 | 三张事件分派表的 handler 簇 | vm、scene、map |
| scene/状态 | 主状态机（funcs_25Exx）、场景卡 | bgm、menu、ev |
| dlg/msg/UI | 对话框/头像/菜单/数字渲染 | gfx、DATO 资源、svc(等键/打字机) |
| bgm/svc/音频 | 播放/音效（16 个 AIL 入口） | AIL 边界、FDMUS/FDOTHER 资源 |
| 随机/计时 | `sub_4EBE3` 随机表、BDA tick `0x4E310`、`sub_4E381` | 低内存镜像（宿主需仿真） |

## 4. DOS/4GW 运行时与游戏逻辑的边界

- **游戏区（0x10000..0x37000 + 工具库 0x4DED4..0x4F000）内零条 `int` 指令**：
  与 DOS/BIOS 的交互全部经 CRT 包装（`int386/fopen/read/malloc`…）→ 还原时游戏层是纯计算+显存访问，平台层只需实现 CRT/中断服务 `[RE_MAP §2 关键结论1]`。
- 直接操作：0xA0000 显存（`gfx_A0000` 粗筛 172 个函数，**含误报待精化**）、低内存 BDA（tick/键缓冲）、DAC 端口 0x3C8/3C9（经 `outp`）。
- 运行时依赖：LE fixup（119 个数据段代码指针槽，`src/fd2_relocate.c` 生成）、
  Watcom 栈探针 `push size; call 0x3702F`（`__chkstk`，非参数）、
  `int386` 视频模式、CRT 堆（`sbrk/__MemAllocator`）。
- **不还原**：Watcom CRT（→libc/宿主层，`port/src/dos.c` 已有全套服务先例）、
  AIL 内部（→边界桩）、DOS/4GW 启动 stub。

## 5. 地址体系（四类地址严格区分）

| 类别 | 示例 | 还原中的处理 |
|---|---|---|
| ① 原始 DOS/4GW 线性地址 | `0x53A45`（数据）、`0x111BA`（代码） | **仅作为 ID/偏移**保留（注释、宏键）；不当宿主指针用 |
| ② Ghidra 分析地址 | 同上线性（Ghidra 工程 `FlameDragon2.rep` 与 IDA 一致） `[RE_MAP §1 已逐项对表]` | 文档引用用；与①同值 |
| ③ 运行时线性地址（混合环境） | guest 镜像装入后的 0x10000..0x64000、低内存 0x0..0x10FFFF | 仅混合宿主（hybrid profile）存在；映射由宿主完成 `[HOST-DESIGN]` |
| ④ 还原程序宿主地址 | `fd2_data1[]` 数组、真函数符号 | 模块代码只经 `fd2_globals.h` 宏/符号访问 |

**设计假设（记录，非既定事实）**：`fd2_globals.h` 计划提供 `FD2_GUEST_FIXED` 配置
（宏名/偏移不变，基址切到③）以便复用 port/ 的 27 个 `*check` 对拍器；standalone 构建用④。
两 profile 的模块源码完全相同。影响面与替代方案见 `uncertainties.md`。

## 6. 复用资产索引（不要重复生成）

| 资产 | 位置 | 用途 |
|---|---|---|
| 函数级测绘/核心档案 | `port/re/RE_MAP.md` + `funcmap.csv` + `func_ranking.csv` | 优先级、调用统计 |
| 309 个已验证翻译 | `port/re/translation_map.csv`（addr→c_name/file/group/check/cases） | 直接复用源码与对拍器 |
| 踩坑清单（动手前必读） | `port/docs/PITFALLS.md`（§8-xx） | 避免重复踩坑 |
| 二进制事实（LE头/fixup 实测） | `port/docs/BINARY-FACTS.md` | 解析器依据 |
| 宿主/地址空间设计 | `port/docs/HOST-DESIGN.md`、`port/docs/ENVIRONMENT.md` | 混合宿主运行与构建命令 |
| 差分调试手段 | `port/docs/DEBUG-MANUAL.md`（自检/对拍/抓帧/A-B 差分） | 第五/六阶段验证 |
| 转译路线与纪律 | `port/docs/TRANSLATION.md`（§1 七步流程、§6 A→B→C→D 路线） | 批量还原流程 |
| 27 个对拍器 | `port/src/*check.c` | 模块级轻量验证（机器码 vs C） |
| 轮次明细 | `port/docs/rounds/01..25` | 单模块的完整病历 |
