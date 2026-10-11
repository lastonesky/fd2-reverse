/* map_ai.h - the unit-AI target/action scorer (original 0x1567E, 0x15880,
 * 0x15B77, 0x15DA2, 0x14237, 0x146D1, 0x1DEBE).
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
 *   0x1598A  map_ai_pick_action_target for each usable action slot of
 *                                    `actor`, flood-fill the reachable cells
 *                                    from the actor and keep the best-scoring
 *                                    target in dword_53C23..dword_53C2F.
 *   0x14237  map_ai_pick_reach_target flood-fill the movement-reachable region
 *                                    for `actor`'s item and score every record
 *                                    the item can hit, keeping the best in
 *                                    dword_53C43..dword_53C4F.
 *
 * Two shared map helpers:
 *
 *   0x146D1  map_exclude_record_cells set the cells of the records whose +6
 *                                    status matches `mode` to 0xFF so ev5_14B16
 *                                    drops them from the reachable-cell scan.
 *   0x1DEBE  map_ai_target_adjacent  1 when a record is orthogonally adjacent
 *                                    to a cell and carries a weak-slot item.
 *
 * The decision driver 0x14EF0 is still pending.
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

/* 0x1598A - for each usable action slot of `actor`, mark the cells reachable
 * from the actor and keep the best-scoring target cell (dword_53C27/2B),
 * action id (dword_53C2F) and score (dword_53C23).  `require_empty` selects
 * the reachable-cell filter.  Always returns 0. */
int map_ai_pick_action_target(int actor, int require_empty);

/* 0x14237 - for the item in the first claimed slot of `actor`, flood-fill the
 * movement-reachable region and score every record the item can hit, storing
 * the best target cell (dword_53C43/47), record index (dword_53C4B) and score
 * (dword_53C4F).  `mode` selects the occupied-cell exclusion (see
 * map_exclude_record_cells) and the reachable-cell filter.  Always returns 0. */
int map_ai_pick_reach_target(int actor, int mode);

/* 0x146D1 - set the cells of every record whose +6 status matches `mode`
 * (0: +6 == 0, else +6 != 0) to 0xFF, so ev5_14B16 drops them.  Skips `actor`
 * and any record whose +5 bit 0 is set. */
void map_exclude_record_cells(int actor, int mode);

/* 0x1DEBE - 1 when record `target` is orthogonally adjacent to cell (x,y) and
 * carries a weak (effect byte +11 <= 1) item in its first slot; else -1. */
int map_ai_target_adjacent(int target, int x, int y);

#endif /* GAME_MAP_AI_H */
