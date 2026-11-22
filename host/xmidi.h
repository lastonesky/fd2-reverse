#ifndef FD2_XMIDI_H
#define FD2_XMIDI_H

#include <stdint.h>

/* Minimal XMIDI (Miles IFF-wrapped MIDI) player built on the Windows MIDI
 * Mapper. The game hands AIL the address of an XDIR directory inside
 * FDMUS.DAT (AIL_init_sequence(handle, address, sequence_num)); the real AIL
 * would pass the selected song to an *.MDI driver, which cannot run here, so
 * the sequence is interpreted directly.
 *
 * XMI's time base is 60 ticks per second; see xmidi.c for the event-stream
 * encoding notes. */

int  xmidi_play(const uint8_t *blob, uint32_t len, int loop_count);
void xmidi_stop(void);
void xmidi_set_volume(int volume);      /* 0..127, applied as channel volume */
void xmidi_set_tick_rate(int ticks_per_second);

/* Play a middle-C test tone once (diagnostic: proves whether the system
 * synthesiser actually produces sound). */
void xmidi_set_test(int on);

/* 0 = play through the Windows MIDI Mapper, 1 (default) = render with the
 * built-in software synthesiser in synth.c and stream it into the software
 * mixer (src/audio.h), 2 = hand the XMI to libADLMIDI (host/adl_backend.c;
 * only present in `make engine ADLMIDI=1` builds - returns 0 otherwise, in
 * which case the backend is left unchanged). */
int  xmidi_set_backend(int backend);

/* --midi-backend=winmidi|opl2|adlmidi. Returns 0 for an unknown/absent name. */
int  xmidi_set_backend_name(const char *name);
int  xmidi_backend(void);
int  xmidi_is_playing(void);

#endif /* FD2_XMIDI_H */
