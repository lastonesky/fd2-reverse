/* figure_view.h - unit figure/animation view helpers (original 0x30E25,
 * 0x2F4D4, 0x30E9D).
 *
 * The still-pending driver sub_2CF30 (0x2CF30) shows a party member's animated
 * figure (FIGANI.DAT) over a scrolling background (BG.DAT) with an alternating
 * portrait strip (TAI.DAT).  Its three closure-ready helpers live here; the
 * fourth callee, sub_2E95B (0x2E95B), only re-reads FD2.TMP through the CRT
 * fread wrapper sub_373CA and is therefore left out of this batch (library
 * zone, see module-map.md).
 *
 *   0x30E25  figure_bg_pick    pick the BG.DAT sub-image index for the shown
 *                              unit: start from byte_52470[dword_53C03] and,
 *                              scanning `units[count-1..0]`, replace it with the
 *                              record's map-cell byte 6 whenever the unit has no
 *                              "skip" flag (rec_skip) or the running pick is 0.
 *   0x2F4D4  figure_bg_scroll  scroll the three BG.DAT strips dword_5413F[0..2]
 *                              (rotating) through the 640-stride work buffer and
 *                              publish 320x200 windows to the screen, then stamp
 *                              the figure and the status panel in the middle.
 *   0x30E9D  figure_play       play one unit's figure group frame by frame: for
 *                              every sub-sprite of `figure` draw `frame` of
 *                              `base` (or vice versa, depending on record +6),
 *                              compositing the 640-stride buffer to VGA; flash
 *                              the DAC with the mode's palette entry and wait a
 *                              tick per frame.
 *
 * ABI is cdecl; argument order and the constants were decoded from the offline
 * disassembly (analysis/cseg01_disasm.txt), which is authoritative.  Every real
 * callee is an already-restored asset: map_cell_info (0x12E38), rec_skip
 * (0x1F183), rec_sub_table5 (0x1CA89), ui_status_panel_at (0x2FACD), rle_decode
 * (0x4E98D), rle_decode_lut (0x4E8D3), res_blit (0x2EB9F), gfx_copy_rows
 * (0x11EB0), svc_play_sfx (0x25A96), svc_wait_ticks (0x17AA9), the CRT
 * outp/j___delay, plus memcpy/memset.  The Watcom stack probe (`push size; call
 * 0x3702F`) is not a callee.  Hand-restored, compile-verified, no differential
 * harness yet (unverified).
 */
#ifndef GAME_FIGURE_VIEW_H
#define GAME_FIGURE_VIEW_H

#include <stdint.h>

/* 0x30E25 - return the BG.DAT sub-image index to use for the unit list
 * `units[0..count-1]`.  `indices` are record numbers into the 80-byte table
 * dword_53A45.  Scans from the last entry down to the first. */
int figure_bg_pick(int count, const uint8_t *units);

/* 0x2F4D4 - scroll the three BG.DAT strips dword_5413F in `tmp` (a 640-stride
 * 0x1F400 buffer) and copy 320x200 windows to the screen; `screen` is the
 * 0xFA00 320-stride buffer, `bg` the current background sub-image and `figure`
 * the LMI stream stamped by res_blit.  `unit` selects the status panel. */
void figure_bg_scroll(int unit, const void *figure, void *screen, void *tmp,
                      const void *bg);

/* 0x30E9D - play the figure group `figure` against the looping group `base` for
 * record `unit`; `mode` selects the palette flash / posture branch, `tmp` is the
 * 640-stride work buffer, `screen` the 320-stride buffer, `bg`/`tai` the two
 * DAT strips drawn on a posture change.  Leaves the last return value in eax;
 * all callers ignore it. */
void figure_play(int unit, int mode, const void *figure, const void *base,
                 void *tmp, void *screen, const void *bg, const void *tai);

#endif /* GAME_FIGURE_VIEW_H */
