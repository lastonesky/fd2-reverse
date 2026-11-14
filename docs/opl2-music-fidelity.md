# FD2 音乐后端（AdLib / OPL2）保真度调试手册

> 最后更新：2026-11-14 · 分支 `modern-platform` · 所有实验代码在 `build/oplbench/`（未入库，gitignore）
>
> 本文档记录"自研 OPL2 渲染器 vs libADLMIDI 参考"这条调试链路的**方法、判据、工具、踩过的坑**，
> 目的是让下一次会话（或另一个人）不用从零重推。

---

## 0. 现状速览（先看这里）

| 曲目 | 包络相关 | 全曲 zcr（ours / ref） | rms 比 | 结论 |
|---|---|---|---|---|
| song01 | **1.000** | 1588 / 1587 | 1.00 | 对齐 |
| song03 | **1.000** | 2508 / 2519 | 1.00 | 对齐 |
| song05 | **1.000** | 2003 / 2000 | 1.00 | 对齐 |

- 参考 = `libADLMIDI`（Nuked-OPL3 核心 + 内嵌 `fatman-2op-ail-default.woplx` 音色），
  经 `render_wav` 的 song 路径渲染（与 `ref_s01.wav` 同一条路径）。
- **寄存器层硬证据**：song01 前 2482 条寄存器写入与 libADLMIDI 的 `ADL_WRITELOG` **逐条一致**
  （含双方的初始化/静音序列，差异只在日志尾部——ref 只录了前 3 秒）。
- 样本级仍有残差：song01 差 rms ≈ 357（信号 2284，约 15%）、song03 ≈ 861（3443，25%）、
  song05 ≈ 262（3044，8.6%）。其中约 10 个百分点来自 **ref 是立体声**（L/R 差 rms 240），
  我们是单声道；其余是小幅相位/音高微差，包络与亮度已无系统偏差。

> ⚠️ 一个重要的"当前设计决策"仍未定：`NUM_CH = 18`（双 emu8950），
> 对齐的是 **libADLMIDI**；原版 DOS `ADLIB.MDI` 是 **9 声道 + 偷声**。
> 要还原 DOS 原味应改回 9（见 §8）。

---

## 1. 目标与约束

- 用 **AdLib / OPL2 (YM3812)** 替换系统默认 MIDI，保证 macOS / Windows / Linux 听感一致。
- 贴近 DOS 原版：`FD2/` 只有 AdLib 系 MDI 驱动（`ADLIB.MDI` 14386 B，AIL 3.02），
  无 GUS / MPU-401 → 原版音乐 = **YM3812 9 声道 FM**。
- 许可宽松：核心选 **emu8950（MIT，单文件 1413 行，text 13.9 KB）**；
  音色用 **The Fat Man 2op AIL（MIT）** 转成 `opl2_patches.h`（128 旋律 + 53 鼓 ≈ 1.5 KB）。
- 体积：相对 `fd2engine` 的 673 KB text 段 **+15.6 KB（+2.3%）**，无 C++、无新动态库。

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
emu8950 × 2（chip0 = ch0..8，chip1 = ch9..17）
   ▼
× OUT_GAIN(-2.0 × 46340/65535 ≈ -1.4142) → int16 WAV @ 49716 Hz 单声道
```

关键文件（`build/oplbench/`）：

| 文件 | 作用 |
|---|---|
| `render_opl2.c` | 主渲染器（歌曲模式 + 单音调试模式 `-n`） |
| `emu8950/emu8950.c` | FM 核心；**所有 FD2 注记改动都在这里**（`grep -n "FD2 注记"`） |
| `emu8950.c.bak-gain` | 改动前快照，`diff` 它可一眼看到核心被改了哪 4 处 |
| `opl2_patches.h` / `opl2_drumkey.h` | `gen_patches.py` 从 `.woplx` 生成的寄存器镜像 |
| `ail_freq.h` | AIL 的 192 项 F-number 表（12 半音 × 16 分步） |
| `render_opl2_dbg.c` | 旧调试副本（`FD2_ONLYDRUM` / `FD2_NODRUM` 分离鼓/旋律） |

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

1. **9 声道 vs 18 声道**：`NUM_CH = 18` 是对齐 libADLMIDI 的选择；原版 DOS 是 9 声道 + 偷声。
   要"原味"需改回 9 **并实现 AIL 的偷声策略**（谁先被掐），否则会明显掉音。
   改回只需改 `#define NUM_CH 9`，但 18 声道对 song01 的包络相关是 1.000 vs 0.897，掉得很明显。
2. **样本级 15~25% 残差**：包络/zcr/电平已对齐，剩余是小幅相位与音高微差（单声道 vs 立体声之外）。
   目前的度量手段已分不出主观差异，需要真实听感反馈。
3. **DOS 原版录音基准缺失**：`FD2/` 下有 `dosbox-fd2.conf`，但还没有抓过 DOSBox 的真实录音。
   如果要以"原版"而非"libADLMIDI"为基准，最可靠的一步是**先录一份 DOSBox 的 song01**，
   再按 §4 的四层逐层对（L2 目标从 ADL 日志换成录音无法直接对寄存器，
   需要用"已知单音 + 录音"反推寄存器，成本会高很多）。
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
