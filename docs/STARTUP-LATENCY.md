# Windows 每天首次启动慢几秒的分析（startup-latency）

> 2026-10-29。现象：Windows 下每天**第一次**启动 `fd2engine.exe` 要等好几秒才出画面；
> 同一次开机内**第二次**启动则几乎瞬时出画面。本文记录本机实测数据、
> 已加的诊断埋点，以及下次开机后如何确诊。

## 1. 本机环境（与结论直接相关）

| 项 | 值 | 影响 |
|---|---|---|
| 数据盘 `E:` | **Btrfs（WinBtrfs 1.10.0 第三方驱动）**，ZHITAI TiPlus5000 2TB **NVMe** | exe 与全部 .DAT 都在第三方文件系统上；prefetch 对它无效 |
| 系统盘 `C:` / `D:` | NTFS，SATA/NVMe SSD | 系统 DLL 在这里 |
| 内存 | 64 GB | 第一次运行后，exe+DLL+数据全部驻留 standby list → 第二次启动瞬时 |
| SysMain（Superfetch） | **已停止且禁用**；WSearch 也禁用 | 开机后没有任何预取/预加载，第一次运行必然全冷 |
| Prefetcher 注册表 | EnablePrefetcher=3（内核 prefetch 仍在） | 只对 NTFS 上、有 .pf 的程序有效；btrfs 上的 exe 无 .pf |
| Windows Defender | 实时防护开启；`fd2engine.exe` 为本机自编译、未签名 | 进程创建时做镜像扫描；**每次 build.ps1 重建后哈希变化 → 必定重扫** |
| GPU | Intel iGPU（主显示器 ROG 4K）+ RTX 5060 Ti + Oray 虚拟显示驱动 | 开机后首个 D3D11 设备要冷启动 iGPU 驱动并枚举 3 个显示设备 |

## 2. 实测数据（2026-10-29，热态）

用 `testdata/` 隔离目录（未触碰 `E:\FD2\FD2.SAV`）：

- 进程启动 → Win32 窗口句柄出现：**80 ms**（进程总 CPU 203 ms）。
- 新埋点日志（见 §4）：`[+94 ms] render init done (D3D11 device + swapchain ready)`，
  `[+94 ms] first frame presented`，`[+234 ms] first game-produced frame published`。
  即热态下从 `main()` 到开机画面总共 **0.23 秒**。
- **磁盘 I/O 排除**：E: 虽是 Btrfs，但实测即使"冷"读也很快——本次开机从未读过的
  `testdata\FIGANI.DAT`（15 MB）首读 51 ms（284 MB/s）；`FD2.EXE.i64`（13 MB）
  733 MB/s。引擎在第一画面之前只读约 200 KB（boot 的 8 个 `res_load`）加上标题
  画面若干项（合计 < 1 MB），冷盘也就是几十毫秒级。

结论：**引擎自身（数据读取、解码、D3D 初始化在热态下）不可能造成"好几秒"**；
慢的部分全部是"每次开机后的第一次"才发生的一次性系统成本。

## 3. "第二次快"的机制

第一次运行把下列内容搬进了 64 GB 内存的 standby list / 缓存，且系统服务、驱动
都已进入工作状态：

1. `fd2engine.exe` + CRT/D3D/WASAPI 全部 DLL 的映像页；
2. `FDOTHER.DAT`/`FDTXT.DAT` 等数据文件的页与 Btrfs 元数据；
3. Defender 对这些文件的扫描结论（同一文件哈希不再重扫）;
4. iGPU D3D11 驱动、WASAPI 音频引擎等开机后首用才初始化的服务。

第二次启动时以上全部命中缓存，所以 0.2 秒级。每天第一次慢 = 夜间关机/重启把
以上清空（外加 SysMain 被禁用，没有任何东西替你预热）。

## 4. 已加的诊断埋点（本轮代码改动）

日志（`fd2engine.log`）现在带毫秒时间戳，`N` = 距 `fd2_engine_init()` 开始的毫秒数：

```
fd2: process start (main reached, wall 08:25:29)     <- 含墙钟，便于对照双击时刻
[+     0 ms] engine init done (WASAPI audio + AIL bring-up finished)
[+    15 ms] main: entering sapp_run (window + D3D11 device are created inside)
[+    94 ms] render init done (D3D11 device + swapchain ready)
[+    94 ms] threads started (game + 18.2 Hz tick + stat + stall)
[+    94 ms] game thread: fd2_game_main() entered (boot.c: AIL + resource loads follow)
[+    94 ms] first frame presented to the window
[+   234 ms] game: first game-produced frame published (boot screen on screen)
```

改动位置：`fd2_tlog()` 在 `host/fd2_kernel.c`（声明于 `host/fd2_kernel.h`）；
埋点在 `fd2_engine_init`/`fd2_engine_start`/`game_thread`（fd2_kernel.c）、
`main()`/`init_cb`/`frame_cb`（host/main_sokol.c）、`fd2_video_publish`
（host/fd2_video.c，第 2 次 publish = 游戏自己画的第一帧；第 1 次是 init 的空白帧）。

Windows（`build\fd2engine.exe`）与 Linux（`host/build/fd2engine`）均已重建。

## 5. 下次开机后如何确诊

明天开机后**第一次**启动游戏（或用 `--autoquit-ms=10000` 做一次无人值守启动），
然后看 `E:\FD2\fd2engine.log`。各 gap 对应的嫌疑：

| 观察到的 gap | 说明 | 嫌疑 |
|---|---|---|
| 双击/启动器时刻 → `process start (wall ...)` 行之间 | CreateProcess + CRT DLL 装载 + **Defender 镜像扫描** | Defender：未签名 exe 的镜像扫描/云端信誉查询；这是最可能吃掉几秒的地方，且**每次重建 exe 后首次启动必现** |
| `entering sapp_run` → `render init done` | D3D11 设备 + swapchain 创建 | 开机后首个 D3D 应用触发 iGPU 驱动冷启动、显示拓扑枚举（3 个显示器） |
| `game thread entered` → `first game-produced frame published` | 数据 fopen/fread + 解码 | Btrfs 冷元数据（已实测很快，可能性低） |
| 所有 gap 均匀变慢 | 系统整体被拖住 | 开机自启动风暴：OneDrive、Teams、Edge、向日葵(AweSun)、Everything、Rime |

## 6. 缓解措施（按性价比排序）

1. **给 Defender 加排除项**（需管理员）：目录 `E:\FD2`（含 `port3\build`）。
   ```
   Add-MpPreference -ExclusionPath 'E:\FD2'
   Add-MpPreference -ExclusionProcess 'fd2engine.exe'
   ```
   这消掉最大的"每次重建后首跑必慢"因素。
2. 接受现状：每天首跑慢一次是 Defender + 冷缓存的组合成本，属系统行为，引擎无 bug。
3. 若想进一步压掉冷缓存：把 `fd2engine.exe` 与数据放回 NTFS（如 `D:\FD2`），
   或重新启用 SysMain（对 btrfs 上的 exe/数据无效，但能预取 C: 上的系统 DLL）。
4. 复测可用隔离目录，勿动用户存档（AGENTS §0.2）：
   ```
   cd E:\FD2\port3\testdata
   E:\FD2\port3\build\fd2engine.exe --autoquit-ms=10000 --volume=0
   ```
   `testdata/probe_io.ps1`、`testdata/probe_run.ps1` 是本轮用的 I/O 与启动计时探针。

## 7. 明确排除的假设

- **数据文件冷读**：实测 E: 冷读 284–733 MB/s，第一画面前读取量 < 1 MB（§2）。
- **Btrfs 驱动慢**：WinBtrfs 1.10.0 顺序读接近 NVMe 原生速度；打开延迟 0–2 ms（暖态）。
- **引擎内部等待**：热态 0.23 s 出画面；引擎没有任何依赖"当天首次"的逻辑。
