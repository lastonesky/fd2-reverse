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

#define VGA_BASE 0xA0000u

#include <stdint.h>
#include <string.h>

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


/* ---- batch 3 ---------------------------------------------------------- */

/* 0x232E8 - funcs_25E23[6]
 * Tables: xs 0x520E4, ys 0x520ED, kind 0x520F6 (9 bytes each). The condition
 * reads dword_53AD5[17] (the 0x20-byte buffer main allocated) and rec_flag(43).
 * origin = 6 | (2 << 32). */
void scene_state_06(void)
{
    const uint8_t *xs   = (const uint8_t *)&fd2_data1[0x20E4];
    const uint8_t *ys   = (const uint8_t *)&fd2_data1[0x20ED];
    const uint8_t *kind = (const uint8_t *)&fd2_data1[0x20F6];

    sub_11506();                                    /* unit_refresh */
    if (((uint8_t *)(uintptr_t)dword_53AD5)[17] != 1 || rec_flag(43) != 0) {
        vm(5);
    } else {
        scene_place_records(xs, ys, (uintptr_t)kind, 0, 8, 43, 12, 7, 2,
                            ((uint64_t)2u << 32) | 6u);
        vm(4);
        sub_112A5(12);                              /* unit_add */
    }
    dword_53C03++;
}

/* 0x234BB - funcs_25E23[7]
 * Tables: xs 0x520FF, ys 0x52109 (10 bytes each); kind is the literal 2.
 * origin = 8 | (14 << 32). Ends by unit_add(5) (shared tail 0x2327D). */
void scene_state_07(void)
{
    const uint8_t *xs = (const uint8_t *)&fd2_data1[0x20FF];
    const uint8_t *ys = (const uint8_t *)&fd2_data1[0x2109];

    scene_place_records(xs, ys, 2u, 0, 9, 28, 14, 16, 0,
                        ((uint64_t)14u << 32) | 8u);
    vm(3);
    dword_51A83 = 0;
    sub_1366A(33);
    vm(4);
    dword_53AFB = 1;
    dword_51A83 = 0;
    sub_1366A(34);
    dword_53AFB = 0;
    sub_11D40(0, 255, 64);
    memset((void *)(uintptr_t)VGA_BASE, 0, 320u * 200u);
    sub_112A5(5);                                   /* unit_add (tail arg) */
    dword_53C03++;
}

/* 0x237D5 - funcs_25E23[11]
 * Tables: xs 0x52129, ys 0x52137, kind 0x52145 (14 bytes each; 14 entries).
 * origin = 4. Ends by unit_add(17) (shared tail 0x239B1). */
void scene_state_11(void)
{
    const uint8_t *xs   = (const uint8_t *)&fd2_data1[0x2129];
    const uint8_t *ys   = (const uint8_t *)&fd2_data1[0x2137];
    const uint8_t *kind = (const uint8_t *)&fd2_data1[0x2145];

    scene_place_records(xs, ys, (uintptr_t)kind, 0, 13, 14, 10, 2, 0, 4u);
    vm(3);
    sub_1366A(45);
    vm(4);
    sub_11506();                                    /* unit_refresh */
    sub_112A5(17);                                  /* unit_add */
    dword_53C03++;
}

/* 0x238DC - funcs_25E23[13]
 * Tables: xs 0x52153, ys 0x52163, kind 0x52173 (16 bytes each; 16 entries).
 * origin = 12 | (10 << 32). */
void scene_state_13(void)
{
    const uint8_t *xs   = (const uint8_t *)&fd2_data1[0x2153];
    const uint8_t *ys   = (const uint8_t *)&fd2_data1[0x2163];
    const uint8_t *kind = (const uint8_t *)&fd2_data1[0x2173];

    sub_10B4E(1);
    scene_place_records(xs, ys, (uintptr_t)kind, 0, 15, 0, 0, 0, 0,
                        ((uint64_t)10u << 32) | 12u);
    vm(2);
    dword_51A83 = 0;
    sub_1366A(47);
    vm(3);
    sub_11506();                                    /* unit_refresh */
    dword_53C03++;
}

/* 0x24C1E - funcs_25E23[23]
 * Scroll-in: rows 2..9 with 30 (view, wait) pairs each, then rows 9..15 with a
 * 12-step palette fade (sub count v3 increments per round). */
void scene_state_23(void)
{
    int i, k, fade;

    vm(2);
    dword_51A83 = 0;
    for (i = 2; i < 10; ++i) {
        sub_24D22(i);
        for (k = 0; k < 30; ++k) {
            sub_11CAC(1);
            sub_17AA9(1);
        }
    }
    vm(3);
    dword_51A83 = 0;
    fade = 0;
    while (i < 15) {
        sub_24D22(i);
        for (k = 0; k < 12; ++k) {
            sub_11D40(0, 255, fade);
            sub_11CAC(0);
            sub_17AA9(1);
            ++fade;
        }
        ++i;
    }
    memset((void *)(uintptr_t)VGA_BASE, 0, 320u * 200u);
    sub_11506();                                    /* unit_refresh */
    dword_53C03++;
}


/* ---- batch 4 ---------------------------------------------------------- */

/* 0x22F37 - funcs_25E23[1]
 * Scans records 5..10 for byte+5 bit0; two different VM cells, then a scripted
 * sequence with three fixed delays; ends by SETTING state 2. */
void scene_state_01(void)
{
    int i, flag = 0;

    for (i = 5; i < 11; ++i) {
        const uint8_t *rec = (const uint8_t *)(uintptr_t)dword_53A45
                           + 80u * (uint32_t)i;
        if ((rec[5] & 1) != 0)
            flag = 1;
    }
    if (flag != 0) {
        vm(7);
    } else {
        vm(6);
        sub_1C220(198);
    }
    sub_135DD(14, 2);
    sub_10B4E(4);
    j___delay(100);
    sub_1366A(14);
    vm(8);
    dword_51A83 = 0;
    j___delay(200);
    sub_1366A(15);
    vm(9);
    dword_51A83 = 0;
    j___delay(200);
    sub_135DD(14, 1);
    j___delay(200);
    sub_1366A(16);
    j___delay(200);
    vm(10);
    sub_112A5(8);                                   /* unit_add */
    sub_11506();                                    /* unit_refresh */
    dword_53C03 = 2;                                /* SET, not ++ */
}

/* 0x235F9 - funcs_25E23[9]
 * Writes 11 records (x/y from the 0x52113 table and an 11-byte literal) plus
 * three records at +4000/+4080/+4160, sets the view origin qwords and plays a
 * scripted sequence. */
void scene_state_09(void)
{
    const uint8_t *xs = (const uint8_t *)&fd2_data1[0x2113];
    static const uint8_t ys_lit[11] = { 0x26, 0x27, 0x26, 0x26, 0x27,
                                        0x26, 0x27, 0x27, 0x28, 0x28, 0x28 };
    uint8_t *base = (uint8_t *)(uintptr_t)dword_53A45;
    int i;

    sub_1F882();
    sub_13536();
    for (i = 0; i < 11; ++i) {
        uint8_t *rec = base + 80u * (uint32_t)i;
        rec[0] = xs[i];
        rec[1] = ys_lit[i];
        rec[3] = 2;
    }
    base[4000] = 15;  base[4001] = 35;  base[4038] = 0;   /* record 50 */
    base[4080] = 14;  base[4081] = 35;  base[4118] = 0;   /* record 51 */
    base[4160] = 16;  base[4161] = 35;  base[4165] = 0;   /* record 52 */
    base[405]  = 0;                                       /* record 5, byte +5 */

    dword_51A83 = 0;
    qword_53AA9 = 0x2200000009ULL;
    qword_53AB1 = 0x2200000009ULL;
    dword_53AB9 = 0;
    dword_53ABD = 0;
    sub_11CAC(1);
    sub_1F525();
    j___delay(200);
    vm(4);
    dword_51A83 = 0;
    sub_1366A(37);
    vm(5);
    sub_11506();                                    /* unit_refresh */
    sub_112A5(11);                                  /* unit_add */
    sub_112A5(6);
    dword_53C03++;
}

/* 0x23A0A - funcs_25E23[15]
 * Places 16 records, counts how many of the ids 66..73 exist (>4 sets a flag),
 * then chooses a VM cell on (dword_53BEF > 18 | flag | row<320). */
void scene_state_15(void)
{
    const uint8_t *xs = (const uint8_t *)&fd2_data1[0x2183];
    const uint8_t *ys = (const uint8_t *)&fd2_data1[0x2193];
    int i, count = 0, flag;

    scene_place_records(xs, ys, 0u, 0, 15, 65, 28, 30, 2,
                        ((uint64_t)25u << 32) | 22u);
    for (i = 0; i < 8; ++i) {
        if (rec_flag(i + 66) != 0)
            ++count;
    }
    flag = (count > 4);
    sub_11506();                                    /* unit_refresh */
    if (dword_53BEF > 18 || flag == 1 ||
        *(const uint16_t *)(const void *)(uintptr_t)dword_53A45 < 0x140u) {
        vm(2);
        dword_51A83 = 0;
        sub_1366A(49);
        vm(3);
    } else {
        vm(4);
        sub_112A5(18);                              /* unit_add */
    }
    dword_53C03++;
}

/* 0x23B5F - funcs_25E23[16]
 * unit_exists(18) chooses between a scripted sequence and placing 16 records
 * (xs 0x521A3, ys 0x521B3, kind 0, origin = 17 | (17 << 32)). */
void scene_state_16(void)
{
    const uint8_t *xs = (const uint8_t *)&fd2_data1[0x21A3];
    const uint8_t *ys = (const uint8_t *)&fd2_data1[0x21B3];

    sub_11506();                                    /* unit_refresh */
    if (sub_33499(18) != 0) {
        vm(5);
        dword_51A83 = 0;
        sub_135DD(17, 14);
        sub_10B4E(3);
        sub_1366A(52);
    } else {
        scene_place_records(xs, ys, 0u, 0, 15, 52, 23, 23, 2,
                            ((uint64_t)17u << 32) | 17u);
        vm(7);
        dword_51A83 = 0;
        sub_1366A(50);
        sub_135DD(17, 14);
        sub_10B4E(3);
        sub_1366A(51);
    }
    vm(6);
    sub_1366A(53);
    vm(8);
    sub_112A5(16);                                  /* unit_add */
    dword_53C03++;
}
