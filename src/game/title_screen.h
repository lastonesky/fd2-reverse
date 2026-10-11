/* title_screen.h - opening / continue-screen helpers (original 0x2DF01,
 * 0x1FF79, 0x1F73F, 0x1F81E).
 *
 * These three are the already-restorable callees of the opening screen 0x1F894
 * (it loads FDOTHER.DAT, scrolls the logo, and asks "continue?" against
 * FD2.SAV):
 *
 *   0x1F73F  title_frame_show  one animation frame: fade out, RLE-decode one
 *                             FDOTHER sub-image to the screen, fade in, wait,
 *                             overlay a 320x200 window of `strip` and fade in;
 *   0x1FF79  title_options_draw blit the 1..3 option bands of `image`,
 *                             highlighting the selected band;
 *   0x2DF01  pal_blend_to     wash DAC entries [first, last) `step`/40 of the
 *                             way toward (r,g,b); driven by the opening screen
 *                             and by the intro (0x2D80D).
 *
 * ABI = cdecl, argument order and constants taken from the offline
 * disassembly. The Watcom stack probe (push size; call 0x3702F) is dropped.
 * Hand-restored, compile-verified, no differential harness yet.
 */
#ifndef GAME_TITLE_SCREEN_H
#define GAME_TITLE_SCREEN_H

/* 0x2DF01 - for each DAC index i in [first, last): outp(0x3C8, i) then one
 * outp(0x3C9, ...) per channel writing
 * `target + (palette[i] - target) * step / 40` (signed division, as the
 * machine's imul/idiv). `step` 40 leaves the palette unchanged, 0 goes all the
 * way to (r, g, b). */
void pal_blend_to(int first, int last, int step, int r, int g, int b);

/* 0x1FF79 - draw `count` option bands (1..3) of `image` (FDOTHER.DAT entry
 * passed by the caller) to the screen; band `selected` is drawn highlighted
 * when 0 <= selected < count, otherwise every band is plain. */
void title_options_draw(void *image, int selected, int count);

/* 0x1F73F - one frame of the opening animation. FDOTHER.DAT entry
 * `image_index` is RLE-decoded to the screen, then the 320x200 window starting
 * at row `row` of the 320-wide `strip` buffer is copied over it; the fades use
 * the palette entry `palette_index`. */
void title_frame_show(int image_index, int palette_index, const void *strip,
                      int row);

/* 0x1F81E - play one ANI.DAT animation `index` for `delay_ms` per frame, then
 * fade the palette out.  When `palette_index` is not -1 the frame buffer is
 * cleared and FDOTHER.DAT entry `palette_index` is (re)loaded into the shared
 * palette copy (dword_53A65) first; the palette subtraction is reset to 0 so
 * the animation starts at full brightness.  The trailing fade is the shared
 * 0x1F882 block (pal_fade_out) that this function tail-falls into. */
void title_ani_play(int index, int delay_ms, int palette_index);

#endif /* GAME_TITLE_SCREEN_H */
