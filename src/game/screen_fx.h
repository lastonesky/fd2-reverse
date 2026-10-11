/* screen_fx.h - full-screen transition effects (original 0x26CE4, 0x27816,
 * 0x278E7).
 *
 * `screen_curtain(mode)` is a four-step shutter: it snapshots the VGA
 * framebuffer, paints a 74x104 block into each of 20 rows of a scratch copy,
 * then slides the four sub-images of dword_53F66 across the screen in four
 * passes, and finally either leaves the last frame (mode != 0) or restores the
 * snapshot (mode == 0). The four per-tile base offsets live in the read-only
 * table 0x52408 ({-39, -13, 13, 39}) copied out of the image.
 *
 * `screen_band_forward`/`screen_band_backward` (0x27816/0x278E7) are the
 * three-step scroll of a 284-byte band at 0xA8FCA against two backing copies
 * (0xA974A / 0xA99CA); later callers reach the effect through 0x27816 (exit)
 * and 0x278E7 (return).
 *
 * ABI = cdecl, one argument. Hand-restored, compile-verified, no differential
 * harness yet.
 */
#ifndef GAME_SCREEN_FX_H
#define GAME_SCREEN_FX_H

/* 0x26CE4 */
void screen_curtain(int mode);

/* 0x27816 */
void screen_band_forward(void);

/* 0x278E7 */
void screen_band_backward(void);

#endif /* GAME_SCREEN_FX_H */
