/* party_change_seq.c - see party_change_seq.h.
 *
 * Direct translations of the machine code at 0x1A813 and 0x1A866.  The
 * disassembly is the authority: Hex-Rays spells the frame index of the first
 * pass's compared class as an uninitialised local `v10`, but the instruction
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
