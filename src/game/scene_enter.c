/* scene_enter.c - funcs_25E3A[] enter-state handlers, batch A (phase 4).
 *
 * Restored from the pseudocode + offline disassembly verification of the shared
 * tails (see scene_states.c for the leave-table counterparts):
 *   0x3312D = vm_run(stream, sub,VGA_P(0xA0000u), 320, 205, 76, 74, 19, 1)
 *             + 0x134E4() + 0x12D7B(0) + retn         (sub pushed by the caller)
 *   0x33594 = push 0; call 0x12D7B; add esp,4; pop ebx; retn
 * The `push 28h; call 0x3702F` prologue is the Watcom stack probe (dropped).
 * Every handler starts with sub_205DA() (= menu_reload_world, verified asset).
 */
#include "fd2_game.h"
#include "scene_enter.h"

#include <stdint.h>

/* vm_run with the fixed tail tuple used by the whole scene family. */
static uint8_t *vm(int sub)
{
    return vm_run(dword_53A79, sub, fd2_vga, 320, 205, 76, 74, 19, 1);
}

/* 0x32FB2 - funcs_25E3A[3] */
void scene_enter_03(void)
{
    sub_205DA();                                    /* menu_reload_world */
    sub_135DD(4, 11);
    sub_1366A(20);
    vm(0);
    dword_51A83 = 0;
    sub_10B4E(1);
    sub_135DD(4, 0);
    j___delay(200);
    vm(1);                                          /* tail 0x3312D */
    sub_134E4();
    sub_12D7B(0);
}

/* 0x33049 - funcs_25E3A[4] */
void scene_enter_04(void)
{
    sub_205DA();
    vm(0);
    dword_51A83 = 0;
    sub_135DD(3, 3);
    j___delay(200);
    sub_10B4E(1);
    sub_11CAC(0);
    j___delay(200);
    sub_1366A(22);
    vm(1);
    dword_51A83 = 0;
    sub_135DD(8, 14);
    sub_1366A(21);
    vm(2);
    sub_134E4();
    sub_12D7B(0);
}

/* 0x33C9D - funcs_25E3A[27] */
void scene_enter_27(void)
{
    int i;

    sub_205DA();
    for (i = 0; i < 20; ++i)
        sub_32975(i);                               /* rec_flag_set1 */
    sub_135DD(29, 15);
    sub_24618(dword_53AB9 + 6, dword_53ABD + 5, 10, 8);
    for (i = 0; i < 20; ++i) {
        uint8_t *rec = (uint8_t *)(uintptr_t)dword_53A45 + 80u * (uint32_t)i;
        if (*(const uint16_t *)(const void *)(rec + 64) != 0)
            rec[5] = 0;
    }
    sub_11CAC(0);
    sub_11DF2(0, 255, 0);
    j___delay(500);
    sub_1366A(85);
    sub_1366A(85);
    sub_1366A(85);
    sub_134E4();
    vm(0);
    dword_51A83 = 0;
    sub_35B78(0, 16, 6);
    sub_35B78(7, 16, 7);
    dword_51A83 = 1;
    sub_12D7B(0);                                   /* tail 0x33594 */
}

/* 0x33DBA - funcs_25E3A[28] */
void scene_enter_28(void)
{
    sub_205DA();
    dword_51A83 = 0;
    sub_135DD(9, 56);
    sub_1366A(86);
    vm(7);
    sub_35B78(9, 19, 8);
    vm(8);                                          /* tail 0x3312D */
    sub_134E4();
    sub_12D7B(0);
}


/* ---- batch B ---------------------------------------------------------- */

/* 0x32D18 - funcs_25E3A[1]
 * Tail 0x33206 = vm_run(sub already pushed) + jmp 0x33140 (0x12D7B(0)). */
void scene_enter_01(void)
{
    sub_205DA();
    sub_135DD(13, 11);
    sub_1366A(9);
    j___delay(50);
    vm(0);
    dword_51A83 = 0;
    j___delay(200);
    sub_1366A(10);
    j___delay(200);
    vm(1);
    dword_51A83 = 0;
    j___delay(200);
    sub_10B4E(1);
    sub_11CAC(0);
    j___delay(200);
    sub_1366A(11);
    vm(2);
    dword_51A83 = 0;
    sub_134E4();
    sub_135DD(6, 12);
    byte_53AFA = 1;
    sub_10B4E(2);
    byte_53AFA = 0;
    sub_1366A(12);
    sub_134E4();
    vm(3);                                          /* tail 0x33206 */
    sub_12D7B(0);
}

/* 0x32E8C - funcs_25E3A[2] */
void scene_enter_02(void)
{
    sub_205DA();
    sub_135DD(3, 17);
    j___delay(200);
    vm(0);
    dword_51A83 = 0;
    sub_1366A(18);
    sub_10B4E(1);
    sub_135DD(3, 6);
    j___delay(200);
    sub_1366A(17);
    vm(1);
    dword_51A83 = 0;
    sub_1366A(19);
    vm(2);
    dword_51A83 = 0;
    sub_135DD(3, 17);
    vm(3);                                          /* tail 0x3312D */
    sub_134E4();
    sub_12D7B(0);
}

/* 0x33E3C - funcs_25E3A[29]
 * Tail 0x3313B = 0x134E4() + 0x12D7B(0) (no extra vm_run). */
void scene_enter_29(void)
{
    sub_205DA();
    sub_1366A(87);
    sub_135DD(16, 19);
    vm(0);
    dword_51A83 = 0;
    sub_135DD(16, 1);
    sub_33F78(21, 21, 5);
    sub_33F78(22, 23, 5);
    sub_33F78(23, 20, 5);
    sub_33F78(24, 24, 5);
    vm(1);
    sub_361B0();
    vm(2);
    dword_51A83 = 0;
    sub_135DD(16, 14);
    sub_33F78(24, 22, 18);
    sub_33F78(25, 21, 18);
    sub_33F78(26, 23, 18);
    dword_51A83 = 1;
    sub_134E4();                                    /* tail 0x3313B */
    sub_12D7B(0);
}


/* ---- batch C ---------------------------------------------------------- */

/* 0x336A0 - funcs_25E3A[22]
 * Marks records 0..15 (rec_flag_set1), places 16 records, hides those with a
 * non-zero u16 at +64 (clearing +5 and setting kind +3 = 2), then a long
 * scripted sequence ending in vm(4) + 0x134E4 + 0x12D7B(0) (tail 0x33594). */
void scene_enter_22(void)
{
    int i;

    sub_205DA();
    for (i = 0; i < 16; ++i)
        sub_32975(i);
    sub_135DD(14, 32);
    sub_24618(dword_53AB9 + 6, dword_53ABD + 5, 10, 8);
    /* The machine reloads ds:0x3a45 fresh on every iteration (0x33707 a1 45
     * 3a 00 00): sub_205DA above frees and reallocates that table via
     * world_load_party, so a pointer captured before the call is stale/null. */
    for (i = 0; i < 16; ++i) {
        uint8_t *rec = (uint8_t *)(uintptr_t)dword_53A45 + 80u * (uint32_t)i;
        if (*(const uint16_t *)(const void *)(rec + 64) != 0) {
            rec[5] = 0;
            rec[3] = 2;
        }
    }
    sub_11CAC(0);
    sub_11DF2(0, 255, 0);
    j___delay(500);
    vm(0);
    sub_135DD(14, 29);
    sub_1366A(68);
    vm(1);
    sub_1366A(69);
    vm(2);
    sub_1366A(70);
    vm(3);
    dword_51A83 = 0;
    sub_135DD(14, 13);
    sub_10B4E(1);
    j___delay(200);
    sub_11DF2(0, 255, 255);
    j___delay(100);
    sub_11CAC(0);
    sub_11DF2(0, 255, 0);
    j___delay(500);
    /* Same live reload at 0x3387C / 0x33886 (a1 45 3a 00 00 before each
     * write); sub_10B4E() and the vm() calls in between may replace it. */
    ((uint8_t *)(uintptr_t)dword_53A45)[1283] = 2;  /* record 16, kind +3 */
    ((uint8_t *)(uintptr_t)dword_53A45)[1363] = 2;  /* record 17, kind +3 */
    vm(4);
    sub_134E4();
    sub_12D7B(0);                                   /* tail 0x33594 */
}

/* 0x33AF1 - funcs_25E3A[26]
 * Optional vm(3) when sub_24B14(100) != -1, then three scroll steps with
 * sub_24618 moves and palette resets; tail 0x3312D with sub = 7. */
void scene_enter_26(void)
{
    sub_205DA();
    sub_135DD(9, 49);
    sub_1366A(76);
    vm(0);
    if (sub_24B14(100) != -1)
        vm(3);
    vm(4);
    sub_135DD(9, 49);
    sub_24618(dword_53AB9, dword_53ABD + 3, 2, 2);
    sub_11DF2(0, 255, 0);
    vm(5);
    sub_24618(dword_53AB9, dword_53ABD, 2, 2);
    sub_11DF2(0, 255, 0);
    sub_1366A(81);
    vm(6);
    sub_24618(dword_53AB9 + 2, dword_53ABD, 2, 2);
    sub_11DF2(0, 255, 0);
    vm(7);                                          /* tail 0x3312D */
    sub_134E4();
    sub_12D7B(0);
}


/* ---- batch D (completes funcs_25E3A) --------------------------------- */

/* 0x3231B - funcs_25E3A[0]
 * The longest enter handler: three scripted sections. It sets dword_53C03 to 32,
 * then 31, then 0 (the state index is reused as a script selector), runs 15/13
 * sub_13185(2) steps, plays the full vm sequence with BGM switches
 * (sub_25977(-1,0) / (11,0)) and closes with sub_134E4 + sub_12D7B(0) and
 * dword_53BF3 = 0. */
void scene_enter_00(void)
{
    int i;

    dword_53C03 = 32;
    sub_205DA();
    sub_135DD(3, 34);
    sub_1366A(99);
    for (i = 0; i < 15; ++i)
        sub_13185(2);
    vm(0);
    dword_51A83 = 0;
    for (i = 0; i < 13; ++i)
        sub_13185(2);
    vm(1);
    dword_51A83 = 0;
    sub_25977(-1, 0);
    dword_53AFB = 1;
    sub_1366A(100);
    dword_53AFB = 0;
    sub_135DD(0, 43);
    sub_25977(11, 0);
    sub_1F525();
    sub_1366A(101);
    vm(2);
    dword_51A83 = 0;
    sub_1366A(102);
    vm(3);
    dword_51A83 = 0;
    sub_1366A(103);
    vm(4);
    dword_51A83 = 0;
    sub_1366A(104);
    vm(5);
    dword_53AFB = 1;
    dword_51A83 = 0;
    sub_1366A(105);
    dword_53AFB = 0;

    dword_53C03 = 31;
    sub_205DA();
    dword_51A83 = 0;
    sub_135DD(5, 42);
    sub_10B4E(1);
    sub_1366A(90);
    vm(0);
    dword_51A83 = 0;
    sub_1366A(91);
    vm(1);
    dword_51A83 = 0;
    sub_1366A(92);
    vm(2);
    dword_51A83 = 0;
    sub_10B4E(3);
    sub_135DD(4, 41);
    vm(3);
    dword_51A83 = 0;
    sub_1366A(93);
    vm(4);
    dword_51A83 = 0;
    sub_32975(2);
    sub_10B4E(5);
    vm(5);
    dword_51A83 = 0;
    sub_1366A(94);
    vm(6);
    dword_51A83 = 0;
    sub_1366A(95);
    vm(7);
    dword_51A83 = 0;
    sub_1366A(96);
    vm(8);
    dword_51A83 = 0;
    sub_1366A(97);
    vm(9);
    sub_25977(-1, 0);
    dword_51A83 = 0;
    dword_53AFB = 1;
    sub_1366A(98);
    dword_53AFB = 0;

    dword_53C03 = 0;
    sub_112A5(0);                                   /* unit_add */
    sub_112A5(9);
    sub_112A5(4);
    sub_112A5(30);
    sub_205DA();
    dword_51A83 = 0;
    sub_135DD(4, 12);
    sub_1366A(0);
    j___delay(200);
    vm(0);
    dword_51A83 = 0;
    j___delay(200);
    sub_135DD(0, 0);
    sub_32999(1);
    sub_1366A(1);
    sub_135DD(0, 15);
    sub_32999(2);
    sub_1366A(2);
    vm(1);
    dword_51A83 = 0;
    j___delay(200);
    sub_1366A(5);
    sub_32975(9);
    sub_11CAC(0);
    j___delay(100);
    vm(2);
    sub_134E4();
    sub_12D7B(0);
    dword_53BF3 = 0;
}
