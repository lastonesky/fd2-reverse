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

#include "../dos.h"

#include <stdint.h>
#include <string.h>

/* BIOS tick word, read through the low-memory mirror (anim.c pattern). */
#define BDA_W(off) (*(volatile uint16_t *)(uintptr_t)(DOS_LOWMEM_BASE + (off)))

/* 0x1F558 */
void map_zoom_draw(int x, int y, int zoom, const uint8_t *const *cells)
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
                cells[qy * 64 + qx] + (uint32_t)cellofs;
            int col;

            for (col = 0; col < 312; ++col) {
                if (qx >= 0 && qx < (int)dword_53AC1)
                    dst[col] = src[rx / 128];
                rx += zoom;
                if (rx >= 3072) {
                    ++qx;
                    rx -= 3072;
                    src = cells[qy * 64 + qx] + (uint32_t)cellofs;
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

/* 0x2000A - the map zoom-in/out transition driver.  Decode the current shape
 * library into a raw 24x24 bitmap library, sample it through the 64x64 source
 * table (same as map_zoom_draw's caller), then play the seven zoom-in frames
 * k = 1..7 (centre lerped by (k+1)/8, zoom k*zoom_w/7 + 128) into the VGA
 * frame; hold the zoomed view - blinking the record markers (`phase` 0..7,
 * ping-ponging by phase_step) and the player cell every two BIOS ticks - until
 * a key is pending; then play the six zoom-out frames m = 6..1 (centre
 * (m+1)/8, zoom m*zoom_w/7 + 128), release the scratch buffers and refresh the
 * map view.
 *
 * Notes / decompiler artifacts:
 *   - the Watcom stack probe (`push 0x60; call 0x3702F`) is dropped; the
 *     malloc/free pair is the guest heap (0x3706E/0x3776E);
 *   - the `>>3` centre lerp is the machine's truncating signed /8 (the
 *     `__CFSHL__`/`sbb` sequence), written here as plain `/ 8`; the machine
 *     uses the pre-increment loop counter for the zoom and the post-increment
 *     one for the centre, faithfully kept below;
 *   - the tick baseline `last_tick` is read uninitialised by the machine (the
 *     first pass therefore always renders); modelled as 0, following the same
 *     convention as map_wait_key (0x12DAC). */
int map_zoom_run(void)
{
    int       base[3];        /* 0x5208A: record-kind marker sprite banks */
    uint8_t   cell[8];
    uint8_t  *saved_shape;
    int       scale;
    int       zoom_w;
    int       cx0, cy0;       /* fixed-point start centre (0x53AA9/0x53AAD) */
    int       cx1, cy1;       /* fixed-point end centre   (0x53AC1/0x53AC5) */
    uint8_t **cells;
    int       phase, phase_step;
    int       last_tick;
    int       i, j, k;

    memcpy(base, &fd2_data1[0x208A], sizeof base);

    scale      = 4;
    zoom_w     = 640;
    phase      = 7;
    phase_step = -1;
    last_tick  = 0;

    saved_shape = dword_53A5D;
    dword_53A5D  = map_shape_lib_decode();

    if ((int)dword_53AC5 > 40) {
        scale  = 3;
        zoom_w = 896;
    }

    cx0 = 3072 * (int)dword_53AA9 + 1536 * dword_51A87;
    cy0 = 3072 * dword_53AAD + 1536 * dword_51A8B;
    cx1 = 1536 * (int)dword_53AC1;
    cy1 = 1536 * (int)dword_53AC5;

    /* 64x64 table of 24x24 source bases over the decoded shape library. */
    cells = (uint8_t **)guest_malloc(64u * 64u * sizeof(uint8_t *));
    for (i = 0; i < (int)dword_53AC5; ++i) {
        for (j = 0; j < (int)dword_53AC1; ++j) {
            map_cell_info(j, i, cell);
            cells[64 * i + j] = dword_53A5D + 6 +
                                576u * *(uint16_t *)(void *)cell;
        }
    }

    /* Zoom-in: the centre uses the post-increment factor k+1, the zoom the
     * pre-increment factor k (machine 0x20143). */
    for (k = 1; k < 8; ++k) {
        map_zoom_draw(cx0 + (k + 1) * (cx1 - cx0) / 8,
                      cy0 + (k + 1) * (cy1 - cy0) / 8,
                      k * zoom_w / 7 + 128,
                      cells);
        memmove((void *)(uintptr_t)VGA_P(0xA0000u), dword_53A49, 0xFA00u);
    }

    kbd_flush();
    while (kbd_pending() == 0) {
        int diff = (int)(int16_t)BDA_W(0x46C) - last_tick;

        if (diff >= 2 || diff < 0) {
            int half_w = scale * (int)dword_53AC1 / 2;
            int half_h = scale * (int)dword_53AC5 / 2;

            for (k = 0; k < (int)dword_53BEB; ++k) {
                const uint8_t *rec =
                    (const uint8_t *)dword_53A45 + 80u * (uint32_t)k;

                if ((rec[5] & 1) || rec[7] == 0x79 || rec[31] == 10)
                    continue;
                ev5_1F6EF(scale * rec[0] + 160 - half_w,
                          scale * rec[1] + 100 - half_h,
                          phase + base[rec[6]], scale);
            }
            ev5_1F6EF(scale * (int)dword_53AB1 + 160 - half_w,
                      scale * (int)dword_53AB5 + 100 - half_h,
                      phase + 24, scale);

            phase += phase_step;
            if (phase < 0 || phase > 7) {
                phase_step = -phase_step;
                phase += phase_step;
            }
            last_tick = (int)(int16_t)BDA_W(0x46C);
        }
    }

    byte_53A8E = 0x10;
    int386(0x16, &word_53A8D, &word_53A8D);

    /* Zoom-out: centre factor m+1, zoom factor m (machine 0x20309). */
    for (k = 6; k > 0; --k) {
        map_zoom_draw(cx0 + (k + 1) * (cx1 - cx0) / 8,
                      cy0 + (k + 1) * (cy1 - cy0) / 8,
                      k * zoom_w / 7 + 128,
                      cells);
        memmove((void *)(uintptr_t)VGA_P(0xA0000u), dword_53A49, 0xFA00u);
    }

    guest_free(cells);
    guest_free(dword_53A5D);
    dword_53A5D = saved_shape;
    map_view_update(0);
    return 0;
}
