/* map_ai.h - the unit-AI target/action scorer (original 0x1567E, 0x15880,
 * 0x15B77, 0x15DA2).
 *
 * Before the decision driver 0x14EF0 picks what a unit does, it evaluates the
 * reachable cells and the usable actions and remembers the best score.  This
 * module holds the scoring half of that: two target-list raters plus the
 * move-target picker that drives them:
 *
 *   0x15880  map_ai_item_score       rate a list of target records for the
 *                                    item/effect whose tbl_602AD entry is
 *                                    `item` (the kind byte at +13 selects the
 *                                    metric).
 *   0x15DA2  map_ai_flag_sum         sum `weight` over the records in `list`
 *                                    whose byte at `field` is zero.
 *   0x1567E  map_ai_pick_item_target for each occupied slot of `actor`, scan
 *                                    the cells reachable from the actor and
 *                                    keep the best-scoring target in
 *                                    dword_53C33..dword_53C3F.
 *   0x15B77  map_ai_action_score     rate a list of target records for the
 *                                    action whose tbl_619FD entry is `action`.
 *
 * The action-target picker 0x1598A (this module's fifth sibling) and the
 * decision driver 0x14EF0 are still pending.
 *
 * ABI = cdecl, decoded from the offline disassembly
 * (analysis/cseg01_disasm.txt), which is the authority for the argument order
 * and the constants.  The Watcom stack probe (`push size; call 0x3702F`) is
 * dropped; the 0x3706E/0x3776E buffer is the game heap (guest_malloc /
 * guest_free).  Hand-restored, compile-verified, no differential harness yet.
 */
#ifndef GAME_MAP_AI_H
#define GAME_MAP_AI_H

#include <stdint.h>

/* 0x15880 - rate the records named by `list` (count) as the targets of the
 * effect whose tbl_602AD (0x602AD / stride 23) entry is `item`. */
int map_ai_item_score(int item, int count, const uint8_t *list);

/* 0x15DA2 - sum `weight` over the records in `list` (count) whose byte at
 * `field` is zero. */
int map_ai_flag_sum(int count, const uint8_t *list, int field, int weight);

/* 0x1567E - for each occupied slot of `actor`, look up the item's effect and
 * scan the cells reachable from the actor, storing the best-scoring target
 * cell (dword_53C37/3B), record index (dword_53C3F) and score (dword_53C33).
 * `require_empty` selects the reachable-cell filter.  Always returns 0. */
int map_ai_pick_item_target(int actor, int require_empty);

/* 0x15B77 - rate the records named by `list` (count) as the targets of the
 * action whose tbl_619FD (0x619FD / stride 7) entry is `action`. */
int map_ai_action_score(int action, int count, const uint8_t *list);

#endif /* GAME_MAP_AI_H */
