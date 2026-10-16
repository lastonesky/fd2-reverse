# FD2.EXE 逆向还原 —— 第一阶段：纯静态逆向

> 目标：把 `E:\FD2\FD2.EXE`（md5 `a6e341a8decc6ebf7f4872076d9cf161`）逆向还原为 C 语言代码。
>
> **构建**：本仓库不含原版程序的任何字节。把你合法持有的 `FD2.EXE`（md5 同上）放到仓库根目录
> （已被 `.gitignore` 忽略）或设 `FD2_EXE=/path/FD2.EXE`，然后：
> - **Linux / macOS**：`make`（= `tools/prepare_image.py` + `make -C host engine`）；
> - **Windows**：`pwsh -File build.ps1`（MSVC）。
>
> 两者都会先经 `tools/prepare_image.py` 从 `FD2.EXE` 生成运行必需的数据段镜像；
> 没有 `FD2.EXE` 就明确报错停止。详见 `EXTERNAL_ARTIFACTS.md`。
> **阶段纪律：只允许静态分析。不运行原程序、不运行调试器、不编译/运行还原出来的 C 代码。**
> 运行对拍、编译调试属于第二阶段（另行开展）。

## 1. 对象与已知事实

| 项 | 值 |
|---|---|
| 格式 | DOS/4GW LE（Linear Executable），LE 头 @文件 0x27ACC |
| 代码段 | `cseg01` 0x10000..0x4F000（258048 B），入口 `start` = 0x3CCB4 |
| 数据段 | `dseg02` 0x50000..0x556B0（22192 B，初始化数据+字符串+分派表） |
| | `dseg03` 0x60000..0x64000（16384 B，vsize 0x34D2） |
| 函数总数 | **1369**（IDA 库 `E:\FD2\FD2.EXE.i64`，已含本次全部修复并保存） |
| 分区 | 游戏本体 0x10000..0x37000（约 569+）；Watcom CRT 0x37000+；Miles AIL V3.02 0x37D3E..0x4D000；游戏工具库 0x4DED4..0x4F000 |
| 资源 | FDTXT/FDOTHER/FDFIELD/FDSHAP/DATO/FDMUS/BG/FIGANI/TAI/ANI.DAT、FDICON.B24、FD2.SAV/TMP（格式见 `E:\FD2\FD2_analysis.md`） |

历史资产（前人多轮分析，本阶段复用其**结论**，地址一律以本 IDA 库复核）：
`port/re/RE_MAP.md`（函数分区/核心档案/分派表）、`port/re/funcmap.csv`、
`port/re/translation_map.csv`（309 个已命名函数）、`port/docs/*`（PITFALLS 记录了
栈探针伪像、真 ABI=cdecl 等关键坑）。

## 2. 本阶段产出（analysis/）

| 文件 | 内容 |
|---|---|
| `decomp/`（1366 个 `.c`） | **全量 Hex-Rays 伪代码**，每个函数一文件，头注释含地址/大小/原型 |
| `disasm_fallback/`（3 个 `.asm`） | 3 个反编译失败函数的完整反汇编 |
| `funcmap.csv` | 1369 函数：地址/大小/名字/分区/调用者计数（游戏侧 vs 库侧） |
| `prototypes.csv` | 每个函数的当前原型（还原时的签名基准） |
| `arity.json` | **1114 个函数的实参个数**（调用点证据推导，见 §3.2） |
| `strings.csv` | 325 条字符串（utf-8 282 / big5 43，含交叉引用） |
| `names.csv` | 551 个已命名地址（段/种类） |
| `dispatch_tables.csv` | 7 张分派表（funcs_1197B/1199C/1541F/25E3A/25E23/30469/37012）逐槽解析 |
| `prototype_heal_log.json` | 原型修复迭代日志（29→3 失败的自愈轨迹） |
| `seg_cseg01.bin` / `seg_dseg02.bin` / `seg_dseg03.bin` | 三段原始字节（还原镜像的数据源） |

## 3. 方法与关键修复（第一阶段的核心技术工作）

### 3.1 原型误判连锁（本次最大卡点，已解决）
IDA 对 Watcom 代码的栈探针伪像（`push size; call 0x3702F`，`0x3702F` =
`xchg eax,[esp+size]; call __CHKSTK; retn 4`）误推 ABI，导致：

- 8 个被调函数被标成 `__fastcall` 26 参数 → **22 个调用者 Hex-Rays `MERR_BADCALL`**；
- 686 个函数原型与真实压栈不一致 → 伪代码**静默丢实参**（比报错更危险）。

修复流水线（已写入 IDB）：
1. **调用点证据推导实参个数**：扫描全段 6556 个 call 站点，取 `add esp,4n`/`lea esp`
  （权威）与前置 `push` 计数（佐证），间接调用经分派表展开到全部目标；
   共识得到 **1114 个函数的真实 cdecl 参数个数**（`arity.json`）。
2. 批量回写 `int __cdecl f(int,…)`（686 个应用 / 428 个已一致 / 255 个无直接调用点）。
3. **自愈迭代**：全量反编译 → 失败点 `errea` 定位到具体 call → 对目标函数
   试错参数个数 → 全调用者复验。29 → 3 失败收敛。

### 3.2 边界错误（已修复）
- 原 `sub_36B00` 是**误造函数**：`push offset sub_36B00` 实为
  `malloc(0x36B00)` / `memset(...,0x36B00)` 的**立即数大小**被当成代码引用。
  真实函数起点 = **0x367D1**（Watcom 序言 `push esi/edi/ebp; sub esp,2C0h`），已重建。

### 3.3 反编译残余失败（3/1369 = 0.2%，已定性）
| 函数 | 错误 | 定性 |
|---|---|---|
| `sub_459F5` | `positive sp value` | AIL 内部回调（`AIL_register_timbre_callback` 注册） |
| `sub_45A20` | `call analysis failed` | AIL 内部回调（`AIL_register_event_callback` 注册） |
| `sub_367D1` | `call analysis failed` | **无任何静态调用/跳转/表引用 ⇒ 疑似死代码**（`sub_3669A` 尾部 `retn` 不落穿）；已存 `disasm_fallback/` 备手工翻译 |

三者均有完整反汇编兜底（`disasm_fallback/`）。

## 4. 还原工程约定（src/）

```
src/
  include/
    fd2_globals.h   ← 生成：段镜像 + 全部命名数据的访问宏（保留 IDA 名）
    fd2_funcs.h     ← 生成：全部函数原型
  game/             ← 手工还原的游戏代码（本阶段主产出，逐模块推进）
  lib/              ← CRT/AIL：映射或桩（见 §5 范围决策）
  fd2_image.c       ← 生成：段镜像初始化数据 + fd2_data_init()
  fd2_relocate.c    ← 生成：数据段内代码指针 → 真实函数地址的重定位
```

**内存模型**：原程序是平坦 32 位（数据固定在 0x50000/0x60000）。还原不把内存钉在
绝对地址，而是：
- 三个段镜像数组（`fd2_code`/`fd2_data1`/`fd2_data2`），
- 每个命名数据符号 → `#define dword_53C03 (*(uint32_t *)&fd2_data1[0x3C03])`
  （伪代码零改动即可编译；地址只在生成头里出现一次）。
- 表内绝对代码指针由 `fd2_relocate()` 在启动时统一改写（第一阶段只生成代码不运行）。

**注意**：初始构建按 **32 位**（指针=4 字节，与镜像内指针同宽）；64 位/跨平台属于
第二阶段之后的“去 guest 化”工作（前人 `port/docs/TRANSLATION.md` §6 路线可循）。
未对齐访问（如分派表 `0x51B91` 奇地址）在 x86/-m32 下合法，宏实现处有标注。

## 5. 范围决策

| 区域 | 策略 |
|---|---|
| 游戏本体 0x10000..0x37000 + 工具库 0x4DED4..0x4F000 | **逐函数手工还原**（去伪影：`a1:` 命名参、`JUMPOUT` 共享尾声、栈探针、垃圾实参），一模块一文件，注释保留 `// 0xXXXXX` |
| Watcom CRT（malloc/fopen/int386…） | **映射到标准 C/平台层**（不逐条还原库实现；原型与语义以反编译为准） |
| Miles AIL（51 个入口 + 内部） | **库边界**：伪代码全量留档；还原侧给接口桩/适配（第一阶段不逐函数还原） |
| 反编译失败 3 个 | `disasm_fallback/` 手工翻译兜底 |

## 6. 阶段纪律（重要）

1. 本阶段**不得**运行 `FD2.EXE`、DOSBox、调试器，**不得**编译或执行 `src/` 下的还原代码。
2. 一切结论来自静态证据（反编译/反汇编/交叉引用/字节扫描）；与前人运行期结论冲突时
   标注“待第二阶段对拍”。
3. 所有地址以 `FD2.EXE.i64` 为准（外部资料是另一 build，md5 不同）。

## 运行参数（命令行开关）

宿主现在统一走 `--name[=value]`（也接受 `--name value`，大小写与 `-`/`_` 不敏感），
环境变量 `FD2_<NAME>` 仍作为回退，二者等价：

| 开关 | 环境变量 | 说明 |
|---|---|---|
| `--unfocus=mute` | `FD2_UNFOCUS` | 窗口失焦/最小化时静音（默认 `mute`） |
| `--unfocus=pause` | | 额外冻结游戏线程（隐含静音） |
| `--unfocus=mute,pause` | | 两者都要 |
| `--unfocus=none` | | 关闭该特性（后台照常出声/运行） |
| `--volume=N` | `FD2_VOLUME` | 主音量 0..100（默认 100） |
| `--upscale=nearest\|linear\|sharp\|bicubic` | `FD2_UPSCALE` | 呈现层放大算法（默认 nearest） |
| `--aspect=off` | `FD2_ASPECT` | 关掉 4:3 保持，铺满窗口 |
| `--log=FILE` | `FD2_LOG` | 日志文件（默认 `fd2engine.log`） |
| `--autoquit-ms=N` | `FD2_AUTOQUIT_MS` | N 毫秒后自动关窗（测试用） |
| `--autokey=HEX,..` | `FD2_KEYS` | 脚本化按键（BIOS scan 码） |
| `--keyplay=FILE` | `FD2_KEYPLAY` | 回放 `--keylog` 记录的时间线 |
| `--keylog=FILE` | `FD2_KEYLOG` | 记录按键时间线 |
| `--hang-exit` | `FD2_HANG_EXIT` | 看门狗判定挂起时直接关窗（默认只记日志） |
| `--stall-ms=N` | `FD2_STALL_MS` | 挂起判定阈值（毫秒） |
| `--typematic=0` | `FD2_TYPEMATIC` | 关掉方向键连发 |

例：

```
fd2engine.exe --unfocus=mute,pause --volume=60 --upscale=sharp
fd2engine.exe --unfocus=none            # 不需要失焦处理
```

启动日志会列出识别到的开关（`fd2: arg   --NAME = '...'`）并把启动失败/策略写进去。
