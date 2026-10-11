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
 *   0x22253  map_units_move_play     driver of the three phases above (also
 *                                    set the record's cell and load the sfx).
 *   0x2218A  map_units_move_deploy   deploy one unit onto its cell and slide
 *                                    the view after it.
 *
 * All are hand-restored and compile-verified; no differential harness yet.
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

/* 0x22253 - play the whole walk/reveal/settle move at cell (at_x, at_y) for
 * record `rec`, whose cell bytes are set to (rec_x, rec_y) in the process. */
void map_units_move_play(int rec, int rec_x, int rec_y, int at_x, int at_y);

/* 0x2218A - deploy the unit `target[0]` onto its cell, slip one
 * rec_sub_table5(actor, 23) entry, book the move cost into dword_53EC8, play
 * the walk from off-map to the unit's cell, slide the view to qword_51CF9 and
 * replay the walk onto that cell.  The middle argument is unused but keeps the
 * call site's 3-argument ABI. */
void map_units_move_deploy(int actor, int unused, const uint8_t *target);

/* Call sites keep the IDA name. */
#define sub_1DB65 map_units_animate
#define sub_22253 map_units_move_play

#endif /* FD2_GAME_MAP_UNITS_H */
