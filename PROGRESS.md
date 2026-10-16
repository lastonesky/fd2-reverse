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

### 45) 地点菜单/商店交互屏簇（本轮，4 函数）→ `src/game/shop_screen.c/.h`（追加）

闭包筛选后，closure-ready 游戏函数只剩同 caller 的 u1 小簇。本轮取地点/商店菜单族里
最连贯的一组：`0x28CBD`/`0x28EFE`（同为待还原驱动器 `sub_279BC` 的 callee，且 `0x28EFE` 的
唯一待还原 callee 就是 `0x28CBD`）＋ `0x2A43E`（待还原驱动器 `sub_29DAA`/`sub_2A43E` 链，
其唯一待还原 callee 是 `0x2A07A`）及其私有助手 `0x2A07A`。四者同属“地点/商店菜单的交互列表/
道具屏”（`shop_screen.h` 既定范围），真闭包＝自身（callee 全在册：`menu_list_select`、
`shop_items_paint/run`、`shop_party_run`、`place_transition`、`place_money_gain/spend`、
`box_run`、`res_load`、`rec_field_byte`、`rec_slot_remove`、`unit_metrics`、`rec_flag`、
`map_window_run`、`rle2_blit`、`dlg_draw_number`、`bgm_play` 及 `tbl_ptr(0x602AD,23)`）。
拉出后 `0x28EFE`（u1）与 `0x2A43E`（u1）各自整函数闭合，`sub_279BC` 待还原 callee 由 4 降为 2。

| 原址 | 新名 | 模块 | 要点 |
|---|---|---|---|
| `0x28CBD` | `shop_sell_run` | shop_screen.c | 循环：`dword_53F5E=dword_53BFB`；`menu_list_select()`（-1 即返回）；取 `dword_53C57` 记录，收集 `+10/11` 八个槽中状态字节<0x80 的道具号；无道具则 `dword_53AD9=rec[7]+1` + `msg_open_portrait` + `vm_run(v10[place],0xA94CC,…)` + `dlg_blit_dato` + `dlg_wait_key(1)` + `msg_close_portrait`；有道具则 `shop_items_paint/run`，确认后 `rec_field_byte` 查道具、`dword_53AE1=(3*item[19])>>2`、`vm_run(v9[place],…)`，若 `map_window_run()!=-1 && dword_53C57==0` 则 `place_transition`+`place_money_gain`+`rec_slot_remove`+`unit_metrics`（即卖出）。 |
| `0x28EFE` | `shop_box_run` | shop_screen.c | 循环：`dword_53F5E=dword_53BFB`；`menu_list_select()`（-1 返回）；保存 `dword_53C67`→`box_run(dword_53C57)`→还原 `dword_53C67`；`dword_53A85=res_load(aDatoDat,…)`。 |
| `0x2A43E` | `shop_buy_run` | shop_screen.c | 循环：`shop_usable_items` 过滤；无可用记录走 588 消息并返回；否则 589 消息 + `shop_party_run` 选人，`dword_53AE1=word_52397[rec[32]]*rec[33]`，590 消息后按 `map_window_run`/`dword_53C57` 与余款（有符号 `money<price`）分支：不足走 504 消息；买成则 `place_money_spend`、`rec[5]=0`、`rec[0x40]=rec[0x42]`、重绘金额条、`bgm_play(17,1)`+`place_transition`+`bgm_play(11,1)`。 |
| `0x2A07A` | `shop_usable_items` | shop_screen.c | 扫描 `0..dword_53BFB`，把 `rec_flag(i)==1` 的 i 写入 `dst`，返回计数（机器用字节计数器）。 |

**陷阱 / 伪代码伪影**：
- **共享尾声**：`0x28CBD` 尾部 `0x28ef7..0x28efd`＝`add esp,0x20; pop edi; pop esi; pop ebx; ret`；
  `0x28EFE` 只压 `ebx`，其 `je 0x28efc`/`jmp 0x28efc` 落在该尾部的 `pop ebx; ret`（跳过 edi/esi 弹出）。两者皆普通 `return`。
- **返回值**：两函数只能经 `menu_list_select` 取消路径退出 → `return -1`（机器 eax=ebx=-1）；`0x2A43E` 声明 `int` 返回 0；调用者均忽略。
- **`dlg_blit_dato` ABI**：机器只压 idx（这里恒 0），按既有约定（PROGRESS 0-J）补传 `dword_53A85,dword_53C67`。
- **v9/v10 表**：`0x28CBD` 从 `0x52458`/`0x52464` 各拷 12 字节做 `int16_t[6]`，两分支分别用 `v10[place]`（无道具）/`v9[place]`（售出）作 `vm_run` 的 sub；globals 仅暴露字节视图 `unk_5245x`，故用 `memcpy(&fd2_data1[0x2458],…)`。
- **有符号比较**：`0x2A43E` 的余款判定机器是 `cmp; jge`（有符号），C 侧 `(int32_t)dword_53BF3 < (int32_t)dword_53AE1`；`0x2A07A` 的循环上界 `cmp ebx,ds:0x53bfb; jge` 亦有符号，用 `(int)dword_53BFB`。
- **原型骗过反编译器**：`fd2_funcs.h` 把 `0x2A43E` 记为 `int()`、`0x2A07A` 记为 `int(int)`；后者实为一参指针（写回缓冲），C 侧定 `int shop_usable_items(uint8_t *)`。
- **伪 callee**：`0x28CBD` 尾 `je 0x28d80`/`jmp 0x28d85` 是自身循环回边，非外部函数。
- **`0x2A07A` 私有助手**：唯一 caller `0x2A43E`，按 `map_levelup_mark_gain` 先例在头文件声明（非 static）。
- 无对拍器，标“已还原未验证”；`0x279BC`/`0x29DAA`/`0x2872B`/`0x28F65`/`0x29300`/`0x29620`/`0x2AA00` 等上层仍待还原。

### 46) AI 到达目标挑选器 + 共享地图助手（本轮，3 函数）→ `src/game/map_ai.c/.h`（追加）

§45 后 closure-ready 游戏头名只剩 u1 小簇；本轮取其中**同族最连贯**的一组：AI 决策驱动
`sub_14EF0`（仍待还原）并调的三个评分器 `0x14237`/`0x1598A`/`0x1567E` 中的第三个 `0x14237`
（前两者的模块即 `map_ai`），及其两个共享助手 `0x146D1`/`0x1DEBE`。真闭包＝自身
（callee 全在册：`rec_slot_find`(0x1B83D)、`rec_field_byte`(0x1B722)、`rec_skip`(0x1F183)、
`path_mark`(0x4E390)、`unit_mark_nearby`(0x145CD)、`map_reveal_reachable`(0x14818)、
`map_cell_info`(0x12E38)、`ev5_14B16`(0x14B16)、`util_fix_records`(0x4DF4C)，及
`tbl_ptr(0x602AD,23)`/`tbl_ptr(0x61646,20)`）；合计 1185 B，3 函数，同属 AI 目标评分族。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x14237` | `map_ai_pick_reach_target` | 取 actor 首槽道具（`rec_slot_find(actor,0)`→`rec_field_byte`→`item_entry` 得 range=item[12]/radius=item[11]），`unit_mark_nearby(mode)` 后 `path_mark` 铺行动可达区，`0x146D1` 排除占用格；对每个剩余可达格（`ev5_14B16` 收集）用道具 range/radius 扫目标记录并打分，最优存 `dword_53C43/47`（格）/`dword_53C4B`（记录）/`dword_53C4F`（分）。`mode` 同时选排除/过滤分支。 |
| `0x146D1` | `map_exclude_record_cells` | 把 `+6` 匹配 `mode`（0: +6==0，否则 +6!=0）的记录所在格（`dword_53A51 + 7 + 4*(y*W+x)`）置 0xFF，使 `ev5_14B16`（跳过 0xFF 格）剔除其格；跳过 actor 与 `+5&1` 者。 |
| `0x1DEBE` | `map_ai_target_adjacent` | 记录 `target` 与格 (x,y) 曼哈顿相邻（`|dx|+|dy|==1`）、`+38==0`、且其首槽道具 effect `+11<=1` 时返回 1，否则 -1。 |

**陷阱 / 伪代码伪影**：
- **伪 callee（0x146D1→0x14237）**：`function-map.csv` 把 `0x146D1` 记为调用 `sub_14237`，实为尾跳 `JUMPOUT(0x145C9)`——落入 `sub_14237` 自身的共享尾声（`0x145c3..0x145cc`＝`xor eax,eax; add esp,0x54; pop ebp; pop edi; pop esi; pop ebx; ret`，供 `0x145c9` 处多条跳入），即普通 `return`，非跨函数调用。
- **反编译器伪影**：`sub_12E38` 写 8 字节 cell 缓冲，Hex-Rays 只声明 `_BYTE v18[5]` 并另立 `v19=cell[5]`；C 侧用 `uint8_t cell[8]` 读 `cell[5]`。
- **缩放表**：`dword_51A12`/`dword_51A2A`（dseg02、`const int32_t*`）按 `cell[5]` 取百分比；乘 `imul` 与 `idiv 100` 按有符号 32 位还原（`v*table/100`）。
- **堆**：`0x3706E`/`0x3776E` → `guest_malloc`/`guest_free`；Watcom 栈探针（`push size; call 0x3702F`）剔除。
- 无对拍器，标“已还原未验证”。拉出后 `0x14B78`(u8) 的真闭包去除 `146D1/14237/1DEBE` 三项，但仍含 `15055/15311/1548E/1567E/1598A…` >8 函数；下一游戏头名仍为 u1 小簇（`0x1D6C8`(sub_1CFF0)/`0x24BDE`(sub_24754)/`0x28632`(sub_2825B)/`0x2AA00`(sub_29DAA)/`0x2F7B6`(sub_2EBE1) 等）。

### 47) actor “approach cell” core + two callers（本轮，3 函数）→ `src/game/map_move.c/.h`（新建）

§46 后 closure-ready 游戏头名升为 `0x14B78`(u8)；离线反汇编证实其真闭包＝**自身**（8 个实调 callee
全在册：`path_mark`(0x4E390)、`path_find`(0x4E4F6)、`ev5_14B16`(0x14B16)、`util_fix_records`(0x4DF4C)、
`unit_mark_nearby`(0x145CD)、`map_exclude_record_cells`(0x146D1)、`map_scroll_path`(0x13488)、
`rec_skip`(0x1F183)，及 `tbl_ptr(0x61646,20)`）。连同其两个 closure-ready caller
`0x14121`(u3)/`0x13E9C`(u1) 整体拉出，合计 1478 B。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x14B78` | `map_move_toward` | 朝格 (tx,ty) 走：先 `path_find(mode0)` 直连；不通则 `path_find(mode1,start_mp=28)` 求带转向偏好的路径，`path_mark` 标记后沿方向表从记录格行走、取最后一个非 0xFF 格为新目标；再 `path_mark`→`map_exclude_record_cells`→`ev5_14B16` 收集可达格，按曼哈顿距离（并列取 `|dx|-|dy|` 最小）挑最近格，最后 `path_find(mode0)`→`map_scroll_path` 提交。返回是否提交。 |
| `0x14121` | `map_move_to_marked` | `path_find(mode2,target 0,0,start_mp=28)` 求可达的 0x40 标记格（出参 2 字节＝目标格）；与记录格相同则空转，否则 `dlg_portrait_glide` 后 `map_move_toward`。`dword_51A83` 0/1 包裹。 |
| `0x13E9C` | `map_move_to_nearest` | 遍历 `dword_53BEB` 条记录，按 `+6`（mode==0 取 `!=0`，否则 `==0`）过滤，取曼哈顿最近者的格；同上 glide + `map_move_toward`。 |

**陷阱 / 伪代码伪影**：
- **闭包修正**：§46 曾记 `0x14B78` 的真闭包含 `15055/15311/1548E/1567E/1598A`；离线反汇编/伪代码证实其**实调**仅上表 8 项，其余为 `0x14B78` 的既有 caller，非闭包。
- **反编译器实参错位**：`sub_14B78` 的 Hex-Rays 渲染把 `path_find/path_mark` 的 `(x,y)` 实参错标成 `(a4,v22)`，并把方向扫描起点错标成 `(a4,v22)`；离线反汇编定为记录格 `(rec[0],rec[1])`、`path_find(...,rec_x,rec_y,...)`、`path_mark(...,rec_x,rec_y,...)`，本模块按反汇编。
- **`0x14121` 出参**：`mode2` 的 `path_find` 只在命中 `cell[-1]&0x40` 时写 2 字节目标格（`path.c` `mark_target`），故栈上 2 字节缓冲足矣（原机帧一致）。
- 共享尾声：`0x14B78` 尾 `jmp 0x11011`、`0x14121` 尾 `jmp 0x10B46` 属外部纯 `return` 尾声（伪 callee）。堆 `0x3706E/0x3776E` → `guest_malloc/guest_free`。
- 无对拍器，标“已还原未验证”。拉出后下一游戏头名回落至 u1 小簇
  `0x1D6C8`(sub_1CFF0)/`0x24BDE`(sub_24754)/`0x28632`(sub_2825B)/`0x2AA00`(sub_29DAA)/`0x2F7B6`(sub_2EBE1) 等。

### 48) 记录伤害/动作效果 leaf 簇（本轮，2 函数）→ `src/game/item_use.c/.h`（追加）

closure-ready 游戏头名 `0x1C81F`(u3) 及其唯一待还原 callee/caller 交叉项：`0x1C75E`(u7)
是它的直接 caller（`sub_20C6F` 行动路由与 `sub_2111A`/`sub_21227`/`0x213B7`/`0x214AD`/`0x21548`/
`sub_2FF01` 共用），其真闭包＝ `0x1C81F` + 既有资产（`rec_skip`/`tbl_ptr`/`util_rand`）。
两者与 §15/§39 已还原的 `unit_heal`(0x1C916)/`unit_heal_by_action`(0x1C8ED)/`unit_restore_mp`(0x1C9DD)
同族（同一 `tbl_619FD` 行动表、同一 80 字节记录表、同一 `dword_53EC8` 经验记账），故追加进 `item_use`：

| 原址 | 新名 | 行为 |
|---|---|---|
| `0x1C81F` | `unit_damage` | 对记录 `rec` 扣除 `amount` 伤害：`9*amount/10 + (rand%100)*amount/1000`，从 +64 HP 减去（截 0）；`rec[7]>=0x44` 时按 `tbl_61AF9(rec[7]-68)` 的 byte 9 × `rec[33]`（存活者再乘 `dmg/maxhp`）记入 `dword_53EC8`；返回造成的伤害。 |
| `0x1C75E` | `unit_action_damage` | 解析行动 `action` 对记录的效果：拷 0x51F96 的 28 个 dword 职业功率表，取 `tbl_619FD[action]` word0 × `tbl[rec[32]-1] / 10` 作基础伤害，`action∈10..12` 且 `rec_skip(rec)` 命中时丢弃，否则以该表 byte2 为概率调用 `unit_damage`；返回造成的伤害（未命中为 0）。 |

**陷阱 / 伪代码伪影**：
- **共享尾声（伪 callee）**：`0x1C81F` 尾 `jmp 0x14230` 落在 `sub_14121`（`map_move_to_marked`）体内——`0x14230` 是 `mov eax,ebp; jmp 0x10B46` 的共享收尾（`0x10B46` 为外部纯 `ret`），故 `function-map` 把 `sub_14121` 记成伪 callee；真正返回的是本函数算出的伤害 `ebp`，C 侧直接 `return dmg;`。
- **Hex-Rays 放回未初始化寄存器**：`0x1C75E` 的调用者伪代码把其返回值渲染成 `if (v6 != 0)` 的未初始化 `v6`（因 `0x1C75E` 被渲染成 `void`）；机器上两条返回路径一条 `jmp` 掠过 `xor eax,eax`（保留 `unit_damage` 的 eax），一条 `xor eax,eax`，故 `0x1C75E` 返回的正是造成的伤害，C 侧按 `int` 还原。
- **LOWORD 伪影**：`0x1C81F` 的 `LOWORD(v4)=sub_4EBE3()` 只是 IDA 对 `%100` 的截断视图；反汇编为全 32 位 `idiv`，C 侧按 `(int32_t)util_rand() % 100` 保留有符号语义。
- **chkstk**：两者皆 `push size; call 0x3702F`，丢弃。
- 无对拍器，标“已还原未验证”。拉出后下一游戏头名仍为 u1 小簇
  `0x1D6C8`(sub_1CFF0)/`0x24BDE`(sub_24754)/`0x28632`(sub_2825B)/`0x2AA00`(sub_29DAA)/`0x2F7B6`(sub_2EBE1) 等。

### 49) `funcs_1541F` 入口 22/26/27 + 共享伤害体（本轮，5 函数）→ `src/game/map_action.c/.h`（追加）

closure-ready 游戏头名 `0x22D1B`(u5)；离线反汇编证实其真闭包＝自身（实调 `map_draw_party_icons_anim`
(0x1C4CC)、`map_draw_party_icons`(0x1C2DA)、`unit_damage`(0x1C81F)、`map_enqueue_status`(0x1E0DB)、
`ev4_1E1DC`(0x1E1DC)、`map_view_update`(0x11CAC)、`map_draw_status_popups`(0x1DF58)、`util_rand`(0x4EBE3)，
全部已还原）。连同其共享体 `0x22CDA` 与三个 `funcs_1541F` 入口包装 `0x22BE1`/`0x22CBF`/`0x22E41`（均为其
直接 caller，共同构成条目 22/26/27）整体拉出，合计 448 B。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x22D1B` | `map_action_damage_records` | 画队伍图标后，对列表里“效果字节 `field` 仍为空且 +32 类不为 25/26”的记录：50% 概率 `unit_damage(rec,10)` + 排队数字 + 把 `rec[field]` 置 `rand%4+2` + `dword_53EC8 += 8*rec[33]`；另一半只排空闲光标精灵。末尾 `map_view_update(0)`，`dword_53EC4!=0` 时 `map_draw_status_popups()`。`table_idx`＝图标子图库（由 `map_action_charge_damage` 传入）。 |
| `0x22CDA` | `map_action_charge_damage` | 共享体：`dword_53EC4=0` → `rec_sub_table5(a1,index)` → 转 `map_action_damage_records`。三个入口包装都压常量后 `jmp 0x22BF7`（本函数体内）。 |
| `0x22BE1` | `map_action_entry22` | 入口 22：`map_action_charge_damage(a1,22,count,list,39)`。 |
| `0x22CBF` | `map_action_entry26` | 入口 26：`map_action_charge_damage(a1,26,count,list,37)`。 |
| `0x22E41` | `map_action_entry27` | 入口 27：`map_action_charge_damage(a1,27,count,list,38)`。 |

**陷阱 / 伪代码伪影**：
- **共享尾声（伪 callee）**：`0x22D1B` 尾有两处 `JUMPOUT`：`0x22BBE` 是它**自己的尾声**
  （`add esp,4; pop ebp/edi/esi/ebx; ret`，与入口 push 顺序一致），`0x22A7B` 是 `call 0x1DF58; jmp 0x22BBE`
  的共享收尾，故 `dword_53EC4!=0` 路径等价于 `map_draw_status_popups(); return 0;`，不视为 callee。
- **共享跳转尾**：`0x22BE1/0x22CBF/0x22E41` 三者前段压各自的常量后都 `jmp 0x22BF7`——`0x22BF7` 正是
  `0x22BE1` 体内 `push a1; call 0x22CDA` 的那一条，故三者语义＝同一 `sub_22CDA` 调用（C 侧各写一条
  语义调用，不模拟跳转）。`0x22BC6` 则 `jmp 0x22A9B`（另一共享体 `sub_22AA8`，§43 已还原），不属本簇。
- **常量锚定**：`sub_20C6F` 行动码 14 直接 `sub_22D1B(a1,27,count,list,38)`（与入口 27 一致）、
  `0x2DE..` 三连调用分别 `(a1,26,...,37)/(a1,22,...,39)/(a1,27,...,38)`——与三入口包装的常量逐一对上。
- **chkstk**：五者皆 `push size; call 0x3702F`，丢弃。返回值为 `unit_damage`/队列残留，调用点不读，C 侧返回 0。
- 无对拍器，标“已还原未验证”。拉出后下一游戏头名仍为 u1 小簇
  `0x1D6C8`(sub_1CFF0)/`0x24BDE`(sub_24754)/`0x28632`(sub_2825B)/`0x2AA00`(sub_29DAA)/`0x2F7B6`(sub_2EBE1) 等。

### 50) `funcs_1541F` 效果入口 0/1/2/3/8 + 共享体（本轮，6 函数）→ `src/game/map_action.c/.h`（追加）

`function-map.csv` 闭包就绪的游戏头名是 `0x214AD`(u3)，但它与 `0x2111A/211A4/21227/213B7`
以及 `funcs_1541F` 条目 0..10 的薄包装同属一张“对记录列表施加行动效果”的家族。本轮取其中最完整
且自洽的一簇：条目 0/1/2/3/8 的包装 + 它们共用的效果体（合计 279 B，全部真依赖已还原）：

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x21227` | `map_action_resolve_list` | 共享体：`dword_53EC4=0` → `map_draw_party_icons_anim(a1,idx,count,list)` → `map_blink_units(a1,idx,count,list)` → `rec_sub_table5(a1,idx)` → 对每条记录 `unit_action_damage(list[i],idx)`，命中时 `map_enqueue_status(dmg,94,list[i])`，未命中 `ev4_1E1DC(list[i])` → 共享尾声 `map_view_update(0); map_draw_status_popups()`。 |
| `0x21206` | `map_action_entry00` | `map_action_resolve_list(a1,0,count,list)`（条目 0） |
| `0x2134B` | `map_action_entry01` | 同上，`idx=1` |
| `0x21364` | `map_action_entry02` | 同上，`idx=2` |
| `0x2137D` | `map_action_entry03` | 同上，`idx=3` |
| `0x21494` | `map_action_entry08` | 同上，`idx=8` |

**陷阱 / 伪代码伪影**：
- **共享尾声（伪 callee 两处）**：`0x21227` 的循环出口 `jge 0x21190` 落在 `sub_2111A` 体内——`0x21190`
  是 `push 0; call 0x11CAC; call 0x1DF58; pop×4; ret` 的**共享收尾**（`sub_2111A/2111A4/21227/213B7`
  共用），非调用；`function-map` 因此把 `sub_2111A` 记为 `sub_21227/213B7/211A4` 的伪 callee。
- **共享跳转尾**：`0x2134B/0x21364/0x2137D/0x21494` 压完常量后 `jmp 0x2121A`；`0x2121A` 正是 `sub_21206`
  体内 `push a1; call 0x21227` 那一条，故四者语义＝同一 `sub_21227` 调用（C 侧各写一条语义调用，不模拟跳转）。
- **chkstk**：六者皆 `push size; call 0x3702F`（`__chkstk` 带 `ret 4`，弹出自己的入参），丢弃。返回值为
  `map_draw_status_popups` 残留，dispatch 经函数指针调用不读，C 侧返回 0。
- **ABI 逐指令核对**：条目包装经 `funcs_1541F[K](a1, count, list)` 进入（`sub_15311`/`sub_1CFF0` 实参），
  包装补上常量 K，效果体签名 `(a1, table_idx=K, count, list)`（`rec_sub_table5(a1,K)`、`map_blink_units(a1,K,count,list)`）。
- 无对拍器，标“已还原未验证”。拉出后 `sub_21227` 的 5 个条目闭合；条目 1/2/3/8 的兄弟
  `0x21396`(条目4) 等（共用另一效果体 `sub_213B7`，blink 走 `map_record_layer_blink`）连同自由体
  `0x2111A/211A4` 与独立条目 9 `0x214AD`(u3) 仍在册；下一游戏头名回落至
  `0x214AD`(u3)、`0x2111A`(u2) 及 u1 小簇 `0x1D6C8`/`0x24BDE`/`0x28632`/`0x2AA00`/`0x2F7B6` 等。

### 51) `funcs_1541F` 条目 4/5/6/7/9 + 共享体 `0x213B7`（本轮，6 函数）→ `src/game/map_action.c/.h`（追加）

拉出 `0x213B7`（共享效果体）、`0x21396`（条目 4）、`0x21449`/`0x21462`/`0x2147B`（条目 5/6/7）、
`0x214AD`（条目 9）。本批后 `funcs_1541F` 的低号条目 0..9 除自由体 `0x2111A`/`0x211A4` 外全部闭合；
`0x2111A`/`0x211A4` 为**动态 action 索引**的独立效果体（非表条目，由 `sub_20C6F` 依 action 码直接调），
归下一批。

| 地址 | 语义名 | 说明 |
|---|---|---|
| `0x213B7` | `map_action_resolve_layer` | 清 `dword_53EC4`；`map_draw_party_icons_anim(a1,bank,count,list)`；
  `map_record_layer_blink(a1,bank,count,list)`；`rec_sub_table5(a1,bank)`；逐记录 `unit_action_damage(list[i],bank)`
  命中则 `map_enqueue_status(dmg,94,list[i])` 否则 `ev4_1E1DC(list[i])`；`map_view_update(0)`+`map_draw_status_popups()` |
| `0x21396` | `map_action_entry04` | `map_action_resolve_layer(a1,4,count,list)` |
| `0x21449` | `map_action_entry05` | `map_action_resolve_layer(a1,5,count,list)` |
| `0x21462` | `map_action_entry06` | `map_action_resolve_layer(a1,6,count,list)` |
| `0x2147B` | `map_action_entry07` | `map_action_resolve_layer(a1,7,count,list)` |
| `0x214AD` | `map_action_entry09` | 清 `dword_53EC4`；`map_draw_party_icons_anim(a1,9,count,list)`；`rec_sub_table5(a1,9)`；**仅首记录**
  `unit_action_damage(list[0],9)` → 命中 `map_enqueue_status(dmg,94,list[0])` 否则 `ev4_1E1DC(list[0])`；尾部同上 |

**陷阱 / 伪代码伪影**：
- **共享尾声（伪 callee）**：`0x213B7`/`0x214AD` 的循环出口 `jge 0x21190` 落在 `sub_2111A` 体内——`0x21190`
  是 `push 0; call 0x11CAC; call 0x1DF58; pop×4; ret` 的**共享收尾**（`sub_2111A/211A4/213B7/214AD` 共用），
  非调用；`function-map` 因此把 `sub_2111A` 记为 `sub_213B7` 的伪 callee。
- **共享跳转尾**：`0x21449/0x21462/0x2147B` 压完常量 5/6/7 后 `jmp 0x213AA`；`0x213AA` 正是 `sub_21396`
  体内 `push [esp+0x10]; call 0x213B7` 那一条，故三者语义＝同一 `map_action_resolve_layer(a1,K,count,list)`
  （C 侧各写一条语义调用，不模拟跳转）。
- **chkstk**：六者皆 `push size; call 0x3702F`（丢弃）。返回值为 `map_draw_status_popups` 残留，
  dispatch 经函数指针调用不读，C 侧返回 0。
- **A/D 两参数序差异**：`sub_2111A` 的 `map_record_layer_blink` 用 `(a1,bank,count,list)`，而 `0x20C6F` 内联的
  20/24 号 action 走的是 `map_blink_units`（`0x1CD17`）——同形不同 blink 助手，本批不混（`0x2111A` 留待下批）。
- **条目 9 仅首记录**：`0x214AD` 不像 `0x213B7` 那样遍历 count，只处理 `list[0]`（单目标动作）；
  `count` 仍传给 `map_draw_party_icons_anim`。
- 无对拍器，标“已还原未验证”。

### 52) `funcs_1541F` 动态 action 体 `0x2111A`/`0x211A4`（本轮，2 函数）→ `src/game/map_action.c/.h`（追加）

拉出 §51 留待的两个**动态 action 索引独立体**（非表条目，由动作路由 `sub_20C6F` 依 action 码直调，
并被 `0x2D80D` 第二处调）。本批后 `funcs_1541F` 相关低号条目/自由体全部闭合。

| 地址 | 语义名 | 说明 |
|---|---|---|
| `0x2111A` | `map_action_anim_resolve` | 动作码 21 体：`map_draw_party_icons_anim(a1,bank,count,list)` → `map_record_layer_blink(a1,bank,count,list)` → 逐记录 `unit_action_damage(list[i],bank)`，命中 `map_enqueue_status(dmg,94,list[i])` 否则 `ev4_1E1DC(list[i])`；结尾 `map_view_update(0)`+`map_draw_status_popups()`。**它是共享尾声 `0x21190` 的宿主**；等价于 `map_action_resolve_layer` 但不清 `dword_53EC4`、不收 `rec_sub_table5` 代价 |
| `0x211A4` | `map_action_heal_list` | 动作码 5/13 体：固定 bank 13 的两遍图标（`map_draw_party_icons_anim`/`map_draw_party_icons`），逐记录 `unit_heal(list[i],amount)` → `map_enqueue_status(healed,105,list[i])`；结尾共享 `0x21190` |

**陷阱 / 伪代码伪影**：
- **共享尾声（伪 callee）**：两函数结尾 `push 0; call 0x11CAC; call 0x1DF58`（`0x21190`）被 `function-map`
  记为 `sub_2111A` 的 callee（`0x21227/0x213B7/0x211A4/0x214AD` 共用），非调用；C 侧写成语义调用。
- **ABI**：cdecl `(a1, count, list, bank)`（`0x211A4` 第四参为治疗量）。逐指令核出：保存 4 寄存器后
  `edi`/`ebp` 指向 `a3`/`a2`（`0x2111A`）与 `a2`/`a3`（`0x211A4`），循环计数取 `[esp+0x18]`/`edi`；
  `sub_1C4CC`/`sub_1CAC7` 的压栈序为 `(a1, bank, count, list)`。
- **chkstk**：两者均为 `push 0x24; call 0x3702F`（丢弃）。返回值为 `map_draw_status_popups` 残留，
  dispatch 经调用点直调不读，C 侧返回 0（与 §51 同风格）。
- 无对拍器，标“已还原未验证”。

### 53) 地图动作路由器 `0x20C6F` → `map_action_router`（本轮，1 函数）→ `src/game/map_action.c/.h`（追加）

拉出 P1 头号游戏闭包就绪项（usage 2，1043B）：整个地图动作族的**路由器**（`map_action.c` 的
0x21082..0x22E41 效果、以及 §43/49/50/51/52 各条，均由它按 action 码分派）。§52 补入
`0x2111A`/`0x211A4` 后其待还原 callee 归零，本轮闭合。

**行为**：`rec_field_byte(actor, slot)`（0x1B722）经 0x4E8BC（base 0x602AD/stride 23）选出 item 条目；
条目 byte +13 = action 码、+14 word = 图标 shape bank。按 action 码分派：
- 5/13 → `map_action_heal_list`；action==5 再 `rec_slot_remove(actor,slot)`
- 6/7 → `item_apply(actor,20/21,count,list,37/38)`，`dword_53EC4!=0` 时 `map_draw_status_popups`
- 8/9/10/17/18/19 → `map_action_add_field`（field 55/57/62/66/70/59，table_idx 17/18/19/13/13/19；
  19 存档/恢复 rec[60]）
- 11 → 固定 bank 13 两遍图标 + 逐记录 `unit_restore_mp`/`ev4_1E1DC`
- 12/15/16 → `map_action_widen`/`map_action_grow_y`/`map_action_grow_x`
- 14/22 → `map_action_damage_records`（table 27/22，field 38/39）
- 20/24 → `map_draw_party_icons_anim`+`map_blink_units`+`unit_action_damage`
- 21 → `map_action_anim_resolve`；23 → `map_units_move_deploy`
所有路径汇入共享尾：`dword_53EC8=0; menu_stop_and_free_music(); steps=map_collect_scripts(buf);
map_units_animate(); ev_script_run(actor,steps,buf)`。

**陷阱 / 伪代码伪影**：
- **chkstk**：`push 0x9C; call 0x3702F`（丢弃）。
- **伪参数**：IDA 给尾部的 `menu_stop_and_free_music`（0x1D4F6）编出 14 个 `v18[i]` 实参，实为**未初始化的
  0x64 字节脚本缓冲区**的栈偏移假象；机器码 `call 0x1d4f6` 不压栈（0 参）。
- **返回值**：`ev_script_run` 为 void，两调用点（0x152F0/0x1BE45）均不读返回值，C 侧返回 0。
- ABI cdecl `(actor, slot, count, list)`（调用点 0x152F0/0x1BE45 依次压 `ebp, dword_53Cx, count, &buf`）。
- 无对拍器，标“已还原未验证”。

### 54) `funcs_1541F` 条目 10/11/12/13 + 共享体（本轮，6 函数）→ `src/game/map_action.c/.h`（追加）

拉出闭包就绪的同表头名 `0x21527`(u3)/`0x21AD9`(u3) 及其两个共享体 `0x21548`/`0x21B18`，
连同两个相邻条目包装 `0x2185F`/`0x21A9E`（同 caller `sub_15311`/`sub_1CFF0`，且 `0x2185F/0x21A9E`
的尾跳就落在 `0x21527` 体内）。本批后 `funcs_1541F` 条目 10..13 闭合（余 11→12 已含、
14/15/16/24 待 `0x21B99/0x2211C/0x22153`，均已因 `0x21B18` 就绪而闭包就绪）。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x21548` | `map_action_zoom_resolve` | 条目 10/11/12 共享体：`map_shape_lib_decode()` 重建 `dword_53A5D` → 建 64×64 源基址表
  （`cells[64*i+j]=dword_53A5D+6+576*cell[0]`）→ 3 帧 `map_zoom_draw`（x/y/zoom 表 `0x52096/0x520A2/0x520AE`，
  x 基 `+3072*dword_53AA9+1536*dword_51A87`、y 基 `+3072*dword_53AAD+1536*dword_51A8B`）逐帧把
  `scratch0+0x504` 拷进 `pool[k]+0x8088` 并重画未标记记录肖像 → 60 tick 把 `pool[n%4]+0x8088` 发到
  `0xA0504`（`n<43 && n%6==0` 时 `svc_play_sfx(dword_53B13,13,1)`）→ 释放暂存、还原 `dword_53A5D` →
  `map_view_update(0)` → 逐记录 `unit_action_damage(list[i],table_idx)` 命中 `map_enqueue_status(dmg,94,·)`
  否则 `ev4_1E1DC` → 尾部 `map_view_update(0)`+`map_draw_status_popups()` |
| `0x21527` | `map_action_entry10` | 条目 10：`map_action_zoom_resolve(a1,10,count,list)` |
| `0x2185F` | `map_action_entry11` | 条目 11：`svc_play_sfx(dword_53B13,2,1)` + `portrait_reveal_cell(a1,15,10)` → `map_action_zoom_resolve(a1,11,…)` |
| `0x21A9E` | `map_action_entry12` | 条目 12：同上半径 30/步 16，索引 12 |
| `0x21B18` | `map_action_heal_action_list` | 条目 13/14/15/16/24 共享体：`dword_53EC4=0` → `map_draw_party_icons_anim/_icons(a1,table_idx,count,list)` →
  `rec_sub_table5(a1,table_idx)` → 逐记录 `map_enqueue_status(unit_heal_by_action(list[i],table_idx),105,list[i])` →
  落在 `0x2111A` 体内的共享尾声 `0x21190`（`map_view_update(0)`+`map_draw_status_popups()`） |
| `0x21AD9` | `map_action_entry13` | 条目 13：`svc_play_sfx(dword_53B13,11,1)` + `portrait_reveal_zoom(1,2)` → `map_action_heal_action_list(a1,13,…)` |

**陷阱 / 伪代码伪影**：
- **共享尾声 / 共享体内跳（伪 callee）**：`0x2185F/0x21A9E` 尾 `jmp 0x2153b`（`0x21527` 体内
  “`push 0xa; push a1; call 0x21548`”那一条）⇒ 各自调用共享体；`0x21527` 只是薄包装。`0x21AD9` 尾
  `jmp 0x21b0b` 属自身线程（对应 `0x21B18` 的 `push a1; call`）；`0x21B18` 尾 `jmp 0x21190` 落在
  `0x2111A` 体内共享尾声 ⇒ 普通 `return`。`function-map` 把 `0x21527`/`0x21AD9` 记为 `0x2185F` 等的
  callee，实为共享代码段（非调用）。
- **OOM 尾不同形**：`0x21548` 的失败路径是 `printf(aOutOfMemoryAtE); exit`（`push 0`，无文本模式切换），
  与 `scenario_load`/`map_shape` 的 `exit(1)` 不同，C 侧照机器写 `exit(0)`。
- **4 槽暂存池的第 4 项别名**：`0x21548` 机器栈槽 `[esp]/[esp+4]/[esp+8]/[esp+0xc]` = `{frame, scratch_p,
  scratch_v18, scratch_p}`（第 3 项与第 1 项同指针），故 `pool[n%4]` 第 3 槽复用 `scratch1`；C 侧照此写。
- **未对齐 dword 表**：`0x52096/0x520A2/0x520AE` 处 9 个 dword（地址非 4 对齐）用 `memcpy` 取，避免
  未对齐指针解引用。
- **ABI（chkstk 骗过反编译器）**：`0x21548` 的 Hex-Rays 局部 `v4..v24` 是真实实参/暂存；实参由
  `mov ds:0x53a49` 与压栈序核定为 `(a1, table_idx, count, list)`；`0x21B18` 同理。
- **返回值**：两共享体尾声 eax 为 `map_draw_status_popups` 残留，dispatch 经函数指针调用不读，C 侧返回 0。
- **chkstk**：六者皆 `push size; call 0x3702F`（不分配帧），丢弃；`malloc/free` → `guest_malloc/guest_free`。
- 无对拍器，标“已还原未验证”。下一 `funcs_1541F` 候选：条目 14/15/16/24（`0x21B99`/`0x2211C`/`0x22153`）。

### 55) `funcs_1541F` 条目 14/15/16(+24)（本轮，3 函数）→ `src/game/map_action.c/.h`（追加）

承接 §54 的推荐：这三个**薄包装**（各 55B，u3）共用同一尾部——压 `count/list/table_idx` 后
`jmp 0x21b0b`（`0x21B18` 体内 `push a1; call 0x21B18` 那一条），即各自调用 §54 已还原的
`map_action_heal_action_list`。三者真依赖全在册（`svc_play_sfx`(0x25A96 已验证)、
`portrait_reveal_zoom`(0x21EB1 §7)、`map_action_heal_action_list`(0x21B18 §54)）。

| 原址 | 新名 | 条目 | 要点 |
|---|---|---|---|
| `0x21B99` | `map_action_entry14` | 14 | `svc_play_sfx(dword_53B13,11,1)` + `portrait_reveal_zoom(2,4)` → `map_action_heal_action_list(a1,14,count,list)` |
| `0x2211C` | `map_action_entry15` | 15 | 同上，`portrait_reveal_zoom(8,4)`，索引 15 |
| `0x22153` | `map_action_entry16` | 16 **与 24** | 同上，`portrait_reveal_zoom(6,6)`，索引 16 |

**陷阱 / 伪代码伪影**：
- **条目 24 复用 0x22153**：`analysis/dispatch_tables.csv` 的 `funcs_1541F` 槽 16 与槽 24 的 target
  同为 `0x22153`（同一 C 函数同时服务两槽），故未新增单独符号；注释已标注。
- **共享体内跳**：三者尾 `jmp 0x21b0b` 落在 `0x21B18` 体内的 `push [esp+0x10]; call 0x21B18` 那一条
  （`0x21AD9` 亦跳此）；C 侧各写一条语义调用，不模拟跳转。
- **ABI / 实参序**：`svc_play_sfx` 压 `(dword_53B13, 11, 1)`；`portrait_reveal_zoom` 压 `(radius, step)`
  （末压者为第一参：`push 4;push 2` ⇒ `(2,4)`）；尾部依次压 `list`(a3)、`count`(a2)、`table_idx` 常量，
  再由 `0x21b0b` 补压 `a1`——逐指令核对 `21b99..21bcf`、`2211c..2214e`、`22153..22185`。
- **chkstk**：三者皆 `push 0x14; call 0x3702F`（不分配帧），丢弃。返回值仅供表分派、无调用者读取，C 侧返回 0。
- 无对拍器，标“已还原未验证”。

### 56) 地图方向选择光标键环 + 逐帧精灵绘制（本轮，3 函数）→ `src/game/map_cursor.c/.h`（追加）

§20 `map_cursor` 家族剩下的三个闭包就绪项（§20 引言里点名的“key loop 0x177FC”）：五条驱动器
`0x16F55/0x1728C/0x18D8C/0x19DF7/0x1BBDC` 用 `dword_53C57` 选方向时，先 `0x1741C` 开补片，再进
`0x177FC` 键环，最后 `0x176B4` 收补片。本轮补上中间这环及其两个 callee（真依赖全在册：
`kbd_pending`(0x10620)/`pal_anim_step`(0x4E31C)/`anim_frame_step`(0x1297D)/`map_render_view`(0x11EEE)/
`dlg_portraits_refresh`(0x127A9)/`map_draw_cursor`(0x1ACF3)/`gfx_copy_rows`(0x11EB0)/
`dlg_portrait_find`(0x12C0D)/`dlg_portrait_draw`(0x127E0)/`gfx_blit_transparent`(0x4ED34)、BIOS tick、
INT 16h `int386`），合计 685 B。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x177FC` | `map_cursor_dir_select` | 键环包装：Esc(01)→−1、Enter(1C/39)→1；方向键 48/50/4B/4D 仅在 `frame_b[0/3/1/2]==0` 时把 `dword_53C57` 置 0/3/1/2，否则返回 0 |
| `0x17898` | `map_cursor_dir_run` | 忙等键环：`while(!kbd_pending())` → `pal_anim_step` → BIOS tick 步进 `dword_53C13` 相位（`diff>3 || diff<0`）→ `anim_frame_step` → `map_render_view(13×8)` → `dlg_portraits_refresh` → `map_draw_cursor` → 0x179D5 → `gfx_copy_rows` 发 312×192 到 `0xA0504`；出环读 INT 16h 扫描码（E0/52→1C、53→01） |
| `0x179D5` | `map_cursor_dir_draw` | 逐帧画 4 个方向光标精灵：索引 `3*frame_a[j]+2*frame_b[j]`（选中项 `j==dword_53C57` 加 `dword_53C13`），tile=`dword_53A89[dword_53A89+4*idx]`，贴在视图原点 + `{-9120,888,936,10944}`；再 `dlg_portrait_draw(found)` |

**陷阱 / 伪代码伪影**：
- **ABI（Hex-Rays 丢第二实参）**：`0x177FC` 的 prototype 被 IDA 记为 `int(int)`，但机器 `mov ebx,[esp+0xc]`
  即第二实参（`ebx` 后又用作 `frame_b` 数组指针）；5 个调用点（`0x16FA9/0x17334/0x18EEA/0x19EC6/0x1BC89`）
  均连压两个 4-int 数组指针，证实签名 `(frame_a, frame_b)`（同 §20 的“ABI 由调用点定案”）。
- **伪 callee（共享尾声）**：`0x179D5` 尾 `jmp 0x13FCC` 落在 `sub_13E9C`（`map_move_to_nearest`）体内
  （`add esp,0x18; pop×4; ret`）⇒ 普通 `return`；`function-map` 不因此传递依赖。
- **BIOS tick 读数**：机器 `mov ax,[0x46c]; cwde`（先 16 位再符号扩展），`diff = tick - dword_53C17`，
  `cmp 3; jg`＋`test; jge` 等价于 `diff>3 || diff<0`；出环时再 `movsx` 一次写回 `dword_53C17`——C 侧同（`anim.c` 既有 `BDA_W` 模式）。
- **REGS 单元**：键码经 `byte_53A8E`（`word_53A8D` 的高字节/AH）读取，与 `boot.c`/`map_interact.c` 一致；
  `int386(0x16,&word_53A8D,&word_53A8D)`。
- **chkstk**：三者皆 `push size; call 0x3702F`，丢弃。无对拍器，标“已还原未验证”。

### 57) 商店/地点“队伍属性预览”行绘制 + 有效属性助手（本轮，2 函数）→ `src/game/shop_party_stats.c/.h`（新建）

闭包就绪头名 `0x2825B`(u2，本轮最高) 及其唯一 callee `0x28632`(u1)——`shop_rows` 的
party 行绘制同族第四员：驱动 `0x27F4A` 快照 VGA、逐行调 `0x2825B`、用 `status_screen_run(2)` 出键环。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x2825B` | `shop_party_stat_rows` | 最多 3 行队员：精灵（bank `dword_53A61`，帧 `12*rec + dword_53F72`（3→1））、名字流 `(+8)+1`、再画 4 组“现值/预览值”对——现值取记录 `+48/+4A/+4C/+4E`（`unit_recalc` 已算好的总属性），预览值取 `unit_stat_totals`，配色由 `ev5_2860A`（0x1F 等/0x2A 低/0x77 高）定，标签子图 `dword_53F66` 的 `+4E/+52/+56/+5A/+5E`，`cursor` 行用亮色 201 |
| `0x28632` | `unit_stat_totals` | 记录基础四属性（`+37/+39/+3E/+3E`，有符号 16 位）＋传入道具表项 `tbl_602AD(item)` 的 `+1/+5/+3/+7`，再对每个已装备槽（`slot[0]&0x40`）按“道具类型类别不同”（`entry[0]<=0x14` vs `>0x14`）叠加其 `+1/+5/+3/+7`；四个 32 位和写入 `out[0..3]`，不回写记录 |

**陷阱 / 伪代码伪影**：
- **Hex-Rays 丢第二实参**：`sub_2860A`（已还原 `ev5_2860A`）原型被记成 `int(a)`，但机器 `cmp [esp+4],[esp+8]`
  即比较两实参；调用点连压 `(现值, 预览值)`，C 侧补回第二实参。
- **chkstk**：两者皆 `push size; call 0x3702F`，丢弃；`0x2825B` 返回值为末次 `dlg_draw_number` 的 eax，
  唯一调用点 `0x27F4A` 不读，C 侧声明 `void`。
- **伪 callee**：`0x28632` 尾 `jmp 0x26E30` 落在 `sub_26CE4`（`screen_curtain`）体内（共享尾声），
  非调用；真依赖仅 `tbl_ptr`。
- 无对拍器，标“已还原未验证”。

### 58) 商店/地点“队伍属性预览”屏驱动器（本轮，1 函数）→ `src/game/shop_party_stats.c/.h`（追加）

接 §57：闭包就绪头名 `0x27F4A`(u1) 即 §57 备注里“still pending”的预览屏驱动器，其唯一真 callee
`shop_party_stat_rows`（0x2825B，§57 已还原）；同族 `0x2872B`(sub_279BC) 因调 `0x16559`（ABI 存疑，见 §0-F）暂跳。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x27F4A` | `shop_party_stat_run` | 与 `shop_party_run`(0x2A29D) 同型：`guest_malloc` 三块 0xFA00（53C5B/5F/63）→ `memmove` 快照 0xA0000 → 清 53F4E/53C57 → `rle2_blit` 面板子图（53F66+[53F66+70]→+35845）→ `shop_party_stat_rows` → 六条 `msg_blit_band` 带；键环 `status_screen_run(2)`：Down 80/Up 72 移位并带滚（`screen_band_forward/backward`、阈值 3），Enter 28/Space 57→返回 1，Esc 1→返回 -1 |

**陷阱 / 伪代码伪影**：
- **共享尾声**：`0x27F4A` 尾条件 `jmp 0x26A73` 落在 `sub_26A0D` 体内，但该尾声 `mov eax,esi; pop ebp/edi/esi/ebx; ret`
  与自身四寄存器帧完全匹配 ⇒ 即 `return esi`（返回 1/-1）；`sub_26A0D` 由此是**伪 callee**。
- **Hex-Rays 丢返回值**：原型被记 `void` 且调用点丢弃，但机器 `jmp` 前 `mov eax,esi`，C 侧声明 `int` 返回选择值。
- **chkstk**：`push 0x28; call 0x3702F`，丢弃。
- 无对拍器，标“已还原未验证”。

### 59) 地图行动子树（`sub_1CFF0`/`sub_18D8C`）的三个闭包就绪叶子（本轮，3 函数）

本轮闭包就绪游戏头名（u1）**全部落在这条地图行动子树里**：`0x1D6C8`/`0x24BDE` 是
`sub_1CFF0`（其 callee 经 `sub_24754`/`sub_2FF01` 传递）的直接/间接叶子，`0x2F7B6` 是
`sub_2EBE1`（`sub_2CF30`→`sub_2FF01`→`sub_1CFF0`）的唯一此前待还原 callee。三者真闭包＝自身，
合计 1067B（≤1500B），故作为同一子树的叶簇整体拉出；本批后 `0x24754`/`0x2EBE1` 待还原 callee 归零，
`0x1CFF0` 由 4 降为 3（余 `0x1CEED`/`0x1D51D`/`0x2FF01`）。

| 原址 | 新名 | 模块 | 要点 |
|---|---|---|---|
| `0x1D6C8` | `pal_flash_entry(index)` | `fade.c`（追加） | `svc_play_sfx(dword_53B13,0,1)` 后 4 次交替：`outp(0x3C8,0)` + 三通道 `byte_51AAD/51AD1/51AF5[index]`（点亮），`svc_wait_ticks(1)`，再 `outp(0x3C8,0)`+三通道 0（熄灭），`svc_wait_ticks(1)`。 |
| `0x24BDE` | `party_has_unit(id)` | `party_roster.c`（追加） | 扫 `dword_53BF7` 的 `dword_53BFB` 条记录（槽 0..count-1，与 `party_member_present` 的 1..count 不同），记录 +8 的 id 字节（`movzx`）等于 `id` 则返回 1，否则 0（`jge` 有符号计数）。 |
| `0x2F7B6` | `map_attack_result(atk,def,out)` | `map_attack.c`（追加） | `map_attack_resolve`(0x1ECC7) 的同胞：同一 rec +0x48/0x4a/0x40/0x42/0x4c/0x4e、5 张 `0x51A12/0x51A2A/0x524A8` 表；`out[0]`=命中/`out[1]`=暴击半防/`out[2]`=反击（item[9]==2 且 `util_rand()%100<item[10]` 时写 def[0x25]=`rand()%4+2`）/`out[4]`=item[9]==3/`out[5]`=伤害；命中后记 `dword_53EC8`（等级表 0x4E84F 的 `[9]`、`v32=atk[0x21]`）。 |

**陷阱 / 伪代码伪影**：
- **既有模块的基址笔误（发现）**：`map_attack_resolve`(0x1ECC7) 用 `tbl_ptr(&fd2_data2[0xAF9],10,...)`，但
  0x4E84F 的反汇编为 `lea edx,ds:0x61af9`（＝`fd2_data2[0x1AF9]`，`item_use.c`/`tables.h` 一致）——
  既有代码少了一个 `0x1000`（`0x60AF9`）。本批新函数按权威反汇编用 `&fd2_data2[0x1AF9]`，**未改动**该已验证资产，
  仅作台账记录待复核。
- **伪 callee / chkstk**：三者皆仅误挂 `sub_3702F`（chkstk，不分配帧），丢弃；无其它伪 callee。
- **写权限**：`0x2F7B6` 会写 def 记录（+0x25），故 `atk`/`def` 用 `uint8_t *`（非 const）。
- **字节回绕**：`v32=atk[0x21]` 的 `+30` 是机器 `add BYTE PTR`，C 侧保持 `uint8_t`。
- **返回值**：`0x1D6C8` 尾声 eax 残留 `svc_wait_ticks`，唯一调用点 `0x1CFF0` 不读 ⇒ `void`；
  `0x24BDE` 返回 0/1、`0x2F7B6` 返回伤害（同时写入 `out[5]`）。
- 无对拍器，标“已还原未验证”；`sub_2AE0E` 型的未初始化读陷阱不适用本批。

### 60) ANI.DAT 动画帧命令解释器（`funcs_37012` 表 + 两个入口，本轮 12 函数）→ `src/game/ani.c/.h`（新建）

本轮闭包就绪头名里，除 CRT/库包装（`0x3645D`/`0x2E95B`/`0x365DA`）与超预算的 `0x2EBE1`(2291B) 外，
唯一连贯的游戏内容簇是 `0x36E3D..0x36FAC` 十件套 + 其调度器 `0x36FF4`(u1) 与缓冲绑定 `0x36FD3`(u1)：
十者互为 `sub_36FF4` 的唯一 callee 且同为 `funcs_37012` 表项，真闭包＝本 12 函数（扁平、无外部依赖），
合计 488B（≤1500B）。**此前批次按“小库函数”跳过（见 §33）；本批据 `0x20421` 装载器证据改判为游戏内容**：
`0x20421` 打开 ANI.DAT，逐帧 `malloc(0x300)` 调色板 + `malloc(0xFA00)` 帧暂存，再
`sub_36FD3(64000, 0xA0000, palette)` + `sub_36FF4(cmd_count, frame_bytes)` —— 即 320×200 VGA 帧缓冲
（`dword_52762`/`word_52760`=64000）与调色板（`dword_52766`/0x300）的动画解码器。

| 原址 | 索引 | 新名 | 行为 |
|---|---|---|---|
| `0x36FD3` | — | `ani_target_set(size,screen,palette)` | 绑定帧缓冲尺寸/指针与调色板指针（写 `word_52760`/`dword_52762`/`dword_52766`） |
| `0x36FF4` | — | `ani_run_frame(count,cmds)` | 逐命令：取一字节 opcode → `funcs_37012[op]`（表＝10 项 dword @`0x5276A`） |
| `0x36E3D` | 0 | `ani_pal_fill` | 调色板（768B）以单字节灰值填充 |
| `0x36E57` | 1 | `ani_pal_copy` | 由流复制 0x300 字节到调色板 |
| `0x36E65` | 2 | `ani_pal_rle` | RLE 展开到调色板（0xC0 标记＝行程） |
| `0x36EA7` | 3 | `ani_pal_blocks` | `n` 个调色板块：{偏移=3·b, 长度=3·b}+数据 |
| `0x36EE0` | 4 | `ani_fb_fill` | 帧缓冲以单字节填充 |
| `0x36F08` | 5 | `ani_fb_copy` | 由流复制 `size` 字节到帧缓冲 |
| `0x36F24` | 6 | `ani_fb_rle` | RLE 展开到帧缓冲（至 `size`） |
| `0x36F69` | 7 | `ani_fb_pixels` | `n` 个单像素 {16 位偏移, 值字节} |
| `0x36F82` | 8 | `ani_fb_fill_runs` | `n` 个填充段 {偏移, 长度, 值} |
| `0x36FAC` | 9 | `ani_fb_copy_runs` | `n` 个复制段 {偏移, 长度}+数据 |

**陷阱 / 伪代码伪影**：
- **隐式 ESI ABI**：`sub_36FF4` 仅在入口 `mov esi,[ebp+0xc]` 一次，之后各 handler 用 `lodsb/lodsw` 自行推进
  ESI 并在返回时归还（调度器循环不重载 ESI）⇒ 流游标是 in/out 隐式实参，C 侧建模为 `const unsigned char **sp`。
- **Hex-Rays 伪影**：各 handler 的 ESI 局部指针被渲染成未初始化（`_BYTE *v1; LOBYTE(v0)=*v1;`）——按上条语义处理。
- **表地址提示**：`funcs_37012` 名指调用点 `0x37012`，真表是 dword 数组 @`0x5276A`（=`fd2_data1[0x276A]`，
  逐项＝0x36E3D/57/65/A7/E0/08/24/69/82/AC），已按镜像字节核实。
- **`rcl ecx,1` 还原长度**：RLE 内 `shr ecx,1; rep stosw; rcl ecx,1; rep stosb` 是 len 的 word+余字节写法
  （等价写 `len` 个字节）；C 侧直接写 `len` 字节。
- **do/while 计数**：op3/7/8/9 的 `dec/jne`、`loop` 均为 do/while（最小执行一次）；ANI 数据计数恒 ≥1，按原样还原。
- **函数计数偏差说明**：本簇 12 函数超“1..6”条，但它是**单一 10 项调度表 + 其两个入口**（同 §0-H/§0-I
  整表拉出先例），闭包扁平、488B、无外部依赖。
- 无对拍器，标“已还原未验证”；另以 `-m32` 临时自测（帧填充 / RLE / 像素 / 填充段 / 复制段 / 调色板块 / 调度）
  全部通过验证行为，自测文件未入库。

### 61) 物品使用屏驱动器（本轮，1 函数）→ `src/game/item_menu.c/.h`（追加，item_menu 家族闭合）

§40 建 `item_menu` 时留下的驱动器 `0x2AA00`(u1, 637B) 是本轮 `function-map.csv` 闭包就绪游戏头名之一，
其全部 callee 均已在册（`item_menu_list`/`item_menu_use`/`item_menu_figure_play` §40，`shop_party_equip_run` §32，
`vm_run`/`dlg_blit_dato`/`dlg_wait_key`/`kbd_flush`/`map_window_run`/`map_draw_text_window`/`bgm_play`/
`ev3_2AEDB`/`rec_slot_remove`/`unit_entry_data`/`unit_metrics` 及 `tbl_ptr(0x615FE,2)`），故按同模块簇拉出，
`item_menu` 家族自顶闭合。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x2AA00` | `item_menu_run()` | 循环：`item_menu_list` 收集可用队员；为空则播空脚本 591 并返回；否则开肖像 + 脚本 592 + `shop_party_equip_run` 选人；取消则返回；确认后再开肖像 + 脚本 594 + `map_window_run`/`map_draw_text_window`，仅当 `dword_53C57==0` 时应用：按 kind（52 / >=50）`rec_slot_remove(rec, ev3_2AEDB(rec,·))` → `bgm_play(16,1)` → `item_menu_figure_play` → `bgm_play(11,0)` → `rec[0x20]=tbl_615FE[kind].byte0`、`rec[7]=kind` → 释放/重建 FDICON.B24 图集（`unit_entry_data` 逐队员）→ `item_menu_use` → `kbd_flush`。 |

**陷阱 / 伪代码伪影**：
- **反编译器丢实参（chkstk 骗过反编译器）**：`sub_2AE0E(v10,v9)` 被 Hex-Rays 削成单参 `sub_2AE0E(a1:v10)`，
  `sub_2A857` 削成 `sub_2A857(a1:v0)`——离线反汇编（`2aa11..2aa19`、`2aad5..2aade`）显示压栈序为
  `(records,kinds)` 与 `(count,members,equip)`，C 侧补回。`v9`（kinds 数组）因此非未初始化读——由
  `item_menu_list` 写出，两者都是 32 字节栈数组（原机为 `[esp+0]`/`[esp+0x20]`）。
- **`buf`=0x53A61**：`if (buf) free(buf);` 即 `guest_free(dword_53A61)`；`fopen(aFdiconB24_4,aRb_8)`（0x50283/0x50280）走 libc。
- **返回值**：机器尾声 eax 为 `fclose`/`kbd_flush` 残留，唯一 caller `0x29F48` 不读，C 侧返回 0（原型 `int(void)`）。
- **chkstk**：`push 0x7c; call 0x3702F`（不分配帧），丢弃。
- 无对拍器，标“已还原未验证”；caller `0x29F48`（地点/商店菜单 0..3 选单）仍待还原。

### 62) 四槽存档/场景选择菜单（本轮，2 函数）→ `src/game/save_menu.c/.h`（新建）

§61 后闭包就绪游戏头名多为大体量驱动器，本轮改由“共享尾声伪 callee”入手：`0x29BCB`(u3) 的 callee 表把
`sub_29AB2` 与 `sub_2872B`（尾跳 `0x28B3C` 落在 `0x2872B` 体内）都算进去；逐指令核 `29ab2..29b40` 后确认
`0x29AB2` **尾跳 `0x28B3C` 只是共享尾声**（4 寄存器 pop + ret，与本函数自己的 `0x29AB2` 帧匹配），**无 `0x2872B` 调用**；
`0x29BCB` 也以 `jmp 0x26A73`（`mov eax,esi; pop×4; ret`）返回。二者的其它 callee 全部在册
（`vm_run`/`msg_blit_band`/`rle2_blit`/`svc_play_sfx`/`dlg_wait_key`/`guest_malloc`/`int386`），故本 2 函数闭包闭合。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x29AB2` | `save_slot_rows(selected, dst, base)` | 画 4 个存档槽行：逐 i 置 `dword_53AE1=i+1`，`vm_run(dword_53A7D, 549, 320*(19i+119)+dst+10, 320, fg, 76,0,0,0)`；槽 id=`byte[base+12587+2600i+2560]`，`0xFF` 时只画空槽标签（流 514，+88），否则流 `id+514`（+40）与 `id+550`（+130）；选中行 fg=201，其余 205。 |
| `0x29BCB` | `save_slot_select(base, wait_keys)` | 取 3 块 0xFA00 暂存（`dword_53C5B/F/63`，`guest_malloc`）→ 快照 VGA → `rle2_blit(dword_53C63+0x8C05, dword_53F66+[dword_53F66+0x46], 320)` → `save_slot_rows(dword_53C57, dword_53C63, base)` → `svc_play_sfx(dword_53EEC,5,1)` → 6 段 `msg_blit_band` → 循环：`wait_keys` 非 0 时 `dlg_wait_key(0)`，否则裸 INT 16h AH=10h（E0/52→1C、53→01 归一）；`0x50`(下)/`0x48`(上) 在 `dword_53C57` 0..3 内移动并各播 sfx 7，重画到 `0xA0000`；回车/空格→1、Esc→−1。 |

**陷阱 / 伪代码伪影**：
- **伪 callee（共享尾声）**：`function-map.csv` 把 `0x29AB2` 的 `jmp 0x28B3C`、`0x29BCB` 的 `jmp 0x26A73` 记成对
  `0x2872B` 的调用；离线反汇编证实二者是跨函数复用的尾部（`0x28B3C`=`pop ebp/edi/esi/ebx; ret` 于 `0x2872B` 末尾，
  `0x26A73`=`mov eax,esi; pop×4; ret`）。真依赖不含这两个函数。
- **返回值**：`0x29BCB` 经 `0x26A73` 返回 eax=esi（1/−1），与 `shop_items_run` 同构；`0x29AB2` 的尾部返回 eax=最后一次
  `vm_run` 残值，三个调用点均不读，C 侧声明 `void`。
- **`dlg_wait_key` 的 void 缺口**：机器调 `0x16C57` 后在 eax 取扫描码，而 port/ 资产把 `dlg_wait_key` 声明为 `void`；
  同 §9/§61 约定，改为 `dlg_wait_key(0); key = byte_53A8E;`（`0x16C57` 已把归一后的扫描码写回 `byte_53A8E`）。
- **槽数据来源**：`base` 是 22987B 的 FD2.SAV 镜像（`0x25EBB` 场景菜单、`0x2968D` 存、`0x2986F` 读三处 caller 均传该缓冲）。
- **未释放**：`0x29BCB` 只分配不释放三块 0xFA00（机器原样，供 caller 复用），与 `shop_items_paint` 一致。
- **chkstk**：`push 0x20; call 0x3702F`（`0x29BCB`）、`push 0x38; call 0x3702F`（`0x29AB2`）不分配帧，丢弃。
- 无对拍器，标“已还原未验证”。

### 63) FD2.SAV 存档/读档驱动器（本轮，2 函数）→ `src/game/save_menu.c/.h`（追加）

§62 已还 `save_slot_select`/`save_slot_rows`（槽选择屏）；其两侧的读写驱动器仍未还原，且二者闭包全在册
（仅 `save_slot_select` + 已还原资产），故本批按“槽族整体闭合”一并拉出：

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x2968D` | `save_slot_write(wait_keys)` | `guest_malloc(0x59CB)` → `fopen("FD2.SAV","rb")`→`fread`→`util_deobfuscate`；失败则 `memset(0xFF)`。循环：`save_slot_select(buf, wait_keys)`，非 −1 时把当前队伍表 `dword_53BF7`(0xA00) 与 10B 状态头（`dword_53C03/53BFB/53BF3`、`byte_51AAB/53AF9/51E61/51E62`）写入选中槽（`12587+2600*idx`），再 `open("wb")`、`[0x59C7]=util_sum_tail4`、`util_deobfuscate`、`fwrite`、`fclose`、复 `util_deobfuscate`；`byte_523E7[dword_53C03]==0` 时播存档完成图（`msg_*`+`vm_run(660)`+`dlg_blit_dato`+`dlg_wait_key`）；循环尾 `msg_close_portrait`；`guest_free`。 |
| `0x2986F` | `save_slot_load(void)` | 同上读图；循环：`save_slot_select(buf, 1)` → `msg_close_portrait`；非 −1 且槽 id≠0xFF：若 `byte_523E7[id]!=0` 播不兼容图（`vm_run(479)`），否则 `memmove` 队伍表、回填状态头、`guest_free(dword_53A61 'buf')`、重建 `FDICON.B24` 图集（`unit_entry_data`）、`dword_53F56=tbl_ptr(...0x4E809)`、`ret=-1`、播载入完成图（`vm_run(478)`）；随后 `dlg_blit_dato`/`dlg_wait_key`/`msg_close_portrait`；`guest_free`。 |

**陷阱 / 伪代码伪影**：
- **CRT 文件对映射 libc**：机器 `0x37324`(fopen)/`0x373CA`(fread)/`0x377A3`(fwrite)/`0x3759C`(fclose)/`0x3771C`(memmove)/`0x37910`(memset) 直接写 `<stdio.h>/<string.h>`，与 `scenario_load.c` 同约定。
- **堆缝**：`0x3706E`/`0x3776E`（malloc/free）走 `guest_malloc`/`guest_free`（`0x59CB` 镜像 + `dword_53A61` 图集）。
- **`0x16559` 单参 ABI**：机器仅压 1 个实参（idx=0），按已还原资产约定写 `dlg_blit_dato(dword_53A85, dword_53C67, 0)`（§0-F 待动态阶段定论）。
- **`0x4E809` 表访问器**：非函数名 `tbl_6238D`，按 `tables.h` 写 `tbl_ptr((void*)&fd2_data2[0x238D], 31, (int)dword_53C03, -31)`（同 `scene_core.c`）。
- **`buf` 短名**：`0x2986F` 的 `buf` 是全局 `dword_53A61`（非局部），直接引用该宏。
- **返回值**：`0x2968D` 尾返回 `guest_free` 残值（无调用点读），声明 `void`；`0x2986F` 机器 `int` 返回同为 `free` 残值，无调用方使用，声明 `void`。
- **chkstk**：`push 0x38/0x34; call 0x3702F` 不分配帧，丢弃。
- 无对拍器，标“已还原未验证”。

### 64) 队伍换人屏的重排叶子（本轮，1 函数）→ `src/game/party_roster.c/.h`（追加）

§27 曾把换人屏驱动器 `0x2AF28` 的两个名册助手 `0x2B439`/`0x2B843` 拉出；本轮取该屏最后一个闭包就绪 callee
`0x2B777`（其唯一在册 callee `sub_2968D` 实为伪依赖，见下），并入同模块 `party_roster`：

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x2B777` | `party_roster_apply_selection(mask)` | 把队伍表二 `dword_53BF7`(0xA00) 拷贝到 `guest_malloc` 暂存后**稳定划分**：槽 `1..dword_53BFB-1` 中，掩码字节 `selected[0..dword_53BFB-2]`（第 i 字节对应槽 i+1）非零者按原序先写回，再写未选中者；槽 0（表头）不动；末 `guest_free` 释放。 |

**�底锅 / 伪代码伪影**：
- **伪 callee（共享尾声）**：`function-map.csv` 把 `sub_2968D` 记为其 callee，但尾部 `push edi; jmp 0x29862` 落
  在 `0x2968D` 体内（`0x29861` 起是 `call free(0x3776E); add esp,4; pop ebp/edi/esi/ebx; ret` 的共享尾声）；
  真语义为 `free(tmp); return`，真依赖只有 CRT `malloc`(0x3706E)/`memmove`(0x3771C)/`free`(0x3776E)。
- **堆缝**：机器 `0x3706E`/`0x3776E` 走 `guest_malloc`/`guest_free`（与同模块 `0x2B843` 一致）。
- **chkstk**：`push 0x20; call 0x3702F` 不分配帧，丢弃。
- 无对拍器，标“已还原未验证”。

### 65) 地点菜单首选项商店驱动（本轮，1 函数）→ `src/game/shop_screen.c/.h`（追加）

§32 留下商店屏族的下一层调用者（`0x2872B`/`0x28CBD`/`0x28F65`/`0x2A43E`…），§45 已收 `0x28CBD/0x28EFE`，
§57/58 收 `0x28F65` 相关预览屏；本轮取 `0x279BC` 四个菜单选项里最后一个闭包就绪的驱动器 `0x2872B`
（其全部真实 callee 均已还原）并入同族 `shop_screen`：

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x2872B` | `shop_place_buy_run(count, items)` | 地点菜单第一项的“按店存货购买”屏：把 `count` 个物品 id（`0x26A0D` 填的字节）交给 `shop_items_paint`/`shop_items_run` 交互列表；确认项在 23 字节物品表 `0x602AD` 里取描述（类字节 `+0`、价格 word `+19`→`dword_53AE1`）；类字节 `<0x20` 时用 `box_item_usable` 筛可接收该物品的队员，否则全体队员入围；玩家选择（`shop_party_stat_run` 或全体时 `menu_list_select`）后，若队员有空槽且 `dword_53BF3 >= dword_53AE1`，则 `rec_slot_claim` 占槽、`unit_metrics` 合算、`place_transition`+`place_money_spend` 扣款；取消返回 0。 |

**陷阱 / 伪代码伪影**：
- **`[esp+N]` 位移随 push 变化**：伪代码把 `push DWORD PTR [esp+0x74]` 的两个参数位读成 `a1`/`a2`；
  实际两条调用前已各压 2 个值，`[esp+0x74]` 落到帧基 `+0x6C`=`a1`（物品数），`ebp=[esp+0x70]`=`a2`（物品指针）。
  故 `shop_items_*` 实收 `(count,items,0)`，与机器 `push 0; push ebp; push [esp+0x74]` 一致。
- **四张 per-place 标签字表**：`0x52428/0x52434/0x52440/0x5244C`（各 3 dword）拷进局部 20-dword 暂存块，
  成员表在头部字节、四表落在 `+0x20/+0x2c/+0x38/+0x44`；列由 `dword_53F4A`（0..4）选字（`*((int16_t*)&v12[k]+place)`）。
  物品表 `0x602AD` 用已还原的 `tbl_ptr(&fd2_data2[0x2AD],23,item,0)`（与 shop_rows 同基址/步长）。
- **伪 callee**：`function-map.csv` 未误挂额外依赖；`0x4E8BC` 即该 23 字节物品表访问器。
- **返回值**：机器唯一出口 `0x28B39`（`add esp,0x58; pop ebp/edi/esi/ebx; ret`）的 EAX 系前一条 `msg_close_portrait`（void）
  残留，调用点 `sub_279BC` 不读取，C 侧取消时返回 0。
- **chkstk**：`push 0x90; call 0x3702F` 不分配帧，丢弃。
- 无对拍器，标“已还原未验证”。

### 66) 地图缩放过渡驱动器 `0x2000A` → `map_zoom_run`（本轮，1 函数）→ `src/game/map_zoom.c/.h`（追加）

闭包就绪游戏头名里，游戏内容项为 `0x2000A`(u1)/`0x1BBDC`(u1)/`0x190AC`(u1)（CRT 包装 `0x3645D/2E95B/365DA/36C56` 按既定策略跳过，`0x2EBE1` 2291B 超预算）。本轮取 `0x2000A`：它是既有、注释里点名待还原的 `map_zoom` 模块驱动器，且是**已还原** `scene_event_tick`(0x117E7) 的直接 callee（`'I'`/`';'` 键进入），真闭包＝自身（`map_shape_lib_decode`(0x1399C)、`map_zoom_draw`(0x1F558)、`ev5_1F6EF`、`map_cell_info`(0x12E38)、`kbd_flush`(0x4E381)、`kbd_pending`(0x10620)、`map_view_update`(0x11CAC)、`int386` 全在册）。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x2000A` | `map_zoom_run` | 解码 0x53A5D 形状库（`map_shape_lib_decode`，先存旧指针）→ 建 64×64 源基址表（`dword_53A5D+6+576*cell[0]`）→ 7 帧放大（中心因子 `(k+1)/8`、缩放 `k*zoom_w/7+128`，`zoom_w`=640，`dword_53AC5>40` 时 scale=3/`zoom_w`=896）→ `kbd_flush` 后忙等到有键：每 ≥2 BIOS tick 画全部未标记记录（跳过 `+5&1`/`+7==0x79`/`+31==10`）与玩家格（`base[rec[6]]`／常量 24 加相位 `phase` 0..7 往复）并 `memmove(dword_53A49→0xA0000,0xFA00)` → `int386(0x16,…)` 取键 → 6 帧缩小（因子 `(m+1)/8`）→ 释放暂存、还原 `dword_53A5D`、`map_view_update(0)`，返回 0 |

**陷阱 / 伪代码伪影**：
- **未初始化 tick 基线**：机器 `last_tick`（栈 `[esp+0x30]`）在首次 `tick - last_tick` 前从未初始化（离线反汇编核实，仅 0x202de 在首次渲染后写回）；首个渲染分支因此依赖栈残值。按既有约定（`map_wait_key` 0x12DAC 的同一模式）初始化为 0 并注释。
- **放大/缩小的中心-缩放因子错位**：两环的机器码都在 `idiv` 得到缩放后 `inc`，中心用自增后的计数器而缩放用自增前的（Hex-Rays 逐字呈现 `v11=v4*v17/7+128; ++v4; sub_1F558(v20+((v4*…)))`）——C 侧按机器写 `(k+1)` 中心 / `k` 缩放。
- **`>>3` 截断有符号除 8**：`sbb eax,edx; sar eax,3`（`__CFSHL__` 伪影）= 截断的 `/8`；`idiv 7` = `/7`。均按机器用普通 `/`（有符号 int）。
- **`qword_53AA9`→两个 dword**：`v20/v19` 分别读 `0x53AA9`（低 dword，globals 里 `dword_53AA9` 是 uint64_t）与 `0x53AAD`（`dword_53AAD`）；`a1`/`a2` 的 `dword_53AB1`/`dword_53AB5` 同理（Hex-Rays 合并成 `qword_53AA9`/`qword_53AB1`）。C 侧用 `(int)dword_53AA9` 与独立 `dword_53AAD` 取低/高 dword。
- **chkstk / 堆缝**：`push 0x60; call 0x3702F` 丢弃；`malloc(0x4000)`/`free` 走 `guest_malloc`/`guest_free`。
- **返回值**：机器尾 = `map_view_update` 的 eax 残值，唯一 caller `0x117E7` 不读（`scene_core.c` 直接 `sub_2000A(); return 0;`），C 侧返回 0。
- 无对拍器，标“已还原未验证”。

### 67) 地图格“特殊事件”处理器 `0x190AC` → `map_cell_event`（本轮，1 函数）→ `src/game/map_event.c/.h`（新建）

闭包就绪游戏头名（按 usage 排序：`0x3645D`(u4)/`0x2E95B`(u3)/`0x2EBE1`(u3，2291B 超预算)/`0x365DA`(u2)
均为 CRT/库包装，按既定策略跳过）中，第一个游戏内容项是 `0x190AC`(u1，1215B)——记录交互驱动器
`0x18D8C`（分支 `dword_53C57>=3`）的 `else` 动作处理器：读光标格（`qword_53AB1`）的 `map_cell_info`
结果（格 16 位标志 = `cell[6]&0x1F`、tile 信息字节 `t[0]`），对未处理过的格执行对话/消息序列后，按
`dword_53A55` 的 3 字节/格事件表（kind、word 值）施加效果：

| kind | 行为 |
|---|---|
| `0` | 设 `dword_53AD9 = value+181` → 两次 `vm_run` → `rec_slot_claim`；成功置 `dword_53AD5[flag]=1`，失败则再走一遍消息 + `box_popup`/`rec_field_byte`/`rec_slot_remove`/`rec_slot_claim` 并回写事件字 |
| `1` | 有值则 `dword_53AE1 = value`，`vm_run` 后 `dword_53BF3 += dword_53AE1`，置 `dword_53AD5[flag]=1` |
| 其他 | `j___delay(200)` + `ev4_196CB()` 后 `funcs_1199C[value](rec_index)` 分派 |

八段 `vm_run` 的 `sub` 由 tile 字节 `&0x20` 选择（421/428、422/429、423、424、425、426/430、427/431、
0x19C），`stream=dword_53A7D`、`addr=0xA9F23`（首次）/`0xAB6E3`（后续），其余 7 参数固定。

**陷阱 / 伪代码伪影**：
- **巨大 callee 列表 = 表展开，非直接调用**：`function-map.csv` 把 `0x190AC` 的 callee 列成 ~100 个
  `sub_34xxx`（`ev3_*`/`ev6_*`）——那是尾部分派 `funcs_1199C[value]`（`call [eax*4+0x51B91]`）的
  表展开；真实直接调用只有 `map_cell_info/vm_run/msg_open_portrait/dlg_blit_dato/dlg_wait_key/
  ev4_196CB/ev4_12263/map_window_run/box_popup/rec_*` 与 `j___delay`。
- **`SHIWORD(v6)` = `out+2`**：Hex-Rays 把 `map_cell_info` 输出的 uint16 标志当 `int` 的高 16 位读；
  机器是 `movsx edi, WORD [esp+2]`，C 侧按两字节拼合并 `(int16_t)` 符号扩展还原。
- **`p` = `dword_53A55`**：Hex-Rays 的 `p` 是全局指针 `dword_53A55`（每格 3 字节事件表），
  与并行“已处理”数组 `dword_53AD5` 一并按 `(uintptr_t)` 解引用（`scenario_load.c`/`ev2.c` 既有约定）。
- **寄存器复用/变量混淆**：伪代码把 `v9`/`v8` 混作一谈；机器上 `[esp+0xc]`=`map_window_run()` 结果
  （用于 `dword_53ADD = r+181`），`[esp+8]`=`rec_field_byte()` 结果（回写事件字），C 侧分开。
- **共享 epilogue / 分支归并**：`0x1951b` 是本函数自身的 epilogue；`0x19523` 段（delay100 + vm_run
  `0x19C` + delay200 + `ev4_196CB`）是外层 `r!=1 || dword_53C57!=0` 的提前返回路径。
- **chkstk**：`push 0x4C; call 0x3702F` 不分配帧，丢弃。
- 返回值：各路径 eax 残留调用结果，唯一 caller `0x18D8C` 不读，C 侧一律返回 0。
- 无对拍器，标“已还原未验证”。

### 68) ANI.DAT 容器播放器 `0x20421` → `ani_play`（本轮，1 函数）→ `src/game/ani.c/.h`（追加）

闭包就绪游戏头名（`0x3645D`(u4)/`0x2E95B`(u3)/`0x2EBE1`(u3，2291B 超预算)/`0x365DA`(u2)
均为 CRT/库包装，按既定策略跳过）中，第一个游戏内容项是 §60 建 `ani` 时留下的 ANI.DAT 装载器
`0x20421`(u5，403B)：`ani_play(index, delay_ms, wait_key)` 打开 ANI.DAT，定位第 `index` 个容器
（目录项位于 `4*index+6`），读 8 字节目录项取数据偏移，再读 0xAD 字节容器头，帧数 = 头 `+0xA5` 处的
带符号字；逐帧读 8 字节 `{数据长度, 命令数}` 头、读 `数据长度` 字节后 `ani_run_frame(命令数, 数据)`
解码到 `0xA0000`；`index==1` 且首帧时从 FDOTHER.DAT 载入采样 78 并 `svc_play_sfx`（片头音）；
每帧 `j___delay(delay_ms)`，`wait_key` 非零时按键（`kbd_pending`）提前结束；每帧头/尾 `kbd_flush`。
调色板 0x300 与暂存 0xFA00 走 game 堆（`guest_malloc/guest_free`），函数结束释放（含提前 break 路径）。

**陷阱 / 伪代码伪影**：
- **`HIWORD/LOWORD` 伪影**：Hex-Rays 把帧数读成 `HIWORD(v5)=fread(...)>>16; LOWORD(v5)=*(WORD*)(v3+165)`，
  机器实为 `mov ax,[esi+0xa5]; mov [esp+0x10],eax`（后续 `movsx`）——C 侧直接 `*(int16_t*)(scratch+0xA5)`。
- **`kbd_flush` 先于分配**：机器先 `kbd_flush` 再 malloc 两缓冲区；C 侧把分配放在 flush/资源载入之后以保顺序。
- **`a3 != 0 && kbd_pending()` 才 break**：`wait_key==0` 时永不因键中断（`kbd_pending`=0x10620、`kbd_flush`=0x4E381）。
- **末尾 `jmp 0x19518`**：共享尾声，即 plain return；caller 不读返回值，C 返回 0。
- **chkstk**：`push 0x38; call 0x3702F` 丢弃。
- 无对拍器，标“已还原未验证”。

### 69) 片头 ANI 动画播放器 `0x1F81E` → `title_ani_play`（本轮，1 函数）→ `src/game/title_screen.c/.h`（追加）

闭包就绪游戏头名中，`0x3645D`/`0x2E95B`/`0x365DA`/`0x36C56` 为 CRT/库包装（既定策略跳过），
`0x2EBE1`(2291B) 超预算；其后第一个游戏内容项是 §60/§68 建 `ani`/`title_screen` 时暴露的
片头序列播放器 `0x1F81E`(u3，100B)：`title_ani_play(index, delay_ms, palette_index)` 在
`palette_index != -1` 时清 `0xA0000`（0xFA00 字节）并把 FDOTHER.DAT 条目 `palette_index` 重载进共享调色板
（`dword_53A65`），随后 `pal_fade_range(0,255,0)` 复位减色量、`ani_play(index, delay_ms, 0)` 播放动画，
末尾经共享尾块 `0x1F882`（`pal_fade_out`）淡出。三个调用点（`0x1F73F` 内与 `0x1F894` 待还原编排者内）
传参 `(0,15,0)`/`(4,90,99)`/`(5,50,0)`/`(6,90,99)`/`(8,90,99)`。

**陷阱 / 伪代码伪影**：
- **尾部 fallthrough 而非 `call`**：`0x1F81E` 的 extent 恰止于 `0x1F882`（函数体无 `ret`），机器直接落入
  共享减色块 `0x1F51E`（入口 `xor ebx,ebx; jmp 0x1F51E`，即 `ebx=0..63` 递增减色）；C 侧按语义写
  `pal_fade_out()`（已验证资产 `0x1F882`），不模拟跳转。`0x1F51E`/`0x1F503` 本身是未登记函数的共享代码块。
- **Hex-Rays 参数错标**：伪代码把 `memset` 首参写成常量 655360 并误列 `a1/a2/a3` 次序；离线反汇编核实为
  `memset(0xA0000, 0, 0xFA00)`、`res_load(FDOTHER.DAT, dword_53A65, a3)`、`pal_fade_range(0,255,0)`、
  `ani_play(a1, a2, 0)`（`ani_play` 的 wait_key 恒 0）。
- **chkstk**：`push 0x10; call 0x3702F` 丢弃。
- 无对拍器，标“已还原未验证”。

### 70) 地图逐记录交互动作驱动器 `0x1BBDC` → `map_action_interact`（本轮，1 函数）→ `src/game/map_action_ui.c/.h`（新建）

闭包就绪游戏头名中，本轮取 caller 唯一、callee 全已还原的 `0x1BBDC`(u1，1058B)——由记录交互驱动器
`0x18D8C` 调用的“对记录执行动作”菜单（`map_cursor.h` 记的五条 4×4 朝向补丁驱动器之一）：
先复制两张朝向 tile 表（`0x51F05`/`0x53F32`，dseg02）→ 开补丁选朝向（`map_cursor_patch_expand/`
`map_cursor_dir_select`/`collapse`）→ 按动作槽 `dword_53C57` 分支：
- **槽 0**：循环 `box_popup(actor,1)` 弹物品/状态框 → 取持物条目（表 `0x4E8BC` base 0x602AD stride 23）的
  字段做两次 `map_reveal_reachable` 洪泛 + `map_pick_record` 选目标；kind 23 时额外要求记录 `[8]==24` 且
  `word[+0x46]>=20`、再确认格，随后交 `map_action_router` 并 `rec_flag_or80(actor)`；命中返回 1，否则 0。
- **槽 1**：队伍换位——`guest_malloc(0x64)` 暂存选人结果，`dlg_portrait_find` 找肖像，`map_slide_view`
  滑回 `v20`（循环前锁存的 `qword_53AB1`），再 `rec_slot_claim/remove` 拼接两人的装备槽；置 `dword_53C53=1`。
- **槽 2**：委派 `box_run(actor)`（**不**调 `unit_metrics`，因机器复用了槽 1 尾声的 `add esp,4` 槽位）。
- **其他**：仅弹框后 `rec_slot_remove` 掉一格，落到共享尾声 `0x1BFF6`（`unit_metrics; return 0`）。

**陷阱 / 伪代码伪影**：
- Watcom 栈探针 `push 0x94; call 0x3702F` 丢弃；两个 16 字节局部是 `rep movs` 拷贝（Hex-Rays 记作
  `v16=unk_51F05`/`v17=unk_53F32`）；`1bc58` 的 `v17.dword1=1` = 首次洪泛无格时置标志。
- Hex-Rays 把记录字段字节记成 `char`，但机器向 `rec_slot_claim` 压的是 movzx 后的 int（0..255），C 侧按 int。
- `v20`（`saved`）是循环前锁存的 `qword_53AB1`，仅 `map_slide_view` 用它；两次洪泛仍读实时 `qword_53AB1`
  （机器每次重新 `mov ds:0x53ab1/ab5`）。
- 无对拍器，标“已还原未验证”。

### 71) 全屏逐帧动画播放器 `0x24336` → `scene_cutscene_play`（本轮，1 函数）→ `src/game/scene_cutscene.c/.h`（新建）

闭包就绪游戏头名中，`0x3645D`/`0x2E95B`/`0x365DA`/`0x36C56` 为 CRT/库包装（既定策略跳过）、`0x2EBE1`(2291B)
与 `0x1F894`(1765B) 超预算、`0x1B1E7`(内层读未初始化 ebp) 暂跳；其后第一个游戏内容项 `0x24336`(u1，384B)
是**已还原** `scene_state_20`（funcs_25E23[20]，0x240FA）的唯一剩余待还原 callee，故整函数闭合、单独成批。
它把 FDOTHER.DAT 条目 34 的 101 帧动画铺满 VGA：
- 先 `ev2_135DD(14,8)` 滚视图；`guest_malloc(0xFA00)` 存当前屏幕、`res_blit(图,0,backdrop)` 作底图；
- 帧 1..68：每帧把底图拷进 `dword_53A49`、`res_blit(图,i,dword_53A49,320,-1)`、拷上屏、`pal_anim_step()`、
  `svc_wait_ticks(3)`、`kbd_flush()`；
- `ani_play(0,15,0)` → `pal_fade_add(0,255,63)` → `j___delay(100)` → `pal_fade_add(0,255,0)` → `j___delay(500)`；
- 帧 69..100：与上一循环相同的逐帧合成（无 DAC 动画）；
- 释放两缓冲；共享尾声 `0x22BB7`（`map_view_update(0)` + 寄存器复原 + ret）。

**陷阱 / 伪代码伪影**：
- 尾部 `jmp 0x22BB7` 是**共享尾声**（`map_view_update(0)`），Hex-Rays 记为 JUMPOUT 并**丢掉该刷新**；C 侧按语义补回。
- Watcom 栈探针 `push 0x28; call 0x3702F` 丢弃；`malloc/memmove/free` 是 CRT 入口 0x3706E/0x3771C/0x3776E，
  走游戏堆 ⇒ C 侧 `guest_malloc/guest_free`。
- 循环计数 `i` 是**同一个寄存器跨两循环**（1..68 再 69..100），故帧 68 只 blit 一次；`pal_anim_step` 仅首循环调用。
- `function-map.csv` 的 callee 列含一个伪 callee `sub_22AF6`（机器码中无此调用），以离线反汇编为准，不入本批。
- 无对拍器，标“已还原未验证”。

### 72) 地图队伍状态弹出面板编排者 `0x1B1E7` → `party_panel_run`（本轮，1 函数）→ `src/game/party_panel.c/.h`（追加，party_panel 家族自顶闭合）

§12 建 `party_panel` 时留下的编排者 `0x1B1E7`(u1，566B) 是本轮闭包就绪游戏头名——`0x19DF7`
（记录交互驱动器）调用的地图队伍面板。此前因“内层读未初始化 `ebp`”暂跳；本轮据 playbook
第 6 条“未初始化 tick 基线寄存器”先例（`map_wait_key` 0x12DAC / `map_zoom_run` 0x2000A）
把基线建模为 0 并注释，逐指令核后拉出。真闭包＝自身（callee 全在册）。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x1B1E7` | `party_panel_run()` | `guest_malloc` 两块 0xFA00（backdrop/scratch）→ `memset(backdrop,0,0xFA00)` → `map_render_view(dword_53A49+0x8088,456,13,8,ox,oy)` → `dlg_portraits_refresh` → `gfx_copy_rows(backdrop+0x504,320,视图,456,312,192)` → `party_panel_draw(scratch,320)` → 滑入 12 步（每步 `memmove(dword_53A49,backdrop)` + `party_panel_band_b/c/d/e(scratch,i)` + `memmove(0xA0000,dword_53A49)`）→ 循环：`key=kbd_pending()`；tick ≠ 基线时 `anim_frame_step()`+`pal_anim_step()`+重绘地图视图+`dlg_portraits_refresh()`+`party_panel_draw(dword_53A49+0x7964,456)`+`gfx_copy_rows(0xA0504,320,视图,456,312,192)`；`key!=0` 时 `kbd_flush` → 滑出 12 步（`band_a/c/d/e`，步序 j=11..0）→ `memmove(0xA0000,backdrop)` → `guest_free`×2 → `kbd_flush` → 返回。 |

**陷阱 / 伪代码伪影**：
- **未初始化 `ebp`（关键，按 playbook 第 6 条定性）**：`0x1B2FE` 的 `cmp eax,ebp`（eax=`movsx word[0x46C]` BIOS tick）
  读的 `ebp` 全函数未写入、未保存，且**从不回写**基线——即原机每趟都重绘（无节流）。C 侧 `baseline=0` 并注释。
- **伪 callee（共享尾声）**：尾 `jmp 0x10C49` 落在 `unit_sprites_build`(0x10B4E) 体内的共享尾声
  （`add esp,4; pop edi; pop esi; pop ebx; ret`，与本函数前言 `push ebx/esi/edi; sub esp,4` 同帧）
  ⇒ 普通 `return`；`function-map` 因此误列 `sub_10B4E`，本批不入依赖。
- **`sub_4E31C` 实为 0 参**：Hex-Rays 把 `anim_frame_step()` 的 eax 残值当 `pal_anim_step` 的实参
  `sub_4E31C(a1: v4)`；机器 `call 0x1297d; call 0x4e31c` 两次均无 `push`，故按 0 参调用（§0-K 已记）。
- **Hex-Rays 别名**：key 字节（机器 `[esp]`）被渲染成 `retaddr`/`[esp+10h]` 并加 `LOBYTE()`/`(_BYTE)`；
  实为独立局部 `key`（机器 `mov [esp],al` 后 `movzx eax,[esp]`）。
- **chkstk**：`push 0x2c; call 0x3702F` 不分配帧，丢弃；`malloc/free`(0x3706E/0x3776E) 走 `guest_malloc/guest_free`。
- **归属**：追加至既有 `party_panel`（§12）；新增函数用语义名调用各 band/draw，无需别名；
  唯一 caller `0x19DF7` 仍待还原。无对拍器，标“已还原未验证”。

### 73) 随机粒子放置助手 `0x21DB2` → `particle_spawn`（本轮，1 函数）→ `src/game/particle.c/.h`（新建）

§71/§72 遗留的闭包就绪游戏头名 `0x21DB2`(u0，255B)：烟柱动画 `0x21BD0`（其唯一 caller，仍待还原）
的逐槽发射器。真闭包＝自身（`cos`/`sin`＝AIL 边界 `0x3CBD5`/`0x3CBE8`，`util_rand` `0x4EBE3` 已还原）。逐指令核过离线反汇编。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x21DB2` | `particle_spawn(radius,index,x[],y[],kind[],cx,cy)` | 依次取 3 个 `util_rand`：`r=((rand%64)*radius)/64-1`（有符号 `idiv`）→ `angle=(double)(rand%360)*dbl_501F8`（度→弧度；`dbl_501F8`=`0.0174532`）→ `x[index]=(int)(cos(angle)*r+cx)`、`y[index]=(int)(sin(angle)*r+cy+dbl_50200)`（`dbl_50200`=`-8.0`）→ `kind[index]=rand%8+1`（亦为返回值）。 |

**陷阱 / 伪代码伪影**：
- **ABI 实为 7 参**：Hex-Rays 仅列 5 参（`char __cdecl(int, unsigned __int8, int, int, int)`）；两处调用点
  （`0x21C72`/`0x21D9D`，均 `add esp,0x1c`）证明压 7 参：`radius, index, x*, y*, kind*, cx, cy`——中心坐标 arg6/arg7
  在机器为 `[esp+0x44]`/`[esp+0x48]`，被 `sar edx,0x1f`/`idiv` 序列掩盖，故 IDA 漏记两参。
- **`__CHP()` → `(int)`**：`call 0x37AF4` 同 fx/map_action/unit_load 先例，退化为截断强转。
- **`cos`/`sin` 为 AIL 边界**（`__int64 __cdecl(__int64)`），按 `fx_dots5`(0x2C67D) 先例改用 `<math.h>`。
- **常数走数据宏**：`dbl_501F8`(0x501F8)/`dbl_50200`(0x50200) 直接用 `fd2_globals.h` 宏（读原始数据段）。
- **归属**：新建 `particle`（语义名：随机偏移粒子放置）；唯一 caller `0x21BD0` 仍待还原。无对拍器，标“已还原未验证”。
- **台账缺口（新记）**：`0x21BD0`（481B，烟柱动画，`push 0x13c; call 0x3702F` 前言）是真实函数，却不在
  `function-map.csv` 中（IDA 未登记）；本轮未纳入（非 pending），留记待台账修复后补入。

### 74) toollib 叶助手补登台账 `0x4E42C/4E4BE/4E5CC/4E680/4E703/4E71F/4E751`、`0x4EC66`、`0x4ECA4/4ECF0/4ED4F`（本轮，11 函数，仅台账登记，无源码改动）

§73 遗留的闭包就绪游戏内容项（`0x4E4BE/4E703/4E71F/4E751/4EC66/4ECA4/4ECF0/4ED4F`）经核对
**早已随 §0-G 迁入的 `path.c`/`rle2.c`/`gfx.c` 一并源码化**，只是作为 `static` 助手未登记入
`src/restored_manifest.txt`，故 `function-map.csv` 仍标“待还原”。本批按 §26 `0x2BF83` 的台账修复先例，
**只补登 manifest 并重跑生成器**，不改任何模块源（本批 >6 函数，属“同模块登记”而非翻译，故不适用
1..6 翻译簇上限）。逐项对照离线反汇编确证实现一致：

| 原址 | 现有实现 | 说明 |
|---|---|---|
| `0x4E42C` | `path.c: rec1` | 一趟 DFS 递归（右/左/下/上） |
| `0x4E4BE` | `path.c: check1` | 一趟可达性/剩余点数更新 |
| `0x4E5CC` | `path.c: rec2` | 二趟递归（记录方向+深度） |
| `0x4E680` | `path.c: check2` | 二趟检查（转向数平局） |
| `0x4E703` | `path.c: mark_target` | 模式 2 标目标 |
| `0x4E71F` | `path.c: count_dirs` | 转向计数 |
| `0x4E751` | `path.c: commit_path` | 提交最佳路径 |
| `0x4EC66` | `rle2.c: rle2_next` | 0xC0 段 RLE 记号解码器 |
| `0x4ECA4` | `gfx.c: gfx_restore_rect`（内联循环） | 紧凑记录→带步幅写回 |
| `0x4ECF0` | `gfx.c: gfx_save_rect`（内联循环） | 带步幅读入→紧凑记录 |
| `0x4ED4F` | `gfx.c: gfx_blit_transparent`（内联循环） | 0 透明 blit |

**陷阱 / 伪代码伪影**：
- **usercall 寄存器传参**：这些叶函数在机器里经寄存器（`ebx`/`esi`/`edi`/`cl`/`dx`/`ebp`）取参，
  Hex-Rays 伪代码把未初始化寄存器渲染成局部变量（如 `4E4BE` 的 `v1=ebx`、`4ECF0` 的 `v1=edi`）；
  现实现已按 `path.h`/`gfx.h` 文档的 ABI 化为普通形参，语义逐指令核对一致。
- **`0x4EC66` 为 0 参 usercall**（state 在 AH/AL + ESI 游标），现实现收敛为 `struct rle2_state`；
  `0x4ECA4/4ECF0/4ED4F` 原是无独立函数头的折叠助手（`path.c`/`gfx.c` 已注明“folded”）。
- **登记条目指向模块头**（`game/path.h`/`game/rle2.h`/`game/gfx.h`）；这三者本就是 `static` 助手、
  无外部原型，故不需改头文件。
- 无对拍器，标“已还原未验证”。

### 75) 开场/继续游戏屏 `0x1F894` → `title_screen_run`（本轮，1 函数，§5 大单函数条款）→ `src/game/title_screen.c/.h`（追加）

闭包就绪列表中已无 in-budget 游戏内容项（余下仅 CRT/库包装与伪函数条目），故按 §5 取一个
超预算单函数 `0x1F894`（1765B，≤2500B）：其**真闭包＝自身**（所有真 callee 均已还原/已验证，
见下），并逐指令核过离线反汇编 `1f894..1f78`。这是 `scenario_menu`（0x25EBB，已还原）唯一
调用点，返回值即主菜单选择，其全部 helper 在 §14/§69 已就绪（`title_screen` 家族至此闭合）。

| 项 | 要点 |
|---|---|
| 返回值 | `int`（ebp）：0=新游戏、1=读档、其余=存档，供 `scenario_menu` 分支 |
| 定场 | res_load FDOTHER 77(音效库)/76(调色板)/74(logo)；`rle_decode(74)` → `pal_fade_in` → 等 1+30 tick → `pal_fade_out`；FDOTHER 99 + `pal_fade_range(0,255,0)` + `ani_play(3,90,1)` |
| logo 缓冲 | `guest_malloc(0x396C0)`（735 行×320）零填充；5 个子图 FDOTHER 69..73 `rle_decode(...,147*i,...,320,-1)` 拼入；`kbd_flush`；旧 `dword_53A45` 释放后 `guest_malloc(0xA0)` |
| 滚动主环 | `j=535..0`：每帧 `gfx_copy_rows(VGA,320,strip+320*j,320,320,200)`（j==535 时 `pal_fade_in`） |
| 定时插帧 | j==25/110/210/330 → `title_ani_play`（(0,15,0)/(8,90,99)/(6,90,99)+(7,50,0)/(4,90,99)+(5,50,0)）；j==10/450 → `title_frame_show(75,76,strip,10)`/`(100,99,strip,450)`；随后共享尾：再 copy_rows + `res_load(101)` + `pal_fade_in` |
| 步进表 | `0x5204E` 的 15×dword `{520,430,410,340,310,300,240,180,150,130,110,87,64,22,1000}`；`j==table[mark]` 时 `frame=0`+`svc_play_sfx(snd,0,1)`+`res_load(102)`+`pal_fade_range(0,255,0)`+`++mark`；`frame==11` 时重载 FDOTHER 101；每帧 `++frame`、`j___delay(30)`、j==0 再 `j___delay(1000)`；`kbd_pending()` 非 0 跳出 |
| 收场 | 41 帧 `pal_blend_to(0,255,k,63,0,0)` 渐黑 → `j___delay(100)` → `kbd_flush` → 释放 strip/logo → 重载 FDOTHER 7/8 → 清屏+`pal_fade_range` → `ani_play(1,15,1)` → `svc_play_sfx2(snd,3,1)` → `pal_fade_add(0,255,64)` → `res_blit6(VGA,320,img,0)` → 41 帧 `pal_blend_to(0,255,m,56,60,63)` → `kbd_flush` |
| 继续询问 | `fopen("FD2.SAV","rb")`：有存档则 `guest_malloc(0x59CB)` + `fread` + `fclose` + `util_deobfuscate(...,22987)`；`util_sum_tail4(...,22987)==*(u32*)(save+0x59C7)` 时 count=2，且 `save[0x30C5]!=0xFF` 时 count=3；`guest_free` |
| 选项环 | `title_options_draw(img,0,count)` → `while(!done)`：画 `(img,choice,count)` → INT 16h（全局 REGS `word_53A8D`，扫描码 `byte_53A8E`）：0x48 上/0x50 下（各 `svc_play_sfx(snd,2,1)`，环绕 0..count-1）、0x0D/0x20/0xE0/0x52 确认（`svc_play_sfx(snd,1,1)`）→ 4×（画 -1 + `j___delay(80)` + 画 choice + `j___delay(80)`）→ `pal_fade_out` → 清屏 → 释放 img → `svc_play_sfx(snd,-1,1)` → 释放 snd → `return choice` |

**陷阱 / 伪代码伪影**：
- **0 参 ABI**：`fd2_funcs.h` 的 `sub_1F894(void)` 正确；Hex-Rays 的 `int __cdecl(int a1)` 及
  `analysis/decomp/1F894_sub_1F894.c` 里 `[esp+4]` 的读取是误判（函数内 `xor ebp,ebp`，ebp 不是帧指针，
  故 Hex-Rays 把 esp 局部错标为 `[ebp+0x40..0x58]` 形参）；调用点 `0x25EC8` 无压栈、无 `add esp`。
- **伪 callee（共享尾声）**：尾 `jmp 0x13994`（`mov eax,ebp` 后）落在 `sub_1366A` 体内
  （0x1366A+818 尾段）⇒ 普通 `return`；`function-map` 因此误列 `sub_1366A`（及其 callee），真依赖不含它。
- **`*(&retaddr + v27)`**：Hex-Rays 把栈上 15×dword 表 `0x5204E` 误标成 `retaddr` 邻域；机器是
  `movzx eax,[esp+0x58]; cmp esi,[esp+eax*4]` ⇒ 即 `marks[mark]==j`。
- **`&loc_396C0`** 是 `malloc(0x396C0)`/`memset(...,0x396C0)` 的立即数（非地址）；`0x1F400`(0xFA00)、
  `0x59CB`、`0x59C7`、`0x30C5` 均为立即数，已逐条核对。
- **堆缝**：`0x3706E`/`0x3776E` → `guest_malloc`/`guest_free`（logo 缓冲、记录表、FD2.SAV 缓冲）；
  CRT `fopen` 0x37324 / `fread` 0x373CA / `fclose` 0x3759C 用 libc（同 scene_core）。
- **`dword_53A65` 复用**：调色板指针被 `res_load` 反复改写（同 scene_core 既有约定）；
  INT 16h 用的是**全局** REGS 块 `word_53A8D`（in/out 同址），非 password.c 的局部块。
- **`svc_play_sfx`/`svc_play_sfx2` 的 bank 是 FDOTHER 77**（本函数自持 `snd`），非 main 的 dword_53EEC。
- 无对拍器，标“已还原未验证”。

### 76) 攻击动画驱动器 `0x2EBE1` → `figure_attack_play`（本轮，1 函数，§5 大单函数条款）→ `src/game/figure_view.c/.h`（追加）

§75 记“下批依 §5 条款取 `0x2EBE1`”。闭包就绪游戏内容项已尽（余下仅 CRT/库包装与伪函数条目），
故取此超预算单函数 `0x2EBE1`（2291B，≤2500B）：其**真闭包＝自身**——所有真 callee 均已还原/已验证
（`figure_bg_scroll`/`figure_bg_scroll_portrait`/`map_attack_result`/`res_blit`/`gfx_copy_rows`/`svc_play_sfx`/
`svc_wait_ticks`/`ui_status_panel_at`/`util_rand` 及 CRT `memset`/`outp`/`j___delay`），并逐指令核过
离线反汇编 `2ebe1..2f4d3`（578 行）。它是 `sub_2E2B0`（地图攻击视图，待还原）调用的同族
figure 播放器（`0x2FB2C`=item_menu_figure_play 的姊妹），故追加至既有 `figure_view`。

| 项 | 要点 |
|---|---|
| 签名/ABI | `int figure_attack_play(int attacker, int defender, const uint8_t *figure, const uint8_t *base, uint8_t *work, uint8_t *screen, const void *portrait, int snd)`；8 参 cdecl（调用点 `0x2E6DE/2E724/2E75B` 均 `add esp,0x20`）。arg1/2＝攻/守记录号（索引 `dword_53A45`＋80），arg3＝攻方 FIGANI 组，arg4＝守方组，arg5＝640 步幅工作缓冲（`memset 0x1F400`），arg6＝320 步幅屏缓，arg7＝TAI 肖像条，arg8＝FDOTHER 音效 bank（调用点压 `ds:0x5414B`） |
| 外环 | `v27` 起始 1；`util_rand()%100<3` 或攻击结果 `out[4]!=0`（仅首次）时增 1（即重播 2 次）；`v30==0` 强制清零退出；`for(;;){ v11=v27--; if(v11==0) break; ... }` 取自旋反（do-while 判据在顶部） |
| 攻击结算 | `dword_54133==0` 时快照守方 HP 字（+0x40）→ `v28`，调 `map_attack_result(attacker,defender,out[6])`；否则 `out[0..5]=0` |
| 第一趟（`figure[1]!=0`） | `memset(work,0,0x1F400)`；`v12<figure[2]`：每子体（`figure[4*i+8]` 偏移）播 `sub[5]`、`gfx_copy_rows(work+320,640,screen,…)`→`res_blit(figure,v12,work+320,640,-1)`→拷到 VGA→`svc_wait_ticks(sub[6])`；收尾按 `rec_att[6]` 选 `figure_bg_scroll(defender,base,screen,work,dword_5413B)` 或 `figure_bg_scroll_portrait(defender,base,portrait,screen,work,dword_5413B)`（另一分支 work 直用、偏移 +320） |
| 主趟 | `v12>=figure[0]` 收尾：`v30==0⇒v27=0`；`v27!=0 && figure[1]==1` 时 `memset(work,0,0x1F400)`＋按 `rec_att[6]` 选 `figure_bg_scroll`/`figure_bg_scroll_portrait`（bg 用 `dword_54137`）。逐子体：`sub[4]!=0` 时 `++v33`、`v30=v28-out[5]*v33/v32`（有符号，钳 0）、写回 `rec_def+0x40`、`dword_54133==0` 时 `ui_status_panel_at(screen,defender)`，再按 `out[0]` 播 `svc_play_sfx(snd,0,1)` 或置 `v8=5`,`v37=33` 并播 `sub[5]`；否则 `sub[5]!=0` 直接播 |
| 逐帧（`j<sub[6]`） | `v38=strip[v8]`（`figure[1]!=0` 时 0）；`gfx_copy_rows(work+16040,400,screen,…)`；按 `rec_att[6]` 与 `sub[7]&1` 决定攻方 `figure` 与守方 `base` 的 stamp 顺序，守方落点＝`work+16040 ∓ off[v8] ∓ 400*v38`（平移表 `0x5266C`={0,4,9,14,18,14}，条纹表 `0x52684`={0,2,4,6,8,10}）；拷 VGA；`sub[4]==1 && v33==v32` 时按 `out[2]`（蓝 1/0x20）/`out[1]`（白 0x3F）闪 DAC＋`j___delay`；`base` 基帧计数 `counter==base_sub[6]` 进位、`base_frame==base[0]` 回绕；`v8` 递减、`v37=-1`、`svc_wait_ticks(1)` |
| 返回 | `dword_54133==1 && v33==v32` 时提前 `return 1`（供 `0x2E2B0` 分支）；否则返回最后一次 `v30` |

**陷阱 / 伪代码伪影**：
- **伪函数/伪 callee**：`sub_3702F`（Watcom chkstk，`push 0xB8; call`）不分配帧，非依赖；`0x37910`=memset、
  `0x3790A`=j___delay、`0x37AE5`=outp（CRT）；`dword_53A45` 表走 `(uint8_t *)dword_53A45 + 80*rec`。
- **未初始化局部快照**：`dword_54133!=0` 时不写 `v28`（机器 `[esp+0x50]` 亦未初始化），C 侧建模为 0 并注释。
- **Hex-Rays 形参错列**：伪代码给 `__int16 *a7`（实为 TAI 条 `const void *`）与 `unsigned __int8 *a4`（守方组）；
  机器 `[esp+0xac]`=a4、`[esp+0xb8]`=a7，逐条核对后按指针语义化。第一趟 loop bound 用 `figure[2]`、主趟用 `figure[0]`（两部同体共用 `v12`）。
- **`out` 块即 `map_attack_result` 的 6 dword 结果区**（`[esp+0x18..0x2C]`）；`out[0]`=命中、`out[1]`=暴击、`out[2]`=反击/效果、`out[4]`=效果 3、`out[5]`=伤害。
- **归属**：追加至既有 `figure_view`（§0-D/§0-F 已建，伞头已含）；唯一 caller `0x2E2B0` 仍待还原。无对拍器，标“已还原未验证”。

### 77) 换人屏（队伍变更）三角 `0x2AF28`/`0x2B4FB`/`0x2B67F`（本轮，3 函数）→ `src/game/party_roster.c/.h`（追加）

§76 记“下批按 §2 取最小传递待还原闭包”。闭包就绪列表已无 in-budget 游戏内容项（仅 CRT/库包装与伪函数），
故取**最小传递待还原闭包**的游戏内容簇——换人屏（队伍变更）三角：真闭包＝此三件（`sub_26152` 是伪 callee、
`sub_3702F` 是 chkstk），合计 1887B（略超 §1 的 ~1500B 指引，属 §2 的“整闭包拉出”）。逐指令核过离线反汇编
`2af28..2b748`。三者与既有 `party_roster`（换人屏的成员助手）同族，故追加至该模块。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x2AF28` | `party_change_run()` | 快照并合成面板 → 12 步滑入（`fb_slide_step`，i==11/5 播音效）→ 选择环（`party_change_wait_key`：Enter 切换所选、`ev5_2B749(selected)==count` 且 `party_roster_apply_selection` 时确认；Esc 取消；左/右/上/下 `4B/4D/48/50` 移动光标）→ 12 步滑出 → 若确认则释放 `dword_53A61`+重载 FDICON.B24+按 `dword_53C03` 分支做 `party_member_present`/`party_member_move_front`+招募肖像序列（`msg_open_portrait(75)`/`vm_run(658)`/`dlg_blit_dato(0)`/`map_window_run`/`map_draw_text_window`/`msg_close_portrait`）。**返回 1=确认，0=取消**（`map_window_run==-1` 或 `dword_53C57!=0` 也返回 0） |
| `0x2B4FB` | `party_change_draw(screen,count,selected,cursor)` | 把快照 `screen` 拷入 `dword_53C63` → 两位计数（`dlg_draw_number` @ `+0x2BFD`/`+0x5B7D`，后者值＝`count-ev5_2B749(selected)`）→ `anim_frame_step` → `box_status_draw(cursor+1,…)` → 光标精灵（bank `dword_53A4D`，`+*(u32*)(+6)`）→ 逐槽 i=0..`dword_53BFB-2` 画图标（bank `dword_53A61`，索引 `12*i+frame+12`，`frame=dword_53C0B` 且 3→1；选中用 `sprite24_plain`（落点 +960），否则 `sprite24_ramp24`） |
| `0x2B67F` | `party_change_wait_key(...)` | `while(!kbd_pending())`：BIOS tick（BDA 0x46C，经 `BDA_W` 镜像）变化时重绘并回写 VGA；随后 `byte_53A8E=0x10` + `int386(0x16,…)` 取键，E0/52/空格→1C、53→01，返回扫描码 |

**陷阱 / 伪代码伪影**：
- **伪 callee / 共享尾声**：机器把 `sub_2AF28` 的 callee 列为含 `sub_26152`（尾 `jmp 0x2B434` 是 `0x2B4FB` 的
  共享 `return`，落在 `0x2AF28` 体内）；`sub_3702F`＝Watcom chkstk（不分配帧）。真依赖＝本 3 函数 + 上列已还原资产。
- **重复调用**：`0x2B4FB` 连调 `ev5_2B749(selected)` 两次（首次结果丢弃），离线反汇编 `2b544/2b54d` 确证，C 侧照写。
- **双栈槽别名**：机器把屏幕拷贝指针分别存 `[esp+20h]`(`v12`) 与 `[esp+28h]`(`p`)，`[esp+24h]` 后被 `fopen` 句柄复用；
  C 侧建模为 `buf` + `fp`（与 Hex-Rays 局部布局一致）。
- **堆缝/资源**：`malloc/free`（`0x3706E/0x3776E`）→ `guest_malloc/guest_free`；DATO 暂存 `dword_53A61` 亦走 `guest_free`；
  `rle2_blit` 源＝`dword_53A81 + *(u32*)((u8*)dword_53A81+0x56/0x5A)`（与 `box_popup.c` 先例一致），再 `res_blit6(+0x8C5,137)`。
- **`dlg_blit_dato`** 按已验证 3 参 API 传 `(dword_53A85, dword_53C67, 0)`（同 `party_member_present`）。
- **字符串**：`fopen(aFdiconB24_5(0x50291), aRb_9(0x5028E))`＝`"FDICON.B24","rb"`。
- 无对拍器，标“已还原未验证”。

### 78) 地图攻击视图簇 `0x2E2B0`/`0x2E9A8`/`0x2E95B`（本轮，3 函数）→ `src/game/figure_view.c/.h`（追加）

§77 记“下批按 §2 取最小传递待还原闭包的游戏内容簇——地图攻击视图簇”。闭包就绪列表已无
in-budget 游戏内容项（仅 CRT/库包装与伪函数），故本轮取该簇：最小传递待还原闭包＝
`0x2E2B0`↔`0x2E9A8`（互依）＋`0x2E95B`（`0x2E2B0` 的唯一库包装 callee），合计 2287B
（1707+503+77，略超 §1 的 ~1500B 指引，属 §2 的“整闭包拉出”）。三者与既有 `figure_view`
（0x2EBE1 攻击动画驱动器为 `0x2E2B0` 的 callee、已在本模块）同族，故追加该模块。
逐指令核过离线反汇编 `2e2b0..2eb9e`（调用点参数逐一按 `[esp+..]` 反推）。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x2E2B0` | `figure_attack_view(attacker,defender)` | 依 `dword_54133` 反查/快照记录（`rec1=s[attacker]`、`rec2=s[defender]`，`rec1[6]!=0` 时交换 `rec2`）→ `map_cell_info` 取 TAI/BG 子图索引（`byte_52470[dword_53C03]`，`v6[32]==0x13‖v6[31]∈{4,5}` 且索引非 0 且 `v6[7]!=0x1c` 时保留）→ `res_load` TAI/FIGANI 四片（`3*row`/`3*row+1`）→ `pal_fade_out` → `res_load` BG → 状态面板 → `figure_attack_scroll` → 两次 `figure_attack_play`（主攻/反击）→ 未确认时再叠一层 → `guest_free` 全清 → 非 demo 时重载 `FD2.TMP`/`FD2SHAP.DAT`、`svc_wait_ticks(6)`、`pal_fade_out`、清 VGA、`map_view_update(1)`、停音效、`pal_fade_in` |
| `0x2E9A8` | `figure_attack_scroll(unit,flag,plate_a,plate_b,work,screen,portrait)` | `rec[6]==0`：`portrait` 先落 `screen`，i=8..0 每步 `gfx_copy_rows(work+320→screen)`、`plate_a` 自 `work+320-10*i` 滑入、`flag==0` 时 `plate_b` 定于 `work+320`、拷 VGA、`pal_fade_range(0,255,6*i)`；否则反角色（`plate_b` 定于 `work`、`plate_a` 自 `work+10*i` 滑出、`portrait` 落 `work+10*i`），末步再补一次 `portrait`→`screen` |
| `0x2E95B` | `figure_tmp_load()` | `fopen("FD2.TMP","rb")` → `guest_malloc(0x32A00)` 写全局 `dword_53A61` → 整文件 `fread` → `fclose`（返回 `fclose` 结果，调用者忽略） |

**陷阱 / 伪代码伪影**：
- **伪 callee / 共享尾声**：`sub_3702F`＝Watcom chkstk（`push 0x64/0x2C/0x18; call`，不分配帧）；`0x2E9A8` 的机器尾
  `jl 0x2E956` 落在 `0x2E2B0` 体内（共享尾声：`pop ebp/edi/esi/ebx; ret`），非调用，C 侧即普通 `return`；
  `0x2E2B0` 的 `rec2` 交换后 `v3[7]`（`def_row`）仍取交换前值（机器用寄存器暂存），C 侧先取 `att_row/def_row`。
- **decompiler 形参错列**：Hex-Rays 给 `0x2E9A8` 的 `a5` 在两分支中语义相反（`rec[6]==0` 时是 `work+320`
  固定侧、否则是 `work` 基准），逐一按 `[esp+..]` 复原；`0x2E2B0` 的 `v8/v14/v18` 三个索引（TAI、BG 板、
  BG 主图）在 `dword_54133!=0` 时被覆写为 `dword_54133`（`att_row∈{0x1a,0x36}‖def_row==0x37` 时 TAI 取 3）。
- **未初始化/全局契约**：`dword_54133` 非 0 分支（demo）会跳过 `ev5_314DE` 与状态面板；`figure_attack_play`
  的 `dword_54133!=0` 早退依赖它先被置 1。
- **堆缝/资源**：`malloc/free`（`0x3706E/0x3776E`）→ `guest_malloc/guest_free`；`dword_53A5D` 的旧缓冲指针直接
  作为 `res_load` 的 `old_buffer` 传入；`dword_5414B/5414F`（`ev5_314DE` 的结果）按 uint32_t 指针语义调用/释放。
- **字符串**：`aFd2Tmp_1(0x502BD)="FD2.TMP"`、`aRb_15(0x502BA)="rb"`；`aFdshapDat(0x51A65)`、`aTaiDat(0x524A0)`、
  `aFiganiDat(0x52495)`、`aBgDat(0x5248E)`。
- 无对拍器，标“已还原未验证”。

### 79) 地图攻击序列驱动器 `0x1548E` → `map_attack_run`（本轮，1 函数）→ `src/game/map_attack.c/.h`（追加）

§78 记“下批按 §2 取次小传递待还原闭包的游戏内容簇”。闭包就绪列表按 usage 降序，游戏内容头名为
`0x1548E`(u2，496B)；生成器验得**真闭包＝自身**（closure size=1，全部 callee 已在册）。它是 manifest
注释所称“attack-result / status-overlay / level-up 家族”的驱动器，与既有 `map_attack` 家族同源，故追加该模块
（不新建头，伞头已含 `map_attack.h`）。

- 语义：`actor` 滑入肖像并朝标记格（`dword_53C43/53C47`，`mode`）移动（`map_move_toward`），目标
  `dword_53C4B` 转身；`byte_53AF9!=0` 时出状态帧并用 `map_attack_bar_drain` 抽目标血条，目标存活
  （剩余 HP≠0）且与 actor 斜向相邻时角色互换做反击（抽 actor 自己的血条），否则退化为 `figure_attack_view`；
  尾段清肖像 → 取队伍 +49 脚本（`map_collect_scripts`）→ 单位动画 → `ev_script_run` → `map_levelup_run`；返回 1。
- **陷阱 / 伪代码伪影**：`sub_3702F`＝Watcom chkstk（伪 callee）；Hex-Rays 把 `0x1E856` 的返回值丢成
  未初始化 `v3`（渲染为 `v3 != 0`），离线反汇编 `155b1: test eax,eax; setne` 说明用的是**返回值**；
  `0x1548E` 里两处 `0x1F04A` 调用 Hex-Rays 把前两次写反，按离线反汇编 `155d5/155e4` 恢复为
  `(target,actor)` 再 `(actor,target)`；本地脚本缓冲是机器 `[esp]` 暂存区（IDA 的 24B `v7`，即 8×3 字节
  脚本条目），原文未初始化、由 `map_collect_scripts` 填充。
- **对既有模块的唯一修正**：`0x1E856` 机器尾 `mov eax,[esp+8]; jmp 0x1317D`（`0x1317D` 为普通尾声）
  返回 `map_attack_resolve` 的最后结果（目标剩余 HP），但既有 `map_attack_bar_drain`（`map_attack.c`，
  状态“已还原未验证”）声明为 `void` 丢掉返回值。为忠实还原 `0x1548E` 的存活判定，将
  `map_attack_bar_drain` 改为返回 `int`（`map_attack.h/.c`）；其唯一调用点即本函数，无其他 C 调用者。
- 无对拍器，标“已还原未验证”。

### 80) 单位图形场景播放器 `0x2DFC8` → `figure_scene_play`（本轮，1 函数）→ `src/game/figure_view.c/.h`（追加）

§79 记“下批按 §2 取次小传递待还原闭包的游戏内容簇”。闭包就绪列表按 usage 降序，跳过
CRT/库包装 `0x3645D`(read)/`0x365DA`(int386) 后，游戏头名同为 u1 的四项中，`0x2DFC8`(744B)
是唯一落在 §1 的 `~1500B` 预算内、且与既有 `figure_view` 同模块（`figure_view.h` 开篇即记
`sub_2CF30` 家族及其复用的一套助手）的项，故取之（`0x15DF3` 126B 为孤立叶子、`0x2CF30`
2269B/`0x2D80D` 1780B 超预算，均留下批）。生成器验得**真闭包＝自身**（callee 全在册）。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x2DFC8` | `figure_scene_play(unit)` | 取记录 `dword_53A45+80*unit`：释放 `dword_53A49`/`dword_53A5D` → 建 320 跨距屏缓（0xFA00）与 640 跨距工作缓（0x1F400）→ `map_cell_info` 得 BG 索引 `cell[6]` → `res_load` TAI.DAT(3)、FIGANI.DAT(3*rec[7]) 与 (3*rec[7]+1)、BG.DAT(cell[6]) → `pal_fade_out` → 屏缓画状态面板 + BG → `ev5_314DE` 开机声 → `figure_attack_scroll(unit,1,fig_a,fig_a,work,screen,tai)` → 逐帧（`fig_b[0]` 帧）：每帧 `gfx_copy_rows(work←screen)` 复位、`res_blit(fig_b,i,work)`、发布 0xA0000、`svc_wait_ticks(f[6])`（先按 `f[5]` 放音）→ 释放六缓冲 → 重载 FD2SHAP.DAT 到 `dword_53A5D` → `svc_wait_ticks(6)`→`pal_fade_out`→清帧→`map_view_update(1)`→停声→`pal_fade_in`。 |

**陷阱 / 伪代码伪影**：
- **未初始化寄存器（playbook §6）**：帧循环计数器是 `edi`，本函数从不写它（入口值来自调用者
  `0x15055`——离线反汇编 `1511c: mov edi,eax` 取自 `map_reveal_reachable(0x14818)` 的返回）；
  C 侧建模为 `0` 并注释（同 `map_wait_key`/`map_zoom_run` 的 tick 基线先例）。
- **返回 void**：函数原型记 `int`，但尾声 `call 0x1F525`(pal_fade_in, void) 后直接 `ret`，
  eax 残留；唯一调用点 `0x15055`（`15195 call 0x2dfc8; add esp,4`）不读返回值 → C 侧 `void`。
- **free/malloc 缝**：`0x3776E/0x3706E` → `guest_free/guest_malloc`；字符串 `aTaiDat/aFiganiDat/aBgDat/aFdshapDat` 与
  `dword_53A55`（FDJSHAP 尺寸表首字节×2）直接复用 figure_view 既有写法。
- **共享尾声（伪 callee）**：`0x2DFC8` 自身尾声 `add esp,0x24; pop ebp/edi/esi/ebx; ret`，无跨函数跳转；
  `sub_3702F`＝Watcom chkstk（`push 0x54`），丢弃。
- **对既有模块的唯一改动**：仅追加函数与头声明，未改任何已验证/已还原函数。
- 无对拍器，标“已还原未验证”。本批后 `0x2DFC8` 的两个调用者 `0x15055`(u2)/`0x31C49`(u1) 转为闭包就绪。

### 81) AI 物品使用执行体 `0x15055` → `map_ai_use_item`（本轮，1 函数，按 §1 取在预算游戏头名）→ `src/game/map_ai.c/.h`（追加）

§80 记“下批按 §1 取 usage 最高的在预算游戏项 `0x15055`(u2，700B)”。闭包就绪列表按 usage 降序，
跳过 CRT/库包装 `0x3645D`(read)/`0x365DA`(int386) 后，`0x15055` 即最高使用的游戏内容项，
且与既有 `map_ai` 同族（它是决策驱动器 `0x14EF0` 的三个执行体之一、`0x14EF0` 在册），故追加该模块
（不新建头，伞头已含 `map_ai.h`）。生成器验得**真闭包＝自身**（全部 callee 在册）。
逐指令核过离线反汇编 `15055..15310`（155 行），ABI＝cdecl、2 参。

| 项 | 要点 |
|---|---|
| 签名/ABI | `int map_ai_use_item(int actor, int require_empty)`（调用点 `14f6c: push a2; push a1`） |
| 取物品 | `dword_53C57 = rec_field_byte(actor, dword_53C3F)`（`dword_53C3F` 是**物品栏槽位**而非目标记录，与 `map_ai_pick_item_target` 写 `dword_53C3F = i` 一致）→ `eff = item_entry(dword_53C57)`（`0x4E8BC`，base 0x602AD/步长 23）→ `filter = require_empty ? eff[17] : (eff[17]==0)` |
| 伸手范围 | `kind=eff[16]`：`<= 0x0F` → `count = map_reveal_reachable(dword_53C37, dword_53C3B, list, eff[18], 0, filter)`；`>= 0x10` → `count = map_scan_records(dword_53C37, dword_53C3B, list, rec[0], rec[1], kind-16, 0)`（reach vs 直线扫，与 `map_ai_pick_item_target` 同构） |
| 通用段 | `util_fix_records(dword_53A51)`；`j___delay(200)`；`dword_51A83 = eff[18]+2` |
| 大招分支（`kind >= 0x10`） | `ev5_1F04A(actor, list[0])` → `map_view_update(1)` → `svc_wait_ticks(1,2)` → `figure_scene_play(actor)`（0x2DFC8）→ `portrait_reveal_cell(actor,80,-4)` → 延时 200 → `for i=64..0: pal_fade_add(0,255,i); j___delay(4)` → `dword_51A83=6` → `kind-=16` → 从 `qword_53AB1` 线性插值新视图中心并**有符号钳制**到 `[0, dword_53AC1/53AC5 - 1]` → `dword_53C1F=0; map_slide_view(...)` → `dword_51A83=0; for j=1..8: dword_53C1F=j; map_view_update(1); svc_wait_ticks(1)` → `util_fix_records` → `svc_wait_ticks(2)` → `dlg_portrait_glide(list[0])` |
| 小分支（`kind < 0x10`） | `map_slide_view(dword_53C37, dword_53C3B)` |
| 收尾 | `map_action_router(actor, dword_53C3F, count, list)`（0x20C6F，物品条目 +13 动作码本体）→ `dlg_portrait_clear()` → `dword_53EC8 = 0` → `return 0` |

**陷阱 / 伪代码伪影**：
- **`qword_53AB1` 假 64 位视图**：Hex-Rays 渲染成 `(dword_53C37 - qword_53AB1)*v14 + qword_53AB1`，但离线反汇编
  `151ec..15251` 全程 32 位（`sub eax, ds:53ab1` / `imul ebx,eax` / `add eax,ds:53ab1`），y 侧用 `ds:53ab5`（即 8 字节值的
  **高 dword**）。C 侧按 32 位 `(int32_t)(dword_53C37 - (uint32_t)qword_53AB1)` 与 `dword_53AB5` 忠实还原。
- **有符号钳制**：`cmp/jl/jge` 全为有符号（`1520b`/`1524e`），`dword_53AC1/53AC5` 虽在生成头里是 `uint32_t`，C 侧比较时
  强转 `int32_t`，越界侧写 `dword_53ACn - 1`（`dec` 的 32 位回绕语义）。
- **伪 callee**：`sub_3702F`＝Watcom chkstk（`push 0x54`，不分配帧），非依赖；尾声 `add esp,0x24; pop×4; ret` 自清，无跨函数跳转。
- **局部缓冲**：`v13`（机器 `[esp]`，0x20B）既作 `map_reveal_reachable`/`map_scan_records` 的 `out`，也作 `map_action_router` 的 `list`；
  C 侧用 `uint8_t list[32]`（与 `map_ai_pick_item_target` 的 `cells[32]` 同尺寸）。`list[0]` 供 `ev5_1F04A`/`dlg_portrait_glide`。
- **`dword_53C3F` 语义**：在 `15055` 里被 `rec_field_byte(actor, slot)` 与 `map_action_router(..., slot, ...)` 当**槽位**用，
  印证它在 `map_ai_pick_item_target` 中确实是槽位 `i`（非目标记录索引）。
- 对既有模块的唯一改动：仅追加函数与头声明，未改任何已验证/已还原函数。
- 无对拍器，标“已还原未验证”。本批后 `0x15055` 的调用者 `0x14EF0`(u6) 仍待其另一 callee `0x15311`（大闭包），暂未闭包就绪。

### 82) 地图命令/行动列表面板 `0x1CEED` → `action_menu_draw`（本轮，1 函数）→ `src/game/action_menu.c/.h`（新建）

按 §1 闭包就绪列表（usage 降序）：§81 拉出 `0x15055` 后，`0x1CEED`(u3，259B) 的真闭包被补齐
（其 callee `0x15055` 现已在册），跳过 CRT 包装 `0x3645D`(u4, read) 后即最高使用的在预算游戏内容项。
生成器验得**真闭包＝自身**（`0x15F84`/`0x1685C`/`0x187D6`/`0x1C269`/`0x4E866` 全在册）。
逐指令核过离线反汇编 `1ceed..1cfeb`。

| 项 | 要点 |
|---|---|
| 签名/ABI | `void action_menu_draw(int rec, int sel, int base)`（cdecl，调用点 `1cff0` 三 push） |
| 列表来源 | `count = rec_collect_slot_bits(rec, list)`（0x1C269，收集记录 +26..+30 的置位 bit，索引 0..39） |
| 每行 | `x=100*(i/4)+18`，`y=320*(22*(i%4)+103)`；`vm_run(dword_53A7D, list[i]+441, x+base+y, 320, i==sel?201:205, 76,0,0,0)`；`y2=320*(22*(i%4)+108)` 处 `dlg_frame_tile(y2+x+base+50, 320, dword_53A81, 92)` 与 `dlg_draw_number(y2+x+base+73, 320, action_entry(list[i])[5], 42, 2)` |
| 行动表 | `0x4E866`＝7 字节条目表 base 0x619FD（`&fd2_data2[0x19FD]`，stride 7），C 侧 `tbl_ptr` |

**陷阱 / 伪代码伪影**：
- **Watcom 栈探针**：`push 0x5C; call 0x3702F` 不分配帧，非依赖。
- **共享尾声**：循环出口 `jge 0x15309` 落在 `0x15309` 的公共尾声（`add esp,24h; pop ebp/edi/esi/ebx; ret`），即普通 `return`；Hex-Rays 渲染成 `JUMPOUT(0x15309)`。
- **`__CFSHL__` 伪影**：`100*((i-(__CFSHL__(i>>31,2)+4*(i>>31)))>>2)+18` 的 sar/sbb 序列就是**向零截断**的 `i/4`（i≥0 即 `i/4`）；`i%4` 由 `idiv` 余数给出。
- **局部缓冲**：机器 `_BYTE v10[32]`（+0x20 处紧跟 `v11`=count）；沿用 `map_ai` 的 `list[32]` 约定。
- **返回值**：调用方均忽略（`0x1D51D`/`0x1CFF0`/`0x17AED`），C 侧取 `void`。
- 对既有模块的唯一改动：伞头 `fd2_game.h` 追加 `#include "action_menu.h"`；未改任何已验证/已还原函数。无对拍器，标“已还原未验证”。

### 83) 行动菜单族：开箱动画与选择环 `0x17AED`/`0x1D51D`（本轮，2 函数）→ `src/game/action_menu.c/.h`（追加）

按 §1 闭包就绪列表（usage 降序）：§82 拉出 `0x1CEED` 后其两个 caller 转为闭包就绪，其中
`0x17AED`(u3，642B) 为当前 usage 最高的在预算游戏内容项（跳过 CRT 包装 `0x3645D`(u4，read)），
`0x1D51D`(u1，427B) 同属行动菜单族（与 `0x17AED` 共享 `0x1CEED`/`0x1C269`/`0x25A96`/`0x4E866`），
二者合计 1069B（≤ §1 的 ~1500B 指引），故作为一簇追加至既有 `action_menu`。
生成器验得每函数**真闭包＝自身**（callee 全在册）；逐指令核过离线反汇编 `17aed..17d6a`、`1d51d..1d6c7`。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x17AED` | `action_menu_open(rec)` | `box_open_anim(rec)`（分配三个 0xFA00 暂存缓冲并开箱）→ `dlg_wait_key(0)`；若 `rec_collect_slot_bits(rec,NULL)!=0`：`svc_play_sfx(dword_53EEC,6,1)`，`i=0..6` 每步 `memmove(53C5B←53C5F,0xFA00)`+`fb_slide_band_a(5)`+`band_b(7)`+`band_c(16*i+94)`+`memmove(VGA←53C5B)`；再补一次 a/b 并把 `53C63+30085` 的 `rle2_blit(dword_53A81+*(u32*)(dword_53A81+90),320)` 贴回→`action_menu_draw(rec,-1,53C63)`+`svc_play_sfx(...,5,1)`→`j=6..0` 逆向收拢→`dlg_wait_key(0)`；随后 `i=0..11`（`i==0||i==7` 播音效）`fb_slide_step(i,53C5B,53C63,53C5F)`→`memmove(VGA←53C5F)`→`guest_free` 三个缓冲 |
| `0x1D51D` | `action_menu_select(rec)` | `action_menu_draw(rec,dword_53C57,0xA0000)`→`count=rec_collect_slot_bits(rec,NULL)`、`last=count-1`→`dlg_wait_key(0)`，`key=byte_53A8E`；0x48 上（`53C57!=0?--53C57:53C57=last`）、0x50 下（`last==53C57?53C57=0:++53C57`）、0x4b 左（`53C57>=4` 则 `-=4`）、0x4d 右（`count-4>53C57` 则 `+=4`），四者确认前均 `svc_play_sfx(dword_53EEC,0,1)` 且返回 0；0x1c/0x39 确认：`rec_collect_slot_bits(rec,list)`＋`*(u16*)(rec+68) < action_entry(list[53C57])[5]` 时返回 0，否则 1；0x01（ESC）返回 −1；其余返回 0 |

**陷阱 / 伪代码伪影**：
- **共享尾声（伪 callee）**：`0x17AED` 机器尾 `push ds:0x53C63; jmp 0x15E94`＝`guest_free(dword_53C63)` +
  `pop ebx; ret`（0x15E94 本体 `call 0x3776e; add esp,4; pop ebx; ret`）；`0x1D51D` 的 `jmp 0x16F04` 是
  公共尾声 `add esp,0Ch; pop ebx/esi/edi; ret`，均即普通 `return`。
- **`0x1D51D` 的伪 callee `sub_16E24`**：它在 `0x16C57`（dlg_wait_key，461B）之后紧邻，被函数图误记为 callee；
  离线反汇编区间内无 `call 0x16E24`，真依赖不含它（同 §82 的边界假阳性）。
- **`dlg_wait_key` 返回扫描码**：机器 `0x16C57` 尾声 `movzx eax,byte_53A8E; jmp 0x15d9a`（返回 eax），
  但 C 原型为 `void`；按既有 `box_input` 先例取 `byte_53A8E`（等价）。
- **`0x17AED` 返回值丢弃**：尾部 `free` 后 eax 残留，三个调用点（`11926`/`11a17`/`29656`）均忽略 → C 侧 `void`。
- **`0x1D51D` 的 `v2=v1`**：Hex-Rays 把一次杂散 eax 存进 v2 并用作 `count-1`；真值来自 `rec_collect_slot_bits` 的返回值。
- **局部缓冲**：`0x1D51D` 机器帧仅 0x0C 字节，而 `rec_collect_slot_bits` 最多写 40 项；沿用既有 `action_menu_draw`
  的 `list[32]` 约定（游戏实测动作数远小于上限）。
- **常数**：`0xFA00`＝320×200 帧；`30085`(0x7585)、`0x5A`(90) 为子图偏移；`655360`(0xA0000)＝VGA 帧基址。
- **对既有模块的唯一改动**：追加两函数与头声明、`action_menu.c` 增 `<string.h>`（`memmove`），
  并订正头注释中两处 caller 的“pending”描述；未改任何已验证/已还原函数。无对拍器，标“已还原未验证”。

### 84) 场所菜单选项屏 + 队员行动列表 `0x29300`/`0x29620`（本轮，2 函数）→ `src/game/place_screen.c/.h`（新建）

按 §1 闭包就绪列表（usage 降序）：跳过 CRT 包装 `0x3645D`(u4，read) 后，usage 最高的在预算游戏内容项为
`0x29620`(u2，109B，真闭包＝自身)。其两个 caller 之一 `0x29300`(u1，800B) 只差它一个 callee（`sub_29DAA` 另需
`0x28F65`），故取同族 caller/callee 对 `0x29620`+`0x29300`＝909B（≤ §1 的 ~1500B 指引），落 `place_screen` 新模块。
生成器验得两函数真闭包＝本簇（callee 全在册）；逐指令核过离线反汇编 `29300..2961f`、`29620..2968c`。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x29300` | `place_screen_run(src)` | 载 FDOTHER 13(`res_load`→`dword_53F66`)+`res_blit6(0xA0000,320,·,0)`+`pal_fade_in`+`j___delay(200)`+`msg_open_portrait`+`vm_run(585)`+`dlg_blit_dato(...,0)`；随后 `for(;;)`：`dword_53C57=v1`→`screen_curtain(0)`→`v6=menu_choice4()`（`v6==1` 时 `v1=dword_53C57`）→`screen_curtain(1)`→`msg_close_portrait`；`v6!=1` 退出。槽 0→`place_member_actions_run()`；槽 1→`save_slot_write(1)`；槽 2→`save_slot_load()`；其余槽→`vm_run(415)`+`map_window_run()`+`map_draw_text_window()`，若 `v2!=-1 && dword_53C57==0` 则 `vm_run(416,0xAAc8C)`+`j___delay(200)`+`msg_close_portrait`+`return 1`。每步收尾 `vm_run(586)`。退出后 `res_blit6`+`j___delay(200)`+`pal_fade_out`+`i=10..0` 的 `place_zoom_blit(·,·,src,128-9i)`+`memmove(VGA←dword_53A49,0xFA00)`+`pal_fade_range(0,255,4i)`→`guest_free(dword_53F66)`→`return 0` |
| `0x29620` | `place_member_actions_run()` | `dword_53F5E=dword_53BFB`；`for(;;)`：`ret=menu_list_select()`+`msg_close_portrait()`+存 `dword_53C67`；`ret==-1` 返回；否则 `action_menu_open(dword_53C57)`+还原 `dword_53C67`+`dword_53A85=res_load(aDatoDat,dword_53A85,byte_52387[0])` 循环 |

**陷阱 / 伪代码伪影**：
- **chkstk**：两者皆 `push size; call 0x3702F`，丢弃（0x3702F 自恢复 ESP，实参仍在 `[esp+N]` 原位）。
- **`<<7` 改写为 `*128`**：`0x29300` 退场的两条中心坐标 `((i*(byte_52363[6*v5+dword_53F4A]-150)/10)<<7)+20480`、
  `((i*(byte_52375[…]-100)/10)<<7)+12800`，机器用有符号 `idiv` 后 `shl eax,7`；商可负，C 侧用 `*128`（32 位结果一致）。
- **`0x4E809` 表访问器**：`v5=*(u8*)tbl_ptr(&fd2_data2[0x238D],31,dword_53C03,-31)`（tables.h 常量 0x6238D/31/-31，同 scene_core）。
- **资源索引固定 `byte_52387[0]`**：机器 `movzx eax,BYTE PTR ds:0x52387`（无下标），两函数均按原位还原（非 `[…+dword_53F4A]`）。
- **堆**：`dword_53F66` 系 `res_load`（guest_malloc）所出，收尾用 `guest_free`（不是宿主的 `free`）；`0x29300` 的 `return 1` 提前退出路径**不释放**，与机器一致。
- **返回/末值**：两函数机器 eax 末值（`place_member_actions_run` 的残留、`place_screen_run` 的 0/1）——前者调用点全忽略（C 侧 `void`），后者按位返回 0/1。
- 无对拍器，标“已还原未验证”。

### 85) 地点菜单驱动器 + “给道具”屏 `0x279BC`/`0x28F65`（本轮，2 函数）→ `src/game/shop_screen.c/.h`（追加）

按 §84 遗留建议取同族 caller/callee 对：`0x28F65`(u2，693B，闭包就绪)与其唯一在册 caller 之一 `0x279BC`(u1，887B，
只差 `0x28F65`)——二者同属地点/商店菜单族（`shop_screen.h` 既定范围：“0x279BC 的四个菜单选项在此驱动”）。
合计 1580B，较 §1 的 ~1500B 指引略超，系 §84 明示的“同族对”取舍。生成器验得真闭包＝本簇（callee 全在册）；
逐指令核过离线反汇编 `279bc..27d32`、`28f65..29219`。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x28F65` | `shop_give_run()` | `for(;;)`：`msg_open_portrait(byte_52387[dword_53F4A])`+`vm_run(512)`+`dlg_blit_dato(...,0)`+`dlg_wait_key(1)`+`msg_close_portrait`；`dword_53F5E=dword_53BFB`；`menu_list_select()`（-1 即返回）；取选中的 source 记录，收集 8 槽中状态字节 `<0x80` 的道具号到 `items[20]`；无道具则 `dword_53AD9=rec[8]+1`+`vm_run(511,…)`+等待；否则 `dword_53C57=0`/`dword_53F4E=0`+`shop_items_paint(n,items,1)`+`shop_items_run(n,items,1)`（-1 收尾回到顶）；确认后 `slot=dword_53C57`+`vm_run(510,…)`+第二次 `menu_list_select()` 选接受者；`rec_slot_free(target)==8`（八槽全占）则 `dword_53AD9=rec[8]+1`+`vm_run(word_5238D[dword_53F4A],…)`+等待；否则 `item=rec_field_byte(source,slot)`→`rec_slot_remove(source,slot)`→`rec_slot_claim(target,item)`→`unit_metrics(source)` |
| `0x279BC` | `place_menu_run(src)` | `place=*(u8*)tbl_ptr(&fd2_data2[0x238D],31,dword_53C03,-31)`；`index=12`（`dword_53F4A==3`→29，`==5`→63）；`res_load(aFdotherDat,…)`→`dword_53F66`+`res_blit6(0xA0000,320,·,0)`+`pal_fade_in`+`j___delay(200)`+`msg_open_portrait`；`panel=dword_53F66+[dword_53F66+10]`→`rle2_blit(0xA76C5,panel,320)`+`dlg_draw_number(0xA7BD0,320,dword_53BF3,31,8)`，并同样 blit/number 到 `dword_53C5F+30405/+31696`；`vm_run((dword_53F4A==1)?501:440,…)`+`dlg_blit_dato(...,0)`；`dword_53F6A=0`/`dword_53F6E=0`。`for(;;)`：`dword_53C57=v1`→`screen_curtain(0)`→`choice=menu_choice4()`（`==1` 时 `v1=dword_53C57`）→`screen_curtain(1)`→`n=place_name_copy(items)`→`dword_53F5E=n`→`msg_close_portrait`；`choice==1` 时按 `dword_53C57` 分派：0→`shop_place_buy_run(n,items)`、1→`shop_sell_run()`、2→`shop_box_run()`、否则→`shop_give_run()`，随后 `msg_open_portrait`+`vm_run((dword_53F4A==1)?503:440,…)`+`dlg_blit_dato(...,0)`；`choice!=1` 退出。退出后 `res_blit6`+`j___delay(200)`+`pal_fade_out`+`i=10..0` 的 `place_zoom_blit(·,·,src,128-9i)`+`memmove(VGA←dword_53A49,0xFA00)`+`pal_fade_range(0,255,4i)`→`guest_free(dword_53F66)`→`dword_53F66=0`→`return (dword_53F4A==0)?1:0` |

**陷阱 / 伪代码伪影**：
- **chkstk**：两者皆 `push size; call 0x3702F`，丢弃（0x3702F 自恢复 ESP，实参仍在 `[esp+N]` 原位）。
- **共享尾声**（均普通 `return`）：`0x28F65` 的 `je 0x29619` 落在 `0x29300` 体内的收尾块（`0x29619: add esp,8; pop edi; pop esi; pop ebx; ret`，与其帧一致）；
  `0x279BC` 尾 `jmp 0x27071`（`0x27071: add esp,0x18; pop ebp; pop edi; pop esi; pop ebx; ret`，与其帧一致）。
- **返回/末值**：`0x28F65` 的唯一出口（首选成员取消）eax 已被 `msg_close_portrait` 覆盖、调用点全忽略 → C 侧 `void`；
  `0x279BC` 收尾 `eax=v10`（`0x53F4A==0` 时为 1，否则 0）→ C 侧 `int` 按位返回（其 caller `0x2670E` 仍待还原）。
- **`<<7` 改写为 `*128`**：`0x279BC` 退场两条中心坐标（`byte_52363/byte_52375` 经 `6*place+dword_53F4A` 索引），机器有符号 `idiv` 后 `shl eax,7`；C 侧用 `*128`（32 位结果一致）。
- **`0x4E809` 表访问器**：`place=*(u8*)tbl_ptr(&fd2_data2[0x238D],31,dword_53C03,-31)`（tables.h 常量 0x6238D/31/-31，同 scene_core/place_screen）。
- **`byte_52387[dword_53F4A]`**：本批两函数按 `dword_53F4A` 索引肖像 id（与 §84 的固定 `[0]` 不同），逐条按机器 `movzx eax,[eax+0x52387]` 还原。
- **`word_5238D` 有符号**：机器 `movsx eax,WORD PTR [eax*2+0x5238d]`；它是消息号（<0x8000），与既有 `shop_place_buy_run` 同按 `word_5238D[dword_53F4A]`（uint16 下标）调用 `vm_run`。
- **`dlg_blit_dato` ABI**：机器只压 idx（此处恒 0），按 §0-J 约定补传 `dword_53A85,dword_53C67`（同上批）。
- **`rec_slot_free==8` 含义**：该访问器实为“状态字节 bit7 清零（占用）的槽数”（名不符实，见 rec.h/rec.c）；`==8` 即八槽全占，故拒绝再收。
- **堆**：`dword_53F66` 系 `res_load`（guest_malloc）所出，收尾用 `guest_free`（不是宿主 `free`）。
- 无对拍器，标“已还原未验证”。本批后 `0x279BC` 的 caller `0x2670E` 仍未闭包就绪；`0x28F65` 的另一 caller `0x29DAA` 仍待还原。

### 86) 场所类型 4 选项屏 `0x29DAA` → `place_screen4_run`（本轮，1 函数）→ `src/game/place_screen.c/.h`（追加）

按 §85 遗留建议取 `0x29DAA`（u1，720B，闭包就绪）：`0x2670E`（地点菜单驱动器，仍待还原）按 `dword_53F4A`
分派场所屏——类型 0→`0x29300`、类型 4→`0x29DAA`、类型 2 跳过、类型 3/其余→`0x279BC`（类型 3 传 0xF）；
本函数即**场所类型 4** 的选项屏，与 `0x29300`（place_screen_run）同壳，落同族模块。真闭包＝自身
（所有 callee 已在册）；生成器验得闭包就绪。合计 720B，在 §1 预算内。逐指令核过离线反汇编 `29daa..2a075`。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x29DAA` | `place_screen4_run(src)` | 取 `v5=place`（`tbl_ptr(&fd2_data2[0x238D],31,dword_53C03,-31)[0]`）；`res_load(aFdotherDat,…,14)`→`dword_53F66`→`res_blit6(0xA0000,320,·,0)`→`pal_fade_in`→`j___delay(200)`→`msg_open_portrait(byte_5238B)`；把 FDOTHER 面板子图（`dword_53F66+[dword_53F66+10]`）`rle2_blit` 到 `0xA76C5` 并 `dlg_draw_number(0xA7BD0,320,dword_53BF3,31,8)`，同样 blit/number 到 `dword_53C5F+0x76C5/+0x7BD0`；`vm_run(585,…)`+`dlg_blit_dato(...,0)`。`for(;;)`：`dword_53C57=v2`→`screen_curtain(0)`→`v6=menu_choice4()`（`==1` 时 `v2=dword_53C57`）→`screen_curtain(1)`→`msg_close_portrait`；`v6==1` 时按 `dword_53C57` 分派：0→`place_member_actions_run`、1→`shop_give_run`、2→`shop_buy_run`、3→`item_menu_run`，随后 `msg_open_portrait(byte_5238B)`+`vm_run(586,…)`+`dlg_blit_dato(...,0)`；`v6!=1` 退出。退出后 `res_blit6`+`j___delay(200)`+`pal_fade_out`+`i=10..0` 的 `place_zoom_blit(·,·,src,128-9i)`+`memmove(VGA←dword_53A49,0xFA00)`+`pal_fade_range(0,255,4i)`→`guest_free(dword_53F66)`→清零→`return 0` |

**陷阱 / 伪代码伪影**：
- **chkstk**：`push size; call 0x3702F`，丢弃（0x3702F 自恢复 ESP，实参仍在 `[esp+N]` 原位）。
- **共享尾声（含副作用，非纯 return）**：`0x29DAA` 唯一出口 `jmp 0x295FF` 落在 `0x29300` 体内的收尾块——`free(dword_53F66)`+`dword_53F66=0`+`xor eax,eax`+`add esp,8; pop edi/esi/ebx; ret`；与本函数帧（`push ebx/esi/edi; sub esp,8`）一致，故 C 侧同样 `guest_free`+清零+`return 0`。
- **返回类型**：唯一出口 EAX=0，且 `0x2670E` 经 `mov ebp,eax` 向上返回其值——故 C 侧 `int` 恒返回 0（与 `place_screen_run` 的 1/0 不同）。
- **面板偏移 +10（非 +70）**：本函数与 `0x279BC`（place_menu_run）一样用 `[dword_53F66+10]`，而非 `shop_items_paint` 的 `+70`；逐条按机器 `mov esi,[eax+0xa]` 还原。
- **`vm_run` 流号**：`0x249`(585) 首帧、`0x24A`(586) 每槽收尾（与 `0x29300` 同）；消息流地址 `0xA94CC`。
- **肖像**：固定 `byte_5238B`（标量），非 `byte_52387[dword_53F4A]`（`0x29300` 用后者 `[0]`）；逐条按机器 `movzx eax,BYTE PTR ds:0x5238b` 还原。
- **分派无 else**：`0x29DAA` 四槽全显式（0/1/2/3），无 `0x29300` 的文本窗分支；`cmp eax,dword_53C57`（eax=v6=1）即槽 1 分支。
- **`<<7` 改写为 `*128`**：退场两条中心坐标同 `0x29300`（机器有符号 `idiv` 后 `shl eax,7`；C 侧用 `*128`，32 位结果一致）。
- `dlg_blit_dato` ABI：机器只压 idx（恒 0），按 §0-J 约定补传 `dword_53A85,dword_53C67`。
- 堆：`dword_53F66` 系 `res_load`（guest_malloc）所出，收尾用 `guest_free`。
- 无对拍器，标“已还原未验证”。本批后 `0x29DAA` 的唯一 caller `0x2670E` 仍未闭包就绪。

### 87) 地点列表入口驱动器 `0x2670E` → `place_menu_commit`（本轮，1 函数）→ `src/game/place_menu.c/.h`（追加）

按 §86 遗留建议取 `0x2670E`（u1，648B，闭包就绪）：它是 `0x26152`（scene_apply_state）地点列表
按 Enter 时提交的分派器，`dword_53F4A` 选场所。逐指令核过离线反汇编 `2670e..26995`。真闭包＝自身
（所有 callee 已在册，§86 点名的 `0x29DAA` 也已在册）。合计 648B，在 §1 预算内。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x2670E` | `place_menu_commit()` | `place=tbl_ptr(&fd2_data2[0x238D],31,dword_53C03,-31)[0]`；`bgm_play(-1,0)`；若 `dword_53F4A==2`：`msg_open_portrait(75)`+`vm_run(dword_53A7D,513,0xA951F,320,205,76,74,19,1)`+`dword_53A51=(uint8_t*)1`+`dlg_blit_dato(·,·,0)`+`v0=map_window_run()`+`map_draw_text_window()`+`dword_53A51=0`+`msg_close_portrait`，`v0==-1||dword_53C57!=0` 即 `return 0`，否则 `(dword_53C03<27&&dword_53BFB>16)||(dword_53C03>26&&dword_53BFB>20)` 时置 `dword_53A45=dword_53BF7`→`party_change_run()`→清零，其返回 0 则 `return 0`，然后 `result=1`；`dword_53A45=dword_53BF7`；`frame=guest_malloc(0xFA00)`→`memmove(VGA←frame)`；`i=1..10`：`place_zoom_blit(((byte_52363[6*place+dword_53F4A]-150)*i/10)*128+20480, ((byte_52375[...]-100)*i/10)*128+12800, frame, 128-9i)`+`memmove(VGA←dword_53A49)`+`pal_fade_range(0,255,4i)`；再 `pal_fade_range(0,255,64)`+`memset(VGA,0,64000)`；分派：0→`bgm_play(13,0)`+`place_screen_run`、4→`bgm_play(11,0)`+`place_screen4_run`、3/5→`bgm_play(dword_53F4A==3?15:14,0)`+`place_menu_run`；`dword_53F4A!=2` 时 `bgm_play(10,0)`；`guest_free(frame)`+`dword_53A45=0`+`return result` |

**陷阱 / 伪代码伪影**：
- **chkstk**：`push 0x3C; call 0x3702F`，丢弃（0x3702F 自恢复 ESP，实参仍在 `[esp+N]` 原位）。
- **共享尾声（纯 return）**：`JUMPOUT(0x29867)` 落在 `0x2986F` 体内，帧一致（`add esp,4; pop ebp/edi/esi/ebx; ret`），即 `return eax`；`0x2670E` 帧（`push ebx/esi/edi/ebp; sub esp,4`）与之吻合。
- **`dlg_blit_dato` ABI**：机器只压 idx（恒 0），按 §0-J 约定补传 `dword_53A85,dword_53C67`。
- **`bgm_play` 参数序**：机器 `push loop_count; push track`；`push 0`+`push 0xd` = `bgm_play(13,0)`。
- **`<<7` 改写为 `*128`**：机器有符号 `idiv 10` 后 `shl eax,7`；C 侧 `*128`，32 位结果一致。
- **场所字节表**：`byte_52363/52375[6*place+dword_53F4A]` 为 `(uint8_t*)&fd2_data1[0x2363/0x2375]`，逐条按机器 `movzx edx,BYTE PTR [ecx+0x5236x]` 还原。
- **堆 / 哨兵**：`frame` 系 0x3706E（guest_malloc）所出，收尾 `guest_free`；`dword_53A51` 用 `(uint8_t*)(uintptr_t)1` 哨兵（同 `party_roster.c`）。
- 无对拍器，标“已还原未验证”。本批后 `0x2670E` 的唯一 caller `0x26152` 的 `sub_2670E()` 调用点闭包就绪。

### 88) 地图标签格查找 `0x15DF3` → `map_find_tag_cell`（本轮，1 函数）→ `src/game/map_scroll.c/.h`（追加）

按 §87 遗留建议取 §1 闭包就绪游戏头名 `0x15DF3`（126B，u1，真闭包＝自身）：它是记录移动
驱动器 `0x13A9F`（仍待还原）case 5 的叶助手，与既有 `map_scroll` 的 `map_scroll_cell_tag`（0x13A44）
同属“地图格标签”概念（0x13A44 由格反查标签，本函数由标签反查格），故追加该模块。逐指令核过
离线反汇编 `15df3..15e6d`。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x15DF3` | `map_find_tag_cell(tag, out)` | 外层 `y=0..dword_53AC5-1`、内层 `x=0..dword_53AC1-1` 遍历全图：`map_cell_info(x,y,info)`；命中判据 `(info[4]&0x60)==0x20 && info[2..3]==tag`（`info[2..3]` 即格标签字，`info[4]` 即 `dword_53A69[tile]` 首字节）；命中则 `out[0]=x, out[1]=y` 并返回 0，否则整图扫完后返回 −1。调用点 `13c53..13c59`（`push &out; push ebp`，`ebp=rec[61]`，`add esp,8`，读 eax）证 ABI=`int(int, uint8_t*)`。 |

**陷阱 / 伪代码伪影**：
- **伪 callee（共享尾声）**：`jmp 0x15677` = `add esp,0xc; pop edi; pop esi; pop ebx; ret`，落在 `map_attack_run`(0x1548E，其体止于 0x15676) 的尾声内，与本函数帧（`push ebx/esi/edi; sub esp,0xc`）一致 ⇒ 普通 `return`；`function-map` 因此把 `sub_1548E` 列为 callee，属假阳性，真依赖只有 `map_cell_info`。
- **chkstk**：`push 0x28; call 0x3702F`（不分配帧），丢弃。
- **Hex-Rays 丢 −1 路径**：伪代码把外循环越界与命中都折成 `LABEL_10: JUMPOUT(0x15677)`，漏掉了 `15e67: mov eax,0xffffffff`。按离线反汇编恢复为“未命中返回 −1”。
- **signed 界比较**：两处 `jge` 为有符号；`dword_53AC1/AC5`（`uint32_t`）为地图尺寸（小正数），C 侧显式 `(int32_t)` 转型保持原语义。
- **形参类型**：机器读 `[esp+4]/[esp+8]`（随 `push ebx/esi/edi; sub esp,0xc` 校正）即第 1/2 实参，与伪代码 `(int, _BYTE *)` 一致；本 C 用 `uint8_t *out`。
- 无对拍器，标“已还原未验证”。本批后闭包就绪待还原仅剩 CRT/库包装与伪函数条目（见下）。

### 89) 队伍图形演示驱动器 `0x2CF30` → `figure_party_show`（本轮，1 函数）→ `src/game/figure_view.c/.h`（追加）

按 §88 遗留建议、依 playbook §5 的“超预算单函数”例外拉出闭包就绪头名 `0x2CF30`（2269B，u1）：
闭包就绪列表已无 in-budget 游戏内容项（仅 CRT/库包装与伪函数条目），本函数的**真闭包＝自身**
（唯一调用者 `0x2FF01` 仍待还原；所有 callee 已在册），已逐指令核对离线反汇编 `2cf30..2d80c`。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x2CF30` | `figure_party_show(unit,mode,count,units)` | 效果分发器 `0x2FF01` 的“图形演示”：释放旧屏/DATO/形状缓冲 → 载 `unit` 格（`map_cell_info` 字节 6）的 BG/TAI 条 + `figure_bg_pick(count,units)` 选出的 BG 条 + FIGANI 组 `3*rec[7]`/`+2` + 三条轮转 BG 条 `dword_5413F` → `pal_fade_out` → 逐 unit 载其 FIGANI 组 → `figure_attack_scroll` 滑入 + `figure_play` → 逐 unit：快照 HP 字 `+0x40`，`unit_damage(units[i], sel*rec[+0x48]/10 - rec[+0x4a])`（`sel`：模式 18/24/28/29→18/15/20/12）夹到快照后回写，再逐姿态子精灵（组 `+2`）处理：姿态子（`+4==1`）置 `step=5`/`blit_mode=33`、HP 按 `posture_n`（模式 28→8 否则 1）分帧下探、刷状态面板；每帧 `anim_cycle_frame(p[i], blit_mode, work+320-shake[step], 640)` + `res_blit(fig_b,j,work+320,640,-1)` + 发屏 + `svc_wait_ticks(1)` → 收尾释放、停音、重载 FDSHAP/FD2.TMP、恢复地图视图。 |

**陷阱 / 伪代码伪影**：
- **ABI（关键）**：唯一调用点 `0x3003f` 压 4 个实参，但 `push 0x100; call 0x3702F` 使 Hex-Rays 只留第 1 参，
  故 `int(int)` 原型错误；按压栈序（末压=第 1 参）定为 `(unit, mode, count, units)`（`ebp`=unit、`[esp+0xe4]`=mode、
  `[esp+0xe8]`=count、`[esp+0xec]`=units）。
- **双重释放（原机 bug，照留）**：`v19`（BG 条 `bg_a`，`[esp+0x88]`）在收尾被 `free` 两次（`2d6c4`、`2d6f1`）；
  C 侧同样 `guest_free(bg_a)` 两次并加注释。
- **常量表 `0x52656`**：镜像字节 = `{00 04 09 0e 12 0e}`＝`{0,4,9,14,18,14}`（与 `figure_attack_play` 的 `dword_5266C` 同）；
  机器 `movsd+movsw` 拷 6 字节到栈局部，C 侧 `memcpy(shake,&unk_52656,6)`。
- **`loc_1F400`**＝立即数 `0x1F400`（`malloc`/`memset` 的 size），非函数引用。
- **`::p` / `buf`**：伪代码的 `::p`＝`dword_53A55`（格表基址，取首字节）`buf`＝`dword_53A61`（DATO 图集）。
- **chkstk**：`push 0x100; call 0x3702F`（不分配帧），丢弃。
- **符号/字节**：`rec[+0x48]`/`rec[+0x4a]` 为 `int16`（`movsx`）；HP 字 `+0x40` 为 `u16`；姿态计数 `v34`、步 `v33`、
  分帧数 `v35` 均为字节（`inc/dec BYTE`、`movzx`）。
- 无对拍器，标“已还原未验证”。

### 90) 队伍图形演示姊妹函数 `0x2D80D` → `figure_demo_show`（本轮，1 函数）→ `src/game/figure_view.c/.h`（追加）

按 §89 遗留建议、依 playbook §5 的“超预算单函数”例外拉出闭包就绪头名 `0x2D80D`（1780B，u1）：
闭包就绪列表已无 in-budget 游戏内容项（仅 CRT/库包装与伪函数条目），本函数的**真闭包＝自身**
（唯一调用者 `0x2FF01` 效果分发器仍待还原；所有 callee 已在册），已逐指令核对离线反汇编 `2d80d..2df00`。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x2D80D` | `figure_demo_show(unit,mode,count,units)` | 效果分发器 `0x2FF01` 的 `mode>=32` 分支（模式 32..35）：释放屏/DATO 缓冲 → `map_cell_info(unit)` 取格字节 6 载 BG/TAI 条 → `rle_decode(bg,0,50,screen,320,-1)` + `ui_status_panel_at` + `pal_fade_out` → 载 FIGANI 组 `3*rec[7]`/`+1` 与 FDOTHER 条目 `mode+33`，再按 `0x5265C` 混合表第 4 列载 FDOTHER 条目入 `dword_54153`（音效库）→ `figure_attack_scroll(unit,1,fig_a,0,work,screen,tai)` + `figure_play(unit,mode,fig_b,fig_b,work,screen,bg,tai)` + `svc_wait_ticks(6)` → 图 A 滑入 8 帧、图 B 9 帧（每帧 `gfx_copy_rows` + `res_blit` + 发屏 + `svc_wait_ticks(1)`）→ 模式 33/34 再盖一次 B → 逐子帧循环（`memmove`+`res_blit`+发屏+`svc_wait_ticks(2)`，按模式在 k=1/2/6 发一次 SFX）→ 11 次 `pal_blend_to(0,255,40-4m,…)` 收束 → 释放 7 个缓冲、`dword_53A49=malloc(0x25680)`、按 `dword_53A55` 首字节载 FDSHAP、`pal_blend_to(0,255,0,…)`+清屏+`map_view_update(1)`+41 次渐入、停音并释放 `dword_54153`、`dword_53EC4=0`+`ev5_1D4CB` → 分发模式动作：32→`map_action_anim_resolve(...,32)`；33→逐 unit `memset(rec+0x25,0,3)` 后 `map_action_heal_list(...,800)`；34→`map_action_grow_x/grow_y/map_action_widen`（间插 `dword_53EC4=0`）；35→`map_action_damage_records(unit,26,...,37)`/`(...,22,...,39)`/`(...,27,...,38)` → 收尾 `menu_stop_and_free_music`。 |

**陷阱 / 伪代码伪影**：
- **ABI（关键）**：唯一调用点 `0x30012`（`2FF01` 的 `ebp>=0x20` 分支，`2fff7: cmp ebp,0x20; jl 0x3001f`）压 4 个实参，但 `push 0x6c; call 0x3702F` 使 Hex-Rays 只留第 1 参，
  故其 `void(int,int,int,int)` 原型里实参序号需按机器的 `[esp+0x4c..0x58]` 逐条核；本 C 取 `(unit,mode,count,units)`
  （`a1`=记录索引、`a2`=模式 32..35、`a3`=count、`a4`=unit 字节表）。
- **混合表 0x5265C**：机器把 **4 个连续 dword**（0x5265C/60/64/68）拷入栈，再按 `mode-32` 取**四个并列字节列**：
  `r=0x5265C[row]`、`g=0x52660[row]`、`b=0x52664[row]`、FDOTHER 索引=`0x52668[row]`。Hex-Rays 把该拷贝渲染成
  `qmemcpy(v21,"?355[]^",8)` 并把索引读成 `v17[mode]`（帧基址错位伪影）；离线反汇编 `2d81e..2d83e`/`2d985`/`2dc2c..2dc57` 为准。
  字节表实值 = 行0 `{0x3f,0x3f,0x3f,0x5b}`、行1 `{0x33,0x39,0x3f,0x5c}`、行2 `{0x35,0x00,0x00,0x5d}`、行3 `{0x35,0x3a,0x09,0x5e}`。
- **FIGANI 组**：载 `3*rec[7]` 与 `+1`（与 §89 `figure_party_show` 的 `+2` 不同）。
- **k 循环尾**：四条分支（模式 34&k==2、35&k==1、33&k==6、32&k==1）发不同 SFX 后**都** `svc_wait_ticks(2)`；
  模式 34&k==2 与 33&k==6 用 `svc_play_sfx(…,1,1)`，模式 35&k==1 与 32&k==1 用 `svc_play_sfx2(…,2,k)`。
- **frees**：`v10`（FDOTHER 条目 `mode+33`）在 m 循环后即释放；`dword_54153`（音效库）最后释放。
- **`dword_53A55`**：机器 `mov ebx,dword_53A55; movzx ebx,[ebx]; add ebx,ebx` ＝ `2*首字节`，用于 FDSHAP 条目索引。
- **chkstk**：`push 0x6c; call 0x3702F`（不分配帧），丢弃；`sub_4E98D`=rle_decode、`sub_2FACD`=ui_status_panel_at、`sub_2DF01`=pal_blend_to。
- 无对拍器，标“已还原未验证”。

### 91) 片头队伍图形演示 `0x31C49` → `figure_showcase_run`（本轮，1 函数，§5 大单函数条款）→ `src/game/figure_showcase.c/.h`（新建）

按 §90 遗留建议、依 playbook §5 的“超预算单函数”例外拉出闭包就绪头名 `0x31C49`（1511B，u1）：
闭包就绪列表已无 in-budget 游戏内容项（仅 CRT/库包装与伪函数条目），本函数的**真闭包＝自身**
（唯一调用者片头驱动 `0x31529` 仍待还原；所有 callee 已在册），已逐指令核对离线反汇编 `31c49..3222f`。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x31C49` | `figure_showcase_run(void)` | 片头队伍演示（由 `0x31529` 无参调用一次）：`world_load_party(30)`、`dword_53C67=3208` → 分配并清零 0x36B00 缓冲、`vm_run(dword_53A79,44,buf+76848,320,205,76,0,25,0)` 预绘 → 500 帧淡入（`pal_fade_range(0,255,fade)`，fade 40→0 于 i<200 每 5 帧减、i>300 每 5 帧增）+ 发屏 + `svc_wait_ticks(1)` → 释放 → 分配 work(0x1F400)/screen(0xFA00)/stage(0xFA00)、载 TAI 条(FDOTHER 3) 与 FDOTHER 56 → `rle_decode(56,0,0,screen,320,-1)`、释放 plate、`bgm_play(4,0)` → 由 `j=dword_53BFB-1` 递减到 0 遍历队伍：记录号 `j(≥2)` / `j==1→0` / `j==0→1`，取 80 字节记录 `rec`、组字节 `grp=rec[7]`，载 FIGANI 组 `3*grp+1`(fig_next)/`3*grp`(fig_base) → `figure_attack_scroll(j,1,fig_base,0,work,screen,portrait)` + `anim_cycle_frame(fig_base,0,work,320)` + 20 帧基组步进（每帧 `gfx_copy_rows`/`anim_cycle_frame(fig_base,-1,…)`/发屏/等待 1）→ 逐子帧 `m<fig_next[2]`：`sub=fig_next+*(u32*)(fig_next+4m+8)`，`gfx_copy_rows`+`res_blit(fig_next,m,work,320,-1)`+发屏+`svc_wait_ticks(sub[6])` → `memmove(stage,screen,0xFA00)`、`dword_53A85=res_load(aDatoDat,dword_53A85,grp)`、`dlg_box_stage(stage,320,5,7,5,5)` → 对话帧预算 `nframes=j?220:440`，逐帧：`gfx_copy_rows(work,320,stage,320,320,200)`、光标闪烁（`cursor` 递减；归零则 `cursor=(util_rand()&0x1F)+40`，`cursor>=2→col=0` 否则 `col=12`）、`rle2_blit(work+3208, dword_53A85+*(u32*)(dword_53A85+col), 320)`、`anim_cycle_frame(fig_base,-1,work,320)`、五条 `vm_run` 文本（`53A79:10`@work+5865、`53A7D:rec[8]+1`@work+5915、`53A79:11`@work+12265、`53A7D:rec[32]+150`@work+12315、`53A79:(n>=220?45:rec[8]+12)`@work+32008(0,20,0)）、发屏、等待 1、`kbd_pending()` 为真则 `j=1`+`kbd_flush()` → 每记录末 `pal_fade_out()` → 收尾释放 screen/stage/fig_next/portrait/work。 |

**陷阱 / 伪代码伪影**：
- **ABI/尾跳**：`int(void)` 原型系 chkstk 伪影；函数体无栈参，出口 `jmp 0x2E2A8` 是共享收尾（`add esp,0x24; pop ebp/edi/esi/ebx; ret`）＝普通 `return`，故 C 取 `void figure_showcase_run(void)`。
- **malloc 尺寸伪影**：Hex-Rays 把 `malloc/memset` 的立即数渲染成 `&loc_36B00`/`&loc_1F400`；实为 `0x36B00`/`0x1F400`。`0x36B00`（其偏移 0x12C30 处写文本）在片头淡入中分块发屏；`0x1F400`＝640 行距 work。
- **`0x36B00` 伪函数条目**：`function-map.csv` 里的 `0x36b00` 条目实为大函数体中部，非独立函数，本轮未碰。
- **记录号映射**：`j!=0 且 j!=1` 用 `(uint8_t)j`，`j==1→0`，`j==0→1`（机器 `bl` 分支），非简单 `j`。
- **末行 vm_run 子号**：`n>=220→45` 否则 `(uint8_t)(rec[8]+12)`（机器 `bl` 单字节，`rec[8]+12` 可能 >255 需回绕）。
- **光标计数器** `cursor`（机器 `[esp+0x1c]` 单字节）与 `col`（0/12）共同选 DATO 子图；`rle2_blit` 目的 `work+dword_53C67(3208)`。
- **伪 callee**：仅误挂 `sub_3702F`(chkstk)；真依赖全部已还原（res/rle/rle2/gfx/fade/svc/vm/dlg/bgm/world_load/kbd/util/anim/figure_attack_scroll）。
- 无对拍器，标“已还原未验证”。

### 92) 收尾演示驱动 `0x31529` → `ending_demo_run`（本轮，1 函数，§5 大单函数条款）→ `src/game/ending_demo.c/.h`（新建）

按 §91 遗留建议、依 playbook §5 的“超预算单函数”例外拉出闭包就绪头名 `0x31529`（1718B，u2）。
它是 `scene_state_26`(0x250CC) 与 `scene_state_29`(0x25757)（两个“结束/战后”离开态）**无参调用一次后即死循环挂起**
的收尾演示；真闭包＝自身（其全部 callee 已在册，逐指令核对了离线反汇编 `31529..31bde`）。

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x31529` | `ending_demo_run(void)` | 收尾演示：`guest_malloc(0x1F400)` work + `guest_malloc(0xFA00)` screen（screen 机器从不释放）；`res_load(FDOTHER,0,54)`→`res_blit(...,0,screen,320,-1)`→`pal_fade_out`→发屏→`pal_fade_in`→`delay(1000)`→`ani_play(2,100,0)`→`pal_fade_add(0,255,63)`→发屏→`res_blit(...,9,VGA,320,-1)`→64 步 `pal_fade_add(0,255,i)`+`delay(4)`→`delay(2000)`→文本页 `dword_53C03==26 ? msg_show_lines(4,17) : (37,2)/(21,3)/(26,4)/(105,5)/(32,6)`→3 轮淡出→`res_blit(...,12..108,VGA,...)`20ms 步进→发屏→文本页 `==26 ? (21,18)/(24,19)/(26,20) : (45,7)`→`delay(2000)`→40 帧循环（`gfx_copy_rows(work+160,640,screen,…)`+两张带 `res_blit(...,n%4+1/5,work+v21/v20,640,-1)`，`v20+=2`/`v21-=2`/`n<25` 再减 2+回写 VGA）→`memmove(work,screen)`+`res_blit(...,1/5,work,320,-1)`+回写→文本页 `==26 ? (32,21)/(36,22)/(32,23) : (32,8)/(36,9)`→200 帧循环（`memmove(work,screen,0xFA00)`+`res_blit(...,ii%4+1/5,work,320,-1)`+`delay(20)`+回写+`pal_fade_range(0,255,v7)`，`ii>135` 起 `++v7`）→`guest_free(work)`+清 VGA+`figure_showcase_run()`+清 VGA→`bgm_play(-1,1)`+`svc_wait_ticks(50)`→`plate=res_load(FDOTHER,0,60)`+`rle_decode`+`pal_fade_in`+`bgm_play(18,0)`+`svc_wait_ticks(80)`+`pal_fade_out`+清 VGA→`pair0_buf=res_load(...,plate,58)`、`pair1_buf=res_load(...,NULL,57)`→20 次循环：`dword_53A65` 临时设为 `pair1_buf`，按表 0x526E9/0x526FD 覆写记录 0 的 `+6/+7` 与记录 1 的 `+86/+87`（`>=0x4C→0 否则 2`；`+7/+87` 取表字节）、`dword_54133=表0x52711[jj]`，`figure_attack_view(0,1)`+`pal_fade_range(0,255,0)`+`svc_wait_ticks(20)`+`res_blit(pair0_buf,jj,VGA,320,-1)`+`svc_wait_ticks(78)`+`pal_fade_out`+清 VGA→还原 `dword_53A65`→`guest_free(pair1_buf)`+`svc_wait_ticks(50)`→`plate=res_load(...,pair0_buf,59)`+`rle_decode`+`pal_fade_in`+`guest_free(plate)`。 |

**陷阱 / 伪代码伪影**：
- **ABI/返回**：`int __cdecl()` 系 chkstk 伪影；函数无栈参，出口 eax＝`free()` 返回值（无调用者使用），故 C 取 `void ending_demo_run(void)`。
- **malloc/memset 立即数伪影**：`malloc(size:&loc_1F400)`＝`0x1F400`；`memset(...,64000)`＝`0xFA00`（320×200 页）。
- **未释放缓冲**：机器只释放 work(0x1F400)、FDOTHER 57、FDOTHER 59；screen(0xFA00) 与 FDOTHER 60/58 从不释放，C 照原样。
- **`dword_53A65` 临时指向**：20 循环内先设回原值（机器 `ebp` 保存）、`figure_attack_view` 前设为 FDOTHER 57 缓冲、之后还原。
- **`dword_53A45` 字段**：循环写记录 0 的 `+6`(kind 0/2)/`+7`(id) 与记录 1 的 `+86`/`+87`（即记录 1 的 +6/+7），阈值 `0x4C`。
- **三张 20 字节只读表**：0x526E9/0x526FD/0x52711（dseg02，`fd2_data1[0x26E9..]`）；第一张机器另在 200 循环写第 6 个 dword（`v16[5]=ii%4`，死写），C 用 24 字节 `pair0_id[6]` 复现。
- **伪 callee**：仅误挂 `sub_3702F`(chkstk)；真依赖全部已还原。
- 无对拍器，标“已还原未验证”。

### 93) 图象/效果总派发器 `0x2FF01` → `figure_effect_play`（本轮，1 函数，**§5 大单函数条款 + 超预算明示允许**）→ `src/game/figure_view.c/.h`（追加）

playbook 批选（闭包就绪、usage 降序、剔除 CRT/库包装）落到唯一游戏内容项 `0x2FF01`（3876B，u2）。
其**真闭包＝自身**（全部 callee 已在册），但 3876B **超出 §5 的 ~2500B 上限**——经主管批准按 A 选项
作为“基石解阻”拉出（源码头、本台账、终报均明示该偏离）。逐指令核对了离线反汇编 `2ff01..30e24`。

**函数语义**：`void figure_effect_play(int unit, int mode, int count, const uint8_t *units)`（cdecl 4 参，`push 0x164` 为 chkstk）。
`mode>=32`→`figure_demo_show`；`mode==24||mode>27`→`figure_party_show`；其余（<24、25..27，实际 0..9）走内联效果：
载入被显示格的 BG/TAI 条（或 `figure_bg_pick`／按 mode 的缺省）、FIGANI `3*rec[7]` 与 `3*rec[7]+2`（若 `*+2==0` 则 +1）、FDOTHER 引擎头像与音效库，
`pal_fade_out` 后 `figure_attack_scroll` 滑入 → `figure_play` 播基础组 → 逐帧驱动 `funcs_30469[mode]`（0x524C6 表，10 项，已由 `fx.c` 全源码化）：
9 帧引导（sel 0/1/2）→ 逐单位 drain 块（sel 3/4/5，把记录 HP 字 `+0x40` 向 `unit_action_damage` 结果抽干）→ 尾段（sel 6/7/8）→ mode 9 的亮出滑；
最后释放全部缓冲、重载 FD2SHAP.DAT/FD2.TMP、清屏、`pal_fade_in`。

**陷阱 / 伪代码伪影**：
- **malloc 立即数伪影**：`malloc(&loc_2A300)`＝`malloc(0x2A300)`（640 步长 work 缓冲）；`guest_malloc(0xFA00)`（screen）。
- **`funcs_30469[a2]` 派发**：机器 `call [eax*4+0x524c6]`，索引＝`ebp`＝**mode 参数**（非 a2 别名）；
  C 侧用 fx.c 十个语义名的本地函数指针表 `fx_table[10]` 复现（不改动已验证的 `fx.c/.h`）。
- **越界帧基伪影**：Hex-Rays 把 `&t88[v72]`（栈上 0x5269C 的 16 字节副本）渲染为 `v42[v72-4]`；
  两张 16 字节 dword 表 `t88`(0x5269C)/`t78`(0x526AC) 与四张字节表 `t98`(0x526BC)/`tbc`(0x526C6)/`tb0`(0x526CF)/`ta4`(0x526D9="RRSTUVWXYZ") 均按镜像地址原样复制，
  仅以 `mode<10` 索引（两个调用点 0x15400 / 0x1D43C 均保证）。
- **未初始化字节**：机器对 `tbc` 只拷 9 字节，`tbc[9]` 在 mode==9 且 `rec[6]!=0` 时会读未初始化栈；按 playbook 6 惯例建模为 0 并注释。
- **共享尾声/伪 callee**：仅误挂 `sub_3702F`(chkstk)；真依赖全部已还原。
- 无对拍器，标“已还原未验证”。

### 94) 单位 AI 决策驱动 + 动作执行器 `0x14EF0`/`0x15311` → `map_ai_decide`/`map_ai_do_action`（本轮，2 函数）→ `src/game/map_ai.c/.h`（追加）

playbook 批选（闭包就绪、usage 降序、剔除 CRT/库包装）落到游戏内容头名 `0x14EF0`(u6，357B)——`map_ai.h`
早已列出的“决策驱动”——连同其唯一仍未还原的 callee `0x15311`(u2，381B)。真闭包＝本 2 函数
（`0x1548E`/`0x15055`/`0x14237`/`0x1598A`/`0x1567E` 等 scorer 与动作执行器均已在册）；逐指令核对了
离线反汇编 `14ef0..1504f` 与 `15311..1548d`。`0x14EF0` 的 `jmp 0x22BBE` 与 `0x15311` 的 `jmp 0x1317D`
类共享尾声均为普通 return。

**函数语义**：
- `0x14EF0` = `int map_ai_decide(int actor, int require_empty)`：先跑三个评分器
  （`map_ai_pick_reach_target`/`map_ai_pick_action_target`/`map_ai_pick_item_target`），再读 `flag=rec[actor].+52&0x40`
  与 `hp_delta=word(rec[actor].+72)-word(rec[dword_53C4B].+74)`，若三项分数（`dword_53C4F`/`dword_53C23`/`dword_53C33`）
  全 <6 则返回 0；否则按分数与 flag/hp 平局规则派发到 `map_attack_run`(0x1548E) / `map_ai_do_action`(0x15311) /
  `map_ai_use_item`(0x15055)，末 `dword_51A83=0` 返回 1。
- `0x15311` = `int map_ai_do_action(int actor, int require_empty)`：`entry=action_entry(dword_53C2F)`（0x4E866/0x619FD）；
  `dword_53C23<6` 直接返回 0；否则滑入肖像→`map_reveal_reachable(53C27,53C2B,list,entry[4],0,filter)`（filter 取 `entry[6]` 或
  其反）→`util_fix_records`→`j___delay(200)`→`dword_51A83=entry[4]+2`→`map_slide_view`→`map_view_update(0)`；
  然后 `dword_53C2F>=10 || byte_53AF9` 走 `ev5_1D4CB`+**funcs_1541F 派发**+`menu_stop_and_free_music`，否则直呼
  `figure_effect_play(actor,dword_53C2F,count,list)`；尾段 `steps=ev5_1B653(hit)`→`map_view_update(0)`→`map_units_animate`
  →`ev_script_run(dword_53C4B,steps,hit)`→`map_view_update(0)`，清 `dword_53EC8`/`dword_51A83` 返回 1。

**陷阱 / 伪代码伪影**：
- **funcs_1541F 派发表**：机器 `call [eax*4+0x51D01]`（28 项，`analysis/dispatch_tables.csv`）；C 侧新建
  `static map_action_fn const action_table[28]`，索引＝`dword_53C2F`，引用 `map_action.h` 的语义名
  （0..27：`map_action_entry00..27`／`map_action_grow_x_cost`(17)/`grow_y_cost`(18)/`widen_cost`(19)/
  `map_action_clear_flag`(25)，槽 23＝`map_units_move_deploy`，槽 24 复用 `map_action_entry16`）。槽 23 的
  `map_units_move_deploy` 原为 `void`，包一层 `map_action_entry23` 返回 0 以统一表类型（返回被调用方丢弃）。
- **未初始化栈参数伪影**：`sub_1D4F6` 在反编译里被渲染成 14 个实参，机器实际不压栈（`call 0x1d4f6`）——按
  语义名 `menu_stop_and_free_music()` 直呼（`map_action.c` 已有同一结论）。
- **局部缓冲布局**：`0x15311` 的输出名单 `v8`＝帧基址（32B，`0x14818` 以字节写），命中块 `v9`＝帧+0x20（12B，
  3 个 3 字节脚本项），`count`＝帧+0x2c；已逐指令按 `esp` 位移还原。
- **有符号比较**：全部分数比较机器用 `jge/jle/jl`（有符号），C 侧显式 `(int)` 转型。
- **`0x1B653` 返回值缺口（已按主管 A 选项修复）**：`0x15311` 需要 `0x1B653(&hit)` 的 EAX 计数（`0x1b64d: mov eax,esi`），
  而 `ev5.c` 的 `ev5_1B653` 原声明为 `void` 把返回值丢弃。经主管批准（A 选项、零调用者、行为保持），
  已把 `ev5.h`/`ev5.c` 的签名改为 `int ev5_1B653(uint8_t *out)` 并 `return count;`（仅签名+return，不动循环）；
  这是对“勿改已验证模块”约束的一次**明示偏离**（该函数在 `src/` 中无任何调用者，改动行为保持）。
  若该改动导致编译/链接问题则回退到本地静态 helper（未发生）。
- **伪 callee**：`0x14EF0`/`0x15311` 均仅误挂 `sub_3702F`(chkstk)；真依赖全部已还原。
- 无对拍器，标“已还原未验证”。

### 95) 记录行动分派器 + 两个回合扫描环 `0x13A9F`/`0x1D80B`/`0x1D8BA` → `map_record_action`/`map_turn_side1`/`map_turn_side0`（本轮，3 函数）→ `src/game/map_turn.c/.h`（新建）

playbook 批选（闭包就绪、usage 降序、剔除 CRT/库包装与伪函数条目）落到游戏内容头名 `0x13A9F`(u3，1021B)——记录行动分派器；
连同它单点解阻的两个无参调用者 `0x1D80B`(u1，175B)/`0x1D8BA`(u1，348B)（二者真闭包＝自身，唯一待还原 callee 就是 `0x13A9F`）。
整簇 3 函数 1544B（≈1500 预算上沿）。逐指令核对了离线反汇编 `13a9f..13e9b`/`1d80b..1d8b9`/`1d8ba..1da15`。

**函数语义**：
- `0x13A9F` = `void map_record_action(int actor, int require_empty)`：记录 `dword_53A45[actor]` 的 +52 低 4 位＝行动码（0..11）；
  `+5 & 5` 置位即空返回。各 case 由 `map_ai_decide`/`map_ai_pick_*` 定目标后提交走位（`map_move_to_marked`/`map_move_to_nearest`/
  `map_move_toward`/`map_attack_run`/`map_ai_do_action`）、滑像（`dlg_portrait_glide`）、失败回退 `item_use`；case 3/5/9 用 +53/+54
  目标格或 +61 标签经 `rec_find`/`map_find_tag_cell`；case 5 命中时按 `dword_53A55 + 3*slot` 的 3 字节条目（+0x53 类型/+0x54 16 位值）
  写 record +49/+50，type==0 调 `rec_slot_claim`，再 `svc_play_sfx2(...,12,1)`+`ev4_12263`+置 +52=7；所有路径经末尾
  `map_scroll_cell_tag(rec[0],rec[1],1)`→`rec_flag_or80`→`dlg_portrait_clear`→`map_view_update(0)`。
- `0x1D80B` = `void map_turn_side1(void)`：遍历 `dword_53BEB` 条记录，对 +6==1 且 `(+5 & 0x81)==0 && +38==0` 者
  `map_record_action(i,1)`；每条再按 `dword_51A8F` 派发场景处理器 `funcs_1199C`、按 `dword_53C03` 派发 `funcs_1197B`；`dword_53ECC!=0` 提前退出。
- `0x1D8BA` = `void map_turn_side0(void)`：第一趟对 +6==0 记录先 `map_ai_pick_action_target(i,0)`+`map_ai_pick_item_target(i,0)`，
  分数（`dword_53C23`/`dword_53C33`）≥6 才 `map_record_action(i,0)`；第二趟只跑 `map_record_action`；两趟同样派发两个处理器表并提前退出。

**陷阱 / 伪代码伪影**：
- **伪 callee（共享尾声）**：`0x13A9F` 的所有出口 `jmp 0x1317D`（落进 `0x13167` 体内 `map_scroll_cell_tag` 调用后的 `add esp,0Ch; pop×4; ret`）＝普通 return；
  `function-map` 因此把 `sub_1300D` 误列为 callee；真依赖全部在册。三者的 chkstk `sub_3702F` 丢弃。
- **返回 void**：`0x13A9F` 的 eax 终值无调用点读取，`fd2_funcs.h` 的 `int` 原型只是 chkstk 伪影；三者 C 侧均声明 `void`（`0x1D80B/0x1D8BA` 的 `int(void)` 同为伪影）。
- **有符号比较**：case 11 与 `0x1D8BA` 的分数阈值机器用 `jge/jl`（有符号），C 侧显式 `(int32_t)` 转型。
- **`dword_53A55`/`dword_53AD5` 标量→指针**：按 `map_event.c` 既有约定 `(uint8_t *)(uintptr_t)dword_53A55`、`dword_53AD5 + 3*tag(+0x53/+0x54)`。
- 无对拍器，标“已还原未验证”。

### 96) 行动菜单驱动器 `0x1CFF0` → `action_menu_run`（本轮，1 函数，§1 闭包就绪游戏头名）→ `src/game/action_menu.c/.h`（追加）

本轮闭包就绪游戏内容头名 `0x1CFF0`(u1，1243B，唯一 caller 是记录交互驱动器 `0x18D8C` 的
`dword_53C57==1` 分支，`0x18D8C` 仍待还原)；其 callee 全部在册，真闭包＝自身，按 §1 同模块簇取走
（`action_menu` 面板家族 `0x17AED/0x1CEED/0x1D51D` 现闭合）：

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x1CFF0` | `action_menu_run(rec)` | 三块 0xFA00 暂存（`dword_53C5B/5F/63`）快照 VGA → `box_open_init`+`action_menu_draw` → `fb_slide_step` 12 步（11→0）滑入 → `dword_53C57=0`；`do { action_menu_select(rec) } while(!=0)`；12 步（0→11）滑出 → 快照回灌 `0xA0000` → `guest_free`×3；`choice==-1` 返回 -1。否则 `rec_collect_slot_bits(rec,slots)`、`e=action_entry(slots[dword_53C57])`、`dword_51A83=e[4]+2`。三分支：`e[3]!=0 && slots==0x17`（双 `map_reveal_reachable`+`map_pick_record`，成功则 `qword_51CF9=qword_53AB1`、`dlg_portrait_glide`）、`e[3]==0`（`map_reveal_reachable(...,dword_51A83-2,0,0)`，`map_pick_record(4或5,...)`）、else（`map_reveal_reachable`+`map_pick_record`，`slots==0x1E` 时 `map_scan_records(lo,hi,list,lo,hi,e[3]-16,1)` 否则 `map_reveal_reachable(...,e[4],0,e[6])`）。收尾 `util_fix_records`；`v9==-1` 则清 `dword_51A83`、`dlg_portrait_glide`、返回 0；否则 `dword_51A83=0`、`map_view_update(0)`、`slots<9||==0x18||>0x1B` → `figure_effect_play`，否则 `ev5_1D4CB`+`pal_flash_entry`+`funcs_1541F[slots](rec,count,list)`+`menu_stop_and_free_music`；再 `map_collect_scripts`+`map_units_animate`+`ev_script_run`，`dword_51A83=1`，返回 1。 |

**陷阱 / 伪代码伪影**：
- **`v17`/`v18` 未初始化（伪代码）**：Hex-Rays 把 `call 0x1b6b7; mov ebx,eax` 的返回写成未初始化 `v17`；
  机器为 `v18=map_collect_scripts(v20)`，C 侧按返回值捕获。
- **分支 B 的自减读序**：`v14 = dword_51A83 - 2` 在 `dword_51A83 = 1` **之前**求值
  （机器 `mov eax,[0x51a83]; sub eax,2; mov [0x51a83],1`）。
- **死判据**：进入 `else if` 后的第二次 `v21[sel]==0x17` 比较在 `e[3]!=0` 时不可达（分支 A 已捕获），
  C 侧按 `e[3]==0 / else` 等价还原。
- **qword_53AB1 拆低/高**：机器分两次 `push [0x53ab5]`/`[0x53ab1]` 作 (x,y)；C 侧
  `(int)qword_53AB1` / `(int)(qword_53AB1>>32)`。分支 C 的 `v22`（高）先存局部再作 `map_scan_records` 的 start_y。
- **指针表 ABI**：`funcs_1541F`（`fd2_globals.h` 的 `void(**)(void)`）实调 `[eax*4+0x51D01]`，实参
  `(rec,count,list)`（与 §94 `map_ai.c` 的 `action_table` 同形）；C 侧显式转型 `(int(*)(int,int,const uint8_t*))`。
- **chkstk**：`push 0x108; call 0x3702F`（不分配帧），丢弃；无伪 callee。
- **归属**：追加进既有 `action_menu`（§82/§83 面板族），头已被伞含，无需改列；无对拍器，标“已还原未验证”。

### 97) 记录交互驱动器 `0x18D8C` → `map_record_interact`（本轮，1 函数，§1 闭包就绪游戏头名）→ `src/game/map_record.c/.h`（新建）

本轮闭包就绪游戏内容头名 `0x18D8C`(u2，800B，记录交互驱动器，§96 取走 `0x1CFF0` 后其
`dword_53C57==1` 分支已解)；真闭包＝自身（callee 全部在册），按 §1 取单函数：

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x18D8C` | `map_record_interact(rec, dir_tiles, reopen)` | 复制 0x51ED5 的 4-dword 补丁索引表 → `dir_tiles[0]=0`、`dword_53EC8=0` → `rec_slot_find(rec,0)`（无槽置 `dir_tiles[0]=1`），否则 `rec_field_byte`→`tbl_ptr(&fd2_data2[0x2AD],23)` 取字节 [11]/[12] 作 `map_reveal_reachable(MAP_X,MAP_Y,NULL,p4,p5,0)` 的范围并 `util_fix_records` → `ev5_173E7`+`map_cursor_patch_expand` → `rec_slot_free==0`/`rec_collect_slot_bits==0`/`rp[39]!=0` 置 `dir_tiles[1]/[2]` → `ev5_173E7` → `do { map_cursor_dir_select } while(==0)` → `map_cursor_patch_collapse`+`map_view_update(0)`；`==-1` 返回 -1。随后按 `dword_53C57`：0 → `guest_malloc(0x64)`+`map_reveal_reachable`+`map_pick_record`+`util_fix_records`+`guest_free`，`!=-1` 则肖像族（`dlg_portrait_find`/`ev5_1F04A`/`figure_attack_view`/`dlg_portrait_clear`）+`map_collect_scripts`+`map_units_animate`+`ev_script_run`，末 `rec_flag_or80`+`kbd_flush`，返回 1（取消则 `map_slide_view` 返回 0）；1 → `action_menu_run` 环，成功 `rec_flag_or80` 后 `dword_53EC8 /= (rp[32]>8 ? rp[33]+30 : rp[33])`；2 → `map_action_interact` 环并清 `dword_53EC8`；其他 → `reopen==0` 时 `item_use` + `map_cell_event` + `rec_flag_or80`。 |

**陷阱 / 伪代码伪影**：
- **第二参是调用者的 4-int 补丁数组**：Hex-Rays 视作 `int*`，既是 `map_cursor_patch_*`/
  `map_cursor_dir_select` 的 `b_tiles`，也承载输出状态位（[0] 无槽 / [1] 受阻 / [2] 无空格）。
  调用点（`0x18a82`/`0x18b33`，均在 `0x18890` 内）压 `(rec, &local16, 0|1)`。
- **签名除法**：`dword_53EC8 /= v18` 机器为 `cdq; idiv ebx`（有符号），而 `dword_53EC8` 在
  `fd2_globals.h` 是 `uint32_t`，故 C 侧显式写 `(uint32_t)((int32_t)dword_53EC8 / v18)`。
- **`rp[32] > 8` 比较**：Hex-Rays 渲染成无符号 `> 8u`，机器为 `jle`（有符号）；字节经 `movzx`，两者等价。
- **chkstk**：`push 0xB0; call 0x3702F`（不分配帧），丢弃；无伪 callee（真闭包＝自身）。
- **归属**：新建 `map_record`（单函数驱动器）；头加进伞含（`map_record.h` 排在 `map_records.h` 前）。
  无对拍器，标“已还原未验证”。

### 98) 队伍变更序列的两个记录助手 `0x1A813`/`0x1A866`（本轮，2 函数）→ `src/game/party_change_seq.c/.h`（新建）

本轮取走闭包就绪游戏内容项 `0x1A813`(u3，83B) 与 `0x1A866`(u3，439B)——两队变更编排者
`0x1A30B` 的最后两个待还原 callee，取走后 `0x1A30B` **转为闭包就绪**（下批头名）：

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x1A813` | `party_change_wave_dispatch(phase)` | 遍历 `dword_53A55` 的 16 条 3 字节波形记录（`ev2/ev3/ev6` 场景脚本写入）：记录 +0 是波次号、+2 是相位；对 +0 等于当前波次计数 `dword_53BEF` 且 +2 等于 `phase` 的记录，调用 `funcs_1199C[+1](0)`（与 `map_turn.c` 同一张场景处理表）。 |
| `0x1A866` | `party_change_class_apply(cls)` | 对 `dword_53A45`（计数 `dword_53BEB` 条、步长 80）中 +6 状态类＝`cls` 且 +5 bit0 为 0 的记录两趟处理：趟1 对 +37 异常计数非零者把当前 HP(+0x40) 减去 `max_hp(+0x42)/10`（下限 0），并播肖像/VM 帧；随后 `map_units_animate()`＋`funcs_1197B[dword_53C03](0)`；趟2 对 6 个 +34..+39 异常计数逐个递减，归零时同帧序列后再 `unit_metrics(rec)`。 |

**陷阱 / 伪代码伪影**：
- **未初始化局部是伪影**：`0x1A866` 的反编译把第二趟比较的类写成一个从未赋值的 `v10`；但 `0x1A8A3`/`0x1A988`
  机器指令比较的是记录 +6 与 `[esp+0x14]`。Watcom 栈探针 `0x3702F`（`xchg eax,[esp+4]; call 0x37042; ret 4`）
  **不分配帧**，故四处 `push reg` 后 `[esp+0x14]` 恰为唯一 cdecl 实参 `cls`（非未初始化寄存器）。C 侧写 `cls`。
- **共享尾声**：`0x1A813` 尾 `jmp 0x190A9`（`pop esi; pop ebx; ret`，其只压 ebx/esi）、`0x1A866` 尾
  `jmp 0x190A7`（`pop ebp/edi/esi/ebx; ret`，其压 ebx/esi/edi/ebp）均落在 `0x190AC` 函数体内的公共尾声,
  是普通 `return`，非调用/非依赖。
- **精确比较**：`0x1A813` 把 3 字节记录的波次/相位字节用 `movzx` 零扩展后与 32 位 `dword_53BEF`/实参比较,
  C 侧写 `(uint32_t)…`；`0x1A866` 同样把 `rec[6]` 零扩展后与 `cls` 比较。
- **chkstk**：两者皆 `push size; call 0x3702F`，丢弃。真闭包＝自身（callee 全部已还原）。
- **归属**：新建 `party_change_seq`（`0x1A30B` 序列的助手；头加进伞含，排在 `party_panel.h` 前）；无对拍器，标“已还原未验证”。

### 99) 队伍变更序列编排者 `0x1A30B` → `party_change_map_seq`（本轮，1 函数，§1 闭包就绪游戏内容头名）→ `src/game/party_change_seq.c/.h`（追加）

本轮取走闭包就绪游戏内容头名 `0x1A30B`(u3，1202B)——§98 两个助手（`party_change_wave_dispatch`/
`party_change_class_apply`）unblock 的编排者；真闭包＝自身（callee 全部已还原，含 `0x13565`/`0x16F55` 是
caller 而非 callee）。函数体：

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x1A30B` | `party_change_map_seq()` | 地图屏“队伍已变更”序列：① `svc_wait_ticks(1)`；② 趟1 对 +6==2、+5&0x81==0、+37/+38==0 且 HP(+0x40)≠max(+0x42) 的记录画 253 号“变更”精灵并置 `flag`，再 `gfx_copy_rows(0xA0504,…)`，有变更则 `svc_play_sfx(dword_53EEC,4,1)`，`svc_wait_ticks(0)`；③ 趟2 同条件者 HP += max/5（截断到 max），画 0 号精灵、`rec_flag_or80(rec)`；④ `gfx_copy_rows`→`svc_wait_ticks(0)`→`map_view_update(0)`→相位1 波/类分派；⑤ `dword_53ECC!=0` 即 `return`（共享尾声）；否则 `reveal_bank_load`→`map_turn_side1`→`reveal_bank_free`；⑥ 队伍音轨变化时 `bgm_play(-1,0)`，`reveal_mosaic_out(82)`→`j___delay(20)`→`reveal_mosaic_in(82)`→`ev5_13536`→`map_view_update(0)`→相位0 分派；⑦ `bgm_play(byte_51E81[idx],0)`→`reveal_bank_load`→`map_turn_side0`→`reveal_bank_free`；⑧ `dword_53BEF++`，再 `reveal_mosaic_out(80)`→`j___delay(150)`→`reveal_mosaic_in(80)`→`dword_51A83=0`→`ev5_13536`→`map_view_update(0)`→`bgm_play(byte_51E63[idx],0)`；⑨ 9 帧 `res_draw_subimage(dword_53A81,0xA0000,320,120,84,k+83)`，k>6 时 `dlg_draw_number(0xA726B,320,dword_53BEF,42,3)`，`j___delay(70)`（k==8 再 500），`dlg_snap_restore`；⑩ `m=2..4,9` 帧 `res_draw_subimage(dword_53A81,dword_53A49+0x8088,456,116,m*m+84,91)`＋`dlg_draw_number(dword_53A49+0x812F+456*m*m,…)`＋`gfx_copy_rows`＋`svc_wait_ticks(1)`＋`dlg_snap_restore`；⑪ 收尾 `map_view_update(0)`→`j___delay(200)`→`dword_53AE9=0`→相位2 分派→`dword_51A83=1`→`dlg_portrait_glide(0)`→`kbd_flush()`。 |

**陷阱 / 伪代码伪影**：
- **未初始化局部＝伪影**：反编译把第二次 `svc_wait_ticks` 的实参写成 `a1:0`，实为 `push DWORD PTR [esp]`，
  即函数开头 `mov DWORD PTR [esp],0` 且**此后从未改写**的局部 dword——恒为 0。C 侧直接写 `0`。
  （同理 `flag` 为 `[esp+4]` 的单字节局部，初值 0，趟1 命中置 1，0x1A3CE 读回。）
- **循环变量改写**：m 循环机器在 `m==5` 时 `mov esi,9; jmp 循环体`（**跳回体**而非自增），故实际迭代
  `m=2,3,4,9`；C 的 `for(m=2;m<6;m++){ if(m==5) m=9; … }` 逐行为等价。
- **共享尾声**：尾 `jmp 0x10B46` 与 5 处 `dword_53ECC!=0` 的 `jne 0x10B46` 都落在 `0x10B46` 的公共尾声
  （`add esp,8; pop ebp/edi/esi/ebx; ret`，恰配本函数 `push ebx/esi/edi/ebp; sub esp,8` 的帧）＝普通
  `return`，非依赖。C 侧用早退 `return`。
- **命名冲突**：`party_change_run` 已被 `party_roster.c` 的 `0x2AF28`（队伍变更**选择屏**）占用，二者不同，
  故本函数命名 `party_change_map_seq`（地图屏序列），避免伞含头原型冲突。
- **chkstk**：`push 0x34; call 0x3702F` 丢弃。真闭包＝自身；无对拍器，标“已还原未验证”。

### 100) 队伍变更序列触发检查 `0x13565` → `party_change_maybe_run`（本轮，1 函数，§1 闭包就绪游戏内容头名）→ `src/game/party_change_seq.c/.h`（追加）

本轮取走闭包就绪游戏内容头名 `0x13565`(u1，120B)——§99 取走 `0x1A30B` 后其唯一待还原
callee 已解（真闭包＝自身；`0x3702F` 是 chkstk 探针）。函数体：

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x13565` | `party_change_maybe_run()` | 遍历 `dword_53A45`（计数 `dword_53BEB`、步长 80）的记录：若存在 +5 标志字节 bit0/bit7 均未置位、+6 状态类＝2 且 +38 异常计数＝0 者则置 `v0=0`（跳过）；遍历结束仍 `v0==1` 时把 `byte_51AAC=0`/`dword_51A83=0`，调用 §99 的 `party_change_map_seq()`，再置回 `1`。唯一调用者＝事件 tick `0x117E7`（在行动/状态处理之后）。 |

**陷阱 / 伪代码伪影**：
- **chkstk**：`push 8; call 0x3702F` 不分配帧；真闭包＝自身。
- **记录地址**：`mov edx,[idx]; shl eax,2; lea ebx,[edx+eax] = idx*5; shl ebx,4 = idx*80`，即 `REC_STRIDE` 步长；`+38`＝偏移 `0x26`，机器用 `movzx`+`test` 判零，C 侧同为 `== 0`。
- **归属**：追加进既有 `party_change_seq`（不新增头、无需改伞含——本模块头已在伞含；调用者 `0x117E7` 仍以 IDA 名 `sub_13565` 引用，按既有约定不在新头加 `#define` 别名）；无对拍器，标“已还原未验证”。

### 101) 记录交互动画驱动器 + 帧泵 `0x18890`/`0x18B84` → `map_record_engage`/`map_wait_anim`（本轮，2 函数，§2 最小闭包）→ `src/game/map_record_engage.c/.h`（新建）

本轮闭包就绪游戏内容项已空——§100 取走 `0x13565` 后待还原只剩 CRT/库包装（`0x3645D`(u4)/`0x365DA`(u2)/`0x36C56`(u1)/`0x365AB`，
既定策略跳过）与伪函数条目（`0x10000`/`0x36B00`）。故按 §2 取“最小传递待还原闭包”者：`0x18890`(u1，756B) 的闭包＝
`{0x18890, 0x18B84}`(共 989B ≤ §1 预算)，`0x18B84` 是 `0x18890` 的唯一待还原 callee。函数体：

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x18890` | `map_record_engage(rec)` | 地图屏“确认选中记录”驱动器（唯一调用者＝事件 tick `0x117E7`）。① `dword_53C53=0`/`dword_51A8F=255`，复制 `0x53F12` 的 4-dword 补丁数组 → `dirbuf`；`size=rec[59]`、`profile=rec[32]`（`rec_skip`→19，`rec[7]==28`→16）。② `cost_row=0x4E8A5(profile)`（表 `0x61646` 步长 20）→ `guest_malloc(size)` → `unit_mark_nearby(1)` → `path_mark(cost_row,MAP_X,MAP_Y,size,dword_53A51,dword_53A69)` → `map_exclude_record_cells(rec,1)`。③ `map_wait_anim(rec)`（现帧泵）→ `map_pick_record(4,0,0)` → `util_fix_records`；`==-1` 则 `map_slide_view(MAP_X,MAP_Y)`＋`free`＋返回 1。④ 否则 `unit_mark_nearby(1)` → `path_find(cost_row,low,high,size,p,MAP_X,MAP_Y,0,…)` → `dword_51A83=0`/`map_slide_view`/`=1`：`n==0` 走 `map_record_interact(rec,dirbuf,0)` 环、`n==255` 直接返回 1，`n` 其余 `map_scroll_path(rec,p,n)`＋`dlg_portrait_clear()`＋（`rec[7]`∉{18,19,34} 置 `dirbuf[1]=1`）＋`map_record_interact(rec,dirbuf,1)` 环；成功即 `map_scroll_cell_tag(rec[0],rec[1],1)`＋`free`＋返回。`dword_53C53!=0`（中止）落公共尾 `rec_flag_or80(rec)`＋`map_scroll_cell_tag`＋返回 `dword_53C53`。 |
| `0x18B84` | `map_wait_anim(rec)` | `0x18890` 取记录前的共享帧泵：`while(!kbd_pending())` 每轮 `pal_anim_step()`，BIOS tick 变化时再 `pal_anim_step()`→`anim_frame_step()`→`map_render_view(dword_53A49+0x8088,456,13,8,qword_53AA9)`→`map_reveal_cursor()`→`dlg_portraits_refresh()`→`ui_status_panel(dword_53A49+0x8088+width,456,rec)`→`map_draw_cursor`→`gfx_copy_rows(0xA0504,320,dword_53A49+0x8088,456,0x138,192)`；`width= dword_53AB9>=7 ? 2285 : 2436`。 |

**陷阱 / 伪代码伪影**：
- **共享尾声＝伪 callee**：`0x18B84` 尾 `jmp 0x18B7F` 落在 `0x18890` 函数体内的公共尾声（`pop ebp/edi/esi/ebx; ret`，两函数的帧共 4 个 `push`），
  是普通 `return`，故 map 把 `0x18890` 列为 `0x18B84` 的 callee 属伪依赖；也正因此 `0x18B84` 实为 `void`（Hex-Rays 的 `int`＋`JUMPOUT(0x18B7F)` 是伪影），真闭包＝`{0x18890,0x18B84}`。
- **未初始化寄存器 tick 基线**：`0x18B84` 首次比较的 `esi` 从未写入（首轮与 `[0x46C]` 比较后才在体尾 `movsx esi,WORD[0x46C]`），
  按 §6 先例建模为 `0`（首轮即重绘），C 侧 `int baseline = 0` 并注释。
- **255 路径泄漏**：`path_find` 返回 255（无路）时 `0x18890` 直接 `return 1` 而不 `free` 缓冲区——原版即泄漏，C 侧逐字节复现并注释。
- **状态字节**：`0x13A44`(`map_scroll_cell_tag`) 三调用点的返回值均不读；`rec[0]`/`rec[1]` 为 `movzx` 后的字节，`n==0` 分支回写 `v9[0]=low`/`v9[1]=high` 亦按字节。
- **chkstk**：`push 0x68; call 0x3702F`（`0x18890`）与 `push 0x2C; call 0x3702F`（`0x18B84`）均不分配帧，丢弃。
- **BIOS tick 读**：`movsx eax,WORD[0x46C]` 经低存镜像（`dos_lowmem_base`，与 `ev5.c` 一致），C 侧 `BDA_W(0x46C)`。
- **归属**：新建 `map_record_engage`（`0x18890` 记录交互动画驱动器＋其帧泵）；头加进伞含（排在 `map_record.h` 与 `map_records.h` 之间）；
  调用者 `0x117E7` 仍以 IDA 名 `sub_18890` 引用，按既有约定不在新头加 `#define` 别名。无对拍器，标“已还原未验证”。

### 102) 游戏内菜单簇 `0x16F55`/`0x1728C`/`0x19DF7` → `sys_menu_run`/`sys_menu_audio`/`sys_menu_system`（本轮，3 函数，§2 最小传递待还原闭包）→ `src/game/sys_menu.c/.h`（新建）

本轮闭包就绪游戏内容项仍为空——§101 之后待还原只剩 CRT/库包装（`0x3645D`/`0x365DA`/`0x36C56`/`0x365AB`，既定策略跳过）
与伪函数条目（`0x10000`/`0x36B00`）。故按 §2 取“最小传递待还原闭包”：互递归簇 `0x16F55`/`0x1728C`/`0x19DF7`（u1）——
`0x16F55` 调 `0x19DF7` 与 `0x1728C`，二者又回指 `0x16F55`，闭包＝三者自身（共 2470B，3 函数 ≤ §4 的 8 函数上限）。函数体：

| 原址 | 新名 | 要点 |
|---|---|---|
| `0x16F55` | `sys_menu_run()` | 外层地图命令菜单：快照两张 4-int 光标表 `0x51E9F`/`0x53EF2`，跑方向补丁选单（`map_cursor_patch_expand`→`map_cursor_dir_select`→`map_cursor_patch_collapse`）→ `map_view_update(0)`；`dword_53C57`＝-1 直接 `return 1`，否则分派：0→`sys_menu_system`；1→记录交战序（`msg_open_portrait(rec[7])`＋`vm_run(417/418)`＋`map_window_run`＋对 `dword_53BEB` 条记录的 `map_slide_view`/`map_move_toward`/`funcs_1199C` 派发/`dlg_portrait_clear`/`rec_flag_or80` 环＋`party_change_map_seq`）；2→`sys_menu_audio`（`return 0`）；3→另一对话序（`vm_run(419/420)`＋`party_change_map_seq`）；余 `return 0`。 |
| `0x1728C` | `sys_menu_audio()` | 四个声音开关（`byte_51E61` 音乐/`byte_51E62` 音效/`byte_53AF9`/`byte_51AAB`）：每轮由四标志重建 `0x51EAF` 补丁表（值 18/20/22/24，`==0` 与 `!=0` 决定 +0/+1）→ 选单 → 翻转命中的开关；音乐开关走 `AIL_set_sequence_volume(0x3B124)`（开 127／关 0，1000 ms）。选单返回 -1 则 `return`。 |
| `0x19DF7` | `sys_menu_system()` | 存档/读档/队伍面板菜单：①探测 FD2.SAV（`fopen`+读 0x59CB 到临时缓冲后**丢弃**，仅取 open 结果；不存在则 `b_tiles[2]=1`）；②扫描 `dword_53BEB` 条记录，凡 +5 位 0 清零且位 7 置位者 `b_tiles[1]=1`；③选单：0→`party_panel_run`（`return 1`）；1→存盘（分配 0x59CB，读入现档 `aFd2Sav_0`→`util_deobfuscate`，写回头 0x30D5 字节区＋`0x59C7`＝`util_sum_tail4`→再 `util_deobfuscate`→`fwrite` 到 `aFd2Sav_1`）后 `vm_run(411)`，取消/失败 `vm_run(412)`，`kbd_flush`＋`return 1`；2→读档（`vm_run(413/414)`＋`bgm_play(-1,0)`＋`scenario_load()`）或 `vm_run(412)`＋`return 1`；余→退出序列（`vm_run(415/416)`＋`bgm_play(-1,1)`），确认 `return -1`，取消走共享尾（`return 1`）。 |

**陷阱 / 伪代码伪影**：
- **共享帧＝跨函数尾声**：三函数帧布局完全相同（`push ebx/esi/edi; sub esp,20h`＋前置 Watcom 栈探针），故 `0x1728C`/`0x19DF7` 尾 `jmp 0x16FDD` 落入 `0x16F55` 的公共尾声（普通 `return`），
  `0x19DF7` 的默认（退出）分支取消路径更直接跳进 `0x16F55` 的**函数体** `0x1716F` 执行其“取消帧”（`vm_run(412)`＋`j___delay(200)`＋`ev4_196CB`）后再 `return 1`。C 侧以私有 helper `sys_menu_cancel_frame()` 复现该共享帧，调用者返回 1（诚实标注）。
- **chkstk**：`push 0x54/0x3C; call 0x3702F`（三函数）均不分配帧，丢弃。
- **16 字节局部＝`rep movs` 表拷贝**：`0x16F55` 拷 `0x51E9F`/`0x53EF2`，`0x1728C` 拷 `0x51EAF`/`0x53F02`，`0x19DF7` 拷 `0x51EF5`/`0x53F22`（均在 dseg02 `fd2_data1`）；`0x1728C` 的 `[esp+0x10]` 四槽随即被菜单值覆盖。
- **`0x177FC` 第二实参丢失**：`0x16FA9`/`0x17334`/`0x19EC6` 处机器压入两张 4-int 表（Hex-Rays 只留一个），按 `map_cursor.h`（同族 `0x1BBDC` 已还原）用 `map_cursor_dir_select(a_tiles, b_tiles)`。
- **无副作用探测**：`0x19DF7` 首次读 FD2.SAV 到新缓冲后立即 `free` 而不使用（仅判断存在性）；存盘时现档尾部（`0x30D5..0x59CB`）保留原文件内容，无档时该段未初始化——原版如此，C 侧逐字节复现。
- **CRT/堆缝**：`fopen/fread/fwrite/fclose` 走 libc；`malloc/free`（`0x3706E`/`0x3776E`）走 `guest_malloc`/`guest_free`；`AIL_set_sequence_volume`（`0x3B124`）按 `bgm.c`/`svc.c` 惯例以原址函数指针调用。
- **归属**：新建 `sys_menu`；头加进伞含（排在 `svc.h` 与 `tables.h` 之间）；调用者 `0x117E7` 仍以 IDA 名 `sub_16F55` 引用，不在新头加 `#define` 别名。无对拍器，标“已还原未验证”。

### 验证与台账

- `tools/check_compile.sh --full`：双 profile 0 错误、`-Werror` 双 profile 全绿、伞头自检 OK。
- `function-map.csv`：本轮 §102 取走 `0x16F55`/`0x1728C`/`0x19DF7`（§2 最小传递待还原闭包），待还原 **17 → 14**（−3），已还原未验证 **306 → 309**（+3）。
- **闭包就绪游戏内容项**：本轮 §102 取走互递归菜单簇 `0x16F55`/`0x1728C`/`0x19DF7` 后，余下待还原仅 CRT/库包装
  （`0x3645D`(u4)/`0x365DA`(u2)/`0x36C56`(u1)/`0x365AB`，既定策略跳过）与伪函数条目（`0x10000`/`0x36B00`）。无独立游戏内容余项。
- `function-map.csv`：本轮 §101 取走 `0x18890`/`0x18B84`（§2 最小传递待还原闭包），待还原 **19 → 17**（−2），已还原未验证 **304 → 306**（+2）。
- **闭包就绪游戏内容项**：本轮取走 `0x18890`/`0x18B84` 后，闭包就绪待还原**已无游戏内容项**——只剩 CRT/库包装
  （`0x3645D`(u4)/`0x365DA`(u2)/`0x36C56`(u1)/`0x365AB`，既定策略跳过）与伪函数条目（`0x10000`/`0x36B00`）；
  其余待还原游戏内容项均需先解自身闭包：音频簇 `0x16F55`/`0x1728C`/`0x19DF7`(u1，共 2470B，§5 超预算)。
- `function-map.csv`：待还原 **30 → 28**（本轮 §94 −2），已还原未验证 **293 → 295**（本轮 +2）。
- `function-map.csv`：待还原 **28 → 25**（本轮 §95 −3），已还原未验证 **295 → 298**（本轮 +3）。
- `function-map.csv`：待还原 **25 → 24**（本轮 §96 −1），已还原未验证 **298 → 299**（本轮 +1）。
- `function-map.csv`：待还原 **24 → 23**（本轮 §97 −1），已还原未验证 **299 → 300**（本轮 +1）。
- `function-map.csv`：待还原 **23 → 21**（本轮 §98 −2），已还原未验证 **300 → 302**（本轮 +2）。
- `function-map.csv`：待还原 **21 → 20**（本轮 §99 −1），已还原未验证 **302 → 303**（本轮 +1）。
- `function-map.csv`：待还原 **20 → 19**（本轮 §100 −1），已还原未验证 **303 → 304**（本轮 +1）。
- **闭包就绪游戏内容项**：本轮 §100 取走 `0x13565`（§99 `0x1A30B` 的触发检查，`0x117E7` 的最后待还原 callee）后，
  闭包就绪待还原**已无游戏内容项**——仍只剩 CRT/库包装（`0x3645D`(u4)/`0x365DA`(u2)/`0x36C56`(u1)/`0x365AB`，既定策略跳过）
  与伪函数条目（`0x10000`/`0x36B00`）；其余游戏内容项均需先解自身闭包：互递归簇 `0x18890`/`0x18B84`(u1，共 989B，闭包就绪于再下游)，
  音频簇 `0x16F55`/`0x1728C`/`0x19DF7`(u1，共 2470B，§5 超预算)。
- **闭包就绪游戏内容项**：本轮 §99 取走闭包就绪游戏内容头名 `0x1A30B`（队伍变更序列编排者）后，
  闭包就绪待还原**已无游戏内容项**——只剩 CRT/库包装（`0x3645D`(u4)/`0x365DA`(u2)/`0x36C56`(u1)/`0x365AB`，
  既定策略跳过）与伪函数条目（`0x10000`/`0x36B00`）；其余待还原游戏内容项（`0x13565`(u1)/`0x16F55`(u1) 等）
  仍需先解其自身闭包。
- **闭包就绪游戏内容项**：本轮 §98 取走 `0x1A813`/`0x1A866`（`0x1A30B` 的最后两个 callee）后，
  游戏内容闭包就绪头名回升为 `0x1A30B`(u3，1202B，队伍变更编排者)；其余仍只剩 CRT/库包装
  （`0x3645D`(u4)/`0x365DA`(u2)/`0x36C56`(u1)/`0x365AB` 等，既定策略跳过）与伪函数条目（`0x10000`/`0x36B00`）。
- **闭包就绪游戏内容项**：本轮 §97 取走记录交互驱动器 `0x18D8C`（u2，末个闭包就绪游戏头名）后，
  闭包就绪待还原**已无游戏内容项**——只剩 CRT/库包装（`0x3645D`(u4)/`0x365DA`(u2)/`0x36C56`(u1)/`0x365AB`，既定策略跳过）
  与伪函数条目（`0x10000`/`0x36B00`）；其 caller `0x18890`(u1，闭包 `18890/18B84/18D8C` 共 1789B 略超 §1 预算)/`0x1A813`/`0x1A866` 待还原。
- **闭包就绪游戏内容项**：本轮 §96 取走闭包就绪游戏头名 `0x1CFF0`（行动菜单驱动）后，
  闭包就绪待还原已无游戏内容项（`0x18D8C` 的 `dword_53C57==1` 分支依赖已解，但它自身仍被 `0x18890`/`0x1A813`/`0x1A866` 等阻挡）；
  其余只剩 CRT/库包装（`0x3645D`(u4)/`0x365DA`(u2)/`0x36C56`(u1)/`0x365AB`，既定策略跳过）与伪函数条目（`0x10000`/`0x36B00`）。
- **闭包就绪游戏内容项**：本轮 §95 取走闭包就绪头名 `0x13A9F`（及其单点解阻的 `0x1D80B`/`0x1D8BA`）后，
  闭包就绪游戏内容头名回落至 `0x1CFF0`(u1，1243B，行动菜单驱动)；`0x1A30B`(u2)/`0x13565`(u1) 等上游仍被 `0x1A813`/`0x1A866` 阻挡。
  其余仅剩 CRT/库包装（`0x3645D`(u4)/`0x365DA`(u2)/`0x36C56`(u1)/`0x365AB`，既定策略跳过）与伪函数条目（`0x10000`/`0x36B00`）。
- **已验证模块签名修复**：`ev5.c` 的 `ev5_1B653` `void→int`（见 §94 末条），经主管批准、零调用者、行为保持。
- **闭包就绪游戏内容项**：本轮 §94 一并取走 `0x14EF0` 与 `0x15311` 后，闭包就绪游戏内容头名回落至
  `0x1CFF0`(u1，1243B，行动菜单驱动，`0x1CFF0` 的 callee `0x1D6C8` 已在 §59 还原) 与受它阻挡的
  `0x18D8C`(u2，800B，记录交互驱动器)；其余仍只剩 CRT/库包装
  （`0x3645D`(u4)/`0x365DA`(u2)/`0x36C56`(u1)/`0x365AB`，既定策略跳过）与伪函数条目（`0x10000`/`0x36B00`）。
- **闭包就绪游戏内容项**：本轮 §93 取走 `0x2FF01`（§5 超预算明示允许的基石解阻）后，闭包就绪待还原已无游戏内容项——
  只剩 CRT/库包装（`0x3645D`(u4)/`0x365DA`(u2)/`0x36C56`(u1)/`0x365AB`，既定策略跳过）与伪函数条目
  （`0x10000`/`0x36B00`）。`0x2FF01` 已拉出后，此前受它阻挡的 `0x15311`/`0x1CFF0`/`0x18D8C` 等上游闭包相应前移
  （见下条）。

> **台账更正**：旧台账称 `0x2FF01` 的“真闭包含 `0x1C75E`/`0x14EF0` 等大件，>8 函数，按 §4 跳过”——经本轮核验为**过期信息**：
> `0x1C75E` 早已还原，且 `0x14EF0` 并不在 `0x2FF01` 的 callee 表内；`0x2FF01` 真闭包＝自身。

> **台账发现**：`0x1CEED` 的 caller `0x1CFF0`（行动菜单驱动）仍待还原，其真闭包含 `0x1AA1D`/`0x1DB65`
> 等大件（>8 函数，按 §4 跳过）；另一个 caller `0x1D51D` 已在 §83 还原。

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
本轮 §44 拉出 `0x31266`(fx_play)；本轮 §45 把地点/商店菜单族的同层四件 `0x28CBD`/`0x28EFE`
（`sub_279BC` 的两个 callee）与 `0x2A43E`/`0x2A07A`（`sub_29DAA`→`sub_2A43E` 链）整体拉出
（`shop_screen.c` 追加），故 `sub_279BC` 待还原 callee 由 4 降为 2（余 `0x2872B`/`0x28F65`）、
`sub_2A43E` 与 `0x28EFE` 整函数闭合、`sub_29DAA` 待还原 callee 由 5 降为 4；下一游戏头名仍为 u1 小簇
`0x1D6C8`(sub_1CFF0)/`0x1DEBE`(sub_14237)/`0x24BDE`(sub_24754)/`0x28632`(sub_2825B)/`0x2F7B6`(sub_2EBE1) 等。
本轮 §51 拉出 `funcs_1541F` 低号条目余部 `0x21396`/`0x21449`/`0x21462`/`0x2147B`（条目 4/5/6/7）
+ 条目 9 `0x214AD` 与其共享体 `0x213B7`（`map_action.c` 追加），故该表 0..9 条目仅剩独立效果体
`0x2111A`(u2)/`0x211A4`(u2)（均为动态 action 索引、由 `sub_20C6F` 直接调）；下一游戏头名回落至
`0x2111A`(u2)/`0x211A4`(u2) 及 u1 小簇 `0x1D6C8`(sub_1CFF0)/`0x24BDE`(sub_24754)/`0x28632`(sub_2825B)/`0x2F7B6`(sub_2EBE1) 等。
本轮 §52 拉出该表余下的两个动态 action 体 `0x2111A`(u2)/`0x211A4`(u2)（`map_action.c` 追加），
故 `funcs_1541F` 相关低号条目/自由体全部闭合；下一游戏头名回落至 u1 小簇
`0x1D6C8`(sub_1CFF0)/`0x24BDE`(sub_24754)/`0x28632`(sub_2825B)/`0x2F7B6`(sub_2EBE1) 等。
本轮 §55 拉出闭包就绪的同表头名 `0x21B99`/`0x2211C`/`0x22153`(u3，条目 14/15/16，槽 24 复用
`0x22153`)，故 `funcs_1541F` 条目 14/15/16/24 闭合；余 17/18/19/25 已在 §15/§22 还原、23 已在 §36，
**全表 28 条（0..27）至此全部源码化**；下一游戏头名仍为 u1 小簇
`0x1D6C8`(sub_1CFF0)/`0x24BDE`(sub_24754)/`0x28632`(sub_2825B)/`0x2F7B6`(sub_2EBE1) 等。
本轮 §56 拉出 `map_cursor` 家族余下的 key loop 三元组 `0x177FC`/`0x17898`/`0x179D5`（同 caller 族，
§20 已点名的 key loop），故 §20 五条方向光标驱动器只剩其自身与 `0x16F55`（其余均已闭包就绪）；
下一游戏头名仍为 u1 小簇 `0x1D6C8`(sub_1CFF0)/`0x24BDE`(sub_24754)/`0x28632`(sub_2825B)/`0x2F7B6`(sub_2EBE1) 等。
本轮 §57 拉出闭包就绪头名 `0x2825B`(u2) 及其唯一 callee `0x28632`（`shop_rows` 的 party 行绘制同族，
新建 `shop_party_stats`），故该 u1 小簇里的 `0x28632`(sub_2825B) 已闭合；下一游戏头名回落至
`0x1D6C8`(sub_1CFF0)/`0x24BDE`(sub_24754)/`0x2F7B6`(sub_2EBE1) 等。
本轮 §58 继而拉出 §57 遗留的预览屏驱动器 `0x27F4A`(u1)，`shop_party_stats` 家族闭合；
本轮 §59 把 u1 小簇三元组 `0x1D6C8`(sub_1CFF0)/`0x24BDE`(sub_24754)/`0x2F7B6`(sub_2EBE1)
整体拉出（`fade.c`/`party_roster.c`/`map_attack.c` 追加），故 `sub_24754`/`sub_2EBE1` 现闭包就绪；
下一游戏头名回落至 u1 小簇 `0x1B1E7`(sub_19DF7，内层读未初始化，暂跳)/`0x1BBDC`/`0x1CFF0`(sub_18D8C)
及商店族 `0x2872B`(sub_279BC)/`0x2AA00`(sub_29DAA) 等，CRT/库包装 `0x3645D`(u4)/`0x2E95B`(u3)/`0x365DA`(u2) 仍按既定策略跳过。
本轮 §60 把该列表里改判为游戏内容的 ANI.DAT 解码簇 `0x36E3D..0x36FAC`+`0x36FD3`/`0x36FF4` 整体拉出（新建 `ani`），故闭包就绪游戏头名转为 `0x1B1E7`/`0x1BBDC`/`0x1CFF0`/`0x2000A`/`0x2872B`/`0x2AA00` 等大驱动器与 `0x36C56`(u1) 等库包装。
本轮 §61 从这批大驱动器中按“同模块”优先拉出 `0x2AA00`(u1，`item_menu` 的驱动器，§40 建模块时留下的唯一待还原项)，`item_menu` 家族自顶闭合；剩余闭包就绪游戏头名仍有 `0x1B1E7`(u1，内层读未初始化 ebp 暂跳)/`0x1BBDC`(u1，地图行动交互驱动)/`0x2000A`(u1，地图缩放视图驱动)/`0x2872B`(u1，商店菜单驱动，`sub_279BC` 的 callee)/`0x2EBE1`(u3，2291B 超预算) 与库包装 `0x3645D`(u4)/`0x2E95B`(u3)/`0x365DA`(u2)/`0x36C56`(u1)（按既定策略跳过）。
本轮 §62 借“伪 callee 校正”拉出 `0x29BCB`(u3) 及其唯一待还原 callee `0x29AB2`（新建 `save_menu`）——`0x29AB2` 的
`jmp 0x28B3C` 只是 `0x2872B` 体内共享尾声，并非调用；此后存放档/场景槽选择屏闭合，`0x28F65`(u2，仍待 `0x29300`)/
`0x29DAA`(u1，仍待 `0x28F65/29300/29620`) 等商店族头名相应前移。
本轮 §63 继而把 `0x2968D`(u2)/`0x2986F`(u1) 两个 FD2.SAV 读写驱动器作为同族追加到 `save_menu`，
故 §62 的槽选择屏及两侧读写一并闭合（`0x29300`(u1，仍待 `0x26996…` 等)/`0x2670E`(u1)/`0x2B777`(u1)
仍为待还原调用者）；下一闭包就绪游戏头名回落至 `0x1B1E7`(u1)/`0x1BBDC`(u1)/`0x2000A`(u1)/`0x2872B`(u1)
与库包装 `0x3645D`(u4)/`0x2E95B`(u3)/`0x365DA`(u2)/`0x36C56`(u1)（按既定策略跳过）。
本轮 §64 拉出 `0x2B777`(u1，队伍换人屏 `0x2AF28` 的最后一个干净 callee，追加 `party_roster`)，
故换人屏只剩 `0x2AF28`/`0x2B4FB`/`0x2B67F` 三角（同族，真闭包 2091B 超预算，暂跳）；
下一闭包就绪游戏头名仍为 `0x1B1E7`(u1)/`0x1BBDC`(u1)/`0x2000A`(u1)/`0x2872B`(u1) 等大驱动器。
本轮 §67 拉出 `0x190AC`(u1，记录交互驱动器 `0x18D8C` 的 `else` 动作处理器，新建 `map_event`)；
其后闭包就绪游戏头名余 `0x1B1E7`(u1，party_panel 编排者，仍因内层读未初始化 ebp 暂跳)/`0x1BBDC`(u1，地图行动交互驱动)
及库包装 `0x3645D`/`0x2E95B`/`0x365DA`/`0x36C56`（按既定策略跳过）、`0x2EBE1`(u3，2291B 超预算)。
本轮 §68 拉出 `0x190AC` 留下的闭包就绪游戏头名 `0x20421`(u5，403B，§60 建 `ani` 时留下的 ANI.DAT
容器播放器，追加 `ani`)；其后剩余闭包就绪游戏头名仍为 `0x1B1E7`(u1，内层读未初始化 ebp 暂跳)/`0x1BBDC`(u1)
及库包装 `0x3645D`/`0x2E95B`/`0x365DA`/`0x36C56`（按既定策略跳过）、`0x2EBE1`(u3，2291B 超预算)。
本轮 §69 拉出闭包就绪游戏头名 `0x1F81E`(u3，100B，片头 ANI 序列播放器，追加 `title_screen`，`0x1F894`
待还原编排者仍会用到)；其后剩余闭包就绪游戏头名仍为 `0x1B1E7`(u1，内层读未初始化 ebp 暂跳，暂跳)/`0x1BBDC`(u1)
及库包装 `0x3645D`/`0x2E95B`/`0x365DA`/`0x36C56`（按既定策略跳过）、`0x2EBE1`(u3，2291B 超预算)。
本轮 §70 拉出闭包就绪游戏头名 `0x1BBDC`(u1，1058B，`0x18D8C` 调用的逐记录动作驱动器，新建 `map_action_ui`)；
其后剩余闭包就绪游戏头名仍为 `0x1B1E7`(u1，内层读未初始化 ebp 暂跳)，及库包装 `0x3645D`/`0x2E95B`/`0x365DA`/
`0x36C56`（按既定策略跳过）、`0x2EBE1`(u3，2291B 超预算)。
本轮 §71 拉出 `0x24336`(u1，384B，`scene_state_20`(0x240FA) 唯一剩余待还原 callee，FDOTHER 条目 34 的
101 帧全屏动画，新建 `scene_cutscene`)；其后剩余闭包就绪游戏头名仅 `0x1B1E7`(u1，内层读未初始化 ebp 暂跳)、
`0x21DB2`(u0，255B，cos/sin 几何助手，可下批取)，及库包装 `0x3645D`/`0x2E95B`/`0x365DA`/`0x36C56`（按既定策略跳过）、
`0x1F894`(u1，1765B)/`0x2EBE1`(u3，2291B) 超预算。
本轮 §73 已拉出该头名 `0x21DB2`(u0，255B，随机粒子放置助手，新建 `particle`)；其后剩余闭包就绪游戏内容均为
**工具库叶函数** `0x4E4BE`(56B)/`0x4E703`(28B)/`0x4E71F`(50B)/`0x4E751`(68B)/`0x4EC66`(22B)/`0x4ECA4`(27B)/
`0x4ECF0`(27B)/`0x4ED4F`(43B)（均 u0，toollib 区，真闭包＝自身，可作下批同区一簇），及库包装
`0x3645D`/`0x2E95B`/`0x365DA`/`0x36C56`/`0x365AB`（按既定策略跳过）、`0x1F894`(u1，1765B)/`0x2EBE1`(u3，2291B) 超预算，
及无名字/伪函数条目 `0x10000`/`0x36B00`（后者即 §1.2 的 `malloc(0x36B00)` 立即数伪函数）。

**台账发现（已解决）**：§26 曾记 `function-map.csv` 把 `0x2BF83`(u2) 列为待还原，而它其实已是
`src/game/fx.c` 的 `fx_advance`（`fx.h` 已声明），只是漏登 `restored_manifest.txt`；已由台账提交
`4d27e6e` 补入 `0x2BF83 sub_2BF83 game/fx.h` 并重跑生成器，该项现转为“已还原未验证”。
（`0x344B4` 已由 §16 连同其上游 `0x34366`/`0x33FAF` 整簇拉出；`0x2DF01/1FF79/1F73F` 已由 §14 拉出。）
策略不变：**按 callee 簇整体拉出**，并先剔除共享尾声造成的伪 callee（可用 Ghidra 结构反编译辅助）。
（`0x2921A`＝地点菜单退场缩放（上批 §11 遗留）；`0x1B1E7`(地图队伍面板编排者，6 个 callee 已在 §12 还原）
现已闭包就绪，惟其内层读未初始化的 ebp，需先定性 tick 基线。）

### 103) 里程碑：游戏内容源码还原**全部完成**（待还原仅余库区/伪函数/死代码）

- `function-map.csv` 现状（1370 行）：**待还原 14**、已还原未验证 **309**、已验证 **309**、
  需要进一步分析 **1**、接口桩 **683**、已确认无需重写 **54**。
- **待还原的 14 项全部属于 Watcom/DOS4GW 库区，按既定策略不还原**（`module-map.md` §2「库区（不投入还原）」、
  `uncertainties.md` D「CRT 全部映射 libc（不还原 Watcom CRT 实现）」）：
  - DOS4GW/DPMI `int386` 包装与内存提交：`0x36522` / `0x365AB` / `0x365DA` / `0x36647` / `0x3666C` / `0x36683`；
  - CRT 文件/堆包装：`0x3645D`(read) / `0x364D4`(malloc) / `0x364FB`(free) / `0x3669A` / `0x36C56`(filelength) / `0x36CAB`；
  - 伪条目：`0x10000`（`int3; jmp $` 起点桩）与 `0x36B00`（`malloc(0x36B00)` 立即数，非真实函数）。
- `需要进一步分析` 的 1 项 `0x367D1`（1151B，静态不可达 + 反编译失败）为**死代码**，维持不投入（`uncertainties.md` §2.2）。
- **结论**：游戏内容函数的源码化到此全部闭合（P0 主链 + 主状态机两张表 + 解释器簇 + P1 长尾
  + 各演出/菜单/商店/存档/换人模块）。余下仅上述库区/伪函数/死代码，均**有意排除**；后续如需运行只需
  宿主机 libc 映射与 AIL/平台桩接线（`module-map.md` §3/§4），不需再还原这些地址。
- 本轮**无源码改动**（仅本条记录）：`function-map.csv` 计数不变，`restored_manifest.txt` 不变，未重跑生成器。

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

---

## 5. 阶段 C：去 guest 化（进行中）

目标：源码不再依赖 32 位 flat guest（`port/docs/TRANSLATION.md` §6 阶段 C）。
顺序（已确认）：**C1 指针数据模型 → C4 删 `ORIG_*` → C2 分派表 → C3 VGA/低内存/平台缝**。
每步验收：standalone/native 严格 64 位 `gcc -fsyntax-only -std=c99 -Wall -Wextra`
（**不加** int-conversion 宽容）0 error；`FD2_GUEST_FIXED` 32 位 hybrid 不回归；
`tools/check_compile.sh --full` 两 profile `-Werror` 全绿。

### 5.1 C1 完成：指针单元改用真实 C 存储（本轮）

- 新增 `src/include/fd2_native.h` + `src/fd2_native.c`：把 45 个指针单元
  （AIL 句柄 `53ED0/53ED8/53EDC/53EE4/53EE8`、0xFA00 三屏暂存 `53C5B/F/63`、
  资源缓冲 `53EEC/53A4D/…/53F66`、记录表 `53A49/53BF7/53C1B`、图集 `53A61`、
  人物屏 `54137/5413B/5414B/5414F/54153` 等）在 standalone profile 下由段镜像
  4 字节宏改为真实 `uint8_t*`/`void*` 变量；`dword_5413F[3]` 与同址别名
  `dword_54143/54147` 用数组元素统一，避免 64 位下重叠。
- `fd2_globals.h` 末尾 `#include "fd2_native.h"`（`tools/gen_headers.py` 同步）。
- 32 个源文件、约 100 个赋值点去掉 `(uint32_t)(uintptr_t)` 位级转换；相应局部
  `uint32_t` 改为指针类型；`figure_attack_play` 的 `snd` 参数改为指针；
  `dword_53C1B`/`dword_53F62` 为只读视图，取 `const uint8_t *`。
- 验收：`tools/check_compile.sh`（默认）standalone strict + hybrid 均 0 error；
  `--full` 两 profile `-Werror` rc=0，伞头自检通过。
- 过渡期宽容现仅 hybrid 需要（bit-exact 的 4 字节镜像 + `ORIG_*` 回调）。

### 5.2 C4 完成：删除全部机器码回调（本轮）

- 207 条 `ORIG_*` 宏（106 个地址/17 个文件）由 `((fn)(uintptr_t)0x…)`
  改为直接指向已还原 C 函数或库边界，随后 594 个调用点**内联**为真实函数名，
  `ORIG_*` 名字在 `src/` 中仅剩注释，机器码 `0x1xxxx..0x4Fxxx` 回调归零。
- 同样处理了未走 `ORIG_` 前缀的 7 个文件的机器回调（`bgm/fade/fx/msg/scene/
  sys_menu/title_screen` 的 `RES_LOAD/AIL_*/OUTP/DELAY/…`）。
- `src/lib/ail.h` 新增 10 个样本/序列句柄入口真签名（AIL `init/start/stop/
  volume/loop_count` × sample/sequence）；`gen_headers.py` 的 `AIL_BOUNDARY_SKIP`
  与 `CRT_BOUNDARY_SKIP` 让 IDA 的栈探针 arity 不再与边界头冲突。
- 新增 `src/lib/crt.h`：`delay/outp/int386/fd2_mem_lock`（DOS/Watcom 边界）。
- 顺带修好 C4 暴露的真签名问题：`ev_script_run` script 参数 `const void*`、
  `path_mark/path_find` 的 `cost_row` 改指针、`res_blit/res_blit6` buf 改 `const`、
  `dlg_blit_dato/dlg_scroll_text` 的 1 参机器 ABI 调用点补 globals；
  `unit_load`/`map`/`ev5` 的 0x4E8xx 表访问器改用 `tbl_ptr(base,stride,i,0)`。
- 验收：standalone strict 0 error、hybrid 0、`--full` 两 profile `-Werror` rc=0。
- 剩余绝对地址仅 VGA/低内存（C3）。

### 5.3 C2 完成：分派表改为真实函数指针数组（本轮）

- 新增 `src/fd2_dispatch.c`（生成器 `tools/gen_dispatch.py`）：7 张游戏分派表
  `funcs_1197B[30]`/`funcs_1199C[90]`/`funcs_1541F[28]`/`funcs_25E23[30]`/
  `funcs_25E3A[30]`/`funcs_30469[10]`/`funcs_37012[10]` 由段镜像 4 字节单元改为
  真实 `void (*)(void)[N]`，初值取自 `analysis/dispatch_tables.csv` 并经
  `fd2_aliases.h` 解析为已还原 C 符号（全部槽位均命中）。
- `fd2_native.h` 对 7 张表 `#undef` 宏并 `extern` 声明真实数组；
  `FD2_GUEST_FIXED` 仍用 loader 重定位的镜像单元。
- `fd2_relocate.c` 改为 no-op（不再往数据镜像写 4 字节函数指针）；
  `gen_headers.py` 同步，`analysis/reloc_suspects.csv` 审计仍生成。
- 修好 `scene_enter.h` 缺失的 `scene_enter_29` 原型（表尾槽 29）。
- 其余 10 张 `funcs_*` 表在 cseg01（AIL/CRT 库区），游戏 C 代码未引用，
  按既定策略保留（待引擎层替换 AIL/CRT）。
- 验收：standalone strict 0 error、hybrid 0、`--full` 两 profile `-Werror` rc=0。

### 5.4 待办

- **C3**：~100 处 `0xA0000/0xA0504` VGA 与 `fd2_lowmem`/`MEMORY` 抽 framebuffer/平台缝。

### 5.4 C3 完成：VGA/低内存抽成 framebuffer 缝（本轮）

- 新增 `src/game/vga.h` + `vga.c`：`uint8_t *fd2_vga` 是唯一的 mode-13h 帧缓冲入口，
  `VGA_P(addr)` 把 guest 线性地址（0xA0000 窗口）换算成宿主指针。native 指向
  宿主数组 `vga_store[0xFA00]`，`FD2_GUEST_FIXED` 指向真实 0xA0000 窗口。
- 全库不再出现裸 0xA0000/0xA0504/0xA94CC… 的指针转换：约 100 处
  `(uintptr_t)0x…` 与 268 处裸地址字面量统一走 `VGA_P(...)`；各 VGA 基址宏
  （`VGA_BASE`/`VGA_FRAME`/`SCR_BASE`/`VGA_SPARE`/`TITLE_BAND*_DST`/`ADDR_TOP`
  /`ADDR_BOTTOM`）改为 `fd2_vga`/`fd2_vga+N`。
- `vm_run` 的 `addr` 由 int 改为 `uint8_t *`（返回同类型），把 VGA 地址真正指针化；
  `ui_meter_tiles/bar/fixed`、`ui_status_panel_at`、`map_status_gauge`、
  `save_slot_rows`、`shop_item_rows/party_bar_rows/party_equip_rows/party_stat_rows`、
  `view_draw_unit_sprite`、`box_status_draw/box_items_draw/box_open_init` 的
  `dst/base` 参数改为指针，调用点同步去掉 `(int)(uintptr_t)` 截断转换。
- 低内存仍走 `fd2_lowmem`/`DOS_LOWMEM_BASE`（平台缝），无裸 0x4xx。
- 验收：standalone strict 0 error、hybrid 0、`--full` 两 profile `-Werror` rc=0。
- C1–C4 至此全部完成；剩余为 AIL/CRT 库边界（引擎层）与低内存平台实现。

### 5.5 阶段 C 收尾状态

| 切片 | 内容 | 状态 |
|---|---|---|
| C1 | 45 个指针单元改真实 C 存储 | ✅ |
| C4 | 删除全部 `ORIG_*` 机器码回调（594 调用点） | ✅ |
| C2 | 7 张游戏分派表改真实函数指针数组；`fd2_relocate` no-op | ✅ |
| C3 | VGA 帧缓冲缝；`vm_run` 及全部绘图层地址参数指针化 | ✅ |

### 5.6 还原正确性修复：`map_cell_event` 事件表基址漏了 `+0x53`

**问题（与宿主无关，属于还原错误）**：`src/game/map_event.c` 的 `map_cell_event()`
在按 flag 读取每 flag 3 字节的事件记录时，把 `kind/value` 取到
`dword_53A55 + 3*flag`，但原机器码（0x190AC）取的是 **`+0x53`**：
```
91cb: movzwl 0x54(%edx,%eax,1),%ebx   ; value = *(u16*)(base + 3*flag + 0x54)
91d0: movzbl 0x53(%edx,%eax,1),%eax   ; kind  = *(u8*)( base + 3*flag + 0x53)
```
`src/game/map_turn.c` 一直是对的。少了 `+0x53` 会把 `value` 读成往后 28 个 flag 的
垃圾值：例如宝箱对应 flag=0 时读到 `0x1E04`，`dword_53AD9 = 7684+181 = 7865`，
越界读 FDTXT 偏移表得到负 offset → 负 glyph index。32 位 DOS 上只是画花字，64 位上
`glyph_table + (index<<5)` 变成未映射地址而崩溃（开宝箱 SIGSEGV 的根因）。

**修复**：`ev` 基址加 `+0x53u`（写入端 `+0x54` 原本就是对的）。不改 `gfx_draw_glyph`：
它对坏 index 的字节级行为要与机器码保持一致，真正要修的是调用方。

**归属**：按 `BRANCHING.md`，这是"原 C 写错了"的还原正确性修复，进 `main`，再合并到
`modern-platform`。（`modern-platform` 侧的同一修复及其原生运行记录见该分支
PROGRESS 的 7.19 节。）

**附带工具**：`tools/fd2sav.py`（FD2.SAV 存档解析/修改，布局全部来自本仓库还原源码）、
`tools/sav_diff.py`（去混淆后按记录字段标注的逐字节 diff）。

### 5.7 知识：把 MV（记录 `+0x3B`）改太大会死机

MV 就是 `path_mark`/`path_find` 的移动力预算：原版 `path_find` 第二遍 `rec2` 把当前路径
放进定长全局栈 `unk_60079`（8 字节/条，`0x60079..0x6017A` 只有 **32 条**），路径一旦超过
32 步就写穿相邻全局；同时剩余移动力按 `int8` 比较，**MV≥128** 时比较翻转、可达格反复
重展开，且第一遍 `rec1` 递归深度 ≈ MV 会压爆调用栈。游戏自身职业表上限是 **9**，所以
原版永远安全；用 `tools/fd2sav.py` 手改时建议别超过 ~15。

（字段：MV=`+0x3B`、EV=`+0x4E`（派生值，`unit_recalc` 一重算就回到“基础 DX”）、
MAXHP=`+0x42`、HP=`+0x40`。port 的 `path.c` 有 `PATH_STACK_MAX=64` 保护，只截断不写穿。）

### 5.8 知识：Linux/POSIX 下开始菜单回车不响应（宿主层，源码无需改）

标题菜单的确认条件（`title_screen.c`：`(uint8_t)word_53A8D == 0x0D || == 0x20 || AH==0xE0/0x52`）
依赖 BIOS `INT 16h` 返回的 `AL`；X11 的 `XLookupString` 只对映射表里的 keysym 发 CHAR
（`XK_Return` 不在表中），所以宿主输入层 `host/main_sokol.c` 的 `push_fr_key()` 要像
`input_post_key()` 一样用 `fr_key_ascii(key)` 预填 ascii 字节，回车才会以 `1C,0D` 到达游戏。
**归属**：宿主/平台修复，按 `BRANCHING.md` 只进 `modern-platform`（运行记录见该分支
PROGRESS §7.20），本条仅作 `main` 侧知识备份。

### 5.9 修复：隐藏商店热键按 ASCII 比对（`scene_apply_state` per-place 热键表）

**机制**：地点/场景交互屏（`scene_apply_state` 0x26152 分支 B）为每个场景读一张
31 字节/条的 per-place 表：`dword_53F56 = tbl_ptr(&fd2_data2[0x238D], 31, state, -31)`。
`ent[1]` = 光标要停的位置，`ent[2]` = 触发的键；两者同时命中就把选项置为隐藏项
`dword_53F4A = 5`，随后 `place_menu_commit`（0x2670E）走隐藏分支。

**问题**：机器码 0x164a8 读 `byte_53A8E`（BIOS `INT 16h` 返回的 AH = **扫描码**）去和
`ent[2]` 比，但表里存的是 **ASCII**（`T _ j W b m Z e p ] ^ i V a l X d o \ g h`）——
所以原版和 port 都对不上，隐藏商店永远进不去。属“原程序数据/代码不一致”，按
`BRANCHING.md` 进 `main`。

**修复**：`src/game/scene_core.c` 同时取 AL(ascii)，热键命中条件改为
`scan == ent[2] || ascii == ent[2]`（switch 仍按 scan，不影响方向键/BGM 分支）。

**热键表**（`state`: place id / 光标 / 键）：

| state | place | cursor | key | state | place | cursor | key |
|---|---|---|---|---|---|---|---|
| 1 | 0 | 0 | `T` | 12 | 1 | 1 | `i` |
| 2 | 2 | 1 | `_` | 13 | 1 | 2 | `V` |
| 3 | 0 | 2 | `j` | 14 | 1 | 3 | `a` |
| 4 | 0 | 3 | `W` | 15 | 1 | 4 | `l` |
| 5 | 2 | 4 | `b` | 16 | 1 | 0 | `X` |
| 6 | 2 | 0 | `m` | 17 | 1 | 1 | `d` |
| 7 | 0 | 1 | `Z` | 18 | 2 | 2 | `o` |
| 8 | 0 | 2 | `e` | 19 | 1 | 3 | `\` |
| 9 | 2 | 3 | `p` | 20 | 1 | 4 | `g` |
| 10 | 2 | 4 | `]` | 21 | 1 | 0 | `h` |
| 11 | 1 | 0 | `^` | | | | |

### 5.10 知识：逆向翻出来的几个游戏秘密（不是 bug，是原版特性）

- **隐藏商店**：见 §5.9。每个地点一张热键表，光标停在指定项、按对应字母才开。
- **密码屏永远成功**：`password.c`（0x33FAF）里 0x34233 的不等分支是**无条件跳转**，
  真正的“密码错误 → exit(1)”整段是死代码；所以那三格密码屏随便输都能过。
- **BGM 试听**：地点/场景交互屏按 scan `0x22`（`G` 键）在 BGM 0..9 间循环（0x264CD）。
- **地图查看键**（`scene_event_tick`，key 是扫描码）：`F1`(0x3B)/`PgUp`(0x49) 缩放地图，
  `F2`(0x3C)/`Home`(0x47) 查看光标所在记录，方向键 0x48/0x50/0x4B/0x4D 平移地图窗口。
- **隐藏道具格**：地图格 `info[4] & 0x60` 非零的“亮点”格进入 `map_cell_event`（0x190AC），
  按 flag 查 `dword_53A55 + 0x53` 的 3 字节表：kind 0 = 给道具（可进背包或换装），
  kind 1 = 给钱，其它 = 派发 `funcs_1199C[value]` 场景脚本。所以“没有宝箱但有个亮点”
  的道具就是这种特殊格，数据在 FDFIELD/场景脚本里。
- **最后一关的睡龙 / 炎龙剑**：龙应是地图上带特殊事件的记录（`funcs_1199C` 场景处理器 +
  `map_cell_event`），传送唤醒和掉落都走事件脚本；具体在 FDFIELD 的章节数据里，源码层只
  能看到通用机制。

---

## 6. `modern-platform` 分支：复用 port 的后端

分支：`main`（原 `master`）→ `modern-platform`。目标：把已调通、画面/声音验证过的
`E:\FD2\port` 宿主的**后端层**迁入 port3，为原生引擎提供现代跨平台音频/音乐/视频实现。

**本轮已搬入 `host/`（verbatim，WSL gcc `make -C host check` 全 0 error）：**

- 视频抽象 `render.h` + 现代实现 `render_sokol.c`（D3D11/Metal/GL）+ Win32 参考 `render_gdi.c`；
- 音频抽象 `audio.h` + 现代实现 `audio_sokol.c`（WASAPI/CoreAudio/ALSA）+ 单设备混音器；
- 音乐 `synth.c` + `xmidi.c` + `dls.c`；
- 输入词汇 `keys.c`、OS 缝 `platform_posix.c`/`platform_win32.c`、`sokol_impl.c` + `vendor/sokol/`。

**不复用**：port 的 `le.c`/`dos.c`/`repl.c`/`host.c`/`main_*.c`（与“跑原始机器码”耦合，
port3 已去 guest）。

**待写适配层**（`host/README.md` §3）：`fd2_kernel.c`（原生宿主内核）、`fd2_video.c`
（`fd2_vga`+调色板→BGRA→`render_present`）、`fd2_audio.c`（用 synth/xmidi 实现
`src/lib/ail.h` 的 16 入口）、`fd2_platform.c`（`int386/outp/delay/fd2_mem_lock` + BDA 键/tick
写入 `fd2_lowmem`）。

---

## 7. `modern-platform`：首个可链接的原生引擎

- **P0/P1 别名补齐**（首次全量链接发现的潜在问题）：port3 自己还原的函数用了语义名，
  但 `fd2_aliases.h` 只覆盖 port/ 的 309 资产，导致调用点/分派表里的 `sub_XXXX` 链接不上。
  新增 `src/include/fd2_restored_aliases.h`（57 条，地址取自 `function-map.csv`，语义名
  取自模块头/.c 的 `/* 0xADDR */` 注释），伞头与 `gen_dispatch.py` 同步。
- **适配层完成**：`fd2_audio.c`（AIL，方案 A）、`fd2_video.c`（palette+VGA→BGRA）、
  `fd2_platform.c`（delay/outp/INT16h/int386/fd2_mem_lock/j___delay）、
  `fd2_kernel.c`（fd2_data_init + 游戏线程 + BDA tick/键环 + dos_lowmem_base）、
  `main_sokol.c`（sokol_app 入口，复用 port 的键映射）。
- **入口重命名**：`boot.c` 的 `main` → `fd2_game_main`（避免与 sokol 入口冲突）。
- **de-guest 修正**：`dos_lowmem_base` 由 `uint32_t` 改 `uintptr_t`（64 位不再截断）。
- **首次全量链接成功**：`make -C host engine` → `host/build/fd2engine`（游戏 C +
  host 后端 + sokol，`-lX11 -lXi -lXcursor -lGL -lasound -ldl -lm -lpthread`）。
- 未决：链接有少量 `-Wpointer-to-int-cast`/`-Wmaybe-uninitialized` 警告（原生 64 位下
  确需清理）；本环境无显示，运行验证留待桌面端。

### 7.1 Windows x64 exe（本轮）

- `build.ps1`（VS18 Enterprise `vcvars64` + `cl`）产出 **`build\fd2engine.exe`**（x64，
  sokol/D3D11 + WASAPI + sokol_app Win32 输入）。
- 入口 `main_sokol.c` 改为**双平台**（直接复用 port 的 `sapp_to_vk/push_vk` Win32 键路径）；
  `fd2_kernel.c` 增加 `fd2_engine_key_set_ascii`（sokol 的 CHAR 事件回填）。
- MSVC/x64 适配：
  - `fd2_funcs.h` 不再声明 Watcom CRT 内部名（`_fsopen/unlink/_set_errno/...` 与 MSVC CRT
    冲突）；`gen_headers.py` 加“只声明 IDA 前缀符号”规则。
  - `outp` 是 MSVC 内建，不能定义 → 全量改名 `fd2_outp`（`crt.h` + 调用点 + 实现）。
  - `platform_win32.c` 的 `plat_data_selector` x64 无内联汇编 → 返回 0。
- 回归：`make -C host check` 仍全 0 error；Linux `fd2engine` 仍可链接。
- 运行需在含 FD2 资源文件的目录（如 `E:\FD2`）；音色库 `gm.dls` 路径待命令行参数化。

### 7.2 运行时三个问题的修复（Windows）

1. **没有音效** → 根因：`seg_dseg02.bin` 尾部 `[0x4000,0x56B0)` 是 **BSS**（IDA 导出成 0xFF），
   而 port 的 LE 加载器会清零、port3 的 `fd2_data_init` 直接 memcpy。于是
   `dword_54133`（demo 门控）=-1，`svc_play_sfx` 的 `dword_54133 != 0` 直接屏蔽全部音效。
   修复：`fd2_data_init` 清零 dseg02 的 BSS；`gen_headers.py` 同步。修复后日志出现
   `set_sample_address` 且 `sfx voices=1`。
2. **卡顿/固定低频** → 实测 tick 只有 16Hz（`Sleep(55)` 被 Windows 默认 15.6ms 粒度拉长到
   ~62.5ms）。加 `timeBeginPeriod(1)` 后 tick=18.0Hz；渲染 61~65fps。
3. **选 Load 后崩溃** → 同 BSS 根因（大量未初始化全局为 0xFF）；修复后 Load 全流程（含选槽）不崩。

另加：`FD2_LOG` 日志重定向（GUI exe 可诊断）、`FD2_KEYS`/`FD2_KEY_DELAY` 脚本按键用于复现、
Windows 未处理异常处理器（打印 code/地址）、`-Console` 调试构建、每 2s 的 fps/tick 统计日志。

### 7.3 64 位 guest 指针残留系统性扫描（本轮）

上一轮修 Load 崩溃时发现：这一类 bug 的共性是 guest 用 32 位整数存指针/绝对地址，
C1/C3 只清了全局变量和十六进制 VGA 地址，局部变量、返回值、短名别名、表项、十进制地址
仍有漏网。本轮做了一次系统性扫描（新增可重跑工具 `tools/scan64.sh`）：

1. **编译器强约束扫描**：standalone 64 位下把 `-Wint-conversion` / `-Wint-to-pointer-cast`
   / `-Wpointer-to-int-cast` 提为 error，`src/game/*.c` + `src/*.c` + `host/*.c` 全绿。
2. **guest 绝对地址扫描**：源码中所有裸 guest 线性地址（0x5xxxx/0x6xxxx/0xA0000）与
   十进制 VGA 地址，全部改走 `fd2_data1/fd2_data2` 镜像或 `VGA_P()`。
3. **`(int)(intptr_t)` / `(uint32_t)(uintptr_t)` 显式截断扫描**：清零。

**找到并修复（按影响排序）**

| 位置 | 问题 | 修复 |
|---|---|---|
| `fd2_globals.h` `dword_53AA9` / `dword_53AB1` | 生成器因 `qword_53AA9/53AB1`（{x,y} 对）把同址的 `dword_` 名也生成为 `uint64_t`。`dword_53AA9 += dx` / `--dword_53AA9` 变成 64 位读写，**顺带改掉高 32 位的 y**（`dword_53AAD`/`dword_53AB5`），视图原点 x/y 被污染 → `map_render_view` 单元指针越界，**New Game/Load 崩溃的真根因**。 | `data_type_overrides.manual.csv` 增加 `dword_53AA9/dword_53AB1 = int32_t`（按符号覆盖，`qword_` 仍 uint64），重生成头文件 |
| `dlg.c` `dword_53A18` | `dlg_open_box` 的 5 个 stage 指针数组被建模成镜面上的 `void **`，64 位下按 8 字节步进写入 0x3A18..0x3A3F，**覆盖相邻标量** `dword_53A2C/30/34/38/3C`（svc 调色板 tick、map_status 锚点）。 | `fd2_native.h/.c`：真正的 `void *dword_53A18[5]` |
| `fx.c` 14 个 `FX_TABLE_*` | 粒子方向/位置表用 `(const void*)(uintptr_t)0x524EE...` 从裸 guest 地址读（与上轮 `screen_curtain` 同类）。 | 新增 `FX_TBL()` 从 dseg02 镜像读；14 处全改 |
| `map_attack.c` `off_61955` | 效果脚本表存的是 **4 字节 guest 绝对地址**（0x619A9…），`tbl_u32` 后直接当指针用。 | `tables.c` 新增 `tbl_off61955()` + 通用 `tbl_guest()`，按镜像换算（`tbl_off627D8` 同用） |
| `rec.c` `TBL_619FD` | `tbl_ptr((void *)(uintptr_t)0x619FD, …)` 把裸 guest 地址当表基址。 | 改为 `&fd2_data2[0x19FD]` 镜像 |
| `map_interact.c` `map_pick_record(int list)` | 第三参数声明成 `int`，调用点 `(int)(intptr_t)list` 截断后函数内再当指针 → 各调用点全崩。 | 参数改为 `const uint8_t *list`，7 个调用点同步 |
| `vm.c` box 句柄 | `int box` 存 `dlg_open_box()` 的指针，`close_open_box` 再 `(void*)(intptr_t)` 转回。 | 全程 `void *box` |
| `res.c` | `res_load` 用 `guest_store_u32(0x53BFF,…)`（native 下 no-op）发布大小，而 `bgm.c` 读的是 `dword_53BFF` → native 恒 0。 | 直接写 `dword_53BFF`（两个 profile 都指向同一内存） |
| `ev.c` / `rec.c` / `gfx.c` | 返回值/全局把指针存进 `uint32_t`（`return (uint32_t)(uintptr_t)…`；`gfx_glyph_tab/gfx_pen_dest`）。 | 返回值改 `void` / 指针类型；gfx 两个 scratch 改 `void*` |
| `map.c` | `(uint32_t)(uintptr_t)dword_53A51 > 1u` 用低 32 位判非空哨兵。 | 改 `(uintptr_t)dword_53A51 > 1u` |
| `map_zoom.c/.h`、`scene_core.c` | `uint8_t**`→`const uint8_t* const*` 不兼容、`char*`→`const uint8_t*` 符号性告警。 | 参数改 `uint8_t *const *`；`buf` 改 `uint8_t*` |
| `fd2_platform.c` | 崩溃处理器只记 code/地址，定位不够。 | 追加 fault 地址、寄存器、`dword_53A51/53A5D/53A69` 与地图尺寸/原点；过滤 `DBG_PRINTEXCEPTION_C` |

**回归**

- `tools/check_compile.sh --full`：standalone + hybrid 双 profile、生成源、头自测，**全 0 error**；
  严格 int-conversion 扫描 0。
- `pwsh -File build.ps1 -Console`：MSVC x64 链接通过（仅剩编码/符号比较告警）。
- 脚本按键复现（`E:\FD2`，`FD2_KEYS` + ASLR `/DYNAMICBASE:NO` + `/MAP` 还原函数）：
  - `New Game: Enter` ×3、`Load: Down,Enter,Enter,Enter` ×3、长过场 25×Enter ×3 ——
    **全部跑满超时无崩溃、无 `fd2_crash.log`**。修复前 New Game/Load 均稳定崩在
    `map_render_view`（返回栈：`map_view_update → map_slide_view → dlg_open_box → vm_run`）。
- 新增/保留诊断：崩溃回溯含 fault 地址与寄存器、地图关键全局；`FD2_LOG`/`FD2_KEYS`。

**工具**：`tools/scan64.sh [standalone|hybrid|warn|nums]` —— 可重跑的本轮扫描入口。

### 7.4 “退出到 DOS 后画面冻住”的根因与修复

**复现**：`build/play.cmd` 启动（开 `FD2_KEYLOG`+`FD2_STALL_MS`），真人操作，记录键序
（`play.keys`：26.3s 起 5×Down → Enter → Enter/Enter/Down/Enter/Enter）。

**根因（不是回归，也不是死循环）**：地图里按 Enter 呼出菜单 → 子菜单选 **“退出到 DOS”**。
日志给出确凿证据：`fd2: exit: sub_117E7 r=-1` —— 内层事件 tick 返回 -1，`boot.c` 置
`quit=1`，`fd2_game_main` 正常走到 teardown（`AIL_shutdown`、切回文本模式）并 return。
但宿主 `main_sokol.c` 的 `frame_cb` 只在 sokol 事件里 quit；游戏线程已经结束，主线程仍在
每帧 `fd2_engine_frame()` 反复呈现**最后一帧** → 画面对用户就是“冻住”，其实游戏已退出。

**修复**：

- `fd2_kernel.c/.h` 新增 `fd2_engine_game_over()`（返回游戏线程结束标志）；`main_sokol.c`
  的 `frame_cb` 在它置位时 `sapp_quit()`，窗口正常关闭。
- 顺带加的诊断（本次联调用）：
  - `FD2_KEYLOG=<path>`：每次真实按键 `<ms>,<scan>,<ascii>`（立即 flush）；日志同步
    `fd2: key @...`。`boot.c` 主循环自增 `fd2_game_progress` 作为存活信号。
  - `FD2_KEYPLAY=<path>`：按记录里的绝对时间轴回放，用于确定性复现。
  - 冻结看门狗：游戏线程 8s（`FD2_STALL_MS`）无迭代推进**且键环里存在未被消费的按键**
    （真死机时用户会按键，而空闲等待/已退出不会）才挂起游戏线程、扫栈打印返回地址 +
    键环 head/tail + 地图关键全局，写 `play.log` 与 `fd2_stall.log`。
  - `boot.c` 在各 `quit=1` 分支打印退出原因（`sub_117E7 r=-1` / `sub_25EBB v=` /
    `scene_switch r=`）。

**回归**：`FD2_KEYPLAY` 回放该键序，进程在 38s 处 **exit=0**（窗口关闭），日志末
`fd2: exit: sub_117E7 r=-1` + `ail: shutdown`。Keys 文件与回放时间轴见
`build/play.keys`。

### 7.5 长按方向键不连续移动：宿主补 BIOS typematic 重复

**现象**：地图上按方向键走一格；按住不放，原版会先停一下再连续走，port3 只走一格。

**原因**：输入层把键盘自动重复丢了——`main_sokol.c` 的 `event_cb` `if (e->key_repeat) break;`
丢弃 sokol 的重复 KEY_DOWN，`push_vk()/push_fr_key()` 的 `g_down[]` 按住去重再丢一次。于是
游戏每次物理按压在 BDA 键环里只收到一个 make code。原版经 INT 16h 读到的是键盘控制器的
**typematic 重复 make 流**（约 500ms 延迟后 ~10.9 次/秒），所以能连续移动。游戏逻辑本身正确
（`scene_event_tick` / `map_cursor_dir_run` 都是"读一个键→走一步"）。

**修复（方案 B，模拟 DOS BIOS typematic）**：`host/fd2_kernel.c` 新增：
- `repeat_note()`：make 记下"最后按下的键"并起 500ms 定时；匹配的 break 解除。重复 make 直接进
  BDA 键环（不过 `keylog_note`，录制里只保留真实输入）。
- `repeat_thread()`：10ms 轮询，到点后每 90ms 推一个 make，直到松开。
- 可调：`FD2_TYPEMATIC_DELAY`（默认 500ms）、`FD2_TYPEMATIC_RATE`（默认 90ms）、
  `FD2_TYPEMATIC=0` 关闭、`FD2_TYPEMATIC_DEBUG=1` 打印每次重复。
- 自动按键/回放（`FD2_KEYS`/`FD2_KEYPLAY` 都是 make 后立即 break）不会触发重复。

**验证**：`FD2_KEYPLAY` 一个 500ms 按下、4500ms 松开的 Down，`FD2_TYPEMATIC_DEBUG=1` 下
日志出现 38 次 `fd2: repeat scan=50`（≈92ms 周期），4500ms break 后停止。

**另**：`fd2_code`（原版机器码 258KB）在 native 构建里已无被执行代码引用，是死数据，后续可删。

### 7.6 释放法术崩溃：目标选择后仍用旧的视图原点

**现象**：地图上选中单位 → 命令菜单选法术 → 回车 → 在地图上选一个敌人目标 → 回车释放
→ 崩溃，未进入法术动画。

**复现**：`FD2_KEYPLAY` 回放 `build/play.keys`（脚本化复现稳定）。崩溃签名：
`res_blit+0x11`，`fault=0x8`，`dword_53A5D=0`，栈
`action_menu_run → figure_effect_play → figure_attack_scroll → res_blit`。

**根因**：`action_menu_run` (0x1CFF0) 在进入分支前把 `qword_53AB1`（地图光标）拷进局部
`lo/hi`，之后 `map_pick_record` (0x115B6) 会让玩家移动光标到目标格。原始机器码在
`map_pick_record` 之后重新**实时**读取 `[0x53ab1]/[0x53ab5]`（0x1D211 / 0x1D31E 的
`push ds:0x53ab1`），因此范围/目标是以**目标格**为中心；还原代码却继续用旧的 `lo/hi`
（施法者格），于是 `map_reveal_reachable(e[4]=0)` 在错误格子上过滤后返回 `count=0`。
`figure_effect_play` 里 `groups[0]` 因 `count==0` 未被加载（端口显式置 NULL），
`res_blit(groups[0]=NULL,0,…)` 读 `NULL+8` 崩溃。原始代码此处读到的是未初始化的栈槽，
通常是个残留的合法指针，所以原版不崩、只画错。

**修复**：
- `action_menu.c`：`map_pick_record` 之后的 `map_reveal_reachable` 改用**实时**
  `qword_53AB1`（branch 1 与 branch 3）；`map_scan_records` 的 `ref` 用实时光标、`start`
  仍用 pre-pick 的 `lo/hi`（与 0x1D343 的 `push [esp+0xdc]/esi` 一致）。
- `figure_view.c`：防御性——`figure_effect_play` 在 `count==0` 时把 `groups[0]` 回退为
  施法者自己的 `fig_a`，避免 `res_blit(NULL)`（`count` 仍为 0，不产生伤害）。

**回归**：`FD2_KEYPLAY` 回放该键序，进程跑满超时、无 `fd2_crash.log`，法术动画正常进入。
双 profile `-Werror` 全绿。

### 7.7 选人按回车“变两次”：break 码被塞进 BIOS 键环

**现象**：地图上选人物按一次回车，菜单不出现或 NPC/敌人信息面板一闪就关。

**取证**：开 `FD2_KEYLOG` 录下真实按键，每次回车只有一条 make(1C)+break(9C)，且无
`fd2: repeat` —— 宿主只发了一次。再在 `int386(0x16)` 打点，看到游戏把 **break 码 0x9C
当成第二个按键读走**（`int16 AH=9C wait -> scan=9C ascii=0D`）。

**根因**：`fd2_engine_key` 把 make 和 break **都** `fd2_kbd_push` 进 BDA 键环
（0040:001E）。真实 BIOS 的 INT 9 只把 **make 码**放进该环（release 仅用于内部换挡），
INT 16h 永远不会返回 break。于是每次按键游戏都会多读到一个幽灵按键；其 AL 还是 0x0D，
“按任意键继续”的信息面板一读就关，命令菜单也会被它顶掉/跳过。

**修复**：`fd2_engine_key` 仍先用 break 清 typematic 状态、仍记入 `FD2_KEYLOG`，但
`scan & 0x80` 的 break 不再进键环。`--autokey` 本就直接 `fd2_kbd_push` 只发 make，
行为自此一致。

**回归**：`FD2_KEYPLAY` 回放该键序，int16 打点里不再出现任何 break（9C/CB/CD/D0…），
每次回车只被读一次；跑满超时无 `fd2_crash.log`。双 profile `-Werror` 全绿。

### 7.8 工具链注意（待办）

重跑 `tools/gen_headers.py`（本轮为固化 `dword_53AA9/53AB1` 的类型而跑过一次）时有两个
需要人工兜底的点：

1. **`gen_image()` 的 dseg02 BSS 边界会回退**：生成出的 `fd2_data_init()` 把清零起点写成
   `0x5000`，会覆盖掉 §7.2 修好的 `0x4000`（dseg02 `[0x4000,0x56B0)` 是 BSS，IDA 导出为
   0xFF）。本轮已把 `src/fd2_image.c` 手工回退保留 `0x4000`。**后续修 `gen_headers.py` 的
   边界逻辑前，每次重生成都要检查这一行**，否则音效/Load 会再次出问题。
2. **`fd2_funcs.h` 会删 273 条陈旧 `sub_*` 原型**（其语义名现在由各自头文件接管，重生成后
   会因宏别名与 `void` 返回类型冲突而必须删除），同时按源码注释里出现的 `dword_XXXXX`
   多生成 3 个宏（`tbl_51F96`/`dword_5266C`/`dword_52684`，实际只在注释里）。编译验证通过，
   属工具输出更新，但重生成后要复核 diff。

其它本轮相关的稳定性注意：
- `fd2_engine_key` 只把 **make 码**放进 BDA 键环（§7.7）；break 仅用于 typematic 状态与
  `FD2_KEYLOG`。以后若新增输入路径，别再把 release 塞进环里。
- `figure_effect_play` 对空目标列表有兜底（§7.6）；`action_menu_run` 里 `map_pick_record`
  之后的坐标一律读**实时** `qword_53AB1`，不要再缓存 `lo/hi`。

### 7.9 音乐“慢了一点点”：循环尾音多塞 1 秒静音

**问题**：商店/部分场景的背景音乐听感比原版慢一点点。

**核对速度（是精确的）**：
- 播放速度由 XMIDI 的 tempo 元事件决定：`tick_rate = 60 × 1e6 / tempo_us`（60 ticks/beat）。
  这与原版 AIL 自身定时一致——port2 的 AIL 逆向记录里 120 BPM 的 tick 步长是 **8333 µs**，
  而 `500000/60 = 8333.33`。
- 抽出 FDMUS.DAT 全部曲目：每首只有 tick 0 一个 tempo，且都是整数 BPM（商店 track 11 = 60、
  城镇 13 = 120、商店 fanfare 17 = 98、标题 19 = 111、开场 18 = 112 …）。实测端口解析打印
  `112.000 BPM / 112.0001 ticks/s` 精确无误。商店主题本身慢是原曲如此。

**真正的缺陷**：`synth.c` 的 `stream_init` 把曲长算成
`g_total = last_tick/tick_rate*rate + rate`（`+rate` 是额外 1 秒）。这 1 秒本意是让最后一个音的
release 响完，但对**循环**曲目它就是循环点，于是**每一圈都多出约 0.85 秒静音**再做下一遍——
地图/城镇/商店这类长循环背景因此显得偏慢。

**修复**：只在一次性播放时保留尾音，循环时不加：
```c
g_total = last_tick / tick_rate * rate;
if (!g_loop) g_total += rate;
```
`g_loop` 在 `synth_play` 调 `stream_init` 之前已设好；离线统计/WAV（`render()` 里 `g_loop=0`）
仍保留尾音。

**诊断**：`xmidi.c` 的解析摘要新增一行常驻打印
`xmidi: tempo <us/beat> (<BPM> BPM), 60 ticks/beat -> <ticks/s> ticks/s, song <s> s`，
以后听感异常可直接对日志核对。

**回归**：双 profile `-Werror` 全绿；Console 版实跑正常，日志显示 `tempo 535714 (112.000 BPM)`。

### 7.10 仓库瘦身：移出 IDA/Ghidra 派生物与编译产物（历史改写）

为发布/GitHub 做准备，把“可由原版程序或 IDA·Ghidra 重新获得”的大文件从**工作区与全部
历史**中删除（`git filter-branch --index-filter`，22 个提交与 5 个 tag 哈希已改写）：

- 原版段字节 `analysis/seg_*.bin`（252K+24K+16K）
- 全量反编译 `analysis/decomp/**`（1366 文件，4.1M）、`analysis/disasm_fallback/**`、
  `analysis/cseg01_disasm.txt`（3.9M）
- IDA/脚本导出 `analysis/{arity,callgraph,data_items,data_type_overrides,decomp_failures,
  dispatch_tables,funcmap,names,prototypes,reloc_suspects,strings}.{csv,json}` 等
- 由段字节生成的 `src/fd2_image.c`（1.5M）中的**代码段部分**（`fd2_code`）+ 生成的
  `function-map.csv`（0.4M）
- 编译产物 `build/**`（106 文件，4.1M）

保留：`analysis/data_type_overrides.manual.csv`、`analysis/prototype_overrides.csv`、
`src/include/*.h`、`src/fd2_relocate.c` 等手工/小体积且编译必需的文件。

结果：`.git` **30.7M → 1.4M**，整库 **60M → 7.0M**。`.gitignore` 已加入上述路径防回归；
`EXTERNAL_ARTIFACTS.md` 记录了重新导出/重建路径。

> 修正（同日）：`src/fd2_image.c` 不能整个删——它里面有两块：死的 **代码段** `fd2_code`
> （258KB 字节 → 1.52MB C 文本，51 个引用它的宏无一被用）与**必需的数据段镜像**
> `fd2_data1/_data2_init`（38KB 字节，游戏运行时通过 `fd2_data_init` 拷进镜像后读取
> 文件名串/静态表等）。现改成**只生成数据段**：`gen_image()` 跳过 cseg01，`src/fd2_image.c`
> 从 1.5MB 瘦到 **~197KB**（已入库），并顺手修了 `gen_image()` 的 BSS 清零起点
> （原来向上取整到 0x5000，会把 dseg02 `[0x4000,0x5000)` 的 0xFF 当数据，正是 §7.2 那个
> 音效 bug 的生成器根源）。**当前检出可直接 `build.ps1` 编译。**
> 历史改写前的完整仓库已备份为 `E:\FD2\port3-backup-<时间戳>.bundle`。

### 7.11 仓库零原版字节：数据段改为构建时从 FD2.EXE 生成

在 §7.10 瘦身之后，仓库里还留着 38KB 原版数据段（`src/fd2_image.c` 的数据部分）。
本轮把它也移出仓库，改为**构建时从本地的 FD2.EXE 生成**：

- 新增 `tools/prepare_image.py`：
  1. 有 `analysis/seg_dseg0{2,3}.bin` 就直接复用；
  2. 否则解析 `FD2.EXE`（DOS/4GW LE）：映射对象页、**应用 LE fixups**（数据表里是
     重定位前的裸偏移，loader 会改写为绝对地址）、写出两个数据段；
  3. 既无段数据又无 `FD2.EXE` → 打印提示并 `exit(1)` 停止构建；
  4. 由数据段生成 `src/fd2_image.c`（只含数据段）。
- `.gitignore` 忽略 `/FD2.EXE`、`analysis/seg_*.bin`、`src/fd2_image.c`；
  `src/fd2_image.c` 从 git 移除（工作区保留为生成物）。
- `build.ps1`、`tools/check_compile.sh --full`、`host/Makefile engine` 都先调用它。

**核对**：从锚定的 `FD2.EXE`（md5 `a6e341a8...`）提取的 dseg02/dseg03 与 IDA 导出的
`analysis/seg_dseg0{2,3}.bin` 逐字节一致（dseg02 前 0x4000、dseg03 全 0x34D2，0 差异；
仅 dseg02 的 BSS 尾部 IDA=0xFF、本工具=0x00，功能等价）。fixups 应用 7948 条、跨页伪
记录跳过 11 条。构建全链路实测通过（复用段 / 提取段 / 无 EXE 报错 / 双 profile 编译）。

结果：仓库内**不再有任何原版程序字节**，同时仍可从玩家自备的 FD2.EXE 一键构建。

### 7.12 跨平台构建（Linux / macOS / BSD）+ 非 Windows 入口

之前的非 Windows 入口只有 `host/Makefile`，且写死 Linux 的 `-DSOKOL_GLCORE` +
X11/GL/ALSA，`platform_posix.c` 也含 Linux 专有调用（`process_vm_readv`、`SYS_gettid`、
`/proc/self/exe`）。本轮做成真正跨平台：

- **新增仓库根 `Makefile`**：`make` = `tools/prepare_image.py` + `make -C host engine`；
  `make check` / `make clean`。可用 `FD2_EXE=/path/FD2.EXE make`。
- **`host/Makefile` 按 `uname -s` 选后端**：`Darwin` → `-DSOKOL_METAL` +
  `-framework Cocoa/QuartzCore/Metal/MetalKit/AudioToolbox/CoreAudio/Foundation`；
  其它（Linux/BSD）→ `-DSOKOL_GLCORE` + `-lX11 -lXi -lXcursor -lGL -lasound -ldl`。
  `check` 目标同时语法自检 host 与 `src/game`。构建前调用 `prepare_image.py`。
- **`platform_posix.c` 可移植化**：`plat_readable` 的 `process_vm_readv`（仅 Linux）加
  `#if defined(__linux__)` 回退；`plat_thread_id` 非 Linux 用 `pthread_self()`；
  `plat_module_path` 在 macOS 用 `_NSGetExecutablePath`；Linux 专有头文件按平台包含。
  （这几个 API 在本机原生端口里其实都没被调用，只保证能编译。）
- `tools/check_compile.sh` 默认 `CC` 由 `gcc` 改为 `cc`（macOS 上 clang 亦可）。

**验证（WSL/Linux）**：`make -C host check` host+game 全 `errors=0`；`make engine`
成功链接 `host/build/fd2engine`（2.4MB）。macOS 路径按 sokol 官方框架与本机无 Linux
专有符号编译核对（本环境无 macOS 可实机验证）。

### 7.13 运行多出 console 窗口：拿到的是 `-Console` 调试构建

现象：运行时除游戏窗外还弹出一个 console 窗口。原因不是代码（无 `AllocConsole`），而是
构建的**链接子系统**：`build.ps1 -Console` 用 `/SUBSYSTEM:CONSOLE`，双击时必然弹一个
console；默认（不带 `-Console`）是 `/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup`，只显示
游戏窗口，日志走 `FD2_LOG`（默认 `fd2engine.log`）。之前调试时构建过 `-Console` 并把它
留在了 `build/`，所以看起来像是 bug。

处理：重新用默认参数构建（`subsystem=WINDOWS GUI`，PE 子系统字头 = 2）；`build.ps1`
在 `-Console` 说明里标注"仅调试、会弹 console 窗口"，并在结尾打印本次的 subsystem。

### 7.14 方案 A 落地：呈现层高清放大（`FD2_UPSCALE`）

**目标**：在不碰游戏逻辑/资源的前提下，让 320×200 的原生画面在窗口里"高清化"。此前
`render_sokol.c` 只有一条 NEAREST 路径（纹理 320×200，铺满窗口），放大会是硬块。

**实现**（`host/render_sokol.c`，改动仅限呈现层）：
- 呈现层的放大模式在 init 时由 **`FD2_UPSCALE`** 选定，默认 `nearest`：
  | 值 | 行为 |
  |---|---|
  | `nearest` | 原来的逐像素 NEAREST（基准/对拍用；`FD2_UPSCALE=0/off` 同义），默认 |
  | `linear` | 普通双线性（平滑、偏软） |
  | `sharp` | **sharp-bilinear**：双线性 + 把越过 texel 边界的过渡变陡（像素边保持锐利） |
  | `bicubic` | Catmull-Rom 4×4，渐变平滑 |
- 所有平滑都在片元着色器里用**单张 NEAREST 采样器手工做**（双线性 4 tap、双三次 16 tap），
  这样 D3D11 与 GL 结果一致，也不必重建采样器状态；`nearest` 模式与原来的字节级输出相同。
- 片元着色器在 init 时**生成**：把 `MODE`/`TEXW`/`TEXH`/`SHARP` 作为 `#define` 常量写进去，
  因此无需 uniform block / cbuffer（跨后端最容易出错的环节）。顶点着色器不变。
- **失败兜底**（顺带修一个静默黑屏源）：`make_program()` 现在校验
  `sg_query_shader_state` / `sg_query_pipeline_state` 是否为 `SG_RESOURCESTATE_VALID`；
  生成着色器失败则退回生成版 `nearest`，再失败退回内置的原始 NEAREST 着色器。以前
  着色器编译失败只会得到一个全黑窗口。
- `FD2_GL_READBACK`（GL 方向自检）只在 `nearest` 模式下运行——平滑滤镜本来就会改变像素，
  继续用它做逐像素比较没有意义。
- **保持原始宽高比**：`sg_begin_pass()` 会把视口重置为整个 framebuffer，现在改成用
  `sg_apply_viewport()` 把绘制限制在一个**居中的 8:5（320:200）矩形**里，周围留黑边；
  窗口任意拉伸都不再拉变形。`FD2_ASPECT=off`（或 `0`/`stretch`）可恢复铺满。

**与方案 B/C 的关系**：本方案只改 `render_sokol.c` 这一层，`render_present(bgra,w,h)` 的
接口不变。以后 B（HD 素材）/C（高分辨率内部缓冲）把更大的帧交给同一个接口即可；届时把
`FD2_UPSCALE=nearest`（或按缩放比自动降级）即可，二者不冲突、可叠加。

**验证（WSL/Linux）**：`cc -fsyntax-only -Wall -Wextra -Werror` 下 `render_sokol.c` 0 error
（standalone 与 `-DSOKOL_GLCORE` 两条路径都过）；着色器为生成源，D3D11 侧仍走
`d3dcompiler_47.dll` 运行时编译，编译失败日志 + 回退已覆盖。

### 7.15 阶段 1：把 BIOS 时序换成现代原语（单调时钟 + 条件变量）

**动机**：运行期时间/输入还停留在"模拟机器"阶段——一个 host 线程每 `Sleep(55)` 改
BDA `0x46C`，游戏侧用**纯自旋**轮询它（`svc_wait_ticks`、`ev5_13460`，共 106 处
`svc_wait_ticks` 调用 + 十余个"等 tick 变化"的空转环），键环还被多个线程无锁读写。这既
空占一个核，也是偶发黑屏/卡死的根因类别。

**原则**：保留**逻辑上的 18.2 Hz tick 语义**（动画/淡入淡出/延时都按 tick 计），只把
实现换成主流原语——单调时钟 + 标准条件变量（`pthread_cond_t` / Windows
`CONDITION_VARIABLE`），不引入任何自定义调度器/环形缓冲。

**改动**：
- `src/platform.h` + `platform_win32.c` / `platform_posix.c`：新增标准 `plat_cond`
  （`plat_cond_init/wait/timedwait/signal/broadcast`，POSIX 侧用单调时钟；macOS 回退
  `CLOCK_REALTIME`）与 `plat_sleep_us`。
- `host/fd2_kernel.c`：
  - BDA 键环由**一把互斥量 + 条件变量**保护；`fd2_kbd_push/pop/flush` 全部加锁；
    `fd2_kbd_wait` 改为条件变量阻塞（不再 `sleep(1)` 轮询）。
  - 新增阻塞原语 `fd2_wait_tick()`（等到 tick 前进）与 `fd2_wait_tick_or_key()`
    （tick 前进 **或** 有键即返回）。
  - tick 线程改为**按绝对时限**调度（`plat_now_us()` + 累计周期，无累积漂移），推进后
    `broadcast` 唤醒等待者。
  - `fd2_engine_key_set_ascii` 与 typematic 槽（`g_rep_*`）也加锁。
  - 游戏线程创建失败时置 `g_quit`（主循环随即关窗），不再留下黑窗。
  - 看门狗改用**两个心跳 + parked 深度**：boot 循环计数 `fd2_game_progress`、已发布帧数
    `fd2_video_publish_count`，以及 `fd2_wait_depth`（`fd2_wait_tick`/`_or_key`/`fd2_kbd_wait`/
    **`fd2_delay_ms`** 都计入）。**两个心跳都停 且 连续 `FD2_STALL_MS`（默认 10s）不在任何等待中**
    才判为真卡死；启动阶段（`fd2_game_progress==0`，正在 load 资源）给 30s 宽限。判定后
    dump 栈，**默认只记日志**（`FD2_HANG_EXIT=1` 才关窗）——避免把正常的长标题屏/过场误判成
    崩溃。
- `src/lib/crt.h` + 游戏侧：`svc_wait_ticks`、`ev5_13460`、`ev5_1E5C0` 以及十余个
  "`while(!kbd_pending())` + 轮询 tick" 的 idle 环（`map_wait_anim`/`map_wait_key`/
  `map_cursor_dir_run`/`party_change_wait_key`/`party_panel_run`/`map_window_run`/
  `map_zoom`/`status_screen`/`dlg_run`/`ev4_11AA8`/`scene_core` 场景选择等）改为调用
  `fd2_wait_tick[_or_key]`；`kbd_flush` 改走加锁的 `fd2_kbd_flush`。

**行为保持**：tick 频率仍是 18.2 Hz；`svc_wait_ticks(0)` 仍立即返回；16 位回绕算术
（`elapsed<0 -> +=0x10000`）与 `dword_53A2C` 账目不变；idle 环仍是"每 tick 重绘一次 +
随时响应按键"，只是从"每秒空转上万次"变成"每 tick 醒一次"。

**验证（WSL/Linux）**：`tools/check_compile.sh --full` 双 profile + `-Werror` 全绿；
`fd2_kernel.c`/`fd2_platform.c`/`platform_posix.c`/`main_sokol.c`/`render_sokol.c`
`-Wall -Wextra -Werror` 0 error；`make -C host engine` 全量链接成功。Windows/MSVC 与
实机手感待本机验证。

**未做（阶段 2）**：帧缓冲双缓冲（消除游戏线程写、渲染线程读的撕裂/竞态）。

### 7.16 阶段 2：帧缓冲双缓冲（消除呈现撕裂/竞态）

**问题**：`fd2_vga`（+`fd2_palette`）由游戏线程写、渲染线程在 `fd2_video_frame()` 里读，
两者无同步——滚动时撕裂、调色板淡入淡出时会读到"半更新"的颜色。

**做法**（主流单生产者/单消费者握手，不引入花哨机制）：
- `host/fd2_video.c` 增加一个 **front 副本**（`g_front_vga` + `g_front_palette`）与一把
  `g_fb_lock`。**只有游戏线程写 `fd2_vga`/`fd2_palette`**，渲染线程只读 front 副本。
- 新增 `fd2_video_publish()`：游戏线程把 `fd2_vga`/`fd2_palette` 拷进 front（持锁）。
  它在**每一帧结束、游戏即将停下来的点**被调用：`fd2_wait_tick`/`fd2_wait_tick_or_key`/
  `fd2_kbd_wait` 入口，以及 `delay()`/`j___delay()` 入口——这些正是"本帧画完、进入等待"
  的时刻，所以发布的永远是完整、稳定的一帧。
- `fd2_video_frame()` 持锁把 front 转成 BGRA 再 `render_present`；渲染线程不再触碰
  `fd2_vga`/`fd2_palette`，数据竞态消失。
- 新增 `fd2_video_init()`（`fd2_engine_init` 在游戏线程启动前调用）建锁并发布初始黑帧。

**为什么不是指针双缓冲**：游戏是 DOS 式**直接写显存**、且大量 **incremental** 更新
（局部重绘、逐行 `memmove`、调色板动画），并非"每帧重画全部"。交换指针会让下一帧的增量
更新落到没有上一帧内容的缓冲上 → 花屏。因此采用"帧末快照发布"，语义等价且零改动游戏逻辑。

**开销**：每次 publish 两次 `memcpy`（64KB+768B ≈ 数 µs）；18.2Hz 下可忽略；渲染线程持锁
仅覆盖一次 64000 像素查表（~数十 µs），游戏发布最多等这么久。

**验证（WSL/Linux）**：`fd2_video.c`/`fd2_kernel.c`/`fd2_platform.c` `-Wall -Wextra -Werror`
0 error；`make -C host engine` 全量链接成功。Windows/MSVC 待本机验证。

### 7.17 修复："拖拽窗口后进程崩溃"实为看门狗误判（阶段 1 回归）

**现象**：用户拖拽/调整窗口大小后进程消失，像是崩溃。

**取证**：无 `fd2_crash.log`；有 `fd2_stall.log`：
```
fd2: STALL no game-thread progress (not parked in a wait) progress=1 frames=1653
  #00 fd2_platform: j___delay+0x14
  #01 title_screen: title_screen_run+0x548
  #02 scene_core: scenario_menu
  #03 boot: fd2_game_main+0x240
```
游戏线程正常停在标题屏的 `j___delay`（`Sleep`）里；`fd2_game_progress` 在**整个标题屏
期间恒为 1**（它只在 `boot.c` 的外/内循环自增，而标题屏是在**一次** `scenario_menu()` 调用
内部跑完的）。

**根因**：阶段 1 的看门狗把"parked"只定义在 `fd2_wait_tick/_or_key/fd2_kbd_wait` 上，
**漏了 `delay()`/`j___delay()`**。于是标题屏的长 `j___delay` 被当成"无进展且未阻塞"，
10s 后触发 `g_quit=1` 关窗。拖拽窗口只是让人在标题屏停留更久，与 resize 本身无关。

**修复**：
- 新增 `fd2_delay_ms()`：`delay`/`j___delay` 都走它——发布本帧 + `fd2_wait_depth++` 后
  `Sleep` + `--`，长固定停顿计入"parked"。
- 看门狗改用**两个心跳**（`fd2_game_progress` + `fd2_video_publish_count`），任一前进即
  重置；且要求"两个心跳都停"**且**"连续 `FD2_STALL_MS` 不在任何等待"才判定。
- 判定行为改为**默认只记日志**（`FD2_HANG_EXIT=1` 才关窗），不再无故关掉正常运行的窗口。

**回归**：`fd2_kernel.c`/`fd2_platform.c`/`fd2_video.c` `-Werror` 0 error；`make -C host
engine` 链接通过。

### 7.18 Windows 构建告警清零（C4819 / C4018）

MSVC 构建输出大量告警，与逻辑无关，但污染构建日志：

- **C4819（"该文件包含不能在当前代码页(936)中表示的字符"）**：源码是 UTF-8（含大量中文
  注释），MSVC 默认按系统代码页 936 读取 → 几乎每个文件都报。修复：`build.ps1` 的
  `$cflags` 加 **`/utf-8`**（已核对：全库没有非 ASCII 的字符串字面量，只有注释，所以连
  执行字符集一起设为 UTF-8 也安全）。
- **C4018（signed/unsigned 比较不匹配）**：还原代码里大量 `for (int i = 0; i < (uint32_t)全局; i++)`
  这类小整数循环计数与 `uint32_t` 全局比较，是既有写法。POSIX 侧 `tools/check_compile.sh`
  本就带 `-Wno-sign-compare`；为两端一致，`build.ps1` 加 **`/wd4018`**（而非逐处强转，
  因为部分 `uint32_t` vs `int32_t` 的比较参与负数语义，盲目改类型有风险）。

### 7.19 修复：开宝箱后段错误（map_event.c 事件表取值漏了 +0x53）

**现象（Arch Linux 实机）**：游玩中打开宝箱，进程 SIGSEGV。core dump 回溯：
```
gfx_draw_glyph
  <- vm_run (vm.c:261)
  <- vm_run (vm.c:170)
  <- map_cell_event (map_event.c:71)
  <- map_record_interact / map_record_engage / scene_event_tick
```
`gfx_draw_glyph` 收到 `index=0xFFFFC603`（-14845），`glyph_table + (index<<5)` 得到
未映射地址 `0x7f04cbfa0d80`（64 位下 32 位回绕不再发生，故由"画花字"变成崩溃）。

**根因**：`map_cell_event` 读取每 flag 3 字节的事件记录时，`kind/value` 取自
`dword_53A55 + 3*flag`，但原机器码（0x190AC）取的是 **`+0x53`**：
```
91cb: movzwl 0x54(%edx,%eax,1),%ebx   ; value = *(u16*)(base + 3*flag + 0x54)
91d0: movzbl 0x53(%edx,%eax,1),%eax   ; kind  = *(u8*)( base + 3*flag + 0x53)
```
（`map_turn.c:94-96` 一直是对的。）于是 `value` 读成往后 28 个 flag 的垃圾值。

**core 取证**：宝箱对应 flag=0，`base[1..2]=04 1E` ⇒ 错值 `0x1E04=7684`，
`dword_53AD9 = 7684+181 = 7865`，越界读 FDTXT 偏移表 → 负 offset → 负 glyph index。
正确记录在 `base+0x53` 处 `00 C0 00`：kind=0、value=0xC0=192、`dword_53AD9=373`（合法）。

**修复**：`src/game/map_event.c` 的 `ev` 基址加 `+0x53u`（写入端 `+0x54` 本已正确）。
不改 `gfx_draw_glyph`：它对坏 index 的字节级行为要与机器码保持一致（有差分 harness），
真正要修的是调用方。`make engine` 全量链接通过，已把新二进制拷到游戏目录 `../fd2engine`。

### 7.20 修复：Linux 下开始菜单回车不响应（POSIX 输入层 Enter 无 ASCII 字节）

**现象**：标题后的主菜单（new game / load / continue）只有**空格**能确认，**回车**无效。

**根因（仅 POSIX 路径）**：`title_screen.c:262` 的确认条件依赖 BIOS `INT 16h` 返回的
`AL`：`(uint8_t)word_53A8D == 0x0D || ... == 0x20 || AH == 0xE0/0x52`。而
`host/main_sokol.c` 的 `push_fr_key()` 对所有非扩展键都把 ascii 先置 0，指望随后由
`sokol` 的 `SAPP_EVENTTYPE_CHAR` 事件补上。但 X11 的 `XLookupString` 只对
`_sapp_x11_keysym_to_unicode()` 表里的 keysym 发 CHAR（拉丁区 + 映射表），
`XK_Return`(0xFF0D) 不在其中 → Return 永远等不到 CHAR → 游戏侧 `AL=0`，菜单不认。
空格能过是因为 `XK_space`(0x20) 命中 Latin-1 直通，有 CHAR 事件。

**修复**：`push_fr_key()` 在 `scan_ascii()` 之后对非扩展键用 `fr_key_ascii(key)` 预填
ascii（与 `input_post_key()`/autokey 路径一致）；真正的 CHAR 事件仍会覆盖它，所以字母
大小写/布局字符行为不变。仅 `host/main_sokol.c` 的 `#else /* !_WIN32 */` 段，Windows
的 ToAscii 路径不受影响。`title_screen.c` 无需改动。

**验证（实机 X11）**：写 `/tmp/sendkey`（XSendEvent 合成按键）向窗口发键，`FD2_KEYLOG`
记录游戏实际读到的 (scan, ascii)：
- 修复前等价于 `1C,00`；
- 修复后 `39,20`（空格）与 **`1C,0D`（回车）** 均正确。
`make engine` 链接通过，已拷到游戏目录 `../fd2engine`。

### 7.21 修复：Alt+F4 / 关闭窗口会直接退出并丢掉未存档进度

**现象**：Linux / Windows 下按 Alt+F4（或点标题栏关闭按钮）窗口立刻关闭，未存档的进度
丢失；原版在商店/教会/道具店/酒店前用组合键进隐藏商店时，按键也容易被 OS 抢走。

**根因**：`host/main_sokol.c` 的 `event_cb` 把 sokol 的 `SAPP_EVENTTYPE_QUIT_REQUESTED`
（Windows `WM_CLOSE`、X11 `WM_DELETE_WINDOW`——Alt+F4 和关闭按钮都归到这里）直接
`sapp_quit()`，没给用户代码拦截的机会。

**修复**：收到 `QUIT_REQUESTED` 时改调 `sapp_cancel_quit()`（sokol 官方的“取消退出”
协议），窗口保持打开、游戏继续；日志打印 `quit request ignored`。主动退出仍走 `Ctrl+Esc`
（`event_cb` 里保留的 `sapp_quit()`）和游戏自己的“返回 DOS”菜单（`fd2_engine_game_over()`
→ `frame_cb` 的 `sapp_quit()`）。

**归属**：纯宿主/平台修复，按 `BRANCHING.md` 只进 `modern-platform`。`make -C host check`
加单独 `-fsyntax-only main_sokol.c` 通过。
