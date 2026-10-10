/* map_action.h - map action-effect handlers (original 0x21082 .. 0x22E41).
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
 *   0x226EA / 0x2282F / 0x22960  the funcs_1541F entries 17/18/19: subtract the
 *                         cost at 0x619FD entry 18/18/19 via rec_sub_table5
 *                         (the x and y cost both charge entry 18 on the
 *                         machine), then run 0x22721 / 0x22866 / 0x22997; the
 *                         funcs_1541F entry 25 is 0x22C04 itself.
 *
 *   0x22A85  +20 table    charge the 0x619FD entry-20 cost, then run
 *                        item_apply over the list with effect byte 37 (rec+37)
 *   0x22BC6  +21 table    the same with entry-21 cost and effect byte 38
 *   0x22AA8               the shared body both of the above jump into
 *                        (charge cost, item_apply, then drain the status queue)
 *
 *   0x22D1B               the shared damage body of the entries 22/26/27:
 *                        draw the icons, then deal 10 damage (50% chance) to
 *                        each target whose effect byte is still empty, arm it
 *                        to rand 2..5, queue the number and book 8*rec[33]
 *   0x22CDA               its shared body: charge the cost, then run 0x22D1B
 *   0x22BE1 / 0x22CBF / 0x22E41  entries 22/26/27: charge index 22/26/27 and
 *                        damage effect byte 39/37/38 respectively
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

/* 0x2282F - charge the 0x619FD entry-18 cost, then run map_action_grow_y. */
int map_action_grow_y_cost(int index, int count, const uint8_t *list);

/* 0x22960 - charge the 0x619FD entry-19 cost, then run map_action_widen. */
int map_action_widen_cost(int index, int count, const uint8_t *list);

/* 0x22AA8 - charge the 0x619FD entry-`index` cost, run item_apply over `list`
 * with per-record effect byte `field`, then drain the queued status numbers.
 * Shared body of the funcs_1541F entries 20/21 (see map_action_entry20/21). */
int map_action_charge_apply(int a1, int index, int count, const uint8_t *list,
                            int field);

/* 0x22A85 - funcs_1541F entry 20: charge cost index 20, effect byte 37. */
int map_action_entry20(int a1, int count, const uint8_t *list);

/* 0x22BC6 - funcs_1541F entry 21: charge cost index 21, effect byte 38. */
int map_action_entry21(int a1, int count, const uint8_t *list);

/* 0x22D1B - deal 10 damage (50% chance) to each listed record whose effect
 * byte `field` is still empty, arm it to rand 2..5, book experience and queue
 * the numbers.  Shared body of the funcs_1541F entries 22/26/27. */
int map_action_damage_records(int a1, int table_idx, int count,
                              const uint8_t *list, int field);

/* 0x22CDA - charge cost `index` then run map_action_damage_records; shared
 * body of the funcs_1541F entries 22/26/27. */
int map_action_charge_damage(int a1, int index, int count, const uint8_t *list,
                             int field);

/* 0x22BE1 - funcs_1541F entry 22: charge cost index 22, effect byte 39. */
int map_action_entry22(int a1, int count, const uint8_t *list);

/* 0x22CBF - funcs_1541F entry 26: charge cost index 26, effect byte 37. */
int map_action_entry26(int a1, int count, const uint8_t *list);

/* 0x22E41 - funcs_1541F entry 27: charge cost index 27, effect byte 38. */
int map_action_entry27(int a1, int count, const uint8_t *list);

/* 0x21227 - charge cost `table_idx`, blink the targets over the map view and
 * resolve action `table_idx` on each listed record, queueing the dealt number.
 * Shared effect body of the funcs_1541F entries 0/1/2/3/8. */
int map_action_resolve_list(int a1, int table_idx, int count,
                            const uint8_t *list);

/* 0x21206 / 0x2134B / 0x21364 / 0x2137D / 0x21494 - funcs_1541F entries
 * 0/1/2/3/8: resolve action 0/1/2/3/8 on the list. */
int map_action_entry00(int a1, int count, const uint8_t *list);
int map_action_entry01(int a1, int count, const uint8_t *list);
int map_action_entry02(int a1, int count, const uint8_t *list);
int map_action_entry03(int a1, int count, const uint8_t *list);
int map_action_entry08(int a1, int count, const uint8_t *list);

/* 0x213B7 - clear the status-queue flag, draw the animated party icons with
 * shape bank `table_idx`, alternate the record-layer animation frames, charge
 * cost `table_idx` via rec_sub_table5, then resolve action `table_idx` on each
 * listed record and queue the dealt number.  Shared body of the funcs_1541F
 * entries 4/5/6/7. */
int map_action_resolve_layer(int a1, int table_idx, int count,
                             const uint8_t *list);

/* 0x21396 / 0x21449 / 0x21462 / 0x2147B - funcs_1541F entries 4/5/6/7:
 * resolve action 4/5/6/7 on the list via map_action_resolve_layer. */
int map_action_entry04(int a1, int count, const uint8_t *list);
int map_action_entry05(int a1, int count, const uint8_t *list);
int map_action_entry06(int a1, int count, const uint8_t *list);
int map_action_entry07(int a1, int count, const uint8_t *list);

/* 0x214AD - funcs_1541F entry 9: charge cost 9, then resolve action 9 on the
 * first listed record only and queue the dealt number. */
int map_action_entry09(int a1, int count, const uint8_t *list);

/* 0x2111A - action router (sub_20C6F) action-code 21 body: draw the animated
 * party icons and the record-layer blink with shape bank `bank`, resolve action
 * `bank` on each listed record and queue the dealt number.  Owns the shared
 * 0x21190 epilogue (map_view_update + map_draw_status_popups) that the siblings
 * 0x21227/0x213B7/0x211A4/0x214AD jump into. */
int map_action_anim_resolve(int a1, int count, const uint8_t *list, int bank);

/* 0x211A4 - action router (sub_20C6F) action-code 5/13 body: draw both
 * party-icon passes with the fixed bank 13, then heal each listed record by
 * `amount` and queue the recovered number with glyph base 105. */
int map_action_heal_list(int a1, int count, const uint8_t *list, int amount);

#endif /* GAME_MAP_ACTION_H */
