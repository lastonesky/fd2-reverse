/* map_status.c - the map-view status frame (original 0x1E611 + 0x1E7F6 +
 * 0x1EC2A + 0x1EB05).
 *
 * Direct translation of the machine code at 0x1E611/0x1E7F6/0x1EC2A; the
 * offline disassembly (analysis/cseg01_disasm.txt) is authoritative.
 *
 *   - the Watcom stack probe (`push size; call 0x3702F`) is dropped;
 *   - 0x1E611's tail `jmp 0x11D2C` lands inside map_view_update (0x11CAC): it
 *     is that function's shared epilogue, which runs gfx_copy_rows(VGA_P(0xA0504u),
 *     320, view, 456, 312, 192) and then returns.  It is reproduced here as
 *     that call plus a plain return, so function-map's 0x1E611 callee sub_11CAC
 *     (and its callees) are false dependencies;
 *   - 0x1E611's Hex-Rays prototype says `int`, but the only value is whatever
 *     gfx_copy_rows leaves in eax and no caller reads it, so it returns void.
 *
 *   - 0x1EC2A's Hex-Rays prototype says `int`; the machine leaves a computed
 *     value in eax but the only caller (0x1EB05) discards it, so it is void
 *     here.  Its `r[3] >= 2` test is a `movzx; cmp 2; jge` over a byte, so the
 *     unsigned `r[3] < 2` below is exact.
 */
#include "fd2_game.h"
#include "map_status.h"

#include <stdint.h>

#define STATUS_VIEW_OFF  0x8088u /* dword_53A49 + 32904  (13x8 tile window)   */
#define STATUS_GAUGE_OFF 0x7964u /* dword_53A49 + 31076 (record gauge strip) */

/* 0x1E7F6 */
void map_status_gauge(uint8_t *dst, int pitch, int rec, const int *pos)
{
    const uint8_t *r =
        (const uint8_t *)(uintptr_t)dword_53A45 + 80u * (uint32_t)rec;
    uint32_t cur = *(const uint16_t *)(uintptr_t)(r + 0x40);
    uint32_t max = *(const uint16_t *)(uintptr_t)(r + 0x42);

    if (cur != 0)
        ui_meter_fixed(dst + pos[0] + pitch * (pos[1] + 6) + 7, pitch,
                       (int)(69u * cur / max) + 1);
}

/* 0x1E611 */
void map_status_frame(const int *pos, int actor, int target)
{
    uint8_t *view  = dword_53A49 + STATUS_VIEW_OFF;
    void    *gauge = dword_53A49 + STATUS_GAUGE_OFF;
    uint8_t *bar;

    map_render_view(view, 456, 13, 8, (int)dword_53AA9, dword_53AAD);
    dlg_portraits_refresh();

    res_draw_subimage((const void *)(uintptr_t)dword_53AD1, view, 456,
                      pos[0] - 4, pos[1] - 4, 48);
    bar = view + 456 * (pos[1] + 2) + pos[0] + 3;
    ui_meter_fixed(bar, 456, 55);
    map_status_gauge(gauge, 456, target, pos);

    if (pos[2] != -1) {
        res_draw_subimage((const void *)(uintptr_t)dword_53AD1, view, 456,
                          pos[2] - 4, pos[3] - 4, 48);
        map_status_gauge(gauge, 456, actor, pos + 2);
    }

    gfx_copy_rows((void *)(uintptr_t)VGA_P(0xA0504u), 320, view, 456, 312, 192);
}

/* 0x1EC2A - compute the two-int {x, y} screen anchor of record `rec` in the
 * current map view, nudged toward the 320x200 frame.  `out[0]`/`out[1]` are
 * filled directly; the driver 0x1EB05 calls this twice to build the actor /
 * target halves of the status `pos` block (dword_53A30 / dword_53A38). */
void map_status_anchor(int *out, int rec)
{
    const uint8_t *r =
        (const uint8_t *)(uintptr_t)dword_53A45 + 80u * (uint32_t)rec;

    out[0] = 24 * ((int)r[0] - (int)dword_53AA9) + 4;
    out[1] = 24 * ((int)r[1] - (int)dword_53AAD);

    if (r[3] < 2u) {
        if (out[1] - 18 >= 0)
            out[1] -= 18;
        else
            out[1] += 5;
        if (out[0] + 108 > 319)
            out[0] -= 86;
        else
            out[0] += 28;
    } else {
        if (out[1] + 37 <= 199)
            out[1] += 22;
        else
            out[1] += 5;
        if (out[0] - 86 < 0)
            out[0] += 28;
        else
            out[0] -= 88;
    }
}

/* 0x1EB05 - flash the actor/target status overlays over the VGA page.
 *
 * Direct translation of the machine code at 0x1EB05; the offline disassembly
 * (analysis/cseg01_disasm.txt) is authoritative.  The Watcom stack probe
 * (`push 0x2c; call 0x3702F`) is dropped.  Hex-Rays renders the second
 * parameter as an uninitialised local because the frozen fd2_funcs.h prototype
 * is `int sub_1EB05(int)`; the machine reads [esp+0x18] (arg 2) and the only
 * caller 0x1548E pushes both, so the real ABI is (actor, target).
 *
 * The target anchor is written unconditionally to dword_53A30; the actor
 * anchor (dword_53A38) is written only when ev5_1F0DC reports a partner (else
 * its x is -1).  Ten frames of portrait sub-images 39..48 are blitted over
 * VGA_P(0xA0000u) with res_draw_subimage, restored with dlg_snap_restore for the first
 * nine, and freed afterwards.  Returns the address of the target anchor block. */
int *map_status_overlay_play(int actor, int target)
{
    int *pos = (int *)(void *)&dword_53A30;
    void *v2 = 0, *v1 = 0;
    int i;

    map_status_anchor(pos, target);
    if (ev5_1F0DC(actor, target) == 1)
        map_status_anchor((int *)(void *)&dword_53A38, actor);
    else
        dword_53A38 = 0xFFFFFFFFu;

    for (i = 0; i < 10; ++i) {
        v2 = res_draw_subimage((const void *)(uintptr_t)dword_53AD1,
                               (void *)VGA_P(0xA0000u), 320, dword_53A30, dword_53A34,
                               i + 39);
        if (dword_53A38 != 0xFFFFFFFFu)
            v1 = res_draw_subimage((const void *)(uintptr_t)dword_53AD1,
                                   (void *)VGA_P(0xA0000u), 320, dword_53A38,
                                   dword_53A3C, i + 39);
        j___delay(25);
        if (i < 9) {
            dlg_snap_restore(v2, (void *)VGA_P(0xA0000u), 320);
            if (dword_53A38 != 0xFFFFFFFFu)
                dlg_snap_restore(v1, (void *)VGA_P(0xA0000u), 320);
        }
    }

    guest_free(v2);
    if (dword_53A38 != 0xFFFFFFFFu)
        guest_free(v1);

    return pos;
}
