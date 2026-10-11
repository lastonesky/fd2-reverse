/* fd2_audio.h - host-side entry points of the reused AIL replacement layer.
 *
 * The game-facing contract stays src/lib/ail.h (16 entries); this header
 * exposes only the extra knobs the host kernel / CLI needs.  See
 * host/fd2_audio.c and port/docs/AUDIO.md.
 */
#ifndef FD2_HOST_AUDIO_H
#define FD2_HOST_AUDIO_H

#include <stdint.h>

/* Override the AIL default sample format (8-bit mono 11025 Hz). */
void ail_set_format(uint32_t sample_rate, int bits, int stereo);

/* Master output volume, 0..100 (100 = the game's own level). */
void ail_set_master_volume(int percent);

/* Where to dump the first PCM sample / XMIDI blobs for format analysis. */
void fd2_ail_set_dump_dir(const char *dir);

#endif /* FD2_HOST_AUDIO_H */
