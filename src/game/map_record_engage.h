/* map_record_engage.h - confirm the selected map record and walk it into place
 * (original 0x18890 + its 0x18B84 frame pump).
 *
 *   0x18890  map_record_engage   the event tick's "act on the selected record"
 *            driver.  For record `rec` it (re)builds the reachable region
 *            around the map cursor, pumps one animation frame, lets the player
 *            pick a standing record, and then traces a path from the picked
 *            cell to the latched cursor target: on success it marches the actor
 *            (map_scroll_path) and runs the on-map record interaction
 *            (map_record_interact) until it settles, re-tagging the cell and
 *            flagging the record on the way out.  Returns 0 while the caller's
 *            `while (map_record_engage(rec) == 0)` loop should keep pumping, and
 *            1 (or the dword_53C53 abort value) when it is done.
 *
 *   0x18B84  map_wait_anim       pump the palette/map animation until a key is
 *            pending: while !kbd_pending(), advance the DAC palette, and once
 *            the BIOS tick has moved redraw the map window, the cursor, the
 *            portraits and the 0x984-byte status panel, then copy the finished
 *            312x192 window to the screen at 0xA0504.
 *
 * Both are cdecl; ABI decoded from the pushes.  Hand-restored, compile-verified,
 * no differential harness.  See map_record_engage.c for the traps.
 */
#ifndef GAME_MAP_RECORD_ENGAGE_H
#define GAME_MAP_RECORD_ENGAGE_H

/* 0x18890 */
int map_record_engage(int rec);

/* 0x18B84 - the shared frame pump 0x18890 calls before the record pick. */
void map_wait_anim(int rec);

#endif /* GAME_MAP_RECORD_ENGAGE_H */
