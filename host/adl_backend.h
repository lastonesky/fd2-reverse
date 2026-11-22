/* adl_backend.h - optional libADLMIDI music backend (branch experiment).
 *
 * The default music backend is host/synth.c: our own OPL2 renderer driving two
 * emu8950 chips.  This file instead hands the *original XMI blob* to
 * libADLMIDI (Nuked-OPL3 core + its embedded AIL/Fat Man 2-op bank), i.e. the
 * exact engine that produced build/oplbench's `r1..r15.wav` reference renders.
 *
 * It plugs into the same seam as synth.c: xmidi_play() gets the XDIR/XMI bytes
 * from FDMUS.DAT, and the mixer pulls mono 16-bit frames from a callback
 * (host/audio.h).  ADLMIDI renders exactly the frames asked for, so streaming
 * it 4096 frames at a time keeps the sequencing timing identical to
 * adl_play()'s offline render.
 *
 * Build: `make engine ADLMIDI=1` (see host/Makefile) - needs the libADLMIDI
 * tree; it is not vendored in this repository yet.
 */
#ifndef FD2_ADL_BACKEND_H
#define FD2_ADL_BACKEND_H

#include <stdint.h>

/* Start playing `blob` (the XMI the game handed AIL). loop != 0 makes the
 * song repeat. Returns 1 on success, 0 if libADLMIDI could not open it. */
int  adl_backend_play(const uint8_t *blob, uint32_t len, int loop);

/* Stop and release the music stream (the device stays initialised). */
void adl_backend_stop(void);

/* AIL_set_sequence_volume: 0..127, ramped over `ms` (0 = immediate). Also
 * opens the mixer's start gate, like synth_set_sequence_volume() does. */
void adl_backend_set_volume(int volume, int ms);

/* Host --volume (0..100 %), multiplied into the rendered slice. */
void adl_backend_set_master_volume(int percent);

/* 1 while a stream is installed in the mixer. */
int  adl_backend_is_playing(void);

/* Number of chips (1 = 18 melodic voices + 5 rhythm channels, the reference
 * configuration).  Mirrors --opl-channels for the emu8950 path in the log. */
void adl_backend_set_chips(int chips);
int  adl_backend_chips(void);

#endif /* FD2_ADL_BACKEND_H */
