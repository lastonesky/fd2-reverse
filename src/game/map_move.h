/* map_move.h - walk an actor toward a map cell along a computed path
 * (original 0x14B78, 0x14121, 0x13E9C).
 *
 * The three functions all end by committing a movement path for one record:
 *
 *   0x14B78  map_move_toward      move `actor` toward cell (target_x,
 *                                target_y): try a direct path first; when it
 *                                is blocked, recompute the reachable region
 *                                and head for the reachable cell closest to
 *                                the target instead.  Returns 1 when a step
 *                                path was committed, else 0.
 *   0x14121  map_move_to_marked   find the 0x40-flagged map cell reachable
 *                                from `actor` (path_find mode 2) and move the
 *                                actor onto it, unless it is already there.
 *   0x13E9C  map_move_to_nearest  find the record whose +6 status matches
 *                                `mode` (0: +6 != 0, else +6 == 0) and whose
 *                                cell is closest to the actor, then move the
 *                                actor onto it, unless it is already there.
 *
 * All three dip the portrait out (`dlg_portrait_glide`) around the move and
 * bracket the movement with dword_51A83 = 0 / 1.  Shared building blocks: the
 * 0x61646 (stride 20) movement-cost row via tbl_ptr, the reachability scan
 * path_mark/path_find (0x4E390/0x4E4F6), the reachable-cell list ev5_14B16,
 * util_fix_records (0x4DF4C), unit_mark_nearby (0x145CD), map_exclude_record_
 * cells (0x146D1) and the direction-list walker map_scroll_path (0x13488).
 *
 * ABI = cdecl, decoded from the offline disassembly
 * (analysis/cseg01_disasm.txt), which is the authority for argument order and
 * constants.  The Watcom stack probe (`push size; call 0x3702F`) is dropped;
 * the 0x3706E/0x3776E buffers are the game heap (guest_malloc/guest_free).
 * Hand-restored, compile-verified, no differential harness yet.
 */
#ifndef GAME_MAP_MOVE_H
#define GAME_MAP_MOVE_H

#include <stdint.h>

/* 0x14B78 - commit the walk of `actor` toward cell (target_x, target_y).
 * `mode` selects the occupied-cell marking (unit_mark_nearby) and the +6
 * exclusion filter (map_exclude_record_cells).  Returns 1 when a path was
 * committed. */
int map_move_toward(int target_x, int target_y, int actor, int mode);

/* 0x14121 - move `actor` onto the 0x40-flagged cell reachable from it, or do
 * nothing when it already stands there / no such cell is reachable.  Returns
 * 1 when a path was committed. */
int map_move_to_marked(int actor, int mode);

/* 0x13E9C - move `actor` onto the nearest record whose +6 status matches
 * `mode` (0: +6 != 0, else +6 == 0), or do nothing when it already stands on
 * that record.  Returns 1 when a path was committed. */
int map_move_to_nearest(int actor, int mode);

#endif /* GAME_MAP_MOVE_H */
