/* map_scroll.h - scroll the world-map view one cell at a time while animating
 * the record that moves with it (original 0x12EAA, 0x1300D, 0x13185, 0x13315,
 * 0x13488, 0x13A44); plus 0x15DF3, the inverse query that locates a tagged map
 * cell.
 *
 * 0x12EAA/0x1300D/0x13185/0x13315 are the four directions down/left/up/right:
 * each nudges the record at dword_53A45[rec] one cell, walks the view origin
 * (dword_53AA9/dword_53AAD for x/y, mirrored in dword_53AB1/dword_53AB5) with
 * it, and over seven animation frames pings the record, steps the palette and
 * frame counters, re-renders the 13x9 (14-tile for one branch) tile window and
 * copies the 312x192 map viewport to the screen. 0x13488 walks a byte list of
 * direction codes (0=down, 1=left, 2=up, 3=right); 0x13A44 re-reads the map
 * cell the view origin now points at and latches its record tag into
 * dword_51A8F. 0x15DF3 scans the whole map for the cell whose record tag
 * equals a given index (used by the record-move driver 0x13A9F to find a
 * rec[61] target), complementing 0x13A44.
 *
 * ABI = cdecl (last push = first argument). The offline disassembly is the
 * authority for the constants and the branch conditions; the Watcom stack
 * probe (`push size; call 0x3702F`) is dropped. Hand-restored,
 * compile-verified, no differential harness yet.
 */
#ifndef GAME_MAP_SCROLL_H
#define GAME_MAP_SCROLL_H

#include <stdint.h>

/* 0x12EAA - move record `rec` one cell down (record state 0). */
void map_scroll_down(int rec);

/* 0x1300D - move record `rec` one cell left (record state 1). */
void map_scroll_left(int rec);

/* 0x13185 - move record `rec` one cell up (record state 2). */
void map_scroll_up(int rec);

/* 0x13315 - move record `rec` one cell right (record state 3). */
void map_scroll_right(int rec);

/* 0x13488 - run a direction-code list of `count` bytes at `dirs`. */
void map_scroll_path(int rec, const uint8_t *dirs, int count);

/* 0x13A44 - latch the record tag of the map cell at (x, y) into dword_51A8F
 * (only when the cell is unflagged and its record tag byte equals `which`). */
void map_scroll_cell_tag(int x, int y, int which);

/* 0x15DF3 - scan the map for the first cell whose terrain-table byte carries
 * bit 0x20 (and not 0x40) and whose record tag word equals `tag`; write its
 * coordinates as out[0]=x, out[1]=y and return 0, else return -1. `out` is
 * written only on success. */
int map_find_tag_cell(int tag, uint8_t *out);

#endif /* GAME_MAP_SCROLL_H */
