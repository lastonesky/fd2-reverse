/* party_change_seq.h - the party-change map sequence (original 0x1A30B,
 * 0x1A813, 0x1A866).
 *
 * 0x1A30B  party_change_map_seq()
 *            run the "party changed" sequence on the map screen: it first snaps
 * the map frame and animates each changed record's HP toward its maximum (two
 * passes over the 80-byte records at dword_53A45), then runs the two turn
 * sweeps (map_turn_side1 / map_turn_side0) bracketed by the mosaic portrait
 * reveal (reveal_mosaic_out / reveal_mosaic_in) and the shared SFX-bank
 * load/free helpers, and between those passes calls the two helpers here with
 * a phase/class argument (0/1/2).  Any sweep can abort the sequence through
 * the pending-transition flag dword_53ECC, which returns straight away:
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
 * ABI = cdecl, no arguments for 0x1A30B.  Decoded from the offline disassembly
 * (analysis/cseg01_disasm.txt), the authority for the argument order, the
 * constants and the control flow.  The Watcom stack probe (`push size; call
 * 0x3702F`) is dropped; the tail `jmp 0x10B46` and the aborts' `jne 0x10B46`
 * are the shared epilogue (plain return) living at the tail of another
 * function, not calls.  All real callees are already-restored assets
 * (dlg_portrait_glide, vm_run, msg_open_portrait, kbd_flush, ev5_1E5C0,
 * ev4_196CB, unit_metrics, map_units_animate, svc_wait_ticks, svc_play_sfx,
 * gfx_copy_rows, view_draw_unit_sprite, rec_flag_or80, map_view_update,
 * bgm_play, reveal_bank_load/free, reveal_mosaic_out/in, map_turn_side1/0,
 * res_draw_subimage, dlg_snap_restore, dlg_draw_number, ev5_13536,
 * party_change_wave_dispatch, party_change_class_apply, j___delay).  No
 * differential harness yet (unverified).
 *
 * 0x13565  party_change_maybe_run()
 *            run party_change_map_seq() only when the party is settled: scan
 * the records for one whose +5 flag byte has neither bit 0 nor bit 7 set, whose
 * +6 status class is 2 and whose +38 ailment counter is zero.  While any such
 * record remains the sequence is skipped; otherwise it is bracketed by
 * byte_51AAC / dword_51A83 being cleared and restored.
 */
#ifndef GAME_PARTY_CHANGE_SEQ_H
#define GAME_PARTY_CHANGE_SEQ_H

/* 0x1A30B - run the whole party-change sequence (the HP catch-up animation,
 * both turn sweeps and their mosaic reveals).  Returns early whenever a sweep
 * sets the pending-transition flag dword_53ECC. */
void party_change_map_seq(void);

/* 0x1A813 - dispatch the scene handlers of the wave/phase records that match
 * the current wave counter dword_53BEF. */
void party_change_wave_dispatch(int phase);

/* 0x1A866 - apply the ailment HP drop and decrement the effect counters of the
 * records whose +6 status class is `cls`. */
void party_change_class_apply(int cls);

/* 0x13565 - run the party-change map sequence when no class-2, unflagged,
 * ailment-free record is left. */
void party_change_maybe_run(void);

#endif /* GAME_PARTY_CHANGE_SEQ_H */
