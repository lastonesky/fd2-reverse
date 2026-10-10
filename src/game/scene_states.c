/* scene_states.c - funcs_25E23[] transition handlers, batch 1 (phase 4).
 *
 * Handlers restored here (each verified instruction-by-instruction against
 * offline disassembly of analysis/seg_cseg01.bin):
 *
 *   idx 5  0x23296  scene_state_05   unit_add(13); 0x10B4E(3); 0x135DD(5,14);
 *                                    0x1366A(27); vm(6); refresh; ++state
 *   idx 8  0x235BC  scene_state_08   rec[885]=0; 0x135DD(6,1); 0x10B4E(4);
 *                                    0x1366A(36); vm(4); refresh; ++state
 *   idx 24 0x24DF2  scene_state_24   vm(6); 0x135DD(4,16); 0x10B4E(2);
 *                                    0x1366A(75); vm(7); unit_add(26); refresh;
 *                                    unit_add(29); ++state
 *   idx 27 0x25464  scene_state_27   vm(7); refresh; ++state  (empty body)
 *
 * The `push 28h; call 0x3702F` prologue is the Watcom stack probe (dropped).
 * The machine code ends these in shared tails reached by `jmp`
 * (0x231DF/0x231C6 = vm_run(stream,sub,...) + unit_refresh + inc dword_53C03;
 * 0x237C8 = unit_add(n) + inc; 0x231F2 = inc), so the C spells the behaviour
 * out per handler instead of reproducing the jump layout (rounds/30 precedent).
 *
 * 0xD4E/0x135DD/0x1366A/0x112A5/0x11506 are the scene/unit services already
 * owned by other modules (verified assets or yet-to-migrate ones); this file
 * only calls them by name.
 */
#include "fd2_game.h"
#include "scene_states.h"

#include <stdint.h>

/* vm_run with the fixed tail tuple used by every state handler:
 * (stream, sub, 0xA0000, 320, 205, 76, 74, 19, 1). */
static int vm(int sub)
{
    return sub_15F84((int)dword_53A79, sub, 0xA0000, 320, 205, 76, 74, 19, 1);
}

/* 0x23296 - funcs_25E23[5] */
void scene_state_05(void)
{
    sub_112A5(13);
    sub_10B4E(3);
    sub_135DD(5, 14);
    sub_1366A(27);
    vm(6);
    sub_11506();
    dword_53C03++;
}

/* 0x235BC - funcs_25E23[8] */
void scene_state_08(void)
{
    ((uint8_t *)(uintptr_t)dword_53A45)[885] = 0;   /* rec 11, byte +5 */
    sub_135DD(6, 1);
    sub_10B4E(4);
    sub_1366A(36);
    vm(4);
    sub_11506();
    dword_53C03++;
}

/* 0x24DF2 - funcs_25E23[24] */
void scene_state_24(void)
{
    vm(6);
    sub_135DD(4, 16);
    sub_10B4E(2);
    sub_1366A(75);
    vm(7);
    sub_112A5(26);
    sub_11506();
    sub_112A5(29);
    dword_53C03++;
}

/* 0x25464 - funcs_25E23[27]: no service calls of its own */
void scene_state_27(void)
{
    vm(7);
    sub_11506();
    dword_53C03++;
}


/* ---- batch 2 (unblocked by the migrated scene_place_records) ----------- */

/* 0x230F2 - funcs_25E23[2]
 * The original copies three 7-byte tables out of the data segment (0x520BA xs,
 * 0x520C1 ys, 0x520C8 kind) into locals and passes them on; only reads happen,
 * so here the image is passed directly. The rec_flag branch skips placement. */
void scene_state_02(void)
{
    const uint8_t *xs   = (const uint8_t *)&fd2_data1[0x20BA];
    const uint8_t *ys   = (const uint8_t *)&fd2_data1[0x20C1];
    const uint8_t *kind = (const uint8_t *)&fd2_data1[0x20C8];

    sub_11506();                        /* unit_refresh (before the test) */
    if (rec_flag(6) != 0) {
        vm(6);
    } else {
        scene_place_records(xs, ys, (uintptr_t)kind, 0, 6, 0, 0, 0, 0, 2u);
        vm(7);
    }
    dword_53C03++;
}

/* 0x231F9 - funcs_25E23[4]
 * Tables: xs from 0x520CF (7 bytes), ys is a 7-byte literal {11,11,11,9,10,9,10},
 * kind from 0x520DD (7 bytes). Argument roles follow the verified
 * scene_place_records: first=0, last=6, single=41, sx=12, sy=8, skind=0,
 * origin = 6 | (4 << 32) (the two pushed dwords, low first). */
void scene_state_04(void)
{
    const uint8_t *xs   = (const uint8_t *)&fd2_data1[0x20CF];
    static const uint8_t ys_lit[7] = { 11, 11, 11, 9, 10, 9, 10 };
    const uint8_t *kind = (const uint8_t *)&fd2_data1[0x20DD];

    scene_place_records(xs, ys_lit, (uintptr_t)kind, 0, 6, 41, 12, 8, 0,
                        ((uint64_t)4u << 32) | 6u);   /* origin = high<<32 | low */
    vm(9);
    sub_112A5(10);                      /* unit_add */
    sub_11506();                        /* unit_refresh */
    dword_53C03++;
}
