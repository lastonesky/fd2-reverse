/* party_roster.c - party-membership helpers (original 0x2B439 + 0x2B843).
 *
 * Direct translation of the machine code; the offline disassembly
 * (analysis/cseg01_disasm.txt, lines for 0x2B439..0x2B995) is authoritative.
 *
 * Decompiler artifacts handled here:
 *   - the Watcom stack probe (`push size; call 0x3702F`) is dropped;
 *   - 0x2B439's id test renders as `dword_53A45 + 80*i + 88` but the machine
 *     indexes the record at slot i+1 (`lea ebx,[edx+1]; ... shl 4`), i.e. the
 *     same as slot 1..count, written out directly below;
 *   - both functions compare an unsigned byte id (movzx) against the record
 *     byte +8, so the compare is on (uint8_t)id;
 *   - `buf` (0x53A61, the DATO sprite bank scratch cell) is freed through the
 *     game heap wrapper, matching dword_53BF7's game-heap allocation.
 */
#include "fd2_game.h"
#include "party_roster.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* 0x2B439 - 1 when member `id` is already in party slots 1..count; otherwise
 * plays the recruit portrait and returns 0. */
int party_member_present(int count, int id)
{
    const uint8_t *recs  = (const uint8_t *)dword_53A45;
    uint8_t        found = 0;           /* v4 ([esp+0]) */
    int            i;

    for (i = 0; i < count; ++i) {
        if (recs[REC_STRIDE * (i + 1) + 8] == (uint8_t)id)
            found = 1;
    }
    if (found == 0) {
        msg_open_portrait(75);
        dword_53AD9 = (uint32_t)(uint8_t)id + 1;
        vm_run(dword_53A7D, 657, 0xA951F, 320, 205, 76, 74, 19, 1);
        dlg_blit_dato(dword_53A85, dword_53C67, 0);
        dword_53A51 = (uint8_t *)(uintptr_t)1;   /* non-NULL sentinel */
        dlg_wait_key(0);
        dword_53A51 = 0;
        msg_close_portrait();
    }
    return found;
}

/* 0x2B843 - move member `id` into party slot 1, pack the rest, then reload the
 * FDICON.B24 icon atlas. */
int party_member_move_front(int id)
{
    const uint8_t *recs  = (const uint8_t *)dword_53A45;
    uint8_t       *party = (uint8_t *)(uintptr_t)dword_53BF7;
    uint8_t        found = 0;           /* v7 ([esp+0])  */
    uint8_t        next  = 2;           /* v8 ([esp+4])  */
    uint8_t       *tmp;
    FILE          *fp;
    int            i;

    for (i = 1; i < (int)dword_53BFB; ++i) {
        if (recs[REC_STRIDE * i + 8] == (uint8_t)id)
            found = (uint8_t)i;
    }

    tmp = (uint8_t *)guest_malloc(0xA00u);
    memmove(tmp, party, 0xA00u);
    memmove(party + REC_STRIDE, tmp + REC_STRIDE * found, 0x50u);
    for (i = 1; i < (int)dword_53BFB; ++i) {
        if (i != found)
            memmove(party + REC_STRIDE * next++, tmp + REC_STRIDE * i, 0x50u);
    }
    guest_free(tmp);
    guest_free((void *)(uintptr_t)dword_53A61);     /* free `buf` */

    fp = fopen(aFdiconB24_6, aRb_10);               /* "FDICON.B24","rb" */
    dword_53BDF = 0;
    for (i = 0; i < (int)dword_53BFB; ++i)
        unit_entry_data(party[REC_STRIDE * i + 7], fp);
    return fclose(fp);
}

/* 0x24BDE - is unit id `id` in the party-record table dword_53BF7?
 *
 * Scans slots 0..dword_53BFB-1 (the machine's loop counter starts at 0, unlike
 * party_member_present's 1..count) and compares the record's id byte (+8) with
 * the zero-extended `id`.  IDA's prototype is int(int); the only callee is the
 * dropped stack probe. */
int party_has_unit(int id)
{
    const uint8_t *party = (const uint8_t *)(uintptr_t)dword_53BF7;
    int i;

    for (i = 0; i < (int)dword_53BFB; ++i) {
        if (party[REC_STRIDE * i + 8] == (uint8_t)id)
            return 1;
    }
    return 0;
}
