/* map_event.h - map cell "special event" handler (original 0x190AC).
 *
 *   0x190AC  map_cell_event   called by the record-interaction driver
 *            (0x18D8C, its `else` action) with a record index.  It looks up
 *            the map cell under the cursor (qword_53AB1), reads that cell's
 *            16-bit flag value and the tile-info byte, and, for a not-yet-
 *            handled event, runs the dialogue/message sequence and applies the
 *            effect stored in the 3-byte per-flag table at dword_53A55:
 *              kind 0 -> claim a record slot / dialogue (dword_53AD9),
 *              kind 1 -> add dword_53AE1 to the party money (dword_53BF3),
 *              other  -> dispatch the scene handler funcs_1199C[value].
 *
 * ABI = cdecl, one int argument.  Uses map_cell_info, vm_run, msg_open_portrait,
 * dlg_blit_dato/dlg_wait_key, ev4_196CB/ev4_12263, svc_play_sfx2, map_window_run
 * and the funcs_1199C table.  Hand-restored, compile-verified, no differential
 * harness yet.
 */
#ifndef GAME_MAP_EVENT_H
#define GAME_MAP_EVENT_H

/* 0x190AC */
int map_cell_event(int rec_index);

#endif /* GAME_MAP_EVENT_H */
