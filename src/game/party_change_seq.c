/* party_change_seq.c - see party_change_seq.h.
 *
 * Direct translations of the machine code at 0x1A30B, 0x1A813, 0x1A866 and
 * 0x13565.  The disassembly is the authority: Hex-Rays spells the frame index
 * of the first pass's compared class as an uninitialised local `v10`, but the
 * instruction
 * at 0x1A8A3/0x1A988 compares the record byte +6 against [esp+0x14], i.e. the
 * single cdecl argument `cls` (the Watcom stack probe at 0x3702F allocates no
 * frame - it is `xchg eax,[esp+4]; call probe; ret 4` - so [esp+0x14] is the
 * argument slot, not a local).
 */
#include "fd2_game.h"
#include "party_change_seq.h"

#include <stdint.h>

/* 80-byte record `idx` of the table at dword_53A45. */
static uint8_t *rec_at(int idx)
{
    return (uint8_t *)dword_53A45 + REC_STRIDE * (uint32_t)idx;
}

/* 0x1A813 */
void party_change_wave_dispatch(int phase)
{
    int i;

    for (i = 0; i < 16; i++) {
        const uint8_t *wave =
            (const uint8_t *)(uintptr_t)(dword_53A55 + 3 * (uint32_t)i);

        if ((uint32_t)wave[3] == dword_53BEF &&
            (uint32_t)wave[5] == (uint32_t)phase)
            ((void (*)(int))funcs_1199C[wave[4]])(0);
    }
}

/* 0x1A866 */
void party_change_class_apply(int cls)
{
    int i;
    int j;
    int k;

    for (i = 0; i < dword_53BEB; i++) {
        uint8_t *rec = rec_at(i);

        if (rec[37] != 0 && (uint32_t)rec[6] == (uint32_t)cls &&
            (rec[5] & 1) == 0) {
            uint16_t hp = *(uint16_t *)(rec + 0x40);
            int drop;

            dword_53AE1 = (int32_t)(*(uint16_t *)(rec + 0x42) / 10);
            drop = (int)hp - dword_53AE1;
            if (drop < 0)
                drop = 0;
            *(uint16_t *)(rec + 0x40) = (uint16_t)drop;

            dword_51A83 = 0;
            dlg_portrait_glide(i);
            dword_51A83 = 1;
            msg_open_portrait(rec[7]);
            vm_run(dword_53A7D, 487, 0xA9F23, 320, 205, 76, 74, 19, 1);
            kbd_flush();
            ev5_1E5C0(10);
            ev4_196CB();
        }
    }

    map_units_animate();
    ((int (*)(int))funcs_1197B[dword_53C03])(0);

    for (j = 0; j < dword_53BEB; j++) {
        uint8_t *rec = rec_at(j);

        for (k = 0; k < 6; k++) {
            if ((uint32_t)rec[6] == (uint32_t)cls && (rec[5] & 1) == 0) {
                if (rec[k + 34] != 0 && --rec[k + 34] == 0) {
                    dword_51A83 = 0;
                    dlg_portrait_glide(j);
                    dword_51A83 = 1;
                    msg_open_portrait(rec[7]);
                    vm_run(dword_53A7D, k + 481, 0xA9F23, 320, 205, 76, 74,
                           19, 1);
                    kbd_flush();
                    ev5_1E5C0(10);
                    ev4_196CB();
                    unit_metrics(j);
                }
            }
        }
    }
}

/* 0x1A30B */
void party_change_map_seq(void)
{
    int flag = 0;
    int i;
    int k;
    int m;

    svc_wait_ticks(1);

    /* pass 1: draw the "changed" sprite of every record whose HP is not yet at
     * its maximum, and remember whether any was drawn. */
    for (i = 0; i < dword_53BEB; i++) {
        uint8_t *rec = rec_at(i);

        if (rec[6] == 2 && (rec[5] & 0x81) == 0 && rec[37] == 0 &&
            rec[38] == 0 && *(uint16_t *)(rec + 0x40) !=
            *(uint16_t *)(rec + 0x42)) {
            view_draw_unit_sprite((int)(intptr_t)dword_53A49 + 32904, 456, i,
                                  2, 253);
            flag = 1;
        }
    }
    gfx_copy_rows((void *)0xA0504, 320, dword_53A49 + 32904, 456, 0x138,
                  192);
    if (flag != 0)
        svc_play_sfx(dword_53EEC, 4, 1);
    svc_wait_ticks(0);

    /* pass 2: raise each such record's HP by max/5, clamped at max. */
    for (i = 0; i < dword_53BEB; i++) {
        uint8_t *rec = rec_at(i);
        int hp = *(uint16_t *)(rec + 0x40);
        int max = *(uint16_t *)(rec + 0x42);

        if (rec[6] == 2 && (rec[5] & 0x81) == 0 && rec[37] == 0 &&
            rec[38] == 0 && hp != max) {
            int raised = max / 5 + hp;

            if (raised > max)
                raised = max;
            *(uint16_t *)(rec + 0x40) = (uint16_t)raised;
            view_draw_unit_sprite((int)(intptr_t)dword_53A49 + 32904, 456, i,
                                  0, 0);
            rec_flag_or80(i);
        }
    }
    gfx_copy_rows((void *)0xA0504, 320, dword_53A49 + 32904, 456, 0x138,
                  192);
    svc_wait_ticks(0);
    map_view_update(0);
    party_change_wave_dispatch(1);
    party_change_class_apply(1);

    if (dword_53ECC != 0)
        return;                             /* 0x10B46 shared epilogue */
    reveal_bank_load();
    map_turn_side1();
    reveal_bank_free();
    if (dword_53ECC != 0)
        return;
    if (byte_51E63[dword_53C03] != byte_51E81[dword_53C03])
        bgm_play(-1, 0);
    reveal_mosaic_out(82);
    j___delay(20);
    reveal_mosaic_in(82);
    ev5_13536();
    map_view_update(0);
    party_change_wave_dispatch(0);
    party_change_class_apply(0);
    if (dword_53ECC != 0)
        return;
    bgm_play((uint8_t)byte_51E81[dword_53C03], 0);
    reveal_bank_load();
    map_turn_side0();
    reveal_bank_free();
    if (dword_53ECC != 0)
        return;

    {
        uint8_t before = byte_51E63[dword_53C03];
        uint8_t after = byte_51E81[dword_53C03];

        dword_53BEF++;
        if (before != after)
            bgm_play(-1, 0);
        reveal_mosaic_out(80);
        j___delay(150);
        reveal_mosaic_in(80);
        dword_51A83 = 0;
        ev5_13536();
        map_view_update(0);
        bgm_play((uint8_t)byte_51E63[dword_53C03], 0);
        for (k = 0; k < 9; k++) {
            void *saved = res_draw_subimage(dword_53A81, (void *)0xA0000, 320,
                                            120, 84, k + 83);

            if (k > 6)
                dlg_draw_number((void *)0xA726B, 320, (int)dword_53BEF, 42, 3);
            j___delay(70);
            if (k == 8)
                j___delay(500);
            dlg_snap_restore(saved, (void *)0xA0000, 320);
        }
        for (m = 2; m < 6; m++) {
            void *saved;

            if (m == 5)
                m = 9;
            saved = res_draw_subimage(dword_53A81, dword_53A49 + 32904, 456,
                                      116, m * m + 84, 91);
            dlg_draw_number(dword_53A49 + 0x812F + 456 * m * m, 456,
                            (int)dword_53BEF, 42, 3);
            gfx_copy_rows((void *)0xA0504, 320, dword_53A49 + 32904, 456,
                          0x138, 192);
            svc_wait_ticks(1);
            dlg_snap_restore(saved, dword_53A49 + 32904, 456);
        }
        map_view_update(0);
        j___delay(200);
        dword_53AE9 = 0;
        party_change_wave_dispatch(2);
        party_change_class_apply(2);
        dword_51A83 = 1;
        dlg_portrait_glide(0);
        kbd_flush();
    }
}

/* 0x13565 - run the party-change map sequence only when the party is settled:
 * scan the 80-byte records at dword_53A45 (count dword_53BEB) for one whose +5
 * flag byte has neither bit 0 nor bit 7 set, whose +6 status class is 2 and
 * whose +38 ailment counter is zero.  While any such record remains the
 * sequence is skipped; otherwise it is bracketed by byte_51AAC / dword_51A83
 * being cleared and restored around party_change_map_seq().  The Watcom stack
 * probe (`push 8; call 0x3702F`) allocates no frame and is dropped. */
void party_change_maybe_run(void)
{
    int i;
    int needs_seq = 1;

    for (i = 0; i < dword_53BEB; i++) {
        const uint8_t *rec = rec_at(i);

        if ((rec[5] & 0x81) == 0 && rec[6] == 2 && rec[38] == 0)
            needs_seq = 0;
    }
    if (needs_seq) {
        byte_51AAC = 0;
        dword_51A83 = 0;
        party_change_map_seq();
        dword_51A83 = 1;
        byte_51AAC = 1;
    }
}
