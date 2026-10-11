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
 * (0x231DF/0x231C6 = vm_run(stream,sub,VGA_P(...)) + unit_refresh + inc dword_53C03;
 * 0x237C8 = unit_add(n) + inc; 0x231F2 = inc), so the C spells the behaviour
 * out per handler instead of reproducing the jump layout (rounds/30 precedent).
 *
 * 0xD4E/0x135DD/0x1366A/0x112A5/0x11506 are the scene/unit services already
 * owned by other modules (verified assets or yet-to-migrate ones); this file
 * only calls them by name.
 */
#include "fd2_game.h"
#include "scene_states.h"

#define VGA_BASE fd2_vga

#include <stdint.h>
#include <string.h>

/* vm_run with the fixed tail tuple used by every state handler:
 * (stream, sub, VGA_P(0xA0000u), 320, 205, 76, 74, 19, 1). */
static uint8_t *vm(int sub)
{
    return vm_run(dword_53A79, sub, fd2_vga, 320, 205, 76, 74, 19, 1);
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


/* ---- batch 5 ---------------------------------------------------------- */

/* 0x23CD5 - funcs_25E23[17]
 * Tables: 17-byte arrays at 0x521C3/0x521D4/0x521E5 (17 entries), origin =
 * 18 | (4 << 32); unit_refresh runs BEFORE place_records (unlike most). */
void scene_state_17(void)
{
    const uint8_t *xs   = (const uint8_t *)&fd2_data1[0x21C3];
    const uint8_t *ys   = (const uint8_t *)&fd2_data1[0x21D4];
    const uint8_t *kind = (const uint8_t *)&fd2_data1[0x21E5];

    sub_11506();                                    /* unit_refresh */
    scene_place_records(xs, ys, (uintptr_t)kind, 0, 16, 17, 25, 8, 1,
                        ((uint64_t)4u << 32) | 18u);
    vm(7);
    dword_51A83 = 0;
    sub_1366A(56);
    vm(8);
    dword_51A83 = 0;
    sub_1366A(57);
    vm(9);
    dword_51A83 = 0;
    sub_1366A(58);
    vm(10);
    sub_112A5(21);                                  /* unit_add */
    sub_112A5(7);
    dword_53C03++;
}

/* 0x244B6 - funcs_25E23[21]
 * Tables 0x52273/0x52283/0x52293 (16 bytes each), origin = 16 | (18 << 32).
 * Ends with a screen clear, fade and unit_refresh (shared tail 0x239AC). */
void scene_state_21(void)
{
    const uint8_t *xs   = (const uint8_t *)&fd2_data1[0x2273];
    const uint8_t *ys   = (const uint8_t *)&fd2_data1[0x2283];
    const uint8_t *kind = (const uint8_t *)&fd2_data1[0x2293];

    scene_place_records(xs, ys, (uintptr_t)kind, 0, 15, 72, 22, 25, 2,
                        ((uint64_t)18u << 32) | 16u);
    vm(4);
    sub_1366A(65);
    vm(5);
    sub_135DD(16, 16);
    sub_1366A(66);
    vm(6);
    sub_135DD(16, 14);
    sub_24618(dword_53AB9, dword_53ABD + 3, 10, 8);
    j___delay(500);
    memset((void *)(uintptr_t)VGA_BASE, 255, 320u * 200u);
    sub_1F882();
    memset((void *)(uintptr_t)VGA_BASE, 0, 320u * 200u);
    sub_11506();                                    /* unit_refresh (tail) */
    dword_53C03++;
}

/* 0x24E80 - funcs_25E23[25]
 * Repositions every record from index 16 whose kind byte (+7) is 31 to (16,6),
 * places 16 records (origin = 9 | (5 << 32)) and plays a scripted sequence.
 * The VM sub-streams depend on dword_53AD5[12] (the 0x20-byte buffer).
 * Tail 0x15487 is a foreign epilogue (returns eax); the caller ignores it. */
void scene_state_25(void)
{
    const uint8_t *xs   = (const uint8_t *)&fd2_data1[0x22D6];
    const uint8_t *ys   = (const uint8_t *)&fd2_data1[0x22E6];
    const uint8_t *kind = (const uint8_t *)&fd2_data1[0x22F6];
    uint8_t *base = (uint8_t *)(uintptr_t)dword_53A45;
    int i, sub7;

    for (i = 16; i < (int)dword_53BEB; ++i) {
        uint8_t *rec = base + 80u * (uint32_t)i;
        if (rec[7] == 31) {
            rec[0] = 16;
            rec[1] = 6;
        }
    }
    scene_place_records(xs, ys, (uintptr_t)kind, 0, 15, 0, 0, 0, 0,
                        ((uint64_t)5u << 32) | 9u);
    sub7 = (int)((uint8_t *)(uintptr_t)dword_53AD5)[12];
    vm(sub7 + 5);
    dword_51A83 = 0;
    sub_1366A(77);
    vm(7);
    dword_51A83 = 0;
    sub_1366A(78);
    vm(sub7 + 8);
    dword_51A83 = 0;
    sub_1366A(79);
    vm(10);
    dword_51A83 = 0;
    sub_1366A(80);
    vm(11);
    sub_11506();                                    /* unit_refresh */
    dword_53C03++;
}

/* 0x250CC - funcs_25E23[26]
 * Clears record byte +5 for records 0..15, places 16 records from 0x52306/0x52316
 * (kind literal 2, origin = 9 | (8 << 32)) and branches on sub_24B14(100):
 *   != -1 : long scripted teardown, then unit_refresh, ++state, sub_25089(), return
 *   == -1 : alternate scripted sequence ending in a deliberate infinite loop
 *           (the original really is `while (1);` - end-of-demo/post-battle branch).
 */
void scene_state_26(void)
{
    const uint8_t *xs = (const uint8_t *)&fd2_data1[0x2306];
    const uint8_t *ys = (const uint8_t *)&fd2_data1[0x2316];
    uint8_t tmp[16];                                /* the original's v7 local */
    uint8_t *base = (uint8_t *)(uintptr_t)dword_53A45;
    int i;

    tmp[0] = fd2_data1[0x2326];
    for (i = 0; i < 16; ++i)
        base[80u * (uint32_t)i + 5] = 0;

    scene_place_records(xs, ys, 2u, 0, 15, 0, 0, 0, 0,
                        ((uint64_t)8u << 32) | 9u);
    vm(8);
    dword_51A83 = 0;
    sub_1366A(82);

    if (sub_24B14(100) != -1) {
        vm(9);
        dword_51A83 = 0;
        sub_1366A(83);
        vm(10);
        dword_51A83 = 0;
        sub_135DD(9, 8);
        sub_1366A(84);
        vm(11);
        sub_25052(80, 5);
        vm(12);
        sub_25052(80, 4);
        j___delay(500);
        sub_25052(80, 3);
        j___delay(250);
        sub_25052(80, 2);
        j___delay(100);
        sub_25052(80, 2);
        j___delay(50);
        sub_25052(80, 2);
        sub_24618(dword_53AB9, dword_53ABD - 1, 10, 10);
        j___delay(500);
        memset((void *)(uintptr_t)VGA_BASE, 255, 320u * 200u);
        sub_1F882();
        memset((void *)(uintptr_t)VGA_BASE, 0, 320u * 200u);
        sub_11506();                                /* unit_refresh */
        dword_53C03++;
        sub_25089();
        return;
    }

    vm(13);
    dword_51A83 = 0;
    sub_1366A(84);
    vm(14);
    dword_51A83 = 0;
    sub_1366A(82);
    vm(15);
    dword_51A83 = 0;
    sub_1C2DA(0, 19, 1, tmp);
    sub_22253(1, 255, 255, base[0], base[1]);
    vm(16);
    dword_51A83 = 0;
    sub_25089();
    sub_31529();
    for (;;)
        ;                                           /* deliberate hang (original) */
}


/* ---- batch 6 ---------------------------------------------------------- */

/* 0x23E74 - funcs_25E23[19]
 * A 32-byte literal holds interleaved x/y (x = [0..15], y = [16..31]) for
 * records 0..15 (kind 1); a second 9-entry set (xs 0x52216, ys literal) writes
 * records at +4160 with kind 3. Origin qword = 0x1F0000001A. */
void scene_state_19(void)
{
    static const uint8_t xy[32] = {
        '!','!','!','"','"','"','#','#','#','#','#','$','$','$','$','$',
        '#','$','"','"','#','$','!','"','#','$','%','!','"','#','$','%'
    };
    static const uint8_t ys2[9] = { '#','%','$','#','"','!','$','#','"' };
    const uint8_t *xs2 = (const uint8_t *)&fd2_data1[0x2216];
    uint8_t *base = (uint8_t *)(uintptr_t)dword_53A45;
    int i, j;

    sub_1F882();
    sub_13536();
    for (i = 0; i < 16; ++i) {
        uint8_t *rec = base + 80u * (uint32_t)i;
        rec[0] = xy[i];
        rec[1] = xy[i + 16];
        rec[3] = 1;
    }
    for (j = 0; j < 9; ++j) {
        uint8_t *rec = base + 80u * (uint32_t)j + 4160;
        rec[0] = xs2[j];
        rec[1] = ys2[j];
        rec[3] = 3;
    }
    dword_51A83 = 0;
    qword_53AA9 = 0x1F0000001AULL;
    qword_53AB1 = 0x1F0000001AULL;
    dword_53AB9 = 0;
    dword_53ABD = 0;
    sub_11CAC(1);
    sub_1F525();
    j___delay(200);
    vm(11);
    dword_51A83 = 0;
    sub_1366A(59);
    vm(12);
    dword_51A83 = 0;
    sub_112A5(25);                                  /* unit_add */
    sub_11506();                                    /* unit_refresh */
    if (dword_53BEF <= 15) {
        sub_10B4E(1);
        sub_1366A(60);
        vm(14);
        dword_51A83 = 0;
        sub_1366A(61);
        vm(15);
        dword_51A83 = 0;
        sub_1366A(62);
        vm(16);
        sub_112A5(28);                              /* unit_add */
    }
    vm(13);
    dword_53C03++;
}

/* 0x240FA - funcs_25E23[20]
 * Tables: 25-byte arrays at 0x52228/0x52241/0x5225A (25 entries), origin =
 * 14 | (10 << 32). Counts sub_2AEDB hits over columns 209..214 x 0..15; when all
 * six rows are present it removes them (rec_slot_remove = 0x1B8E7). */
void scene_state_20(void)
{
    const uint8_t *xs   = (const uint8_t *)&fd2_data1[0x2228];
    const uint8_t *ys   = (const uint8_t *)&fd2_data1[0x2241];
    const uint8_t *kind = (const uint8_t *)&fd2_data1[0x225A];
    int i, j, count = 0;

    scene_place_records(xs, ys, (uintptr_t)kind, 0, 24, 25, 23, 14, 1,
                        ((uint64_t)10u << 32) | 14u);
    vm(5);
    for (i = 0xD1; i < 0xD7; ++i)
        for (j = 0; j < 0x10; ++j)
            if (sub_2AEDB(j, i) != -1)
                ++count;

    if (count == 6) {
        for (i = 0xD1; i < 0xD7; ++i)
            for (j = 0; j < 0x10; ++j) {
                int v = sub_2AEDB(j, i);
                if (v != -1)
                    sub_1B8E7(j, v);                /* rec_slot_remove */
            }
        sub_1C220(100);
        vm(7);
        dword_51A83 = 0;
        sub_1366A(63);
        vm(8);
        dword_51A83 = 0;
        sub_1366A(64);
        vm(9);
        sub_24336();
        vm(10);
    } else {
        vm(6);
    }
    sub_112A5(24);                                  /* unit_add */
    sub_112A5(23);
    sub_11506();                                    /* unit_refresh */
    dword_53C03++;
}

/* 0x25757 - funcs_25E23[29]
 * Final handler: 20-entry tables (0x52327/0x5233B/0x5234F), origin =
 * 16 | (18 << 32); then a fade-out loop (sub_11DF2, 63 steps), 40 view waits,
 * two VM cells and a deliberate `while (1);` (original is __noreturn). */
void scene_state_29(void)
{
    const uint8_t *xs   = (const uint8_t *)&fd2_data1[0x2327];
    const uint8_t *ys   = (const uint8_t *)&fd2_data1[0x233B];
    const uint8_t *kind = (const uint8_t *)&fd2_data1[0x234F];
    int i, j;

    scene_place_records(xs, ys, (uintptr_t)kind, 0, 19, 0, 0, 0, 0,
                        ((uint64_t)18u << 32) | 16u);
    vm(9);
    dword_51A83 = 0;
    sub_1366A(88);
    vm(10);
    sub_135DD(16, 18);
    sub_12CEA(22, 23);
    sub_24618(dword_53AB9, dword_53ABD + 1, 10, 8);
    sub_25089();
    dword_53C03++;
    sub_25089();
    dword_51A83 = 0;
    sub_1088D(dword_53C03);                         /* world_load_party */

    qword_53AA9 = 0x50000000BULL;
    qword_53AB1 = 0x50000000BULL;
    dword_53AB9 = 0;
    dword_53ABD = 0;
    sub_11CAC(1);
    for (i = 62; i >= 0; --i) {
        sub_11DF2(0, 255, i);
        j___delay(4);
    }
    for (j = 0; j < 40; ++j) {
        sub_11CAC(0);
        sub_17AA9(1);
    }
    vm(0);
    dword_51A83 = 0;
    sub_135DD(11, 12);
    sub_1366A(89);
    vm(1);
    sub_31529();
    for (;;)
        ;                                           /* deliberate hang (original) */
}


/* ---- batch 7 (completes funcs_25E23) ---------------------------------- */

/* 0x24754 - funcs_25E23[22]
 * 17-entry tables (0x522A3/0x522B4/0x522C5), origin = 14 | (14 << 32); three-way
 * branch on sub_24B14(100)/sub_24BDE(18)/dword_53BEF, then a long transition that
 * reloads FDFIELD.DAT entry 69 + FDSHAP.DAT entries 46/47 and un-obfuscates. */
void scene_state_22(void)
{
    const uint8_t *xs   = (const uint8_t *)&fd2_data1[0x22A3];
    const uint8_t *ys   = (const uint8_t *)&fd2_data1[0x22B4];
    const uint8_t *kind = (const uint8_t *)&fd2_data1[0x22C5];
    int i;

    scene_place_records(xs, ys, (uintptr_t)kind, 0, 16, 17, 21, 21, 2,
                        ((uint64_t)14u << 32) | 14u);
    if (sub_24B14(100) == -1) {
        vm(9);
        dword_51A83 = 0;
        sub_1366A(71);
    } else {
        vm(8);
        sub_112A5(22);                              /* unit_add */
    }
    if (sub_24BDE(18) != 0) {
        vm(10);
        dword_51A83 = 0;
        sub_1366A(72);
        sub_32975(17);                              /* rec_flag_set1 */
        vm(11);
    } else if (dword_53BEF < 15) {
        vm(13);
        sub_112A5(19);                              /* unit_add */
    } else {
        vm(12);
        dword_51A83 = 0;
        sub_1366A(72);
        sub_32975(17);
    }
    sub_11506();                                    /* unit_refresh */
    dword_53C03++;
    vm(14);
    j___delay(400);
    sub_2189A(1, 15, 10);
    sub_24B4D(30);
    vm(15);
    j___delay(400);
    sub_2189A(1, 15, 10);
    sub_24B4D(30);
    vm(16);
    j___delay(400);
    sub_2189A(1, 30, 16);
    for (i = 0; i < 64; i += 2) {
        sub_11DF2(0, 255, i);
        j___delay(4);
    }
    dword_53A51 = res_load("FDFIELD.DAT",
                    (void *)(uintptr_t)dword_53A51, 69);
    dword_53A5D = res_load("FDSHAP.DAT",
                    (void *)(uintptr_t)dword_53A5D, 46);
    dword_53A69 = res_load("FDSHAP.DAT",
                    (void *)(uintptr_t)dword_53A69, 47);
    sub_4DF4C((uint8_t *)(uintptr_t)dword_53A51);   /* util_mask_recolor */
    sub_10652();                                    /* world_load_tiles */
    sub_135DD(14, 29);
    sub_11DF2(0, 255, 0);
    sub_1366A(73);
    sub_135DD(14, 14);
    sub_1366A(73);
    sub_1366A(73);
    vm(17);
}

/* 0x2548C - funcs_25E23[28]
 * Record 20 +7/+8 = 126, a scripted multi-stage scroll (sub_24B4D/sub_361B0 with
 * fixed delays) and a fade-out/fade-in pair; ends with unit_refresh + ++state. */
void scene_state_28(void)
{
    uint8_t *base = (uint8_t *)(uintptr_t)dword_53A45;
    int i;

    vm(10);
    sub_35F10(20);
    base[1607] = 126;
    base[1608] = 126;
    vm(11);
    sub_10B4E(9);
    sub_135DD(9, 8);
    sub_12CEA(15, 10);
    sub_22253((int)dword_53BEB - 1, 15, 10, 15, 10);

    vm(12);
    dword_51A83 = 0;
    sub_24B4D(20);  j___delay(600);
    sub_24B4D(20);  j___delay(600);
    sub_24B4D(20);
    vm(13);
    dword_51A83 = 0;
    sub_24B4D(20);  j___delay(200);
    sub_24B4D(20);  j___delay(200);
    sub_24B4D(20);
    vm(14);
    dword_51A83 = 0;
    sub_24B4D(20);  j___delay(200);
    sub_24B4D(20);  j___delay(100);
    sub_24B4D(40);  j___delay(200);
    sub_361B0();    j___delay(300);
    sub_361B0();    j___delay(300);
    sub_361B0();    j___delay(300);
    vm(15);

    for (i = 0; i < 64; ++i) {
        sub_11DF2(0, 255, i);
        j___delay(4);
    }
    memset((void *)(uintptr_t)VGA_BASE, 0, 320u * 200u);
    j___delay(800);
    for (i = 62; i >= 0; --i) {
        sub_11DF2(0, 255, i);
        j___delay(4);
    }
    sub_11506();                                    /* unit_refresh */
    dword_53C03++;
}
