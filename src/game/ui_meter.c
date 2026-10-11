/* ui_meter.c - unit status panel / meter bars.  See ui_meter.h.
 *
 * Direct translations of the machine code at the stated addresses.  The meter
 * tiles come from the box-frame sprite resource `dword_53A81` through the
 * already-verified dlg_frame_tile (0x1685C); the panel background comes through
 * rle2_blit (0x4EBFF); the numbers through dlg_draw_number(_pair) (0x187D6 /
 * 0x1875D); the name stream through vm_run (0x15F84).
 */
#include "ui_meter.h"
#include "dlg.h"
#include "rle2.h"
#include "vm.h"

#include <stdint.h>

#include "../include/fd2_globals.h"

/* 0x17D6F - draw a segmented meter of `count` tiles.  A non-zero count draws
 * one tile at dst, then (count-1) body tiles starting at tile+1, then an end
 * tile at tile+2; a zero count draws the fixed 101-segment empty meter
 * (tiles 29 and 30). */
void ui_meter_tiles(uint8_t *dst, int pitch, int count, int tile)
{
    const void *table = (const void *)dword_53A81;
    int i;

    if (count != 0) {
        dlg_frame_tile(dst, pitch, table, tile);
        for (i = 1; i < count; ++i)
            dlg_frame_tile(dst + i, pitch, table, tile + 1);
        dlg_frame_tile(dst + i, pitch, table, tile + 2);
    } else {
        i = 0;
        while (++i <= 101)
            dlg_frame_tile(dst + i, pitch, table, 29);
        dlg_frame_tile(dst + i, pitch, table, 30);
    }
}

/* 0x18795 - value/max -> segments.  The machine does a signed
 * `value*101 / max + 1`; max==0 returns without drawing (the original leaves
 * eax untouched; callers ignore it). */
void ui_meter_bar(uint8_t *dst, int pitch, int tile, int value, int max)
{
    int segments;

    if (max == 0)
        return;
    segments = (value != 0) ? (101 * value / max + 1) : 0;
    ui_meter_tiles(dst, pitch, segments, tile);
}

/* 0x18C6D - draw the whole status box for `rec`.  The four signed 16-bit words
 * at record offsets +64/+66/+68/+70 are the two value/max pairs; the byte at
 * +33 is an auxiliary number and the byte at +8 selects the name sub-stream. */
void ui_status_panel(char *dst, int pitch, int rec)
{
    const uint8_t *r = (const uint8_t *)dword_53A45 + 80u * (uint32_t)rec;
    int v5 = *(const int16_t *)(const void *)(r + 64);
    int v8 = *(const int16_t *)(const void *)(r + 66);
    int v7 = *(const int16_t *)(const void *)(r + 68);
    int v6 = *(const int16_t *)(const void *)(r + 70);
    const uint8_t *b81 = (const uint8_t *)dword_53A81;

    rle2_blit(dst,
              (const void *)(b81 + *(const int32_t *)(const void *)(b81 + 0x5E)),
              pitch);

    ui_meter_bar((uint8_t *)dst + 22 * pitch + 21, pitch, 23, v5, v8);
    ui_meter_bar((uint8_t *)dst + 31 * pitch + 21, pitch, 26, v7, v6);

    dlg_draw_number(dst + 4 * pitch + 132, pitch,
                    r[33], 31, 2);
    dlg_draw_number_pair(dst + 21 * pitch + 126, pitch,
                         v5, v8, 3);
    dlg_draw_number_pair(dst + 30 * pitch + 126, pitch,
                         v7, v6, 3);

    vm_run(dword_53A7D, r[8] + 1,dst + 4 * pitch + 5,
           pitch, 205, 76, 0, 0, 0);
}

/* 0x2FACD - place the status box for `rec` on the HUD.  The panel sits at
 * dst+1451 normally, or at dst+49280 when the record has no sprite (and always
 * for record 17 while game mode dword_53C03 is 24). */
void ui_status_panel_at(uint8_t *dst, int rec)
{
    int off;

    if (((const uint8_t *)dword_53A45)[80 * rec + 6] != 0)
        off = 1451;
    else
        off = 49280;
    if (dword_53C03 == 24 && rec == 17)
        off = 49280;

    ui_status_panel((char *)dst + off, 320, rec);
}

/* 0x1E739 - draw the fixed 70-tile gauge used over the map view.  A positive
 * `count` draws the filled style (left cap 23, body 24 up to count-1, right cap
 * 25) followed by empty tiles 29 up to position 69 and the end cap 30; a
 * non-positive count draws the all-empty 29..29,30 bar.  Tiles come from the
 * frame resource dword_53A81; the machine's odd total lengths (71 vs 70) are
 * kept verbatim. */
void ui_meter_fixed(uint8_t *dst, int pitch, int count)
{
    const void *table = (const void *)dword_53A81;
    int i;

    if (count <= 0) {
        for (i = 0; i < 69; ++i)
            dlg_frame_tile(dst + i, pitch, table, 29);
        dlg_frame_tile(dst + i, pitch, table, 30);
        return;
    }

    dlg_frame_tile(dst, pitch, table, 23);
    for (i = 1; i < count; ++i)
        dlg_frame_tile(dst + i, pitch, table, 24);
    dlg_frame_tile(dst + i, pitch, table, 25);

    if (count < 70) {
        while (i < 69) {
            ++i;
            dlg_frame_tile(dst + i, pitch, table, 29);
        }
        ++i;
        dlg_frame_tile(dst + i, pitch, table, 30);
    }
}
