# module-map.md — 模块划分、依赖与还原排程

> 第一阶段交付物（含第二阶段范围/优先级决策）。覆盖数据来自 `function-map.csv`
> 聚合（脚本重算法：`python tools/gen_function_map.py` 后自行 crosstab）。
> 原则：**语义完整单元**切分，不按地址段切；验证资产优先复用，不新造。

## 0. 覆盖总览（截至本轮）

| 状态 | 数量 | 说明 |
|---|---|---|
| 已验证行为一致 | **309** | `port/re/translation_map.csv`：逐函数机器码对拍（cases 见表）+ `repl.c` 已接入 |
| 已还原，尚未动态验证 | 7 | port3：`main`(boot.c) + 6 个 AIL 入口签名(lib/ail.h) |
| 待还原 | 316 | 游戏区 298 + 工具库 5 + 模块推断 13 |
| 暂时保留接口桩 | 683 | AIL 148 + 库区未分类 535（**不逐条还原**，桩/映射策略） |
| 已确认且无需重写 | 54 | 命名 CRT → libc/宿主层映射（复用 `port/src/dos.c`） |
| 需要进一步分析 | 1 | `0x367D1`（静态不可达 + 反编译失败） |
| **调用量覆盖** | **71.3%** | done-usage 4324 / total 6068（加权视角比函数数更真实） |

## 1. 模块清单（已验证部分 = 可直接复用的源码与对拍器）

> 复用方式：将 `port/src/game/<mod>.{c,h}` 与对拍器作为**已确认资产**引入 port3
> （引用方式见 §4 迁移策略），不重写、不重新对拍。

| 模块 | 函数数 | 职责 | 权威源 | 对拍器 | 共享状态（主） |
|---|---|---|---|---|---|
| rle / rle2 | 5+1 | RLE 行解压 blit、0xC0 文本 blit | rle.c/rle2.c | rlecheck/rle2check | 目标帧缓冲 |
| gfx | 7(+3 推断) | 矩形存取/透明 blit/字形/扫描线重排 | gfx.c | gfxcheck | 0xA0000、`53A65` |
| sprite24 | 7 | 24×24 精灵 7 变体 | sprite24.c | sprite24check | 子图偏移表 `53A61` |
| util / tables / path | 19+19+2(+6) | 字节/调色板变换、表访问器、BFS 寻路 | util.c/tables.c/path.c | utilcheck/tablescheck/pathcheck | dseg03 表、`60068` 格网 |
| res / unitld | 4+7 | DAT/LMI 解包、FDICON 链 | res.c/unit_load.c | rescheck/ev6check | 8 个资源指针、`53BFF`(待核) |
| rec / unit | 30 | 80B 记录表全操作 | rec.c/unit.c | reccheck(74k 例) | `53A45/53BEB/53BF7/53BFB` |
| vm | 1 | 0x15F84 脚本解释器（9 栈参） | vm.c | vmcheck(5512) | EDI=当前记录 |
| ev / ev2..ev6 | 30+30+28+28+25+10 | 三张事件表 91+ 项 handler | ev*.c | evcheck/ev2check/ev6check | 事件上下文、地图/记录 |
| scene / menu | 8+35 | 状态切换 handler、菜单 action | scene.c/menu_actions.c | scenecheck | `53C03/53ECC/51AAC` |
| map / anim / fade | 19+?+6 | 地图视图/揭示/动画帧/淡变 | map.c/anim.c/fade.c | mapcheck(122500)/fadecheck | `53A51/53AC1/53C03/60000` |
| dlg / msg / fx | 23+?+9 | 对话框/合成屏/效果动画(10 中 9) | dlg.c/msg.c/fx.c | boxcheck/keycheck/msgcheck/fxcheck | `53C5B/F/63`、funcs_30469 |
| svc / bgm / kbd | 5+1+1 | 等键/tick/PCM、换曲、键盘 | svc.c/bgm.c/kbd.c | leafcheck/bgmcheck | AIL 句柄、BDA |
| world_load 等 | ? | 早期装载（10652/1088D 已在组内） | world_load.c | worldcheck | — |

## 2. 待还原模块与优先级（第二阶段排程依据）

**排序依据**：RE_MAP ★ 评级（专家排序）+ function-map 调用量 + 依赖闭合需求
（instruction §3 的优先面：主循环/核心状态 > 实体数据 > 核心算法 > 随机计时 > 文件/存档）。

### P0 — 主循环与核心状态（阻塞一切整体运行）
| 内容 | 锚点 | 备注 |
|---|---|---|
| 主状态机三件套 | `sub_25EBB`(0x25EBB)、`sub_117E7`(0x117E7)、`sub_26152`(0x26152) + `sub_22E5C` | 每 tick 执行；调用量权重被 usage 低估（调用者少但频率极高） |
| 进入/离开表 | `funcs_25E23` 剩 ~19 项、`funcs_25E3A` 剩 ~22 项 | 已有 6+8 项 scene/menu 资产可套用模式 |
| 解释器簇 | `0x1AA1D`(u9)、`0x197E5`、`0x19953`(**u17 待还原第 1**)、`0x1DB65`(u8, notify) | RE_MAP 标注"解释器簇"，与 vm/ev 强耦合 |

### P1 — 高调用量长尾（top usage 待还原，见 function-map 排序）
`0x2C67D`(u15, fx 表[6] 最后一块 1151B)、`0x2FACD`(u15)、`0x115B6`(u9)、
`0x14B78`(u8, 五个 caller 共享)、`0x18409`(u8)、`0x24618`(u8, 六 caller)、
`0x1C75E`(u7)、`0x13FD4/14EF0/1B932/2189A/22046`(u6) …（完整见 csv）
按簇打包还原（多 caller 共享 = 同一源文件的概率高，用 callees 关系聚类）。

### P2 — 语义关键块
- **存档/文件直接状态**：`sub_10010`(0x10010, 1552B, FD2.SAV/TMP 全链) —— 先补分析
  （它 36 个 callee、未还原，是存档唯一入口）。
- **funcs_1197B(30 槽)/funcs_1541F(28 槽)** 两表 handler（覆盖状态待查，可能大半未还原）。
- 工具库残余 5 + 模块推断 13（rle/path/gfx/fx/bgm/ev3 的同组尾巴）。
- `world_load`/`unit` 组残余、`0x10652/1088D` 周边未入组部分。

### P3 — 收尾与整体
- 298 个无组 game 函数按依赖闭合逐批清（**禁止**逐函数孤立处理：按 P0/P1 的调用链
  整簇拉出，一次带全 callee）。
- `0x367D1` 死代码：决定删除或手工翻（反汇编已存）。

### 库区（不投入还原）
- CRT 54 命名 + 535 未分类库内部 → libc/宿主层（复用 `port/src/dos.c` 服务表）+ 桩；
- AIL 148 内部 + 已声明 16 入口 → `lib/ail.h` 边界 + 适配（复用 `port/src/ail.c`/AUDIO.md 结论）。

## 3. 依赖与数据流（模块间）

```
CRT/平台层(dos.c复用) ─► 所有模块（stdio/malloc/int386/时间）
AIL 边界(ail.h/桩)    ◄─ bgm/svc ─ ◄─ scene/menu/主循环
资源层 res/unitld ─► gfx/sprite24/rle ─► dlg/msg/map/fx/anim
记录表 rec/unit ─► vm ─► ev*(三表) ─► scene ─► 主循环(sub_25EBB/117E7/26152)
                      └► menu/map/dlg（读写同一记录）
map/path ─► scene（地图态）；fade/gfx ─► 几乎所有演出模块
随机(sub_4EBE3)/计时(BDA tick/0x4E310)/kbd ─► svc ─► 全部交互路径
```
- 共享状态的所有权：`53C03/53ECC/51AAC` = scene（主循环只读写这几个枢纽）；
  记录表 = rec 独占写（三写入者已锁定）；调色板 = fade/gfx；资源指针 = res 写、大家读。
- 反向依赖注意：**vm/ev 深度依赖"当前记录指针"约定（EDI 语义）**——还原 ev 簇时必须
  连同 vm 上下文一起（不可只拉单函数）。

## 4. 还原/迁移策略（批量模式，控制重复劳动）

1. **已验证 309**：整体引入（源码 + 对拍器保留为回归资产），状态直接标"已验证"；
   仅做接口层适配（见 `uncertainties.md` §设计假设 A：FD2_GUEST_FIXED 双 profile）。
2. **新还原（P0..P2）**：按 `port/docs/TRANSLATION.md §1` 七步的**批量版**——
   一个模块一次做完：伪代码（`analysis/decomp/` 按地址取）+ 该簇共享全局 → 模块 .c/.h →
   编译/静态检查（第五阶段轻量验证）→ 有对拍器的套用，无对拍器的标"已还原未验证"。
3. **桩**：集中在 `src/lib/`（ail.h 已立），桩的行为假设必须写在桩头注释 + uncertainties 登记。
4. **每模块落盘**：源码 + function-map 状态更新 + PROGRESS 一句话（防上下文丢失）。
