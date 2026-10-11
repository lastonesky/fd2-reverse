/* party_roster.h - the party-change screen and its membership helpers.
 *
 *   0x2AF28  party_change_run()   the screen itself: snapshot the map view,
 *              compose the roster panel, slide it in, run the selection loop
 *              (toggle a member with Enter; confirm when all are selected),
 *              slide out and — on confirm — apply the roster and play the
 *              recruit/leave portrait sequence.  Returns 1 when the change was
 *              accepted, 0 otherwise (Esc or aborted portrait).
 *   0x2B4FB  party_change_draw(screen, count, selected, cursor)  redraw one
 *              frame of the panel into dword_53C63 from the snapshot `screen`:
 *              the two counters, the record status box, the cursor sprite and
 *              each party slot's icon (plain when selected, ramp when not).
 *   0x2B67F  party_change_wait_key(screen, count, selected, cursor)  idle
 *              until a key is pending, redrawing the panel whenever the BIOS
 *              tick advances, then read one BIOS key and remap it.
 *
 * Helpers originally restored for the screen (0x2B439, 0x2B843 and 0x2B777;
 * all three are callees of 0x2AF28).
 *
 *   0x2B777  party_roster_apply_selection(mask)  stable-partition slots
 *            1..dword_53BFB-1 of the party table (dword_53BF7) so that the
 *            slots whose mask byte (0..dword_53BFB-2) is set come first, in
 *            their original order, followed by the unset ones, also in order.
 *            Slot 0 is the table header and is left untouched; the count-1
 *            mask bytes pair with slots 1..count-1.
 *
 *   0x2B439  party_member_present(count, id)  scan party slots 1..count of the
 *            record table (dword_53A45) for the record whose id byte (+8) is
 *            `id`; when it is absent, play the recruit-portrait sequence
 *            (msg_open_portrait(75) -> vm_run(657) -> dlg_blit_dato(0) -> wait
 *            for a key -> msg_close_portrait) and return 0, otherwise 1.
 *   0x2B843  party_member_move_front(id)  move the member whose id byte (+8) is
 *            `id` into slot 1 of table 2 (dword_53BF7), pack the remaining
 *            slots, release the DATO scratch bank `buf` (0x53A61) and reload
 *            the FDICON.B24 icon atlas; returns the fclose() result (unused).
 *
 * Both scan table 1 by an unsigned-byte id compare, so ids above 255 can never
 * match.  Hand-restored from the offline disassembly; compile-verified and
 * marked 已还原未验证 (no differential harness yet).
 */
#ifndef GAME_PARTY_ROSTER_H
#define GAME_PARTY_ROSTER_H

/* 0x2B439 */
int party_member_present(int count, int id);

/* 0x2B843 */
int party_member_move_front(int id);

/* 0x24BDE - 1 when some party record in dword_53BF7 (slots 0..dword_53BFB-1)
 * carries unit id (+8) == id. */
int party_has_unit(int id);

/* 0x2B777 - stable-partition party slots 1.. by the per-slot selection mask
 * (count-1 bytes), moving selected slots to the front of table 2. */
void party_roster_apply_selection(const uint8_t *selected);

/* 0x2AF28 - the party-change screen; 1 when the roster change is confirmed,
 * 0 when it is cancelled.  `count` (15, or 19 when dword_53C03 > 26) is the
 * number of icons shown; the 32-byte `selected` mask is the slot state. */
int party_change_run(void);

/* 0x2B4FB - draw one frame of the party-change panel into dword_53C63. */
void party_change_draw(const uint8_t *screen, int count,
                       const uint8_t *selected, int cursor);

/* 0x2B67F - wait for a key, animating the panel, and return its (remapped)
 * scan code (INT 16h AH=0x10: E0/52/space -> 1Ch, 53h -> 01h). */
int party_change_wait_key(const uint8_t *screen, int count,
                          const uint8_t *selected, int cursor);

#endif /* GAME_PARTY_ROSTER_H */
