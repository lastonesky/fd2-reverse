/* ev.c - FD2 scene-event handlers of funcs_1199C (source translation).
 *
 * See ev.h for the address table, the real ABI and the per-function return
 * values. This module is deliberately small: each function is the literal
 * machine-code behaviour, with the service calls going through their
 * **original addresses** (the scene.c / msg.c pattern):
 *
 *   - in the host, src/repl.c has already patched vm_run (0x15F84) and the
 *     record services to C, so the calls land on the translations;
 *   - src/evcheck.c hooks the same addresses to recording stubs, so this C
 *     can be compared against the original machine code call by call.
 *
 * The record table (dword_53A45), the one-shot flag block (dword_53AD5) and
 * the VM stream (dword_53A79) stay at their original data-segment addresses;
 * all three are read by dozens of not-yet-translated sites, so the C must
 * touch the real dwords (docs/TRANSLATION.md §3).
 */
#include "ev.h"

#include <stdint.h>
#include "fd2_game.h"   /* de-guest C4: restored cross-module decls */
#include "../include/fd2_globals.h"

/* --- service entry points (called at their original addresses) --------- */
typedef int      (*vm_fn)(void *stream, int sub, int addr, int pitch, int fg,
                          int shadow, int bgfill, int line_step, int wait);
typedef uint32_t (*status_fn)(int start, int end, int value);
typedef int      (*flag_fn)(int index);
typedef int      (*slot_free_fn)(int index);
typedef int      (*slot_claim_fn)(int index, int value);
typedef uint32_t (*mask_records_fn)(void);


/* --- original data-segment globals ------------------------------------- */

#define REC_STRIDE 80
#define VGA_BASE   fd2_vga

/* Every vm_run call in this cluster passes the same eight tail arguments
 * (addr VGA_P(0xA0000u), pitch 320, fg 205, shadow 76, fill 74, line step 19,
 * wait 1); only the sub-stream changes. */
static uint8_t *vm_sub(int sub)
{
    return vm_run((void *)(uintptr_t)dword_53A79, sub, VGA_BASE, 320,
                       205, 76, 74, 19, 1);
}

/* 0x34738 */
void ev_rec13_set1(void)
{
    *((uint8_t *)(uintptr_t)dword_53A45 + 13 * REC_STRIDE + 6) = 1;
    vm_sub(7);
}

/* 0x348EA */
void ev_status24_27(void)
{
    uint8_t flag = *((uint8_t *)(uintptr_t)dword_53AD5 + 16);

    if (flag == 0) {
        rec_status_set(24, 27, 7);
        vm_sub(3);
        *((uint8_t *)(uintptr_t)dword_53AD5 + 16) = 1;
    }
}

/* 0x34A6C - the scan runs to completion (no early break), then the second
 * sub-stream runs when at least one flag was clear. */
void ev_status7_36(void)
{
    int i, hit = 0;

    rec_status_set(7, 36, 7);
    vm_sub(8);

    for (i = 7; i <= 36; i++) {
        if (rec_flag(i) == 0)
            hit = 1;
    }
    if (hit)
        vm_sub(11);
}

/* 0x34B2F */
void ev_flag8_gate(void)
{
    if (rec_flag(8) == 0)
        vm_sub(2);
}

/* 0x34CF1 - 80*idx is the machine code's 32-bit `shl/add/shl` sequence. */
void ev_rec6_gate(int idx)
{
    uint32_t b = *((uint8_t *)(uintptr_t)dword_53A45
                   + (uint32_t)idx * REC_STRIDE + 6);

    if (b != 0) {
        rec_status_set(9, 27, 0);
        *((uint8_t *)(uintptr_t)dword_53AD5 + 16) = 1;
    }
}

/* 0x34D92 */
void ev_mask_records(void)
{
    vm_sub(2);
    rec_status_mask_records();
}

/* 0x34F74 */
void ev_clear_status12_13(void)
{
    vm_sub(2);
    *((uint8_t *)(uintptr_t)dword_53A45 + 12 * REC_STRIDE + 52) = 0;
    *((uint8_t *)(uintptr_t)dword_53A45 + 13 * REC_STRIDE + 52) = 0;
}

/* 0x35123 - the machine code passes the incoming argument (which the first
 * test has already proved to be 0) to both slot services. */
void ev_slot8_claim(int idx)
{
    if (idx != 0)
        return;
    if (rec_slot_free(idx) == 8)
        return;
    if (*((uint8_t *)(uintptr_t)dword_53AD5 + 16) != 0)
        return;

    rec_slot_claim(idx, 89);
    vm_sub(11);
    *((uint8_t *)(uintptr_t)dword_53AD5 + 16) = 1;
}

/* 0x35191 */
void ev_status16_71(void)
{
    uint8_t flag = *((uint8_t *)(uintptr_t)dword_53AD5 + 16);

    if (flag == 0) {
        rec_status_set(16, 71, 0);
        vm_sub(1);
        *((uint8_t *)(uintptr_t)dword_53AD5 + 16) = 1;
    }
}

/* 0x351E6 - the window 64..73 is a constant, independent of the record
 * count; the fields cleared are byte +53 (not +52). */
void ev_clear64_73(void)
{
    int i;

    vm_sub(6);
    for (i = 64; i <= 73; i++)
        *((uint8_t *)(uintptr_t)dword_53A45 + (uint32_t)i * REC_STRIDE + 53) = 0;

    rec_status_set(64, 73, 3);
    rec_status_set(35, 49, 0);
}

/* 0x35258 */
void ev_status16_34(void)
{
    vm_sub(8);
    rec_status_set(16, 34, 0);
}
