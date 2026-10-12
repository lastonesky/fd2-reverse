/* party_roster.c - party-membership helpers (original 0x2B439 + 0x2B843 +
 * 0x2B777).
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
 *     game heap wrapper, matching dword_53BF7's game-heap allocation;
 *   - 0x2B777's tail `push edi; jmp 0x29862` is a shared epilogue in 0x2968D's
 *     body (`call free`; pop ebp/edi/esi/ebx), i.e. `free(tmp); return`, so its
 *     only dependencies are the CRT malloc/memmove/free.
 */
#include "fd2_game.h"
#include "party_roster.h"
#include "../dos.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* BIOS tick word (BDA 40:6C), read through the low-memory mirror exactly as
 * anim.c/box_popup.c do. */
#define BDA_W(off)  (*(volatile uint16_t *)(uintptr_t)(DOS_LOWMEM_BASE + (off)))

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
        vm_run(dword_53A7D, 657,VGA_P(0xA951Fu), 320, 205, 76, 74, 19, 1);
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

/* 0x2B777 - stable-partition the party table dword_53BF7 by the count-1 mask
 * bytes `selected[0..count-2]` (each pairing with slot i+1): the set slots come
 * first in order, then the unset ones.  Slot 0 (the header) and the tail of the
 * 0xA00-byte table are copied through untouched.  The Watcom probe is dropped;
 * the tail `push edi; jmp 0x29862` is 0x2968D's shared free-epilogue. */
void party_roster_apply_selection(const uint8_t *selected)
{
    uint8_t *party = (uint8_t *)(uintptr_t)dword_53BF7;
    uint8_t *tmp   = (uint8_t *)guest_malloc(0xA00u);
    int      next  = 1;                 /* esi: next destination slot */
    int      i;

    memmove(tmp, party, 0xA00u);
    for (i = 0; i < (int)dword_53BFB - 1; ++i) {
        if (selected[i] != 0)
            memmove(party + REC_STRIDE * next++, tmp + REC_STRIDE * (i + 1), 0x50u);
    }
    for (i = 0; i < (int)dword_53BFB - 1; ++i) {
        if (selected[i] == 0)
            memmove(party + REC_STRIDE * next++, tmp + REC_STRIDE * (i + 1), 0x50u);
    }
    guest_free(tmp);
}

/* ------------------------------------------------------------------ *
 * Party-change screen: 0x2AF28 and its two helpers 0x2B4FB / 0x2B67F.
 *
 * Decompiler artifacts handled here (the offline disassembly is the
 * authority):
 *   - the three `sub_3702F` stack probes allocate no frame and are dropped;
 *   - the machine writes the screen-copy pointer to two stack slots
 *     ([esp+20h] `v12` and [esp+28h] `p`); the [esp+24h] slot is later reused
 *     for the fopen() handle, so they are modelled as one `buf` plus `fp`;
 *   - malloc/free go through the game heap (guest_malloc/guest_free), and the
 *     DATO scratch bank dword_53A61 is freed through guest_free too;
 *   - the BIOS key read uses the global REGS cell word_53A8D / byte_53A8E
 *     (AH = 0x10) via int386, and the idle loop polls the BIOS tick word at
 *     BDA 0x46C (through the low-memory mirror, as anim.c/box_popup.c do);
 *   - dlg_blit_dato takes its (dato_buf, box_pos, idx) globals explicitly,
 *     matching the verified port/ API (see scene_core.c);
 *   - a `jmp 0x2B434` at the tail of 0x2B4FB is a shared `return` in 0x2AF28.
 */

/* 0x2B4FB - one panel frame: counters, status box, cursor and slot icons. */
void party_change_draw(const uint8_t *screen, int count,
                       const uint8_t *selected, int cursor)
{
    uint8_t *work = (uint8_t *)(uintptr_t)dword_53C63;
    const uint8_t *bank;
    int      frame;
    int      i;

    memmove(work, screen, 0xFA00u);
    dlg_draw_number(work + 11261, 320, count, 31, 2);
    ev5_2B749(selected);
    dlg_draw_number(work + 23421, 320, count - ev5_2B749(selected), 42, 2);
    anim_frame_step();
    frame = dword_53C0B;
    if (frame == 3)
        frame = 1;
    box_status_draw(cursor + 1, dword_53C63);

    bank = (const uint8_t *)dword_53A4D;
    sprite24_plain(bank + *(const uint32_t *)(bank + 6),
                   work + (30 * (cursor / 10) + 104) * 320
                        + 28 * (cursor % 10) + 23, 320);

    for (i = 0; i < (int)dword_53BFB - 1; ++i) {
        int            off;
        const uint8_t *src;

        bank = (const uint8_t *)dword_53A61;
        src  = bank + *(const uint32_t *)(bank
                        + 4u * (uint32_t)(12 * i + frame + 12));
        off  = (30 * (i / 10) + 100) * 320 + 28 * (i % 10) + 23;
        if (selected[i] != 0)
            sprite24_plain(src, work + off + 960, 320);
        else
            sprite24_ramp24(src, work + off, 320);
    }
}

/* 0x2B67F - idle+animate until a key is pending, then read and remap it. */
int party_change_wait_key(const uint8_t *screen, int count,
                          const uint8_t *selected, int cursor)
{
    while (kbd_pending() == 0) {
        int32_t tick = (int32_t)(int16_t)BDA_W(0x46C);

        if (tick != (int32_t)dword_53F46) {
            dword_53F46 = (uint32_t)tick;
            party_change_draw(screen, count, selected, cursor);
            memmove((void *)(uintptr_t)VGA_P(0xA0000u),
                    (const void *)(uintptr_t)dword_53C63, 0xFA00u);
        }
        fd2_wait_tick_or_key();
    }

    byte_53A8E = 0x10;
    int386(0x16, &word_53A8D, &word_53A8D);
    if (byte_53A8E == 0xE0 || byte_53A8E == 0x52
        || (uint8_t)word_53A8D == 0x20)
        byte_53A8E = 0x1C;
    if (byte_53A8E == 0x53)
        byte_53A8E = 0x01;
    return byte_53A8E;
}

/* 0x2AF28 */
int party_change_run(void)
{
    uint8_t  selected[32];
    uint8_t *buf;
    FILE    *fp;
    int      count  = 15;
    int      result = 0;    /* edi: 1 confirmed, -1 escaped        */
    int      cursor = 0;    /* esi                                 */
    int      i, key, sel, wret;

    memset(selected, 0, 30);
    if (dword_53C03 > 26)
        count = 19;

    dword_53C5B = guest_malloc(0xFA00u);
    dword_53C5F = guest_malloc(0xFA00u);
    buf         = (uint8_t *)guest_malloc(0xFA00u);
    dword_53C63 = guest_malloc(0xFA00u);

    memmove((void *)(uintptr_t)dword_53C5F,
            (const void *)(uintptr_t)VGA_P(0xA0000u), 0xFA00u);
    memmove(buf, (const void *)(uintptr_t)dword_53C5F, 0xFA00u);

    rle2_blit(buf + 2332,
              (const void *)(uintptr_t)
                  (*(const uint32_t *)((uintptr_t)dword_53A81 + 86)
                   + (uintptr_t)dword_53A81), 320);
    rle2_blit(buf + 30085,
              (const void *)(uintptr_t)
                  (*(const uint32_t *)((uintptr_t)dword_53A81 + 90)
                   + (uintptr_t)dword_53A81), 320);
    res_blit6(buf + 2245, 320, (void *)(uintptr_t)dword_53A81, 137);

    party_change_draw(buf, count, selected, 0);

    for (i = 11; i >= 0; --i) {
        if (i == 11 || i == 5)
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 5, 1);
        fb_slide_step(i, (char *)(uintptr_t)dword_53C5B,
                      (char *)(uintptr_t)dword_53C63,
                      (char *)(uintptr_t)dword_53C5F);
    }

    kbd_flush();

    do {
        key = party_change_wait_key(buf, count, selected, cursor);
        if (key == 1)
            result = -1;
        if (key == 28) {
            svc_play_sfx2((const void *)(uintptr_t)dword_53EEC, 7, 1);
            selected[cursor] ^= 1;
            key = 77;
            if (ev5_2B749(selected) == count) {
                result = 1;
                party_roster_apply_selection(selected);
            }
        }
        if (key == 75) {
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
            if (--cursor == -1)
                cursor = (int)dword_53BFB - 2;
        }
        if (key == 77) {
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
            if (++cursor == (int)dword_53BFB - 1)
                cursor ^= (int)dword_53BFB - 1;
        }
        if (key == 72 && cursor > 9) {
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
            cursor -= 10;
        }
        if (key == 80 && cursor < (int)dword_53BFB - 11) {
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
            cursor += 10;
        }
        party_change_draw(buf, count, selected, cursor);
        memmove((void *)(uintptr_t)VGA_P(0xA0000u),
                (const void *)(uintptr_t)dword_53C63, 0xFA00u);
    } while (result == 0);

    for (i = 0; i <= 11; ++i) {
        if (i == 0 || i == 7)
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 6, 1);
        fb_slide_step(i, (char *)(uintptr_t)dword_53C5B,
                      (char *)(uintptr_t)dword_53C63,
                      (char *)(uintptr_t)dword_53C5F);
    }
    memmove((void *)(uintptr_t)VGA_P(0xA0000u),
            (const void *)(uintptr_t)dword_53C5F, 0xFA00u);

    guest_free((void *)(uintptr_t)dword_53C5B);
    guest_free((void *)(uintptr_t)dword_53C5F);
    guest_free(buf);
    guest_free((void *)(uintptr_t)dword_53C63);

    if (result == 1) {
        guest_free((void *)(uintptr_t)dword_53A61);   /* free `buf` DATO bank */
        fp = fopen(aFdiconB24_5, aRb_9);              /* "FDICON.B24","rb"     */
        dword_53BDF = 0;
        for (i = 0; i < (int)dword_53BFB; ++i)
            unit_entry_data(((const uint8_t *)(uintptr_t)dword_53BF7)[80 * i + 7],
                            fp);
        fclose(fp);

        if (dword_53C03 == 16 && unit_exists(18) != 0) {
            sel = party_member_present(count, 18);
        } else if (dword_53C03 == 17 || dword_53C03 == 19 || dword_53C03 > 25) {
            sel = party_member_present(count, 9);
        } else {
            switch (dword_53C03) {
            case 18:
                sel = party_member_present(count, 16);
                break;
            case 20:
                sel = party_member_present(count, 21);
                break;
            case 21:
            case 22:
                sel = party_member_present(count, 24);
                break;
            case 25:
                result = party_member_present(count, 9);
                if (result == 0)
                    goto done;
                sel = party_member_present(count, 29);
                break;
            default:
                goto done;
            }
        }
        result = sel;
    }
done:
    if (result != 1)
        return 0;

    if (dword_53C03 == 17 || dword_53C03 == 19 || dword_53C03 > 25) {
        party_member_move_front(9);
    } else {
        switch (dword_53C03) {
        case 20:
            party_member_move_front(21);
            break;
        case 21:
        case 22:
            party_member_move_front(24);
            break;
        case 25:
            party_member_move_front(9);
            party_member_move_front(29);
            break;
        default:
            break;
        }
    }

    msg_open_portrait(75);
    vm_run(dword_53A7D, 658,VGA_P(0xA951Fu), 320, 205, 76, 74, 19, 1);
    dlg_blit_dato(dword_53A85, dword_53C67, 0);
    dword_53A51 = (uint8_t *)(uintptr_t)1;
    kbd_flush();
    wret = map_window_run();
    dword_53A51 = 0;
    map_draw_text_window();
    msg_close_portrait();

    if (wret == -1 || dword_53C57 != 0)
        return 0;
    return 1;
}
