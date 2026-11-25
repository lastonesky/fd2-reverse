# FD2 音乐后端调试手册（现行：SB Pro 2 / OPL3）

> 最后更新：2026-11-25 · 所有实验代码在 `build/oplbench/`（未入库，gitignore）
>
> **阅读顺序**：现行后端见 **§16**（Nuked OPL3 / YMF262，源码在 `host/opl3/`）。
> §0~§15 是历史上"自研 OPL2 渲染器 vs libADLMIDI 参考"的调试链路，其中的 `emu8950/`、
> `adl_backend.c`、`ADLMIDI=1`、`make engine OPL3=0`、`--adl-chips` 等**均已从仓库移除**，
> 相关命令与判据仅作方法学存档，不可再照抄执行。
>
> 本文档记录那条调试链路的**方法、判据、工具、踩过的坑**，目的是让下一次会话不必从零重推。

---

## 0. 现状速览（先看这里）

**2026-11-14 更新：声道分配 + 实时控制器修好之后，15 首全部 0.999~1.000，生产引擎
（`host/synth.c` + `host/xmidi.c`）与对拍镜像 `render_opl2.c` 已做到逐字节一致（14/15），
唯一例外 song08 是 1 个采样的相位差，原因见 §12.4。**

| 曲目 | 包络相关 | rms 比 | 引擎 vs 镜像 |
|---|---|---|---|
| song01 / 03 / 04 / 05 / 06 / 07 / 09 / 13 / 14 / 15 | **1.000** | 1.00 | 逐字节相同 |
| song02 / 10 / 11 / 12 | **0.999** | 1.00 | 逐字节相同 |
| song08 | **0.999** | 1.00 | 1 个采样相位差（§12.4） |

- 参考 = `libADLMIDI`（Nuked-OPL3 核心 + 内嵌 `fatman-2op-ail-default.woplx` 音色），
  经 `render_wav` 的 song 路径渲染（`r1..r15.wav`，49716 Hz 立体声）。
- **寄存器层硬证据**：song01 前 2482 条寄存器写入与 libADLMIDI 的 `ADL_WRITELOG` **逐条一致**
  （含双方的初始化/静音序列，差异只在日志尾部——ref 只录了前 3 秒）。
  song14 的帧级日志（`FD2_FRAMELOG`）在 4536 事件全程与镜像逐条相同，连每条写入的生效帧号也一致。
- **生产引擎同样验证过**：`build/oplbench/engine_check.c` 把 `host/synth.c` + `host/xmidi.c`
  原样编进一个离线 harness，按 FDMUS 资源号导出 WAV；资源 18（开头动画）输出与
  `render_opl2` 的 song14 逐字节相同（3,428,655 样本，0 差异）。
- 样本级仍有残差：song01 差 rms ≈ 357（信号 2284，约 15%）、song03 ≈ 861（3443，25%）、
  song05 ≈ 262（3044，8.6%）。其中约 10 个百分点来自 **ref 是立体声**（L/R 差 rms 240），
  我们是单声道；其余是小幅相位/音高微差，包络与亮度已无系统偏差。

> 📌 本节的结论属于**已移除的 OPL2 后端**。现行 OPL3 后端固定 18 声部
> （`SBPRO2.MDI`），增益 `-1.0`，见 §16；下表仅作为历史对拍记录保留。

---

## 1. 目标与约束

- 用软件 FM 合成替换系统默认 MIDI，保证 macOS / Windows / Linux 听感一致。
- 贴近 DOS 原版：`FD2/MDI.INI` 选中的是 **`SBPRO2.MDI`**（AIL 3.02 的 OPL3 驱动），
  DOSBox-X 配置是 `sbtype=sb16` + `oplmode=auto`。所以现行核心是 **Nuked OPL3**
  （18 个两段式声部），见 §16。
- 许可：核心为 **Nuked OPL3 v1.8（LGPL-2.1）**，源码与许可证在 `host/opl3/`；
  音色仍是 **The Fat Man 2op AIL（MIT）** 的 `opl2_patches.h`（128 旋律 + 53 鼓）。
- 本节的"emu8950 / OPL2 / +15.6 KB / 无动态库"是**旧后端的历史约束**，已不再适用。

---

## 2. 数据流与代码位置

```
FDMUS.DAT / *.xmi
   │  parse_events()            XMI 解码（delta 是 7-bit 字节求和；note 时长是标准 VLQ）
   ▼
note_on / prog / CC7 / CC11
   │  AIL 音量公式  mv = ((cc7*cc11*2)>>8 + 1) * AIL_VEL[vel>>3] * 2 >> 8 + 1
   │  音色：旋律 opl2_bank_melodic[prog]；鼓 opl2_bank_percussion[note-35] + opl2_drum_key[note-35] 固定音高
   ▼
opl_patch() / opl_note() / opl_off()
   │  opl_write()  → 写缓冲（WRITEBUF_SIZE 2048 / WRITEBUF_DELAY 2，模拟 Nuked 的 OPL3_WriteRegBuffered）
   ▼
Nuked OPL3 × 1（bank0 = ch0..8，bank1 = ch9..17，newmode=1）
   ▼
× OUT_GAIN(-1.0) → int16 WAV @ 49716 Hz 单声道
```

> 上面这一段是**现行 OPL3** 的口径；原有"emu8950 × 2 / OUT_GAIN -1.4142"的图已被替换。

关键文件（`build/oplbench/` 为历史实验目录；现行源码在 `host/`）：

| 文件 | 作用 |
|---|---|
| `host/opl3/nukedopl3.c/.h` | 现行 FM 核心（Nuked OPL3 v1.8） |
| `host/opl3/opl3_emu.c/.h` | 双 bank 句柄适配器（OPL_new/writeReg/calc） |
| `host/opl2_patches.h` / `opl2_drumkey.h` | Fat Man/AIL 两段式寄存器镜像（现行仍用） |
| `host/ail_freq.h` | AIL 的 192 项 F-number 表（12 半音 × 16 分步） |
| `render_opl2.c`（历史） | 旧 OPL2 对拍镜像，含歌曲模式与单音模式 `-n` |

---

## 3. 构建（无 cmake，全手工 gcc）

```bash
cd build/oplbench

# 主渲染器
gcc -O2 -Iemu8950 -o render_opl2 render_opl2.c emu8950/emu8950.c emu8950/emuadpcm.c -lm

# 双核同流对比（emu8950 + 本地 Nuked opl3.c）
gcc -O2 -Iemu8950 -o feed_regs feed_regs.c emu8950/emu8950.c emu8950/emuadpcm.c opl3.c -lm

# 同上但可切 newm=1（OPL3 模式）
gcc -O2 -Iemu8950 -o feed_regs3 feed_regs3.c emu8950/emu8950.c emu8950/emuadpcm.c opl3.c -lm

# 只喂 libADLMIDI 自带的 Nuked（排除两份源码不一致）
gcc -O2 -I libADLMIDI/src/chips/nuked -o feed_lm feed_lm.c \
    libADLMIDI/src/chips/nuked/nukedopl3.c -lm

# 参考渲染器（libADLMIDI 全库，需先有 libADLMIDI/*.o）
g++ -O2 -I libADLMIDI/include -o render_wav render_wav.cpp libADLMIDI/*.o -lm

# 单音参考（realtime 路径，⚠️ 其输出与自身日志不符，仅作粗参考，见 §7.5）
g++ -O2 -I libADLMIDI/include -o rt_note rt_note.cpp libADLMIDI/*.o -lm
```

渲染：

```bash
./render_opl2 xdir/song01.xmi song01_emu8950.wav          # 歌曲模式（xdir/ 是带 XDIR 包装的 xmi）
./render_opl2 x out.wav -n 66 60 100 1000                 # 单音：prog 66 / note 60 / vel 100 / 1000 ms
./render_wav  xdir/song01.xmi 0 ref_s01.wav 0 0 60        # 参考：xmi bank out songIdx emu 最大秒数
```

> 裸 `songs/*.xmi` 是 `FORM XMID`，libADLMIDI 的解析器只吃 `FORM ... XDIR`，
> 所以参考渲染一律用 `xdir/`；自己的 `render_opl2` 两种都能读。

---

## 4. 调试方法论：四层定位，不跨层下结论

这是整条链路里最值钱的部分。任何"音色不对/节奏不对"的模糊反馈，都按下面四层逐层证伪：

| 层 | 判据 | 工具 | 一致意味着 |
|---|---|---|---|
| **L1 事件层** | (tick, ch, note, dur, vel) 逐条相同 | `cmp_notes.py` | 乐谱解析、时序没问题 |
| **L2 寄存器层** | 寄存器写序列逐条相同 | `FD2_WRITELOG` vs `ADL_WRITELOG` + `diff` | 音色映射、音量公式、偷声策略没问题 |
| **L3 核层** | 同一寄存器流喂两核，波形/频谱一致 | `feed_regs` | 差异只在核实现，不在上层 |
| **L4 整曲层** | 包络相关、zcr、rms、分段稳定 | `cmp_song.py` | 整体听感指标对齐 |

**核心原则**：上一层的结论只能由上一层的证据支撑。
本轮就有两个反例——"声道不够"（L2 的推测）和"rt_note 参考"（L4 的输入）都曾把人带偏。

### 4.1 L1 事件层

```bash
./xmi2mid xdir/song01.xmi > s01_adl.mid
/home/ted/.workbuddy/binaries/python/versions/3.13.12/bin/python3 cmp_notes.py s01_adl.mid xdir/song01.xmi
```

输出逐条对比表，末尾会打印"共 N 个 note-on 全部一致"。
song01 = 1337、song03 = 1393、song05 = 2846，全部一致。

### 4.2 L2 寄存器层（最有杀伤力）

```bash
# 我方
FD2_WRITELOG=1 ./render_opl2 xdir/song01.xmi /dev/null 2>o_s01.log >/dev/null
# 参考
ADL_WRITELOG=1 ./render_wav xdir/song01.xmi 0 /dev/null 0 0 3 2>adl_s01.log >/dev/null

grep "^W " o_s01.log | awk '{print $2,$3}' > a_o.txt
grep -E "^(W|WI) " adl_s01.log | sed 's/^WI /W /' | awk '{print $2,$3}' > a_a.txt
diff a_o.txt a_a.txt | head
```

注意：
- 我方日志里还有 `K ...`（key-on 标记）行，`awk` 取列前必须 `grep "^W "` 过滤，否则出现 `0 0` 假差异。
- libADLMIDI 的日志前缀是 `W` 和 `WI`（`WI` = 波形选择），统一成 `W` 再比。
- 参考只录前 3 秒时，diff 尾部会有 `>` 行，那是 ref 更长，不是差异。

**当前结果：前 2482 条完全一致。**

### 4.3 L3 核层（隔离"核 vs 上层"）

把同一份寄存器流同时喂 emu8950 和本地 Nuked，看两个核的分歧有多大：

```bash
grep "^W " r66.log > r66c.log
./feed_regs r66e.wav r66n.wav 1.0 < r66c.log
python3 cmp_auto.py r66e.wav r66n.wav 261.63   # 谱距离
python3 zcr.py r66e.wav r66n.wav               # 亮度
```

- `feed_regs` 的参数：`out_emu.wav out_nuked.wav 秒数 < 寄存器日志`
- 若两核谱距离很小而 ours vs ref 很大 → 问题在**上层**（映射/音量/时序），不在核。
- 反之若两核就差很多 → 核实现差异（KSL/包络/LFO），去改 `emu8950.c`。

### 4.4 L4 整曲层

```bash
python3 cmp_song.py song01_emu8950.wav ref_s01.wav
```

输出包络相关（±2 s 内做 lag 搜索）、全曲与每 5 s 的 zcr / rms 比。
判读标准：
- 包络相关 < 0.9 → 节奏/发声时长层面还有问题，回到 L1/L2。
- 包络相关高但 zcr 差 > 10% → 音色亮度问题（PM 深度、KSL、波形选择）。
- zcr 对但 rms 比偏离 1.0 → 增益标定（`OUT_GAIN`）。

---

## 5. 工具速查表

| 工具 | 用法 | 说明 / 坑 |
|---|---|---|
| `render_opl2` | `<xmi> <out.wav> [song]`；单音 `x <out.wav> -n <prog> <note> <vel> [ms]` | 单音模式不跑 `opl_reference_init()`，只有 18 条写入，**不能**用来和 ADL 的 1000+ 条日志比 |
| `render_wav` | `<xmi> <bank> <out.wav> [song] [emu] [秒]` | 唯一可信的整曲参考路径（走 `adl_play`，int16 样本计数，frames = got/2） |
| `rt_note` | `<prog> <note> <vel> <out.wav> [ms] [rate] [ch]` | ⚠️ realtime 路径输出与自身寄存器日志不符，**不要当基准** |
| `cmp_notes.py` | `<mid> <xmi>` | 需要先 `./xmi2mid xdir/x.xmi > x.mid` |
| `cmp_song.py` | `<ours.wav> <ref.wav>` | 包络相关 + 分段 zcr/rms |
| `cmp_auto.py` | `<ours.wav> <ref.wav> [f0_hz]` | 谐波谱距离；**强烈建议显式给 f0** |
| `sweep_pm.py` | `<wav> [-v]` | 早期版本，f0 固定值，已被 `cmp_auto.py` 取代 |
| `zcr.py` | `<wav>...` | 过零率（/s），对"亮不亮"敏感、对"音头"不敏感 |
| `feed_regs` / `feed_regs3` / `feed_lm` | 见 §4.3 | `feed_regs3` 支持 `FD2_NEWM=1`、`FD2_IMMEDIATE`、`FD2_STATELOG` |
| `gen_patches.py` | `<woplx> <out.h>` | 从 `libADLMIDI/fm_banks_new/ail/fatman-2op-ail-default.woplx` 生成音色表 |

### 调试环境变量

| 变量 | 作用域 | 含义 |
|---|---|---|
| `FD2_WRITELOG=1` | render_opl2 | 输出 `W reg val`（stderr） |
| `FD2_FRAMELOG=1` | render_opl2 | 输出 `N 采样号 reg val`，看写入延迟 |
| `FD2_PM=<pct>` | emu8950 | 相位调制深度百分比（默认 100） |
| `FD2_FB=<pct>` | emu8950 | 调制器反馈深度百分比（默认 100） |
| `FD2_NEWM=1` | feed_regs3 / feed_lm | Nuked 跑 OPL3 模式（`0x105=01`） |
| `ADL_WRITELOG=1` | render_wav / rt_note | libADLMIDI 打印寄存器写入 |
| `ADL_EMU=<n>` | rt_note2 | 切换 libADLMIDI 的模拟器（0=Nuked, 2=MAME, 6=DOSBox…） |
| `FD2_ONLYDRUM` / `FD2_NODRUM` | render_opl2_dbg | 只渲鼓 / 只渲旋律 |

---

## 6. 已定位的根因（按发现顺序，每条都有硬证据）

| # | 症状 | 根因 | 修复位置 |
|---|---|---|---|
| R1 | 所有 WF≠0 的音色都像正弦，鼓"沙沙" | 未写 `reg 0x01 = 0x20`（waveform select enable）。emu8950 的 `0xE0` 写入被 `reg[0x01] & 0x20` 门控，静默丢弃 | `render_opl2.c` main / render_single 各写一次 |
| R2 | 鼓"沙沙"糊住旋律 | 曾按"硬件节奏模式"实现（`0xBD=0xE0` + 6 声道），与 libADLMIDI 完全不同 | 改回：鼓 = 普通 FM 声道 + `opl2_drum_key[]` 固定音高；`rhythm_*()` 保留但已弃用不再调用 |
| R3 | 音色"差一点"、不够"悠扬" | 缺 `0xBD=0xC0`（深 AM 震音 + 深 VIB 颤音）。libADLMIDI 会写，我们曾刻意不写 | 两处 `opl_write(0xBD, 0xC0)` |
| R4 | 音色偏暗（zcr 879 vs 1318） | PM 深度不足 | **结论后来被推翻**：真正原因是 KSL 定点截断（R7）；`pm_scale` 现在默认 100 |
| R5 | 整曲电平只有参考一半 | `lookup_exp_table` 从 `<<1` 改 `<<2` 后 `OUT_GAIN` 没跟着改 | 恢复 `<<1`；幅度与调制深度**分离**：输出用 `OUT_GAIN = -1.4142`，调制走 `pm_scale/fb_scale` |
| R6 | —— | 曾把 `<<2` 当作"对齐 Nuked 2:1 幅度"的手段，结果 PM 深度也翻倍，高频过量、f0 消失 | 同 R5 |
| R7 | 高次谐波偏强 3~8 dB，音色"发毛" | `makeTllTable()` 用 `int32_t tmp` + `tmp >> (3-kx)`，浮点 dB 转定点时截断，KSL 档位量化误差 | 改 `double tmp` + `tmp / (1 << (3-kx)) / EG_STEP`；**改完 PM/FB 回归 100%** |
| R8 | 震音深度不对（12.3 vs 15.9） | `lfo_am` 用 `am_table` 查表且把深浅两档搞反（`>>0 / >>2`） | 改成 210 步三角波：`triangle >> (am_mode ? 2 : 4)`（深档 /16→/4） |
| R9 | 长音会一路衰减到没声 | 包络在 DECAY 阶段到达 SL 后仍继续推进 | 加 `at_sustain_level` 判断（`eg_out>>4 == SL && SL != 15`）时冻结 |
| R10 | "最大并发 19，9 声道不够" | 解析脚本误用 running status（XMI 每个事件都显式写 status） | 用 `cmp_notes.read_xmi` 重算；**但 18 声道仍然是对的**（见 R11） |
| R11 | 改 9↔18 声道"渲染逐字节相同" | 那次对比用了陈旧二进制 / `/tmp` 不持久 | 重测：9 声道 vs 18 声道差 rms 1584；对参考包络相关 9ch=0.897、18ch=**1.000** |
| R12 | 单音对比"四个音色全部大幅偏差（220~512）" | 参考来自 `rt_note` 的 realtime 路径，其音频含 130 Hz 子谐波族，与自身寄存器日志喂 Nuked 的结果都不符 | 全部作废，改用 `render_wav` song 路径 + 最小 SMF |

### 另外两个"数据侧"的坑（不是引擎的问题）

- **xmi2mid 不缩放音符时值**：它把 delta ×3（SMF div 127、tempo 0x0562ad），但 note-on 自带的
  duration 不乘 3。所以用 `xmi2mid` 产物做参考时，音长只有真实值的 1/3（1.0 s vs 2.0 s）。
  对比时间窗口要限制在对齐的前 300 ms。
- **ADL 有 ~31 ms 启动延迟**（约 1544 样本，来自初始化写入的 writebuf 队列）。
  按音头对齐（`onset()`）或做 lag 搜索，别直接按文件开头切窗，否则所有鼓的统计全歪。

---

## 7. 度量陷阱（血泪清单）

1. **f0 必须显式给**。`cmp_auto.py` 的自动检测取"最大峰"，遇到基波弱/缺失的 FM 音色会锁到
   2 次或 3 次谐波（note 60 曾被识成 521.9 Hz），H1..H13 全落在谐波间隙里，谱距离就是噪声。
   ```bash
   # note 60 → 261.63 Hz；通式 440 * 2^((note-69)/12)
   python3 cmp_auto.py ours.wav ref.wav 261.63
   ```
2. **整体电平倾斜会淹没谱形差异**。prog66 原始谱距离 37.6，但 12 个谐波是均匀的 −3.13 dB——
   去掉这个常数倾斜后残差只有 **2.0**，形状其实一致。判"音色"前先 descatter。
3. **zcr 对某些音色完全不敏感**。prog0/prog66 这类基波主导的音色，PM 从 60% 扫到 250%
   zcr 都是 1760/s。
4. **rms≈0 的短促 blip 不能算谱距离**。HH（note 42）是幅度极小的非负脉冲，
   "谱距离 283" 纯属噪声，别再追。
5. **`rt_note` 不是基准**（R12）。它和 `render_wav` 的差异不是参数问题，是路径本身的问题。
6. **`cmp_song.py` 的包络相关对 lag 敏感**，一定要让脚本自己搜（默认 ±2 s，20 ms 步进）。
   手动给 lag 会得到一堆假低分。
7. **立体声 vs 单声道**。ref 是 stereo（L/R 差 rms ≈ 240），我们单声道，
   样本级残差里有约 10 个百分点来自这里，不是音色问题。
8. **XMI 的 delta 与 duration 编码不同**：delta = 多个 7-bit 字节**求和**（遇 bit7=1 停），
   note duration / meta length = 标准 SMF **移位** VLQ。搞反会出现"事件数腰斩、时长减半"。
9. **手工构造的 XMI 会让 xmi2mid 崩**（`src_ptr + 1 < src_end` 断言）。要复刻真实结构：
   `INFO = 01 00` 计数 + `TIMB` chunk + 时间签名 meta + `FF 2F 00` + 零填充。
10. **沙箱里不能用 process substitution** `<(cmd)` 和 `/dev/fd`。先重定向到临时文件再喂。
11. **`/tmp` 在两次命令之间不保证持久**。所有中间产物写在项目目录（如 `build/oplbench/`）。

---

## 8. 当前状态与待决问题

### 8.1 各曲最大复音数（`cmp_notes.read_xmi` 统计）

| 曲目 | note-on | 最大并发 | 曲目 | note-on | 最大并发 |
|---|---|---|---|---|---|
| song01 | 1337 | 11 | song09 | 1591 | 14 |
| song02 | 1422 | 20 | song10 | 588 | 8 |
| song03 | 1393 | 16 | song11 | 877 | 14 |
| song04 | 2234 | 13 | song12 | 91 | 9 |
| song05 | 2846 | 18 | song13 | 56 | 10 |
| song06 | 979 | 17 | song14 | 2256 | 25 |
| song07 | 513 | 10 | song15 | 329 | 21 |
| song08 | 572 | 18 | | | |

**只有 song10 / song12（并发 ≤9）在 9 声道下不需要偷声**，其余 13 首都会偷。
原版 AIL 的偷声策略（谁被掐、掐的顺序）目前**没有实现**，我们用的是
"空位 → 同通道已结束 → 最旧"的简化策略。

### 8.2 单音保真度（`cmp_auto.py`，显式 f0 = 261.63）

| prog | 谱距离（原始） | 去整体倾斜后 |
|---|---|---|
| 57 | 2.9 | 3.2 |
| 50 | 13.5 | 10.3 |
| 66 | 37.6（−3.13 dB 均匀倾斜） | **2.0** |
| 97 | 60.8 | ——（近正弦音色，谐波全在 −44 dB 以下，指标无意义） |

### 8.3 待决 / 未解

1. **9 声道 vs 18 声道（历史待决，现已关闭）**：旧 OPL2 后端曾纠结 9（ADLIB.MDI）还是
   18（对齐 libADLMIDI）。查清 `MDI.INI` 选中 `SBPRO2.MDI` 后结论明确：**18 声部**才是
   原版路径；现行 OPL3 后端固定 18，`--opl-channels` 只用于诊断。见 §16。
2. **样本级 15~25% 残差**：包络/zcr/电平已对齐，剩余是小幅相位与音高微差（单声道 vs 立体声之外）。
   目前的度量手段已分不出主观差异，需要真实听感反馈。
3. **DOS 原版录音基准缺失**：`FD2/` 下有 `dosbox-fd2.conf`，但还没有抓过 DOSBox 的真实录音。
   现行 OPL3 后端的电平与偷声统计已验证（§16.3），**听感 A/B 仍待用户确认**。
   若要继续做逐样本对拍，最可靠的一步是先录一份 DOSBox 的 song01，
   再按 §4 的方法逐层对（从录音反推寄存器成本很高）。
4. **AIL 精确偷声策略**未实现（见 8.1）。
5. **`rhythm_*()` 死代码**：`render_opl2.c` 里还留着硬件节奏模式的实现（`rhythm_setup/patch/hit`），
   已无调用点，但 `grep 0xBD` 时会看到 `0xBD=0xE0` 的两行，别被误导。

---

## 9. 回归检查清单（可直接复制）

```bash
cd build/oplbench
P=/home/ted/.workbuddy/binaries/python/versions/3.13.12/bin/python3

# 1) 重建
gcc -O2 -Iemu8950 -o render_opl2 render_opl2.c emu8950/emu8950.c emu8950/emuadpcm.c -lm

# 2) 渲染三首预览
for s in 01 03 05; do ./render_opl2 xdir/song$s.xmi song${s}_emu8950.wav; done

# 3) 对参考的包络/zcr/rms（期望：包络相关 ≥0.99，zcr 差 <1%，rms 比 1.00±0.03）
for s in 01 03 05; do echo "== song$s =="; $P cmp_song.py song${s}_emu8950.wav ref_s$s.wav | head -3; done

# 4) 寄存器层（期望：前 2482 条无差异）
FD2_WRITELOG=1 ./render_opl2 xdir/song01.xmi /dev/null 2>o_s01.log >/dev/null
ADL_WRITELOG=1 ./render_wav  xdir/song01.xmi 0 /dev/null 0 0 3 2>adl_s01.log >/dev/null
grep "^W " o_s01.log | head -2555 | awk '{print $2,$3}' > a_o.txt
grep -E "^(W|WI) " adl_s01.log | sed 's/^WI /W /' | awk '{print $2,$3}' > a_a.txt
diff a_o.txt a_a.txt | head

# 5) 事件层（期望：全部一致）
./xmi2mid xdir/song01.xmi > s01_adl.mid
$P cmp_notes.py s01_adl.mid xdir/song01.xmi | tail -3
```

参考基线（2026-11-14）：

```
song01 包络相关=1.000  zcr 1588/1587  rms 1.00
song03 包络相关=1.000  zcr 2508/2519  rms 1.00
song05 包络相关=1.000  zcr 2003/2000  rms 1.00
```

---

## 10. 核心改动的逐条 diff（`emu8950.c.bak-gain` → 当前）

四处，全部带 `FD2 注记` 注释：

1. `makeTllTable()`：`int32_t tmp` → `double tmp`；`(tmp >> (3-kx)) / EG_STEP` → `tmp / (1 << (3-kx)) / EG_STEP`
2. `update_ampm()`：`am_table[...] >> (am_mode ? 0 : 2)` → 210 步三角波 `>> (am_mode ? 2 : 4)`
3. `calc_envelope()`：DECAY 阶段加 `at_sustain_level` 冻结判断
4. `calc_slot_car()` / `calc_slot_mod()`：PM 输入过 `pm_scale()`、反馈过 `fb_scale()`
   （默认均 100%，即等价原版；保留 `FD2_PM` / `FD2_FB` 供离线扫描）

`lookup_exp_table()` 保持原版 `<< 1`（注释说明了为什么不能再改成 `<<2`）。

---

## 11. 相关文档

- `.workbuddy/memory/2026-11-13.md` — 后端选型（emu8950 vs 各核的体积/许可实测）、
  XMI 解码坑、音色表来源、算子映射事故
- `.workbuddy/memory/2026-11-14.md` — 本轮的逐日过程记录（含已被推翻的中间结论）
- `docs/STARTUP-LATENCY.md` — 首启黑屏（与音乐无关，但同为初始化时序问题）

---

## 12. 2026-11-14：开头动画 BGM 的根因与生产引擎移植

### 12.1 症状与突破口

"游戏开头动画的 BGM 和 libADLMIDI 不一致"。该曲 = FDMUS.DAT 资源 **track 18**
（`xdir/song14.xmi`，4536 事件 / 2256 note-on / 68.97 s，最大并发 25），
作为本轮验收用例。

### 12.2 根因（L2 层证据）

寄存器层对比发现两处**与音色直接相关**的上层差异（核 emu8950 本身无问题）：

1. **声道分配器不是 ADL 的那一套**。libADLMIDI 用
   `MIDIplay::calculateChipChannelGoodness`（默认 OffDelay）：空闲声道 `-koff_ms`、
   非活动但仍在 Release 的声道 `-koff_ms-40000`（同音色时特判为 `-koff_ms`，优先复用）、
   活动声道 `-4000000-kon_ms`，同音色 +300、`vibdelay<70 ms || kon>20 s` 再加 10、
   打击乐 +50、可疏散站 +4×n，同分取最小声道号；`AdlChannel::addAge` 每调度分段推进
   koff/kon/vibdelay。旧实现是"空位 → 同通道已结束 → 最旧"，在并发达 19~25 音的曲目里
   会频繁偷错声道 → 音色/乐句断口不对。
2. **实时控制器完全缺失**：CC7/CC11 变更时上游会 `noteUpdateAll(Upd_Volume)` 重写两个算子 TL；
   CC1 颤音每个调度分段重写 A0/B0（`vibspeed=2π*5`、`vibdepth=0.5/127`）；
   E0 弯音按 `bendsense=2` 半音写分数音高；CC64 踏板保持 note-off 不 key-off。
   旧实现只在 note-on 时算一次音量、音高只支持整数 tone。

### 12.3 修复位置

- `build/oplbench/render_opl2.c`（对拍镜像）：goodness/addAge/控制器/分数音高/on_ms 表。
- `host/synth.c`（生产引擎）：把上述全部逻辑移植过来（`voice_t` 增加 `kon_us/vibdelay_us/
  fixed_sustain/sustained/vel/tone/patch`；`ail_fnum(double tone,…)`；`channel_goodness()`；
  `voice_update_pitch/volume()`；`apply_event` 里的 CC1/CC7/CC11/CC64/E0）。
- `host/xmidi.c`：事件表改为 **xmi2mid 的时间线插入**语义（`insert after all tick<=target`），
  删掉"同 tick note-off 优先"的 `qsort`。实测 note-off 优先会让 song02 掉到 0.950、
  (tick,channel,priority) 排序让 song02 掉到 0.858。
- `host/opl2_patches.h` / `build/oplbench/opl2_patches.h`：新增
  `opl2_melodic_on_ms[128]` / `opl2_percussion_on_ms[53]`（DUR_K_ON，40000 = 固定延音）。

### 12.4 验证与已知残余

```powershell
# 镜像（基准）与生产引擎，同一个 49716 Hz 原生速率
cd build\oplbench
powershell -File build_win.ps1 ours          # render_opl2.exe
.\engine_check.exe <FDMUS.DAT> 18 e14.wav    # 生产引擎代码（host/synth.c + host/xmidi.c）
python cmp_song.py e14.wav v14.wav           # 期望 包络相关=1.000、逐 5 s zcr 相同
```

结果：15 首里 14 首**逐字节相同**；song08 例外，引擎输出整体比镜像早 1 个采样
（20.1 µs，不可听）。原因是镜像把每 tick 的时间算成 `(Δticks*3.0)*step`，
引擎算 `Δticks/tick_rate`；在 tick 30 恰好命中 `carry = 0.5` 的进位边界，二者相差 1 ulp，
一个向上取整、一个向下取整。libADLMIDI 实际用的是整数有理数
`(tempo_us*ticks)/ppqn`（见 `midiseq/impl/tempo_fraction.hpp`），取值与引擎一致，
所以镜像那一侧才是近似。

设备混音率（2026-11-15 更新）：已改为 `audio_init(44100)`（`host/fd2_kernel.c:713`），
AIL 侧仍按 11025 送数据（`ail_set_format(11025, 8, 0)`），SFX 由混音器重采样到设备率。
此前的 11025 设备会把 49716→11025 的线性插值抽取做成"丢高频 + 混叠"：参考里 >5.5 kHz
的内容（占总能量 0.45%）全部消失，同时 >5.5 kHz 折回使 3.2~5.5 kHz 比 44.1 kHz 渲染高
+2.4/+3.3 dB，谱质心 407 Hz（参考 440~448 Hz）——这是"整首听感略不同"的根因。
同一引擎改为 44100 后，20 Hz~16 kHz 与 libADLMIDI 参考差 ≤0.3 dB（16~22 kHz −0.6 dB）。
代价：单曲预渲染缓冲 1.5 MB → 6.1 MB（临时，随曲目切换释放），渲染墙钟 820 → 851 ms
（+4%；OPL 原生 tick 数不随输出率变化）。44100 下循环每遍 3,040,609 帧，偶有一遍
3,040,574（−35 帧 ≈ 0.8 ms，下一遍恢复，不累积）。

**循环模式的口径**：`bgm_play(track, loop_count)` 里 `0 = 循环`（BGM）、`1 = 一次性`
（过场/战败曲），开场动画用的是 0。渲染器在循环模式下会在末尾回绕一次，因此在日志里能看到
`notes 2267` 而解析层是 `2256 note-on`——多出的 11 个是回绕后重放的前几个音，
只影响最后 16 ms 的 dump 尾部。11 kHz 设备时期用 `FD2_LOOP=1 engine_check …`
（loop_count=0）复现，输出与当时游戏实际 dump 的 WAV 逐字节相同（1,520,718 B）；
设备率改为 44.1 kHz 后用 `FD2_RATE=44100 FD2_LOOP=1` 对比。

### 12.5 复现清单

```powershell
cd build\oplbench
powershell -NoProfile -ExecutionPolicy Bypass -File build_win.ps1 ours
$env:FD2_FRAMELOG="1"; .\render_opl2.exe .\songs\song14.xmi v14.wav *> framelog.txt
.\engine_check.exe g:\FD2\port3\testdata\FDMUS.DAT 18 g14.wav
# 逐字节：Compare-Object 或 [IO.File]::ReadAllBytes 对比 v14.wav / g14.wav

# 与游戏实际播放完全一致（循环模式 + 设备速率）
$env:FD2_RATE="11025"; $env:FD2_LOOP="1"
.\engine_check.exe g:\FD2\port3\testdata\FDMUS.DAT 18 g14_loop.wav
```

## 13. 2026-11-14：循环播放后"节奏变慢"（调度器丢帧）

### 13.1 症状

实际游戏里循环曲目播放到第二遍开始，整首曲子的速度就不对了（听感"整体变慢"，
也出现过变快）。一次性曲目正常；第一遍正常。`bgm_play(track, 0)` 的曲目全部受影响
（开场图鉴展示 = track 4，61.72 s；ending = track 18，68.95 s；城内/菜单曲同样是循环）。

### 13.2 根因

`host/synth.c` 的 `stream_fill()` 在把"事件时间轴"换算成采样帧时，用
`g_skip` 记住"本段被缓冲区边界截断、还没画出来的帧"，下次调用再补画。
旧实现对 `g_skip` 的记账有误：

```c
if (count > call) {            /* call = min(g_call_left, n - written) */
    g_skip = count - call;     /* ① 覆盖式赋值 */
    count = call;
}
...
if (g_call_skipped) {          /* ② 补画分支 */
    g_skip -= count;           /* ③ 又减一次 → 欠账被抹掉或下溢 */
}
```

两个后果：

1. **丢帧**：正常分段在"缓冲区剩余不足"处被切开时 `g_skip` 被赋值；下一次循环
   （`g_call_left != 0` 时）走正常分支，`g_skip = count - call` 直接**覆盖**掉尚未补画的欠账
   → 事件时间轴比音频推进得多，听感变快、每遍变短。
2. **重复补帧/下溢**：补画分支里 `count` 已被 ① 截断成 `call`，③ 再减一次，欠账小于
   `2*call` 时 `uint32_t` 下溢成 ~4.29e9，随后大量帧被当作"补画"渲染但事件不前进
   → 听感变慢、每遍变长。

libADLMIDI 的对应实现（`adlmidi.cpp::adl_playFormat`）用的是**有符号**的
`setup.tick_skip_samples_delay`，且欠账只按"实际画出的帧"扣减，出现同样边界时最多
把余量丢弃为负值（`hasSkipped = tick_skip > 0` 自然关掉），不会变成天文数字，也不会
在正常分支覆盖欠账。所以这是**本仓库移植时引入的缺陷**，不是与参考一致的既有行为。

### 13.3 为什么第一遍没事、循环后才出现

事件表每遍重新从 tick 0 开始，音频块固定为 4096 帧（`audio_sokol.c` 的 `MUS_FRAMES`），
`ADL_CHUNK = 2048`。干净地从 `written=0, g_call_left=0` 起步时，两个计数同步推进，
块边界恰好落在分块边界上，不会出现"欠账 + 正常分支"的组合；而**循环回绕发生在块中间**
（`written != 0`），回绕后两者的对齐被永久打破，于是每一块都会踩到上面的记账错误。
这就是"第一遍正常、第二遍开始不对"的原因，也解释了为什么 63 s 附近（track 4 的
61.72 s 回绕点 + 音乐启动延迟）开始出问题。

### 13.4 修复

`host/synth.c::stream_fill()`：

- 补画分支只在 `want = min(g_skip, 2048)` 之后截断 `count`，并且 `g_skip -= count`
  **只扣一次**（`count <= want <= g_skip`，不可能下溢）；
- 正常分支画完 `count` 帧后立刻 `g_skip = want - count` 记录欠账；
- 补画判定改为直接看 `g_skip > 0`（不再依赖 `g_call_left == 0` 那一刻的缓存标志
  `g_call_skipped`），使块边界任意切分都能正确续画。

一次性渲染（`render()` 里 `stream_fill(pcm, g_total)`，单个大请求）走的分支与旧代码
等价，所以**既有对拍结果不变**。

### 13.5 验证（修正前 / 修正后）

固定 11025 Hz（改动前的设备速率）、4096 帧切片（与 `audio_sokol.c` 一致），循环渲染 300 s：

| 用例 | 修正前每遍长度 | 修正后每遍长度 |
|---|---|---|
| track 18（应为 68.95 s / 760,152 帧） | 760152, 722600, 760152, 722600…（第二遍快 ~5%） | 760152 ×5 |
| track 18，2048 帧切片 | 760152, 689537, 966564, 856064, 856064…（快 9%，慢 27%，稳态慢 12.6%） | 760152 ×5 |
| track 4（应为 61.72 s / 680,452 帧） | 680452, 633263, 632203, 638976…（快 ~6%） | 680452 ×4 |
| track 18，49716 Hz 原生 | — | 3,427,822 ×3 |

失帧/欠账计数器在修正前的 track 18（2048 切片）第二遍记录到 `lost 70615` 帧，
与 760,152 − 689,537 = 70,615 完全对上；track 4（4096 切片）第二遍记录到
`lost 47189`。

修正前后在同一时间点的波形漂移（track 4，以修正后为基准）：

```
t = 62.00s  lag   0 frames (   0.0 ms)
t = 64.00s  lag +2214 frames (+200.8 ms)   ← 循环点 61.72 s 之后开始累积
t = 66.00s  lag +3664 frames (+332.3 ms)
t = 68.00s  lag +5834 frames (+529.2 ms)
t = 70.00s  lag +7566 frames (+686.3 ms)
```

修正后相邻两遍的残余对齐误差：track 18 @11025 = 0 帧，track 4 @11025 = 0 帧，
track 18 @49716 = 5 帧（0.1 ms，来自重采样相位未在回绕时清零，不影响节奏）。
逐样本不完全相同是正常的：回绕只做 `opl_panic()`（全部 key-off）+ 事件/计时状态清零，
不重置振荡器相位与 Release 尾音。

**对既有对齐结论的影响：无。** 同一构建下一次性渲染
`engine_check <FDMUS.DAT> 18 out.wav` 在 11025 Hz 与 49716 Hz 的 MD5 与修复前完全一致
（`08E6A5E5…` / `58514187…`），15 首逐字节/包络对拍结论不受影响。

### 13.6 长时播放 / 循环的同一机制说明

- 该缺陷由"**音频切片边界** vs **2048 帧分块**"的对齐决定，与曲子长度无关；但它只在
  对齐被打破后触发，而唯一的打破点是**循环回绕**（回绕落在切片中间）。所以：
  一次性长曲（如 track 8 = 124.9 s）第一遍不受影响；所有 `bgm_play(track, 0)` 的循环曲
  从第二遍起每遍时长都会漂移（实测 57.4 s ~ 96.7 s，正确值 61.7/69.0 s），且方向（快/慢）
  取决于回绕点落在切片内的位置。
- 曲目不会越播越偏：`g_skip` 等状态每遍回绕都清零，错误是**每遍重置后重新出现**的稳定
  模式（所以听感是"第二遍开始一直不对"，而不是"逐渐变慢"）。
- 若将来把 `MUS_FRAMES` 改成与 `ADL_CHUNK` 不成整数倍，或从别处以不同块大小拉流，
  不带循环的一次性曲目也会踩到；修复后对块大小没有这个隐含约束。

### 13.7 复现清单

```powershell
cd build\oplbench
# 一次性（回归：应与修复前 MD5 相同）
$env:FD2_RATE="11025"; .\engine_check.exe g:\FD2\port3\testdata\FDMUS.DAT 18 one.wav
# 循环 300 s，观察每遍长度（修复后应恒为 760152 / 680452）
$env:FD2_RATE="11025"; $env:FD2_LOOP="1"; $env:FD2_STREAM="300"; $env:FD2_LOOPLOG="1"
$env:FD2_BLOCK="4096"
.\engine_check.exe g:\FD2\port3\testdata\FDMUS.DAT 18 loop.wav
# 相邻两遍的残余对齐误差（应为 0 帧 lag）
python cmp_passes.py loop.wav.stream.wav 760152 4
```


---

## 14. 2026-11-22：9 声道（单 OPL2）与 AIL 偷声

### 14.1 先纠正一个容易搞混的事实

`libADLMIDI` 的 `adl_setNumChips(dev, 1)` **不是 9 声道**：`NUM_OF_CHANNELS = 23`
（18 个 2-op 通道 + 5 个节奏通道），`m_numChannels = m_numChips * 23`
（`src/adlmidi_opl3.cpp:1111`）。所以 `render_wav.cpp` 里的 `setNumChips(1)` 实际渲染出
**18 个 2-op 声道**，这就是 `r1..r15.wav` 的真实配置，也是为什么早期把 `NUM_CH` 定成 18
才对得上参考（`adl_setNumChips(1)` 且 `NUM_OF_CHANNELS=23`）。

结论：**参考基线本身不是 DOS 原味**。原版 `ADLIB.MDI`（AIL 3.02）是 **9 声道 + 偷声**。
要"原味"就必须离开参考基线，因此这一档**没有自动判据**，只能靠听感 + DOSBox 录音（§8.3-3）。

### 14.2 实现

`host/synth.c` 的声道数改成运行时：`synth_set_channels(9|18)`，命令行
`--opl-channels=9|18`（或 `FD2_OPL_CHANNELS`），默认 **9**。

- 两个 emu8950 仍然都实例化：18ch 的寄存器流与改动前逐字节相同（`FD2_WRITELOG` 对比
  16951 条写入、0 差异；PCM 与改动前 `cmp` 相同），9ch 时 chip1 只被初始化序列静音、永不 key-on。
- 初始化流按 `g_num_chips` 走：`ref_init_chip()` 只初始化 `g_num_ch` 个通道（与 ADL 的
  1-chip `ADL_WRITELOG` 前 72 行一致）；`ref_silence_all()` 的 18 通道扫描**保持原样**
  （ADL 自己的静音遍历也跨两个寄存器库，改它会破坏 18ch 的字节一致性）。
- 偷声：`find_voice()`/`channel_goodness()` 本来就是 ADL `calculateChipChannelGoodness`
  的转写，只是一直没被触发（18ch 基本不偷）。9ch 下补上 ADL 对**踏板保持音**的权重
  （`-5e5 - kon/2`，普通音是 `-4e6 - kon`），即"先拿踏板音开刀"。
  日志行新增 `stolen N` 统计，`synth: ... 9 voices, peak voices 9, notes 2256, stolen 567`。

### 14.3 实测（`build/oplbench/engine_check`，FD2_OPL_CHANNELS=9）

| 曲 | 最大同时发声 | 被偷音符 | 9ch vs 18ch 包络 | rms 比 |
|---|---|---|---|---|
| song01 | 7 | 0 | 0.892 | 1.03 |
| song02 | 9 | 282 | 0.809 | 1.03 |
| song03 | 9 | 34 | 0.970 | 0.99 |
| song04 | 9 | 18 | 0.944 | 0.99 |
| song05 | 9 | 638 | 0.830 | 0.96 |
| song06 | 9 | 81 | 0.916 | 0.97 |
| song07 | 9 | 150 | 0.946 | 0.98 |
| song08 | 9 | 112 | 0.854 | 0.99 |
| song09 | 9 | 0 | 0.972 | 1.01 |
| song10 | 8 | 0 | 0.981 | 0.99 |
| song11 | 9 | 12 | 0.830 | 1.01 |
| song12 | 8 | 0 | 0.954 | 0.94 |
| song13 | 9 | 0 | 0.986 | 1.00 |
| song14 | 9 | 567 | 0.838 | 0.85 |
| song15 | 9 | 45 | 0.683 | 0.85 |

- 即使 `stolen 0` 的曲子（song01/09/10/12/13）9ch 与 18ch 也不相同：18ch 会把音符分到两个
  芯片上，两颗 emu8950 的相位/直流各自独立，单/双芯片本来就听得出差别。
- 偷声最多的 song05(638/2846)、song14(567/2256)、song02(282/1422)：这三首在 9ch 下会明显
  掉音，是"原味"与"好听"冲突最直观的地方，A/B 时优先听它们。

### 14.4 复现

```bash
cd build/oplbench
cc -O2 -I../../host -I../../src -o engine_check engine_check.c ../../host/synth.c \
   ../../host/xmidi.c ../../host/emu8950/emu8950.c ../../host/emu8950/emuadpcm.c -lm
# 18ch 必须与改动前逐字节一致（回归）
FD2_OPL_CHANNELS=18 ./engine_check <FDMUS.DAT> 18 r18.wav
# 9ch 统计
FD2_OPL_CHANNELS=9  ./engine_check <FDMUS.DAT> 18 r9.wav
python3 cmp_song.py r9.wav r18.wav
```

---

## 15. 分支 `feature/libadlmidi-backend`：直接接 libADLMIDI（历史，已移除）

> ⚠️ 该后端及其 `host/adl_backend.c/.h`、`ADLMIDI=1` 构建开关、`--adl-chips` 参数
> 已从仓库删除。本节只记录当时的方法与坑（尤其是外层 XMI 长度的计算），
> 不要按本节内容再接一次。现行后端见 §16。

目的（当时）：让"参考音色"能直接在游戏里听，而不是只能听 `build/oplbench/r*.wav`。

### 15.1 接线方式

复用了 `host/xmidi.c` 已有的后端开关（原先 0 = Windows MIDI Mapper、1 = 内置 OPL2），
新增 **2 = libADLMIDI**：

```
FDMUS.DAT ──res_load──> XDIR/XMI blob
                          ├── g_backend==1 : xmidi.c 自己解析 EVNT -> synth.c(emu8950)
                          └── g_backend==2 : 原样交给 adl_openData() -> libADLMIDI
```

- `host/adl_backend.c/.h`：`adl_init(audio_rate())` → `adl_setNumChips` →
  `adl_setLoopEnabled` → `adl_switchEmulator(NUKED)` → `adl_setBank(0)`
  （bank 0 = `AIL (The Fat Man 2op set, default AIL)`，与 `opl2_patches.h` 同源）→
  `adl_openData(blob)`。
- 混音：ADLMIDI 输出交错立体声 int16，`adl_play()` **恰好渲染被请求的帧数**并按采样推进
  序列器，所以在音频回调里按 2048 帧分块拉流就是实时播放；每块折成单声道
  `(L+R)/2`、乘上 AIL 序列音量 × `--volume`（含 AIL 的 `ms` 渐变）后交给
  `host/audio.h` 的混音器。开始闸门（`audio_hold_music`）与 `synth.c` 同一套语义。
- 构建：`make engine ADLMIDI=1 [ADLMIDI_DIR=...]`；`--midi-backend=opl2` 在同一份二进制里
  切回 emu8950（对比用），`--adl-chips=N` 改仿真芯片数（默认 1）。

### 15.2 验证（离线，`FD2_RATE=49716`）

用同一份 FDMUS 资源（18 号 = 开头动画 = song14）走 `adl_backend` 导出，与
`build/oplbench/r14.wav`（`render_wav.cpp` 现渲染）对比：

- 直接调 `adl_openData`+`adl_play` 的对照程序（`/tmp/adlraw.c`）输出与 `r14.wav`
  **MD5 完全相同**（`9b79e60d…`）→ 说明库配置（bank/chips/emulator/块大小）与参考一致；
- 走 `xmidi_play()` 的端到端输出 → 包络相关 0.988 / rms 0.99（残差来自"参考是立体声、
  游戏是单声道"的 `(L+R)/2` 下混，与 1 LSB 取整），无时序偏移。

**调用的顺序有讲究**：先 `setBank` 后 `setNumChips` 时与参考只有 56% 采样相同；
改成 render_wav 的顺序（chips → loop → emulator → bank → open）后升到 75%（其余为下混），
raw 对照则完全一致。

### 15.3 许可与分发

libADLMIDI 是 **LGPL-2.1**，本仓库**不 vendor** 它的源码（`ADLMIDI_DIR` 指向本地 checkout
的 .o）。本分支只调用它的公开 C API，不改它的源码（它自己的 `AGENTS.md` 禁止 AI 生成的贡献）。
要正式发布这个后端，需要决定：动态链接 `.so`，或随包提供可重链接的 `.o`。

### 15.4 坑：libADLMIDI 只认外层 XMI 文件，不认裸的 `FORM XMID`

第一版接上后进游戏**没有 BGM**，日志是：

```
ail: init_sequence(seq=0) base=...4e0 song=...502 len=9850 head=FORM
adlmidi: openData(9850 bytes) failed: Unknown or unsupported file format
```

因为游戏侧的 `locate_xmid()`（FDMUS 的 IFF 树遍历）交给播放器的是最内层的
`FORM XMID`（`TIMB`/`EVNT` 容器），而 libADLMIDI 的加载器要的是**整份 XMI 文件**：

```
FORM 0000000e XDIRINFO 00000002 0100 CAT ...      <- ADL 认这个
  └─ FORM 002672 XMID TIMB ...                    <- ADL 不认识（= locate_xmid 的产物）
```

而且外层 `FORM XDIRINFO` 的长度字段只有 14（只覆盖它自己的 8+6 字节头），**不能**用它算
整份文件长度。可行算法：`total = (XMID 相对 start 的偏移) + 8 + XMID 自己的 big-endian 长度`，
对 FDMUS 全部 15 首验证过，结果恰好等于资源表里的 `end - start`（都是 XMID@0x22）。

实现：`host/fd2_audio.c` 的 `ail_seq` 新增 `raw`/`raw_len`，`AIL_start_sequence` 在
backend==2 时走新增的 `xmidi_play_raw()`。

另一处：AIL 的 `loop_count` 语义是 **0 = 无限循环**（`bgm_play(track, 0)` 全是循环曲，
`bgm_play(16/17, 1)` 是过场小曲），`synth_play()` 已经按 `count == 0` 映射；
ADLMIDI 分支第一版写反了（0 当成不循环），一曲播完就静音，已改为 `loop_count == 0`。

---

## 16. 2026-11-25：改用 SB Pro 2 的 OPL3（YMF262）核心

### 16.1 起因：原版选的是 SBPRO2.MDI，不是 ADLIB.MDI

`FD2/MDI.INI`（游戏数据，不在仓库里）写的是：

```
DEVICE      Creative Labs Sound Blaster(TM) 16
DRIVER      SBPRO2.MDI
IO_ADDR     220h
```

`AILDRVR.LST` 对 `[sbpro2.mdi]` 的说明："If your Sound Blaster Pro is a 'current'
model equipped with the Yamaha OPL-3 synthesizer ... you should select this option"。
`dosbox-fd2.conf` 又是 `sbtype=sb16` / `oplmode=auto` / `oplemu=default`。
所以用户记忆中的"真机味"来自 AIL 的 **OPL3 驱动**（18 个两段式声道 + newmode），
而此前两条路径（emu8950 ×2、libADLMIDI bank 0）都在不同程度上按 OPL2 建模。

ScummVM `audio/miles_adlib.cpp` 给出了该驱动的寄存器口径：

- `resetAdLib()`：`0x105=1`（newmode）、`0x104=0`（18×2op）、`0x01=0`、`0x08=0`、
  `0xBD=0xC0`；
- `setRegister()`：`reg & 0x100` 选 bank 1，其余写 bank 0；
- OPL3 声像在 `0xC0` 的 bit4/5，默认 pan `0x40`(center) → `0x30`（左右都开）。

FDMUS 的 EVNT 里统计到 CC7/CC1/CC64，但**没有 CC10**，所以所有声道居中，
`(L+R)/2` 折单声道不丢信息（这也是本轮没有改混音器为立体声的原因）。

### 16.2 实现

- 新增 `host/opl3/`：`nukedopl3.c/.h`（Nuked OPL3 v1.8，LGPL-2.1，连同
  `LICENSE.LGPL-2.1.txt` 入库）和 `opl3_emu.c/.h` 适配器。
- 适配器把**一颗** YMF262 伪装成 synth.c 期待的两颗"芯片"：`OPL_new()` 两次分别
  拿到 bank 0 / bank 1 句柄，`OPL_writeReg()` 把高位映射成 `0x100`，
  `OPL_calc()` 只在 bank 0 上生成一帧、把 L/R 折成 mono，bank 1 返回 0。
- synth.c 自己的 `0x04/0x05` 过滤使 `0x104/0x105` 永远写不进去（旧 OPL2 下这两条
  本来就是死写），所以适配器的 `OPL_reset()` 显式写 `0x105=1, 0x104=0, 0x008=0,
  0x0BD=0xC0`，把 OPL3 newmode 固定下来。
- 增益：直接用 Nuked 的原始输出，`OUT_GAIN = -1.0`（旧的 `-1.4142` 是给 emu8950
  补偿幅度用的；沿用会高 √2，实测 RMS 3418 vs 参考 2417）。
- 声部数固定 18（SBPRO2.MDI）；`--opl-channels` 仅用于诊断降声道。
  `--midi-backend=sbpro2|opl3` 是内置后端的别名。
- 构建：POSIX `make engine`、Windows `pwsh -File build.ps1`，两者都固定编入
  `opl3/nukedopl3.c` + `opl3/opl3_emu.c`；无 `OPL3=0` / `-NoOPL3` / `ADLMIDI=1`。

### 16.3 验证（离线，`FD2_RATE=49716`）

| 后端 | 声部 | 峰值复音 | 被偷 | RMS | 说明 |
|---|---:|---:|---:|---:|---|
| emu8950 | 9 | 9 | 567 | 2436 | 旧默认 |
| Nuked OPL3 | 9 | 9 | 567 | 2417 | 同一音序，仅核心不同 |
| Nuked OPL3 | 18 | 18 | 66 | 2417 | **新默认** |
| libADLMIDI `r14.wav` | 18 | — | — | 2417 | 参考（立体声折 mono） |

资源 18（开头动画）全程 3,428,655 样本，非静音、无跑飞；启动日志确认核心名。
OPL3/OPL2 逐样本不同（平均绝对差 6214/32768），说明切换确实生效。
**听感仍需在 DOSBox-X / 真机上 A/B**；本轮只做了电平、流程和偷声统计验证。

### 16.4 已知边界

- 仍是 2-op bank（Fat Man AIL），没有 4-op 乐器；SBPRO2.MDI 本身也以 2-op 为主。
- 仍是单声道混音器；FDMUS 无 CC10，居中声像下折 mono 无损。
- §0 / §14 的"emu8950 与 libADLMIDI 逐字节一致"是**旧后端**的历史结论，
  现行单一 OPL3 后端不再满足该口径；它仍可作为方法学参考。
- Windows 侧只改了 `build.ps1`，本机没有 MSVC，**未做实机构建验证**。
- emu8950、libADLMIDI 两个后端及其构建开关/参数/接线均已从仓库移除。
