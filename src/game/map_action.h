/* map_action.h - map action-effect handlers (original 0x21082 .. 0x22C04).
 *
 * The per-record effects dispatched by the map action router sub_20C6F (action
 * codes 8..24) and by the funcs_1541F event/action table that sub_15311 and
 * sub_1CFF0 invoke.  Each one draws the party icons, walks a list of record
 * indices, flags the records whose effect byte is still empty and books the
 * experience in dword_53EC8, then refreshes the view and pops the queued
 * status numbers:
 *
 *   0x21082  +a7 table    add `value` to a 16-bit record field for the first
 *                        target, show it, recompute the sprite metrics and
 *                        drop the cost slot
 *   0x22721  +17 table    set record flag +34 (x-enlarge) to rand 2..5 and
 *                        grow the record's +72 metric
 *   0x22866  +18 table    set record flag +35 (y-enlarge) to rand 2..5 and
 *                        grow the record's +74 metric
 *   0x22997  +19 table    set record flag +36 (widen) to rand 2..5 and grow
 *                        the record's +76/+78 metrics by 15
 *   0x22C04  +25 table    clear bit 7 of record flag +5 (rec_flag_or80 writer)
 *
 *   0x226EA / 0x22960      the funcs_1541F entries 17/19: subtract the cost at
 *                         0x619FD entry 18/19 via rec_sub_table5, then run
 *                         0x22721 / 0x22997; the funcs_1541F entries 25 is
 *                         0x22C04 itself.
 *
 * ABI = cdecl, arguments decoded from the pushes (the offline disassembly is
 * the authority).  Record fields index the 80-byte table dword_53A45; the
 * shared epilogues `jmp 0x1317D` / `jmp 0x22BBE` are plain returns here.  Hand
 * restored, compile-verified, no differential harness yet.
 */
#ifndef GAME_MAP_ACTION_H
#define GAME_MAP_ACTION_H

#include <stdint.h>

/* 0x21082 - for the first record named by `list`, add `value` (16-bit) to the
 * field at byte `field` of the record, queue that number with glyph base 94,
 * refresh and pop the view, recompute record `rec`'s metrics and remove
 * `slot` from its equipment list.  `table_idx` selects the icon shape bank. */
int map_action_add_field(int a1, int value, int field, int slot, int count,
                         const uint8_t *list, int table_idx);

/* 0x22721 - flag record +34 (x-enlarge) for each target whose +34 is empty and
 * grow the record's +72 metric by (word * dbl_50210 + 1); otherwise queue the
 * record's idle cursor sprite. */
int map_action_grow_x(int a1, int count, const uint8_t *list);

/* 0x22866 - flag record +35 (y-enlarge) for each target whose +35 is empty and
 * grow the record's +74 metric by (word * dbl_50218 + 1); otherwise queue the
 * record's idle cursor sprite. */
int map_action_grow_y(int a1, int count, const uint8_t *list);

/* 0x22997 - flag record +36 (widen) for each target whose +36 is empty, grow
 * the +76/+78 metrics by 15; otherwise queue the idle cursor sprite. */
int map_action_widen(int a1, int count, const uint8_t *list);

/* 0x22C04 - clear bit 7 of record flag +5 (rec_flag_or80) for each target that
 * has it set; others queue their idle cursor sprite. */
int map_action_clear_flag(int a1, int count, const uint8_t *list);

/* 0x226EA - charge the 0x619FD entry-18 cost, then run map_action_grow_x. */
int map_action_grow_x_cost(int index, int count, const uint8_t *list);

/* 0x22960 - charge the 0x619FD entry-19 cost, then run map_action_widen. */
int map_action_widen_cost(int index, int count, const uint8_t *list);

#endif /* GAME_MAP_ACTION_H */
