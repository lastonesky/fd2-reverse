# 《炎龙骑士团2》(FD2) 逆向还原 —— 现代原生引擎

> 把 DOS 游戏《炎龙骑士团2》的 `FD2.EXE` 逐函数逆向还原为 C 代码，并构建为可在
> Windows / Linux（macOS）原生运行的引擎：sokol 视频（D3D11/OpenGL/Metal）、
> WASAPI/ALSA 音频、内置 OPL3（Nuked OPL3）音乐合成 + FD2 自带 AIL 音色库。
>
> **状态：已还原完成，并经真实通关验证** —— 第 1 关到第 30 关完整流程可玩，
> 关键剧情道具的获取与使用（勇者徽章(89) 转职英雄、黄金徽章(209)、天空之钥(100)
> 通往黄金城）以及结局演示均正常。仍可能存在未覆盖分支 / 隐藏事件的 bug；
> 遇到异常请附 `fd2engine.log` 反馈。
>
> **构建**：本仓库不含原版程序的任何字节。把你合法持有的 `FD2.EXE`
> （md5 `a6e341a8decc6ebf7f4872076d9cf161`）放到仓库根目录（已被 `.gitignore`
> 忽略）或设 `FD2_EXE=/path/FD2.EXE`，然后：
> - **Linux / macOS**：`make`（= `tools/prepare_image.py` + `make -C host engine`）→ `host/build/fd2engine`
> - **Windows**：`pwsh -File build.ps1`（MSVC）→ `build\fd2engine.exe`
>
> 两者都会先经 `tools/prepare_image.py` 从 `FD2.EXE` 生成运行必需的数据段镜像；
> 没有 `FD2.EXE` 就明确报错停止。详见 `EXTERNAL_ARTIFACTS.md`。

## 1. 对象与已知事实

| 项 | 值 |
|---|---|
| 格式 | DOS/4GW LE（Linear Executable），LE 头 @文件 0x27ACC |
| 代码段 | `cseg01` 0x10000..0x4F000（258048 B），入口 `start` = 0x3CCB4 |
| 数据段 | `dseg02` 0x50000..0x556B0（22192 B，初始化数据+字符串+分派表）；`dseg03` 0x60000..0x64000（vsize 0x34D2） |
| 函数总数 | **1369**（IDA 库 `FD2.EXE.i64`，已含全部原型/边界修复并保存） |
| 分区 | 游戏本体 0x10000..0x37000（约 569+）；Watcom CRT 0x37000+；Miles AIL V3.02 0x37D3E..0x4D000；游戏工具库 0x4DED4..0x4F000 |
| 显示/音频 | VGA Mode 13h，320×200×256；Miles AIL V3.02（51 个命名入口） |
| 资源 | FDTXT/FDOTHER/FDFIELD/FDSHAP/DATO/FDMUS/BG/FIGANI/TAI/ANI.DAT、FDICON.B24、FD2.SAV/TMP、`SAMPLE.OPL`/`SAMPLE.AD`（AIL 音色库，格式见 `docs/opl2-music-fidelity.md` §17） |

所有地址以 `FD2.EXE.i64` 为权威。总体架构 / 入口与主循环 / 子系统依赖见
`reverse-engineering-overview.md`，数据布局见 `data-layout.md`，模块排程见
`module-map.md`，存疑清单见 `uncertainties.md`；逐轮修复与根因记录见 `PROGRESS.md`。

## 2. 仓库结构

| 路径 | 内容 |
|---|---|
| `src/game/` | 手工还原的游戏代码（87 个 `.c` 模块：状态机、地图、战斗、对白、转职、结局…），注释保留原址 `// 0xXXXXX` |
| `src/include/` | 生成头：段镜像访问宏（`fd2_globals.h`）、函数原型（`fd2_funcs.h`）、IDA 名→语义名别名 |
| `src/lib/` | CRT / AIL 边界头（`crt.h`、`ail.h`）——映射到平台层，不逐条还原库实现 |
| `src/fd2_image.c`、`fd2_dispatch.c`、`fd2_relocate.c` | 数据段镜像初始化；7 张分派表（真实宿主数组）；生成的空 relocate（历史职责见 `PROGRESS.md`） |
| `host/` | 现代宿主：sokol 入口/视频/音频、OPL3 合成（`opl3/`）、AIL 音色（`ail_timbres.c`）、输入/看门狗/启动（`fd2_kernel.c`）→ 详见 `host/README.md` |
| `tools/` | 构建与维护工具：`prepare_image.py`、存档工具 `fd2sav*`、字形/对白导出等 → 详见 `tools/README.md` |
| `analysis/` | 段镜像 `seg_dseg0{2,3}.bin` 与原型/数据类型覆盖表；大批量导出已移出 git，见 `EXTERNAL_ARTIFACTS.md` |
| `docs/` | 机制 / 隐藏事件 / 对白 / 结局 / 音乐保真 / 启动调用链（见下「文档索引」） |

## 3. 运行

引擎在**游戏数据目录**中运行（读取 `FDTXT.DAT`、`FDOTHER.DAT`、`FDSHAP.DAT`、
`FDFIELD.DAT`、`DATO.DAT`、`FDMUS.DAT`、`BG.DAT`、`FDICON.B24`、`SAMPLE.OPL` …
与 `FD2.SAV`）：

- **Linux / macOS**：`./run-linux.sh [DATA_DIR]`（缺数据文件会提示），或
  `cd DATA_DIR && /path/to/fd2engine`；
- **Windows**：`cd E:\FD2`（你的数据目录）后运行 `build\fd2engine.exe`。

显卡驱动异常时可加 `FD2_GL_SOFTWARE=1`（强制 Mesa 软件渲染，Linux）。
脚本化按键 / 回放（`--keys`、`--keylog`、`--keyplay`）用于回归测试；它们会读写
`FD2.SAV`，建议在隔离目录（如 `testdata/`）里跑，并先备份存档。

## 4. 还原工程约定（src/ 与内存模型）

- 三个段镜像数组（`fd2_code`/`fd2_data1`/`fd2_data2`）+ 生成头里的访问宏
  （`#define dword_53C03 (*(uint32_t *)&fd2_data1[0x3C03])`）：还原代码与
  Hex-Rays 伪代码零改动即可编译，地址只在生成头里出现一次。
- **去 guest 化已完成**：引擎是 64 位原生构建，源码中不再出现 guest 绝对地址
  （残留扫描工具 `tools/scan64.sh`）；7 张分派表是 `src/fd2_dispatch.c` 的真实
  数组，`src/fd2_relocate.c` 只是生成的空实现。
- 未对齐访问（如分派表 0x51B91 奇地址）在 x86/x86-64 下合法。
- `FD2_GUEST_FIXED` 混合 profile 仍保留，用于与 `port/` 的差分对拍器联编
  （`PROGRESS.md` §0-B）。

## 5. 范围决策

| 区域 | 策略 |
|---|---|
| 游戏本体 0x10000..0x37000 + 工具库 0x4DED4..0x4F000 | **逐函数手工还原**（去伪影：`a1:` 命名参、`JUMPOUT` 共享尾声、栈探针、垃圾实参），一模块一文件，注释保留 `// 0xXXXXX` |
| Watcom CRT（malloc/fopen/int386…） | **映射到标准 C / 平台层**（`src/lib/crt.h`、`src/platform.h`），不逐条还原库实现；原型与语义以反编译为准 |
| Miles AIL（51 个入口 + 内部） | **库边界**：不还原 AIL 内部；游戏调用的入口由 `host/fd2_audio.c` 实现，音乐经 `host/synth.c` + Nuked OPL3 合成，音色来自游戏数据 `SAMPLE.OPL`（机制见 `docs/opl2-music-fidelity.md`） |
| 反编译失败 3/1369（0.2%） | 均为 AIL 回调 / 疑似死代码，定性与反汇编存档记录见 `PROGRESS.md`（文件已移出 git，见 `EXTERNAL_ARTIFACTS.md`） |

## 运行参数（命令行开关）

宿主统一走 `--name[=value]`（也接受 `--name value`，大小写与 `-`/`_` 不敏感），
环境变量 `FD2_<NAME>` 仍作为回退，二者等价：

| 开关 | 环境变量 | 说明 |
|---|---|---|
| `--unfocus=mute` | `FD2_UNFOCUS` | 失焦/最小化时静音，回来恢复（**默认**） |
| `--unfocus=pause` | | 冻结游戏线程（隐含静音，音乐在混音线程合成，不静音会继续响） |
| `--unfocus=none` / `sound` | | 不处理，后台照常出声/运行 |
| `--unfocus=fade` | | 失焦音量淡出、获得焦点淡入 |
| `--unfocus-fade-ms=N` | `FD2_UNFOCUS_FADE_MS` | 淡入淡出时长，默认 300 ms |
| `--close=button` | `FD2_CLOSE` | **默认**：标题栏关闭按钮可关，Alt+F4/Cmd+W 拦截 |
| `--close=both` | | 两种都关 |
| `--close=none` | | 两种都拦（关闭按钮也不响应；Ctrl+Esc 在 Windows 会被开始菜单截获，实际不可用） |
| `--volume=N` | `FD2_VOLUME` | 主音量 0..100（默认 100，混音器统一衰减） |
| `--midi-backend=sbpro2` | `FD2_MIDI_BACKEND` | 内置 OPL3 合成器（默认；也接受 `opl3`/`synth`/`builtin`）。`winmidi` 仅 Win32 |
| `--opl-channels=N` | `FD2_OPL_CHANNELS` | FM 声部数，默认 18（SBPRO2.MDI） |
| `--ail-timbres=FILE` | `FD2_AIL_TIMBRES` | AIL 音色库路径；默认依次尝试 `SAMPLE.OPL`、`SAMPLE.AD`（游戏数据自带） |
| `--upscale=nearest\|linear\|sharp\|bicubic` | `FD2_UPSCALE` | 呈现层放大算法（默认 nearest） |
| `--aspect=off` | `FD2_ASPECT` | 关掉 8:5（320×200）比例保持，铺满窗口 |
| `--log=FILE` | `FD2_LOG` | 日志文件（默认 `fd2engine.log`） |
| `--autoquit-ms=N` | `FD2_AUTOQUIT_MS` | N 毫秒后自动关窗（测试用） |
| `--keys=HEX,..` | `FD2_KEYS` | 脚本化按键（BIOS scan 码，十六进制逗号分隔） |
| `--key-delay=N` | `FD2_KEY_DELAY` | 上述按键的注入间隔，默认 800 ms |
| `--keyplay=FILE` | `FD2_KEYPLAY` | 回放 `--keylog` 记录的时间线 |
| `--keylog=FILE` | `FD2_KEYLOG` | 记录按键时间线 |
| `--hang-exit` | `FD2_HANG_EXIT` | 看门狗判定挂起时直接关窗（默认只记日志） |
| `--stall-ms=N` | `FD2_STALL_MS` | 挂起判定阈值（毫秒），默认 10000 |
| `--typematic=0` | `FD2_TYPEMATIC` | 关掉方向键连发 |
| `--typematic-delay=N` | `FD2_TYPEMATIC_DELAY` | 长按后开始连发的延迟，默认 500 ms |
| `--typematic-rate=N` | `FD2_TYPEMATIC_RATE` | 连发周期，默认 33 ms（≈30 次/秒，对齐 DOSBox-X 默认） |

例：

```
fd2engine.exe --unfocus=pause --volume=60 --upscale=sharp
fd2engine.exe --unfocus=fade --unfocus-fade-ms=500
fd2engine.exe --unfocus=none            # 不需要失焦处理（= sound）
```

启动日志会列出识别到的开关（`fd2: arg   --NAME = '...'`）并把启动失败/策略写进去。

## 常见问题：结局

结局有两处容易被误认为 BUG 的**原机行为**（不是移植缺陷）：

- **只介绍 2–3 人的去向**：名单演示途中按任意键会跳到末尾（原机设定）。想看全 32 人，
  最后一段结局对白出现后就不要再按键。
- **停在 THE END、音乐继续、按键无响应**：原机在结局后就是死循环。移植版点标题栏的
  关闭按钮即可正常退出，不必强杀进程；Alt+F4 默认被拦截，可用 `--close=both` 开启。

详见 [docs/ending-behavior.md](docs/ending-behavior.md)。

## 文档索引

| 文档 | 内容 |
|---|---|
| `PROGRESS.md` | 工程台账：每轮还原 / 修复的记录与根因分析 |
| `EXTERNAL_ARTIFACTS.md` | 仓库外关键产物（`FD2.EXE`、IDA 库）与数据段镜像生成 |
| `reverse-engineering-overview.md`、`data-layout.md`、`module-map.md`、`uncertainties.md` | 逆向总体架构 / 数据布局 / 模块排程 / 存疑清单 |
| `docs/game-knowledge.md` | 法术、装备、道具、转职与敌人装备规则 |
| `docs/hidden-events.md` | 全关卡隐藏事件格与隐藏商店 |
| `docs/dialogue.md`（另有 `.html`） | 全对白导出（繁体原文） |
| `docs/ending-behavior.md` | 结局两处原机行为的证据与应对 |
| `docs/opl2-music-fidelity.md` | 音乐还原与音质保真 |
| `docs/STARTUP-CALLCHAIN.md`、`docs/STARTUP-LATENCY.md` | 启动调用链与首启延迟 |
| `host/README.md`、`tools/README.md` | 宿主后端说明 / 工具速查 |
| `BRANCHING.md` | 分支模型（`main` 还原源码权威版 vs `modern-platform` 原生引擎） |
| `HD_REMASTER.md` | 高清化方案讨论稿（未实施） |
