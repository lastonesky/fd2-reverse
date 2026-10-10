/* fb_slide.h - screen-buffer slide compositor helpers (original 0x182AD ..
 * 0x18409).
 *
 * These four routines are the low-level pieces of the 12-step dialogue/map box
 * transition: they memmove rows of a 320x200 (0xFA00-byte) frame, copying a
 * shifted band out of a source buffer into a scratch buffer, and 0x18409
 * assembles one step of the twelve. Callers are the message-portrait code
 * (0x17E0B / 0x1B932 / 0x1BFFE build three 0xFA00 scratch buffers and call
 * 0x18409 twelve times), so nothing here persists state of its own.
 *
 * Original addresses:
 *   0x182AD  band A (86 rows, 0x8C0 -> 0x8C5, byte count 86 clipped by x)
 *   0x18312  band B (0x56 rows, 0x5C -> 0x91C, 0xDF-byte rows)
 *   0x1839B  band C (<=102 rows, 0x5 -> 0x7585, 0x136-byte rows)
 *   0x18409  one transition step: memmove(a2,a4,0xFA00) + the three bands +
 *            memmove(0xA0000, a2, 0xFA00)
 *
 * True ABI = cdecl: (int x, char *dst, char *src[, char *saved]). The `x`
 * argument is the per-step offset (0..11); negative values shift the band and
 * clip its length. All row strides are 320. The self-test/compile gate covers
 * these; they have no differential harness yet (unverified).
 */
#ifndef GAME_FB_SLIDE_H
#define GAME_FB_SLIDE_H

/* 0x182AD */
void fb_slide_band_a(int x, char *dst, char *src);

/* 0x18312 */
void fb_slide_band_b(int x, char *dst, char *src);

/* 0x1839B */
void fb_slide_band_c(int x, char *dst, char *src);

/* 0x18409 */
void fb_slide_step(int x, char *dst, char *src, char *saved);

#endif /* GAME_FB_SLIDE_H */
