/* party_roster.h - party-membership helpers shared by the party-change screen
 * (original 0x2B439 and 0x2B843; both are callees of the still-pending 0x2AF28).
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

#endif /* GAME_PARTY_ROSTER_H */
