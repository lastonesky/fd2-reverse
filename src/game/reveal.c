/* reveal.c - expanding-circle portrait reveal.  See reveal.h.
 *
 * Direct translations of the machine code.  The palette translation and the
 * buffer/refresh helpers are the already-verified entries
 * util_translate(0x4DEEC), dlg_portraits_refresh(0x127A9),
 * map_render_view(0x11EEE), gfx_copy_rows(0x11EB0), svc_play_sfx(0x25A96),
 * pal_fade_add(0x11DF2) and ev5_1D4CB(0x1D4CB).
 */
#include "fd2_game.h"
#include "reveal.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define REVEAL_MAP_OFF 0x8088   /* dword_53A49 + 32904 */
#define REVEAL_STRIDE  456

/* 0x219AD - one circular-arc pass.  `scale` is the pixel-per-unit factor
 * (16 in every call site) and the result is divided by the constant 10.0. */
void reveal_arc(int cx, int cy, int radius, int scale, int y0, int y1,
                const void *table)
{
    int y;

    for (y = y0; y < y1; ++y) {
        if (y > cy - radius && y < cy + radius) {
            int d    = abs(cy - y);
            int half = (int)(sqrt((double)(radius * radius - d * d)) *
                             (double)scale / 10.0);
            int left, cwid, rwid;

            if (cx - half >= 0) {
                left = cx - half;
                cwid = half;
            } else {
                left = 0;
                cwid = cx;
            }
            rwid = (cx + half < 312) ? half : 312 - cx;

            util_translate(table, (uint32_t)(cwid + rwid),
                           dword_53A49 + REVEAL_STRIDE * y + left +
                               REVEAL_MAP_OFF);
        }
    }
}

/* 0x22046 - arc pass, portrait refresh, second arc pass from the centre down,
 * then a solid band for rows [y0, cy). */
void reveal_band(int cx, int cy, int radius, int y0, int y1,
                 const void *table)
{
    int      half = (int)((double)radius * 1.6);
    int      left, cwid, rwid;
    uint8_t *dst;
    int      y;

    reveal_arc(cx, cy, radius, 16, y0, y1, table);
    dlg_portraits_refresh();
    reveal_arc(cx, cy, radius, 16, cy, y1, table);

    if (cx - half >= 0) {
        left = cx - half;
        cwid = half;
    } else {
        left = 0;
        cwid = cx;
    }
    rwid = (cx + half < 312) ? half : 312 - cx;

    dst = dword_53A49 + REVEAL_STRIDE * y0 + left + REVEAL_MAP_OFF;
    for (y = y0; y < cy; ++y) {
        util_translate(table, (uint32_t)(cwid + rwid), dst);
        dst += REVEAL_STRIDE;
    }
}

/* 0x24618 - the close-up reveal driver.  `col`/`row` select a 24-px view cell;
 * `offset` is the starting x of the growing circle and `step` its increment. */
int portrait_reveal(int col, int row, int offset, int step)
{
    uint8_t *scratch;
    int      i;

    ev5_1D4CB();

    scratch = (uint8_t *)guest_malloc(0x25680u);
    map_render_view(scratch + REVEAL_MAP_OFF, REVEAL_STRIDE, 13, 8,
                    (int)qword_53AA9, dword_53AAD);
    svc_play_sfx((const void *)(uintptr_t)dword_53B13, 11, 1);

    for (i = 9; i > 0; --i) {
        const void *tbl = (const void *)(dword_53A6D +
                                         *(const int32_t *)(const void *)
                                             (dword_53A6D + 4 * i + 6));

        memmove(dword_53A49, scratch, 0x25680u);
        reveal_band(col * 24 + 12, row * 24 + 16, offset, 0, 192, tbl);
        gfx_copy_rows((void *)VGA_P(0xA0504u), 320, dword_53A49 + REVEAL_MAP_OFF,
                      REVEAL_STRIDE, 0x138, 192);
        offset += step;
        j___delay(5);
    }

    guest_free(scratch);
    j___delay(500);

    for (i = 0; i < 64; i += 2) {
        pal_fade_add(0, 255, i);
        j___delay(4);
    }

    ev5_1D4CB();
    return 0;
}

/* 0x2189A - same reveal, centred on the map cell of record `rec` instead of a
 * 24-px view cell: the circle starts at `radius` and grows by `step` for ten
 * frames, then the map view is fully refreshed. */
int portrait_reveal_cell(int rec, int radius, int step)
{
    const uint8_t *r = (const uint8_t *)dword_53A45 + 80u * (uint32_t)rec;
    int      vx = (r[0] - (int)qword_53AA9) * 24 + 12;
    int      vy = (r[1] - dword_53AAD) * 24 + 18;
    uint8_t *scratch = (uint8_t *)guest_malloc(0x25680u);
    int      i;

    map_render_view(scratch + REVEAL_MAP_OFF, REVEAL_STRIDE, 13, 8,
                    (int)qword_53AA9, dword_53AAD);

    for (i = 0; i < 10; ++i) {
        const void *tbl = (const void *)(dword_53A6D +
                                         *(const int32_t *)(const void *)
                                             (dword_53A6D + 4 * i + 6));

        memmove(dword_53A49, scratch, 0x25680u);
        reveal_arc(vx, vy, radius, 12, 0, 192, tbl);
        dlg_portraits_refresh();
        gfx_copy_rows((void *)VGA_P(0xA0504u), 320, dword_53A49 + REVEAL_MAP_OFF,
                      REVEAL_STRIDE, 0x138, 192);
        radius += step;
    }

    guest_free(scratch);
    map_view_update(0);
    return 0;
}

/* 0x21EB1 - zoom variant of the cell reveal: the circle is centred on the map
 * cell (dword_53AB9, dword_53ABD) and grows by `step` for nine frames, then
 * seven more frames repeat at the last radius, then the view is refreshed. */
int portrait_reveal_zoom(int radius, int step)
{
    int      vx = 24 * dword_53AB9 + 12;
    int      vy = 24 * dword_53ABD + 16;
    uint8_t *scratch = (uint8_t *)guest_malloc(0x25680u);
    int      i;

    map_render_view(scratch + REVEAL_MAP_OFF, REVEAL_STRIDE, 13, 8,
                    (int)qword_53AA9, dword_53AAD);

    for (i = 9; i > 0; --i) {
        const void *tbl = (const void *)(dword_53A6D +
                                         *(const int32_t *)(const void *)
                                             (dword_53A6D + 4 * i + 6));

        memmove(dword_53A49, scratch, 0x25680u);
        reveal_band(vx, vy, radius, 0, 192, tbl);
        gfx_copy_rows((void *)VGA_P(0xA0504u), 320, dword_53A49 + REVEAL_MAP_OFF,
                      REVEAL_STRIDE, 0x138, 192);
        radius += step;
        j___delay(5);
    }

    j___delay(200);
    radius -= step;

    for (i = 3; i < 10; ++i) {
        const void *tbl = (const void *)(dword_53A6D +
                                         *(const int32_t *)(const void *)
                                             (dword_53A6D + 4 * i + 6));

        memmove(dword_53A49, scratch, 0x25680u);
        reveal_band(vx, vy, radius, 0, 192, tbl);
        gfx_copy_rows((void *)VGA_P(0xA0504u), 320, dword_53A49 + REVEAL_MAP_OFF,
                      REVEAL_STRIDE, 0x138, 192);
        j___delay(5);
    }

    guest_free(scratch);
    map_view_update(0);
    j___delay(200);
    return 0;
}
