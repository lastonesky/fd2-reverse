/* party_change_seq.h - the per-record helpers of the party-change map sequence
 * (original 0x1A813, 0x1A866).
 *
 * The still-pending orchestrator 0x1A30B runs a "party changed" sequence on the
 * map screen: it snaps the frame, runs the two turn sweeps (map_turn_side1 /
 * map_turn_side0), brackets them with the mosaic portrait reveal
 * (reveal_mosaic_out / reveal_mosaic_in) and, between those passes, calls the
 * two helpers here with a phase/class argument (0/1/2):
 *
 *   0x1A813  party_change_wave_dispatch(phase)
 *            walk the 16 three-byte wave records at dword_53A55 (the table the
 *            ev2/ev3/ev6 scenario scripts fill): entry byte +0 is the wave
 *            number and byte +2 is the phase.  For every entry whose wave byte
 *            equals the live wave counter dword_53BEF and whose phase byte
 *            equals `phase`, run the scene handler funcs_1199C[entry byte +1]
 *            with argument 0 (the same table map_turn.c dispatches).
 *
 *   0x1A866  party_change_class_apply(cls)
 *            two passes over the 80-byte records at dword_53A45 (count
 *            dword_53BEB) for the records whose +6 status class is `cls` and
 *            whose +5 flag bit 0 is clear:
 *              pass 1 - a record with a non-zero +37 ailment counter loses
 *                       max_hp(+0x42)/10 from its current hp (+0x40, clamped at
 *                       0) and plays the ailment portrait/VM frame;
 *              then map_units_animate + funcs_1197B[dword_53C03](0);
 *              pass 2 - each of the six per-effect counters +34..+39 that is
 *                       non-zero is decremented and, when it reaches zero,
 *                       plays the same portrait/VM frame followed by
 *                       unit_metrics(record).
 *
 * ABI = cdecl, one int argument.  Decoded from the offline disassembly
 * (analysis/cseg01_disasm.txt), the authority for the argument order, the
 * constants and the control flow.  The Watcom stack probe (`push size; call
 * 0x3702F`) is dropped; the tail `jmp 0x190A9` / `jmp 0x190A7` are shared
 * epilogues (plain return) living at the tail of another function, not calls.
 * All real callees are already-restored assets (dlg_portrait_glide, vm_run,
 * msg_open_portrait, kbd_flush, ev5_1E5C0, ev4_196CB, unit_metrics,
 * map_units_animate).  No differential harness yet (unverified).
 */
#ifndef GAME_PARTY_CHANGE_SEQ_H
#define GAME_PARTY_CHANGE_SEQ_H

/* 0x1A813 - dispatch the scene handlers of the wave/phase records that match
 * the current wave counter dword_53BEF. */
void party_change_wave_dispatch(int phase);

/* 0x1A866 - apply the ailment HP drop and decrement the effect counters of the
 * records whose +6 status class is `cls`. */
void party_change_class_apply(int cls);

#endif /* GAME_PARTY_CHANGE_SEQ_H */
