/*
 * OPL2 music synthesiser for the FD2 host port.
 *
 * The DOS originals used AIL with a two-operator OPL2 tone bank.  This backend
 * emulates that path with two emu8950 OPL2 chips (18 melodic voices) and the
 * Fat Man / AIL two-operator bank, then resamples the 49716 Hz native OPL
 * output to the host audio rate.  The register write order, patch caching and
 * buffered register queue intentionally follow libADLMIDI/Nuked OPL3, which is
 * the reference used to tune the patch and timing behaviour.
 */

#include "synth.h"
#include "platform.h"
#include "audio.h"
#include "emu8950/emu8950.h"
#include "opl2_patches.h"
#include "opl2_drumkey.h"
#include "ail_freq.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define OPL_NATIVE   49716u
#define OPL_CLOCK    3579545u
#define OUT_GAIN     (-2.0 * 46340.0 / 65535.0)
#define RSM_FRAC     10
#define NUM_CH       18
#define ADL_CHUNK    2048u

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
    int      used;
    int      ever_used;
    int      ins;
    int      midi_ch;
    int      note;
    uint32_t start_tick;
    uint32_t end_tick;
    uint8_t  b0;
    int64_t  koff_us;
} voice_t;

static voice_t          g_voice[NUM_CH];
static const opl2_patch *g_patch_cache[NUM_CH];
static int              g_prog[16];
static int              g_chvol[16];
static int              g_chexpr[16];
static int              g_master_vol = 127;

static uint64_t         g_notes;
static uint64_t         g_peak_voices;

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

static void opl_write_now(uint16_t reg, uint8_t val)
{
    unsigned low = reg & 0xff;
    if (low == 0x04 || low == 0x05)
        return;
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
    for (int ch = 0; ch < 18; ++ch) {
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
    ref_reset(2);
    ref_update_categories(2);

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

static void ail_fnum(int tone, int *fnum, int *block)
{
    int pitch, idx, halftone, octave;
    uint16_t f;

    if (tone < 0) tone = 0;
    pitch = (tone - 12) * 16;
    while (pitch < 12 * 16) pitch += 12 * 16;
    while (pitch > 96 * 16 - 1) pitch -= 12 * 16;

    idx      = pitch >> 4;
    halftone = (idx % 12) * 16 + (pitch & 15);
    octave   = idx / 12;

    f = ail_freqtable[halftone];
    if (!(f & 0x8000)) {
        if (octave > 0) --octave;
        else f /= 2;
    }
    f &= 0x3FF;
    if (octave > 7) octave = 7;
    *fnum  = (int)f;
    *block = octave;
}

/* ---------------- Patch / voice control ---------------- */

static void opl_patch(int ch, const opl2_patch *p, int mv)
{
    g_opl = chip_of(ch);
    int m = OP_OFF[ch % 9], c = OP_OFF[ch % 9] + 3;
    const opl2_op *mod = &p->op[1];
    const opl2_op *car = &p->op[0];
    int am = p->rC0 & 1;
    uint8_t tl_m = (uint8_t)((mod->r40 & 0xC0) | ail_scale_tl(mod->r40 & 0x3F, mv, am));
    uint8_t tl_c = (uint8_t)((car->r40 & 0xC0) | ail_scale_tl(car->r40 & 0x3F, mv, 1));
    uint8_t c0 = (uint8_t)(p->rC0 | 0x30);

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

static void opl_note(int ch, int tone)
{
    g_opl = chip_of(ch);
    int fnum, block;
    ail_fnum(tone, &fnum, &block);
    opl_write(0xA0 + ch % 9, (uint8_t)(fnum & 0xFF));
    g_voice[ch].b0 = (uint8_t)(0x20 | ((block & 7) << 2) | ((fnum >> 8) & 3));
    opl_write(0xB0 + ch % 9, g_voice[ch].b0);
}

static void opl_off(int ch)
{
    g_opl = chip_of(ch);
    g_voice[ch].b0 = (uint8_t)(g_voice[ch].b0 & ~0x20);
    opl_write(0xB0 + ch % 9, g_voice[ch].b0);
}

static int voice_off_ms(const voice_t *v)
{
    if (v->ins >= 128) {
        int k = v->ins - 128;
        if (k < 0) k = 0;
        if (k > 52) k = 52;
        return opl2_percussion_off_ms[k];
    }
    return opl2_melodic_off_ms[v->ins & 127];
}

static int find_voice(int ins)
{
    int best = -1;
    int64_t best_score = -0x7FFFFFFFFFFFFFFFLL;

    for (int i = 0; i < NUM_CH; ++i) {
        if (g_voice[i].used)
            continue;
        int64_t score;
        if (!g_voice[i].ever_used) {
            score = 0;
        } else {
            score = -(g_voice[i].koff_us / 1000);
            if (score < 0 && g_voice[i].ins != ins)
                score -= 40000;
        }
        if (score > best_score) {
            best_score = score;
            best = i;
        }
    }
    if (best >= 0)
        return best;

    best = 0;
    for (int i = 1; i < NUM_CH; ++i)
        if (g_voice[i].start_tick < g_voice[best].start_tick)
            best = i;
    return best;
}

static void note_on(int midi_ch, int note, int vel, uint32_t tick)
{
    int is_drum = (midi_ch == 9 && note >= 35 && note <= 87);
    int ins = is_drum ? 128 + note - 35 : g_prog[midi_ch] & 127;
    const opl2_patch *p = is_drum ? &opl2_bank_percussion[note - 35]
                                  : &opl2_bank_melodic[ins];
    int tone = is_drum ? (int)opl2_drum_key[note - 35] : note;
    int mv = ail_midi_volume(vel, g_chvol[midi_ch], g_chexpr[midi_ch], g_master_vol);
    int ch = find_voice(ins);

    if (g_voice[ch].used)
        opl_off(ch);
    opl_patch(ch, p, mv);
    opl_note(ch, tone);

    g_voice[ch].used = 1;
    g_voice[ch].ever_used = 1;
    g_voice[ch].ins = ins;
    g_voice[ch].midi_ch = midi_ch;
    g_voice[ch].note = note;
    g_voice[ch].start_tick = tick;
    g_voice[ch].end_tick = tick + 60;
    g_voice[ch].koff_us = 0;
    ++g_notes;
}

static void note_off(int midi_ch, int note)
{
    for (int v = 0; v < NUM_CH; ++v) {
        if (g_voice[v].used && g_voice[v].midi_ch == midi_ch && g_voice[v].note == note) {
            opl_off(v);
            g_voice[v].used = 0;
            g_voice[v].koff_us = 1000LL * voice_off_ms(&g_voice[v]);
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
static int                g_call_skipped;

static int peak_voices_now(void)
{
    int n = 0;
    for (int i = 0; i < NUM_CH; ++i)
        if (g_voice[i].used)
            ++n;
    return n;
}

static void opl_panic(void)
{
    for (int ch = 0; ch < NUM_CH; ++ch)
        opl_off(ch);
    memset(g_voice, 0, sizeof g_voice);
    memset(g_patch_cache, 0, sizeof g_patch_cache);
}

static void reset_sequence_state(void)
{
    g_ei = 0;
    g_rendered = 0;
    g_wait = g_delay = g_carry = 0.0;
    g_skip = 0;
    g_call_left = 0;
    g_call_skipped = 0;
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
    g_notes = 0;
    g_peak_voices = 0;
    g_master_vol = 127;
    for (int i = 0; i < 16; ++i) {
        g_prog[i] = 0;
        g_chvol[i] = 100;
        g_chexpr[i] = 127;
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
        if (e->msg[1] == 7)       g_chvol[ch] = e->msg[2];
        else if (e->msg[1] == 11) g_chexpr[ch] = e->msg[2];
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

        if (g_call_left == 0) {
            g_call_left = ADL_CHUNK;
            g_call_skipped = g_skip > 0;
        }

        double eat = g_delay < maxdelay ? g_delay : maxdelay;
        uint32_t call = g_call_left < (n - written) ? g_call_left : (n - written);
        uint32_t count;
        if (g_call_skipped) {
            count = g_skip < 2048 ? g_skip : 2048;
        } else {
            g_delay -= eat;
            g_carry += (double)g_rate * eat;
            count = (uint32_t)g_carry;
            g_carry -= count;
        }
        if (count > call) {
            g_skip = count - call;
            count = call;
        }

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

        if (g_call_skipped) {
            g_skip -= count;
            g_call_skipped = g_skip > 0;
            continue;
        }

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

        int64_t age_us = (int64_t)(eat * 1000000.0);
        for (int v = 0; v < NUM_CH; ++v) {
            if (!g_voice[v].used) {
                g_voice[v].koff_us -= age_us;
                if (g_voice[v].koff_us < 0)
                    g_voice[v].koff_us = 0;
            }
        }
    }
    return written;
}

/* ---------------- Whole-song dump / statistics ---------------- */

static int render(const synth_event *ev, int count, double tick_rate,
                  uint32_t rate, int16_t **out_pcm, uint32_t *out_samples)
{
    uint32_t total;
    int16_t *pcm;

    stream_init(ev, count, tick_rate, rate);
    total = g_total;
    pcm = (int16_t *)malloc((size_t)total * sizeof(int16_t));
    if (!pcm) {
        printf("synth: cannot allocate %u samples\n", (unsigned)total);
        return 0;
    }
    stream_fill(pcm, total, 1.0);
    *out_pcm = pcm;
    *out_samples = total;
    return 1;
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

static const char *g_dump_wav_path;

void synth_set_dump_path(const char *path)
{
    g_dump_wav_path = path;
}

int synth_is_playing(void)
{
    return audio_music_src() != NULL;
}

void synth_stop(void)
{
    audio_set_music(NULL);
}

long synth_rendered_notes(void)
{
    return (long)g_notes;
}

int synth_play(const synth_event *ev, int count, double tick_rate,
               uint32_t sample_rate, int loop)
{
    int16_t *pcm = NULL;
    uint32_t samples = 0;
    uint32_t rate = sample_rate;
    uint64_t t0, t1;
    double sec;

    if (!ev || count <= 0)
        return 0;

    g_loop = loop ? 1 : 0;
    t0 = plat_now_ms();
    if (!render(ev, count, tick_rate, rate, &pcm, &samples))
        return 0;
    t1 = plat_now_ms();
    sec = (double)samples / (double)rate;
    uint64_t rendered_notes = g_notes, peak_voices = g_peak_voices;
    printf("synth: %d events, %.3f s, %u samples, peak voices %llu, "
           "notes %llu, rendered in %llu ms (%.1f x real-time)\n",
           count, sec, (unsigned)samples,
           (unsigned long long)peak_voices,
           (unsigned long long)rendered_notes,
           (unsigned long long)(t1 - t0),
           (t1 > t0) ? sec / ((double)(t1 - t0) / 1000.0) : 0.0);

    if (g_dump_wav_path) {
        FILE *f = fopen(g_dump_wav_path, "wb");
        if (f) {
            uint8_t h[44] = {'R','I','F','F',0,0,0,0,'W','A','V','E',
                             'f','m','t',' ',16,0,0,0,1,0,1,0,0,0,0,0,
                             0,0,0,0,2,0,16,0,'d','a','t','a',0,0,0,0};
            uint32_t data_bytes = samples * 2, riff = 36 + data_bytes;
            memcpy(h + 4, &riff, 4);
            memcpy(h + 24, &rate, 4);
            uint32_t br = rate * 2;
            memcpy(h + 28, &br, 4);
            memcpy(h + 40, &data_bytes, 4);
            fwrite(h, 1, 44, f);
            fwrite(pcm, 2, samples, f);
            fclose(f);
            printf("synth: wrote MIDI dump %s (%u samples)\n",
                   g_dump_wav_path, (unsigned)samples);
        }
    }
    free(pcm);

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
