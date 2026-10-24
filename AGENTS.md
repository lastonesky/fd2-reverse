# AGENTS.md — port3 工作约定（原生 FD2 引擎）

> 面向接手本仓库的 agent / 协作者。**先读本文件，再动代码和数据。**
> 背景、进度与机制细节看 `README.md`、`PROGRESS.md`、`docs/`。

---

## 0. 最高优先级：用户存档不可丢

**`E:\FD2\FD2.SAV` 是用户的真实存档，不是测试夹具。**
`FD2.SAV.bak-*`、`FD2.SAV.old-*`、`FD2.SAV.mistaken-*` 也都是用户数据，**不提交、不删除、不覆盖**。

### 0.1 任何运行引擎 / 回放按键 / 改存档之前

1. 先给当前存档做一份带时间戳的快照，并记录 md5：
   ```bash
   cd /e/FD2
   cp -p FD2.SAV FD2.SAV.agent-$(date +%Y%m%d-%H%M%S)
   md5sum FD2.SAV FD2.SAV.agent-*
   ```
2. 确认快照与当前文件一致：
   ```bash
   cd port3
   PYTHONIOENCODING=gbk python tools/fd2sav.py info
   ```
   （存档必须是 22,987 B；校验和 OK。）
3. **不要**用旧快照“回滚”：
   - 禁止 `cp /tmp/FD2.SAV.cur FD2.SAV` 这类操作；
   - 禁止拿昨天的备份当“干净基线”；
   - 恢复前必须先看 `mtime`、md5、`fd2sav info`，确认它确实是**本次测试前**的状态。

### 0.2 首选做法：隔离测试目录

不要直接在 `E:\FD2` 里跑会写存档的回放。推荐建一个临时数据目录：

```bash
mkdir -p /e/FD2/port3/testdata
cd /e/FD2/port3/testdata
# 数据文件用软链接/硬链接或直接复制，至少包括：
# FDTXT.DAT FDOTHER.DAT FDSHAP.DAT FDFIELD.DAT DATO.DAT FDMUS.DAT
# BG.DAT FDICON.B24 FIGANI.DAT FDOTHER.trad.DAT ...
cp -p /e/FD2/FD2.SAV ./FD2.SAV
# 在该目录启动引擎，读写的都是这份副本
/e/FD2/port3/build/fd2engine.exe --keyplay=... --log=...
```

测试结束后只清理 `testdata/`，**不要动 `E:\FD2\FD2.SAV`**。

### 0.3 如果不慎覆盖了存档

1. **立刻停止运行任何会写 `FD2.SAV` 的程序。**
2. 保留现场：把当前 `FD2.SAV` 另存为 `FD2.SAV.mistaken-<时间>`，不要再覆盖。
3. 按 `mtime` 列出所有 `FD2.SAV.bak-*`，用 `tools/fd2sav.py -f <file> info` 对比地点/场景/金钱/等级，确定最新可用状态。
4. 若 `tools/fd2sav_gui.py` 仍开着，它内存里可能还有最近一次写入的存档缓冲区；**只读**读取进程内存属于最后手段，必须校验 checksum 后才能写回。
5. 恢复顺序：最新备份 → 运行中 GUI 的内存缓冲区 → 更早备份。**每一步都先校验，再覆盖。**

> 2026-10-24 事故：agent 用 `/tmp/FD2.SAV.cur`（10/23 23:15 的旧快照）覆盖了用户
> 10/24 19:21 的存档。后来从仍开着的 `fd2sav_gui` 进程内存里找回了 19:21 写入的
> 缓冲区（与 19:21 备份差 13 字节、checksum 有效），才没有丢进度。
> **教训：测试基线必须来自“当前”存档；旧临时文件永远不能当回滚源。**

---

## 1. 仓库与数据边界

- 仓库是 `E:\FD2\port3`；游戏数据在 `E:\FD2`，**不在 git 里**。
- `FD2.EXE`、`FD2.SAV*`、`*.DAT`、`*.B24` 等是用户资产，**不要提交**。
- `src/fd2_image.c`、`analysis/seg_*.bin` 是构建生成物，按 `.gitignore` 处理。
- 不要 `git add -A` 把用户数据或临时回放日志带进提交。

---

## 2. 构建与运行

Windows（主用）：

```powershell
cd E:\FD2\port3
pwsh -File build.ps1          # -> build\fd2engine.exe
```

Linux / WSL：

```bash
wsl -- bash -lc 'cd /mnt/e/FD2/port3 && make engine'   # -> host/build/fd2engine
```

运行引擎时工作目录必须是数据目录（通常是 `E:\FD2` 或隔离的 `testdata/`）。
常用参数：

```
--log=FILE --keylog=FILE --keyplay=FILE --volume=N
```

回放 `c13log.txt` 这类按键日志前，先执行 §0 的存档快照。

---

## 3. 提交、推送、发布

- 主分支：`modern-platform`；推送前确认 `git status` 干净。
- 提交信息用 `fix:` / `docs:` / `chore:` 前缀，说明根因和影响。
- 发布前：
  1. 更新 `PROGRESS.md`（只写“发生了什么”，机制细节进 `docs/`）；
  2. 用 `tools/fd2_dialogue.py --no-pdf` 重新生成对白文档（若改了字形映射）；
  3. Windows/Linux 都重建；
  4. 打 annotated tag（如 `v0.9-playable`）并推送；
  5. `gh release create` 附 Linux 预构建和 sha256。
- **发布前确认没有把用户存档、回放日志、临时文件带进仓库。**

---

## 4. 文档纪律

- 代码/行为/参数改动 → 同一轮更新 `README.md` 或 `docs/`。
- 进度/计划/已完成项 → `PROGRESS.md`。
- 机制、根因、踩坑 → `docs/`（不要堆进 `PROGRESS.md`）。
- 字形映射改动：同时改 `tools/fd2data/_rebuild.py` 和 `tools/fd2data/glyphs.json`，再重新生成对白文档。
