/* map_turn.h - the per-record turn loops and the record-action dispatcher
 * (original 0x13A9F, 0x1D80B, 0x1D8BA).
 *
 * A full turn walks the 80-byte records at dword_53A45 (count dword_53BEB) and,
 * for each ready record, runs the action it selected on a previous menu pass:
 *
 *   0x13A9F  map_record_action  execute record `actor`'s pending action.  The
 *            low nibble of record byte +52 is the action code (0..11); the
 *            decision helper map_ai_decide/map_ai_pick_* chain picks the move
 *            or attack, and the case then commits it, glides the portrait,
 *            dispatches the scene handler (funcs_1199C) for the +61 cell tag,
 *            and always finishes by re-tagging the cell (map_scroll_cell_tag),
 *            setting record +5 bit 7 (rec_flag_or80), clearing the portrait and
 *            refreshing the map view.
 *
 *   0x1D80B  map_turn_side1     iterate every record with +6 status class 1 and
 *                               (in the +5 & 0x81 == 0, +38 == 0 state) run
 *                               map_record_action(actor, 1), then the +0x51A8F
 *                               scene handler and the per-state handler table
 *                               funcs_1197B[dword_53C03].  Stops early on a
 *                               pending transition (dword_53ECC != 0).
 *   0x1D8BA  map_turn_side0     the class-0 pass: first score every class-0
 *                               record (map_ai_pick_action_target /
 *                               map_ai_pick_item_target) and act when a score
 *                               reaches 6, then a second class-0 sweep that
 *                               only runs map_record_action.  Same per-record
 *                               handler dispatch and early stop as 0x1D80B.
 *
 * The two class-0/class-1 loops bracket the runs of the party-change
 * orchestrator 0x1A30B (still pending).  Both are called argument-less.
 *
 * ABI = cdecl, decoded from the offline disassembly
 * (analysis/cseg01_disasm.txt), which is the authority for argument order,
 * constants and control flow.  The Watcom stack probe (`push size; call
 * 0x3702F`) is dropped; the tail `jmp 0x1317D` of 0x13A9F is a shared epilogue
 * (plain return).  Hand-restored, compile-verified, no differential harness.
 */
#ifndef GAME_MAP_TURN_H
#define GAME_MAP_TURN_H

#include <stdint.h>

/* 0x13A9F - run the action code in record `actor`'s +52 low nibble.
 * `require_empty` is the reachable-cell filter forwarded to the AI scorers. */
void map_record_action(int actor, int require_empty);

/* 0x1D80B - one turn sweep over the records whose +6 status class is 1. */
void map_turn_side1(void);

/* 0x1D8BA - the class-0 sweep (scored first pass, then an action-only pass). */
void map_turn_side0(void);

#endif /* GAME_MAP_TURN_H */
