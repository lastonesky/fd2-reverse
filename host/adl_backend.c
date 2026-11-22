/* adl_backend.c - optional libADLMIDI music backend (see adl_backend.h).
 *
 * Why this exists: host/synth.c is a hand-written OPL2 renderer that *models*
 * libADLMIDI's sequencing.  When the goal is "does it sound like the reference
 * WAVs", the shortest path is to run the reference engine itself.  This file
 * is that path, kept out of the default build (`make engine ADLMIDI=1`) so the
 * dependency-free build stays the default.
 *
 * Threading: exactly like synth.c - the mixer's device callback pulls frames
 * from adl_fill() while holding the recursive audio lock, and the game thread
 * installs/removes the source under the same lock (audio.h documents it).
 *
 * Licensing note: libADLMIDI is LGPL-2.1. This file only calls its public C
 * API (`adl_*`); nothing from its sources is copied or modified.
 */

#include "adl_backend.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "platform.h"

#include "adlmidi.h"

#define ADL_CHUNK_FRAMES 2048          /* scratch for one adl_play() call */

static struct ADL_MIDIPlayer *g_dev;
static uint32_t        g_rate;
static int             g_chips = 1;
static int16_t         g_buf[ADL_CHUNK_FRAMES * 2];   /* interleaved L/R */

/* Same volume model as synth.c: AIL's sequence level (0..127) x host --volume,
 * with the ramp AIL performs over `ms`. */
static double   g_seq_from = 127.0;
static double   g_seq_to   = 127.0;
static uint64_t g_ramp_t0;
static int      g_ramp_ms;
static uint64_t g_arm_t0;
static int      g_master_pct = 100;

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

static double stream_gain(void)
{
    return ((double)g_master_pct / 100.0) * (seq_volume_now() / 127.0);
}

/* One mixer slice: ask ADLMIDI for `nframes` stereo frames and fold them to
 * the mono int16 the mixer wants.  adl_play() renders ahead of wall clock but
 * consumes exactly the requested sample count from the sequencer, so the
 * timing is that of the file, not of the call rate. */
static unsigned adl_fill(int16_t *dst, unsigned nframes)
{
    unsigned done = 0;
    double   g    = stream_gain();

    while (done < nframes) {
        unsigned want = nframes - done;
        int      got;
        int      n;

        if (want > ADL_CHUNK_FRAMES)
            want = ADL_CHUNK_FRAMES;
        if (!g_dev)
            break;
        got = adl_play(g_dev, (int)(want * 2), g_buf);
        if (got <= 0)
            break;                       /* end of song (no loop): silence   */
        n = got / 2;
        for (int i = 0; i < n; ++i) {
            int l = g_buf[2 * i], r = g_buf[2 * i + 1];
            int v = (int)lround((double)(l + r) * 0.5 * g);
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            dst[done + (unsigned)i] = (int16_t)v;
        }
        done += (unsigned)n;
    }
    if (done < nframes)
        memset(dst + done, 0, (nframes - done) * sizeof(int16_t));
    return nframes;
}

static int dev_open(void)
{
    if (g_dev)
        return 1;
    g_rate = audio_rate();
    if (!g_rate)
        g_rate = 44100;
    g_dev = adl_init((long)g_rate);
    if (!g_dev) {
        printf("adlmidi: adl_init(%u) failed\n", (unsigned)g_rate);
        return 0;
    }
    /* Same call order as build/oplbench/render_wav.cpp, which produced the
     * reference WAVs: chips -> loop -> emulator -> bank -> open. */
    adl_setNumChips(g_dev, g_chips);
    adl_setLoopEnabled(g_dev, 0);
    adl_switchEmulator(g_dev, ADLMIDI_EMU_NUKED);
    if (adl_setBank(g_dev, 0) != 0) {    /* 0 = AIL (The Fat Man 2-op)       */
        printf("adlmidi: bank 0 unavailable: %s\n", adl_errorInfo(g_dev));
        adl_close(g_dev);
        g_dev = NULL;
        return 0;
    }
    printf("adlmidi: backend up (libADLMIDI, bank 0 = AIL Fat Man 2-op, "
           "%d chip%s, %u Hz, %s)\n",
           g_chips, g_chips == 1 ? "" : "s", (unsigned)g_rate,
           adl_chipEmulatorName(g_dev));
    return 1;
}

int adl_backend_play(const uint8_t *blob, uint32_t len, int loop)
{
    if (!blob || len < 16)
        return 0;
    if (!dev_open())
        return 0;

    adl_setLoopEnabled(g_dev, loop ? 1 : 0);
    if (adl_openData(g_dev, blob, (unsigned long)len) < 0) {
        printf("adlmidi: openData(%u bytes) failed: %s\n",
               (unsigned)len, adl_errorInfo(g_dev));
        return 0;
    }

    printf("adlmidi: playing %.3f s, loop=%d, %u events\n",
           adl_totalTimeLength(g_dev), loop ? 1 : 0,
           (unsigned)adl_trackCount(g_dev));

    g_arm_t0 = plat_now_ms();
    audio_hold_music();
    audio_set_music(adl_fill);
    return 1;
}

void adl_backend_stop(void)
{
    if (audio_music_src() == adl_fill)
        audio_set_music(NULL);
}

void adl_backend_set_volume(int volume, int ms)
{
    if (volume < 0)   volume = 0;
    if (volume > 127) volume = 127;
    g_seq_from = seq_volume_now();
    g_seq_to   = (double)volume;
    g_ramp_ms  = ms > 0 ? ms : 0;
    g_ramp_t0  = plat_now_ms();

    if (audio_music_held()) {
        audio_release_music();
        printf("adlmidi: stream released after %lu ms -> sequence volume "
               "%.0f/127, gain %.3f\n",
               (unsigned long)(plat_now_ms() - g_arm_t0),
               seq_volume_now(), stream_gain());
    }
}

void adl_backend_set_master_volume(int percent)
{
    if (percent < 0)   percent = 0;
    if (percent > 100) percent = 100;
    g_master_pct = percent;
}

int adl_backend_is_playing(void)
{
    return audio_music_src() == adl_fill;
}

void adl_backend_set_chips(int chips)
{
    if (chips < 1) chips = 1;
    if (chips > 4) chips = 4;
    g_chips = chips;
    if (g_dev)
        adl_setNumChips(g_dev, g_chips);
}

int adl_backend_chips(void)
{
    return g_chips;
}
