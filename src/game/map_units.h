/* map_units.h - unit sprites inside the map view: the idle blink animation
 * (original 0x1DB65) and the three drawing phases of the unit-move animation
 * (original 0x22470, 0x22547, 0x22656, driven by 0x22253).
 *
 *   0x1DB65  map_units_animate       collect the visible records, mark them
 *                                    and blink their sprites over 13+6+6
 *                                    frames via a 0x25680 scratch buffer.
 *   0x22470  map_units_move_walk     walk-in: 11 atlas frames (dword_53AD1
 *                                    indices 114..124) blitted into the
 *                                    destination cell.
 *   0x22547  map_units_move_reveal   reveal pass around (cx, cy): 6 frames of
 *                                    dword_53A6D, returns the last frame.
 *   0x22656  map_units_move_settle   reveal pass around (cx, cy): 10 frames of
 *                                    dword_53A6D.
 *
 * All four are hand-restored and compile-verified; no differential harness yet.
 */
#ifndef FD2_GAME_MAP_UNITS_H
#define FD2_GAME_MAP_UNITS_H

/* 0x1DB65 - collect the unit records visible in the current view, mark them
 * and blink their sprites over the map view. */
void map_units_animate(void);

/* 0x22470 - blit the walking frames of `scratch` into the cell at
 * (dst_x, dst_y), publishing each frame to 0xA0504. */
void map_units_move_walk(int dst_x, int dst_y, void *scratch);

/* 0x22547 - blit `sprite` into `scratch` at (dst_x, dst_y) and play the six
 * reveal frames of the dword_53A6D palette band around (cx, cy); returns the
 * last frame sprite (feeds the next reveal_band). */
const void *map_units_move_reveal(int dst_x, int dst_y, void *scratch,
                                  int cx, int cy, const void *sprite);

/* 0x22656 - play the ten reveal frames of the dword_53A6D band around
 * (cx, cy).  Only `scratch`, `cx` and `cy` are read; the other four keep the
 * driver's 7-argument ABI.  Returns the last tick reading. */
int map_units_move_settle(int unused1, int unused2, void *scratch,
                          int cx, int cy, const void *unused3, int unused4);

/* Call sites keep the IDA name. */
#define sub_1DB65 map_units_animate

#endif /* FD2_GAME_MAP_UNITS_H */
