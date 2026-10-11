/* scene.c - scene/state routines of the main state machine family.
 *
 * First one: 0x22E5C, reached from main (0x25BF4) when dword_53ECC == 1. It is
 * a pure sequence of services, which is why it is a good first piece of the
 * family (the state handlers in funcs_25E23[]/funcs_25E3A[] are much bigger).
 *
 * The services are called through their **original addresses** on purpose:
 *   - in the host, 0x25977 (bgm_play), 0x17AA9 (svc_wait_ticks) and 0x111BA
 *     (res_load) are already patched to C by src/repl.c, and the rest run as
 *     machine code - exactly what the original did;
 *   - src/scenecheck.c hooks them to record the call sequence, so the same C
 *     can be compared against the original 0x22E5C call by call.
 *
 * The buffer is freed with guest_free (0x3776E), the same heap res_load used -
 * the original's tail does `push ebx; jmp free`.
 */
#include "scene.h"
#include "guest_mem.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "fd2_game.h"   /* de-guest C4: restored cross-module decls */
#include "../include/fd2_globals.h"

#define VGA_BASE fd2_vga

/* --- service entry points ---------------------------------------------- */
typedef void  (*bgm_fn)(int track, int loop_count);
typedef int   (*wait_fn)(int ticks);
typedef void  (*fade_fn)(void);
typedef void *(*res_load_fn)(const char *name, void *old_buffer, int index);
typedef int   (*rle_blit_fn)(void *buf, int index, void *dst, int pitch,
                             int mode);


/* The three services the funcs_25E23[] state handlers use, called through
 * their original addresses for the same reason as above (host: patched to C;
 * scenecheck: hooked to recording stubs). */
typedef int  (*vm_fn)(void *stream, int sub, int addr, int pitch,
                      int fg, int shadow, int bgfill, int line_step, int wait);
typedef void (*unit_refresh_fn)(void);
typedef int  (*unit_add_fn)(int id);
typedef int  (*unit_exists_fn)(int id);


/* Game globals (obj1 data segment) used by the handlers:
 *   dword_53A79  the current VM stream pointer (vm_run's first argument)
 *   dword_53C03  the main-loop state index that selects from funcs_25E23[]
 *                / funcs_25E3A[]; each handler ends by advancing it. */

void scene_card(void)
{
    void *buf;

    bgm_play(-1, 1);                       /* fade the music out          */
    svc_wait_ticks(1);
    pal_fade_out();                            /* darken the palette          */
    buf = res_load("FDOTHER.DAT", NULL, 79);
    memset((void *)(uintptr_t)VGA_BASE, 0, 320 * 200);
    res_blit(buf, 0, (void *)(uintptr_t)VGA_BASE, 320, -1);
    pal_fade_in();
    svc_wait_ticks(9);
    res_blit(buf, 1, (void *)(uintptr_t)VGA_BASE, 320, -1);
    svc_wait_ticks(36);
    guest_free(buf);
}

/* --- funcs_25E23[] state handlers -------------------------------------
 *
 * `main` (0x25BF4) steps the story through one state index:
 *
 *     mov  eax, dword_53C03
 *     call funcs_25E23[eax*4]   ; transition handler (this cluster)
 *     call sub_26152
 *     mov  eax, dword_53C03
 *     call funcs_25E3A[eax*4]   ; renderer for the new state
 *
 * Each handler below is one table entry. They are plain `void fn(void)`: the
 * `push 28h; call 0x3702F` prologue is the Watcom stack probe (_chkstk), not
 * an argument - the same artifact as in vm.c/unit.c (rounds/08 §37.3).
 *
 * All six draw the same VGA_P(0xA0000u) VGA cell with the same 9 vm_run arguments
 * (pitch 320, fg 205, shadow 76, fill 74, line step 19, wait 1); only the
 * sub-stream differs (the one exception, 0x239BD, computes its sub-stream from
 * unit_exists). dword_53C03 is the observable side effect: 0x22EF6
 * *assigns* 1, the other five increment it. The originals share tail code
 * (0x23790/0x2389F/0x23E39 all tail-jump to 0x231F2); the C spells the
 * behaviour out per handler instead of reproducing the jump layout. */

/* The shared vm_run(stream=dword_53A79, sub,VGA_P(...)) with the fixed 8 tail args. */
static uint8_t *vm9(void)
{
    return vm_run((void *)(uintptr_t)dword_53A79, 9,VGA_P(0xA0000u), 320, 205, 76, 74, 19, 1);
}
static uint8_t *vm4(void)
{
    return vm_run((void *)(uintptr_t)dword_53A79, 4,VGA_P(0xA0000u), 320, 205, 76, 74, 19, 1);
}
static uint8_t *vm3(void)
{
    return vm_run((void *)(uintptr_t)dword_53A79, 3,VGA_P(0xA0000u), 320, 205, 76, 74, 19, 1);
}

/* 0x22EF6 - funcs_25E23[0]: vm_run(...,9,VGA_P(...)), refresh, then *set* state 1. */
void scene_state_00(void)
{
    vm9();
    unit_refresh_all();
    dword_53C03 = 1;
}

/* 0x231BC - funcs_25E23[3]: vm_run(...,4,VGA_P(...)), refresh, advance state. */
void scene_state_03(void)
{
    vm4();
    unit_refresh_all();
    dword_53C03++;
}

/* 0x23790 - funcs_25E23[10]: vm_run(...,3,VGA_P(...)), refresh, join unit 14. */
void scene_state_10(void)
{
    vm3();
    unit_refresh_all();
    unit_add(14);
    dword_53C03++;
}

/* 0x2389F - funcs_25E23[12]: vm_run(...,9,VGA_P(...)), refresh, join unit 3. */
void scene_state_12(void)
{
    vm9();
    unit_refresh_all();
    unit_add(3);
    dword_53C03++;
}

/* 0x239BD - funcs_25E23[14]: draw sub-stream 12 when a party record for
 * identity 12 already exists, else 13, refresh, join unit 15, advance.
 * The sub-stream is the low byte of `(unit_exists(12) ^ 1) + 12`; the original
 * only uses al (`xor al,1; add al,0Ch; movzx eax,al`), so the C truncates with
 * a uint8_t cast - which keeps the two equal for any return value, not just
 * 0/1. The tail (unit_add; inc dword_53C03) is shared with 0x23790 in the
 * machine code; the C writes the behaviour out directly (rounds/30). */
void scene_state_14(void)
{
    int exists = unit_exists(12);
    int sub    = (int)(uint8_t)((exists ^ 1) + 12);

    vm_run((void *)(uintptr_t)dword_53A79, sub,VGA_P(0xA0000u), 320, 205, 76, 74, 19, 1);
    unit_refresh_all();
    unit_add(15);
    dword_53C03++;
}

/* 0x23E39 - funcs_25E23[18]: refresh *before* vm_run(...,3,VGA_P(...)), advance. */
void scene_state_18(void)
{
    unit_refresh_all();
    vm3();
    dword_53C03++;
}

/* 0x233C6 - place records from coordinate arrays, set the view origin and
 * fade back in. */
typedef void (*h0v_fn)(void);

void scene_place_records(const uint8_t *xs, const uint8_t *ys, uintptr_t kind,
                         int first, int last, int single,
                         int sx, int sy, int skind, uint64_t origin)
{
    int i;

    pal_fade_out();
    ev5_13536();
    for (i = first; i <= last; i++) {
        uint8_t *rec = dword_53A45 + 80u * (uint32_t)i;
        rec[0] = xs[i];
        rec[1] = ys[i];
        rec[3] = (kind < 4) ? (uint8_t)kind
                            : ((const uint8_t *)(uintptr_t)kind)[i];
    }
    if (single != 0) {
        uint8_t *rec = dword_53A45 + 80u * (uint32_t)single;
        rec[0] = (uint8_t)sx;
        rec[1] = (uint8_t)sy;
        rec[3] = (uint8_t)skind;
    }
    dword_51A83 = 0;
    qword_53AA9 = origin;
    qword_53AB1 = origin;
    dword_53AB9 = 0;
    dword_53ABD = 0;
    map_view_update(1);
    pal_fade_in();
    delay(200);
}
