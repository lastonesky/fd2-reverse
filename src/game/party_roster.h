/* party_roster.h - party-membership helpers shared by the party-change screen
 * (original 0x2B439, 0x2B843 and 0x2B777; all three are callees of the
 * still-pending 0x2AF28).
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

#endif /* GAME_PARTY_ROSTER_H */
