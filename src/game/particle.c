/* particle.c - random-offset particle placement helper.  See particle.h.
 *
 * Direct translation of the machine code at 0x21DB2 (analysis/cseg01_disasm.txt
 * and analysis/decomp/21DB2_sub_21DB2.c).  The offline disassembly is
 * authoritative.
 *
 * The function has seven cdecl stack arguments, decoded from the two call
 * sites in 0x21BD0 (both end with `add esp,0x1c`):
 *
 *   radius, index, x[], y[], kind[], cx, cy
 *
 * ABI notes / Hex-Rays artifacts:
 *   - the `push 0x38; call 0x3702F` Watcom stack probe is dropped (it does not
 *     allocate a frame);
 *   - Hex-Rays shows only five parameters because the `sar edx,0x1f / idiv`
 *     sequences hide the two centre coordinates; the machine reads arg6 at
 *     [esp+0x44] and arg7 at [esp+0x48];
 *   - Hex-Rays renders the `__int64` cos/sin returns and the FPU round-trip as
 *     a bogus double local `v8` and calls `_CHP()`; that helper (call 0x37AF4)
 *     is a plain `(int)` cast (same as fx.c / map_action.c / unit_load.c);
 *   - cos/sin are the AIL boundary entries 0x3CBD5 / 0x3CBE8 and are taken
 *     from <math.h>, exactly as fx_dots5 (0x2C67D) does.
 *
 * util_rand (0x4EBE3) is already restored; its result is non-negative, but the
 * machine divides with signed `idiv`, so the `(int32_t)` views below reproduce
 * the exact arithmetic.
 */
#include "particle.h"
#include "util.h"
#include "../include/fd2_globals.h"

#include <math.h>

uint8_t particle_spawn(int radius, unsigned index, int16_t *x, int16_t *y,
                       uint8_t *kind, int cx, int cy)
{
    int    r;
    double angle;

    /* 0x21DC1..0x21DDF: rand % 64, scaled by radius, / 64, - 1. */
    r = (int)(((int32_t)util_rand() % 64) * radius) / 64 - 1;

    /* 0x21DE3..0x21E02: rand % 360 degrees -> radians. */
    angle = (double)((int32_t)util_rand() % 360) * dbl_501F8;

    /* 0x21E05..0x21E4F: x = cos(angle) * r + cx. */
    x[index] = (int16_t)(int)(cos(angle) * (double)r + (double)cx);
    /* 0x21E4F..0x21E8C: y = sin(angle) * r + cy - 8.0. */
    y[index] = (int16_t)(int)(sin(angle) * (double)r + (double)cy + dbl_50200);

    /* 0x21E90..0x21EA8: kind = rand % 8 + 1 (also the return value). */
    kind[index] = (uint8_t)((int32_t)util_rand() % 8 + 1);
    return kind[index];
}
