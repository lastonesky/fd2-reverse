/* map_interact.h - map-view record interaction loop (original 0x115B6 ..
 * 0x14742).
 *
 *   0x14742  map_find_records   collect the records within Manhattan radius
 *            `radius` of (x, y) whose status class matches `filter`, writing
 *            their indices to `out` (if non-NULL); returns the count.
 *   0x12DAC  map_wait_key       wait for a key while stepping the palette and
 *            refreshing the map view on tick changes; returns the scan code.
 *   0x115B6  map_pick_record    the interactive map loop: key -> window
 *            scroll (ev4 steppers), confirm/cancel, record cycling and the
 *            "enter cell" checks.  `mode` (4/5/6) selects the record class and
 *            the `list` holds the candidate record indices, `sel` is the
 *            starting selection (or the cycle length when mode != 6).
 *
 * ABI = cdecl.  Uses ev4 window steppers, map_view_update, pal_anim_step,
 * rec_flag/rec_skip, the 0x4E8A5 table and map_cell_info.  Hand-restored,
 * compile-verified, no differential harness yet.
 */
#ifndef GAME_MAP_INTERACT_H
#define GAME_MAP_INTERACT_H

/* 0x14742 */
int map_find_records(int x, int y, int radius, uint8_t *out, int filter);

/* 0x12DAC */
int map_wait_key(void);

/* 0x115B6 */
int map_pick_record(int mode, int sel, int list);

#endif /* GAME_MAP_INTERACT_H */
