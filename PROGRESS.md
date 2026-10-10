# PROGRESS — port3 还原工程台账

> 更新：**第二阶段任务启动轮**（源码还原/编译/差分验证）。阶段纪律变更：
> 现在允许编译与运行（第五/六阶段）；原程序运行仅限后续对拍需要时。IDA MCP 通道
> 当前断连（协议握手失败），已完成的导出足够支撑计划内工作；新 IDB 查询待重建。

## 0-A. 本轮交付（第二阶段任务 · 第一阶段：成果检查与计划）

| 交付物 | 状态 |
|---|---|
| `reverse-engineering-overview.md` | ✅ 架构/入口/主循环/子系统/边界/四类地址 |
| `function-map.csv` | ✅ **1370 行×18 列**（调用图/模块/六态/重要性/置信度，`tools/gen_function_map.py` 生成，可重跑） |
| `data-layout.md` | ✅ 确认/推测/待核三色标注，指向权威对拍源 |
| `module-map.md` | ✅ 模块清单+优先级排程（P0 主状态机链 → P1 高用量长尾 → P2 存档/两表 → P3 收尾） |
| `uncertainties.md` | ✅ 六节：ABI/控制流/布局/语义/设计假设/动态清单 |
| 覆盖统计 | 309 已验证 + 7 已还原 + 316 待还原 + 683 桩 + 54 映射；**调用量覆盖 71.3%** |

## 0-B. 第三阶段（统一模型）+ 第五阶段（轻量验证）本轮落地

- **双 profile 状态模型**（假设 A 落地）：`fd2_globals.h` 支持 `FD2_GUEST_FIXED`
  （混合对拍：段钉原始线性地址，复用 port/ 的 27 个对拍器+repl.c）与 standalone
  （普通数组+重定位）；模块源码两 profile 完全同源。
- **第五阶段静态验证全绿**（WSL gcc 14.2, `-std=c99 -Wall -Wextra`，零告警）：
  1. standalone 模块（boot/res）✅  2. hybrid 模块 ✅  3. 生成源 standalone ✅
  4. 生成源 hybrid ✅  5. `fd2_funcs.h` 头自测 ✅
- 验证抓到并修复的真问题：短名宏碰撞（参数名剥离）、非法标识符（IF@*）、
  `time_t`/`__noreturn`/`_UNKNOWN` 类型、**重定位误改数据常量**（119 槽重分类：19 映射/100 不动，
  `analysis/reloc_suspects.csv`，详见 uncertainties §3.7）。
- 工具链事实：本机 bash 无 gcc，**WSL gcc 14.2** 可用；IDA MCP 断连待重建。

## 0-D. P0 主状态机链：**核心三件已还原（本轮完结）**

**已还原（`src/game/scene_core.c/.h`）**

| 原址 | 新名 | 内容 | 验证 |
|---|---|---|---|
| `0x25EBB` | `scenario_menu` | 新游戏/读档/存档三分支；FD2.SAV 22987B → `util_deobfuscate` → 槽位循环（2600B 步长→队伍表 0xA00 + 9 字节头部）→ `scene_apply_state` | 双 profile `-Werror` 通过；**未动态验证** |
| `0x117E7` | `scene_event_tick` | 内层每 tick：行动者轮换 / 四方向步进 / 确认→记录选择→`funcs_1197B[state]`+`funcs_1199C[0x51A8F]` | 同上 |
| `0x26152` | `scene_apply_state` | 释放 5 个场景缓冲（含 IDA 短名 `p`/`buf`=0x53A55/0x53A61，已模块内别名化）→ 按队伍重载 FDICON → 分支 A（`byte_523E7[state]!=0`：地图流程）／分支 B（3 字节表 `{0x0B,0x3D,0x3E}` 选背景 + INT 16h AH=10h 扫描码交互循环 + tick 帧动画 + 热键表 `sub_4E809(state)[1]/[2]`） | 同上，**含离线反汇编逐条核对** |

- 新增**离线反汇编链路**（IDA 通道断连期间的关键手段）：`analysis/seg_cseg01.bin` +
  `objdump -D -b binary -mi386 -Mintel --adjust-vma=0x26152` → 解决了伪代码 4 处歧义
  （局部 3 字节表源自 0x52405、old-buffer 恒 0、`__PAIR64__`=两次字节比较、返回值 `dword_53F4A != 2`）。
- 新增**原型覆盖机制**（`analysis/prototype_overrides.csv`）：`sub_1F894`/`sub_11AA8`→`int(void)`。
- 生成器修复：字符串标签判据统一（影响 `aRb_0`/`aFd2Sav_4` 等）。
- 回归：boot/res/scene_core + 生成源，**双 profile `-Werror` 全绿**；表覆盖已按“首个非法槽即表尾”重算。
- `function-map.csv`：待还原 316→**313**，已还原未验证 7→**10**。

**表覆盖（真实表长）**

| 表 | 表长 | 去重 | 已验证 | 待还原 |
|---|---|---|---|---|
| `funcs_1197B` | 30 | 18 | 18 | 0 |
| `funcs_1199C` | 90 | 90 | 90 | 0 |
| `funcs_25E3A` | 30 | 28 | 18 | **10** |
| `funcs_25E23` | 30* | ~25 | 6 | **24** |
| `funcs_1541F` | 28 | 27 | 0 | **27** |
| `funcs_30469` | 11 | 10 | 9 | **2** |
| `funcs_37012` | 10 | 10 | 0 | **10** |

\* 首个非法槽规则给 30；RE_MAP 记 25 项 ⇒ 尾部几槽疑为邻接数据（待核）。

**下一批**：`funcs_25E23` 余 24 项（scene_state_* 模板批量）→ `funcs_25E3A` 余 10 项（`0x3231B/32D18/32E8C/32FB2/33049/336A0/33AF1/33C9D/33DBA/33E3C`）→ 启动 309 资产迁入。

## 0-E. `funcs_25E23` handler 批量还原：batch 1（本轮）

**已还原**（`src/game/scene_states.c/.h`，每一条都逐指令核对了离线反汇编）：

| 索引 | 原址 | 新名 | 行为 |
|---|---|---|---|
| [5] | `0x23296` | `scene_state_05` | `unit_add(13); 0x10B4E(3); 0x135DD(5,14); 0x1366A(27); vm(6); refresh; ++state` |
| [8] | `0x235BC` | `scene_state_08` | `rec[11].+5=0; 0x135DD(6,1); 0x10B4E(4); 0x1366A(36); vm(4); refresh; ++state` |
| [24] | `0x24DF2` | `scene_state_24` | `vm(6); 0x135DD(4,16); 0x10B4E(2); 0x1366A(75); vm(7); unit_add(26); refresh; unit_add(29); ++state` |
| [27] | `0x25464` | `scene_state_27` | 空体：`vm(7); refresh; ++state` |

**共享尾声解码表（离线反汇编，全部已解）**

| 尾声地址 | 语义 | 归属 |
|---|---|---|
| `0x231C6` / `0x231DF` | `vm_run(stream, sub, 0xA0000,320,205,76,74,19,1) + unit_refresh + ++state`（sub 由跳转者压栈；0x231DD 的 `push 4` 属 sub_231BC 自身） | sub_231BC 体内 |
| `0x231F2` | `++state; retn` | sub_231BC 体内 |
| `0x2327D` / `0x2328A` | `unit_add(arg)+refresh+++state` / `++state`（**是 sub_230F2 自己的尾声**） | sub_230F2 体内 |
| `0x237C8` | `unit_add(arg)+++state` | sub_23790 体内 |
| `0x239AC` / `0x239B1` | `refresh+++state` / `++state` | sub_239BD 体内 |
| `0x15487` / `0x1B5EA` | 其他模块的纯 epilogue（`return <eax>`） | 外部函数 |

**已备好但待写**（需 `scene_place_records`（0x233C6，已验证资产）先迁入）：
idx [2] `sub_230F2`（三张 7 字节表 0x520BA/C1/C8 = xs{8,7,9,6,10,8,8}/ys{3,3,3,2,2,4,1}/kind{2,2,2,3,1,2,0}；`rec_flag(6)` 分支→vm6 或 place_records(...,0,6,0,0,0,0,2)+vm7）、
idx [4] `sub_231F9`（xs{12,11,13,10,10,14,14}/ys 字面量/kind{2,2,2,3,3,1,1}；place_records(...,0,6,41,12,8,0,origin=0x400000006)+vm9+unit_add(10)+refresh+ ++state）。

**余量**：`funcs_25E23` 待还原 24→**20**；`function-map.csv` 已更新。

---

## 0-F. 309 已验证资产迁入（完成）+ batch 2

**结果**：`port/src/game` 的 31 对 `.c/.h` + `guest_mem.*` + `dos.h/platform.h/le.h` → `port3/src/`；
**双 profile 零错误、`-Werror` 全绿、伞头自测通过**（`tools/check_compile.sh [--full]`）。

**新增基础设施（可重复）**

| 工具/产物 | 作用 |
|---|---|
| `tools/extract_data_types.py` → `analysis/data_type_overrides.csv` | 从 port/ **模块源**（不含检查器）提取声明：147 条（lvalue/ptr），同地址多声明按“指针优先”打分消解，30 条冲突已标注 |
| `analysis/data_type_overrides.manual.csv` | **按符号名**覆盖（同一地址两个名字语义不同：`byte_53D34[i]` 需字节指针视图、`(&dword_53D34)[i]` 需标量 lvalue） |
| `tools/migrate_port_assets.py` | 拷贝 + 剥离两种数据宏声明（305 处）+ 冻结文件级补丁（`world_load_alias.inc`） |
| `src/include/fd2_aliases.h` | **298 条 IDA名→语义名**（11 条 c_name 是 `static` 文件内助手如 `tbl_6238D`，**不生成别名**） |
| `src/game/fd2_game.h` | 伞头（globals+funcs+aliases+全部模块头） |
| `src/game/guest_mem.h/.c` | 沿用 port/ 堆缝（standalone=libc，hybrid=游戏堆） |

**生成器强化**：数据宏不再为「迁移函数名」（`migrated_names`）与「源里定义为函数的任何名字」（`_FD_DEF_RE` 扫描，含 static 助手）生成——退出宏/函数撞名。

**过渡期宽容（已记录，去 guest 阶段清理）**：`-Wno-int-conversion/-Wno-int-to-pointer-cast/-Wno-pointer-to-int-cast`。
原因：port/ 各文件对同一指针单元有 uint32_t/指针两种写法，统一后部分赋值靠过渡期隐式转换（32 位下位级等价，假设 B）。检查脚本头注释已说明。

**本轮登记的待确认项**

1. **`0x16559` ABI**：机器码仅压 1 个实参（`0x262D8`），port/'s `dlg_blit_dato` 取 3 参且 `dlgcheck` 800 例通过（其调用点传 `(dword_53A85, dword_53C67, idx)`）。`scene_core.c` 暂按 port/ 签名调用并注释；需动态阶段定论。
2. 11 个 `static` 表访问器（`tbl_4E7DD/4E7F2/4E809/4E821/4E838/4E84F/4E866/4E87D/4E88E/4E8A5/4E8BC`）：调用方改用已迁入的 `tbl_ptr(base,stride,index,offset)`（`scene_core.c` 已示范 0x4E809→base 0x6238D/stride 31/offset -31）。
3. `world_load.c` 的 IDA 短名 `p`/`buf` → 模块内别名（`world_load_alias.inc`）。

**下一步**：`funcs_25E23` 余 9 项（idx 17/19/20/21/22/25/26/28/29）—— 其中 25/26（`0x24E80`→尾 `0x15487`、`0x250CC`→尾 `0x1B5EA`）是**外部 epilogue**（返回 eax，非简单推进状态），需单独核；其余按模板推进。→ 然后 `funcs_25E3A` 余 10 项 → 解释器簇。

## 0-G. `funcs_25E23` batch 3/4（本轮）—— **21/30 已源码化**

| 索引 | 新名 | 要点 |
|---|---|---|
| [6] | `scene_state_06` | 表 0x520E4/ED/F6（9B）+ `dword_53AD5[17]`/`rec_flag(43)` 双条件；origin=0x200000006 |
| [7] | `scene_state_07` | 表 0x520FF/52109（10B）+ kind 字面量 2；末尾 `unit_add(5)`（尾 0x2327D）；origin=0xE00000008 |
| [11] | `scene_state_11` | 表 0x52129/2137/2145（14B×14）+ `unit_add(17)`（尾 0x239B1）；origin=4 |
| [13] | `scene_state_13` | 表 0x52153/2163/2173（16B×16）+ `unit_add` 前置 `sub_10B4E(1)`；origin=0xA0000000C |
| [23] | `scene_state_23` | 行滚动 2..9（每行 30×（视图+等 tick））+ 行 9..15 每行 12 步淡出（fade 递增）+ 清屏 |
| [1] | `scene_state_01` | 扫记录 5..10 的 +5 bit0 选 VM 单元格；三段固定延时；**末尾 `dword_53C03 = 2`（赋值非自增）** |
| [9] | `scene_state_09` | 写 11 条记录（表 0x52113 + 11B 字面量）+ 三条记录（+4000/4080/4160）；设视图 origin qword；脚本序列 |
| [15] | `scene_state_15` | 表 0x52183/2193（16B×16）+ `rec_flag(66..73)` 计数 >4 置标志；条件含 `dword_53BEF>18` |
| [16] | `scene_state_16` | `unit_exists(18)` 双分支；表 0x521A3/21B3；origin=0x1100000011 |

- 共享尾声语义在本批再次验证（0x2327D=unit_add+refresh+inc、0x239B1=inc）并**写进 C**（不模拟跳转）。
- `-Werror` 抓到 `qword_53AA9` 被同址 `int32_t` 声明赢走 → 手工覆盖 + **提取器加"宽度优先"消解**（64>32>16）。
- 验证：双 profile 0 错误、`-Werror` 双 profile 全绿、伞头 OK。

---

## 0-H. **`funcs_25E23`（主状态机"离开态"表）30/30 全部源码化** —— 本轮里程碑

| 来源 | 数量 | 内容 |
|---|---|---|
| port/ 已验证资产 | 6 | idx 0/3/10/12/14/18（`scene.c`，scenecheck 100 例） |
| port3 新还原 | 24 | `scene_states.c`：idx 1/2/4/5/6/7/8/9/11/13/15/16/17/19/20/21/22/23/24/25/26/27/28/29 |

- **表长经覆盖核查定论**：槽 0..29 全为真函数、30 起为越界垃圾 ⇒ **30 项**（旧文档"25 项"偏差已在 uncertainties §2.4 标注解决）。
- 模式固化：表数据按镜像偏移直传 `scene_place_records`；`origin` 64 位按两压栈 dword 解出；共享尾声（0x231C6/0x231DF/0x231F2/0x2327D/0x2328A/0x237C8/0x239AC/0x239B1）语义直接写进 C；两个**外部 epilogue**（0x15487/0x1B5EA，返回 eax）与两个**故意死循环**（idx 26/29 的 `while(1);`）均按原样还原。
- 验证：**双 profile 0 错误、`-Werror` 双 profile 全绿、伞头 OK**；`function-map.csv`：待还原 298→**289**，已还原未验证 25→**34**。

---

## 0-I. **`funcs_25E3A`（主状态机"进入态"表）30/30 全部源码化** —— 本轮里程碑

| 来源 | 数量 | 内容 |
|---|---|---|
| port/ 已验证资产 | 20 | 槽 5..25 的 `menu_*`（`menu_actions.c`） |
| port3 新还原 | 10 | `scene_enter.c`：槽 **0/1/2/3/4/22/26/27/28/29** |

- 覆盖核查：两表（`funcs_25E23`/`funcs_25E3A`）**extent=30、覆盖 30、缺 0** ⇒ **主状态机两张表全闭合**。
- 新增尾声语义（离线反汇编核实）：`0x3312D`=vm_run(sub 由跳转者压栈)+`0x134E4`+`0x12D7B(0)`；`0x33206`=vm_run+`0x33140`（同 0x12D7B(0)）；`0x3313B`=`0x134E4`+`0x12D7B(0)`；`0x33594`=`0x12D7B(0)`。各跳转前压的 sub 值：32FB2→1、33DBA→8、32D18→3、32E8C→3、33AF1→7、33E3C→(无 vm)。
- 验证：**双 profile 0 错误、`-Werror` 双 profile 全绿、伞头 OK**；`function-map.csv`：待还原 289→**279**，已还原未验证 34→**44**。

---

## 0-J. 解释器簇起步 + `0x16559` ABI 定论（本轮）

**1) `0x1AA1D` 还原为 `ev_script_run`**（`src/game/ev_script.c`）
事件脚本解释器：`(rec_index, steps, script)`，逐条 3 字节 `{op,u16 arg}`：
- `op0` 头像对话（`dword_53AD9=arg+181`）→ `rec_slot_claim` 失败时开框/等键/跑 `0x19953`+`map_draw_text_window`，成功且 `dword_53C57==0` 时走 `rec_field_byte`/`rec_slot_remove` 并 `continue`；
- `op1` 头像脚本（`dword_53AE1=arg` → `vm_run(sub 435)` → blit/等键/关框 → `dword_53BF3 += arg`）；
- `op2` `j___delay(200)` + `funcs_1199C[arg](rec_index)`；`op3` `vm_run(sub=arg)`；
- 尾声 `0x22BBE` = 纯返回（帧匹配）。

**2) `0x16559` ABI 定论（原先的不确定项，已解决）**
- 机器码**真 ABI = 1 个实参**（DATO 子图索引，`eax=[esp+8]`；缓冲 `dword_53A85` 与框位 `dword_53C67` 在**函数内部**读取）——全段 20 个调用点均只压 1 个 0，无一例外；`0x16559` 本体反汇编逐条核实。
- port/ 的 `dlg_blit_dato(dato_buf, box_pos, idx)` 是**行为等价的测试友好重构**（把两个全局显式化），其调用点显式传 `(dword_53A85, dword_53C67, idx)`。
- 处置：保留 port/ 的 3 参 API（避免改动已验证资产与其检查器），新调用点同样传三参；不确定性登记改为已解决。

**3) 顺带核查**：`0x197E5` **早已是已验证资产**（`map_draw_text_window`，mapcheck 122500）——旧"解释器簇待办"清单含它是过期信息。

**4) 新增全段离线反汇编资产**：`analysis/cseg01_disasm.txt`（79691 行，`objdump --adjust-vma=0x10000`），后续核 ABI/指令边界/立即数陷阱可直接 grep，不必依赖 IDA 通道。

**余下解释器簇**：`0x19953`(1188B)、`0x1DB65`(857B)。

---

## 0-K. P0 完成度核对（2026-10，脚本核验）

| P0 项 | 状态 |
|---|---|
| `main` 0x25BF4 骨架 | ✅ port3 已还原（boot.c） |
| 主循环 `0x25EBB` / `0x117E7` / `0x26152` | ✅ port3 已还原（scene_core.c） |
| 转场卡 `0x22E5C` | ✅ 已验证资产（scene.c） |
| `funcs_25E23` 离开态表 | ✅ **30/30** |
| `funcs_25E3A` 进入态表 | ✅ **30/30** |
| 解释器簇 `0x1AA1D` / `0x197E5` | ✅ 已还原（ev_script.c） / 已验证（map.c） |
| **解释器簇 `0x19953`（地图窗，1188B）** | ❌ **待还原** |
| **解释器簇 `0x1DB65`（notify，857B）** | ❌ **待还原** |
| 依赖：`0x25977`bgm / `0x4EBE3`随机 / `0x4E381`清键 / `0x4E310`tick / `0x4E31C`调色板 / `0x4EBFF`文本blit / `0x11EEE`地图核 | ✅ 全部已验证资产 |

**结论：P0 = 16/16 ✅ 已闭合**（2026-10 收口）：`0x1DB65` → `map_units_animate`（`map_units.c`，视图内单位精灵动画：3 段帧循环 13+6+6、`malloc(0x25680)` 暂存、`memmove` 还原）；`0x19953` → `map_window_run`（`map_window.c`，交互地图窗：rand%30+2 空闲帧、4×4 像素揭示、每 ≥2 tick 步进 `pal_anim_step`、`dword_53C13` 0..3 相位、**返回 1/-1** 供 `scene_apply_state`/`ev_script_run` 分支）。两处伪代码陷阱均已解：`&loc_1A599+3`=立即数 `0x1A59C`、`&loc_2567F+1`=`0x25680`、`sub_4E31C` 实为 0 参。
注意 P0 闭合 ≠ 可运行：整体运行还需 P1/P2 长尾 + 平台层/AIL 桩接线（module-map §3/§4）。

---

## 0-L. P1 启动（本轮）：fx 收尾 + 两簇源码化

### 1) `0x2C67D` → `fx_dots5`（`src/game/fx.c`）——P1 头号项，fx 分派表闭合

- `funcs_30469[6]` 的 1151 B handler，也是该表**最后一块**；此前被单独摘出（唯一含 CRT `cos/sin`）。
- 双效果共用：`selector 1/2/7/8` 把 5 粒子摆在椭圆上（角度 `72*n` 度，半径 `byte_540C9`，
  y 基 `v23`=30，`rec[+6]==0` 时 90）：
  `x[n]=(int)(cos a*byte_540C9+v23)`、`y[n]=(int)(sin a*byte_540C9*1.2+30.0)`；两处常量
  经段镜像核出（`0x502AA`=double 1.2、`0x502B2`=double 30.0、float `0x3C8EFA2D`=π/180）。
  两张只读表 `0x52605`(5 dword)/`0x52619`(5 B) 从镜像复制；`rec[+6]==0` 时前三项取反/清零。
- `selector 0/3/6` 装填/复位旋转子（`byte_540C9/CA`、`dword_54090/98`、`dword_540A4/AC`），
  `4/5` 为每帧五槽（`dword_540B0[]` 相位、`byte_540C4[]` 子图计数），每 5 帧发一次音效、
  相位命中 2 时返回 1。
- **离线反汇编定案**：Hex-Rays 把 `fild/fmul/fadd/__CHP/fistp` 渲染成 `_CHP()` + 乱序
  `__int64`，逐指令回读 `0x2C7DD..0x2C890` 得上述两条公式；`__CHP`=截断转换（CRT 助手）。
- **fx 表语义完整性**：`funcs_30469[0..9]` 现已 10/10 全部源码化（`fx.c`/`fx.h`），
  `sub_31266` 小分派器只剩 `0x2FB2C/0x2FE14` 依赖。

### 2) `0x182AD/0x18312/0x1839B/0x18409` → `fb_slide_band_a/b/c` + `fb_slide_step`（`src/game/fb_slide.c/.h`）

- 对话框/地图框 12 步滑动转场的最底层合成器：三条 320 行带搬运（`memmove`，步长 320、
  常量偏移 `0x8C0/0x8C5`、`0x5C/0x91C`、`0x5/0x7585`），`0x18409` 每步
  `memmove(dst,saved,0xFA00)` → 三条带 → `memmove(0xA0000,dst,0xFA00)`。
- 调用方证据：`0x17E0B`/`0x1B932`/`0x1BFFE`（`dword_53C5B/5F/63` 三块 0xFA00 暂存）。
- ABI=`(int x, char *dst, char *src[, char *saved])`；负 x 移位并按 `-x` 裁剪字节数，
  逐指令核对。无对拍器，标“已还原未验证”。

### 3) `0x2FACD` + `0x18C6D`/`0x18795`/`0x17D6F` → `src/game/ui_meter.c/.h`

- `0x2FACD`（u15，当时 P1 剩余最高用量）= `ui_status_panel_at`：选 HUD 槽位（+1451 或
  +49280；`dword_53C03==24 && rec==17` 强制 +49280）后转 `ui_status_panel`。
- `0x18C6D` = 状态面板：`rle2_blit` 背景 → 两条 `ui_meter_bar`（记录 +64/66 与 +68/70 的
  `int16` 值/上限）→ `dlg_draw_number`(+33, base 31, 2)/`dlg_draw_number_pair`×2
  → 9 参 `vm_run`（名字流）；末 `jmp 0x1317D` = 普通 `return`。
- `0x18795` = 值/上限→段数（有符号 `101*value/max+1`）；`0x17D6F` = 按段画 tile
  （`dlg_frame_tile`，tile / +1 / +2；count==0 时固定 101 段 29/30）。
- **closure 校正**：`16 进制` 的“共享尾声跳转”会被 `function-map` 算成 callee（`0x18C6D`
  的 `jmp 0x1317D` 落在 `0x1300D` 体内），故不要按该近似的闭包直接展开——真依赖只需
  本 4 个函数。

### 4) `0x219AD` + `0x22046` + `0x24618` → `src/game/reveal.c/.h`（扩张圆开图）

- `0x24618`（u8，本轮最高）= `portrait_reveal`：`ev5_1D4CB` → malloc 0x25680 →
  `map_render_view(13×8)` → 循环 i=9..1（`memmove` 暂存 → `reveal_band(col*24+12,row*24+16)`
  → `gfx_copy_rows(0xA0504, 312×192)` → x+=step → delay5）→ `free` → delay500 →
  `pal_fade_add(0,255,j)` 步进 → `ev5_1D4CB`。
- `0x22046` = `reveal_band`：两次 `reveal_arc`（第二次 y 从圆心起，中间夹 `dlg_portraits_refresh`）
  + 下半 `(int)(radius*1.6)` 半宽的实心带；`0x219AD` = `reveal_arc`：逐行
  `half=(int)(sqrt(r²-dy²)*16/10)` 并按 312 裁剪，`util_translate` 输出。（常量 `0x501F0`=10.0、
  `0x50208`=1.6 经段镜像核出。）
- **closure 校正（再次）**：`0x219AD` 的 `jmp 0x1951B` 落在 `0x190AC` 体内、`0x24618` 的
  `jmp 0x10B46` 属外部尾声——两者都是伪 callee，真依赖只要本 3 个函数。

### 5) `0x2189A` + `0x26CE4`（本轮）—— 另两个独立闭合项

- `0x2189A` → `portrait_reveal_cell`（追加到 `reveal.c`）：以**记录地图格**为中心的开图，
  `vx=(rec[0]-ox)*24+12`、`vy=(rec[1]-oy)*24+18`，半径从 `a2` 起每帧 `+=a3`，10 帧后
  `map_view_update(0)`。
- `0x26CE4` → `screen_curtain`（新模块 `screen_fx.c/.h`）：全屏百叶转场——暂存 0xA0000 →
  20 行 `memset(p+320i+54281,74,104)` → 4 趟滑出 `dword_53F66` 的 4 张子图
  （除数 `mode==0?4-j:j+1`，基偏移表 `0x52408`={-39,-13,13,39}）→ `mode!=0` 保留末帧否则还原。

### 6) 道具/治疗使用簇 → `src/game/item_use.c/.h`（本轮）

- `0x13FD4` → `item_use`（u6）：滑出肖像 → 两次精灵闪烁（`view_draw_unit_sprite` + `gfx_copy_rows`）
  → HP 加 `max/5`；入口要求“受伤且 +37/+38 为零”。
- `0x22AF6` → `item_apply`：先 `map_draw_party_icons(_anim)`，再逐记录治疗 10（`unit_heal`+
  `map_enqueue_status`+清字段+经验）或排入光标精灵。
- `0x1C916` → `unit_heal`：加 `9·amount/10 + (rand%100)·amount/1000`，按 max 截断，
  按 `40·rec[33]·heal/max` 记账 `dword_53EC8`；返回本次增量。
- `0x1DA16` → `view_draw_unit_sprite`：地图格内 24×24 精灵（方向 +3、帧 +2、相位 `53C07/53C0B`），
  越界丢弃；`mode` 0=plain / 2=const。
- **ABI 注**：机器向 `0x4E127` 多压一个实参，但被调方从不读第 4 参（`[ebp+0x14]` 无引用）；
  已对拍验证的 `sprite24_const` 只取 3 参，故 C 丢弃多余实参。

### 7) `0x21EB1` + `0x27816` + `0x278E7`（本轮）+ Ghidra 交叉验证

- `0x21EB1` → `portrait_reveal_zoom`（追加 `reveal.c`）：以格 `(0x53AB9,0x53ABD)` 为中心，
  半径从 `radius` 每帧 `+=step` 共 9 帧，然后再在末半径上重复 7 帧（`i=3..9`）。
- `0x27816`/`0x278E7` → `screen_band_forward/backward`（追加 `screen_fx.c`）：284 字节带
  在 `0xA8FCA` 与两个后备拷贝 `0xA974A`/`0xA99CA` 之间三步搬运 + `0xAEC4A`/`0xAE9CA` 填充。
- **Ghidra MCP 交叉验证**（与 IDA 互补）：对 `0x2C67D / 0x2189A / 0x21EB1 / 0x27816 / 0x278E7 /
  0x1DA16` 分别用 Ghidra 反编译比对，**结构/常量/公式全一致**（特别是 `0x2C67D` 的
  `cos·byte_540C9+v23` 与 `sin·byte_540C9·1.2+30.0`、`0x1DA16` 的视图边界式、以及两条
  屏幕带循环的 `0x140/0x11C/0xa8fca/0xa974a/0xa99ca` 常量）。Ghidra 不恢复 chkstk 后的
  栈参，故用于**结构/常量互证**而非 ABI。

### 8) 队伍状态屏簇 → `src/game/status_screen.c/.h`（本轮）

- `0x26EDA` → `status_screen_run`（u6）：拷 `0x52418` 到本地 4-dword layout → BDA tick
  驱动面板动画（`dword_53F52` 步进 / `dword_53F72` 帧）→ 轮到 `dlg_blit_dato(0)`/`(3)`
  交替头像 → `kbd_pending()` 后 INT 16h 取键（含 E0/52→1C、53→1 重映射）。
- `0x27079` → `status_panel_draw`（mode 0=高亮头像、1/3=帧带、2=图标行）。
- `0x2810B` → `status_names_draw`：最多 6 行头像（`sprite24_plain49`）+ 名字流（`vm_run`），
  高亮行用亮调色板（205/201）。
- Ghidra 对 `27079/2810B/184C0` 的结构反编译**逐分支一致**（尤其确认 `184C0` 的
  `rec[2*i+10] & 0x80` 循环条件，澄清了 IDA 的 `v15>=0` 写法）。

### 9) 消息/道具弹框簇 → `src/game/box_popup.c/.h`（本轮，6 函数）

- `0x17FC0` `box_status_draw`：两血条 + 数字 + 三条 `vm_run` 名字流 + 框图块。
- `0x184C0` `box_items_draw`：逐槽跳 `rec[2*i+10]&0x80`，按道具类型（`tbl_ptr` base
  `&fd2_data2[0x2AD]`/stride 23）选 tile 64..67 + 数字，`+0x40` 位加 3。
- `0x17EEF` `box_open_init`：`res_load(DATO.DAT)` → `dlg_box_stage` → 两条 mask 条 → 面板。
- `0x17E0B` `box_open_anim`：三块 0xFA00 暂存 + `box_open_init`/`box_items_draw` + 12 步 `fb_slide_step`。
- `0x1B9DE` `box_input`：上/下/翻页 + 确认（经道具表 `[13]` 检查），ESC 返回 -1。
- `0x1B932` `box_popup`：循环到确认/取消，滑出，释放三块暂存（返回 `ret != -1`）。
- **发现并绕过已验证资产的缺口**：`0x16C57` 机器码**返回扫描码**（`movzx eax,[0x53A8E]`），
  但 port/ 的 `dlg_wait_key` 声明为 `void`。为不动已验证资产，调用点改为
  `dlg_wait_key(0); key = byte_53A8E;`（行为等价，已写入注释）。

### 10) 地图交互环 → `src/game/map_interact.c/.h`（本轮）—— P1 头号 u9 收口

- `0x115B6` `map_pick_record`（u9，本轮最高）："选记录"交互环——`map_wait_key` 取键，
  方向键走 `ev4_11B48/9B/FA/C59` 视窗步进 + `svc_play_sfx`，逗号/点号在 `list` 中轮换，
  回车/空格按 `mode`（4/5/6）做不同确认（含 mode==6 的重复记录去重 + `rec_skip` 检查）。
- `0x12DAC` `map_wait_key`：等键 + `pal_anim_step` + tick 变化时 `map_view_update(0)`。
  （机器首轮读未初始化的 `esi`，已在 C 里定为 0，注释说明。）
- `0x14742` `map_find_records`：曼哈顿半径内按 `rec[5]&1` 与 `rec[6]` 状态类收集，返回计数。
- **Ghidra 结构比对**：`0x115B6` 的双层循环/`mode` 分支/重复去重与 C 版**逐分支一致**。

### 11) 地点/场景菜单选择环 → `src/game/menu_select.c/.h`（本轮）

- `0x27D33` `menu_list_select`（u6，535B，本轮最高）＝地点菜单（`dword_53F4A` 选定）的
  滚动列表选择环：`guest_malloc` 三块 `0xFA00`（共享全局 `dword_53C5B/F/63`）→ 快照 VGA →
  `rle2_blit` 面板子图（`dword_53F66+[+70]` @ `+35845`）→ `status_names_draw` 布名行 → 6 条
  `msg_blit_band` → 循环 `status_screen_run(3)`，上/下（`4B/4D`）走一行、翻页（`48/50`）走两行，
  越过窗口界线 `dword_53F4E±2` 时 `screen_band_forward/backward` 滚动，回车/空格返回 1、Esc 返回 -1。
- `0x26E38` `menu_choice4`（u3）＝同一渲染器上的四槽环绕选择环：`status_screen_run(0)`，
  左右在 `dword_53C57`（0..3）环绕，回车/空格 1、Esc -1。两者共用同键值表（`4B/4D/48/50/1C/39/1`）。
- **共享尾声 + 有符号比较（陷阱）**：`0x27D33` 的反编译把三个下行分支折成 `LABEL_7/9/13`
  goto；本 C 用 `scroll_down`/`scroll_up_tail` 标号等价还原（已逐指令核对 `27e1a..27f49`）。
  机器对 32 位全局 `dword_53C57/53F4E/53BFB` 用 `jge/jle/jl`（**有符号**）比较，C 侧显式
  `(int32_t)` 转型以保原语义（`-Wno-sign-compare` 之外仍按原样写）。
- **伪 callee**：`0x27D33` 曾误挂 `sub_3702F`(chkstk)；真实依赖仅 `rle2_blit/status_names_draw/
  msg_blit_band/screen_band_forward/backward`（全部已还原）＋ `0x26E38`。闭包=本 2 函数。
- **未在机内释放**：`0x27D33` 只分配不释放 `0x53C5B/F/63`——这三个全局与 `msg.c`(0x1956B/26996)、
  `box_popup.c`(0x17E0B/1B932) 复用，释放点在 `msg_close_portrait`/`box_popup`，故 C 版同样不 free。
- **相邻未还原**：`0x2921A`（u4，230B，中心缩放最近邻 blit 到 `dword_53A49`）为同一批地点菜单
  驱动的退场缩放，本轮未纳入（可与 `0x26A0D`/`0x28B41` 等菜单驱动一并拉出）。

### 12) 地图队伍状态面板 → `src/game/party_panel.c/.h`（本轮，6 函数）

被 0x1B1E7（地图队伍弹出面板，仍待还原）驱动的整簇。0x1B41D 用已还原资产
（`res_blit6`/`dlg_draw_number`/`vm_run`/`ev5_1B5F1`/`unit_exists`）把一行行的头像 tile、
数字和两条名字流画进一块 0xFA00 暂存缓冲；另外 5 条 band 助手再把该缓冲中的横带按
竖直滑动偏移搬进帧缓冲 `dword_53A49`，即队伍面板的滑入/滑出：

| 原址 | 新名 | 几何 |
|---|---|---|
| `0x1AF1E` | `party_panel_band_a` | 170B×117 行；源行随 step 下移（75→…），越界按 `-` 裁剪 |
| `0x1AF99` | `party_panel_band_b` | 170B×117 行；源偏移固定 11915，目标行随 step |
| `0x1B019` | `party_panel_band_c` | 102B×17 行；源偏移 `109+320*r` |
| `0x1B0AD` | `party_panel_band_d` | 170B×16 行；源偏移固定 49675 |
| `0x1B14B` | `party_panel_band_e` | 63B×15 行；源偏移固定 55169 |
| `0x1B41D` | `party_panel_draw` | 4×`res_blit6`(tile 0x85..0x88) + 6×`dlg_draw_number` + 2×`vm_run`(sub `2*dword_53C03+597`/`+598`，`dword_53C03==16 && unit_exists(18)==0` 时前者 `-2`) |

- 每步的负中间值（`srcy<0`/`t<0`）在机器里就是“片段长度裁剪 + 源/目标归零”，已逐指令
  在离线反汇编核出（`1af1e..1b1e6`、`1b41d..1b5f0`）；常量 0x2E40(11840)/0x2E8B(11915)/
  0xC20B(49675)/0xD781(55169)/0x6D(109)/0x4B(75)/0x81(129)/0xAA(170)/0x66(102)/0x3F(63) 均来自段镜像。
- **伪 callee**：五条 band 的唯一“被调”是 `sub_3702F`(chkstk) + memmove；真依赖只有 memmove。
- **未纳入**：编排者 `0x1B1E7` 在循环里读**未初始化的 ebp**（BIOS tick 基线，全函数未赋值、
  未保存该寄存器），语义含歧义，故本轮只拉出 6 个干净 callee（`0x1B1E7` 现已闭包就绪）。

### 13) 地图滚动步进簇 → `src/game/map_scroll.c/.h`（本轮，6 函数）

把队伍记录/地图视口一起推动一格的动画簇（`scene_enter` 与本家族链路上的
`0x14B78`/`0x18890`/`0x13A9F` 都会用到）：

- `0x12EAA/0x1300D/0x13185/0x13315` → `map_scroll_down/left/up/right`：把记录
  `dword_53A45[rec]`（步长 80）挪一格（状态字节 `rec[+3]`=0..3、动画帧 `rec[+4]`=1..6），
  同时平移视口原点（x=`dword_53AA9`/`dword_53AB1`，y=`dword_53AAD`/`dword_53AB5`），7 帧里
  每帧 `map_unit_ping(0x32230)`→`pal_anim_step(0x4E31C)`→`anim_frame_step(0x1297D)`→
  `map_render_view(0x11EEE)`（13×9；`0x1300D` 固定 14 列，`0x13315` 按分支 13/14 列）→
  `dlg_portraits_refresh(0x127A9)`→`gfx_copy_rows(0x11EB0)`（312×192 → 0xA0504）→`ev5_13460`。
  常量 `0x8088/0x8070/0x55C8`、`±4/±1824/0x720`、行偏移 456/320 均取自离线反汇编。
  四处“是否已到位”的判据与 `ev4_11B48/9B/FA/C59` 同构（`dword_53AC5/51A8B`、`dword_53AC1/51A87`）。
- `0x13488` → `map_scroll_path`：按方向码表逐项调用上四者（0=下/1=左/2=上/3=右）。
- `0x13A44` → `map_scroll_cell_tag`：读 `map_cell_info` 的格（`flags&0x60==0` 且 tag!=0）→
  查 `dword_53A55 + 2*(tag-1)` 的 `+0x33/+0x34`，命中 `which` 时写 `dword_51A8F`。
- **伪 callee / 共享尾声**：四者的尾跳（`jmp 0x13167/0x1314F/0x12FF0/0x13180`）都落在邻函数体内，
  是共享尾声（调 `map_scroll_cell_tag` 或纯 `ret`），非调用；真依赖＝本簇 6 个 + 上列已还原资产。
- **qword 伪视图**：Hex-Rays 把相邻的 `dword_53AA9/dword_53AAD` 与 `dword_53AB1/dword_53AB5` 并成
  `qword_53AA9/qword_53AB1`；机器其实按 32 位读写（低位=x，高位=y），C 侧按 `ev4.c` 既有约定恢复。
- **返回值**：`0x13A44` 在部分路径于 eax 留一字节，其三个调用点（`0x1300D/0x13A9F/0x18890`）均不读，
  C 侧声明 `void`。

### 14) 开场/继续游戏画面助手 → `src/game/title_screen.c/.h`（本轮，3 函数）

开场屏 `0x1F894`（载 FDOTHER.DAT、滚动 logo、读 FD2.SAV 询问“继续？”）的三个已闭包就绪 callee：

- `0x2DF01` → `pal_blend_to(first,last,step,r,g,b)`（u5，本轮最高）：把 DAC 段 `[first,last)` 每项按
  `target + (pal[i]-target)*step/40` 混合（机器用 `imul/idiv`，**有符号截断**；`step`=40 原样输出、0 全变目标色），
  每项先 `outp(0x3C8,i)` 再逐通道 `outp(0x3C9,·)`；`outp` 经原地址 `0x37AE5` 调用（同 `fade.c`）。被开场屏与片头 `0x2D80D` 共用。
- `0x1FF79` → `title_options_draw(image,selected,count)`（u4）：把 `image`（FDOTHER 项）的第 1..3 条选项带
  `res_blit6` 到屏幕固定偏移 `0xACD81/0xAD8C1/0xAE401`（各差 9 行=2880B），选中行用偶数子图（2/4/6）、
  否则奇数（1/3/5）；`count` 决定画 1..3 条（`cmp;jle` 有符号）。原函数 eax 留 `res_blit6` 的返回值，调用点不读，C 侧声明 `void`。
- `0x1F73F` → `title_frame_show(image_index,palette_index,strip,row)`（u1）：淡出 → `memset(0xA0000,0,0xFA00)`
  → `res_load(FDOTHER,·,palette_index)` 换调色板 + `res_load(FDOTHER,0,image_index)` → `rle_decode` 到屏
  → 淡入 → `svc_wait_ticks(1)`/`(6)` → 淡出 → `res_load(FDOTHER,·,101)` →
  `gfx_copy_rows(0xA0000,320,strip+320*row,320,320,200)` → 淡入：把 735 行的 5 段合成长图按 `row` 竖直开窗。
- **伪 callee**：三者皆仅误挂 `sub_3702F`(chkstk)；真依赖全部已还原（res/rle/gfx/fade/svc）。三者无对拍器，标“已还原未验证”。

### 15) 地图行动效果处理簇 → `src/game/map_action.c/.h`（本轮，6 函数）

地图行动路由 `sub_20C6F`（行动码 8..24 分派）与 `funcs_1541F` 事件/行动表共同调用的
“逐记录效果处理”簇。全部真依赖已还原（map/rec/ev4/unit_load/util）：

- `0x21082` → `map_action_add_field(a1,value,field,slot,count,list,table_idx)`：画队伍图标 →
  对 `list[0]` 记录的 `field` 处 16 位字 `+= value` → `map_enqueue_status(value,94,list[0])` →
  刷新并弹状态数字 → `unit_metrics(a1)` → `rec_slot_remove(a1,slot)`。被 `sub_20C6F` 以多个
  行动码（字段偏移 55/57/59/62/66/70、字形基 13/17/18/19）复用。
- `0x22721` → `map_action_grow_x`：对每个目标，若记录 +34 标志为空则置为 `rand%4+2`，
  把 +72 度量按 `(word * dbl_50210 + 1.0)` 增长（**Watcom `fild/fmul/fld1/faddp/__CHP/fistp`**，
  C 侧即 `(int)((double)word * dbl_50210 + 1.0)` 截断）并把该增量显示为数字，`dword_53EC8 += 2*r[33]`；
  否则只 `ev4_1E1DC` 排空闲光标精灵。
- `0x22997` → `map_action_widen`：同构，作用 +36 标志并把 +76/+78 度量各 `+= 15`，显示 15。
- `0x22C04` → `map_action_clear_flag`：`rec_sub_table5(a1,25)` 后对每个目标清除记录 +5 的 bit7
  （`rec_flag_or80` 的逆操作），`dword_53EC8 += 8*r[33]`；否则排空闲光标精灵。
- `0x226EA`/`0x22960` → `map_action_grow_x_cost`/`map_action_widen_cost`：`funcs_1541F` 第 17/19 项，
  先 `rec_sub_table5(index,18/19)` 扣费再调用上面对应的 0x22721/0x22997。

**陷阱/伪代码伪影**：
- **伪 callee（共享尾声）**：`0x22721` 的 callee 表误挂 `sub_1300D`（尾 `jmp 0x1317D` 落进其共享尾声），
  `0x22997` 误挂 `sub_22AF6`（尾 `jmp 0x22BBE`）——均按普通 return 处理，不传递依赖。
- **chkstk**：六者皆 `push size; call 0x3702F`，丢弃。
- **`_CHP()`**：`0x22721` 的 `call 0x37AF4` 是 Watcom 浮点取整辅助，配 `fistp` 即 C 的 `(int)` 截断。
- **符号比较**：`0x22C04` 的 `rec[5] >= 0` 在机器上是 `test byte,0x80; je`，C 侧写 `(rec[5] & 0x80) == 0`
  以避免 `uint8_t` 恒真；`v6 > 8 && v6 < 25` 用 `movzx` 无符号量，正数下与有符号同。
- **返回值**：`0x22C04`/`0x22721`/`0x22997` 尾声处的 eax 是 `map_view_update`/`map_draw_status_popups`
  的残留（均 void），调用点不读，C 侧返回 0。六者无对拍器，标“已还原未验证”。

### 16) 密码输入屏 → `src/game/password.c/.h`（本轮，3 函数）

地图视图处理器 `0x117E7` 解锁的密码屏整簇（`0x344B4`(u5) 为闭包就绪头部）：

- `0x33FAF` → `password_screen_run`：`guest_malloc(0xFA00)` 屏幕缓冲 → `res_load` FDOTHER 96/97/98
  → `rle_decode`(97) 到缓冲并释放 → `util_rand()%100` 取随机两位数密码 → 按该值查 `0x60181`
  的 3 字节记录（`util_rec3`）→ `ev5_34317` 解出 6 个字型索引 → `password_frame_draw` →
  `pal_fade_in` → 键循环（INT 16h，AH=0x10；E0/52/39→1C、53→1）：左/右（4B/4D）移动光标 0..5、
  回车（1C）追加字型（最多 3 个）、退格（01）删除，各带 `svc_play_sfx` → 释放、`pal_fade_out`
  → `memset(0xA0000,0,0xFA00)` → `map_view_update(1)` → `pal_fade_in`。
- `0x34366` → `password_frame_draw`：拷屏到新缓冲 → `sprintf(buf,"%02d",n)` 拆两位数 → 逐字型
  `password_glyph_blit`（两位数 col 150/162、row 75；6 个轮盘字型 col 25i+86、row 132；
  已选字型 col 32i+116、row 97；光标 `dword_53A4D` 的 0 号子图 col 25·sel+86、row 132）
  → 拷回 0xA0000。
- `0x344B4` → `password_glyph_blit`：单字型 `sprite24_plain`，源 `img+*(int*)(img+4*index+6)`、
  目标 `surface+stride*row+col`（机器 ABI 即 `(img,index,surface,stride,col,row)`）。

**陷阱 / 伪代码伪影**：
- **死代码（关键发现）**：`0x34215..0x3423A` 把 3 个已输入字型与密码比较，但不等分支 `0x34233`
  是**无条件跳转**（字节 `39 D8 EB E9` = `cmp eax,ebx; jmp 0x3421E`），故置失败标志的 `0x34235`
  不可达、`0x34243` 整段“密码错误”路径（`rle_decode`(98) + `j___delay(1000)` + 重试计数
  `0x34274` + `AIL_shutdown` + INT 10h 文字模式 + `printf("Sorry your password is wrong !!!")` +
  `exit(1)`）是**死代码**；Hex-Rays 亦整段省略。本 C 未翻译该死段（屏幕恒通过），仍按机器保留
  98 号资源的载入/释放。已核 `0x34235`/`0x34243` 无任何跳转/调用目标。
- **chkstk**：三者皆 `push size; call 0x3702F`，丢弃。
- **局部 REGS**：INT 16h 用局部 REGS 块（AX 在偏移 0），扫描码取高字节（偏移 1）；其余键读取器
  用全局 `word_53A8D`。
- **资源 98**：仅死段使用，本 C 仍载入并在收尾释放（保持分配序列一致）。
- 无对拍器，标“已还原未验证”。

### 17) 队伍肖像马赛克揭示过渡 → `src/game/mosaic_reveal.c/.h`（本轮，4 函数）

地图上“角色加入 / 队伍变更”时的整屏过渡簇（编排者 `0x1A30B` 未还原，仅取其闭包就绪的
三段：`0x1F42D` 头 + 两个 caller `0x1F1CC`/`0x1F30A` + 其私有块化 blit `0x4EB59`）：

- `0x4EB59` → `mosaic_blit(block,dst,src)`：把 `src+0x504` 的 312×192 窗口按 `block×block`
  栅格重采样写到 `dst+0x504`（`block==1` 即 1:1）；每格取格左上角源字节。行内每 `block` 列步进
  `block`，每 `block` 行把源指针整体前移 `320*block`。机器用 16 位 `mul` 把 `320*block` 只留低 16 位
  （本簇 1..17，不溢出）。仅被本簇两个 caller 使用（usage=2），是私有 helper。
- `0x1F42D` → `reveal_slide_draw(offset,index)`：在 456 行距地图窗（`dword_53A49+0x8088`）里
  保存左右两条肖像带背景 → 左带画子图 `index`（列 `85-offset`）、右带画子图 81（列 `165+offset`，
  皆行 82）→ 把 312×192 窗口拷到 VGA 暂存页 `0xA0504`（`gfx_copy_rows`）→ `svc_wait_ticks(1)`
  → `dlg_snap_restore` 还原两条带。即“两肖像沿水平方向分开”的一帧。
- `0x1F1CC` → `reveal_mosaic_out(index)`：`guest_malloc(0xFA00)` 暂存屏 → 拷 `0xA0000` 屏幕
  → 连续 `reveal_slide_draw(25*i,i=4..0)`、`(1)`、`(0)` 播放分带帧 → 存回 `dword_53A49`、清暂存
  → 循环 `j=0..15`：`mosaic_blit(j+1,暂存,屏)`（1→16 渐粗）→ 在暂存上画子图 `index`/81 并释放
  → `pal_fade_range(16,255,j)`（**渐暗**，`j` 即减色量）→ 拷回屏 → 等 1 tick → 释放、`kbd_flush`。
- `0x1F30A` → `reveal_mosaic_in(index)`：清暂存 → 循环 `i=16..0`：`mosaic_blit(i+1,暂存,屏)`
  （17→1 渐细）+ 画子图/释放 + `pal_fade_range(16,255,i)`（**渐亮**）+ 拷回屏 + 等 1 tick
  → `map_render_view(dword_53A49+0x8088,456,13,8,dword_53AA9,dword_53AAD)` → `dlg_portraits_refresh`
  → 再播 `reveal_slide_draw(25*j,j=0..4)`。

**陷阱 / 伪代码伪影**：
- **共享尾声**：`0x1F42D` 尾 `jmp 0x184BA` 落进 `fb_slide_step` 尾声（`add esp,0xC; pop esi; pop ebx; ret`），
  该 `add esp,0xC` 恰好清掉第二次 `dlg_snap_restore` 的 3 个实参（Watcom cdecl 调用者清栈），
  机器上是普通 `return`，非调用/非依赖。
- **chkstk**：四者皆 `push size; call 0x3702F`，丢弃。
- **寄存器复用**：`0x1F30A` 尾部把 `esi`（原暂存指针，已 free）改作循环计数，C 侧用独立 `j`。
- **伪 qword 视图**：`0x1F30A` 的 Hex-Rays 把相邻 `dword_53AA9/dword_53AAD` 并成 `qword_53AA9`，
  机器按 32 位读（低位=ox、`dword_53AAD`=oy）；C 侧按 `map_scroll`/`ev4` 既有约定取 `(int)dword_53AA9`。
- **块化读法**：Hex-Rays 未表出 `0x4EB59` 的“行指针复用+栅格重采样”结构，须以反汇编为准（行内只在
  栅格边界步进、行间 `push/pop esi` 复用同一行）。
- **语义归属**：`0x4EB59` 位于库区（funcmap zone=lib），但仅被本簇调用，故随簇归入本模块。
- 无对拍器，标“已还原未验证”。

### 18) 地点/场所菜单辅助簇 → `src/game/place_menu.c/.h`（本轮，5 函数）

`dword_53F4A` 选定当前场所；菜单环本身是 `menu_list_select`（0x27D33），场所/商店子屏由
`sub_2670E/279BC/2872B/28CBD/29300/29DAA/2A43E` 驱动（均待还原）。本簇是这些驱动共用的屏幕片段：

- `0x26A0D` → `place_name_copy(dst)`：按 `dword_53F4A` 从 `dword_53F56` 的表取场所名片段
  （1→偏移 3、长 12；3→偏移 15、长 8；其他→偏移 23、长 8），逐字节拷到 `dst`，遇 `0xFF` 停，
  返回字节数。
- `0x2921A` → `place_zoom_blit(src_x,src_y,src,zoom)`：把 320×200（步长 320）源图最近邻放大进
  实时帧 `dword_53A49`；`src_x/src_y` 为源点 ×128（定点），`zoom` 为倍率 ×128；先 `memset` 清屏、
  越界裁剪。地点菜单退场逐帧调用它（`i=10..0`，`zoom=128-9*i`；x 中心 `((i*(byte_52363[..]-150)/10)<<7)+20480`）。
- `0x28B41` → `place_transition()`：按 `dword_53F4A` 分支播放转场（1/5：`res_blit6` 带擦除循环；
  3：单带 + `svc_wait_ticks(8)`；4：9 带 + 两段 `pal_fade_add` 增亮/减暗各 `j___delay(4)` + 等待），
  结束 `dlg_blit_dato(dword_53A85,dword_53C67,0)`、`kbd_flush()`。
- `0x26A7A`/`0x26B91` → `place_money_gain/spend(amount)`：把金额 `dword_53BF3` 加/减 `amount`，用
  `%0.8d` 拆成 8 位数字逐位列滚动动画（`ev5_26C9B` 画 6×9 字形，列基址 `0xA7A90+6*n`），每帧 `j___delay(10)`。

**陷阱 / 伪代码伪影**：
- **实参定序**：`0x2921A` 机器帧里 4 个实参在 `[esp+0x1c..0x28]`，经离线反汇编（cdecl 末压者在低地址，
  调用点 `0x26882/0x268a2` 的压栈序）定为 `(src_x, src_y, src, zoom)`；Hex-Rays 的 `a1/a2` 顺序一致。
- **有符号除 128**：机器用 `sar/sbb` 惯用法实现“向零截断”的 /128；两处读取都先做
  `(unsigned)<范围` 守卫（源列 x∈[0,0xA000)、源行 y∈[0,0x6400)），非负下 `>>7` 与之等价，C 侧照写。
- **数字滚动细节**：`0x26A7A` 滚动标志从 1 递增（达 10 时进位显示位、**不重置**标志——下一轮 while
  才按 `cur==tgt` 重新初始化），字形索引 `flag+9*cur`；`0x26B91` 标志从 9 递减，字形索引 `9*cur+flag-1`，
  且显示位在每轮 while 开头 `--cur`（0→9 回绕）。两者逐句与 Hex-Rays 一致，已在离线反汇编 `26a7a..26c96` 核过。
- **共享尾声**：`0x26A7A`/`0x26B91` 共用尾声 `0x26B8A`（`add esp,0x3C; pop edi/esi/ebx; ret`），是普通 `return`。
- **chkstk**：五者皆 `push size; call 0x3702F`，丢弃。
- **返回值**：`0x2921A` 退出时 eax 残留 `zoom`、`0x28B41` 残留 `kbd_flush` 的左值，调用点均不读，C 侧声明 `void`。
- 无对拍器，标“已还原未验证”。

### 19) 地图行动 / 记录列表助手簇 → `src/game/map_records.c/.h` + `map_action.c`（本轮，3 函数）

地图行动路由 `sub_20C6F`（行动码 8..24）与其驱动 `sub_1CFF0/15055/1567E/1548E/18D8C` 共用的
“y 放大”handler 与记录收集助手；三者真依赖全部已还原：

- `0x22866` → `map_action_grow_y`（追加 `map_action.c`）：与 `map_action_grow_x`（0x22721）同构——
  先 `map_draw_party_icons_anim/_icons(a1,18,a2,a3)`，再逐目标：若记录 +35 空则置 `rand%4+2`，
  把 +74 度量按 `(word * dbl_50218 + 1.0)` **截断**增长（Watcom `fild/fmul/fld1/faddp/__CHP/fistp`），
  数字显示、`dword_53EC8 += 2*rec[33]`；否则 `ev4_1E1DC` 排空闲光标精灵。记录 +72/+74/+76/+78 在
  `unit_load.c` 里即 x/y/h/w，故 +72→x（0x22721）、+74→y（本函数）、+76/+78→w/h（0x22997）。
  是 `sub_20C6F` 行动码 15 的 handler。
- `0x1B6B7` → `map_collect_scripts(out)`（新模块 `map_records`）：扫 `dword_53A45` 的 `dword_53BEB`
  条记录，凡 `(rec[5]&1)==0 && rec[49]!=0xFF && word[rec+64]==0` 者，把 +49 处的 3 字节“脚本条目”
  `memmove` 到 `out + 3*count`，**返回 count**（机器把计数留在 eax）。`sub_20C6F` 尾部即用它填好
  局部数组后交给 `ev_script_run`（0x1AA1D）。
- `0x149F8` → `map_scan_records(ref_x,ref_y,out,start_x,start_y,steps,want_empty)`：
  从格 `(start_x,start_y)` 出发，每步按方向（同轴比 `ref`）±1 推进 `steps` 步，令全局 `qword_53AB1`
  置 `(x,y)` 后 `dlg_portrait_find()` 找该格记录，命中且 `(want_empty==0 && rec[6]!=0) ||
  (want_empty!=0 && rec[6]==0)` 则写入 `out`；越界（`0<=x<dword_53AC1 && 0<=y<dword_53AC5`，均
  **有符号**比较）跳过。收尾还原 `qword_53AB1`，返回 count。`sub_15055/1567E/1CFF0` 用它做“从目标格
  朝选中格”的行扫描。

**陷阱 / 伪代码伪影**：
- **伪 callee（共享尾声返 eax）**：三者的返回都靠跳进**邻函数**的共享尾声——
  `0x1B6B7` 尾 `jmp 0x1B64D`（在 `sub_1B5F1` 体内，`mov eax,esi; pop×3; ret`）、
  `0x149F8` 尾 `jmp 0x114FF`（在 `sub_1145A` 体内，`add esp,0x10; pop×3; ret`）、
  `0x22866` 尾 `jmp 0x2281B`（在 `sub_22721` 体内：`map_view_update(0)`+`map_draw_status_popups()`）。
  故 `function-map` 把 `sub_1B5F1/sub_1145A/sub_22721` 误列为 callee；真依赖仅本 3 函数 + 上列已还原资产。
- **返回值**：IDA 因尾声不在本体内把 `0x1B6B7/0x149F8` 标成 `void`，实际 eax 回计数——
  调用点 `0x1562f/0x18fd6/0x1d48d/0x21058` 均 `mov ebx,eax` 后作 `ev_script_run` 的 steps 参数，
  故 C 侧声明为 `int`。
- **qword 视图**：`0x149F8` 的 `qword_53AB1` 是 x/y 两个 32 位单元（低位 x=0x53AB1、高位
  y=0x53AB5）；C 侧用 `qword_53AB1` 整体存取（与 `dlg.c`/`scene.c` 约定一致），步进用 32 位局部
  变量以免 64 位进位污染另一半。
- **word[rec+64] 判零**：机器 `movzx; test; jg skip`（有符号 >0 跳过）；与同簇 `ev5_1B653`、
  `map_units.c` 的既有写法一致，按 `== 0` 还原（该字段为非负计数）。
- **chkstk**：三者皆 `push size; call 0x3702F`，丢弃。
- 无对拍器，标“已还原未验证”。

### 20) 地图方向光标补片动画簇 → `src/game/map_cursor.c/.h`（本轮，4 函数）

`dword_53C57` 选方向的地图交互族（驱动器 `0x16F55/0x1728C/0x18D8C/0x19DF7/0x1BBDC`，均待还原）共用的
“tile 补片 + 窗口暂存回填”动画：每帧先把暂存的 72×72 字节块贴回地图视图，再在 4 个偏移处
`gfx_blit_transparent` 盖 4 张 tile，然后重画记录肖像并把 312×192 窗口发布到 VGA 暂存页 `0xA0504`。
全部真依赖已还原（`dlg_portrait_find/draw`、`svc_play_sfx`、`anim_frame_step`、`map_render_view`、
`dlg_portraits_refresh`、`map_draw_cursor`、`gfx_blit_transparent`、`gfx_copy_rows`、`kbd_flush`）：

| 原址 | 新名 | 行为 |
|---|---|---|
| `0x175A9` | `map_cursor_window_save` | 释放旧 `dword_53A71` → `guest_malloc(0x1440)` → 72 行×72 字节从视图块拷入 |
| `0x17643` | `map_cursor_window_restore` | 反向：把 72×72 块贴回视图（每行步长 456） |
| `0x1741C` | `map_cursor_patch_expand` | 开场 4 帧：偏移表 `{0x390×4}` 每帧 `−0x8E8/−6/+6/+0x8E8`（发散）；先 find→sfx→anim→render→refresh→cursor→save |
| `0x176B4` | `map_cursor_patch_collapse` | 收场 4 帧：偏移表 `{−0x23A0,0x378,0x3A8,0x2AC0}` 每帧 `+0x8E8/+6/−6/−0x8E8`（收敛）+ 末尾再回填一次 + `kbd_flush`；sfx 在 find 之前 |

**陷阱 / 伪代码伪影**：
- **探针不是 callee，且不分配帧（新的关键发现）**：`0x3702F` = `xchg [esp+4],eax; call 0x37042; mov eax,[esp+4];
  ret 4`——`0x37042` 各条路径全是 `ret`，**不改 esp**。故 `push size; call 0x3702F` 只做守卫页探测，
  函数帧来自显式的 `push ebx/esi/edi/ebp` + `sub esp,0x18`。这解释了 Hex-Rays 把 `0x1741C` 的
  `[esp+0x2C]/[esp+0x30]` 误当“未初始化局部”：按上述净效应，`[esp+0x30]` = `[esp0+8]` = **第 2 实参**。
- **ABI 由调用点定案**：两台机器码的前言一致，`mov ebp,[esp+0x30]` 即第二实参，故签名 =
  `(const int *a_tiles, const int *b_tiles)`（各 4 个 dword 的索引数组；调用点
  `0x16F91..0x16F97`、`0x1BC72..0x1BC77` 用 `lea/push` 压入局部数组佐证）。Hex-Rays 在 `0x1741C`
  **丢掉这两个实参的加载**，把 4 元素偏移表误标成 `retaddr/a1/a2/v7`，必须以反汇编为准。
- **伪 callee（共享尾声）**：两个画家尾部 `jmp 0x13FCC` 落在 `sub_13E9C` 体内（`add esp,0x18; pop×4; ret`）
  → 普通 `return`；`function-map` 因此误列 `sub_13E9C`。真依赖只有本簇 4 个 + 上列已还原资产。
- **块基点 vs 补片基点**：暂存块基点 = `+0x8088 + 24*(dword_53AB9−1) + 10944*(dword_53ABD−1)`（视图原点
  左上隔一格），补片基点 = `+0x8088 + 24*dword_53AB9 + 10944*dword_53ABD`（恰好差一格 = 10968 字节）。
- **索引表**：`0x51E9F` = `{7,5,6,4}`（镜像常量），`0x53EF2` 镜像里全 0（运行时填）；tile 偏移 =
  `*(dword_53A89 + 4*(3*a[j]+2*b[j]))`，tile 指针 = `dword_53A89 + 该偏移`，每帧 4 张（机器每轮
  重读 `dword_53A89`，C 侧同）。
- **堆**：`dword_53A71` 是 `fd2_globals.h` 里的 `uint32_t` 标量 → 用 `guest_malloc/guest_free`（同 `0x3706E/0x3776E`）。
- **返回值**：两者尾声 eax 残留最后一次 `memmove` 的结果，两个调用点均不读，C 侧声明 `void`。
- 无对拍器，标“已还原未验证”。

### 21) 队员图形动画视图助手簇 → `src/game/figure_view.c/.h`（本轮，3 函数）

仍待还原的驱动器 `sub_2CF30`（0x2CF30，2269B）播放队员的 FIGANI.DAT 动画并铺 BG.DAT 卷动背景 +
TAI.DAT 交替肖像；本轮取其三个闭包就绪的 callee（第四个 callee `0x2E95B` 只在 CRT `fread` 包装
`sub_373CA` 上重读 FD2.TMP，属库区，本批不收）：

- `0x30E25` → `figure_bg_pick(count,units)`（u2）：从 `byte_52470[dword_53C03]` 起，倒序扫 `units[]`
  的 80 字节记录，经 `map_cell_info` 取格信息，凡 `rec_skip(unit)==0 || sel==0` 就用其字节 6 作 BG 子图
  索引；返回该索引（`sub_2FF01` 用它选 `res_load(BG.DAT,...)`）。
- `0x2F4D4` → `figure_bg_scroll(unit,figure,screen,tmp,bg)`（u3）：两趟 i=9..0 把三张 BG 条带
  `dword_5413F[(i)%3]` / `[(i+2)%3]` 经 `rle_decode` 铺进 640 步长工作缓冲（第二趟落点 +320），每步
  `gfx_copy_rows` 从 `work+32*i` 发 320×200 窗到屏；中间清两缓冲→解当前 bg 到 320 步长屏→
  `ui_status_panel_at(screen,unit)`→`res_blit(figure,0,work,640,-1)`。
- `0x30E9D` → `figure_play(unit,mode,figure,base,tmp,screen,bg,tai)`（u3，840B）：按 `mode` 选
  `dword_53A6D` 调色条带下标（8/32/33→19、>3→15、否则 11）；逐 `figure` 子精灵
  （数=figure[2]，偏移=figure[4i+8]）→音效/姿态切换（`rec_sub_table5`+状态面板+非菜单模式画
  bg/tai 并置 `flash=6`）→逐帧把 320 步长缓冲合成进 640 缓冲、按记录 +6 决定 `figure[i]`/`base[frame]`
  先后、发框到 VGA、`outp` 调色闪 6 帧、推进 base 帧计/`svc_wait_ticks(1)`。

**伪代码/语义要点**：
- **算符顺序即 ABI**：本批三次调用都用离线反汇编的压栈序定了实参顺序（尤其 `0x30E9D` 的两次
  `rle_decode_lut` 首参是 `a7`(a8)；`0x2F4D4` 的 `sub_2FACD(a3,a1)`＝`ui_status_panel_at(screen,unit)`）。
- **`_EAX` 类型**：`dword_53A6D` 在 `fd2_globals.h` 是 `uint8_t*`，调色条带按 32 位读
  `dword_53A6D + *(uint32_t*)(dword_53A6D+4*stride+6)`（同 `map.c`/`reveal.c` 约定）。
- **chkstk**：三者皆 `push size; call 0x3702F`，丢弃。
- **返回**：`0x2F4D4`/`0x30E9D` 的 eax 残留被调用点忽略，C 侧声明 `void`；`0x30E25` 返回索引，声明 `int`。
- 无对拍器，标“已还原未验证”。

### 22) 地图视图缩放 / 仪表帧缓冲助手簇（本轮，4 函数）

本轮 closure-ready 头名的四个 u3 游戏函数（新模块 `map_zoom.c/.h` + 三个已还原模块追加）：

| 原址 | 新名 | 模块 | 要点 |
|---|---|---|---|
| `0x1E739` | `ui_meter_fixed(dst,pitch,count)` | ui_meter.c | 固定宽仪表：正 count 画左帽 23 → 身 24 → 右帽 25 → 空格 29（到位置 69）→ 尾帽 30（位置 70）；count<=0 画全空 29..29 后尾帽 30（末位 69）。两个分支总长 71/70 的不一致原样保留。 |
| `0x1F558` | `map_zoom_draw(x,y,zoom,cells)` | **map_zoom.c（新）** | 地图缩放过渡帧：定点中心 (x,y)、`zoom`/128 倍、13×8 格窗左上 = 中心 − (156·zoom, 96·zoom)/3072；清 `dword_53A49` 后在 +0x504 逐行按 `cells[row*64+col]` 最近邻放大 312×192（每 24 输出列进一格、每输出列源字节步进 `zoom`/128）。 |
| `0x2282F` | `map_action_grow_y_cost` | map_action.c | `funcs_1541F[18]`：`dword_53EC4=0` → `rec_sub_table5(index,18)` → `map_action_grow_y(index,count,list)`。 |
| `0x265EC` | `scene_map_view_draw` | scene_core.c | `scene_apply_state` 交互分支每帧重绘地图视图（0x26434/0x2647E）：`memmove(dword_53A5D→dword_53A49,0x25680)` → `rle2_blit(dword_53A49+0x1A20C, dword_53F5A, 456)` → `vm_run(dword_53A7D, dword_53F4A+495, dword_53A49+0x1ACC4,456,…)` → 记录格光标精灵 `dword_53A61[frame]`（frame 3→1）→ `gfx_copy_rows(0xA0504,320,dword_53A49+0x8088,456,312,192)`。 |

- **入口定位**：`dword_53F56 = tbl_ptr(&fd2_data2[0x238D],31,dword_53C03,-31)`（同 scene_core 既有写法），其首字节 `v2` 与 `dword_53F4A` 共定 `byte_52363/52375[6*v2+dword_53F4A]` 的格坐标。

**陷阱 / 伪代码伪影**：
- **伪 callee（共享尾声）**：`0x1F558` 尾 `jmp 0x18888` 落进 `dlg_draw_number`(0x187D6) 体内尾声（`add esp,0x1C; pop×4; ret`）→ 普通 `return`；`function-map` 误列 `sub_187D6`。
- **`__CFSHL__`**：`0x1F558` 的 `/128`（`sar/shl/sbb/sar` 惯用法）与 `/3072`、`%3072`（`idiv`）在 Hex-Rays 里被渲染成 `__CFSHL__`/`__CFSHR__`；两输入先归一到 `[0,3072)` 非负，C 侧用普通 `/`、`%`（int）。
- **指针表 ABI**：`0x1F558` 的 `a4` 是调用方建的 64×64 个 32 位“源基址”表（`cells[row*64+col]`，指向 `dword_53A5D` 工作缓冲，行距固定 64），C 侧按 `const uint32_t *` 读并以 `(uintptr_t)` 还原为指针；建表方 `0x2000A`/`0x21548` 仍待还原（尾随 `memmove(0xA0000,dword_53A49,0xFA00)` 发布）。
- **entry-18 两次**：`0x226EA`(entry17=x) 与 `0x2282F`(entry18=y) 机器码都 `push 0x12`（同扣 0x619FD 第 18 项），`0x22960`(entry19) `push 0x13`；原样保留（`map_action.h` 注释已更正）。
- **返回值/别名约束**：`0x265EC` 尾声 eax 残留 `gfx_copy_rows`，三个调用点均不读，但 `fd2_funcs.h` 原型为 `int`，故 `scene_core.h` 以 `#define sub_265EC scene_map_view_draw` 保持 `int` 返回 0（沿用该头既有别名模式）。
- **chkstk**：四者皆 `push size; call 0x3702F`，丢弃。
- 无对拍器，标“已还原未验证”。

### 23) 地图状态叠加帧（本轮，2 函数）→ `map_status.c/.h`（新）

本轮 closure-ready 头名是 `0x1E7F6`(u4)——§22 拉出 `0x1E739` 后它才就绪；其唯一 caller `0x1E611`(u1)
的其余 callee 均已在册，故作为同簇整体拉出。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x1E611` | `map_status_frame(pos,actor,target)` | 地图视图状态帧：`map_render_view(dword_53A49+0x8088,456,13,8,dword_53AA9,dword_53AAD)` → `dlg_portraits_refresh()` → 目标记录精灵 `res_draw_subimage(dword_53AD1,view,456,pos0-4,pos1-4,48)` → 上方 55 段 `ui_meter_fixed(view+456*(pos1+2)+pos0+3,456,55)` → 目标 HP 表 `map_status_gauge(dword_53A49+0x7964,456,target,pos)`；`pos[2]!=-1` 时再画 actor 精灵并 `map_status_gauge(...,actor,pos+2)`；末 `gfx_copy_rows(0xA0504,320,view,456,312,192)` 发布。 |
| `0x1E7F6` | `map_status_gauge(dst,pitch,rec,pos)` | 记录 HP 表：读 `record+0x40` 当前 / `+0x42` 最大（均 u16），当前为 0 直接返回；否则 `ui_meter_fixed(dst+pos0+pitch*(pos1+6)+7, pitch, 69*cur/max+1)`。 |

- **入口定位**：`pos` 为 4 个 int 的屏幕像素块 `{x0,y0,x1,y1}`，`pos[0..1]` 属 `target`（call 站点第 3 实参 `dword_53C4B`），`pos[2..3]` 属 `actor`（第 2 实参）；由 `0x1EB05` 填（`&dword_53A30`）。

**陷阱 / 伪代码伪影**：
- **伪 callee（共享尾声）**：`0x1E611` 尾 `jmp 0x11D2C` 落进 `map_view_update`(0x11CAC，已验证) 的共享尾声，实为 `gfx_copy_rows(0xA0504,320,view,456,312,192)` + `return`；`function-map` 误列 `sub_11CAC`（及其全部 callee）。
- **`jle` vs `!=0`**：`0x1E7F6` 的 `movzx` u16 后 `test/jle`，值域 `[0,65535]`，故“当前==0 才返回”与 `!= 0` 等价。
- **返回值**：`0x1E611` 的 `fd2_funcs.h` 原型为 `int`，但仅 eax 残留 `gfx_copy_rows`、无调用点读取，C 侧声明 `void`；`0x1E7F6` 同为 `void`。
- **chkstk**：二者皆 `push size; call 0x3702F`，丢弃。
- 无对拍器，标“已还原未验证”。

### 24) 物品使用盒选择助手（本轮，2 函数）→ `src/game/box_popup.c`（追加）

物品使用盒 `sub_1BFFE`（＝ `0x17E0B`/`0x1B9DE`/`0x17EEF`/`0x184C0` 的编排环）与商店菜单 `sub_2872B`
共用的两条“物品可否被该队员使用 / 标记所选项”助手，均 closure-ready：

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x1C1C3` | `box_item_usable(rec,item)` | 取队员记录 +32 字节作索引，查物品类别表（`tbl_ptr(&fd2_data2[0x188A],7,byte+32,0)`，7 字节行）；若物品类型字节（物品表 0x602AD，同 `item_rec`）等于该行前 6 字节之一则返回 1，否则 0。 |
| `0x1C142` | `box_item_select(rec,slot)` | 先 `rec_field_byte(rec,slot)` 取所选槽的物品牌；对 8 个槽（状态字节 +2*i+10）凡 0x40 位置位且物品牌与所选“同半区”（`>=0x80` 与 `<0x80` 同类）者清 0x40；再把所选槽置 0x40。返回 slot。 |

**陷阱 / 伪代码伪影**：
- **判据逻辑**：`0x1C142` 的清位条件在机器上先 `cmp ebx,0x80; jge`（有符号——但 ebx∈[0,255]，等价无符号），再 `movzx; cmp 0x80` 比物品牌；等价于 `(v3<0x80 && byte<0x80) || (v3>=0x80 && byte>=0x80)`，已逐指令核对 `1c142..1c1c2`。
- **chkstk**：二者皆 `push size; call 0x3702F`，丢弃（`0x3702F` 不分配帧）；`pop esi/ebx` 为显式。
- **返回值**：`0x1C142` 尾声 eax＝第二实参（slot），Hex-Rays 亦返 `a2`；`0x1C1C3` 返回 0/1。调用点 `1BFFE` 读取，C 侧均声明 `int`。
- **归属**：追加进既有 `box_popup.c`（物品盒 `1BFFE` 的私有选择助手；`sub_2872B` 商店亦复用）。无对拍器，标“已还原未验证”。

### 25) 人物图形视图的另两个同胞助手（本轮，2 函数）→ `src/game/figure_view.c/.h`（追加）

`sub_2CF30` 家族（§21 的 `figure_view`）的另两个“人物动画演示”渲染器，分别被驱动器
`sub_2EBE1`(0x2EBE1，2291B) 与 `sub_2FB2C`(0x2FB2C，744B) 调用；两者皆 closure-ready，
真依赖全部已还原（`rle_decode`/`res_blit`/`gfx_copy_rows`/`ui_status_panel_at`/`svc_wait_ticks`）：

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x2F631` | `figure_bg_scroll_portrait(unit,figure,portrait,screen,work,bg)` | `figure_bg_scroll`(0x2F4D4) 的变体：先 `i=1..9` 轮转 `dword_5413F[i%3]` 解到 `work+320` 并逐帧发布；再 `memset` 两缓冲 → 在 `screen` 上解 `bg`(0,50) 与 `portrait`(164,157) → `ui_status_panel_at(screen,unit)` → `gfx_copy_rows(work+320,640,screen,320,320,200)` → `res_blit(figure,0,work+320,640,-1)` → 第二趟 `i=1..10` 轮转 `dword_5413F[(i+1)%3]` 解到 `work` 并逐帧发布。 |
| `0x2FE14` | `figure_frame_run(figure,work,frames)` | 逐 `frames` 帧播放人物组 `figure`：每帧 `memset(work,0,0x1F400)` → 轮转 `dword_5413F[strip]`(strip=(strip+1)%3) 解到 `work`(640) → `res_blit(figure,sub,work,640,-1)` → `gfx_copy_rows(0xA0000,320,work,640,320,200)`；`figure[0]`=子精灵数、子精灵 `+6`=该子精灵帧数（`counter` 到则 `sub++`，回绕清零）→ `svc_wait_ticks(1)`。 |

**陷阱 / 伪代码伪影**：
- **ABI 由离线反汇编定案**：`0x2F631` 的 `mov ebp,[esp+0x24]`（chkstk 不分配帧 + 4 push 后）= 第 5 实参 = `work`；调用点 `0x2EBE1` 的压栈序列
  `sub_2F631(unit, figure, portrait, screen, work, bg)` 与之吻合，且 `0x2F4D4` 同处调用为 `(unit,figure,screen,tmp,bg)`，故 `a3`=`portrait`（解在 (164,157)，同 `figure_play` 的 `tai`）。
- **`0x2FE14` 的 `sub esp,8` 帧**：三个实参在 `[esp+0x1c/0x20/0x24]`（4 push + `sub esp,8` 后）；`res_blit` 的 5 个实参经 `push [esp+0x28]` 的求值时机核为 `(figure,sub,work,640,-1)`。
- **`%3` 与 `counter 回绕`**：两处 `idiv 3`/`xor` 序列为无符号取余与“相等即清零”（`v4^=v5`/`v3^=v6`），因被除数非负，C 侧直接 `(x+1)%3`、`++counter==count` 且相等时置 0。
- **chkstk**：二者皆 `push size; call 0x3702F`，丢弃（`0x3702F` 不分配帧）。
- **返回值**：`fd2_funcs.h` 对 `0x2F631`/`0x2FE14` 的原型均为 `void`，Hex-Rays 亦为 `void`；调用点不读，C 侧声明 `void`。
- 无对拍器，标“已还原未验证”。

### 26) 队伍变更过渡的共享音效库加载/释放对 → `src/game/mosaic_reveal.c/.h`（本轮，2 函数）

§17 留下的编排者 `0x1A30B`（队伍变更/肖像马赛克过渡）本轮只剩两个闭包就绪 callee——
它在每段处理环（`0x1D80B`/`0x1D8BA`）前后成对调用它们：

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x1A7BD` | `reveal_bank_load` | `byte_53AF9 != 0` 时把 `dword_53B0F` 清 0 再 `res_load(aFdotherDat, NULL, 64)`；该全局是共享音效库（`0x1E98C` 把它当 `svc_play_sfx` 的 `bank` 实参用） |
| `0x1A7F1` | `reveal_bank_free` | 同一 flag 门下释放 `dword_53B0F`（机器 `push ds:0x53B0F; call 0x3776E`）→ 走 `guest_free` 缝 |

**陷阱 / 伪代码伪影**：
- **`old_buffer` 是 NULL**：机器明确 `push 0` 作 `res_load` 第 2 实参（不是把 `dword_53B0F` 旧值传进去），
  故每次都新分配；前一条 `mov ds:0x53B0F,0` 只是“载入期间清零”，C 侧照写以保序。
- **chkstk**：两者皆 `push 0x10`/`push 8; call 0x3702F`，丢弃（`0x3702F` 不分配帧）。
- **返回值**：两者 eax 残留（`res_load` 指针 / `free` 结果），调用点不读，C 侧声明 `void`。
- **归属**：`0x1A30B` 的过渡即 `reveal_mosaic_out/in`（`push 0x52`/`0x50`），故两助手追加进
  既有 `mosaic_reveal.c`（该模块仍无对拍器，保持“已还原未验证”）。

### 27) 队伍名册成员助手 → `src/game/party_roster.c/.h`（本轮，2 函数）

`0x2AF28`（队伍变更/编成屏，仍待还原）的两个闭包就绪 callee，即本轮 `function-map.csv`
闭包就绪头名（均 u2、同 caller）：

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x2B439` | `party_member_present(count,id)` | 扫记录表 1（`dword_53A45`）槽 1..count 的 id 字节（+8），命中返回 1；未命中则播招募肖像序列（`msg_open_portrait(75)` → `vm_run(dword_53A7D,657,0xA951F,320,205,76,74,19,1)` → `dlg_blit_dato(dword_53A85,dword_53C67,0)` → `dword_53A51` 置 1 哨兵 → `dlg_wait_key(0)` → 清 0 → `msg_close_portrait`），返回 0。 |
| `0x2B843` | `party_member_move_front(id)` | 扫表 1 找 id 字节命中的槽（全表扫、取最后一个匹配）；把表 2（`dword_53BF7`）的该记录搬到槽 1 并顺移紧凑其余槽（`guest_malloc(0xA00)` 暂存整表 → 三条 `memmove`）→ 释放暂存 + 释放 DATO 精灵库 `buf`（`dword_53A61`）→ `fopen(FDICON.B24,"rb")` 逐队员 `unit_entry_data(+7,fp)` 重建图集 → `fclose`（返回 fclose 结果，调用点不读）。 |

**陷阱 / 伪代码伪影**：
- **Hex-Rays 索引折算**：`0x2B439` 的 `dword_53A45 + 80*i + 88` 实为槽 `i+1` 的 `+8` 字节
  （机器 `lea ebx,[edx+1]; ... shl 4` 得 `80*(i+1)`），C 侧直接写槽 1..count。
- **无符号字节比较**：两者对记录 +8 与实参 id 均用 `movzx`（零扩展 `cmp`），故 C 侧 `(uint8_t)id`；id>255 永不命中。
- **`dword_53AD9`**：机器 `movzx; inc` ⇒ `(uint32_t)(uint8_t)id + 1`（32 位写入）。
- **`dword_53A51` 哨兵**：`0x2B439` 等待键期间把它置 1、随后清 0（`dlg_wait_key` 读它选文本基址），
  与 `scene_core.c` 同写法——此路径是“哨兵”用法而非地图格表用法。
- **`buf`（0x53A61）释放**：机器 `push ds:0x53A61; call 0x3776E`，走 `guest_free` 缝
  （该单元由 `res_load` 的 `guest_malloc` 分配）；`dword_53BF7` 由 boot.c 初始化、此处不释放。
- **fopen/fclose**：机器直接调 CRT `0x37324/0x3759C`；本 C 用 `fopen(aFdiconB24_6,aRb_10)`
  （即 `fd2_data1` 的 `"FDICON.B24"`/`"rb"`）与 libc `fclose`（同 `scene_core.c` 的 FDICON 重载段）。
- **chkstk**：二者皆 `push size; call 0x3702F`，丢弃。
- 无对拍器，标“已还原未验证”。

### 28) 地图视口记录/单位精灵叠加 → `src/game/map_sprites.c/.h`（本轮，2 函数）

`map.c` 的 0x1C2DA/0x1C4CC（队伍图标绘制族）的**同胞**，闭包就绪头名（均 u2、同族）：

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x1CB94` | `map_draw_record_layer(base,count,list,frame)` | `map_render_view(base+0x8088,456,13,8,ox,oy)` → 扫全表记录（`dword_53BEB`、步长 80），跳过 +5 bit0 置位者与视口外者；视口内记录：索引在 `list`（`count` 项）中者 → `rle2_blit_trans(dst, overlay, 456)`（overlay = 0x53AD1 动画库第 `frame` 帧；驱动 0x1CAC7 传 74/75 交替），否则按 kind 字节与 `dword_53C0B` 从 0x53A61 库取 `sprite24_plain`。单次合成，不循环。 |
| `0x1CD17` | `map_blink_units(unused,colour,count,list)` | `guest_malloc(0x25680)` 存底图 → 10 帧：回存底图 → 逐个 `list` 记录按视口内坐标 `sprite24_ramp(spr,dst,456,byte_52006[colour],7-i%8)` 闪烁 → `gfx_copy_rows(0xA0504,320,view,456,312,192)`+`svc_wait_ticks(1)` → 末尾回存底图 + `guest_free`。 |

**陷阱 / 伪代码伪影**：
- **共享尾声**：`0x1CB94` 尾 `jmp 0x1317D` 是 `0x1300D` **体内**的共享尾声（`add esp,0xc; pop*4; ret`）⇒ 纯返回，IDA 的
  `sub_1300D` 是**伪 callee**（同 map_action/map_cursor 已记录的同一尾声）。`0x1CD17` 尾 `jmp 0x1C4BC` 先压 `v14` 再进
  共享尾声（该处 `call 0x3776E`=guest_free）⇒ 语义等价于 `guest_free(scratch); return;`。
- **视口原点**：伪代码把 `0x53AA9`/`0x53AAD` 合成 `qword_53AA9`；机器分两次读 **32 位 dword**（`mov eax,ds:0x53aa9` /
  `ds:0x53aad`）并用**有符号**边界比较。C 侧按 `(int)dword_53AA9` + `dword_53AAD` 与 `int` 比较还原。
- **未初始化寄存器局部**：`0x1CB94` 伪代码的 `v16`/`v17`（ebp 系）实为形参 base 与第 4 参 frame（反汇编读 `[esp+0x20]`/`[esp+0x2c]`），C 侧直接用形参。
- **常数**：`dst = view + 24*(x-ox) + 10944*(y-oy) - 0xAB0`（末项 2736 = 24×114）；精灵库索引 `12*kind + (dword_53C0B==3 ? 2 : dword_53C0B)`。
- **表只读**：`0x1CD17` 的色基机器先拷 `0x52006` 起 30 字节到栈局部 `v13` 再索引（只读、无写），C 侧等价写 `byte_52006[colour]`。
- **chkstk**：两者皆 `push size; call 0x3702F`，丢弃。
- **归属**：新建模块。`map.c` 系**已验证**模块，按 playbook 不改动，仅在伞头加 `map_sprites.h`（字母序插在
  `map_scroll.h` 与 `map_status.h` 之间）。
- 无对拍器，标“已还原未验证”。

### 29) 物品子菜单循环（本轮，1 函数）→ `src/game/box_popup.c`（追加，物品盒家族闭合）

§24 收尾的 `box_item_usable`/`box_item_select` 之后，物品盒（DATO）编排环 `0x1BFFE` 成为本轮
闭包就绪的游戏函数头名（u2，324B）；它的全部 callee 均已在册（`0x17E0B`/`0x1B9DE`/`0x17EEF`/
`0x184C0`/`0x1C142`/`0x1C1C3`/`0x18409` 及 `rec_field_byte`(0x1B722)/`rec_slot_free`(0x1B8A6)/
`unit_metrics`(0x1B750)），故按“同模块簇”直接拉出，物品盒模块现闭合：

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x1BFFE` | `box_run(rec)` | `box_open_anim(rec)` + `dword_53C57=0` → 内层 `do{ v1=box_input(rec,0); }while(v1==0)`；`v2=rec_field_byte(rec,dword_53C57)`；若 `v1==-1 || rec_slot_free(rec)==0` → 12 步 `fb_slide_step` 滑出 + `memmove(0xA0000,dword_53C5F,0xFA00)` + 释放三块 0xFA00 暂存 → 返回；否则若 `box_item_usable(rec,v2)!=0` → `box_item_select` + `unit_metrics` + 把屏存进 `dword_53A49` → `box_open_init`/`box_items_draw` 重铺箱面 → 回贴屏，继续环。 |

**陷阱 / 伪代码伪影**：
- **共享尾声（伪 callee）**：尾 `jmp 0x10C49` 落在 `0x10B4E` 体内它自己的尾声
  （`add esp,0x4; pop edi; pop esi; pop ebx; ret`）。该 `add esp,0x4` 恰好清掉**最后一次**
  `guest_free(0x53C63)` 压入的实参——前两次 free 各自带 `add esp,0x4`，故本函数净栈平衡，
  跳转即普通 `return`，**不是**对 `sub_10B4E/sub_10C50` 的调用；`function-map` 亦未误列。
- **返回值**：机器 eax 残留最后一次 `guest_free` 的结果；两个调用点（`0x1BFC6` 压 `ebp`；
  `0x28F30` 压 `dword_53C57`）均不读，C 侧声明 `void`（签名 `(int rec)`）。
- **chkstk**：`push 0x20; call 0x3702F`（`0x3702F` 不分配帧），丢弃。
- **归属**：追加进既有 `box_popup.c`（`fd2_game.h` 伞头已含该模块头，无需改动）。两个 caller
  `0x1BBDC`（物品盒驱动，`sub_53C57==2` 分支）与 `0x28EFE`（商店菜单）仍待还原。无对拍器，
  标“已还原未验证”。

### 30) 存档场景加载 + 章节开场 → `src/game/scenario_load.c/.h`（本轮，1 函数）

`scenario_menu`（0x25EBB，已还原）第三分支与地图驱动器 `0x19DF7`（待还原）共用的
“读 FD2.SAV → 载入章节资源 → 播放开场”整链；也是 module-map §2 的“存档唯一入口”。
本轮 `function-map.csv` 闭包就绪游戏函数头名（u2，1552B；真闭包＝自身——callee 全在册，
含被伪列出的 `sub_22AF6`）。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x10010` | `scenario_load()` | 读 FD2.SAV(0x59CB) → `util_deobfuscate` → `util_sum_tail4` 与尾 dword（+0x59C7）比对，不符则播“读档失败”卡（`msg_open_portrait(75)`+`vm_run(436,…)`+`dlg_blit_dato`+`dlg_wait_key`+`ev4_196CB`）。随后：`memmove` 队伍表（源 +0x8A3，0xA00）→ `res_load` FDOTHER[0]/FDFIELD[3·s+2] → 释放并重分配 'p'(0x53A55, 0x8A3) 拷入 → `world_load_tiles` → FDTXT[s+1]/FDFIELD[3s]/FDSHAP[2·p[0]][] → `util_fix_records` → 记录表 dword_53A45 重分配 0x1E00、拷入 80·records 与 0x20 额外块 → 释放 'buf'(0x53A61) 并以 FDICON.B24 经 `unit_entry_data` 重建图集（`dword_53BDF=0`）→ `fwrite` FD2.TMP(0x32A00) → 从 +0x30C3 的 18 字节场景字段写一批全局（dword_53BEF/53C03/53AB9/53ABD/53BFB/53BF3、`qword_53AA9`/`qword_53AB1`、byte_53AF9/51AAB/51E61/51E62）→ 释放两缓冲 → `bgm_play`/`map_view_update(1)`/`pal_fade_in` → 开场两段（9 步带淡入，x=84，j>6 画数字；4 步地图窗淡入，k=2/3/4/9）→ `map_view_update(0)`/`j___delay(200)`/`kbd_flush`。 |

**陷阱 / 伪代码伪影**：
- **共享尾声（伪 callee）**：尾 `jmp 0x22BBE` 落在 `sub_22AF6`（`item_apply`）体内的共享尾声
  （`add esp,0x4; pop ebp/edi/esi/ebx; ret`；与 `0x10010` 前言 `push ebx/esi/edi/ebp; sub esp,4`
  完全同帧）⇒ 普通 `return`；`function-map` 遂误列 `sub_22AF6`。
- **qword 伪视图**：Hex-Rays 把相邻 dword `0x53AA9/0x53AAD`、`0x53AB1/0x53AB5` 并成
  `qword_53AA9`/`qword_53AB1`；机器分两次写 32 位零扩展字节，C 侧按两字节打包进 64 位单元
  （`(uint64_t)field[3] | ((uint64_t)field[4]<<32)`）。
- **宏视图**：`dword_53A5D` 展开为 `uint8_t *`（非 `**`），`res_load` 返回值直接赋入。
- **OOM 三路径同形**：`word_53A8D=3; int386(0x10,&word_53A8D,&word_53A8D); printf(msg); exit(1);`
  折叠为文件内静态 `oom_exit`（三段消息 aOutOfMemory/_0/_1 见 dsi 0x04/0x23/0x37）。
- **字符串**：FD2.SAV 的 name/mode 无 `fd2_globals` 宏（dsi 0x1B/0x18），模块内以
  `(const char*)&fd2_data1[...]` 直取；FDICON/TMP 用既有 aFdiconB24/aFd2Tmp。
- **图集重建**：'buf'(0x53A61) 先 `guest_free`（未置空），随后 `unit_entry_data` 因 `dword_53BDF==0`
  重新 `guest_malloc(0x32A00)`；FD2.TMP 的 `fwrite` 用重建后的图集。
- **chkstk**：`push 0x3C; call 0x3702F`（不分配帧，仅探守卫页），丢弃；CRT fopen/fread/fwrite/fclose
  → libc（同 scene_core），malloc/free → `guest_malloc`/`guest_free`。
- 无对拍器，标“已还原未验证”。

### 31) 商店/地点菜单列表行绘制 → `src/game/shop_rows.c/.h`（本轮，3 函数）

`0x2670E`（地点菜单，待还原）派生的商店屏家族（`0x279BC`→`0x2872B/28CBD/28F65`→
`0x27738/275E6`；`0x2AF28`→`0x2A29D`；`0x2AA00`→`0x2A857`）共用的三个行绘制器；
本轮 `function-map.csv` 闭包就绪游戏函数头名（三者的 callee 全在册，真闭包＝自身）。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x272D0` | `shop_item_rows(count,items,cursor,dst,half_price)` | 最多 6 条（两列，列偏移 148，行基 26）物品行：按 `tbl_ptr(0x602AD,23)` 物品项的 `[0]` 选类型块 59/60/61；名字流 `idx+181`；按类型画子块 64/65/66/67（其余走 `res_blit6` t41）配带符号 16 位数字（`[+1]/[+5]/[+14]`，base 42，3 位）；末块 t15 + 5 位数字（`[+19..20]`，base 119），`half_price` 时数字取 `3*v>>2`。 |
| `0x2A0C2` | `shop_party_bar_rows(count,dst,cursor,members)` | 最多 3 条队伍行：单位精灵（bank 0x53A61，帧 `12*rec + dword_53F72`，`==3`→1）、名字 `rec[8]+1`、标签 `rec[31]+140`/`rec[32]+150`、末条 t15 + 5 位数字 `rec[33] * (int16)word_52399[rec[32]-1]`（base 119）。 |
| `0x2A694` | `shop_party_equip_rows(count,dst,cursor,members,equip)` | 最多 3 条队伍行：同单位精灵与前三条流，第 4 条流经设备表 `tbl_ptr(0x615FE,2,idx,-64)`（`(idx-0x20)*2`）取 `[0]+150`；行基 26，名字列 +40/+130/+175/+239。 |

**陷阱 / 伪代码伪影**：
- **共享尾声（伪 callee）**：`0x2A0C2` 尾 `jmp 0x26E30` 是 `add esp,0x14; pop ebp/edi/esi/ebx; ret`
  的共享尾声，与本函数前言（`push ebx/esi/edi/ebp; sub esp,0x14`）同帧 ⇒ 普通 `return`（写进 C，不模拟跳转）。
- **返回值**：三函数 Hex-Rays 声明 `int`，但返回的只是最后一次 callee 的 eax 残留，调用点均不读
  ⇒ C 侧声明 `void`。
- **原始表访问器**：`0x4E8BC`（物品项 base 0x602AD/stride 23）与 `0x4E7DD`（设备 base 0x615FE/stride 2/offset -64）
  按 playbook 改用 `tbl_ptr`；`sprite24_plain49(0x4E29C)`/`dlg_frame_tile(0x1685C)`/`res_blit6(0x16886)`/
  `dlg_draw_number(0x187D6)`/`vm_run(0x15F84)` 用语义名。
- **有符号度**：物品项 `[+1]/[+5]/[+14]` 与 `word_52399[]` 均为**带符号 16 位**（机器 `movsx`），
  `[+19..20]`/`word_52399[tier]` 的 `tier=rec[32]-1` 为**无符号字 / 有符号索引（可为 -1）**。
- 无对拍器，标“已还原未验证”；caller（`0x275E6/27738/2A29D/2A857`）仍待还原。

### 32) 商店/地点菜单的交互列表屏 → `src/game/shop_screen.c/.h`（本轮，4 函数）

§31 行绘制器（`shop_item_rows`/`shop_party_bar_rows`/`shop_party_equip_rows`）的三/四个调用者——
商店屏家族（`0x2872B/28CBD/28F65`、`0x2A43E`、`0x2AA00`）共用的整屏“暂存 + 键循环”；本轮
`function-map.csv` 闭包就绪游戏头名（四者 callee 全在册，真闭包＝自身）。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x27738` | `shop_items_paint(count,items,half_price)` | 物品屏一次性铺屏：三块 `guest_malloc(0xFA00)` → 快照 VGA → `rle2_blit(53C63+35845, 53F66+[53F66+70], 320)` 铺面板 → `shop_item_rows(count,items,53C57,53C63,half_price)` → 6 条 `msg_blit_band(13i+112,53C5B,53C63)`。**无键循环、不释放**（共享暂存由别处释放）。 |
| `0x275E6` | `shop_items_run(count,items,half_price)` | 物品屏键循环（`status_screen_run(1)`）：右/左（4D/4B）逐格移动光标 `dword_53C57`（钳在 0..count-1）、上/下翻页（48/50）走 2 格；每步越窗（下 `cursor-53F4E>=6`、上 `cursor<53F4E`）时 `dword_53F4E` 相应 ±2 并 `screen_band_forward/backward()`，随后 `shop_item_rows` 重绘到 `0xA0000`。回车/空格返 1、Esc 返 -1。 |
| `0x2A29D` | `shop_party_run(count,members)` | 队伍屏键循环（`status_screen_run(2)`）+ `shop_party_bar_rows`；上/下（48/50）逐格移动、越窗 `dword_53F4E` ±1。写 `dword_53F62=members`、`dword_53F5E=count`。返 1/-1。 |
| `0x2A857` | `shop_party_equip_run(count,members,equip)` | 同上，但第 4 条标签流取自设备数组：`shop_party_equip_rows(count,dst,cursor,members,equip)`。返 1/-1。 |

**陷阱 / 伪代码伪影**：
- **ABI 由离线反汇编定案（chkstk 骗过反编译器）**：`0x2A29D`/`0x2A857` 的 Hex-Rays 原型被削成 `int(int)`，
  其中 `v5=[ebp+8]`/`v6=[ebp+Ch]` 被当“未初始化局部”。机器前言 `push ebx/esi/edi/ebp` 后
  `mov edi,[esp+0x14]`（第 1 实参）/`mov ebp,[esp+0x18]`（第 2 实参）；`0x2A857` 的第 3 实参在
  `push [esp+0x1c]`（2A694 调用点）。故签名 `(count, members)` / `(count, members, equip)`。
  `0x275E6`/`0x27738` 的第 3 实参按 `movzx eax,BYTE PTR [esp+0x18]`/`[esp+0x10]` 定为 **字节** `half_price`。
- **共享尾声（伪 callee）**：`0x2A29D`/`0x2A857` 尾 `jmp 0x26A73` 落在邻函数体内
  （`mov eax,esi; pop ebp; pop edi; pop esi; pop ebx; ret`），与本函数四寄存器帧同帧 ⇒ 普通
  `return esi`（循环结果），已写进 C（不模拟跳转）。
- **chkstk**：四者皆 `push size; call 0x3702F`——`0x3702F` 不分配帧（仅探守卫页），丢弃。
- **有符号度**：光标/窗口 `dword_53C57`/`dword_53F4E` 为 32 位，机器用 `jl/jle/jge`（有符号），
  C 侧显式 `(int32_t)` 转型（同 `menu_select.c` 约定）。
- **调用点互证**：`0x2872B` 先 `push 0;push items;push count;call 0x27738`（一次性铺屏）再同样压参 `call 0x275E6`
  （进循环）；`0x2A43E` 以 `(count,members)` 调 `0x2A29D`；`0x2AA00` 以三参调 `0x2A857`。
- **归属**：新建模块；三块 0xFA00 暂存仍走 `guest_malloc`（`menu_select.c`/`msg.c`/`box_popup.c` 共用同一全局）。
- 无对拍器，标“已还原未验证”；caller（`0x2872B/28CBD/28F65/2A43E/2AA00`）仍待还原。

### 33) 地图视图 / 形状库助手簇（本轮，3 函数）→ `map_shape.c/.h`（新）+ `map_sprites.c`/`map_status.c` 追加

本轮 `function-map.csv` 闭包就绪游戏头名三个 u2（真闭包＝自身；三个都属地图视图渲染族）：

| 原址 | 新名 | 模块 | 要点 |
|---|---|---|---|
| `0x1399C` | `map_shape_lib_decode` | **map_shape.c（新）** | 把当前的 `dword_53A5D` 形状库（头 `{u16 24,u16 24,u16 count}` + `count` 个 u32 流偏移（+6 起）+ RLE 流）解码为**连续的原始 24×24 位图库**（新头 + `count`×576B），返回新指针；调用方 `0x2000A`/`0x21548` 把旧指针存起、把新指针写回 `dword_53A5D`，再建 64×64 指针表供 `map_zoom_draw` 采样。 |
| `0x1CAC7` | `map_record_layer_blink` | map_sprites.c | 两次 `map_draw_record_layer`（帧 74 画入 `dword_53A49`、帧 75 画入 `guest_malloc(0x25680)` 暂存）→ 4 轮交替把两幅 312×192 窗发布到 `0xA0504`（每半轮 `j___delay(90)`）→ `map_view_update(0)` + 释放暂存。 |
| `0x1EC2A` | `map_status_anchor` | map_status.c | 记录 `rec` 在当前地图视图里的两 int `{x,y}` 锚点（`24*(rec[0]-dword_53AA9)+4`、`24*(rec[1]-dword_53AAD)`），再按 `rec[3]` 分支向 320×200 框内微调；驱动 `0x1EB05`（`0x1548E` 的状态叠加层）调用两次填 `dword_53A30`/`dword_53A38` 的 actor/target 两半 `pos`。 |

**陷阱 / 伪代码伪影**：
- **共享尾声（伪 callee）**：`0x1399C` 尾 `jmp 0x10C49` 落在 `0x10B4E`（`unit_sprites_build`）体内的共享尾声
  （`add esp,4; pop edi; pop esi; pop ebx; ret`）⇒ 普通 `return`（eax = 新库指针）；`0x1CAC7` 尾 `jmp 0x14B72`
  落在 `ev5_14B16`（0x14B16）体内共享尾声（`add esp,4; pop esi; pop ebx; ret`）⇒ 普通 `return`。两者均写进 C，不模拟跳转。
- **OOM 尾不同形**：`0x1399C` 的失败路径是 `push aOutOfMemoryAtR(0x50122); jmp 0x10056`——即
  `printf; exit(1)`，**不含** `scenario_load` 的 `word_53A8D/int386(0x10)` 文本模式切换（后者在 `0x10034`）。
- **ABI（chkstk 骗过反编译器）**：`0x1CAC7` 的机器帧只读第 3/4 实参（`[esp+0x1c]` 两次＝`count`/`list`），
  **第 1/2 实参从不读**；两次调用均为 `map_draw_record_layer(base, count, list, 74/75)`（`base` 先 `dword_53A49` 后暂存）。
- **无符号字节比较**：`0x1EC2A` 的 `rec[3]` 判据机器为 `movzx; cmp 2; jge`，值域 0..255，C 侧写 `r[3] < 2u`；
  `dword_53AA9` 机器按 **32 位 dword** 读进 24 位算式（`(int)dword_53AA9` 取 qword 视图低位），`dword_53AAD` 为 `int32_t`。
- **返回值**：三者尾声 eax 均被调用点忽略（`0x1399C` 例外——调用方把 eax 存回 `dword_53A5D`），
  故 `0x1CAC7`/`0x1EC2A` 声明 `void`、`0x1399C` 声明 `uint8_t *`。
- **chkstk**：三者皆 `push size; call 0x3702F`（不分配帧），丢弃。
- **归属**：`map_sprites.h`/`map_status.h` 已在伞头；新增 `map_shape.h`，伞头字母序插在 `map_scroll.h` 与 `map_sprites.h` 之间。
- 无对拍器，标“已还原未验证”。

### 34) 单位移动动画绘制相位簇（本轮，3 函数）→ `src/game/map_units.c/.h` 追加

闭包就绪游戏函数中 **同 caller（`sub_22253`）** 的小簇：0x22253（541B，待还原，推断组 `ev3`）
的唯一三个 callee，即“单位移动动画”的绘制相位。三者真依赖全部已还原/已验证。

| 原址 | 新名 | 行为 |
|---|---|---|
| `0x22470` | `map_units_move_walk` | 走位：对 `dword_53AD1` 的 11 帧（下标 114..124）循环——从 `scratch` 还原地图视图；`rle2_blit_trans` 把该帧贴到格 (dst_x,dst_y)（`+0x8088 + 24*(dx−视图原点) + 10944*(dy−视图原点) + 456`，步长 456）；`dlg_portraits_refresh`；`gfx_copy_rows` 发布 312×192 到 `0xA0504`；`svc_wait_ticks(1)`。 |
| `0x22547` | `map_units_move_reveal` | 揭示：先把 `sprite` `rle2_blit_trans` 进 `scratch` 同一格，再对 i=5..0——从 `scratch` 还原、`reveal_band(cx,cy,11,(cy/5)*i,192, dword_53A6D 帧 i)`、发布、`j___delay(10)`；收尾两次 `svc_wait_ticks(1)`，返回最后一帧 sprite（EDI）。 |
| `0x22656` | `map_units_move_settle` | 稳定：i=0..9 的 `reveal_band(cx,cy,11,0,192, dword_53A6D 帧 i)`；只读第 3/4/5 实参。 |

**陷阱 / 伪代码伪影**：
- **chkstk 骗过反编译器（ABI 以离线反汇编定案）**：`0x22547` 的 `prototypes.csv`/`arity.json` 记为
  `int(int)`（1 参），但机器前言 `mov ebp,[esp+0x1c]; mov esi,[esp+0x24]; mov eax,[esp+0x14]` 与唯一调用点
  `0x22348`（`0x22253` 内，压 **6** 个实参）都要求 **6 参**；Hex-Rays 把多出的实参标成越帧局部 `v4..v8`。
  C 侧按 6 参还原。
- **返回值是 sprite 指针而非 int**：`0x22547` 尾 `jmp 0x1E5B9`（落在 `0x1E529` 体内的共享尾声
  `mov eax,edi; pop×4; ret`）⇒ eax = 循环最后一帧 EDI（指针）；`0x22253` 随即把它当 `reveal_band` 的 table。
  C 侧声明 `const void *`（台账 `int` 系尾声不在本体内造成的误标）。
- **`0x22656` 只读第 3/4/5 实参**（`[esp+0x1c]`=`scratch`、`[esp+0x24]`=cy、`[esp+0x34]`→cx）：调用点仍压 7 参
  （前两参为该驱动的目标格，末两参为 sprite/返回值），C 侧保留 7 参签名，未用者留名并以 `(void)` 消除告警。
- **共享尾声（伪 callee）**：`0x22547` 尾 `jmp 0x1E5B9` 落在 `0x1E529`（已验证 dlg 资产）体内，是普通返回，非调用。
- **chkstk**：三者皆 `push size; call 0x3702F`（不分配帧），丢弃。
- **归属**：追加进既有 `map_units`（单位精灵在地图视图内的动画），未改已验证模块；`map_units.h` 已在伞头，无需改列。
- 无对拍器，标“已还原未验证”。

### 35) 单位移动动画驱动（本轮，1 函数）→ `src/game/map_units.c`（追加，单位移动族闭合）

§34 三个相位的调用者 `0x22253`（541B，u5）是本轮 `function-map.csv` 闭包就绪游戏头名，其 callee 全在册
（含 §34 三相位 + `res_load`/`map_render_view`/`svc_play_sfx`/`reveal_band`），故按同模块簇整体拉出：

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x22253` | `map_units_move_play(rec, rec_x, rec_y, at_x, at_y)` | `res_load(FDOTHER,0,81)`（音效库）→ `guest_malloc(0x25680)` 暂存 → `map_render_view(暂存+0x8088,…)` → `map_units_move_walk(at_x,at_y,暂存)` → 若记录的新格 `(rec_x,rec_y)` 与动画格 `(at_x,at_y)` 相同则音效索引取 1（否则 0），`svc_play_sfx` → `map_units_move_reveal`（返回末帧 sprite）→ **把记录 `dword_53A45+80*rec` 的字节 0/1 写成 `(rec_x,rec_y)`** → `memmove(dword_53A49, 暂存, 0x25680)` → `reveal_band(cx,cy,11,0,192,末帧)` → 把约 24 行（视图首行 18 行）的 312 字节窗口从暂存拷贝回帧缓冲（每行 24 字节、行距 320）→ `map_units_move_settle` → `guest_free`×2。`cx=24*dword_53AB9+12`、`cy=24*dword_53ABD+15`。 |

**陷阱 / 伪代码伪影**：
- **ABI（chkstk 骗过反编译器，以离线反汇编定案）**：机器帧 `push ebx/esi/edi/ebp; sub esp,0x1c` 后
  `mov ebx,[esp+0x30]` 即第 1 实参 `rec`；第 2/3 实参 `[esp+0x34]/[esp+0x38]` 写入记录字节 0/1，
  第 4/5 实参 `[esp+0x3c]/[esp+0x40]` 为动画格；比较 `arg2==arg4 && arg3==arg5` 定音效索引。
- **`free((void*)p)` 的误名**：Hex-Rays 把 `res_load` 结果（栈 local `[esp+8]`）误渲染成第二形参 `p`；
  机器实为 `0x3776E`(guest_free) 释放该 `res_load` 结果，C 侧走 `guest_free` 缝。
- **sprite 偏移**：`dword_53AD1 + *(uint32_t*)(dword_53AD1+0x1F6)`（0x1F6=502，单帧覆盖图集项）。
- **共享尾声非跳转**：机器尾 `jmp 0x18888`（落进 `dlg_draw_number` 体内尾声）＝普通 `return`；
  `function-map` 误列 `sub_187D6`，真实依赖不含它（故未调用）。
- **伪 callee**：`0x22253` 的 callee 表未含 §34 相位之外的真依赖；`sub_3702F`(chkstk) 丢弃。
- **别名**：`scene_states.c`（§7/§34 既有）已按 IDA 名调用 `sub_22253`，故 `map_units.h` 沿用同头
  `#define sub_22253 map_units_move_play` 保持调用点不变；返回值原为残留 eax，调用点均不读，C 侧声明 `void`。
- **chkstk**：`push 0x4C; call 0x3702F`（不分配帧），丢弃。
- **归属**：追加进既有 `map_units`（§34 三相位的驱动），单位移动族（`0x22253` + 三相位）现闭合；
  其 caller `0x2218A`/`0x250CC`/`0x2548C`/`0x33F78` 仍待还原。无对拍器，标“已还原未验证”。

### 36) 单位部署/跟随（本轮，1 函数）→ `src/game/map_units.c`（追加）

§35 的 `0x22253` 唯一在册 caller `0x2218A`(u4, 201B) 是本轮 closure-ready 头名，callee 全在册
（`dlg_portrait_glide`/`rec_sub_table5`/`map_slide_view`/`map_units_move_play`），按同模块簇拉出：

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x2218A` | `map_units_move_deploy(actor, unused, target)` | `dlg_portrait_glide(*target)`；`rec_sub_table5(actor,23)`；`dword_53EC8 += 10*rec[33]`（`rec[32]∈(8,25)` 时再加 30）；`map_units_move_play(idx,255,255,rec[0],rec[1])`（从图外落到记录原格）；`dword_51A83=0`；`map_slide_view(qword_51CF9 低/高)`；`map_units_move_play(idx,x,y,x,y)`（落到 qword_51CF9 格）；`dword_51A83=1`。`rec = dword_53A45 + 80*idx`。 |

**陷阱 / 伪代码伪影**：
- **qword_51CF9 = 坐标对**：机器分两次 `push [0x51cfd]`/`push [0x51cf9]` 作 `map_slide_view`/`map_units_move_play` 的 (x,y)；Hex-Rays 渲染成 `qword_51CF9`/`SHIDWORD`。C 侧低 32 位为 x、高 32 位为 y。
- **中参未用**：`0x2218A` 唯一调用点 `0x20C6F` case 23 压 3 参（`sub_2218A(a1,a3,a4)`），体只读第 1/3 参；第 2 参保留 3 参 ABI，C 侧 `(void)` 消警。
- **chkstk**：`push 0x1c; call 0x3702F`（不分配帧），丢弃。
- **伪 callee**：`sub_3702F` 丢弃；其余 callee 均为真依赖。
- **归属**：追加进既有 `map_units`（§34/§35 单位移动族），未改已验证模块；无对拍器，标“已还原未验证”。

### 37) 单位 AI 目标/行动评分器（本轮，4 函数）→ 新建 `src/game/map_ai.c/.h`

本轮 closure-ready 头名是 `0x15880`(u1) 与 `0x15DA2`(u1)；二者分别是 AI 决策驱动 `0x1567E`/`0x15B77`
的评分助手，两个 driver 的 callee 亦全部在册（`map_reveal_reachable`/`map_scan_records`/`ev5_14B16`/
`util_fix_records`/`rec_slot_free`/`rec_skip`/`rec_collect_slot_bits`/`tbl_ptr` 访问器），故按同模块簇整体拉出
（4 函数，1416B；决策驱动 `0x14EF0`、行动目标挑选器 `0x1598A` 仍待还原）：

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x15880` | `map_ai_item_score(item,count,list)` | 按 `tbl_602AD`(0x602AD,stride23) 条目 `item` 的 kind(+13)：kind∈{5,13} 时对每个目标按 HP(+64)/上限(+66) 比 `max/3`、`max/2` 取 0/3/8，若 `(int8_t)rec[52]<0` 再 `*3`；kind∈{20,21,24} 时 `w=word(+14)`、`tbl_619FD(w)` 首 16 位作阈值（kind≠24 时改写 w），每个目标 `w>=rec[64]?18:8`。返回总和。 |
| `0x15DA2` | `map_ai_flag_sum(count,list,field,weight)` | 对 `list` 每条目标记录，若 `rec[field]==0` 则 `sum += weight`。 |
| `0x1567E` | `map_ai_pick_item_target(actor,require_empty)` | `dword_53C33=0`；`guest_malloc(0x190)`；`rec_slot_free(actor)=n`（为 0 直接返回 0）；对 i<n：取槽位物品 `rec[2i+11]` 查 `tbl_602AD`；`eff[16]>15` 时 band=rad=1，否则 band=eff[16],rad=0；`eff[13]==0` 跳过；`map_reveal_reachable(v17,v18,NULL,band,rad,0)`→`ev5_14B16(buf)` 取候选格→逐格按 `eff[16]<=15` 分别 `map_reveal_reachable(cx,cy,cells,eff[18],0,v7)` 或 `map_scan_records(cx,cy,cells,v17,v18,eff[16]-16,0)`（`v7 = require_empty?eff[17]:(eff[17]==0)`）→非空则 `map_ai_item_score(item,count,cells)`，更大则存 `dword_53C33/37/3B/3F`。收尾 `guest_free`，返回 0。 |
| `0x15B77` | `map_ai_action_score(action,count,list)` | 按 `action`：<13 时（≥10 先 `rec_skip` 过滤）按 `tbl_619FD(action)` 首 16 位阈值取 8/24，`rec[8]==0` 时 `(int)(s*dbl_50144)`；<17 时按 HP/3、HP/2 取 0/3/8，`rec[52]&1` 时 `*2`；<20 转 `map_ai_flag_sum(count,list,action+17,3)`；case 20/21/22 按 `rec[37]/[38]/[39]` 各 +6（22 需 `rec[39]==0 && rec_collect_slot_bits(list[i],NULL)!=0`）；case 26/27 转 `map_ai_flag_sum(...,37/38,4)`。 |

**陷阱 / 伪代码伪影**：
- **`0x15880` 返回 int 而原型标 void**：`0x1567E` 的 Hex-Rays 把调用结果写成未初始化局部 `v11`（原型 void 所致）；机器 `call 0x15880; cmp eax,[0x53c33]`，C 侧按 `int` 捕获返回值。
- **`__CHP` 浮点取整**：`0x15B77` 的 `fild/fmul dbl_50144/call 0x37AF4(fistp)` 即 `(int)((double)s*dbl_50144)` 截断。
- **同行不同判据**：`0x15880` 用 `test byte[rec+52],0x80`（`(int8_t)rec[52]<0`），`0x15B77` 同位置用 `test byte[rec+52],1`（`rec[52]&1`）——按机器分别还原。
- **无符号 word 的 /3、/2**：机器先 `movzx` 零扩展再 `idiv`/`sar`，实为无符号语义；C 侧取 int（0..65535）后做整除，比较同为有符号（值域正，等价）。
- **`case 22` 的 dry-run**：`rec_collect_slot_bits(list[i], NULL)` 的第二参机器压 `rec[39]`（此分支下恒 0＝空指针），`rec.c` 已支持 `out=NULL` 的 dry run。
- **chkstk**：四者皆 `push size; call 0x3702F`（不分配帧），丢弃。
- **`0x1567E` 的丢弃调用**：`push 0; call 0x4E8A5`（`tbl_61646(0)`）返回值机器未用，C 侧以 `(void)tbl_ptr(...)` 保留。
- **`0x1567E` 早期返回不 free**：`rec_slot_free==0` 时机器直接返回（跳过 `free(p)`），C 侧照写（原泄漏行为）。
- **归属/别名**：新建 `map_ai`；现有还原模块无按 IDA 名调用这四者（`0x1598A`/`0x14EF0` 仍待还原），故无需 `#define` 别名。无对拍器，标“已还原未验证”。

### 38) 单位 AI 行动目标挑选器（本轮，1 函数）→ `src/game/map_ai.c/.h`（追加）

本轮 closure-ready 游戏头名是 `0x1598A`(u3)——`map_ai` 同簇、callee 全在册（`rec_collect_slot_bits`/
`path_mark`/`ev5_14B16`/`util_fix_records`/`map_reveal_reachable`/`map_ai_action_score` + `tbl_ptr`）。调用量
更高的闭包头名 `0x3645D`(u4)/`0x2E95B`(u3)/`0x365DA`(u2) 仍是 CRT/库包装（read/fread/int386），按
module-map §2 既定策略跳过。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x1598A` | `map_ai_pick_action_target(actor,require_empty)` | `dword_53C23=0`；`cost_row=tbl_61646(0)`；`p=guest_malloc(0x190)`；`limit=word[rec+68]`；`rec_collect_slot_bits(actor,items)=n`（`n==0` 或 `rec[39]!=0` 直接返回 0，原泄漏 p）；对 i<n：`act=tbl_619FD(items[i])`，`act[5]>limit` 跳过；`path_mark(cost_row,x,y,act[3],dword_53A51,dword_53A69)` → `ev5_14B16(p)` 取候选格 → 逐格 `flag=require_empty?act[6]:(act[6]==0)` → `map_reveal_reachable(cx,cy,cells,act[4],0,flag)` → 非空则 `map_ai_action_score(items[i],cnt,cells)`；`score>dword_53C23 || (score==dword_53C23 && word[act]>best)` 时存 `dword_53C23/27/2B/2F`；收尾 `guest_free`，返回 0。 |

**陷阱 / 伪代码伪影**：
- **未初始化的 tie-break 槽**：机器首次比较时读未初始化的 `[esp+0x48]`（仅成功更新后才写回 `word[act]`）；C 侧定为 `best=0` 并注释。
- **`sub_3702F` chkstk**：`push 0x84; call 0x3702F`（不分配帧），丢弃。
- **早期返回不 free**：`n_items==0` 或 `rec[39]!=0` 时机器跳过 `free(p)` 直接返回 0，C 侧照写（原泄漏行为）。
- **无符号 word**：`movzx word[esi]`/`word[esi+0x44]` 零扩展，C 侧按 `uint16_t` 读取。
- **归属/别名**：追加至既有 `map_ai`；被调方均以语义名调用（`path_mark`/`map_reveal_reachable`/`map_ai_action_score`），无需新增 `#define` 别名。无对拍器，标“已还原未验证”。

### 39) 治疗量/魔法点恢复簇（本轮，2 函数）→ `src/game/item_use.c/.h`（追加）

本轮 closure-ready 游戏头名是 `0x1C8ED`/`0x1C9DD`(u1)——两者同簇（共用已还原的 `unit_heal`(0x1C916)），
真闭包＝自身（callee：`sub_1C916`(item_use)/`sub_4E866`(tbl_ptr 0x619FD)/`sub_4EBE3`(util_rand)）。
调用量更高的闭包头名 `0x3645D`(u4)/`0x2E95B`(u3)/`0x365DA`(u2) 仍是 CRT/库包装（read/fread/int386），
`0x190AC`/`0x1B1E7`(u1) 真闭包 >8 函数，按 module-map §2 既定策略跳过。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x1C8ED` | `unit_heal_by_action(rec,action)` | `act=tbl_619FD(action)`（0x619FD/stride7）；`unit_heal(rec, *(int16*)act)`——按行动条目首 16 位作治疗量。 |
| `0x1C9DD` | `unit_restore_mp(rec,amount)` | 对 +68 池（上限 +70）做与 `unit_heal` 同形的恢复：`new=mp+9*amount/10+(rand%100)*amount/1000`，夹到 +70；`r[7]<0x4B` 时 `dword_53EC8 += 40*r[33]*(new-mp)/max`（无 +30 加成、无 r[32] 判定）；返回 fixed+random。 |

**陷阱 / 伪代码伪影**：
- **共享尾声**：`jmp 0x22BBE`（1C916 尾部，`add esp,4; pop ebp/edi/esi/ebx; ret`）是普通尾声；1C9DD 的 `jmp 0x1C9C7` 跳入 1C916 体内的 XP 记帐序列（`idiv [esp]` 用各自局部 max），C 侧展开为独立代码。
- **签名**：`sub_1C8ED`/`sub_1C9DD` 机器均 `movsx`/无符号读取，原型记 `void(int,int)` 但实际返回 eax；C 侧按 `unit_heal` 惯例返回 int（调用方 21B18/20C6F 均忽略返回值）。
- **`sub_3702F` chkstk**：`push 0x10`/`push 0x18; call 0x3702F`（不分配帧），丢弃。
- **`max != 0` 守卫**：机器 `idiv [esp]`（[esp]=max）无显式零判定，与既有 `unit_heal` 一致加 `&& max != 0`（真实调用点 20C6F 已先判 `word[rec+70]!=0`），避免 C 侧除零。
- **+68/+70 语义**：`box_status_draw`(0x17FC0) 将其绘为第二根量表、`ev5_25089` 令 `+68=+70`，判为魔法点池。
- **归属/别名**：追加至 `item_use`；被调方均以语义名调用（`unit_heal`/`tbl_ptr`/`util_rand`），无需新增 `#define` 别名；`fd2_game.h` 已含 `item_use.h`。无对拍器，标“已还原未验证”。

### 40) 地图"对队员使用道具"屏簇（本轮，3 函数）→ `src/game/item_menu.c/.h`（新建）

本轮 closure-ready 游戏头名仍被 CRT/库包装 `0x3645D`(u4)/`0x2E95B`(u3)/`0x365DA`(u2) 与巨型 CRT
包装 `0x190AC`(u1，callee 全为 `sub_34xxx..sub_36xxx` 数学库) 占据；`0x1B1E7`(u1) 真闭包自身但**内层读未初始化**
（`retaddr`/`v0`，§32 已记）暂跳。按"同 caller"聚类，取仍待还原的驱动器 `sub_2AA00`(0x2AA00) 的三个
callee——`0x2AE0E`/`0x2AC7D`/`0x2FB2C`——作为一簇（1350B，真闭包＝自身，callee 全在册）。本批后
`sub_2AA00` 的待还原 callee 由 3 归零（该驱动器已闭包就绪）。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x2AE0E` | `item_menu_list(records,kinds)` | 扫 `i<dword_53BFB` 的 80B 记录：`rec[33]>=0x14 && rec[7]<0x12 && rec[7]!=7`；`records[n]=i`，`kinds[n]=type+32`，`ev3_2AEDB(i,byte_523D5[type])!=-1` 时 `type+50`，`type==9 && ev3_2AEDB(i,90)!=-1` 时 52；返回 n（**2 个实参**，反编译只列了 1 个）。 |
| `0x2AC7D` | `item_menu_use(rec)` | `tbl_620A1(rec[7])`；`kbd_flush`→`msg_open_portrait(rec[7])`→`dword_53AD9=rec[0x20]+150`→`vm_run(...,595,0xA951F,320,205,76,74,19,1)`→`dlg_blit_dato`→`kbd_flush`；五连 `msg_show_page(rec+0x37/0x39/0x3e/0x42/0x46, tbl+0..8, 490..494, prev_page)`；`tbl_615FE(rec[7])[1]!=0` 时 `vm_run(...,596,6080*page+0xA951F,...)`+`dlg_wait_key(0)`+`rec[0x3b]+=dword_53AE1`；`unit_metrics(rec)`+`msg_close_portrait()`+`rec[0x21]=1/rec[0x3c]=0/word[+0x40]=word[+0x42]/word[+0x44]=word[+0x46]`；末尾 `kbd_flush`。 |
| `0x2FB2C` | `item_menu_figure_play(rec,group)` | `res_load(BG.DAT)` ×3 → `dword_5413F[0..2]`；`malloc(0x1F400)` 工作缓冲 +`malloc(0xFA00)` 快照；`res_load(FIGANI.DAT, 3*rec[7])`/`(…,3*group)`；`pal_fade_out`+`svc_wait_ticks(1)`；`i=8..0`：`memset`+（BDA tick 非 0 时 `strip=(strip+1)%3`）+`rle_decode(dword_5413F[strip],0,50,work,640,-1)`+`res_blit(fig_user,0,work+10*i,640,-1)`+`gfx_copy_rows`+`pal_fade_range(0,255,6*i)`；`figure_frame_run(fig_user,work,16)`；`j=0..19` `pal_fade_add(0,255,3*j)`+`j___delay(10)`；再 `rle_decode+res_blit(fig_target,work+10*j=+200)+gfx_copy_rows+pal_fade_range(0,255,0)+figure_frame_run(fig_target,work,24)+pal_fade_out+memmove(恢复)+pal_fade_in`；七次 `free`。 |

**陷阱 / 伪代码伪影**：
- **`sub_2AE0E` 实参个数**：反编译原型记 `int(int)`，但反汇编读 `[esp+0x1c]`（记录号输出数组）与 `[esp+0x20]`（kind 输出数组）两槽；调用点 `sub_2AA00` 亦压两参（`v10,v9`）。C 侧定为 2 参。
- **`dword_5413F` 是 3 元素数组**：`dword_5413F[0]` 与独立标量 `dword_54143`/`dword_54147` 相邻（0x5413F/43/47）；`res_load` 返回值经 `(uint32_t)(uintptr_t)` 存入。
- **BDA tick 判定**：`mov eax,0x46c; movsx eax,WORD[eax]; je` → `if (BDA_W(0x46C)!=0) strip=(strip+1)%3`；反编译误渲染为 `if (&p != nullptr)`。
- **`0x2AC7D` 返回值**：机器 `ret` 时 eax 来自其调用的 `kbd_flush`（void），无意义；C 侧声明为 void（调用方忽略）。
- **`sub_2FB2C` 的 i/j 复用**：`j` 循环结束后第 2 次 `res_blit` 用 `work+10*j`（j==20→+200），C 侧按同序写。
- **chkstk**：三者皆 `push size; call 0x3702F`（不分配帧），丢弃。
- **归属/别名**：新建 `item_menu`；被调方均以语义名调用（`msg_show_page`/`vm_run`/`res_load`/`figure_frame_run`…），无需 `#define` 别名（`sub_2FE14` 已按 figure_view.h 的 `figure_frame_run` 调用）。无对拍器，标"已还原未验证"。

### 41) 攻击结算助手簇（本轮，2 函数）→ `src/game/map_attack.c/.h`（新建）

本轮 closure-ready 游戏头名仍被 CRT/库包装 `0x3645D`(u4)/`0x2E95B`(u3，需 CRT `fread`)/`0x365DA`(u2)
与巨型 CRT 数学库包装 `0x190AC`(u1) 占据；`0x1B1E7`(u1) 真闭包自身但**内层读未初始化**（`retaddr`/`v0`，§32 已记）暂跳。
按"同 caller"聚类，取仍待还原的驱动器 `sub_1E856`(0x1E856，攻击血条收束动画) 的两个 callee——
`0x1E98C`/`0x1ECC7`——作为一簇（1276B，真闭包＝自身，callee 全在册）。本批后 `sub_1E856` 的
待还原 callee 由 2 归零（该驱动器已闭包就绪）。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x1E98C` | `map_attack_sprite(attacker,defender)` | 攻击者的道具（`rec_slot_find(a1,0)`→`rec_field_byte`→`tbl_ptr(0x2AD,23)`）选出动画脚本 `tbl_u32(0x1955,eff[0])`：脚本首字节为帧数，每帧跳过 0xFF 的 SFX 索引（`byte_53C6B!=0` 时改用固定 4）、`res_draw_subimage(dword_53AD1,0xA0000,320,x,y,script[2i+1])`+`j___delay(80)`+`dlg_snap_restore`；`byte_53C6B==0` 且 `i==0/1` 时先 `view_draw_unit_sprite(0xA0504,320,a2,2,253 / 0,0)`。x/y 由 defender 记录 x/y（+0/+1）减视口原点 `0x53AA9`/`0x53AAD` 后 ×24。 |
| `0x1ECC7` | `map_attack_resolve(attacker,defender)` | 取 attacker/defender 记录（`dword_53A45+80*idx`）的 +0x48/+0x4A/+0x40/+0x42/+0x4C/+0x4E 字与 +0x20/+0x21 字节；道具条目的 `[9]`(v7)/`[10]`(v24) 定性；`!rec_skip(rec)` 时 `map_cell_info` 取 `info[5]` 并按 `dword_51A12/51A2A` 加成 +0x48/+0x4A；`byte_524A8[rec[0x20]-1]` 起手，`v7==4` 加 v24，`v7==2` 时 RNG 命中则改 defender[0x25] 并闪屏（`ev5_203BD`/`pal_fade_add`）；主判定 `rng%100<v21-v20` 时置 `byte_53C6B=0`、二次闪屏、按 `9*(atk-def)/10`（+RNG）算伤害、`defender[+0x40]-=dmg`（下限 0），返回剩余 HP；`attacker[6]==2 && defender[7]>=0x44` 时按 `tbl_ptr(0xAF9,10,defender[7]-68)` 与 `v29/v30` 写经验 `dword_53EC8`。 |

**陷阱 / 伪代码伪影**：
- **`sub_1ECC7` 实参个数**：Hex-Rays 原型记 `int(int)` 并把第二参渲染成帧指针相对局部 `v31`（本函数无 `mov ebp,esp`）。调用点 `sub_1E856` 先压 a2 再压 a1（1E90A/1E90E），被调方读 `[esp+0x50]` 作 a2 ⇒ 真原型 `int(int,int)`；C 侧定为 2 参。
- **v30 字节回绕**：`v30 = v2[0x21]` 在机器码里是字节槽，`add BYTE[esp+0x34],0x1e` 回绕 mod 256（`movzx` 读回），故 C 侧用 `uint8_t v30`。
- **共享尾声**：`0x1E98C` 尾 `jmp 0x1317D`、`0x1ECC7` 尾 `jmp 0x111B2` 均落在他函数体内，是纯 epilogue（`0x1ECC7` 已先 `mov eax,[esp+0x2c]` 返回值）；C 侧作普通 `return`。`0x1ECC7` 返回剩余 HP（`v28`），调用方 `sub_1E856` 以之驱动血条。
- **`0x1E98C` 返回值**：机器跳出时 eax 未定义，唯一调用方忽略，C 侧声明 `void`。
- **chkstk**：两者皆 `push size; call 0x3702F`（不分配帧），丢弃（假 callee）。
- **归属/别名**：新建 `map_attack`；被调方均以语义名调用（`rec_slot_find`/`rec_field_byte`/`rec_skip`/`tbl_ptr`/`tbl_u32`/`svc_play_sfx`/`view_draw_unit_sprite`/`res_draw_subimage`/`dlg_snap_restore`/`pal_fade_add`/`ev5_203BD`/`util_rand`/`map_cell_info`），无需 `#define` 别名。无对拍器，标"已还原未验证"。

### 42) 攻击结算/状态叠加/升级屏家族（本轮，4 函数）→ `map_attack.c`/`map_status.c` 追加 + 新建 `src/game/map_levelup.c/.h`

本轮 closure-ready 游戏头名是 `0x1E856`(u2)——`sub_1548E`（攻击/升级结算驱动器，待还原）的
闭包头名。其同 caller 家族还有 `0x1EB05`(u1)、`0x1E292`(u2) 与后者的私有助手 `0x1D79C`(u1)；
四者真闭包＝自身（callee 全在册/已验证），合为一个连贯簇（合 1377B）。拉出后 `sub_1548E` 的
待还原 callee 由 4 归零（该驱动器已闭包就绪）。

| 原址 | 新名 | 模块 | 要点 |
|---|---|---|---|
| `0x1E856` | `map_attack_bar_drain` | map_attack.c | 逐帧把 `defender`（记录 `dword_53A45+80*a2`）的血条从攻击前段数 `70*cur/max` 排到 `map_attack_resolve` 返回剩余 HP 对应的 `69*rem/max+1`：每 tick `ui_meter_fixed(pos[0]+320*(pos[1]+6)+0xA0000+7,320,v8)` + `j___delay(8)`，段数递减；`attacker` 手持道具的 `[9]==3` 或 `rand%100<3` 时整体重复一次（机器 v10 count-down do/while）。 |
| `0x1EB05` | `map_status_overlay_play` | map_status.c | 用 `map_status_anchor` 填 target（`&dword_53A30`）；`ev5_1F0DC==1` 时再填 actor（`&dword_53A38`）否则 `dword_53A38=-1`；10 帧把头像子图 39..48 `res_draw_subimage` 贴到 `0xA0000`（actor 帧用 `dword_53A38/3C`），前 9 帧 `dlg_snap_restore`，末尾 `guest_free`；返回 `&dword_53A30`。 |
| `0x1E292` | `map_levelup_run` | **map_levelup.c（新）** | 升级屏：XP(`dword_53EC8`)为 0、`rec[5]&1`、或已达上限（职业 30/31→99，否则 40）直接返回；否则取职业表 `0x4E821`、累计 `xp=rec[60]+dword_53EC8`；`kbd_flush`+`msg_open_portrait`+vm488+`dlg_blit_dato`；`while(xp>=100)`：`++rec[33]`、vm489、5 条 `msg_show_page(rec+0x37/39/3e/42/46, tbl+2i, 490..494, page)`、职业增益表 `0x4E7F2[rec[10]]` 命中等级则置 `dword_53AD9=gain+441` + `map_levelup_mark_gain` + vm587(`6080*page+0xA951F`)、`unit_metrics`、`xp-=100`（满级清零）；收尾 `ev5_1E5C0(11)`/`ev4_196CB`、`rec[60]=xp`、`dword_53EC8=0`。 |
| `0x1D79C` | `map_levelup_mark_gain` | map_levelup.c | 置记录标志位：`rec[26 + gain/8] |= mask[gain%8]`，掩码表 `fd2_data1[0x2024]`＝{01,02,04,08,10,20,40,80}。 |

**陷阱 / 伪代码伪影**：
- **ABI（chkstk/冻结原型骗过反编译器）**：`0x1EB05` 的 `fd2_funcs.h` 原型为 `int(int)`，Hex-Rays 把第 2 实参标成未初始化局部 `v5`；机器读 `[esp+0x18]`（arg2），唯一调用点 `0x1548E` 压两参，故真为 `(actor,target)`。`0x1E856` 的 `0x1ECC7`/`0x1E98C` 调用点各压两参（`push [esp+0x24]` 连续两次，第二次因入栈后偏移回落而读到 arg1），真为 `(a1,a2)`。
- **共享尾声（伪 callee）**：`0x1E856` 尾 `jmp 0x1317D`、`0x1E292` 尾 `jmp 0x1317D`、`0x1D79C` 尾 `jmp 0x114FF` 均落在他函数体内＝普通 `return`；`function-map` 对后两者误列 `sub_1300D`/`sub_1145A`。
- **`sub_1E292` 的 IDA 名仍被调用**：`scene_core.c`（已还原）以 `sub_1E292(sel)` 调用，故 `map_levelup.h` 保留 `#define sub_1E292 map_levelup_run`（同 `map_units.h` 的 `sub_1DB65/sub_22253`）；`fd2_funcs.h` 冻结不改。
- **原始表访问器**：`0x4E821`（职业表 base 0x620A1/stride 11）、`0x4E7F2`（增益表 base 0x626B3/stride 12）按 playbook 改用 `tbl_ptr(&fd2_data2[…],…)`；掩码表直接用 `&fd2_data1[0x2024]`（globals 只暴露标量 `unk_52024/28`）。
- **`__CFSHL__`**：`0x1D79C` 的 `gain/8`/`gain%8` 在 Hex-Rays 里被渲染成 `__CFSHL__`；调用点传入的是 `(u8)` 值（0..255），C 侧普通 `/`、`%`。
- **返回值**：`0x1E856` 尾声 eax 残留、无调用者读取 → `void`；`0x1E292` 声明 `int` 返回 0（与冻结原型一致）；`0x1EB05` 返回 `&dword_53A30`。
- **chkstk**：四者皆 `push size; call 0x3702F`（不分配帧），丢弃；CRT `free`→`guest_free`。
- 无对拍器，标“已还原未验证”；caller `sub_1548E`/`sub_117E7` 的其余部分仍待还原。

### 43) `funcs_1541F` 条目 20/21 + 共享体（本轮，3 函数）→ `src/game/map_action.c/.h`（追加）

按 playbook 的闭包筛选：closure-ready 游戏函数按 usage 排序后，u4 以上只剩 CRT/库包装（跳过），
高用量小簇中 `0x22A85`(u3,35B) 的闭包最小（`{0x22A85,0x22AA8}`=113B，3 单位 u3）；其兄弟
`0x22BC6`(u3,27B) 是同一尾部的另一个入口，二者合为连贯簇（真闭包＝`{0x22A85,0x22AA8,0x22BC6}`
=140B，callee 全在册：`rec_sub_table5` 0x1CA89 / `item_apply` 0x22AF6 / `map_draw_status_popups`
0x1DF58）。二者均属 `funcs_1541F` 分派表条目 20/21（`sub_15311`/`sub_1CFF0` 调用），归入
`map_action` 模块（该头文档范围 0x21082..0x22C04）。

| 原址 | 新名 | 模块 | 要点 |
|---|---|---|---|
| `0x22AA8` | `map_action_charge_apply` | map_action.c | `dword_53EC4=0; rec_sub_table5(a1,index); item_apply(a1,index,count,list,field); if(dword_53EC4) map_draw_status_popups();` 尾 `jne 0x1DF58` 为共享尾声，非独立调用。 |
| `0x22A85` | `map_action_entry20` | map_action.c | funcs_1541F 条目 20：`push 37/a3/a2/20` → 落穿 0x22A9B → `map_action_charge_apply(a1, 20, count, list, 37)`。 |
| `0x22BC6` | `map_action_entry21` | map_action.c | funcs_1541F 条目 21：`push 38/a3/a2/21` → `jmp 0x22A9B` → `map_action_charge_apply(a1, 21, count, list, 38)`。 |

**陷阱 / 伪代码伪影**：
- **共享体内跳（Hex-Rays `JUMPOUT`）**：`0x22BC6` 的伪代码只有 `JUMPOUT(0x22A9B)`；离线反汇编显示它压 `38/a3/a2/21` 后 `jmp 0x22A9B`，而 `0x22A85` 是压 `37/a3/a2/20` 后**落穿**同一地址，之后 0x22A9B 再 `push [esp+0x14](a1)` 并 `call 0x22AA8`。故二入口参数映射 `(a1,index,count,list,field)` 分别为 `(·,20,·,·,37)`/`(·,21,·,·,38)`（按 push 深度逐条核算，与 `rec_sub_table5`/`item_apply` 的解压一致）。
- **冻结原型**：`fd2_funcs.h` 仍声明 `void sub_22A85(...)`/`void sub_22AA8(...)`/`int sub_22BC6(...)`；已还原模块中无调用点，故不追加 `#define` 别名（playbook：只用语义名）。
- **chkstk / 返回**：三者首部 `push size; call 0x3702F` 丢弃；结论返回值仅供表分派、无调用者读取 → `int` 返回 0（与同模块 `map_action_clear_flag` 一致）。
- 无对拍器，标“已还原未验证”；`sub_15311`/`sub_1CFF0` 及其余 `funcs_1541F` 条目仍待还原。

### 44) `0x31266` → `fx_play`（本轮，1 函数）→ `src/game/fx.c/.h`（追加）

按 playbook 闭包筛选：closure-ready 游戏头名（u1 小簇）里 `0x31266` 与 fx 分派表直接相关
（`fx.h` 开头即注明它是两个 `call funcs_30469[reg*4]` 站点之一），其“伪 callee”（表条目
`sub_2B996..2CE1A` 与尾跳目标 `sub_2FE14`）全部已在册，真依赖＝本函数 + 已还原的
`gfx_copy_rows`/`res_blit`/`svc_wait_ticks`。

| 原址 | 新名 | 模块 | 要点 |
|---|---|---|---|
| `0x31266` | `fx_play` | fx.c | 以 `work+18880`（0x49C0）为 640 跨距工作条，逐帧把 `background` 拷入 → 用 `funcs_30469[sel]` 选择符 4/5 各渲染一次 → 两条 `res_blit`（`layer0` 用 `layer0[0]-1` 索引、`layer1`/`layer2` 按 `35*i*step` 行偏移）→ 发布到 `0xA0000` → `svc_wait_ticks(1)`；首环 i=1..4、次环 j=4..0 共 9 帧。 |

**ABI / 陷阱**：
- **ABI＝cdecl 8 参**，由唯一直调点 `sub_2FF01` 的 `0x30870..0x3089d`（8 次 push + `add esp,0x20`）核实；
  `sel==7||sel==3` 时背景源在 `scratch` 与 `scratch-640` 间交替（字节局部 `flip`）。
- **共享尾声**：尾部 `jmp 0x2FE0C` 落在 `sub_2FE14` 体内，是标准收尾（`add esp,0xc; pop×4; ret`），
  返回值无人读 → C 声明 `void`。
- **step 符号**：记录 `dword_53A45[80*rec+6]` 为 0 取 +1 否则 −1（机器默认 −1 再按零置 1）。
- **表条目为伪 callee**：`function-map` 把 `funcs_30469[sel]` 展开成 10 个 handler + `sub_2FE14`，
  实为间接表调用与共享尾声，真依赖只有 `gfx_copy_rows`/`res_blit`/`svc_wait_ticks`。
- 无对拍器，标“已还原未验证”；其唯一 caller `sub_2FF01`(u2，3876B) 仍待还原。

### 验证与台账

- `tools/check_compile.sh --full`：双 profile 0 错误、`-Werror` 双 profile 全绿、伞头自检 OK。
- `function-map.csv`：待还原 **145 → 144**（本轮 §44 −1），已还原未验证 **178 → 179**（本轮 +1）。（计数取 §43 后实测基线。）

### P1 余量（下一批候选，按 `function-map.csv` usage 排序）

本轮 §13 已拉出 `0x12EAA/1300D/13185/13315/13488/13A44`（它们是 `0x14B78` 与 `0x18890` 的直接 callee），
故 `0x14B78`(u8) 的真闭包已缩小，但仍含 `146D1/14237/1DEBE/15055/15311/1548E/1567E/1598A…`，>8 函数，暂跳。
`0x1C75E`(u7，链 `1C81F`→`14121`→`14B78`，同上跳过)、`0x14EF0/2E9A8`(u6)。
本轮 §17 已拉出 `0x1F42D` 及其两个 caller `0x1F1CC`/`0x1F30A` 与私有 helper `0x4EB59`；
本轮 §18 已拉出 `0x2921A` 及其同簇的 `0x26A0D/28B41/26A7A/26B91`；
本轮 §19 已拉出 `0x1B6B7`/`0x149F8`（`map_records`）与 `0x22866`（`map_action`）；
本轮 §20 已拉出 `0x175A9`/`0x17643`/`0x1741C`/`0x176B4`（`map_cursor`），故其 5 个驱动器
（`0x16F55/0x1728C/0x18D8C/0x19DF7/0x1BBDC`）只剩 `0x177FC`+`0x17898` 与 `0x16F55` 自身；
调用量排序下，`function-map.csv` 的闭包就绪头名是 `0x3645D`(u4)，但它是 CRT `read` 包装（尾调 `sub_3CF50`=`lseek`，
库区不投资源，同 module-map §2）；`0x2E95B`(u3) 同样因需 CRT `fread`（`sub_373CA`）而暂跳。
本轮 §21 已把同属 `sub_2CF30` 的 `0x2F4D4/30E25/30E9D`(u3/u2) 作为一簇拉出。
本轮 §22 已拉出当前 closure-ready 头名四个 u3：`0x1E739`/`0x1F558`/`0x2282F`/`0x265EC`。
本轮 §23 已拉出 `0x1E7F6`(u4) 及其 caller `0x1E611`（`map_status`），故地图状态叠加族只剩
`0x1E856`(u2，仍待 `0x1E98C`/`0x1ECC7`)。
本轮 §24 已拉出 `0x1C142`/`0x1C1C3`（`box_popup` 物品选择助手，同 caller `1BFFE`/`2872B`）。
本轮 §25 已拉出 §21 `figure_view` 家族的另两个同胞 `0x2F631`/`0x2FE14`(u2)（驱动器 `0x2EBE1`/`0x2FB2C`）。
本轮 §26 已拉出闭包就绪头名 `0x1A7BD`/`0x1A7F1`(u2，`0x1A30B` 的音效库加载/释放对)。
本轮 §27 已拉出闭包就绪头名 `0x2B439`/`0x2B843`(u2，同 caller `sub_2AF28` 的队伍名册助手)。
本轮 §28 已拉出闭包就绪头名 `0x1CB94`/`0x1CD17`(u2，`map.c` 的 0x1C2DA/0x1C4CC 同族；地图视口记录精灵族现只剩调用者 `0x1CAC7` 与格坐标 `0x1EC2A`)。
本轮 §29 已拉出闭包就绪头名 `0x1BFFE`(u2，物品盒编排环，`box_popup` 家族闭合；其 caller `0x1BBDC`/`0x28EFE` 仍待还原)。
本轮 §30 已拉出 `0x10010`(u2，FD2.SAV 存档加载/章节开场链；真闭包＝自身)，故当前闭包就绪
游戏头名新增 `0x1399C`(u2，此前被 `0x10010` 挡住的闭包)，另有 `0x1EC2A`(u2，格坐标/标记锚点)
/`0x272D0`/`0x2A0C2`/`0x2A694`(u2，名册行绘制；同一 `sub_2670E`/`2872B`… 商店屏家族)
/`0x1CAC7`(u2，§28 `map_draw_record_layer` 的驱动器)；
本轮 §31 已把 §30 列出的商店屏行绘制三件套 `0x272D0`/`0x2A0C2`/`0x2A694`(u2) 整体拉出
（`src/game/shop_rows.c`，真闭包＝自身），闭包就绪游戏头名转为 `0x1399C`(u2)、`0x1EC2A`(u2)、
`0x1CAC7`(u2)，及商店屏族里仍被上游挡住的 `0x275E6`/`0x27738`/`0x2A29D`/`0x2A857`；
本轮 §32 已把这四个行绘制器的调用者（整屏“暂存 + 键循环”）`0x275E6`/`0x27738`/`0x2A29D`/`0x2A857`(u3/u3/u1/u1)
作为同族整体拉出（`src/game/shop_screen.c`，真闭包＝自身），故闭包就绪游戏头名转为
`0x1399C`(u2)、`0x1EC2A`(u2)、`0x1CAC7`(u2)；商店屏族的下一层调用者 `0x2872B`/`0x28CBD`/`0x28F65`/
`0x2A43E`/`0x2AA00` 仍待还原；
本轮 §33 已把这个头名三元组 `0x1399C`/`0x1CAC7`/`0x1EC2A`(u2) 整体拉出（`map_shape` 新建 + `map_sprites`/`map_status` 追加），
闭包就绪游戏头名转为 `0x15880`(u1)、`0x15DA2`(u1)，其后同层 u1 的小簇（同 caller）仍以
`0x22470`/`0x22547`/`0x22656`(sub_22253，拉出后 u5 的 `sub_22253` 即闭包就绪) 为最优候选；
CRT/库包装 `0x3645D`(read)/`0x2E95B`(fread)/`0x365DA`(int386) 及 `0x36E3D..0x36FAC` 小库函数按既定策略跳过；
另有多组同 caller 的小簇（均 u1）：`0x1C9DD`(sub_20C6F)、`0x22470`/`0x22547`/`0x22656`(sub_22253)、
`0x2AC7D`/`0x2AE0E`/`0x2FB2C`(sub_2AA00)、`0x1E98C`/`0x1ECC7`(sub_1E856)、`0x1B1E7`(sub_19DF7)。
本轮 §35 取同 caller 小簇的最优候选，把 `0x22470`/`0x22547`/`0x22656`(u1，`sub_22253` 的三个 callee)
整体拉出（`map_units.c` 追加），故 `sub_22253` 现已闭包就绪；
本轮 §36 继而把 `0x22253` 的唯一在册 caller `0x2218A`(u4) 拉出（`map_units` 追加），单位移动族自顶闭合；
本轮 §37 再把 closure-ready 头名 `0x15880`/`0x15DA2`(u1) 连同其 driver `0x1567E`(u2)/`0x15B77`(u1) 整体拉出
（新建 `map_ai`），故该模块只剩行动目标挑选器 `0x1598A`(u3) 与决策驱动 `0x14EF0`(u6)；
本轮 §38 继而把该头名 `0x1598A`(u3) 拉出（`map_ai` 追加），故 `map_ai` 只剩决策驱动 `0x14EF0`(u6)；
游戏头名转为 `0x1c8ed`/`0x1c9dd`(u1，`sub_1C916`/`sub_4EBE3` 小簇)；
本轮 §39 继而把这个头名对 `0x1C8ED`/`0x1C9DD` 拉出（`item_use` 追加），治疗族闭合。
下一游戏头名转为 `0x2e95b`(u3，仍需 CRT `fread` `sub_373CA`，暂跳)，其后为同属 `sub_20C6F`/`sub_21B18`
等地图驱动器及 u1 小簇（`0x1E98C`/`0x1ECC7`、`0x1B1E7`），均受各自 caller 闭包阻挡。
本轮 §40 继而把同 caller（`sub_2AA00`）的三个 u1 callee `0x2AC7D`/`0x2AE0E`/`0x2FB2C` 拉出（新建 `item_menu`），
故 `sub_2AA00` 已闭包就绪；下一游戏头名再回落至同 caller 对 `0x1E98C`/`0x1ECC7`（`sub_1E856`）与 u1 小簇
`0x1D6C8`/`0x1D79C`/`0x1DEBE`/`0x1EB05`/`0x22AA8`/`0x24BDE`/`0x28632`/`0x2A07A`/`0x2F7B6` 等（分别闭簇其 caller）。
本轮 §41 继而把这个同 caller（`sub_1E856`）对 `0x1E98C`/`0x1ECC7` 拉出（新建 `map_attack`），故 `sub_1E856` 已闭包就绪；
下一游戏头名回落至 u1 小簇 `0x1D6C8`/`0x1D79C`/`0x1DEBE`/`0x1EB05`/`0x22AA8`/`0x24BDE`/`0x28632`/`0x2A07A`/`0x2F7B6` 等
（分别闭簇其 caller），其间 `0x1B1E7`(u1) 因内层读未初始化暂跳。
本轮 §42 把这个家族里仍 closure-ready 的 `0x1D79C`/`0x1EB05`/`0x1E856`（连同需它的 `0x1E292`）
整体拉出（`map_attack`/`map_status` 追加 + 新建 `map_levelup`），故 `sub_1548E` 已闭包就绪；下一游戏头名
回落至 u1 小簇 `0x1D6C8`(sub_1CFF0)/`0x1DEBE`(sub_14237)/`0x22AA8`(sub_22A85)/`0x24BDE`(sub_24754)/
`0x28632`(sub_2825B)/`0x2A07A`(sub_2A43E)/`0x2F7B6`(sub_2EBE1)/`0x31266`(sub_2FF01) 等（分别闭簇其 caller）。
本轮 §43 把该列表里的 `0x22A85`/`0x22BC6`（`funcs_1541F` 条目 20/21，u3）连同其共享体 `0x22AA8`
整体拉出（`map_action.c` 追加），故同族 `funcs_1541F` 只剩 `0x22BE1`/`0x22CBF`/`0x22E41`（均需 `sub_22D1B`
→ `sub_1C81F`，闭包 >8 暂跳）与表中其余大片条目；下一游戏头名仍为 u1 小簇
`0x1D6C8`(sub_1CFF0)/`0x1DEBE`(sub_14237)/`0x24BDE`(sub_24754)/`0x28632`(sub_2825B)/
`0x2A07A`(sub_2A43E)/`0x2F7B6`(sub_2EBE1)/`0x31266`(sub_2FF01) 等。

**台账发现（已解决）**：§26 曾记 `function-map.csv` 把 `0x2BF83`(u2) 列为待还原，而它其实已是
`src/game/fx.c` 的 `fx_advance`（`fx.h` 已声明），只是漏登 `restored_manifest.txt`；已由台账提交
`4d27e6e` 补入 `0x2BF83 sub_2BF83 game/fx.h` 并重跑生成器，该项现转为“已还原未验证”。
（`0x344B4` 已由 §16 连同其上游 `0x34366`/`0x33FAF` 整簇拉出；`0x2DF01/1FF79/1F73F` 已由 §14 拉出。）
策略不变：**按 callee 簇整体拉出**，并先剔除共享尾声造成的伪 callee（可用 Ghidra 结构反编译辅助）。
（`0x2921A`＝地点菜单退场缩放（上批 §11 遗留）；`0x1B1E7`(地图队伍面板编排者，6 个 callee 已在 §12 还原）
现已闭包就绪，惟其内层读未初始化的 ebp，需先定性 tick 基线。）

---

## 0-C. 下一步（按 module-map P0 执行）

1. **P0 主状态机链**：`sub_25EBB`/`sub_117E7`/`sub_26152`/`sub_22E5C` + `funcs_25E23` 剩余 ~19 项
   + `funcs_25E3A` 剩余 ~22 项（以 scene/menu 已验证资产为模板，整链拉出，不逐函数）。
2. **引入 309 已验证资产**：port/src/game → port3/src/game（适配 fd2_* 宏层，对拍器保留）。
3. 解释器簇 0x19953/0x1AA1D/0x197E5/0x1DB65（P0 尾）→ P1 高用量长尾按 callee 聚类批量。
4. IDA 通道重建后：① 98 个 `interior-ptr?` 指令边界核验；② `dword_53A45` 写入者锁定；
   ③ `funcs_1197B/1541F` 覆盖联查。
5. 然后进入第六阶段：固定场景差分对拍设计（RNG/tick/输入固定，参照 DEBUG-MANUAL 资产）。

---

## 1. 静态阶段已解决的硬骨头（保留存查）

## 0. 结论一览

| 问题 | 状态 | 证据 |
|---|---|---|
| 全量伪代码提取 | ✅ **1366/1369**（99.8%） | `analysis/decomp/`；3 个失败有反汇编兜底 |
| 函数原型（参数个数） | ✅ **1114 个从调用点证据推导**，686 个已回写 IDB | `analysis/arity.json`、`prototype_heal_log.json` |
| 数据符号覆盖 | ✅ **694/694 = 100%**（含 188 个合成 `unk_*`） | `src/include/fd2_globals.h` |
| 分派表提取 | ✅ 7 张表逐槽解析 | `analysis/dispatch_tables.csv` |
| 字符串表 | ✅ 325 条（utf-8 282 / big5 43） | `analysis/strings.csv` |
| 段镜像 | ✅ 三段原始字节 | `analysis/seg_*.bin` → `src/fd2_image.c`(生成) |
| 数据段代码指针修复清单 | ✅ 119 槽 | `src/fd2_relocate.c`(生成) |
| 还原 C 模块 | ✅ **范例 2 个（res.c 资源加载器 + boot.c/main 骨架）** | `src/game/` |

## 1. 本轮解决的硬骨头（第二阶段前必须知道的坑）

1. **原型误判连锁**（最大卡点）：Watcom 栈探针 `push size; call 0x3702F`
   （=`xchg eax,[esp+size]; call __CHKSTK; retn 4`）让 IDA 误推 ABI ⇒ 8 个被调函数
   被标 `__fastcall(26 参数)` ⇒ 22 个调用者 `MERR_BADCALL`；另有 686 个函数参数个数
   与真实压栈不一致（**伪代码静默丢实参**，比报错更隐蔽）。修复流水线：
   6556 个 call 站点取证（`add esp,4n` 权威 + push 计数佐证 + 间接调用经表展开）→
   批量回写 → 自愈迭代（29→3 失败）。**关键教训：`int(...)` 可变参数原型也会触发
   BADCALL，必须给精确个数。**
2. **误造函数 `sub_36B00`**：`push offset sub_36B00` 其实是 `malloc(0x36B00)` /
   `memset(...,0,0x36B00)` 的**立即数**被当成交叉引用。真实函数起点 = `0x367D1`
   （已重建）。**教训：看到 `push offset sub_XXXX` 先看上下文是不是 size 参数。**
3. **死代码发现**：`0x367D1`（重建后）无任何调用/跳转/表引用/落穿 ⇒ 静态不可达，
   反编译亦失败（`call analysis failed`，微码层 SP 轨迹正常、ctree 参数转换失败，
   根因未明）——留 `analysis/disasm_fallback/` 待第二阶段或手工翻译。
4. **AIL 内部回调 2 个反编译失败**（`sub_459F5` positive-sp / `sub_45A20`
   call-analysis-failed）：均为 `AIL_register_{event,timbre}_callback` 注册的回调，
   属 AIL 库边界（范围外），有反汇编兜底。
5. **`&loc_2567F + 1` 陷阱**：奇数地址的 size 立即数被 IDA 渲染成"地址+1"
   （=`malloc(0x25680)`）。语料中共 9 个 `loc_*` 引用需在手工还原时逐个核实是
   **size 立即数**还是**共享尾声 JUMPOUT**（本轮已定性 2 个：
   `loc_1005E`=exit(1) 尾声、`loc_16F04`=公共 epilogue `add esp,0Ch; pop×3; retn`，
   10 处跳入）。
6. **printf 家族是可变参数**：arity 多数票会低估（printf 实为每站点 1..3 个参数）——
   raw 伪代码里 printf 调用的实参可能被截断；手工还原时按站点反汇编补齐。
   涉及 `printf(0x37119)/sprintf(0x37B29)/fprintf(0x3F46B)`（CRT 映射，影响可控）。
7. **data-in-code**：cseg01 里有字符串（`a123456789abcde` @0x46DF5）与
   `jpt_/funcs_` 表（CRT/AIL 区 17 张），段镜像按只读处理。

## 2. 还原工程现状（src/）

| 产物 | 状态 |
|---|---|
| `include/fd2_globals.h` | ✅ 生成：1041 宏（用法扫描决定标量/指针视图），语料 100% 覆盖 |
| `include/fd2_funcs.h` | ✅ 生成：1369 原型；**145 个寄存器 ABI（`@<reg>`/`__spoils`）标 MANUAL** |
| `fd2_image.c` | ✅ 生成：三段镜像 + `fd2_data_init()` + `fd2_jumpout()` 桩 |
| `fd2_relocate.c` | ✅ 生成：119 个数据段代码指针 → 真实函数/代码镜像 |
| `game/res.c`+`res.h`、`game/boot.c`+`boot.h` | ✅ 手工还原范例（0x111BA 资源加载器、0x25BF4 main 骨架），标识符静态自检 PASS |
| `lib/ail.h` | ✅ AIL 边界：游戏实际调用的 6 个入口真签名（其余 10 个随 bgm/sfx 还原时补） |
| `restored_manifest.txt` | ✅ 已还原清单（fd2_funcs.h 自动跳过，避免双重声明） |
| `game/*.c` 其余 | ⬜ 未开始（见 §3 顺序） |
| `lib/` | ⬜ CRT→libc 映射、AIL 边界桩（第二阶段细化） |

**内存模型**（README §4）：三段镜像数组 + 地址偏移宏；函数代码里不出现绝对地址
（只有生成头里一处）。32 位指针宽度假设已标注（去 guest 化属后续阶段）。

## 3. 下一轮计划（手工还原顺序，依据 RE_MAP 优先级）

1. **骨架**：`main`(0x25BF4) → 启动链（`start`/CRT 入口按需）→ 主状态机
   `sub_25EBB`(0x25EBB) + `funcs_25E23/25E3A` 两张表的 30+10 个 handler。
2. **资源层**：`res_load`(✅ 本轮) → `sub_10652/1088D`(FDOTHER/FDICON 解包) →
   `sub_10B4E` 链（unit_load）。
3. **脚本/事件**：`sub_15F84`(VM) → `funcs_1199C` 91 项 → `funcs_1197B` 120 项
   → `funcs_1541F` 28 项（表清单在 `analysis/dispatch_tables.csv`）。
4. **图形工具库**：0x4DED4..0x4F000 全区段（RLE/blit/sprite24/path/tables——
   前人已对拍验证过语义，可直接对照 `port/src/game/` 的既有翻译）。
5. **显示/对话/地图/存档** 等大模块（RE_MAP §4 核心档案逐条收）。
6. 归档遗留：死代码 `0x367D1` 手工翻；AIL/CRT 边界文档化。

## 4. 遗留未知（明确记录）

- `0x367D1` 反编译失败根因（ctree call 转换）+ 静态不可达判定；
- 145 个寄存器 ABI 原型（CRT/AIL/数学库内部，按范围决策多数不必还原）；
- 255 个函数无直接调用点（纯表分派/回调），arity 待按表站点证据补；
- big5 的 43 条"字符串"多为 data-in-code，语义未逐条核。
