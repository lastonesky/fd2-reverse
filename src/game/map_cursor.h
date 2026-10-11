/* map_cursor.h - on-map direction-patch animation (original 0x175A9/0x17643/
 * 0x1741C/0x176B4).
 *
 * The five drivers 0x16F55/0x1728C/0x18D8C/0x19DF7/0x1BBDC show a 4x4 tile
 * patch on the map view while the player picks a direction.  Each of them
 * builds two 4-dword index arrays (copies of 0x51E9F and 0x53EF2), then calls
 *
 *   0x1741C  paint the patch opening   (called before the key loop 0x177FC)
 *   0x176B4  paint the patch closing   (called after it, then kbd_flush)
 *   0x177FC  map_cursor_dir_select     the busy-wait key loop + arrow handling
 *   0x17898  map_cursor_dir_run        one loop iteration set (runs inside it)
 *   0x179D5  map_cursor_dir_draw       the four direction sprites per frame
 *
 * Both painters use the same three primitives:
 *
 *   0x175A9  map_cursor_window_save     stash the 72x72-byte block the patch
 *                                       is stamped over (heap block dword_53A71)
 *   0x17643  map_cursor_window_restore  copy that block back
 *   0x4ED34  gfx_blit_transparent       stamp one tile of the patch
 *
 * The block sits on the map view (dword_53A49 + 0x8088, stride 456) at
 * 24*(dword_53AB9-1) + 10944*(dword_53ABD-1) - one map tile up/left of the
 * view origin - while the painters stamp at 24*dword_53AB9 + 10944*dword_53ABD.
 * Per frame the painters restore the block and stamp the four tiles
 * dword_53A89 + dword_53A89[3*a_tiles[j] + 2*b_tiles[j]] at four byte offsets
 * laid out in a 4-dword local table the machine updates by fixed deltas:
 *
 *   0x1741C  { 0x390, 0x390, 0x390, 0x390 }
 *            per frame  -0x8E8, -6, +6, +0x8E8   (offsets diverge)
 *   0x176B4  { -0x23A0, 0x378, 0x3A8, 0x2AC0 }
 *            per frame  +0x8E8, +6, -6, -0x8E8   (offsets converge)
 *
 * ABI is cdecl with exactly two pointers to 4-int arrays, decoded from the
 * pushes at the call sites (e.g. 0x16F91..0x16F97, 0x1BC72..0x1BC77) - the
 * offline disassembly is authoritative, Hex-Rays loses the argument loads of
 * 0x1741C.  Both painters tail-jump to 0x13FCC, an epilogue inside sub_13E9C,
 * so IDA reports a false callee sub_13E9C; the Watcom stack probe
 * (`push size; call 0x3702F`) is likewise not a callee.  Hand-restored,
 * compile-verified, no differential harness yet.
 */
#ifndef GAME_MAP_CURSOR_H
#define GAME_MAP_CURSOR_H

/* 0x175A9 - release the previous stash (dword_53A71, if any), allocate 72x72
 * bytes and copy the map-view block into it.  The block stays allocated until
 * the game tears the view down. */
void map_cursor_window_save(void);

/* 0x17643 - copy the stashed 72x72-byte block back over the map view. */
void map_cursor_window_restore(void);

/* 0x1741C - four opening frames: restore, stamp the patch, redraw the portrait
 * and publish the view.  `a_tiles`/`b_tiles` are the caller's two 4-int index
 * arrays (the patch tile is dword_53A89[3*a_tiles[j] + 2*b_tiles[j]]). */
void map_cursor_patch_expand(const int *a_tiles, const int *b_tiles);

/* 0x176B4 - four closing frames, then a final restore and kbd_flush. */
void map_cursor_patch_collapse(const int *a_tiles, const int *b_tiles);

/* 0x179D5 - draw the four direction-cursor sprites of one animation frame:
 * sprite j is dword_53A89[dword_53A89 + 4*(3*frame_a[j] + 2*frame_b[j])]
 * (the selected direction dword_53C57 adds the dword_53C13 phase), stamped at
 * the map-view origin plus the fixed offset table {-9120,888,936,10944}, then
 * redraw the found record portrait. `frame_a`/`frame_b` are 4-int arrays. */
void map_cursor_dir_draw(const int *frame_a, const int *frame_b);

/* 0x17898 - the key loop: while no key is pending, step the palette/frame
 * animations, render the 13x8 map view, redraw the portraits and cursor, draw
 * the four direction sprites and publish the 312x192 window to the VGA spare
 * page.  Returns the INT 16h scan code (E0/52 -> 1C, 53 -> 01). */
int map_cursor_dir_run(const int *frame_a, const int *frame_b);

/* 0x177FC - interpret one key from map_cursor_dir_run: Esc(01) -> -1,
 * Enter(1C/39) -> 1; arrow keys 48/50/4B/4D move dword_53C57 to 0/3/1/2 when
 * the matching entry of `frame_b` is 0, then return 0.  Hex-Rays loses the
 * second argument (the machine loads ebx = [esp+0xc]); the call sites push two
 * 4-int arrays, so the signature is (frame_a, frame_b). */
int map_cursor_dir_select(const int *frame_a, const int *frame_b);

#endif /* GAME_MAP_CURSOR_H */
