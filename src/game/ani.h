/* ani.h - ANI.DAT animation-frame command decoder (original 0x36FD3 .. 0x36FF4).
 *
 * 0x20421 plays the ANI.DAT container: for every frame it mallocs a 768-byte
 * palette, binds the two output buffers and runs a short command stream.
 * The decoder itself is a ten-entry dispatch table (funcs_37012, the dword
 * table at 0x5276A whose entries are the 0x36E3D..0x36FAC family):
 *
 *   ani_target_set(size, screen, palette)   bind the outputs (0x36FD3)
 *   ani_run_frame(count, cmds)              execute `count` commands (0x36FF4)
 *
 * Each command is a one-byte opcode 0..9 selecting a handler; the handler
 * consumes its own operand bytes from the stream and writes into either the
 * fixed 768-byte palette (0x52766) or the `size`-byte frame buffer (0x52762,
 * e.g. the VGA framebuffer 0xA0000).  The stream cursor is passed through ESI
 * in the original, i.e. it is an implicit in/out argument; in C each handler
 * takes `const unsigned char **sp` and advances *sp past the bytes it read.
 *
 * Opcode map (original address -> handler, target buffer):
 *   0  0x36E3D  ani_pal_fill       palette = one byte, grey
 *   1  0x36E57  ani_pal_copy       palette = 0x300 stream bytes
 *   2  0x36E65  ani_pal_rle        palette = run-length expanded to 0x300
 *   3  0x36EA7  ani_pal_blocks     palette subset blocks {offset=3*b,len=3*b}
 *   4  0x36EE0  ani_fb_fill        frame buffer = one byte
 *   5  0x36F08  ani_fb_copy        frame buffer = `size` stream bytes
 *   6  0x36F24  ani_fb_rle         frame buffer = run-length expanded to `size`
 *   7  0x36F69  ani_fb_pixels      single pixels at 16-bit offsets
 *   8  0x36F82  ani_fb_fill_runs   fill runs {offset,len,value}
 *   9  0x36FAC  ani_fb_copy_runs   copy runs {offset,len} from the stream
 *
 * Hand-restored from the offline disassembly and compile-verified; there is
 * no differential harness yet (unverified).
 */
#ifndef GAME_ANI_H
#define GAME_ANI_H

#include <stdint.h>

/* 0x36FD3 - bind the frame buffer (size + pointer) and the palette buffer. */
void ani_target_set(uint16_t size, void *screen, void *palette);

/* 0x36FF4 - run `count` commands from `cmds` through the funcs_37012 table. */
void ani_run_frame(short count, const unsigned char *cmds);

/* funcs_37012 handlers; each reads its operands from *sp and advances *sp. */
void ani_pal_fill(const unsigned char **sp);      /* 0x36E3D */
void ani_pal_copy(const unsigned char **sp);      /* 0x36E57 */
void ani_pal_rle(const unsigned char **sp);       /* 0x36E65 */
void ani_pal_blocks(const unsigned char **sp);    /* 0x36EA7 */
void ani_fb_fill(const unsigned char **sp);       /* 0x36EE0 */
void ani_fb_copy(const unsigned char **sp);       /* 0x36F08 */
void ani_fb_rle(const unsigned char **sp);        /* 0x36F24 */
void ani_fb_pixels(const unsigned char **sp);     /* 0x36F69 */
void ani_fb_fill_runs(const unsigned char **sp);  /* 0x36F82 */
void ani_fb_copy_runs(const unsigned char **sp);  /* 0x36FAC */

#endif /* GAME_ANI_H */
