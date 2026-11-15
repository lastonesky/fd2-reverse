# 启动调用链：从 `main` 到游戏画面（startup-callchain）

> 记录当前 `modern-platform` 分支上，进程入口到游戏真正跑起来的完整函数调用链。
> 行号为撰写时的实测值，若代码移动以 `grep -n` 为准。
> 相关文档：`docs/STARTUP-LATENCY.md`（启动耗时埋点）、`src/game/boot.c` 头注释（原版地址对照）。

## 0. 结论速览

- **真正的 `int main` 在 `host/main_sokol.c`，不在 `src/`。** `src/` 里只有引擎侧入口
  `fd2_game_main()`（`src/game/boot.c:74`），它对应原版 `FD2.EXE` 的 `main @0x25BF4`。
- **游戏逻辑不跑在主线程**：`fd2_engine_start()` 起一个 `game_thread` 跑
  `fd2_game_main()`，宿主线程只负责窗口、D3D11 渲染和事件。
- 启动的最后一步是 `fd2_game_main()` 里的**外层主状态机**（`boot.c` 第 7 步）：
  先播 BGM 18，再进 `scenario_menu()` → `title_screen_run()` 显示标题画面。

## 1. 宿主线程（窗口 / 渲染 / 事件）

```
host/main_sokol.c:568   int main(int argc, char **argv)
 ├─ host/fd2_kernel.c:673   fd2_engine_init(argc, argv)
 │     host_args_init / --log 重定向 stdout+stderr
 │     fd2_install_crash_handler()
 │     fd2_data_init()                段镜像（FD2.EXE 生成物）
 │     dos_lowmem_base = fd2_lowmem   BDA 低内存映射
 │     fd2_video_default_palette() / fd2_video_init()
 │     audio_init(44100) + ail_set_format(11025,8,0)
 │     parse_keys() / parse_keyplay() / keylog / unfocus 策略
 │     → 返回 0
 ├─ 读 AUTOQUIT_MS / CLOSE 参数，设置 g_rd（320x200, scale=3）
 └─ sapp_run(&desc)                   创建窗口 + D3D11 设备 + swapchain
      ├─ host/main_sokol.c:359  init_cb()
      │    ├─ render_init(&g_rd)                  渲染后端就绪
      │    └─ host/fd2_kernel.c:781  fd2_engine_start()
      │         ├─ plat_thread(game_thread)     ★ 游戏线程（见 §2）
      │         ├─ plat_thread(tick_thread)      18.2 Hz BIOS tick → lowmem[0x46C]++
      │         ├─ plat_thread(stat_thread)      每 2 s 打印 fps / tick_rate
      │         ├─ plat_thread(stall_thread)     卡死看门狗（心跳见 §4）
      │         ├─ plat_thread(repeat_thread)    typematic 连发（--typematic）
      │         ├─ plat_thread(key_thread)       --autokey 注入
      │         └─ plat_thread(keyplay_thread)   --keyplay 按键回放
      │
      └─ 每帧: host/main_sokol.c:387  frame_cb()
           ├─ fd2_engine_frame() → host/fd2_video.c:68  fd2_video_frame()
           │       读 VGA 显存发布到 swapchain，fd2_video_publish_count++
           ├─ 首帧打 tlog "first frame presented"
           ├─ fd2_engine_game_over() → sapp_quit()   游戏线程返回时关窗
           └─ --autoquit-ms 到点 sapp_quit()
```

平台线程实现：Windows `host/platform_win32.c:392 CreateThread`，POSIX
`host/platform_posix.c:480 pthread_create`（统一走 `plat_thread()`）。

## 2. 游戏线程（真正的启动）

```
host/fd2_kernel.c:320   static void game_thread(void *arg)
 └─ src/game/boot.c:74   int fd2_game_main(int argc, char **argv)
      1. AIL bring-up        AIL_startup / AIL_install_MDI_INI / AIL_install_DIG_INI
                             AIL_allocate_sequence_handle / _allocate_sample_handle ×2
      2. 资源装载 RELOAD()   res_load(FDOTHER.DAT idx 31/1..6, FDTXT.DAT idx 0)
      3. 堆分配              malloc(0x20) / malloc(0x25680) / malloc(0xA00)
      4. VGA 13h             int386(0x10, AX=0x13)
      5. BIOS tick 快照      dword_53C0F = (int16)lowmem[0x46C]
      6. 随机表打乱          n = rand()%256;  sub_4EBE3(util_rand) × n
      7. 主状态机 for(;;)   ← 见 §3
      8. 退出                AIL_shutdown → int386(0x10, AX=3) 恢复文本模式 → return 0
                             game_thread 置 g_quit=1 → frame_cb 关窗
```

`boot.c` 的每一段都标了对应的原版地址（`0x25C01`、`0x25C63`、`0x25D66`、`0x25DB1` …），
是与 `FD2.EXE` 逐条对照过的。

## 3. 主状态机（`src/game/boot.c` 第 7 步）

```
for (;;) {                                    /* 外层 @0x25DB1 */
    ++fd2_game_progress;                      /* 心跳，见 §4 */

    sub_25977(0x12, 0)          = bgm_play(18)            标题 BGM

    v = sub_25EBB()             = scenario_menu (scene_core.c:57)
    │   ├─ choice = sub_1F894() = title_screen_run (title_screen.c:100)  ★标题画面
    │   │      FDOTHER 74/76/77 资源、logo 滚动动画、选项带
    │   ├─ choice == 0  新游戏   pal_fade_out → dword_53C03=0
    │   │                       res_load(FDOTHER,0) 调色板
    │   │                       funcs_25E3A[0]() 进入 state 0
    │   │                       bgm_play(byte_51E63[0]) → kbd_flush → return 0
    │   ├─ choice == 1  读档     res_load(FDOTHER,13) → pal_fade_out → 清屏
    │   │                       fopen("FD2.SAV","rb") 读 0x59CB(22987) 字节
    │   │                       sub_4DF28 = util_deobfuscate 解密
    │   │                       sub_29BCB = save_slot_select 选槽
    │   │                       memmove 派对表 0xA00 + 头 → dword_53C03 等
    │   │                       sub_26152 = scene_apply_state → funcs_25E3A[state]()
    │   └─ 其它           存档   bgm_play(-1) → sub_10010 = scenario_load → return 0
    │
    ├─ v == 0  → 内层 for(;;)  @0x25DCE
    │      r = sub_117E7()     = scene_event_tick (scene_core.c:148)   一拍事件
    │      │   key = sub_11AA8()  = ev4_11AA8   等按键
    │      │   分支: 切换演员记录 / sub_2000A / sub_12C0D / sub_25A96 SFX …
    │      │   r == 0  → 继续内层（游戏进行中）
    │      ├─ dword_53ECC == 1 → sub_22E5C = scene_card (转场卡)
    │      └─ dword_53ECC == 2 → 状态切换:
    │              funcs_25E23[dword_53C03]()   离开 (fd2_dispatch.c:53)
    │              sub_26152 = scene_apply_state
    │              成功 → funcs_25E3A[dword_53C03]() 进入 (fd2_dispatch.c:62)
    │                     bgm_play(byte_51E63[dword_53C03])
    │              失败 → quit = 1
    │              sub_4E381 = kbd_flush
    │      r == -1 → quit；r > 0 → 回外层
    ├─ v == -1 → 回外层（重启 BGM + 标题）
    └─ 其它    → quit = 1
}
```

- **两张分派表**：`funcs_25E23[30]`（离开）与 `funcs_25E3A[30]`（进入），
  定义在 `src/fd2_dispatch.c:53` / `:62`，由 `dword_53C03`（当前状态号，0..29）索引。
- `dword_53ECC` 是状态机的"请求"字节：1 = 转场卡，2 = 状态切换，由事件层写入。
- 别名总表：`src/include/fd2_aliases.h`、`src/include/fd2_restored_aliases.h`、
  `src/game/scene_core.h:43-45`。

## 4. 线程与共享数据

| 线程 | 位置 | 职责 |
|---|---|---|
| 主线程（sokol） | `host/main_sokol.c` | 窗口、D3D11 swapchain、键盘事件 → BDA 环形缓冲 |
| **游戏线程** | `fd2_kernel.c:320` | `fd2_game_main()` 全部游戏逻辑 |
| tick 线程 | `fd2_kernel.c:272` | 绝对截止期调度的 18.2 Hz，`lowmem[0x46C]++` 并唤醒等待者 |
| stat 线程 | `fd2_kernel.c:300` | 每 2 s 打印 fps / tick_rate |
| stall 线程 | `fd2_kernel.c:577` | 心跳看门狗（见下） |
| repeat / autokey / keyplay | `fd2_kernel.c` | 连发、注入、回放 |

共享内存（无锁或仅局部加锁）：

- `fd2_lowmem` — 原版低内存 / BDA，`0x46C` 是 BIOS tick 计数。
- `fd2_vga` — 0xA0000 VGA 显存 320×200，游戏线程写、`fd2_video_frame()` 读。
- `fd2_game_progress`（`src/game/boot.c`，`volatile unsigned`）— 主循环每轮自增，
  与 `fd2_video_publish_count`（`host/fd2_video.c:33`）一起作为
  `stall_thread` 的**双心跳**：区分"游戏卡死"和"游戏正阻塞在等按键"。
  主循环未起来时（资源装载阶段）看门狗给 30 s 宽限。

## 5. 与原版地址对照（速查）

| 本仓库 | 原版 `FD2.EXE` |
|---|---|
| `host/main_sokol.c:main` | 宿主新增（原版为 DOS/LE 入口 `start @0x3CCB4`） |
| `src/game/boot.c:fd2_game_main` | `main @0x25BF4`，状态机 `@0x25DB1` |
| `scenario_menu` | `0x25EBB` |
| `scene_event_tick` | `0x117E7` |
| `scene_apply_state` | `0x26152` |
| `bgm_play` | `0x25977` |
| `scene_card` | `0x22E5C` |
| `title_screen_run` | `0x1F894` |
| `ev4_11AA8`（等按键） | `0x11AA8` |
| `kbd_flush` | `0x4E381` |
| `util_rand` | `0x4EBE3` |
| `funcs_25E23[]` / `funcs_25E3A[]` | `0x25E23` / `0x25E3A` 分派表 |
