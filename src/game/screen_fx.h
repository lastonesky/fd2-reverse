/* screen_fx.h - full-screen transition effects (original 0x26CE4).
 *
 * `screen_curtain(mode)` is a four-step shutter: it snapshots the VGA
 * framebuffer, paints a 74x104 block into each of 20 rows of a scratch copy,
 * then slides the four sub-images of dword_53F66 across the screen in four
 * passes, and finally either leaves the last frame (mode != 0) or restores the
 * snapshot (mode == 0). The four per-tile base offsets live in the read-only
 * table 0x52408 ({-39, -13, 13, 39}) copied out of the image.
 *
 * ABI = cdecl, one argument. Hand-restored, compile-verified, no differential
 * harness yet.
 */
#ifndef GAME_SCREEN_FX_H
#define GAME_SCREEN_FX_H

/* 0x26CE4 */
void screen_curtain(int mode);

#endif /* GAME_SCREEN_FX_H */
