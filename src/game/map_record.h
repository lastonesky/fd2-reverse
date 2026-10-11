/* map_record.h - on-map per-record interaction driver (original 0x18D8C).
 *
 *   0x18D8C  map_record_interact   act on the record under the map cursor.
 *            It resets the caller's 4-int direction-patch tile array
 *            `dir_tiles`, opens the 4x4 direction patch over the cursor
 *            (map_cursor_patch_expand / map_cursor_dir_select /
 *            map_cursor_patch_collapse) and then branches on the selected slot
 *            dword_53C57:
 *
 *              slot 0  pick a record standing on the map (map_pick_record over
 *                      a 0x64 scratch buffer), latch the view, then find the
 *                      record's portrait and play its attack view
 *                      (figure_attack_view); finally the actor is flagged and
 *                      its event script runs (map_collect_scripts +
 *                      map_units_animate + ev_script_run).  Returns -1 on ESC,
 *                      0 when the pick was cancelled, 1 on success.
 *              slot 1  run the action menu (action_menu_run); on success scale
 *                      the booked experience dword_53EC8 by the record's
 *                      animation duration.  Returns 1, or 0 when cancelled.
 *              slot 2  run the interactive action driver (map_action_interact).
 *                      Returns 1, or 0 when cancelled.
 *              other   apply item_use (unless the caller passed reopen != 0)
 *                      and the cell's special event (map_cell_event), then flag
 *                      the actor.  Returns 1.
 *
 * ABI = cdecl with three arguments: the record index, a pointer to the caller's
 * 4-int direction-patch tile array (also used to return the `no slot` /
 * `blocked` / `no free slot` status flags at [0]/[1]/[2]), and a `reopen` flag.
 * Hand-restored, compile-verified, no differential harness yet.
 */
#ifndef GAME_MAP_RECORD_H
#define GAME_MAP_RECORD_H

/* 0x18D8C */
int map_record_interact(int rec, int *dir_tiles, int reopen);

#endif /* GAME_MAP_RECORD_H */
