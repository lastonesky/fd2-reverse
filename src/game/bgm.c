/* bgm.c - background music entry (source translation of 0x25977).
 *
 * The only "change track" entry point (32 call sites). It keeps the current
 * track number in byte_51A11, reloads FDMUS.DAT entry `track` when it changes,
 * and hands the buffer to AIL. The 2 s fade-in on every change is the game's
 * own doing (docs/rounds/06-audio-fade.md): 0 ms of silence, 127 over 2000 ms -
 * except tracks 16/17 (the victory fanfares), which go to full volume at once.
 *
 * Services are called through their **original addresses** on purpose:
 *   - in the running host those AIL entries are already patched to src/ail.c
 *     and 0x111BA to src/game/res.c, so this is the same code path the
 *     machine-code callers take;
 *   - src/bgmcheck.c hooks exactly these addresses to stubs, so the same C
 *     can be compared against the original 0x25977 call by call.
 * 0x3666C is DPMI "lock linear region" (it calls int 0x31 AX=0600 via
 * 0x365DA); in a flat host it is a no-op service, kept as a call for fidelity.
 *
 * Verified byte-behaviour by src/bgmcheck.c (event sequence + globals).
 */
#include "bgm.h"

#include <stdint.h>
#include <stddef.h>
#include "fd2_game.h"   /* de-guest C4: restored cross-module decls */
#include "../include/fd2_globals.h"

/* --- the game's data segment (IDA names kept) --------------------------- */

/* --- service entry points ---------------------------------------------- */
typedef void  *(*res_load_fn)(const char *name, void *old_buffer, int index);
typedef void   (*lock_fn)(void *buf, uint32_t len);
typedef void  *(*ail_init_seq_fn)(void *h, void *start, int32_t seq_num);
typedef void   (*ail_seq_fn)(void *h);
typedef void   (*ail_vol_fn)(void *h, int32_t volume, int32_t ms);
typedef int32_t (*ail_loop_fn)(void *h, int32_t count);


void bgm_play(int track, int loop_count)
{
    /* The original zero-extends the stored byte and compares it against the
     * full 32-bit argument (`movzx eax,byte_51A11; cmp eax,arg_0`), so
     * track==-1 is *never* "already playing" even when the byte is 0xFF. */
    if ((unsigned)byte_51A11 == (unsigned)track)
        return;                                  /* already playing it      */
    byte_51A11 = (uint8_t)track;

    if (track == -1) {
        AIL_set_sequence_volume(dword_53ED0, 0, 4000);        /* fade the current one out */
        return;
    }
    if (!byte_53EF0)
        return;                                  /* music switched off       */

    if (dword_53EE0)
        AIL_stop_sequence(dword_53ED0);

    dword_53EE0 = res_load("FDMUS.DAT", dword_53EE0, track);
    fd2_mem_lock(dword_53EE0, dword_53BFF);          /* res_load set 53BFF       */

    AIL_init_sequence(dword_53ED0, dword_53EE0, 0);
    /* The machine code sets the loop count after AIL_start_sequence (0x25A4x).
     * The host AIL layer starts rendering from the current count, so a one-shot
     * fanfare (16/17 with count 1) following a looping BGM (count 0) used to
     * start with count 0 and loop forever.  AIL applies the count to the
     * running sequence; setting it before start is the same observable
     * behaviour and lets the synth pick the correct one-shot/loop mode. */
    AIL_set_sequence_loop_count(dword_53ED0, loop_count);
    AIL_start_sequence(dword_53ED0);

    if (byte_51E61) {
        if (track == 16 || track == 17) {
            AIL_set_sequence_volume(dword_53ED0, 127, 0);     /* fanfare: full volume now */
        } else {
            AIL_set_sequence_volume(dword_53ED0, 0, 0);       /* silence, then            */
            AIL_set_sequence_volume(dword_53ED0, 127, 2000);  /* ramp up over 2 s         */
        }
    } else {
        AIL_set_sequence_volume(dword_53ED0, 0, 0);
    }
}
