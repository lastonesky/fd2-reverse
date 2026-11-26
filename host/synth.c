/*
 * OPL3 music synthesiser for the FD2 host port.
 *
 * MDI.INI in the original game data selects SBPRO2.MDI ("Creative Labs Sound
 * Blaster 16"), i.e. AIL's OPL3 driver, not ADLIB.MDI.  The note/volume/voice
 * logic below drives one Nuked YMF262 with OPL3 newmode on (18 two-op voices,
 * two register banks), matching the SB Pro 2 / SB16 path DOSBox-X emulates.
 *
 * The Fat Man / AIL two-operator bank is used.  The native 49716 Hz OPL output
 * is resampled to the host audio rate.  The buffered register queue and patch
 * caching intentionally follow the reference renders used to tune timing.
 *
 * Channel count is a runtime choice (synth_set_channels), default 18.
 */

#include "synth.h"
#include "platform.h"
#include "audio.h"
#include "opl3/opl3_emu.h"      /* one YMF262, two register banks (SB Pro 2) */
#include "ail_timbres.h"        /* SAMPLE.OPL: the game's real AIL timbre bank */
#include "opl2_patches.h"       /* fallback only when SAMPLE.OPL is unavailable */
#include "opl2_drumkey.h"
#include "ail_freq.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define OPL_NATIVE   49716u
#define OPL_CLOCK    3579545u
#define OUT_GAIN     (-1.0)
#define RSM_FRAC     10
#define MAX_CH       18
#define ADL_CHUNK    2048u

/* SBPRO2.MDI (OPL3) exposes 18 two-op voices across two register banks on one
 * YMF262.  g_num_chips is what the init/silence loops walk. */
static int g_num_ch    = 18;
static int g_num_chips = 2;

static OPL *g_chip[2];
static OPL *g_opl;

/* Two OPL2 chips share the register layout below: channels 0..8 use chip 0,
 * channels 9..17 use chip 1.  Within a chip, modulator offsets are 0/1/2,
 * 8/9/10, 16/17/18 and carrier offsets are +3. */
static const uint8_t OP_OFF[9] = { 0, 1, 2, 8, 9, 10, 16, 17, 18 };

/* AIL velocity curve: indexed by vel>>3, mapping velocity to 82..127. */
static const uint8_t AIL_VEL[16] = {
    82, 85, 88, 91, 94, 97, 100, 103,
    106, 109, 112, 115, 118, 121, 124, 127
};

typedef struct {
    int      used;          /* key-on 中 */
    int      ever_used;
    int      ins;
    int      midi_ch;
    int      note;
    uint32_t start_tick;
    uint32_t end_tick;
    uint8_t  b0;
    int64_t  koff_us;       /* note-off 后剩余的 Release 时间 */
    int64_t  kon_us;        /* DUR_K_ON 剩余时间 */
    int64_t  vibdelay_us;
    int      fixed_sustain; /* DUR_K_ON == 40000：上游用固定延音，不衰减 */
    int      sustained;     /* CC64 踏板保持中 */
    uint8_t  vel;
    double   tone;          /* 未加弯音/颤音的基础音高（鼓为固定音高） */
    const opl2_patch *patch;
    const ail_timbre *timbre; /* non-NULL when the AIL bank supplied the patch */
    uint16_t priority;      /* Miles currentPriority, 32767 on note-on */
} voice_t;

static voice_t          g_voice[MAX_CH];
static const opl2_patch *g_patch_cache[MAX_CH];
static int              g_prog[16];
static int              g_chvol[16];
static int              g_chexpr[16];
static int              g_master_vol = 127;

/* Per-MIDI-channel realtime controller state (libADLMIDI MIDIchannel). */
static double g_bend[16];
static double g_bendsense[16];
static int    g_vibrato[16];
static int    g_sustain[16];
static double g_vibpos[16];
#define VIB_SPEED (2.0 * 3.141592653589793 * 5.0)
#define VIB_DEPTH (0.5 / 127.0)

static uint64_t         g_notes;
static uint64_t         g_peak_voices;
static uint64_t         g_steals;        /* notes cut while still sounding */
static int              g_timbres_auto_tried;
static uint8_t          g_timb[256 * 2]; /* XMIDI TIMB: (patchId, bankId) pairs */
static int              g_timb_count;
static uint8_t          g_voice_protect[16]; /* Miles CC112 per MIDI channel */
static uint8_t          g_active_voices[16];
static uint8_t          g_pan[16];           /* CC10, 64 = centre */
static int              g_circular_phys = -1;

/* ---------------- Nuked-style buffered register queue ---------------- */

#define WRITEBUF_SIZE  2048
#define WRITEBUF_DELAY 2

typedef struct {
    uint64_t time;
    uint16_t reg;
    uint8_t  val;
    uint8_t  used;
} writebuf_entry;

static writebuf_entry g_writebuf[WRITEBUF_SIZE];
static uint32_t       g_writebuf_cur;
static uint32_t       g_writebuf_last;
static uint64_t       g_writebuf_samplecnt;
static uint64_t       g_writebuf_lasttime;
static int            g_write_buffered;

static OPL *chip_of(int ch)
{
    return g_chip[(unsigned)ch / 9 & 1];
}

static int g_writelog;          /* FD2_WRITELOG=1: dump every chip write */

static void opl_write_now(uint16_t reg, uint8_t val)
{
    unsigned low = reg & 0xff;
    if (low == 0x04 || low == 0x05)
        return;
    if (g_writelog)
        fprintf(stderr, "W %03X %02X\n", (unsigned)reg, (unsigned)val);
    if (reg & 0x100) {
        OPL_writeReg(g_chip[1], low, val);
    } else if (low < 0x10 || low == 0xBD) {
        OPL_writeReg(g_chip[0], low, val);
        OPL_writeReg(g_chip[1], low, val);
    } else {
        OPL_writeReg(g_chip[0], low, val);
    }
}

static void opl_tick(void)
{
    if (!g_write_buffered)
        return;
    while (g_writebuf[g_writebuf_cur].used &&
           g_writebuf[g_writebuf_cur].time <= g_writebuf_samplecnt) {
        writebuf_entry *w = &g_writebuf[g_writebuf_cur];
        opl_write_now(w->reg, w->val);
        w->used = 0;
        g_writebuf_cur = (g_writebuf_cur + 1) % WRITEBUF_SIZE;
    }
    ++g_writebuf_samplecnt;
}

static void opl_write(uint16_t reg, uint8_t val)
{
    uint16_t full_reg = (uint16_t)(reg + (g_opl == g_chip[1] ? 0x100 : 0));
    writebuf_entry *w;
    uint64_t time1, time2;

    if (!g_write_buffered) {
        opl_write_now(full_reg, val);
        return;
    }

    w = &g_writebuf[g_writebuf_last];
    if (w->used) {
        opl_write_now(w->reg, w->val);
        g_writebuf_cur = (g_writebuf_last + 1) % WRITEBUF_SIZE;
        g_writebuf_samplecnt = w->time;
    }
    w->reg = full_reg;
    w->val = val;
    w->used = 1;
    time1 = g_writebuf_lasttime + WRITEBUF_DELAY;
    time2 = g_writebuf_samplecnt;
    if (time1 < time2)
        time1 = time2;
    w->time = time1;
    g_writebuf_lasttime = time1;
    g_writebuf_last = (g_writebuf_last + 1) % WRITEBUF_SIZE;
}

static void opl_write_full(uint16_t reg, uint8_t val)
{
    g_opl = g_chip[(reg >> 8) & 1];
    opl_write(reg & (uint16_t)~0x100, val);
}

/* ---------------- libADLMIDI-equivalent reset sequence ---------------- */

static void ref_update_categories(int chips)
{
    for (int i = 0; i < chips; ++i) {
        opl_write_full(0x0BD, 0xC0);
        opl_write_full(0x104, 0x00);
    }
}

static void ref_silence_all(int chips)
{
    for (int i = 0; i < chips; ++i)
        opl_write_full(0x0BD, 0xC0);

    for (int card = 0; card < chips; ++card) {
        /* The 18-channel sweep is deliberate: ADLMIDI's own silence pass for a
         * given `chips` walks both register banks, and the 18ch output is
         * byte-validated against the reference renders. */
        for (int ch = 0; ch < 18; ++ch) {
            uint16_t base = (uint16_t)((ch / 9) << 8);
            uint16_t m = (uint16_t)(base + OP_OFF[ch % 9]);
            opl_write_full((uint16_t)(base + 0xB0 + ch % 9), 0x00);
            opl_write_full((uint16_t)(0x40 + m), 0x3F);
            opl_write_full((uint16_t)(0x80 + m), 0xFF);
            opl_write_full((uint16_t)(0x40 + m + 3), 0x3F);
            opl_write_full((uint16_t)(0x80 + m), 0xFF);
        }
        static const uint16_t tail[][2] = {
            {0x050, 0x090}, {0x053, 0x090}, {0x054, 0x107F},
            {0x052, 0x092}, {0x055, 0x107F}, {0x051, 0x091}
        };
        for (size_t i = 0; i < sizeof(tail) / sizeof(tail[0]); ++i) {
            opl_write_full(tail[i][0], 0x3F);
            opl_write_full(tail[i][1], 0xFF);
        }
    }
}

static void ref_init_chip(void)
{
    static const uint16_t init[] = {
        0x004, 0x60, 0x004, 0x80, 0x105, 0x00, 0x105, 0x01,
        0x105, 0x00, 0x001, 0x20, 0x105, 0x01, 0x008, 0x00
    };
    for (int ch = 0; ch < g_num_ch; ++ch) {
        uint16_t base = (uint16_t)((ch / 9) << 8);
        uint16_t m = (uint16_t)(base + OP_OFF[ch % 9]);
        opl_write_full((uint16_t)(0x20 + m), 0x00);
        opl_write_full((uint16_t)(0x20 + m + 3), 0x00);
        opl_write_full((uint16_t)(base + 0xA0 + ch % 9), 0x00);
        opl_write_full((uint16_t)(base + 0xB0 + ch % 9), 0x00);
    }
    for (size_t i = 0; i < sizeof(init) / sizeof(init[0]); i += 2)
        opl_write_full(init[i], (uint8_t)init[i + 1]);
}

static void ref_reset(int chips)
{
    for (int i = 0; i < chips; ++i)
        ref_init_chip();
    ref_update_categories(chips);
    ref_silence_all(chips);
}

static void opl_reference_init(void)
{
    ref_reset(g_num_chips);
    ref_update_categories(g_num_chips);

    OPL_reset(g_chip[0]);
    OPL_reset(g_chip[1]);
    memset(g_writebuf, 0, sizeof g_writebuf);
    g_writebuf_cur = g_writebuf_last = 0;
    g_writebuf_samplecnt = g_writebuf_lasttime = 0;
    g_opl = g_chip[0];

    ref_reset(1);
    for (int i = 0; i < 4; ++i) {
        ref_silence_all(1);
        if (i == 0) {
            OPL_reset(g_chip[0]);
            OPL_reset(g_chip[1]);
            memset(g_writebuf, 0, sizeof g_writebuf);
            g_writebuf_cur = g_writebuf_last = 0;
            g_writebuf_samplecnt = g_writebuf_lasttime = 0;
            g_opl = g_chip[0];
        }
        ref_reset(1);
    }
}

/* Voice count: 9 (one OPL2, DOS original) or 18 (two chips, libADLMIDI's
 * single-chip layout and therefore the mode the reference WAVs match).  Any
 * other value is rounded up to the next whole chip; must be called before
 * synth_play() - the allocator and the ADL-equivalent init stream size are
 * fixed per sequence. */
void synth_set_channels(int n)
{
    if (n < 9)
        n = 9;
    if (n > MAX_CH)
        n = MAX_CH;
    n = ((n + 8) / 9) * 9;
    g_num_ch    = n;
    g_num_chips = n / 9;
}

int synth_channels(void)
{
    return g_num_ch;
}

/* ---------------- AIL volume / pitch tables ---------------- */

static int ail_midi_volume(int velocity, int volume, int expression,
                           int master_volume)
{
    uint32_t mv = (uint32_t)(volume * expression) * 2u >> 8;
    uint32_t v;
    if (mv) ++mv;
    v = AIL_VEL[(velocity & 0x7F) >> 3];
    mv = mv * v * 2u >> 8;
    if (mv) ++mv;
    if (master_volume < 127)
        mv = mv * (uint32_t)master_volume / 127u;
    if (mv > 127) mv = 127;
    return (int)mv;
}

static int ail_scale_tl(int tl, int mv, int do_op)
{
    int val = (~tl) & 0x3F;
    if (do_op) val = val * mv / 127;
    return (~val) & 0x3F;
}

/* AIL tone->F-Number：查表 192 项（12 半音 × 16 分步），bit15 表示借一个八度。
 * 直接照 model_ail.c 的 oplModel_ailFreq 移植；tone 允许小数（弯音/颤音）。 */
static void ail_fnum(double tone, int *fnum, int *block)
{
    int32_t note, pitch, octave, octave_offset = 0;
    double bend_dec;
    int idx, halftone;
    uint16_t f;

    if (tone < 0.0) tone = 0.0;
    note = (int32_t)tone;
    bend_dec = tone - (double)(int)tone;
    if (bend_dec > 0.5) {
        note += 1;
        bend_dec -= 1.0;
    }
    pitch = (int32_t)(bend_dec * 4096.0) + 8192;
    pitch = ((pitch - 0x2000) / 0x20) * 2;

    note -= 12;
    while (note < 0)  { --octave_offset; note += 12; }
    while (note > 95) { ++octave_offset; note -= 12; }

    pitch += ((uint8_t)note << 8) + 8;
    pitch /= 16;

    while (pitch < 12 * 16)      pitch += 12 * 16;
    while (pitch > 96 * 16 - 1)  pitch -= 12 * 16;

    idx      = pitch >> 4;
    halftone = (idx % 12) * 16 + (pitch & 15);
    octave   = idx / 12;

    f = ail_freqtable[halftone];
    if (!(f & 0x8000)) {
        if (octave > 0) --octave;
        else f /= 2;
    }
    f &= 0x3FF;

    octave += octave_offset;
    if (octave < 0) octave = 0;
    if (octave > 7) octave = 7;   /* 超出范围时 AIL 会加 MULT，实际曲目不会触发 */
    *fnum  = f;
    *block = octave;
}

/* ---------------- Patch / voice control ---------------- */

static uint8_t pan_bits(int midi_ch)
{
    int val = g_pan[midi_ch] & 0x7F;
    return (uint8_t)(val < 60 ? 0x10 : (val > 67 ? 0x20 : 0x30));
}

static void opl_patch(int ch, const opl2_patch *p, int mv, uint8_t pan)
{
    g_opl = chip_of(ch);
    int m = OP_OFF[ch % 9], c = OP_OFF[ch % 9] + 3;
    const opl2_op *mod = &p->op[1];
    const opl2_op *car = &p->op[0];
    int am = p->rC0 & 1;
    uint8_t tl_m = (uint8_t)((mod->r40 & 0xC0) | ail_scale_tl(mod->r40 & 0x3F, mv, am));
    uint8_t tl_c = (uint8_t)((car->r40 & 0xC0) | ail_scale_tl(car->r40 & 0x3F, mv, 1));
    uint8_t c0 = (uint8_t)((p->rC0 & 0x0F) | pan);

    if (g_patch_cache[ch] != p) {
        opl_write(0x20 + m, mod->r20);
        opl_write(0x20 + c, car->r20);
        opl_write(0x60 + m, mod->r60);
        opl_write(0x60 + c, car->r60);
        opl_write(0x80 + m, mod->r80);
        opl_write(0x80 + c, car->r80);
        opl_write(0xE0 + m, mod->rE0);
        opl_write(0xE0 + c, car->rE0);
        opl_write(0xC0 + ch % 9, c0);
        g_patch_cache[ch] = p;
    }
    opl_write(0xC0 + ch % 9, c0);
    opl_write(0x40 + m, tl_m);
    opl_write(0x40 + c, tl_c);
}

static void opl_note(int ch, double tone)
{
    g_opl = chip_of(ch);
    int fnum, block;
    ail_fnum(tone, &fnum, &block);
    opl_write(0xA0 + ch % 9, (uint8_t)(fnum & 0xFF));
    g_voice[ch].b0 = (uint8_t)(0x20 | ((block & 7) << 2) | ((fnum >> 8) & 3));
    opl_write(0xB0 + ch % 9, g_voice[ch].b0);
}

/* 当前应写入的 AIL tone：基础音高 + 弯音 + 颤音（同 MIDIplay::noteUpdFreq） */
static double voice_tone(const voice_t *v)
{
    int mc = v->midi_ch;
    double tone = v->tone + g_bend[mc] * g_bendsense[mc];
    if (!v->sustained && g_vibrato[mc])
        tone += (double)g_vibrato[mc] * VIB_DEPTH * sin(g_vibpos[mc]);
    return tone;
}

/* 只重写 A0/B0，用于弯音/颤音；踏板保持的音不弯（见上游 noteUpdFreq） */
static void voice_update_pitch(int ch)
{
    voice_t *v = &g_voice[ch];
    if (v->sustained)
        return;
    g_opl = chip_of(ch);
    int fnum, block;
    ail_fnum(voice_tone(v), &fnum, &block);
    opl_write(0xA0 + ch % 9, (uint8_t)(fnum & 0xFF));
    v->b0 = (uint8_t)(0x20 | ((block & 7) << 2) | ((fnum >> 8) & 3));
    opl_write(0xB0 + ch % 9, v->b0);
}

/* 实时 CC7/CC11：重写两个算子的 TL（上游 MIDIplay::touchNote） */
static void voice_update_volume(int ch)
{
    voice_t *v = &g_voice[ch];
    const opl2_patch *p = v->patch;
    if (!p)
        return;
    int mc = v->midi_ch;
    int mv = ail_midi_volume(v->vel, g_chvol[mc], g_chexpr[mc], g_master_vol);
    int m = OP_OFF[ch % 9], c = OP_OFF[ch % 9] + 3;
    int am = p->rC0 & 1;
    g_opl = chip_of(ch);
    opl_write(0x40 + m, (uint8_t)((p->op[1].r40 & 0xC0) |
                                  ail_scale_tl(p->op[1].r40 & 0x3F, mv, am)));
    opl_write(0x40 + c, (uint8_t)((p->op[0].r40 & 0xC0) |
                                  ail_scale_tl(p->op[0].r40 & 0x3F, mv, 1)));
}

/* AIL drivers implement MIDI CC10 by rewriting OPL3 C0 panning bits.  FD2's
 * XMI barely uses CC10, but the path is kept so explicit pan events behave
 * like the MDI driver rather than being silently forced to centre. */
static void voice_update_pan(int ch)
{
    voice_t *v = &g_voice[ch];

    if (!v->used || !v->patch)
        return;
    g_opl = chip_of(ch);
    opl_write(0xC0 + ch % 9,
              (uint8_t)((v->patch->rC0 & 0x0F) | pan_bits(v->midi_ch)));
}

static void opl_off(int ch)
{
    g_opl = chip_of(ch);
    g_voice[ch].b0 = (uint8_t)(g_voice[ch].b0 & ~0x20);
    opl_write(0xB0 + ch % 9, g_voice[ch].b0);
}

/* YM3812 envelope rate (0..15) -> approximate milliseconds.  These values
 * only drive voice-allocation aging, never the OPL registers themselves. */
static const uint16_t OPL_ENV_MS[16] = {
    40000, 4096, 3277, 2621, 2097, 1678, 1341, 1073,
      858,  686,  549,  439,  351,  281,  225,  180
};

static int ail_env_on_ms(const ail_timbre *t)
{
    /* t->data.op[0] is the carrier (AIL op2). */
    const opl2_op *car = &t->data.op[0];
    int ar = car->r60 >> 4, dr = car->r60 & 15, sl = car->r80 >> 4;
    int on;

    if (sl >= 15)              /* sustains at full level: AIL keeps the voice */
        return 40000;
    on = OPL_ENV_MS[ar] + (OPL_ENV_MS[dr] * (15 - sl)) / 15;
    return on > 40000 ? 40000 : on;
}

static int ail_env_off_ms(const ail_timbre *t)
{
    int rr = t->data.op[0].r80 & 15;
    return OPL_ENV_MS[rr];
}

static int voice_off_ms(const voice_t *v)
{
    if (v->timbre)
        return ail_env_off_ms(v->timbre);
    if (v->ins >= 128) {
        int k = v->ins - 128;
        if (k < 0) k = 0;
        if (k > 52) k = 52;
        return opl2_percussion_off_ms[k];
    }
    return opl2_melodic_off_ms[v->ins & 127];
}

static int voice_on_ms(const voice_t *v)
{
    if (v->timbre)
        return ail_env_on_ms(v->timbre);
    if (v->ins >= 128) {
        int k = v->ins - 128;
        if (k < 0) k = 0;
        if (k > 52) k = 52;
        return opl2_percussion_on_ms[k];
    }
    return opl2_melodic_on_ms[v->ins & 127];
}

/* Miles "newer" physical allocator: ring search from the last assigned
 * voice, then prioritySort() when all voices are busy.  The real driver has
 * 20 virtual voices over 18 physical ones; this engine keeps the 18 physical
 * voices as the only layer, so an incoming note always steals immediately
 * (with CC112 protection and per-channel active counts matching Miles). */
static int find_voice(int midi_ch)
{
    int start = 0, n, ch;

    if (g_circular_phys >= 0 && g_circular_phys < g_num_ch)
        start = (g_circular_phys + 1) % g_num_ch;
    for (n = 0; n < g_num_ch; ++n) {
        ch = (start + n) % g_num_ch;
        if (!g_voice[ch].used) {
            g_circular_phys = ch;
            return ch;
        }
    }

    {
        uint16_t lowest = 0xFFFF;
        int victim = 0;

        for (ch = 0; ch < g_num_ch; ++ch) {
            int mc = g_voice[ch].midi_ch;
            uint16_t p = (g_voice_protect[mc] >= 64)
                ? 0xFFFF : g_voice[ch].priority;

            if (p >= g_active_voices[mc])
                p = (uint16_t)(p - g_active_voices[mc]);
            else
                p = 0;
            if (p <= lowest) {      /* Miles uses <= for voicedLowest */
                lowest = p;
                victim = ch;
            }
        }
        return victim;
    }
}

/* Resolve the bank AIL would use for this program/percussion note from the
 * song's TIMB chunk.  Bank 127 entries belong to channel 9; all other entries
 * are melodic programs.  Defaults match FD2's SAMPLE.OPL (0 / 127). */
static int timb_bank_for(int is_drum, int pid)
{
    int default_bank = is_drum ? 127 : 0;

    for (int i = 0; i < g_timb_count; ++i) {
        uint8_t p = g_timb[2 * i];
        uint8_t b = g_timb[2 * i + 1];
        if ((int)p != pid)
            continue;
        if (is_drum && b == 127)
            return 127;
        if (!is_drum && b != 127)
            return (int)b;
    }
    return default_bank;
}

static void note_on(int midi_ch, int note, int vel, uint32_t tick)
{
    int is_drum = (midi_ch == 9 && note >= 35 && note <= 87);
    int ins = is_drum ? 128 + note - 35 : g_prog[midi_ch] & 127;
    const ail_timbre *t = NULL;
    const opl2_patch *p;

    if (ail_timbres_loaded()) {
        int pid  = is_drum ? (note & 0x7F) : (g_prog[midi_ch] & 127);
        int bank = timb_bank_for(is_drum, pid);

        t = ail_timbre_find(bank, pid);
    }
    if (t) {
        p = &t->data;
    } else {
        /* SAMPLE.OPL missing: keep the old generic bank as a safe fallback. */
        p = is_drum ? &opl2_bank_percussion[note - 35]
                    : &opl2_bank_melodic[ins];
    }

    /* AIL gives percussion a fixed pitch via the instrument's transposition
     * field; melodic transposition is zero in FD2's bank.  ScummVM's
     * MilesAdlibDriver does exactly this (channel 9: note = transposition). */
    double trans = t ? (double)t->trans : 0.0;
    double tone  = is_drum
        ? (t ? trans : (double)opl2_drum_key[note - 35])
        : (double)note + trans;
    int mv = ail_midi_volume(vel, g_chvol[midi_ch], g_chexpr[midi_ch], g_master_vol);
    int ch = find_voice(midi_ch);

    if (g_voice[ch].used) {
        int old_ch = g_voice[ch].midi_ch;

        opl_off(ch);              /* 偷声前先放音，避免爆音 */
        if (g_active_voices[old_ch])
            --g_active_voices[old_ch];
        ++g_steals;
    }
    ++g_active_voices[midi_ch];
    g_circular_phys = ch;         /* ring advances on reassignment too */

    g_voice[ch].priority = 32767; /* Miles: currentPriority at note-on */
    g_voice[ch].used = 1;
    g_voice[ch].ever_used = 1;
    g_voice[ch].ins = ins;
    g_voice[ch].midi_ch = midi_ch;
    g_voice[ch].note = note;
    g_voice[ch].start_tick = tick;
    g_voice[ch].end_tick = tick + 60;
    g_voice[ch].koff_us = 0;
    g_voice[ch].timbre = t;
    g_voice[ch].kon_us = 1000LL * voice_on_ms(&g_voice[ch]);
    g_voice[ch].fixed_sustain = (voice_on_ms(&g_voice[ch]) == 40000);
    g_voice[ch].vibdelay_us = 0;
    g_voice[ch].sustained = 0;
    g_voice[ch].vel = (uint8_t)vel;
    g_voice[ch].tone = tone;
    g_voice[ch].patch = p;

    opl_patch(ch, p, mv, pan_bits(midi_ch));
    opl_note(ch, voice_tone(&g_voice[ch]));
    ++g_notes;
}

static void note_off(int midi_ch, int note)
{
    for (int v = 0; v < g_num_ch; ++v) {
        if (g_voice[v].used && g_voice[v].midi_ch == midi_ch && g_voice[v].note == note) {
            if (g_sustain[midi_ch]) {
                /* 踏板保持：不 key-off，等 CC64<64 再统一放音 */
                g_voice[v].sustained = 1;
                g_voice[v].note = -1;
                g_voice[v].koff_us = 0;
            } else {
                opl_off(v);
                if (g_active_voices[midi_ch])
                    --g_active_voices[midi_ch];
                g_voice[v].used = 0;
                g_voice[v].koff_us = 1000LL * voice_off_ms(&g_voice[v]);
            }
        }
    }
}

/* ---------------- Native OPL tick and host-rate resampler ---------------- */

static int16_t opl_native_sample(void)
{
    int s = (int)OPL_calc(g_chip[0]) + (int)OPL_calc(g_chip[1]);
    int v = (int)lround(s * OUT_GAIN);
    if (v > 32767) v = 32767;
    if (v < -32768) v = -32768;
    opl_tick();
    return (int16_t)v;
}

static uint64_t g_output_frames;
static int32_t g_rsm_old, g_rsm_cur, g_rsm_cnt, g_rsm_ratio;

static void resampler_setup(uint32_t outrate)
{
    g_rsm_ratio = (int32_t)(((uint64_t)outrate << RSM_FRAC) / OPL_NATIVE);
    g_rsm_old = g_rsm_cur = 0;
    g_rsm_cnt = 0;
    g_output_frames = 0;
}

static int16_t resampler_tick(void)
{
    int32_t sc = g_rsm_cnt;
    if (g_output_frames++ == 0) {
        g_rsm_cnt = sc + (1 << RSM_FRAC);
        return 0;
    }
    while (sc >= g_rsm_ratio) {
        g_rsm_old = g_rsm_cur;
        g_rsm_cur = opl_native_sample();
        sc -= g_rsm_ratio;
    }
    int32_t out = (g_rsm_old * (g_rsm_ratio - sc) + g_rsm_cur * sc) / g_rsm_ratio;
    g_rsm_cnt = sc + (1 << RSM_FRAC);
    return (int16_t)out;
}

/* ---------------- Stream scheduling ---------------- */

static const synth_event *g_ev;
static int                g_ev_count;
static double             g_tick_rate;
static uint32_t           g_rate;
static uint32_t           g_total;
static uint32_t           g_rendered;
static int                g_ei;
static int                g_loop;
static double             g_wait, g_delay, g_carry;
static uint32_t           g_skip;
static uint32_t           g_call_left;

static int peak_voices_now(void)
{
    int n = 0;
    for (int i = 0; i < g_num_ch; ++i)
        if (g_voice[i].used)
            ++n;
    return n;
}

static void opl_panic(void)
{
    for (int ch = 0; ch < g_num_ch; ++ch)
        opl_off(ch);
    memset(g_voice, 0, sizeof g_voice);
    memset(g_patch_cache, 0, sizeof g_patch_cache);
    memset(g_active_voices, 0, sizeof g_active_voices);
    memset(g_voice_protect, 0, sizeof g_voice_protect);
    g_circular_phys = -1;
}

static void reset_sequence_state(void)
{
    g_ei = 0;
    g_rendered = 0;
    g_wait = g_delay = g_carry = 0.0;
    g_skip = 0;
    g_call_left = 0;
}

static void stream_init(const synth_event *ev, int count, double tick_rate,
                        uint32_t rate)
{
    g_ev = ev;
    g_ev_count = count;
    g_tick_rate = tick_rate;
    g_rate = rate;

    if (!g_chip[0]) {
        g_chip[0] = OPL_new(OPL_CLOCK, OPL_NATIVE);
        g_chip[1] = OPL_new(OPL_CLOCK, OPL_NATIVE);
        OPL_setChipType(g_chip[0], 2);
        OPL_setChipType(g_chip[1], 2);
    }
    g_write_buffered = 1;
    g_writelog = getenv("FD2_WRITELOG") != NULL;
    OPL_reset(g_chip[0]);
    OPL_reset(g_chip[1]);
    memset(g_writebuf, 0, sizeof g_writebuf);
    g_writebuf_cur = g_writebuf_last = 0;
    g_writebuf_samplecnt = g_writebuf_lasttime = 0;
    g_opl = g_chip[0];
    opl_reference_init();
    g_opl = g_chip[0];

    memset(g_voice, 0, sizeof g_voice);
    memset(g_patch_cache, 0, sizeof g_patch_cache);
    memset(g_active_voices, 0, sizeof g_active_voices);
    memset(g_voice_protect, 0, sizeof g_voice_protect);
    g_circular_phys = -1;
    g_notes = 0;
    g_peak_voices = 0;
    g_steals = 0;
    g_master_vol = 127;
    for (int i = 0; i < 16; ++i) {
        g_prog[i] = 0;
        g_chvol[i] = 100;
        g_chexpr[i] = 127;
        g_pan[i] = 64;
        g_bend[i] = 0.0;
        g_bendsense[i] = 2.0 * 128.0 / (128.0 * 8192.0);
        g_vibrato[i] = 0;
        g_sustain[i] = 0;
        g_vibpos[i] = 0.0;
    }

    uint32_t last_tick = ev[count - 1].tick;
    g_total = (uint32_t)((double)(last_tick + 2) / tick_rate * (double)rate) + 1;
    resampler_setup(rate);
    reset_sequence_state();
}

static void apply_event(const synth_event *e, uint32_t t)
{
    uint8_t st = e->msg[0];
    int ch = st & 0x0F;
    int hi = st & 0xF0;

    if (hi == 0xC0) {
        g_prog[ch] = e->msg[1];
    } else if (hi == 0xB0) {
        if (e->msg[1] == 7) {
            g_chvol[ch] = e->msg[2];
            for (int v = 0; v < g_num_ch; ++v)
                if (g_voice[v].used && g_voice[v].midi_ch == ch)
                    voice_update_volume(v);
        } else if (e->msg[1] == 11) {
            g_chexpr[ch] = e->msg[2];
            for (int v = 0; v < g_num_ch; ++v)
                if (g_voice[v].used && g_voice[v].midi_ch == ch)
                    voice_update_volume(v);
        } else if (e->msg[1] == 10) {
            printf("synth: CC10 pan ch=%d val=%d\n", ch, e->msg[2]);
            g_pan[ch] = e->msg[2];
            for (int v = 0; v < g_num_ch; ++v)
                if (g_voice[v].used && g_voice[v].midi_ch == ch)
                    voice_update_pan(v);
        } else if (e->msg[1] == 1) {
            g_vibrato[ch] = e->msg[2];
            if (g_vibrato[ch] == 0)
                g_vibpos[ch] = 0.0;
        } else if (e->msg[1] == 64) {
            if (e->msg[2] >= 64) {
                g_sustain[ch] = 1;
            } else {
                g_sustain[ch] = 0;
                for (int v = 0; v < g_num_ch; ++v)
                    if (g_voice[v].used && g_voice[v].sustained &&
                        g_voice[v].midi_ch == ch) {
                        opl_off(v);
                        if (g_active_voices[ch])
                            --g_active_voices[ch];
                        g_voice[v].used = 0;
                        g_voice[v].sustained = 0;
                        g_voice[v].koff_us = 0;  /* 上游此路径不设 Release 时间 */
                    }
            }
        } else if (e->msg[1] == 112) {
            /* Miles MILES_CONTROLLER_PROTECT_VOICE */
            g_voice_protect[ch] = e->msg[2];
        }
    } else if (hi == 0xE0) {
        g_bend[ch] = (double)((e->msg[1] & 0x7F) | ((e->msg[2] & 0x7F) << 7)) - 8192.0;
        for (int v = 0; v < g_num_ch; ++v)
            if (g_voice[v].used && g_voice[v].midi_ch == ch)
                voice_update_pitch(v);
    } else if (hi == 0x90 && e->msg[2] != 0) {
        note_on(ch, e->msg[1], e->msg[2], t);
    } else if (hi == 0x80 || (hi == 0x90 && e->msg[2] == 0)) {
        note_off(ch, e->msg[1]);
    }
}

static uint32_t stream_fill(int16_t *dst, uint32_t n, double gain)
{
    uint32_t written = 0;
    const double maxdelay = 512.0 / (double)g_rate;
    const double mindelay = 1.0 / (double)g_rate;

    while (written < n) {
        if (g_ei >= g_ev_count) {
            /* All events consumed.  Keep rendering the OPL tail (release
             * decay / noise) until the requested frame count, matching the
             * reference render_opl2.c tail loop.  Without this the dump
             * would be zero-padded and the release tails would be lost. */
            if (g_loop) {
                if (getenv("FD2_LOOPLOG"))
                    fprintf(stderr, "synth: loop restart after %u frames\n", g_rendered);
                opl_panic();
                reset_sequence_state();
                continue;
            }
            while (written < n) {
                int v = (int)((double)resampler_tick() * gain);
                if (v > 32767) v = 32767;
                if (v < -32768) v = -32768;
                dst[written++] = (int16_t)v;
            }
            return n;
        }

        if (g_call_left == 0)
            g_call_left = ADL_CHUNK;

        double eat = g_delay < maxdelay ? g_delay : maxdelay;
        uint32_t call = g_call_left < (n - written) ? g_call_left : (n - written);
        uint32_t want;
        if (g_skip) {
            /* 上一段被缓冲区边界截断的帧：这段时间已经计过，只补画，不推进事件 */
            want = g_skip < 2048 ? g_skip : 2048;
        } else {
            g_delay -= eat;
            g_carry += (double)g_rate * eat;
            want = (uint32_t)g_carry;
            g_carry -= want;
        }
        uint32_t count = want > call ? call : want;

        for (uint32_t i = 0; i < count; ++i) {
            int v = (int)((double)resampler_tick() * gain);
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            dst[written++] = (int16_t)v;
            ++g_rendered;
        }
        g_call_left -= count;

        int pv = peak_voices_now();
        if ((uint64_t)pv > g_peak_voices)
            g_peak_voices = (uint64_t)pv;

        if (g_skip) {
            g_skip -= count;
            continue;
        }
        /* 本段还没画完的帧留到下一个缓冲区继续，必须精确记账：丢掉或重复
         * 都会让事件时间轴相对音频整体前移/后移（听感就是节奏变快/变慢）。 */
        g_skip = want - count;

        g_wait -= eat;
        while (g_ei < g_ev_count && g_wait <= mindelay * 0.5) {
            uint32_t t = g_ev[g_ei].tick;
            do {
                apply_event(&g_ev[g_ei++], t);
            } while (g_ei < g_ev_count && g_ev[g_ei].tick == t);
            if (g_ei < g_ev_count)
                g_wait += (double)(g_ev[g_ei].tick - t) / g_tick_rate;
        }
        g_delay = g_wait > 0.0 ? g_wait : 0.0;

        /* libADLMIDI AdlChannel::addAge：每个调度分段推进一次 */
        int64_t age_us = (int64_t)(eat * 1000000.0);
        int64_t neg_us = 1000LL * (-0x1FFFFFFFL);
        for (int v = 0; v < g_num_ch; ++v) {
            voice_t *vv = &g_voice[v];
            if (vv->used) {
                vv->koff_us = 0;
                if (!vv->fixed_sustain) {
                    vv->kon_us -= age_us;
                    if (vv->kon_us < neg_us)
                        vv->kon_us = neg_us;
                }
                vv->vibdelay_us += age_us;
            } else {
                vv->koff_us -= age_us;
                if (vv->koff_us < neg_us)
                    vv->koff_us = neg_us;
                if (vv->koff_us < 0)
                    vv->koff_us = 0;
            }
        }

        /* libADLMIDI TickIterators -> updateVibrato：先按当前相位重写音高，再推进相位 */
        for (int mc = 0; mc < 16; ++mc) {
            int any = 0;
            if (g_vibrato[mc]) {
                for (int v = 0; v < g_num_ch; ++v) {
                    if (g_voice[v].used && g_voice[v].midi_ch == mc &&
                        !g_voice[v].sustained) {
                        voice_update_pitch(v);
                        any = 1;
                    }
                }
            }
            if (any) g_vibpos[mc] += eat * VIB_SPEED;
            else     g_vibpos[mc] = 0.0;
        }
    }
    return written;
}

/* ---------------- Mixer / volume ---------------- */

static double   g_seq_from = 127.0;
static double   g_seq_to   = 127.0;
static uint64_t g_ramp_t0;
static int      g_ramp_ms;
static uint64_t g_arm_t0;

static double seq_volume_now(void)
{
    uint64_t e;

    if (g_ramp_ms <= 0)
        return g_seq_to;
    e = plat_now_ms() - g_ramp_t0;
    if (e >= (uint64_t)g_ramp_ms)
        return g_seq_to;
    return g_seq_from + (g_seq_to - g_seq_from) * (double)e / (double)g_ramp_ms;
}

static double stream_gain(void);

static unsigned synth_music_fill(int16_t *dst, unsigned nframes)
{
    stream_fill(dst, (uint32_t)nframes, stream_gain());
    return nframes;
}

void synth_set_sequence_volume(int volume, int ms)
{
    if (volume < 0)   volume = 0;
    if (volume > 127) volume = 127;
    g_seq_from = seq_volume_now();
    g_seq_to   = (double)volume;
    g_ramp_ms  = ms > 0 ? ms : 0;
    g_ramp_t0  = plat_now_ms();

    if (audio_music_held()) {
        audio_release_music();
        printf("synth: stream released after %lu ms -> sequence volume %.0f/127, "
               "gain %.3f\n",
               (unsigned long)(plat_now_ms() - g_arm_t0),
               seq_volume_now(), stream_gain());
    }
}

static int g_master_pct = 100;

static double stream_gain(void)
{
    return ((double)g_master_pct / 100.0) * (seq_volume_now() / 127.0);
}

void synth_set_master_volume(int percent)
{
    if (percent < 0)   percent = 0;
    if (percent > 100) percent = 100;
    g_master_pct = percent;
}

/* ---------------- Public play/stop API ---------------- */

int synth_is_playing(void)
{
    return audio_music_src() != NULL;
}

void synth_stop(void)
{
    audio_set_music(NULL);
}

int synth_play(const synth_event *ev, int count, double tick_rate,
               uint32_t sample_rate, int loop)
{
    uint32_t rate = sample_rate;

    if (!ev || count <= 0)
        return 0;

    /* The game data directory is the process CWD.  Load the AIL bank lazily
     * so offline harnesses that compile synth.c get the same timbres without
     * needing an explicit init call. */
    if (!ail_timbres_loaded() && !g_timbres_auto_tried) {
        g_timbres_auto_tried = 1;
        if (!ail_timbres_load("SAMPLE.OPL"))
            ail_timbres_load("SAMPLE.AD");
    }

    /* The original AIL sequencer rendered one slice at a time from a timer
     * callback.  Do the same: initialise the stream and let the mixer pull
     * slices via synth_music_fill().  No whole-song pre-render, ever. */
    g_loop = loop ? 1 : 0;
    stream_init(ev, count, tick_rate, rate);
    g_arm_t0 = plat_now_ms();
    audio_hold_music();
    audio_set_music(synth_music_fill);
    return 1;
}

void synth_set_bank_path(const char *path)
{
    (void)path;
}

int synth_load_ail_timbres(const char *path)
{
    g_timbres_auto_tried = 1;
    return ail_timbres_load(path);
}

void synth_set_timb(const uint8_t *pairs, int count)
{
    int n = count > 256 ? 256 : (count > 0 ? count : 0);

    g_timb_count = 0;
    if (!pairs || n <= 0)
        return;
    memcpy(g_timb, pairs, (size_t)n * 2);
    g_timb_count = n;
}
