/* map_zoom.c - map zoom transition frame (original 0x1F558, 407 bytes).
 *
 * Direct translation of the machine code at 0x1F558; the offline disassembly
 * (analysis/cseg01_disasm.txt) is authoritative.  Behaviour:
 *
 *   - the fixed-point centre is (x, y) with 128 = one map cell / zoom unit;
 *     the visible top-left cell is centre - ( 156*zoom, 96*zoom ) / 3072, i.e.
 *     half of the 13x8-cell window scaled by zoom (156 = 312/2, 96 = 192/2 and
 *     3072 = 24*128);
 *   - the whole dword_53A49 frame is cleared, then each of the 192 output rows
 *     samples the source cell pointed at by cells[row*64 + col], advancing the
 *     source cell every 24 output pixels and the source byte by zoom/128 per
 *     output pixel (nearest-neighbour).
 *
 * Notes / decompiler artifacts:
 *   - the Watcom stack probe (`push 0x3C; call 0x3702F`) is dropped;
 *   - Hex-Rays renders the truncating `/128` and the signed `/3072` remainders
 *     through `__CFSHL__`/`__CFSHR__` helpers; here they are plain C `/` and `%`
 *     on ints (both inputs are already normalised non-negative);
 *   - the tail `jmp 0x18888` lands in dlg_draw_number's (0x187D6) shared
 *     epilogue; it is a plain return, and the function-map callee sub_187D6 is
 *     therefore false.
 */
#include "fd2_game.h"
#include "map_zoom.h"

#include <stdint.h>
#include <string.h>

/* 0x1F558 */
void map_zoom_draw(int x, int y, int zoom, const uint32_t *cells)
{
    int      qx = (x - 156 * zoom) / 3072;
    int      rx = (x - 156 * zoom) % 3072;
    int      qy = (y - 96 * zoom) / 3072;
    int      ry = (y - 96 * zoom) % 3072;
    int      qx0, rx0, row;
    uint8_t *dst;

    if (ry < 0) {
        ry += 3072;
        --qy;
    }
    if (rx < 0) {
        rx += 3072;
        --qx;
    }
    qx0 = qx;
    rx0 = rx;

    memset(dword_53A49, 0, 0xFA00u);
    dst = dword_53A49 + 0x504;

    for (row = 0; row < 192; ++row) {
        int cellofs = 24 * (ry / 128);

        if (qy >= 0 && qy < (int)dword_53AC5) {
            const uint8_t *src =
                (const uint8_t *)(uintptr_t)(cells[qy * 64 + qx] +
                                             (uint32_t)cellofs);
            int col;

            for (col = 0; col < 312; ++col) {
                if (qx >= 0 && qx < (int)dword_53AC1)
                    dst[col] = src[rx / 128];
                rx += zoom;
                if (rx >= 3072) {
                    ++qx;
                    rx -= 3072;
                    src = (const uint8_t *)(uintptr_t)(cells[qy * 64 + qx] +
                                                       (uint32_t)cellofs);
                }
            }
        }

        qx = qx0;
        rx = rx0;
        ry += zoom;
        if (ry >= 3072) {
            ++qy;
            ry -= 3072;
        }
        dst += 320;
    }
}
