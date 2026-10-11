/* ail.c - Miles AIL (Audio Interface Library) replacement layer.
 *
 * Why this exists
 * ---------------
 * FD2.EXE has Miles AIL V3.02 linked in. The library itself is fine, but it
 * plays sound by loading a 16-bit real-mode driver (*.DIG for digital audio,
 * *.MDI for MIDI) and jumping straight into that code, which cannot run inside
 * a Win32 process. With the driver files blocked, every AIL_install_* returned
 * 0, the game set its "no digital audio device" flag and ran silently.
 *
 * How it is replaced
 * ------------------
 * IDA shows the game calls only 16 AIL entry points (port/re/RE_MAP.md ��3):
 *
 *     AIL_startup, AIL_shutdown,
 *     AIL_install_DIG_INI, AIL_install_MDI_INI,
 *     AIL_allocate_sample_handle, AIL_allocate_sequence_handle,
 *     AIL_init_sample, AIL_set_sample_address, AIL_set_sample_loop_count,
 *     AIL_start_sample, AIL_stop_sample,
 *     AIL_init_sequence, AIL_start_sequence, AIL_stop_sequence,
 *     AIL_set_sequence_volume, AIL_set_sequence_loop_count
 *
 * Each of those entry points is patched to a 5-byte `jmp rel32` targeting the
 * host implementation below. AIL's public API uses the Watcom cdecl calling
 * convention, which for these signatures is ABI-identical to MSVC __cdecl
 * (arguments on the stack, caller cleans up, return value in EAX), so the
 * patched calls are compatible without any thunking.
 *
 * Digital audio goes through the one software mixer in src/audio.h: the game
 * hands AIL a pointer and length into its own memory (AIL_set_sample_address),
 * we copy that block, bake in the gain and the start/stop ramp, and hand it to
 * the mixer as a one-shot voice. MIDI sequences are parsed (xmidi.c) and
 * synthesised by synth.c into the same mixer, so music and effects share one
 * device (docs/AUDIO.md §11.10).
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "platform.h"
#include "xmidi.h"
#include "synth.h"
#include "audio.h"

#define AIL_OBJ0_BASE    0x00010000u
/* FDPS asks for 8 sample handles in one go (sub_30270 loops 8 times), FD2 for 2. */
#define AIL_MAX_SAMPLES  8
#define AIL_MAX_SEQS     4

/* ------------------------------------------------------------- state ----- */

typedef struct {
    int        used;
    uint8_t   *addr;          /* guest sample pointer (AIL_set_sample_address) */
    uint32_t   len;
    int32_t    loop_count;
    int32_t    volume;
    int32_t    pan;
    int32_t    type;
    uint32_t   rate;          /* playback rate (AIL default unless changed) */
    int        channels;      /* from AIL_set_sample_type (FDPS sets it)     */
    int        bits;
    int32_t    playing;
    int        dumped;        /* sample bytes already written to dump_dir */
    uint8_t   *pcm;           /* host copy: handed to the mixer on play    */
} ail_sample;

typedef struct {
    int             used;
    const uint8_t  *data;     /* XMIDI blob inside FDMUS.DAT               */
    uint32_t        len;      /* derived from the IFF FORM header          */
    int32_t         seq_num;
    int32_t         volume;
    int32_t         loop_count;
    int32_t         playing;
} ail_seq;

static ail_sample  g_samples[AIL_MAX_SAMPLES];
static ail_seq     g_seqs[AIL_MAX_SEQS];
static uint32_t    g_rate    = 11025;   /* AIL default playback rate */
static int         g_bits    = 8;       /* AIL default sample type: mono 8-bit */
static int         g_stereo  = 0;
static char        g_dump_dir[PLAT_MAX_PATH];
static int         g_dump_seq;
/* How often a sample was cut off while still playing (AIL semantics: stop
 * then start). A cut is inherent to retriggering - what we removed is the
 * *device* teardown that used to go with it. Counted so the residual is
 * visible instead of being guessed at (docs/AUDIO.md). */
static unsigned long g_sample_cuts;

/* ------------------------------------------------------------- helpers --- */

static void dump_blob(const char *tag, const void *p, uint32_t len)
{
    char path[PLAT_MAX_PATH * 2];   /* dir + sep + tag + seq */
    FILE *f;

    if (!g_dump_dir[0] || !p || !len)
        return;
    if (len > 512 * 1024)
        len = 512 * 1024;
    snprintf(path, sizeof path, "%s/%s_%d.bin", g_dump_dir, tag, g_dump_seq++);
    f = fopen(path, "wb");
    if (!f)
        return;
    fwrite(p, 1, len, f);
    fclose(f);
    printf("ail: dumped %u bytes of %s to %s\n", (unsigned)len, tag, path);
}

/* Summarise the block the game is about to play: 8-bit unsigned PCM hovers
 * around 0x80 while 16-bit signed data shows high bytes clustered at 0x00 or
 * 0xFF. This is what pins down the format the game assumes. */
static void log_pcm_stats(const char *what, const uint8_t *p, uint32_t len)
{
    uint32_t i, near_128 = 0, hi_zero = 0;
    uint8_t lo = 0xFF, hi = 0x00;

    if (!len)
        return;
    for (i = 0; i < len; i++) {
        if (p[i] >= 0x60 && p[i] <= 0xA0) near_128++;
        if (p[i] < lo) lo = p[i];
        if (p[i] > hi) hi = p[i];
    }
    for (i = 1; i < len; i += 2)
        if (p[i] == 0x00 || p[i] == 0xFF) hi_zero++;
    printf("ail: %s len=%u  range=%02X..%02X  near-0x80=%.0f%%  odd-byte-00/FF=%.0f%%"
           "  head=%02X %02X %02X %02X %02X %02X %02X %02X\n",
           what, (unsigned)len, lo, hi, 100.0 * near_128 / len,
           100.0 * hi_zero / (len / 2 + 1),
           p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
}

static ail_sample *sample_of(void *handle)
{
    ail_sample *s = (ail_sample *)handle;
    if (s < g_samples || s >= g_samples + AIL_MAX_SAMPLES)
        return NULL;
    return s->used ? s : NULL;
}

static ail_seq *seq_of(void *handle)
{
    ail_seq *s = (ail_seq *)handle;
    if (s < g_seqs || s >= g_seqs + AIL_MAX_SEQS)
        return NULL;
    return s->used ? s : NULL;
}

/* ------------------------------------------------------------- mixer ----- */

/* A sample handle owns no device: src/audio.h has the only output device,
 * shared with the music, and it takes the host copy we build here (gain +
 * 3 ms ramp already baked in) as a one-shot voice it frees when the voice
 * runs out. So "stop" is "drop the voice", and a voice that was still
 * sounding is a real cut - counted, not silent (docs/AUDIO.md §11.6). */

static int sample_index(const ail_sample *s)
{
    return (int)(s - g_samples);
}

static void sample_stop(ail_sample *s)
{
    if (audio_sfx_stop(sample_index(s)))
        g_sample_cuts++;                /* a real cut: documented, not silent */
    if (s->pcm) {                       /* a copy play() never handed over     */
        free(s->pcm);
        s->pcm = NULL;
    }
    s->playing = 0;
}

/* Master output volume (host --volume, 0..100, default 100 = no attenuation,
 * the game's own level): attenuates what actually reaches the mixer, nothing
 * else. The pipeline above it - AIL volume tracking, the copy of the guest
 * PCM handed to the mixer - is untouched, so audio bugs still show up at any
 * volume. Debug runs pass --volume=10 to stay quiet. */
static int g_master_volume = 100;

void ail_set_master_volume(int percent)
{
    if (percent < 0)
        percent = 0;
    if (percent > 100)
        percent = 100;
    g_master_volume = percent;
    synth_set_master_volume(percent);
    printf("ail: master output volume = %d%% (music + SFX scaled in the mixer)\n",
           percent);
}

/* Scale a host-side PCM copy by `g` (0..1). 8-bit PCM is unsigned,
 * 16-bit is signed little-endian; channels are interleaved and all share the
 * same gain, so the loop is per sample either way. */
static void pcm_apply_gain(uint8_t *pcm, uint32_t bytes, int bits, double g)
{
    uint32_t i, n;

    if (bits <= 8) {
        for (i = 0; i < bytes; i++) {
            double v = ((int)pcm[i] - 128) * g;
            int    q = (int)(v + (v >= 0.0 ? 0.5 : -0.5)) + 128;
            pcm[i] = (uint8_t)(q < 0 ? 0 : (q > 255 ? 255 : q));
        }
    } else {
        int16_t *p = (int16_t *)(void *)pcm;
        n = bytes / 2;
        for (i = 0; i < n; i++) {
            double v = (double)p[i] * g;
            int    q = (int)(v + (v >= 0.0 ? 0.5 : -0.5));
            p[i] = (int16_t)(q < -32768 ? -32768 : (q > 32767 ? 32767 : q));
        }
    }
}

/* Fade the first and last few milliseconds of a PCM copy to silence.
 *
 * A one-shot voice starts and stops wherever the waveform happens to be: an
 * 8-bit buffer that begins at, say, 200 instead of the 128 silence level
 * steps the output the instant the voice starts, and the same step happens
 * again when it ends. Those two steps are the "click" (docs/AUDIO.md). The
 * DOS hardware fed the same waveform to a Sound Blaster, which was far more
 * forgiving; a resampling mixer is not.
 *
 * 3 ms is below the threshold where a level change is audible as such, but
 * it turns both steps into ramps. Applied to the host copy only - guest
 * memory is never touched. */
#define AIL_RAMP_MS 3

static void pcm_apply_ramp(uint8_t *pcm, uint32_t bytes, int bits,
                           uint32_t rate, int channels)
{
    uint32_t nsamp, ramp, i;
    int16_t *p16;

    nsamp = bytes / (bits > 8 ? 2u : 1u);
    if (nsamp < 8 || rate == 0)
        return;
    ramp = (uint32_t)((double)rate * AIL_RAMP_MS / 1000.0) *
           (uint32_t)(channels > 0 ? channels : 1);
    if (ramp == 0)
        return;
    if (ramp > nsamp / 2)
        ramp = nsamp / 2;               /* never eat more than half a sample */

    if (bits <= 8) {
        for (i = 0; i < ramp; i++) {
            double g = (double)i / (double)ramp;
            int head = (int)pcm[i] - 128;
            int tail = (int)pcm[nsamp - 1 - i] - 128;
            pcm[i]               = (uint8_t)((int)(head * g) + 128);
            pcm[nsamp - 1 - i]   = (uint8_t)((int)(tail * g) + 128);
        }
    } else {
        p16 = (int16_t *)(void *)pcm;
        for (i = 0; i < ramp; i++) {
            double g = (double)i / (double)ramp;
            p16[i]             = (int16_t)(p16[i] * g);
            p16[nsamp - 1 - i] = (int16_t)(p16[nsamp - 1 - i] * g);
        }
    }
}

static void sample_play(ail_sample *s)
{
    uint32_t played;
    int      bits;
    int      cut;
    int      idx;

    if (!s->addr || !s->len)
        return;
    idx = sample_index(s);

    /* Retrigger: only a voice that is genuinely still sounding is a cut -
     * a finished sample needs no cut at all (same test the old "header not
     * WHDR_DONE" check made). */
    cut = audio_sfx_active(idx);
    sample_stop(s);

    s->pcm = (uint8_t *)malloc(s->len);
    if (!s->pcm)
        return;
    memcpy(s->pcm, s->addr, s->len);
    bits = s->bits ? s->bits : g_bits;
    {
        /* master volume x the game's AIL_set_sample_volume (0..127, the AIL
         * default is 127): applied to the copy, never to guest memory. */
        double g = (double)g_master_volume / 100.0 *
                   (double)s->volume / 127.0;
        if (g < 1.0)
            pcm_apply_gain(s->pcm, s->len, bits, g);
    }
    pcm_apply_ramp(s->pcm, s->len, bits,
                   s->rate ? s->rate : g_rate, s->channels);

    /* The mixer takes ownership of the copy - converted to mono float there,
     * resampled from the sample's rate to the device rate, freed at the end. */
    if (audio_sfx_play(idx, s->pcm, s->len,
                       s->rate ? s->rate : g_rate,
                       s->channels ? s->channels : (g_stereo ? 2 : 1),
                       (unsigned)bits)) {
        s->pcm = NULL;                  /* owned by the mixer now */
        s->playing = 1;
        played = s->len;
        printf("ail: play %u bytes (%.2f s @ %u Hz, loop=%d)%s\n",
               (unsigned)played, (double)played / (g_rate * (g_stereo ? 2 : 1) * (g_bits / 8)),
               (unsigned)g_rate, (int)s->loop_count, cut ? " (cut)" : "");
    } else {
        printf("ail: play %u bytes - mixer refused the copy\n", (unsigned)s->len);
    }
}

/* ------------------------------------------- host AIL implementations ---- */

/* All of these are __cdecl to match the Watcom cdecl AIL API. */

static int32_t host_AIL_startup(void)
{
    printf("ail: startup -> ok (replacement layer)\n");
    return 1;
}

static void host_AIL_shutdown(void)
{
    int i;
    xmidi_stop();
    for (i = 0; i < AIL_MAX_SAMPLES; i++) {
        if (g_samples[i].used) {
            sample_stop(&g_samples[i]);
            if (g_samples[i].pcm) free(g_samples[i].pcm);
            memset(&g_samples[i], 0, sizeof g_samples[i]);  /* FDPS re-inits audio in-process */
        }
    }
    for (i = 0; i < AIL_MAX_SEQS; i++)
        memset(&g_seqs[i], 0, sizeof g_seqs[i]);
    printf("ail: shutdown (%lu samples cut short)\n",
           (unsigned long)g_sample_cuts);
}

/* The game checks these for non-zero before using any sample/sequence API. */
static int32_t host_AIL_install_DIG_INI(void)
{
    printf("ail: install_DIG_INI -> fake driver handle 1\n");
    return 1;
}

static int32_t host_AIL_install_MDI_INI(void)
{
    printf("ail: install_MDI_INI -> fake driver handle 1\n");
    return 1;
}

static void *host_AIL_allocate_sample_handle(int32_t driver)
{
    int i;
    (void)driver;
    for (i = 0; i < AIL_MAX_SAMPLES; i++) {
        if (!g_samples[i].used) {
            memset(&g_samples[i], 0, sizeof g_samples[i]);
            g_samples[i].used = 1;
            g_samples[i].volume = 127;      /* AIL default */
            g_samples[i].pan = 64;
            g_samples[i].rate = g_rate;
            g_samples[i].channels = g_stereo ? 2 : 1;
            g_samples[i].bits = g_bits;
            printf("ail: allocate_sample_handle -> %d\n", i);
            return &g_samples[i];
        }
    }
    printf("ail: allocate_sample_handle -> out of handles\n");
    return NULL;
}

static void *host_AIL_allocate_sequence_handle(int32_t driver)
{
    int i;
    (void)driver;
    for (i = 0; i < AIL_MAX_SEQS; i++) {
        if (!g_seqs[i].used) {
            memset(&g_seqs[i], 0, sizeof g_seqs[i]);
            g_seqs[i].used = 1;
            g_seqs[i].volume = 127;
            printf("ail: allocate_sequence_handle -> %d\n", i);
            return &g_seqs[i];
        }
    }
    return NULL;
}

static void host_AIL_init_sample(void *h)
{
    ail_sample *s = sample_of(h);
    if (!s)
        return;
    s->len = 0;
    s->addr = NULL;
    s->loop_count = 1;
    s->type = 0;                            /* DIG_F_MONO_8 */
    /* Stop, but do not leave a voice running: AIL_init_sample is called on
     * every single sound effect (svc_play_sfx: init/addr/loop/start), and
     * stopping a voice that already finished must not count as a cut - it
     * does not, sample_stop only counts a voice that was still sounding
     * (round 33 33.7 counted the same thing through waveOutReset). */
    sample_stop(s);
}

static void host_AIL_set_sample_address(void *h, void *start, uint32_t len)
{
    ail_sample *s = sample_of(h);
    if (!s)
        return;
    s->addr = (uint8_t *)start;
    s->len = len;
    if (s->len) {
        log_pcm_stats("set_sample_address", s->addr, s->len);
        if (!s->dumped) {
            s->dumped = 1;                  /* one copy per distinct sample */
            dump_blob("ail_smp", s->addr, s->len);
        }
    }
}

static void host_AIL_set_sample_loop_count(void *h, int32_t count)
{
    ail_sample *s = sample_of(h);
    if (s)
        s->loop_count = count;
}

static void host_AIL_start_sample(void *h)
{
    ail_sample *s = sample_of(h);
    if (s)
        sample_play(s);
}

static void host_AIL_stop_sample(void *h)
{
    ail_sample *s = sample_of(h);
    if (!s)
        return;
    sample_stop(s);
    printf("ail: stop_sample\n");
}

/* AIL_set_sample_type(handle, type, flags) - type encodes channels+bits
 * (0=mono/8, 1=mono/16, 2=stereo/8, 3=stereo/16). FDPS computes it from the
 * WAV header it is about to play, so this is what decides the format the
 * mixer is given; FD2 never calls it and keeps the AIL defaults. */
static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* AIL_init_sequence is handed the address of the XDIR directory inside
 * FDMUS.DAT (its FORM header covers only the 22-byte catalogue), and
 * sequence_num selects which song to use. This mirrors AIL's own walker
 * (obj0 sub_42520): descend FORM/CAT wrappers until the block type is XMID,
 * then pick the sequence_num-th "FORM XMID" child of that CAT. */
static const uint8_t *locate_xmid(const uint8_t *base, int index, uint32_t *out_len)
{
    const uint8_t *p = base;
    uint32_t blk;

    if (!base)
        return NULL;
    for (;;) {
        if (memcmp(p, "FORM", 4) && memcmp(p, "CAT ", 4))
            return NULL;
        blk = 8 + be32(p + 4);
        if (!memcmp(p + 8, "XMID", 4))
            break;
        p += blk;
    }
    if (!memcmp(p, "FORM", 4)) {            /* the blob is a single song */
        if (index != 0)
            return NULL;
        *out_len = 8 + be32(p + 4);
        return p;
    }
    {                                       /* CAT: index-th XMID child */
        const uint8_t *end = p + blk;
        const uint8_t *c = p + 12;
        int n = index;
        while (c + 8 <= end) {
            uint32_t clen = 8 + be32(c + 4);
            if (!memcmp(c + 8, "XMID", 4)) {
                if (n == 0) {
                    *out_len = clen;
                    return c;
                }
                n--;
            }
            if (!clen)
                break;
            c += clen;
        }
    }
    return NULL;
}

static void host_AIL_init_sequence(void *h, void *start, int32_t sequence_num)
{
    ail_seq *q = seq_of(h);
    const uint8_t *blob = NULL;
    uint32_t len = 0;

    if (!q)
        return;
    q->seq_num = sequence_num;
    q->data = NULL;
    q->len = 0;
    if (!start)
        return;
    blob = locate_xmid((const uint8_t *)start, (int)sequence_num, &len);
    if (!blob || len < 16) {
        printf("ail: init_sequence(seq=%d) - no XMID found from %p\n",
               (int)sequence_num, start);
        return;
    }
    q->data = blob;
    q->len = len;
    printf("ail: init_sequence(seq=%d) base=%p song=%p len=%u head=%c%c%c%c\n",
           (int)sequence_num, start, (const void *)blob, (unsigned)len,
           blob[0], blob[1], blob[2], blob[3]);
    dump_blob("ail_seq", blob, len);
}

static void host_AIL_start_sequence(void *h)
{
    ail_seq *q = seq_of(h);
    if (!q || !q->data)
        return;
    q->playing = 1;
    if (!xmidi_play(q->data, q->len, q->loop_count))
        printf("ail: start_sequence(seq=%d) - playback unavailable\n", (int)q->seq_num);
}

static void host_AIL_stop_sequence(void *h)
{
    ail_seq *q = seq_of(h);
    if (q)
        q->playing = 0;
    xmidi_stop();
}

static void host_AIL_set_sequence_volume(void *h, int32_t volume, int32_t ms)
{
    ail_seq *q = seq_of(h);
    if (q)
        q->volume = volume;
    printf("ail: set_sequence_volume(%d, over %d ms)%s\n", (int)volume, (int)ms,
           ms > 0 ? " - ramped" : "");
    /* The built-in synthesiser streams its output, so the ramp lands on the
     * next slice and really takes `ms` milliseconds (AIL semantics). The MIDI
     * Mapper backend gets the level as a channel-volume message instead. */
    synth_set_sequence_volume((int)volume, (int)ms);
    xmidi_set_volume((int)volume);
}

static void host_AIL_set_sequence_loop_count(void *h, int32_t count)
{
    ail_seq *q = seq_of(h);
    if (q)
        q->loop_count = count;
}


/* ------------------------------------------------- public boundary (C) ---
 *
 * port3's game calls the 16 AIL entries through src/lib/ail.h.  The bodies
 * above are the same host_AIL_* implementations the port/ host proved out;
 * these wrappers only reconcile the int32_t <-> void* handle spelling. */
#include "../src/lib/ail.h"

int32_t AIL_startup(void)                        { return host_AIL_startup(); }
void    AIL_shutdown(void)                       { host_AIL_shutdown(); }

void   *AIL_install_DIG_INI(void)                { return (void *)(intptr_t)host_AIL_install_DIG_INI(); }
void   *AIL_install_MDI_INI(void)                { return (void *)(intptr_t)host_AIL_install_MDI_INI(); }

void   *AIL_allocate_sample_handle(void *dig)    { return host_AIL_allocate_sample_handle((int32_t)(intptr_t)dig); }
void   *AIL_allocate_sequence_handle(void *mdi)  { return host_AIL_allocate_sequence_handle((int32_t)(intptr_t)mdi); }

int32_t AIL_init_sample(void *h)                 { host_AIL_init_sample(h); return 0; }
int32_t AIL_set_sample_address(void *h, const void *start, uint32_t len)
                                                 { host_AIL_set_sample_address(h, (void *)(uintptr_t)start, len); return 0; }
int32_t AIL_set_sample_loop_count(void *h, int32_t loops)
                                                 { host_AIL_set_sample_loop_count(h, loops); return 0; }
int32_t AIL_start_sample(void *h)                { host_AIL_start_sample(h); return 1; }
int32_t AIL_stop_sample(void *h)                 { host_AIL_stop_sample(h); return 0; }

void   *AIL_init_sequence(void *h, void *start, int32_t seq_num)
                                                 { host_AIL_init_sequence(h, start, seq_num); return h; }
void    AIL_start_sequence(void *h)              { host_AIL_start_sequence(h); }
void    AIL_stop_sequence(void *h)               { host_AIL_stop_sequence(h); }
void    AIL_set_sequence_volume(void *h, int32_t volume, int32_t ramp_ms)
                                                 { host_AIL_set_sequence_volume(h, volume, ramp_ms); }
int32_t AIL_set_sequence_loop_count(void *h, int32_t count)
                                                 { host_AIL_set_sequence_loop_count(h, count); return 0; }

/* ---- host knobs (CLI / kernel) ---- */
void fd2_ail_set_dump_dir(const char *dir)
{
    if (dir && dir[0])
        strncpy(g_dump_dir, dir, sizeof g_dump_dir - 1);
}

void ail_set_format(uint32_t sample_rate, int bits, int stereo)
{
    if (sample_rate >= 4000 && sample_rate <= 192000)
        g_rate = sample_rate;
    if (bits == 8 || bits == 16)
        g_bits = bits;
    g_stereo = stereo ? 1 : 0;
}
