/* ev3.c - FD2 funcs_1199C scene-script cluster, remaining 23 entries + 5
 * helpers (see ev3.h). Written from the original disassembly.
 *
 * Two traps drove the disassembly-first approach here:
 *   - Hex-Rays mis-merges the `push <frame>; call 0x3702F` stack probe into
 *     the argument list, and for the larger entries (0x35854/0x33F78) it then
 *     mislabels a *global* (dword_53AD9) as a vm_run's stream argument. The
 *     disassembly is the source of truth.
 *   - cdecl order: the *last* push is the *first* argument. Every call here
 *     was decoded from the pushes, not from Hex-Rays.
 */
#include "ev3.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include "fd2_game.h"   /* de-guest C4: restored cross-module decls */
#include "../include/fd2_globals.h"

/* --- service entry points (called at their original addresses) --------- */
typedef int      (*vm_fn)(void *stream, int sub, int addr, int pitch, int fg,
                          int shadow, int bgfill, int line_step, int wait);
typedef void     (*scroll_fn)(int x, int y);        /* 0x135DD              */
typedef int      (*scene_fn)(int rec, int count, const void *script);
typedef void     (*load_fn)(int index);             /* 0x10B4E              */
typedef int      (*wait_fn)(int ticks);             /* 0x17AA9              */
typedef void     (*seq_fn)(int a, int b, int c);    /* 0x35B78              */
typedef void     (*clear_fn)(int from);             /* 0x35F10              */
typedef void     (*ramp_fn)(void);                  /* 0x361B0              */
typedef int      (*find_fn)(int rec, int value);    /* 0x2AEDB              */
typedef void     (*delay_fn)(int ms);               /* 0x3790A              */
typedef void     (*pal_add_fn)(int s, int e, int a);/* 0x11DF2              */
typedef void     (*view_fn)(int mode);              /* 0x11CAC              */
typedef void     (*flush_fn)(void);                 /* 0x4E381              */
typedef void     (*msg_open_fn)(int id);            /* 0x1956B              */
typedef void     (*msg_close_fn)(void);             /* 0x196CB              */
typedef void     (*blit_fn)(int n);                 /* 0x16559              */
typedef void     (*waitkey_fn)(int speaker);        /* 0x16C57              */
typedef int      (*slot_free_fn)(int index);        /* 0x1B8A6              */
typedef int      (*slot_claim_fn)(int index, int v);/* 0x1BB8C              */
typedef void     (*slot_remove_fn)(int index, int s);/* 0x1B8E7             */
typedef int      (*rec_field_fn)(int index, int slot);/* 0x1B722            */
typedef void     (*map_cell_fn)(int x, int y, uint8_t *out); /* 0x12E38     */
typedef void    *(*res_load_fn)(const char *f, void *old, int idx); /*0x111BA*/
typedef void     (*res_blit_fn)(void *buf, int idx, void *dst, int pitch, int mode);
typedef void     (*free_fn)(void *p);               /* 0x3776E              */
typedef void     (*notify_fn)(void);                /* 0x1DB65 / 0x12263    */
typedef void     (*two_fn)(int a, int b);           /* 0x2E2B0              */
typedef void     (*one_fn)(int a);                  /* 0x1366A              */
typedef void     (*pair3_fn)(int a, int b, int c);  /* 0x33F78              */
typedef void     (*pair2_fn)(int a, int b);         /* 0x12CEA              */
typedef void     (*five_fn)(int a, int b, int c, int d, int e); /* 0x22253   */
typedef uint32_t (*status_fn)(int start, int end, int value);
typedef int      (*unit_add_fn)(int id);
typedef int      (*flag_fn)(int index);
typedef int      (*flag_or80_fn)(int index);
typedef uint32_t (*rand_fn)(void);


/* --- original data-segment globals ------------------------------------- */

#define REC_STRIDE 80
#define VGA_BASE   fd2_vga

static void vm(void *stream, int sub, uint8_t *addr)
{
    vm_run(stream, sub, addr, 320, 205, 76, 74, 19, 1);
}
static void vm_sub(void *stream, int sub) { vm(stream, sub, VGA_BASE); }

/* ---------------------------------------------------------------- 0x352CA */
void ev3_352CA(int arg)
{
    static const uint8_t scr[3] = { 0x00, 0xD3, 0x00 };
    sub_1AA1D(arg, 1, scr);
    vm_sub(dword_53A79, 0x0B);
}

/* ---------------------------------------------------------------- 0x35346 */
void ev3_35346(int arg)
{
    static const uint8_t scr[3] = { 0x00, 0xD5, 0x00 };
    vm_sub(dword_53A79, 3);
    sub_1AA1D(arg, 1, scr);
    vm_sub(dword_53A79, 4);
}

/* ---------------------------------------------------------------- 0x35468 */
void ev3_35468(int arg)
{
    (void)arg;
    unit_sprites_build((int)dword_53BEF / 2);
    svc_wait_ticks(1);
    ev2_135DD(0, 0);
    svc_wait_ticks(8);
    ev2_135DD(0x1C, 0);
    svc_wait_ticks(8);
    ev2_135DD(0x1C, 0x20);
    svc_wait_ticks(8);
    ev2_135DD(0, 0x20);
    svc_wait_ticks(8);
    if (dword_53BEF == 2)
        vm_sub(dword_53A79, 3);
}

/* ---------------------------------------------------------------- 0x355F0 */
void ev3_355F0(int arg)
{
    static const uint8_t scr[3] = { 0x00, 0x65, 0x00 };
    sub_1AA1D(arg, 1, scr);
    vm_sub(dword_53A79, 3);
}

/* ---------------------------------------------------------------- 0x356B3 */
void ev3_356B3(int arg)
{
    (void)arg;
    unit_sprites_build((int)dword_53BEF);
    ev2_135DD(0, 4);
    delay(400);
    ev2_135DD(0, 0x16);
    delay(400);
    ev2_135DD(0x1A, 0x18);
    delay(400);
    ev2_135DD(0x1A, 2);
    delay(400);
}

/* ---------------------------------------------------------------- 0x35730 */
void ev3_35730(int arg)
{
    static const uint8_t scr[3] = { 0x00, 0x0B, 0x00 };

    if (arg == 0 && *(uint8_t *)(uintptr_t)dword_53AD5 == 0) {
        vm_sub(dword_53A79, 0);
        sub_2E2B0(0x11, arg);
        sub_1DB65();
        if (rec_flag(0x11) != 0) {
            *(uint8_t *)(uintptr_t)dword_53AD5 = 1;
            ev4_12263();
            map_view_update(1);
            sub_1AA1D(arg, 1, scr);
        }
    }
    dword_53EC8 = 0;
}

/* ---------------------------------------------------------------- 0x357DD */
void ev3_357DD(int arg)
{
    (void)arg;
    ev2_135DD(6, 0x28);
    unit_sprites_build(1);
    ev4_1366A(0x4A);
    vm_sub(dword_53A79, 5);
    dlg_portrait_clear();
}

/* ---------------------------------------------------------------- 0x35833 */
void ev3_35833(int arg)
{
    (void)arg;
    unit_sprites_build((int)dword_53BEF);
    ev2_135DD(9, 0);
    delay(400);
}

/* ---------------------------------------------------------------- 0x35854 */
void ev3_35854(int arg)
{
    static const uint8_t table5[5] = { 0x1D, 0x2B, 0x33, 0x3D, 0x47 };
    uint8_t out[8];   /* map_cell_info writes 5 bytes (the +4 kind flag) */
    int     i;

    kbd_flush();
    msg_open_portrait(*(uint8_t *)(uintptr_t)(dword_53A45 + REC_STRIDE * arg + 7));

    if (rec_slot_free(arg) == 8) {
        vm_sub(dword_53A7D, 0x1E0);
        dlg_blit_dato(dword_53A85, dword_53C67, 0);
        dlg_wait_key(0);
        ev4_196CB();
        return;
    }

    map_cell_info((int)dword_53AB1, (int)dword_53AB5, out);
    dword_53AD9 = (uint32_t)table5[out[2]] + 0xB5;
    vm(dword_53A7D, 0x1A6, VGA_P(0xA9F23u));
    dlg_blit_dato(dword_53A85, dword_53C67, 0);
    dlg_wait_key(0);
    rec_slot_claim(arg, table5[out[2]]);
    ev4_196CB();
    for (i = 0; i < 5; i++)
        *(uint8_t *)(uintptr_t)(dword_53AD5 + i) = 1;
    ev4_12263();
}

/* ---------------------------------------------------------------- 0x35A0D */
void ev3_35A0D(int arg)
{
    void  *buf;
    int    e, i;

    if (*(uint8_t *)(uintptr_t)(dword_53AD5 + 0x0C) != 0)
        return;

    msg_open_portrait(*(uint8_t *)(uintptr_t)(dword_53A45 + REC_STRIDE * arg + 7));
    e = ev3_2AEDB(arg, 0xD0);
    if (e == -1) {
        vm(dword_53A79, 2, VGA_P(0xA951Fu));
        dlg_blit_dato(dword_53A85, dword_53C67, 0);
        dlg_wait_key(0);
        ev4_196CB();
        return;
    }
    rec_slot_remove(arg, e);
    vm(dword_53A79, 3, VGA_P(0xA951Fu));
    dlg_wait_key(0);
    ev4_196CB();

    buf = res_load("FDOTHER.DAT", NULL, 0x2D);
    for (i = 0; i < 0x3B; i++) {
        res_blit(buf, i, (void *)(uintptr_t)VGA_P(0xABCE4u), 320, -1);
        svc_wait_ticks(2);
    }
    free(buf);

    *(uint8_t *)(uintptr_t)(dword_53AD5 + 0x0C) = 1;
    ev4_12263();
    unit_sprites_build(1);
    unit_add(0x1F);
    vm_sub(dword_53A79, 4);
}

/* ---------------------------------------------------------------- 0x35C40 */
void ev3_35C40(int arg)
{
    uint8_t v;

    (void)arg;
    v = *(uint8_t *)(uintptr_t)(dword_53AD5 + 0x10);
    if (v == 1) {
        vm_sub(dword_53A79, 1);
        ev3_35B78(9, 0x2C, 3);
        ev3_35B78(0, 9, 4);
        ev3_35B78(0x11, 9, 5);
        dword_51A83 = 1;
    } else if (v == 2) {
        vm_sub(dword_53A79, 2);
        ev3_35F10(0x10);
    }
    ++*(uint8_t *)(uintptr_t)(dword_53AD5 + 0x10);
}

/* ---------------------------------------------------------------- 0x35CF1 */
void ev3_35CF1(int arg)
{
    (void)arg;
    if (*(uint8_t *)(uintptr_t)(dword_53AD5 + 0x10) == 0) {
        *(uint8_t *)(uintptr_t)(dword_53A55 + 3) = (uint8_t)dword_53BEF;
        *(uint8_t *)(uintptr_t)(dword_53AD5 + 0x10) = 1;
    }
}

/* ---------------------------------------------------------------- 0x35D1E */
void ev3_35D1E(int arg)
{
    (void)arg;
    vm_sub(dword_53A79, 3);
    ev3_35B78(0x11, 0x12, 1);
    vm_sub(dword_53A79, 6);
}

/* ---------------------------------------------------------------- 0x35D9E */
void ev3_35D9E(int arg)
{
    (void)arg;
    vm_sub(dword_53A79, 4);
    ev3_35B78(0x0E, 7, 2);
    vm_sub(dword_53A79, 6);
    *(uint8_t *)(uintptr_t)(dword_53AD5 + 0x12) = 1;
}

/* ---------------------------------------------------------------- 0x35E0E */
void ev3_35E0E(int arg)
{
    if (*(uint8_t *)(uintptr_t)(dword_53A45 + REC_STRIDE * arg + 6) != 0
        && *(uint8_t *)(uintptr_t)(dword_53AD5 + 0x11) == 0
        && *(uint8_t *)(uintptr_t)(dword_53AD5 + 0x12) != 0) {
        *(uint8_t *)(uintptr_t)(dword_53A55 + 9) = (uint8_t)dword_53BEF;
        *(uint8_t *)(uintptr_t)(dword_53AD5 + 0x11) = 1;
    }
}

/* ---------------------------------------------------------------- 0x35E5B */
void ev3_35E5B(int arg)
{
    (void)arg;
    rec_status_set(0x29, 0x2D, 0);
    vm_sub(dword_53A79, 5);
    ev3_35B78(8, 7, 3);
    ev3_35B78(4, 7, 4);
    ev3_35B78(0, 7, 5);
    /* 0x35E5B's tail jumps into 0x35D1E at 0x35D55, which after the
     * 0x35B78 call is not a plain `retn` - it continues with vm_run(6). */
    vm_sub(dword_53A79, 6);
}

/* ---------------------------------------------------------------- 0x35EC1 */
void ev3_35EC1(int arg)
{
    (void)arg;
    if (*(uint8_t *)(uintptr_t)(dword_53AD5 + 0x13) != 0) {
        vm_sub(dword_53A79, 2);
        ev3_35F10(0x14);
    }
    ++*(uint8_t *)(uintptr_t)(dword_53AD5 + 0x13);
}

/* ---------------------------------------------------------------- 0x35F48 */
void ev3_35F48(int arg)
{
    (void)arg;
    ev3_35B78(4, 0x23, 2);
    ev3_35B78(0x0E, 0x23, 3);
    dword_51A83 = 1;
}

/* ---------------------------------------------------------------- 0x35F88 */
void ev3_35F88(int arg)
{
    uint8_t v;

    (void)arg;
    v = *(uint8_t *)(uintptr_t)(dword_53AD5 + 0x10);
    ev3_35B78(0x0A, 0x1D, v);
    if (v != 7)
        *(uint8_t *)(uintptr_t)(dword_53A55 + 3) = (uint8_t)(dword_53BEF + 1);
    ++*(uint8_t *)(uintptr_t)(dword_53AD5 + 0x10);
}

/* ---------------------------------------------------------------- 0x35FCF */
void ev3_35FCF(int arg)
{
    uint8_t *rec = dword_53A45 + REC_STRIDE * (uint32_t)arg;

    if (*(uint8_t *)(uintptr_t)(rec + 6) == 0)
        return;
    if (*(uint8_t *)(uintptr_t)(dword_53AD5 + 0x11) != 0)
        return;

    if (*(uint8_t *)(uintptr_t)(rec + 8) != 9) {
        msg_open_portrait(*(uint8_t *)(uintptr_t)(rec + 7));
        vm(dword_53A79, 0, VGA_P(0xA951Fu));
        dlg_blit_dato(dword_53A85, dword_53C67, 0);
        dlg_wait_key(0);
        ev4_196CB();
    } else {
        vm_sub(dword_53A79, 1);
        *(uint8_t *)(uintptr_t)(dword_53AD5 + 0x11) = 1;
        *(uint8_t *)(uintptr_t)(dword_53A55 + 6) = (uint8_t)(dword_53BEF + 1);
        *(uint8_t *)(uintptr_t)(dword_53AD5 + 0x10) = 4;
        *(uint8_t *)(uintptr_t)(dword_53A55 + 3) = (uint8_t)dword_53BEF;
    }
}

/* ---------------------------------------------------------------- 0x360B6 */
void ev3_360B6(int arg)
{
    int i;

    (void)arg;
    if (*(uint8_t *)(uintptr_t)(dword_53AD5 + 0x11) != 4) {
        rec_flag_or80(1);
        ++*(uint8_t *)(uintptr_t)(dword_53AD5 + 0x11);
        *(uint8_t *)(uintptr_t)(dword_53A55 + 6) = (uint8_t)(dword_53BEF + 1);
        return;
    }
    vm_sub(dword_53A79, 2);
    unit_sprites_build(1);
    *(uint8_t *)(uintptr_t)(dword_53AD5 + 0x15) = (uint8_t)(dword_53BEB - 3);
    *(uint8_t *)(uintptr_t)(dword_53A55 + 9) = (uint8_t)dword_53BEF;
    ev3_361B0();
    delay(400);
    ev3_361B0();
    delay(400);
    for (i = 3; i <= 6; i++) {
        ev3_361B0();
        vm_sub(dword_53A79, i);
    }
}

/* ---------------------------------------------------------------- 0x3623C */
void ev3_3623C(int arg)
{
    int r, base;

    (void)arg;
    *(uint8_t *)(uintptr_t)(dword_53A55 + 9) = (uint8_t)(dword_53BEF + 1);
    r = (int)util_rand();
    base = *(uint8_t *)(uintptr_t)(dword_53AD5 + 0x15);
    rec_flag_or80(base + r % 3);
    rec_flag_or80(base + (r + 1) % 3);
}

/* ---------------------------------------------------------------- 0x362E8 */
void ev3_362E8(int arg)
{
    uint8_t v;

    (void)arg;
    ev2_135DD(0x10, 1);
    v = *(uint8_t *)(uintptr_t)(dword_53AD5 + 0x10);
    vm_sub(dword_53A79, (int)v + 2);
    dword_51A83 = 0;
    ev2_135DD(0x10, 0x0E);
    ev3_33F78(0x18 - (int)v, 0x16, 0x12);
    v = *(uint8_t *)(uintptr_t)(dword_53AD5 + 0x10);
    if (v != 4) {
        unit_sprites_build((int)v);
        rec_status_set(0x18 - (int)v, 0x18 - (int)v, 0);
        ev3_33F78(2 * (int)v + 0x19, 0x15, 0x12);
        ev3_33F78(2 * (int)v + 0x1A, 0x17, 0x12);
    } else {
        rec_status_set(0x14, 0x14, 0x0B);
    }
    dword_51A83 = 1;
}

/* ======================= helpers ======================================= */

/* 0x35B78 */
void ev3_35B78(int a, int b, int c)
{
    ev2_135DD(a, b);
    unit_sprites_build(c);
    delay(300);
    pal_fade_add(0, 0xFF, 0xFF);
    delay(200);
    pal_fade_add(0, 0xFF, 0);
    map_view_update(0);
    delay(400);
}

/* 0x35F10 */
void ev3_35F10(int from)
{
    int i;
    for (i = from; i < (int)dword_53BEB; i++)
        *(uint16_t *)(uintptr_t)(dword_53A45 + REC_STRIDE * (uint32_t)i + 0x40) = 0;
    sub_1DB65();
}

/* 0x361B0 */
void ev3_361B0(void)
{
    int i;
    for (i = 0; i < 0x40; i++) {
        pal_fade_add(0, 0xFF, i);
        delay(8);
    }
    delay(400);
    for (i = 0x3E; i >= 0; i--) {
        pal_fade_add(0, 0xFF, i);
        delay(8);
    }
}

/* 0x2AEDB */
int ev3_2AEDB(int rec, int value)
{
    int n = rec_slot_free(rec);
    int i;
    if (n == 0)
        return -1;
    for (i = 0; i < n; i++)
        if (rec_field_byte(rec, i) == value)
            return i;
    return -1;
}

/* 0x33F78 */
void ev3_33F78(int a, int b, int c)
{
    map_slide_view(b, c);
    sub_22253(a, b, c, b, c);
}
