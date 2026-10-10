/* mosaic_reveal.h - party-portrait mosaic reveal transition (original 0x4EB59,
 * 0x1F42D, 0x1F1CC, 0x1F30A).
 *
 * One "character joins / party changes" transition on the map screen.  The live
 * 0xFA00-byte frame is copied to a scratch buffer, then re-composited frame by
 * frame while a palette fade runs:
 *
 *   0x4EB59  block-replicate (mosaic / pixelate) blit.  The 312x192 window at
 *            src+0x504 is re-sampled on a `block`x`block` grid and written to
 *            dst+0x504: block == 1 is a 1:1 copy, larger values coarsen.
 *   0x1F42D  one transient frame of the two sliding portrait strips: save the
 *            background under both strips, blit sub-image `index` in the left
 *            strip and sub-image 81 in the right strip split by `offset`, copy
 *            the 312x192 window to the VGA scratch page 0xA0504, wait a tick,
 *            then restore the background.
 *   0x1F1CC  "out": mosaic the live frame with block 1..16 while fading the
 *            palette down, compositing sub-image `index` over every frame.
 *   0x1F30A  "in": mosaic with block 17..1 while fading back up, then redraw
 *            the map window and refresh the portraits.
 *
 * True ABI is cdecl.  The scratch buffer lives on the game heap (guest_mem.h);
 * every other dependency is an already-verified asset: res_draw_subimage
 * (0x15F0E), gfx_copy_rows (0x11EB0), dlg_snap_restore (0x15E71),
 * svc_wait_ticks (0x17AA9), pal_fade_range (0x11D40), map_render_view
 * (0x11EEE), dlg_portraits_refresh (0x127A9), kbd_flush (0x4E381).  No
 * differential harness yet (unverified).
 */
#ifndef GAME_MOSAIC_REVEAL_H
#define GAME_MOSAIC_REVEAL_H

#include <stdint.h>

/* 0x4EB59 - block-replicate 312x192 window, block x block cells. */
void mosaic_blit(int block, uint8_t *dst, uint8_t *src);

/* 0x1F42D - one frame of the two sliding portrait strips at split `offset`. */
void reveal_slide_draw(int offset, int index);

/* 0x1F1CC - mosaic coarsen (1..16) + palette fade down. */
void reveal_mosaic_out(int index);

/* 0x1F30A - mosaic refine (17..1) + palette fade up + map/portrait restore. */
void reveal_mosaic_in(int index);

#endif /* GAME_MOSAIC_REVEAL_H */
