/* ev2.c - FD2 funcs_1199C scene-script handlers, second batch (see ev2.h).
 *
 * One function per table entry, written from the original disassembly with the
 * literal semantics: no reinterpretation, just the pushes turned into
 * arguments. The services are called at their original addresses; the shared
 * map-scroll stepper 0x135DD is the only helper translated in this batch, and
 * the four shared tails (0x34F4C/0x34F65 = vm_run(1)/vm_run(<sub>), 0x3528D/
 * 0x3528F = status-set, 0x3530D = vm_run, 0x3566E = 0x35B78) are inlined.
 */
#include "ev2.h"

#include <stdint.h>
#include "fd2_game.h"   /* de-guest C4: restored cross-module decls */
#include "../include/fd2_globals.h"

/* --- service entry points (called at their original addresses) --------- */
typedef int      (*vm_fn)(void *stream, int sub, int addr, int pitch, int fg,
                          int shadow, int bgfill, int line_step, int wait);
typedef uint32_t (*status_fn)(int start, int end, int value);
typedef int      (*unit_add_fn)(int id);
typedef int      (*load_fn)(int index);      /* 0x10B4E (file + heap)        */
typedef uint32_t (*wait_fn)(int ticks);      /* 0x17AA9 (BIOS tick wait)     */
typedef void     (*seq_fn)(int a, int b, int c); /* 0x35B78 (fade sequence)  */
typedef void     (*clear_fn)(int from);      /* 0x35F10 (clear records)      */
typedef void     (*view_fn)(int mode);       /* 0x11CAC (map view update)    */
typedef void     (*flush_fn)(void);          /* 0x4E381 (keyboard flush)     */


/* --- original data-segment globals ------------------------------------- */

#define REC_STRIDE 80
#define VGA_BASE   fd2_vga

/* Every vm_run call in this batch passes the same eight tail arguments. */
static void ev2_vm_run(int sub)
{
    vm_run((void *)(uintptr_t)dword_53A79, sub, VGA_BASE, 320,
                205, 76, 74, 19, 1);
}

/* 0x135DD - step the view window to (x, y); each step refreshes the view and
 * flushes the keyboard. The low/high halves of qword_53AB1 count the steps. */
void ev2_135DD(int x, int y)
{
    dword_51A83 = 0;
    while (x != (int)dword_53AA9) {
        if (x >= (int)dword_53AA9) { dword_53AB1 += 1; dword_53AA9 += 1; }
        else                  { dword_53AB1 -= 1; dword_53AA9 -= 1; }
        map_view_update(0);
        kbd_flush();
    }
    while (y != (int)dword_53AAD) {
        if (y >= (int)dword_53AAD) { dword_53AB5 += 1; dword_53AAD += 1; }
        else                  { dword_53AB5 -= 1; dword_53AAD -= 1; }
        map_view_update(0);
        kbd_flush();
    }
}

/* 0x35298 */
void ev2_35298(int arg)
{
    (void)arg;
    unit_sprites_build(1);
    ev2_vm_run(0x0A);
}

/* 0x35321 */
void ev2_35321(int arg)
{
    (void)arg;
    unit_sprites_build(2);
    ev2_135DD(0x11, 0x25);
    ev2_vm_run(1);
}

/* 0x353B5 */
void ev2_353B5(int arg)
{
    (void)arg;
    unit_sprites_build(1);
    ev2_vm_run(6);
}

/* 0x353E7 */
void ev2_353E7(int arg)
{
    (void)arg;
    rec_status_set(0x10, 0x10, 3);
}

/* 0x353FA */
void ev2_353FA(int arg)
{
    (void)arg;
    rec_status_set(0x1D, 0x3B, 3);
}

/* 0x3540F */
void ev2_3540F(int arg)
{
    (void)arg;
    rec_status_set(0x10, 0x1F, 3);
}

/* 0x35422 */
void ev2_35422(int arg)
{
    (void)arg;
    unit_sprites_build(1);
    ev2_vm_run(1);
    unit_add(27);
}

/* 0x3551C */
void ev2_3551C(int arg)
{
    (void)arg;
    rec_status_set(0x23, 0x2A, 3);
    rec_status_set(0x43, 0x4A, 3);
}

/* 0x3553F */
void ev2_3553F(int arg)
{
    (void)arg;
    unit_sprites_build(dword_53BEF / 2);
    ev2_135DD(0x20, 0x23);
    svc_wait_ticks(8);
    ev2_135DD(0, 0x23);
    svc_wait_ticks(8);
    if (dword_53BEF == 3)
        ev2_vm_run(1);
}

/* 0x355B7 */
void ev2_355B7(int arg)
{
    (void)arg;
    unit_sprites_build(2);
    ev2_135DD(0x10, 0x2A);
    svc_wait_ticks(8);
    unit_add(20);
    ev2_vm_run(2);
}

/* 0x35638 - two fades whose record index is a byte of the chapter counter. */
void ev2_35638(int arg)
{
    uint8_t t;

    (void)arg;
    t = (uint8_t)((uint8_t)dword_53BEF - 0x0E);
    t = (uint8_t)(t + t);
    ev3_35B78(2, 0x0B, t);
    ev3_35B78(0x1A, 0x0B, (uint8_t)(t + 1));
}

/* 0x35677 */
void ev2_35677(int arg)
{
    (void)arg;
    ev2_vm_run(5);
    ev3_35F10(0x12);
}

/* 0x35997 - status(39,44,0) when record `arg` has byte +6 set. */
void ev2_35997(int arg)
{
    if (*(uint8_t *)(uintptr_t)(dword_53A45 + REC_STRIDE * arg + 6) != 0)
        rec_status_set(0x27, 0x2C, 0);
}

/* 0x359CB - two status writes when record `arg` has byte +6 set. */
void ev2_359CB(int arg)
{
    if (*(uint8_t *)(uintptr_t)(dword_53A45 + REC_STRIDE * arg + 6) != 0) {
        rec_status_set(0x17, 0x18, 0);
        rec_status_set(0x35, 0x38, 0);
    }
}

/* 0x35BEE */
void ev2_35BEE(int arg)
{
    (void)arg;
    if (*(uint8_t *)(uintptr_t)(dword_53AD5 + 0x11) == 0) {
        *(uint8_t *)(uintptr_t)(dword_53A55 + 3) = (uint8_t)(dword_53BEF + 1);
        *(uint8_t *)(uintptr_t)(dword_53AD5 + 0x11) = 1;
    }
}

/* 0x35C1D */
void ev2_35C1D(int arg)
{
    (void)arg;
    ev3_35B78(3, 0x1B, 1);
    ev3_35B78(0x0F, 0x1B, 2);
}

/* 0x35D85 */
void ev2_35D85(int arg)
{
    (void)arg;
    *(uint8_t *)(uintptr_t)(dword_53A55 + 6) = (uint8_t)dword_53BEF;
}

/* 0x35F79 */
void ev2_35F79(int arg)
{
    (void)arg;
    *(uint8_t *)(uintptr_t)(dword_53AD5 + 0x12) = 1;
}

/* 0x36214 */
void ev2_36214(int arg)
{
    (void)arg;
    *(uint8_t *)(uintptr_t)(dword_53AD5 + 0x13) = 1;
}

/* 0x36228 */
void ev2_36228(int arg)
{
    (void)arg;
    *(uint8_t *)(uintptr_t)(dword_53AD5 + 0x14) = 1;
}

/* 0x362B0 */
void ev2_362B0(int arg)
{
    (void)arg;
    rec_status_set(0x14, 0x14, 0x0B);
}

/* 0x362C5 */
void ev2_362C5(int arg)
{
    (void)arg;
    ++*(uint8_t *)(uintptr_t)(dword_53AD5 + 0x10);
    *(uint8_t *)(uintptr_t)(dword_53A55 + 3) = (uint8_t)(dword_53BEF + 1);
}

/* 0x363DE */
void ev2_363DE(int arg)
{
    (void)arg;
    ev2_vm_run(8);
    ev3_35F10(0x14);
}

/* 0x36416 */
void ev2_36416(int arg)
{
    (void)arg;
    rec_status_set(0x10, dword_53BEB - 1, 0);
}

/* 0x3642E / 0x36439 / 0x36440 / 0x36447 / 0x3644E - the dispatcher's no-op
 * entries (0x3642E is `push 4; call chkstk; ret`, the four others jump into
 * its `call` and do the same). */
void ev2_3642E(int arg) { (void)arg; }
void ev2_36439(int arg) { (void)arg; }
void ev2_36440(int arg) { (void)arg; }
void ev2_36447(int arg) { (void)arg; }
void ev2_3644E(int arg) { (void)arg; }
