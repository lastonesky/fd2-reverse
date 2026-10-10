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
