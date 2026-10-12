# BRANCHING.md — 分支模型与"改动该放哪"的判定规则

> 目的：让"还原源码"永远只有一份权威版本，宿主/运行时的东西不污染它，同时不让两个分支
> 对同一份源码产生分歧。本文只讲**规则**；改动内容本身记在 `PROGRESS.md`。

## 1. 两条长期分支

| 分支 | 定位 | 内容 |
|---|---|---|
| **`main`** | **还原源码的权威版本**（source of record）+ 工具链 | `src/`（逐函数还原 + 去 guest 后的干净 C + 平台缝声明）、`tools/`、`analysis/`、文档。**不含 `host/`，不以原生运行与否为成败标准** |
| **`modern-platform`** | **原生引擎**：`main` + 宿主后端 + 构建 + 运行时策略 | `host/`（sokol/D3D11/WASAPI/ALSA/线程/看门狗…）、`build.ps1`、根 `Makefile`、以及所有"只服务现代宿主"的运行时改动 |

两者对 `src/` 中**共享的部分**必须一致；差异只允许是"`modern-platform` 多出来的宿主/运行时
钩子"。

## 2. 判定规则：一个改动该进哪条分支？

问一句：

> **如果把这份还原后的 C 放回原来的 32 位 DOS/4GW + 原始数据上跑，这个改动还需要吗？**

| 回答 | 类别 | 分支 |
|---|---|---|
| **不需要** | 宿主 / 平台 / 构建 / 运行时策略 | `modern-platform` |
| **需要，因为原来的 C 写错了** | 还原正确性修复（与宿主无关） | **`main`**（再合并到 `modern-platform`） |
| **需要，因为原程序依赖加载器 / 是 UB** | 数据模型 / 工具链正确性 | 通常 `main` |

## 3. 典型归属

**进 `main`（与宿主无关的源码正确性/可移植性）**
- 用真实指针存储（不许把指针塞进 `uint32_t`/`int`）；64 位安全的类型
- 全局量类型、结构体布局、函数边界等**还原错误**的修正
- 把"原程序读未初始化栈"的局部变量改成确定性初值
- 反映原意的逻辑 bug 修复
- 去 guest 化（C1–C4）
- 生成脚本与它们的产物（`gen_headers.py`、`gen_dispatch.py`、`src/include/*.h`…）
- 工具链层面的源码改名（如 `outp` → `fd2_outp`，因 MSVC 有内建 `outp`）——它不改变算法

**只进 `modern-platform`（只服务原生宿主）**
- `host/*`：sokol、D3D11、WASAPI/ALSA、渲染、音频、`fd2_kernel`/`fd2_video`/`fd2_platform`
- `build.ps1`，含 **`/DYNAMICBASE:NO`（关闭 ASLR）**——纯为崩溃定位时地址稳定
- `timeBeginPeriod`、条件变量/线程、`fd2_lowmem` 的 BDA 镜像、`int386`/`outp` 适配实现
- **运行时策略钩子**：`fd2_wait_tick` / `fd2_wait_tick_or_key` / `fd2_kbd_flush`、
  `plat_cond` / `plat_sleep_us`（把自旋换成阻塞、把按键等待换成条件变量）
- 高清放大 / 保持宽高比、看门狗行为

> 注：`modern-platform` 独有的"运行时钩子"会出现在 `src/game` 里（如 idle 环尾的
> `fd2_wait_tick_or_key()`）。这是**有意的 seam 调用**（和 `delay()`/`int386()` 同一类，
> 由宿主实现），但它改变了时序，所以**不回迁 `main`**。

## 4. 工作流

1. **与宿主无关的修正 → 先提交到 `main`**。
2. `git checkout modern-platform && git merge main`，让 `modern-platform` 拿到同一份源码。
3. **只服务原生的改动 → 只提交到 `modern-platform`**，永不回流 `main`。
4. 新增还原内容（新函数/新模块）也走第 1–2 步。

这样：`main` 始终是"正确、与宿主无关的还原 C"，`modern-platform` 是"它 + 原生宿主"，
两者对共享源码不产生分歧。

## 5. 现状锚点

- `main` 已包含：P0/P1 全量还原、去 guest C1–C4，以及本轮从 `modern-platform` 回迁的
  64 位指针/类型修正、未初始化局部变量确定性化、`outp`→`fd2_outp`、别名头与生成器更新。
- `modern-platform` 额外包含：`host/` 全量后端、`build.ps1`、阶段 1（条件变量时序/输入）、
  阶段 2（帧缓冲双缓冲）、高清呈现（方案 A）、看门狗，以及 `1af7307` 里的运行时钩子。
