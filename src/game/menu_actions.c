/* menu_actions.c - FD2 menu/party action handlers (see menu_actions.h).
 *
 * All originals are cdecl: the C bodies push the same arguments the machine
 * code does (rec_flag(index), unit_exists(id), vm_run(stream,...), res_load
 * index, ...) and end with the same service calls. External helpers are
 * reached through their fixed game addresses so the differential harness can
 * hook them and the running host sees the repl-patched C.
 */
#include "menu_actions.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "fd2_game.h"   /* de-guest C4: restored cross-module decls */
#include "../include/fd2_globals.h"

/* --- fixed-address helpers --------------------------------------------- */
typedef int  (*flag_fn)(int);
typedef int  (*unit_exists_fn)(int);
typedef void (*h2_fn)(int, int);
typedef void (*h1_fn)(int);
typedef void (*h0_fn)(void);
typedef int  (*vm_fn)(void *, int, int, int, int, int, int, int, int);
typedef int  (*load_fn)(int);


/* --- globals ----------------------------------------------------------- */

#define REC 0x50u

/* --- shared vm tails --------------------------------------------------- */
/* 0x33206/0x3312D: draw one menu sub-stream, then move the portrait back. */
static void menu_vm(int sub)
{
    vm_run((void *)(uintptr_t)dword_53A79, sub,
            0xA0000, 0x140, 0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* loc_3344D */
static void menu_tail_draw0(void)
{
    menu_vm(0);
    dlg_portrait_glide(0);
}

/* 0x3312D is shared, but the sub-stream number is the 8th argument pushed by
 * the *caller* before entering it:
 *   loc_33028 (0x33219, 0x3367E) pushes 1,
 *   loc_3310C (0x33367, 0x335DA) pushes 2,
 *   0x33AAE directly        pushes 0.
 * So the tail proper is "draw sub-stream N, clear the mouth, glide back". */
static void menu_tail_drawN_clear(int sub)
{
    menu_vm(sub);
    dlg_portrait_clear();
    dlg_portrait_glide(0);
}

/* loc_33440 + loc_3344D */
static void menu_tail_step_clear_draw0(int sel)
{
    ev4_1366A(sel);
    dlg_portrait_clear();
    menu_tail_draw0();
}

/* loc_3310C (sub 2) + loc_3312D */
static void menu_tail_step_draw2_clear(int sel)
{
    ev4_1366A(sel);
    menu_tail_drawN_clear(2);
}

/* --- 0x205DA ----------------------------------------------------------- */
void menu_reload_world(void)
{
    dword_51A83 = 0;
    dword_53ECC = 0;
    world_load_party(dword_53C03);
    memset((void *)(uintptr_t)dword_53AD5, 0, 32);
    qword_53AA9 = 0;
    qword_53AB1 = 0;
    dword_53AB9 = 0;
    dword_53ABD = 0;
    map_view_update(1);
    dword_51A83 = 1;
    pal_fade_in();
    dword_53BEF = 1;
    kbd_flush();
}

/* --- menu A: option-denied checks -------------------------------------- */

/* 0x206C5 */
void menu_need_records_marked(void)
{
    int i;
    ev5_205BE();
    for (i = 5; i < 11; i++) {
        if ((*(uint8_t *)(uintptr_t)(dword_53A45 + REC * (uint32_t)i + 5) & 1) == 0)
            return;
    }
    dword_53ECC = 1;
}

/* 0x20707 */
void menu_need_flags_clear_50_51(void)
{
    ev5_205BE();
    if (rec_flag(50) != 0 || rec_flag(51) != 0)
        dword_53ECC = 1;
}

/* 0x2073D */
void menu_need_flag_clear_14(void)
{
    ev5_205BE();
    if (rec_flag(14) != 0)
        dword_53ECC = 1;
}

/* 0x20765 */
void menu_need_any_clear_15_26(void)
{
    int i, any_clear = 0;

    ev5_205BE();
    for (i = 0; i < 12; i++) {
        if (rec_flag(i + 15) == 0)
            any_clear = 1;
    }
    if (any_clear == 0) {
        dword_53ECC = 1;
        menu_vm(0x0A);
    }
    if ((int32_t)dword_53BEF > 5) {
        if (rec_flag(0x3B) != 0) {
            dword_53ECC = 1;
            menu_vm(2);
        }
    }
}

/* 0x20822 */
void menu_need_flag_clear_64(void)
{
    ev5_205BE();
    if (rec_flag(0x40) != 0)
        dword_53ECC = 1;
}

/* 0x2084A */
void menu_need_flag_clear_65(void)
{
    ev5_205BE();
    if (rec_flag(0x41) != 0)
        dword_53ECC = 1;
}

/* 0x20872 */
void menu_need_flag_clear_52_no_unit18(void)
{
    ev5_205BE();
    if (unit_exists(0x12) != 0)
        return;
    if (rec_flag(0x34) != 0) {
        menu_vm(2);
        dword_53ECC = 1;
    }
}

/* 0x20926 */
void menu_need_flag_clear_64_late(void)
{
    ev5_205BE();
    if ((int32_t)dword_53BEF <= 6)
        return;
    if (rec_flag(0x40) != 0)
        dword_53ECC = 1;
}

/* 0x20957 */
void menu_need_any_clear_26_43(void)
{
    int i, any_clear = 0;

    ev5_205BE();
    for (i = 0x26; i < 0x2E; i++) {
        if (rec_flag(i + 15) == 0)
            any_clear = 1;
    }
    if (any_clear == 0) {
        dword_53ECC = 1;
        menu_vm(0x0A);
    }
    if (rec_flag(0) != 0 || rec_flag(0x34) != 0)
        dword_53ECC = 1;

    any_clear = 0;
    for (i = 0x15; i < 0x25; i++) {
        if (rec_flag(i + 15) == 0)
            any_clear = 1;
    }
    for (i = 0x2E; i < 0x44; i++) {
        if (rec_flag(i + 15) == 0)
            any_clear = 1;
    }
    if (any_clear == 0)
        dword_53ECC = 2;
}

/* 0x20A51 */
void menu_need_flags_clear_16_17(void)
{
    ev5_205BE();
    if (rec_flag(0x10) != 0 || rec_flag(0x11) != 0)
        dword_53ECC = 1;
}

/* 0x20A87 */
void menu_need_flag_clear_1(void)
{
    ev5_205BE();
    if (rec_flag(1) != 0)
        dword_53ECC = 1;
}

/* 0x20B14 */
void menu_need_flag_clear_16(void)
{
    ev5_205BE();
    if (rec_flag(0x10) != 0)
        dword_53ECC = 1;
}

/* 0x20B3C */
void menu_need_flags_clear_1_2(void)
{
    ev5_205BE();
    if (rec_flag(1) != 0 || rec_flag(2) != 0)
        dword_53ECC = 1;
}

/* --- menu B: menu-screen actions --------------------------------------- */

/* 0x3314B */
void menu_show_reload(void)
{
    menu_reload_world();
    dword_51A83 = 0;
    menu_tail_draw0();
}

/* 0x33219 */
void menu_show_reload_pair(void)
{
    menu_reload_world();
    ev2_135DD(7, 32);
    ev4_1366A(31);
    menu_vm(0);
    ev2_135DD(7, 23);
    ev4_1366A(32);
    menu_tail_drawN_clear(1);
}

/* 0x3332B */
void menu_show_mark_two(void)
{
    menu_reload_world();
    ev2_135DD(10, 0);
    *(uint8_t *)(uintptr_t)(dword_53A45 + 4038) = 100;
    *(uint8_t *)(uintptr_t)(dword_53A45 + 4118) = 100;
    menu_tail_draw0();
}

/* 0x3346B (also 0x335A0 and 0x33674) */
void menu_show_plain(void)
{
    menu_reload_world();
    menu_tail_draw0();
}

/* 0x3347C */
void menu_show_step20(void)
{
    menu_reload_world();
    ev2_135DD(20, 20);
    menu_tail_draw0();
}

/* 0x335AA */
void menu_show_or_rebuild_sprites(void)
{
    menu_reload_world();
    if (unit_exists(0x12) == 0)
        unit_sprites_build(1);
    menu_tail_draw0();
}

/* 0x3367E */
void menu_show_step16_then_clear(void)
{
    menu_reload_world();
    ev2_135DD(16, 28);
    menu_tail_step_clear_draw0(0x43);
}

/* 0x33AAE */
void menu_show_step9_then_clear(void)
{
    menu_reload_world();
    ev2_135DD(9, 39);
    ev4_1366A(76);
    menu_tail_drawN_clear(0);
}

/* 0x33169 */
void menu_rebuild_sprites_and_show(void)
{
    menu_reload_world();
    menu_vm(0);
    dword_51A83 = 0;
    byte_53AFA = 1;
    unit_sprites_build(1);
    byte_53AFA = 0;
    ev2_135DD(8, 1);
    ev4_1366A(28);
    ev2_135DD(8, 0);
    ev4_1366A(29);
    menu_vm(1);
    dlg_portrait_glide(0);
}

/* 0x3327D */
void menu_mark_records_and_show(void)
{
    int i;
    menu_reload_world();
    for (i = 0; i < 11; i++)
        *(uint8_t *)(uintptr_t)(dword_53A45 + REC * (uint32_t)i + 3) = 2;
    ev2_135DD(6, 0);
    menu_vm(0);
    dword_51A83 = 0;
    ev4_1366A(35);
    menu_vm(1);
    dlg_portrait_glide(0);
    dlg_portrait_clear();
}

/* 0x33367 */
void menu_rebuild_sprites_and_clear(void)
{
    menu_reload_world();
    menu_vm(0);
    dword_51A83 = 0;
    ev2_135DD(10, 7);
    unit_sprites_build(1);
    ev4_1366A(38);
    menu_vm(1);
    menu_tail_step_draw2_clear(0x27);
}

/* 0x333F5 */
void menu_reset_and_show(void)
{
    menu_reload_world();
    ev2_135DD(4, 4);
    byte_53AFA = 1;
    unit_sprites_build(1);
    byte_53AFA = 0;
    ev4_1366A(40);
    ev2_135DD(11, 40);
    ev4_1366A(41);
    dlg_portrait_clear();
    menu_tail_draw0();
}

/* 0x334D9 */
void menu_show_gated_by_unit(void)
{
    int sub;

    menu_reload_world();
    sub = (unit_exists(0x0C) == 0) ? 3 : 0;
    menu_vm(sub);
    ev2_135DD(0x18, 0x11);
    menu_vm(sub + 1);
    dword_51A83 = 0;
    ev4_1366A(0x30);
    menu_vm(sub + 2);
    dlg_portrait_glide(0);
}

/* 0x335DA */
void menu_step_pair_then_clear(void)
{
    menu_reload_world();
    menu_vm(0);
    dword_51A83 = 0;
    ev2_135DD(0x10, 4);
    ev4_1366A(0x36);
    menu_vm(1);
    dword_51A83 = 0;
    ev2_135DD(0x10, 4);
    menu_tail_step_draw2_clear(0x37);
}

/* --- menu B tail (0x338C4 / 0x3396A / 0x1D4F6) ------------------------- */

typedef void *(*resload_fn)(const char *, void *, int);
typedef int   (*sfx_fn)(void *, int, int);
typedef void  (*h1v_fn)(int);
typedef void  (*free_fn)(void *);


/* 0x338C4 */
void menu_show_step_pairs(void)
{
    menu_reload_world();
    menu_vm(0);
    unit_sprites_build(1);
    ev2_135DD(0, 4);
    delay(400);
    ev2_135DD(0, 0x16);
    delay(400);
    ev2_135DD(0x1A, 0x18);
    delay(400);
    ev2_135DD(0x1A, 2);
    delay(400);
    menu_vm(1);
    dlg_portrait_glide(0);
}

/* 0x3396A */
void menu_show_map_pan(void)
{
    menu_reload_world();
    dword_53B13 = 0;
    dword_53B13 = res_load("FDOTHER.DAT", NULL, 88);
    ev2_135DD(5, 0);
    menu_vm(1);
    memset((void *)(uintptr_t)dword_53A49, 0, 0x25680u);
    svc_play_sfx((void *)(uintptr_t)dword_53B13, 1, 1);
    ev4_24B4D(20);
    delay(600);
    svc_play_sfx((void *)(uintptr_t)dword_53B13, 1, 1);
    ev4_24B4D(20);
    delay(600);
    svc_play_sfx((void *)(uintptr_t)dword_53B13, 1, 1);
    ev4_24B4D(20);
    delay(600);
    svc_play_sfx((void *)(uintptr_t)dword_53B13, 1, 1);
    ev4_24B4D(60);
    menu_vm(2);
    dlg_portrait_glide(0);
    menu_stop_and_free_music();
}

/* 0x1D4F6 */
void menu_stop_and_free_music(void)
{
    svc_play_sfx((void *)(uintptr_t)dword_53B13, -1, 1);
    free((void *)(uintptr_t)dword_53B13);
}
