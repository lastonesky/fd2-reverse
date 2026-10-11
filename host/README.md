# host/ — 复用 `port/` 的已验证宿主与后端

本目录是 `modern-platform` 分支的**平台/后端层**，来源是已经调通、画面与声音都验证过的
`E:\FD2\port`（下称 port）。原则：**能原样搬的就原样搬，不重写、不重新对拍**；只有
port 里与“运行原始机器码”强耦合的部分（LE 加载、repl 热替换、DOS 故障注入）才另写适配。

## 1. 原样搬入（verbatim，已在本仓库 WSL gcc 下 0 error）

| 文件 | 角色 | 依赖 |
|---|---|---|
| `render.h` | **视频抽象接口**：`render_init/present/resize/shutdown`，输入 320×200 BGRA | — |
| `render_sokol.c` | **现代跨平台视频实现**：sokol_gfx（Win=D3D11 / mac=Metal / Linux=GL） | sokol_gfx/app/glue/log |
| `render_gdi.c` | Win32 参考实现/回退（对拍基准） | windows.h |
| `audio.h` | **音频抽象接口**：单设备 + 单软件混音器（music + Σsfx） | — |
| `audio_sokol.c` | **现代跨平台音频实现**：sokol_audio（WASAPI/CoreAudio/ALSA） | sokol_audio |
| `synth.h/.c` | 软件 MIDI 合成器（序列渲染成 PCM） | audio/dls |
| `xmidi.h/.c` | XMIDI 解析（AIL 的曲目格式；≠标准 SMF） | synth/audio |
| `dls.h/.c` | DLS Level-1 音色库（gm.dls）解析 | — |
| `keys.h`、`keys.c`、`keys_win32.c` | 便携键词汇 `fr_key` ↔ BIOS 扫描码 | 平台 |
| `platform_posix.c`、`platform_win32.c` | `src/platform.h` 的 OS 实现（内存/文件/线程/时钟） | 平台系统头 |
| `sokol_impl.c` + `vendor/sokol/` | sokol 单编译单元实现 + 头（47k 行，随仓库） | 系统 GL/ALSA 等 |

> `src/platform.h` 已经是 port 的同一份（port3 早期迁移过），因此这里**不重复放**
> `platform.h`，构建时用 `-I../src` 解析。

## 2. 不复用（port 的“跑机器码”机器）

| port 文件 | 为什么不需要 |
|---|---|
| `le.c` / `entry.c` | LE 加载器/地址预约——port3 已去 guest，不再是 guest 镜像 |
| `dos.c` / `dos_fault_*.c` | 软件中断陷阱 + 低内存仿真——port3 用原生调用替代 |
| `repl.c` | 机器码↔C 热替换缝——C1–C4 后已无机器码 |
| `host.c` / `main_*.c`（port 版） | 宿主内核是“起 guest 线程”的，需为原生引擎另写（见 §3） |

## 3. 待写的适配层（port3 专用，新增）

| 适配件 | 职责 | 复用来源 |
|---|---|---|
| `host/fd2_kernel.c` | 原生宿主内核：加载资源 → 跑 port3 的 `main` → 每帧 `host_frame` | port `host.c` 的帧/截图/时钟逻辑 |
| `host/fd2_video.c` | `fd2_vga`(C3 缝) + 调色板副本 → BGRA → `render_present` | port `host.c` 的 palette→BGRA |
| `host/fd2_audio.c` | 实现 `src/lib/ail.h` 的 16 个 AIL 入口 | port `ail.c` 内部逻辑 |
| `host/fd2_platform.c` | `int386`/`outp`/`delay`/`fd2_mem_lock` + BDA tick/键环写入 `fd2_lowmem` | port `dos.c` 的相关分支 + `keys.c` |

### 关键对接点（port3 ↔ host）

- **视频**：port3 画面在 `fd2_vga[0..0xFA00]`（`src/game/vga.h`），调色板在数据镜像
  `0x53A65`（`pal_fade_range` 写的副本）。适配层把两者转成 BGRA，交给 `render_present`。
- **音频/音乐**：port3 游戏侧只调 `src/lib/ail.h` 的入口（`boot.c`/`bgm.c`/`svc.c`/…）。
  `fd2_audio.c` 用 port 的 `xmidi.c`+`synth.c`+`audio.h` 实现它们：`AIL_init_sequence` 交给
  `xmidi`，`AIL_set_sample_address`/`_start_sample` 交给 `audio_sfx_play`。
- **输入/计时**：port3 游戏读低内存镜像 `fd2_lowmem`（BDA tick `0x46C`、键缓冲 `0x41A/41C`）。
  适配层把宿主键盘/定时器写进这个数组，等价于 port `dos.c` 对 guest 做的事。

## 4. 构建

见 `host/Makefile`（POSIX 后端对象语法自检）。完整可运行宿主需要 §3 的适配层落地后，
把 `src/*.c` + `src/game/*.c` + `host/*.c` 一起链接。

## 5. 经验直接继承（不用重踩）

- `docs/AUDIO.md`（在 `port/`）：XMIDI delta 累加、无 running status、Note-On 带音长；
  单设备混音器与 start gate；8-bit unsigned mono 11025 的判定依据。
- `docs/BACKEND.md`、`rounds/16-entry-layer.md`：sokol 三平台、XWayland、同 tick 抓帧比较。
- `docs/PITFALLS.md`：音频/宿主踩坑清单（§8-52 起播抢跑、§8-54 声音取证等）。
