/* map_scroll.c - single-cell map view scroll steps (see map_scroll.h).
 *
 * Direct translations of the machine code at the stated addresses. The offline
 * disassembly is authoritative for the constants, the signed comparisons and
 * the (clip-free) control flow; Hex-Rays' 64-bit qword_53AA9/qword_53AB1 are
 * just the x/y pairs typed as 32-bit values here (dword_53AAD/dword_53AB5 are
 * the high halves). All callees are already-restored assets:
 *   map_unit_ping(0x32230)  pal_anim_step(0x4E31C)  anim_frame_step(0x1297D)
 *   map_render_view(0x11EEE)  dlg_portraits_refresh(0x127A9)
 *   gfx_copy_rows(0x11EB0)  ev5_13460(0x13460)  map_cell_info(0x12E38)
 * The Watcom stack probe (push size; call 0x3702F) and the shared epilogues
 * (jmp into a neighbour's tail) become plain returns here.
 */
#include "fd2_game.h"
#include "map_scroll.h"

#define REC_STRIDE 80

/* 0x12EAA - record `rec` moves down; the view follows (origin y +1). */
void map_scroll_down(int rec)
{
    uint8_t *r = (uint8_t *)dword_53A45 + REC_STRIDE * rec;
    int      by = 0;                  /* v1: per-step screen/bypass offset */
    int      dy = 0;                  /* v9: per-step origin delta          */
    int      row = 0x8088;            /* v4: map-bitmap row offset          */
    int      frame;
    int      y0 = r[1];

    r[3] = 0;
    dword_53B0B = 0;
    if ((int)y0 - dword_53AAD <= 5
        || dword_53AC5 - dword_51A8B == dword_53AAD) {
        ++dword_53ABD;
    } else {
        by = 0x720;
        dy = 1;
    }

    for (frame = 1; frame < 7; ++frame) {
        map_unit_ping(rec);
        pal_anim_step();
        r[4] = (uint8_t)frame;
        anim_frame_step();
        dword_53AF5 += by;
        dword_53B0B += dy;
        map_render_view(dword_53A49 + 0x8088, 456, 13, by ? 9 : 8,
                        (int)dword_53AA9, dword_53AAD);
        dlg_portraits_refresh();
        row += by;
        gfx_copy_rows((void *)(uintptr_t)0xA0504u, 320, dword_53A49 + row,
                      456, 0x138, 0xC0);
        ev5_13460();
    }

    r[1] = (uint8_t)(y0 + 1);
    dword_53AAD += dy;
    ++dword_53AB5;
    r[4] = 0;
    dword_53AF5 = 0;
    dword_53B0B = 0;
    map_scroll_cell_tag((int)dword_53AB1, (int)dword_53AB5, 0);
}

/* 0x1300D - record `rec` moves left; the view follows (origin x -1). */
void map_scroll_left(int rec)
{
    uint8_t *r = (uint8_t *)dword_53A45 + REC_STRIDE * rec;
    int      by = 0;
    int      dx = 0;
    int      row = 0x8088;
    int      frame;
    int      x0 = r[0];

    r[3] = 1;
    dword_53B07 = 6;
    if ((int)x0 - (int)dword_53AA9 < 2 && (int)dword_53AA9 != 0) {
        by = -4;
        dx = -1;
    } else {
        --dword_53AB9;
    }

    for (frame = 1; frame < 7; ++frame) {
        map_unit_ping(rec);
        pal_anim_step();
        r[4] = (uint8_t)frame;
        anim_frame_step();
        dword_53AED = 24;
        dword_53AF5 += by;
        dword_53B07 += dx;
        map_render_view(dword_53A49 + 0x8070, 456, 14, 8,
                        (int)dword_53AA9 - 1, dword_53AAD);
        dword_53AED = 0;
        dlg_portraits_refresh();
        row += by;
        gfx_copy_rows((void *)(uintptr_t)0xA0504u, 320, dword_53A49 + row,
                      456, 0x138, 0xC0);
        ev5_13460();
    }

    r[0] = (uint8_t)(x0 - 1);
    dword_53AA9 += dx;
    --dword_53AB1;
    r[4] = 0;
    dword_53AF5 = 0;
    dword_53B07 = 0;
    map_scroll_cell_tag((int)dword_53AB1, (int)dword_53AB5, 0);
}

/* 0x13185 - record `rec` moves up; the view follows (origin y -1). */
void map_scroll_up(int rec)
{
    uint8_t *r = (uint8_t *)dword_53A45 + REC_STRIDE * rec;
    int      by = 0;
    int      dy = 0;
    int      row = 0x8088;
    int      frame;
    int      y0 = r[1];

    r[3] = 2;
    dword_53B0B = (dword_53AAD != 0) ? 6 : 0;
    if ((int)y0 - dword_53AAD < 2 && dword_53AAD != 0) {
        by = -1824;
        dy = -1;
    } else {
        --dword_53ABD;
    }

    for (frame = 1; frame < 7; ++frame) {
        map_unit_ping(rec);
        pal_anim_step();
        r[4] = (uint8_t)frame;
        anim_frame_step();
        dword_53AF5 += by;
        dword_53B0B += dy;
        if (dword_53AAD != 0) {
            dword_53AF1 = 24;
            map_render_view(dword_53A49 + 0x55C8, 456, 13, 9,
                            (int)dword_53AA9, dword_53AAD - 1);
            dword_53AF1 = 0;
        } else {
            map_render_view(dword_53A49 + 0x8088, 456, 13, 9,
                            (int)dword_53AA9, 0);
        }
        dlg_portraits_refresh();
        row += by;
        gfx_copy_rows((void *)(uintptr_t)0xA0504u, 320, dword_53A49 + row,
                      456, 0x138, 0xC0);
        ev5_13460();
    }

    r[1] = (uint8_t)(y0 - 1);
    dword_53AAD += dy;
    --dword_53AB5;
    r[4] = 0;
    dword_53AF5 = 0;
    dword_53B0B = 0;
    map_scroll_cell_tag((int)dword_53AB1, (int)dword_53AB5, 0);
}

/* 0x13315 - record `rec` moves right; the view follows (origin x +1). */
void map_scroll_right(int rec)
{
    uint8_t *r = (uint8_t *)dword_53A45 + REC_STRIDE * rec;
    int      by = 0;
    int      dx = 0;
    int      row = 0x8088;
    int      frame;
    int      x0 = r[0];

    r[3] = 3;
    dword_53B07 = 0;
    if ((int)x0 - (int)dword_53AA9 <= 10
        || dword_53AC1 - dword_51A87 == dword_53AA9) {
        ++dword_53AB9;
    } else {
        by = 4;
        dx = 1;
    }

    for (frame = 1; frame < 7; ++frame) {
        map_unit_ping(rec);
        pal_anim_step();
        r[4] = (uint8_t)frame;
        anim_frame_step();
        dword_53AF5 += by;
        dword_53B07 += dx;
        map_render_view(dword_53A49 + 0x8088, 456, by ? 14 : 13, 8,
                        (int)dword_53AA9, dword_53AAD);
        dlg_portraits_refresh();
        row += by;
        gfx_copy_rows((void *)(uintptr_t)0xA0504u, 320, dword_53A49 + row,
                      456, 0x138, 0xC0);
        ev5_13460();
    }

    r[0] = (uint8_t)(x0 + 1);
    dword_53AA9 += dx;
    ++dword_53AB1;
    r[4] = 0;
    dword_53AF5 = 0;
    dword_53B07 = 0;
    map_scroll_cell_tag((int)dword_53AB1, (int)dword_53AB5, 0);
}

/* 0x13488 - walk the direction-code list, one map step per entry. */
void map_scroll_path(int rec, const uint8_t *dirs, int count)
{
    int i;

    for (i = 0; i < count; ++i) {
        uint8_t d = dirs[i];
        if (d == 0)
            map_scroll_down(rec);
        else if (d == 1)
            map_scroll_left(rec);
        else if (d == 2)
            map_scroll_up(rec);
        else
            map_scroll_right(rec);
    }
}

/* 0x13A44 - latch the record tag of the cell at (x, y). The machine leaves a
 * byte in eax on some paths but no caller reads it. */
void map_scroll_cell_tag(int x, int y, int which)
{
    uint8_t cell[8];

    map_cell_info(x, y, cell);
    if ((cell[4] & 0x60) == 0) {
        unsigned idx = *(uint16_t *)(void *)(cell + 2);
        if (idx != 0) {
            uint8_t v = *(uint8_t *)(uintptr_t)(dword_53A55 + 2 * (idx - 1) + 0x33);
            uint8_t t = *(uint8_t *)(uintptr_t)(dword_53A55 + 2 * (idx - 1) + 0x34);
            if (v != 0xFF && t == which)
                dword_51A8F = v;
        }
    }
}
