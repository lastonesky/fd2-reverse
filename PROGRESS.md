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

### 验证与台账

- `tools/check_compile.sh --full`：双 profile 0 错误、`-Werror` 双 profile 全绿、伞头自检 OK。
- `function-map.csv`：待还原 **276 → 258**，已还原未验证 **47 → 65**（+18）。

### P1 余量（下一批候选，按 `function-map.csv` usage 排序）

`0x115B6`(u9)、`0x14B78`(u8)、`0x1C75E`(u7，链 `0x1C81F`→`0x14121`→`0x14B78`)、
`0x14EF0/1B932/26EDA/27D33/2E9A8`(u6) ……
策略不变：**按 callee 簇整体拉出**（一个模块一次做完），并先剔除共享尾声造成的伪 callee。

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
