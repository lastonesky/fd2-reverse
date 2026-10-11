/* map_units.c - animate the unit sprites inside the map view (original 0x1DB65).
 *
 * Collects up to 30 records that are visible in the current view window
 * (record byte +5 bit0 clear, sprite word at +64 zero, coordinates inside
 * [ox-1 .. ox+w+?] x [oy-1 .. oy+h+1] where (ox,oy) is the view origin in
 * qword_53AA9 and (dword_51A87, dword_51A8B) the view size) and remembers the
 * destination cell address of each:
 *     dst = dword_53A49 + 10944*(y-1-oy) + 24*(x-1-ox) + 30168
 * (10944 = 456*24, the view buffer is at dword_53A49 + 32904).
 *
 * Then, if any were found it plays:
 *   13 frames: map_render_view + per-record dlg_portrait_draw (+kind byte j%4
 *              for spriteless records) + map_refresh_records + gfx_copy_rows
 *              + svc_wait_ticks(1);
 *   then marks those records' byte +5 = 1 and, on a 0x25680 scratch buffer
 *   (the same size the task buffer in main uses), plays 6 frames with atlas
 *   sub-images 68..73 (0x1DB65's caller passes FDOTHER.DAT entry 5 as
 *   dword_53A81) blitted into each remembered cell, then restores the scratch
 *   from a saved copy for 6 more frames (74..79) and frees it.
 * With no visible record it just marks them (+5 = 1) and returns.
 *
 * The `&loc_2567F + 1` and `malloc(&loc_2567F + 1)` renderings are the
 * size-immediate trap: both are 0x25680 (documented in PROGRESS §1.5).
 */
#include "fd2_game.h"
#include "map_units.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define VIEW_SRC_OFF  32904u      /* dword_53A49 + 0x8088 */
#define CELL_BASE     30168u      /* dword_53A49 + 0x75D8 */
#define CELL_STRIDE   10944u      /* 456 * 24 */
#define SCRATCH_SIZE  0x25680u

/* 0x1DB65 */
void map_units_animate(void)
{
    uint8_t *buf   = (uint8_t *)(uintptr_t)dword_53A49;
    uint8_t *atlas = (uint8_t *)(uintptr_t)dword_53A81;
    uint8_t *v19[30];
    uint8_t *scratch;
    int ox = (int)qword_53AA9;
    int oy = (int)(qword_53AA9 >> 32);
    int found = 0;
    int i, j, n;

    for (i = 0; i < (int)dword_53BEB; ++i) {
        uint8_t *rec = (uint8_t *)(uintptr_t)dword_53A45 + 80u * (uint32_t)i;
        int x = rec[0];
        int y = rec[1];

        if ((rec[5] & 1) == 0
            && *(const uint16_t *)(const void *)(rec + 64) == 0
            && x >= ox - 1 && x <= (int)dword_51A87 + ox
            && y >= oy - 1 && y <= (int)dword_51A8B + oy + 1) {
            v19[found++] = buf + CELL_STRIDE * (uint32_t)(y - 1 - oy)
                               + 24u * (uint32_t)(x - 1 - ox) + CELL_BASE;
        }
    }

    if (found == 0) {
        for (i = 0; i < (int)dword_53BEB; ++i) {
            uint8_t *rec = (uint8_t *)(uintptr_t)dword_53A45 + 80u * (uint32_t)i;
            if (*(const uint16_t *)(const void *)(rec + 64) == 0)
                rec[5] = 1;
        }
        return;
    }

    for (j = 0; j < 13; ++j) {
        sub_11EEE((int)(uintptr_t)buf + VIEW_SRC_OFF, 456, 13, 8, ox, oy);
        for (i = 0; i < (int)dword_53BEB; ++i) {
            uint8_t *rec = (uint8_t *)(uintptr_t)dword_53A45 + 80u * (uint32_t)i;
            if ((rec[5] & 1) == 0) {
                if (*(const uint16_t *)(const void *)(rec + 64) == 0)
                    rec[3] = (uint8_t)(j % 4);
                sub_127E0(i);
            }
        }
        sub_129EC();
        sub_11EB0((char *)0xA0504, 320, (char *)(uintptr_t)dword_53A49 + VIEW_SRC_OFF,
                  456, 0x138u, 192);
        sub_17AA9(1);
    }

    for (i = 0; i < (int)dword_53BEB; ++i) {
        uint8_t *rec = (uint8_t *)(uintptr_t)dword_53A45 + 80u * (uint32_t)i;
        if (*(const uint16_t *)(const void *)(rec + 64) == 0)
            rec[5] = 1;
    }

    scratch = (uint8_t *)malloc(SCRATCH_SIZE);      /* 0x25680 */
    sub_11EEE((int)(uintptr_t)scratch + VIEW_SRC_OFF, 456, 13, 8, ox, oy);
    {
        uint8_t *saved = buf;
        dword_53A49 = (uint32_t)(uintptr_t)scratch;
        sub_127A9();
        dword_53A49 = (uint32_t)(uintptr_t)saved;
    }
    sub_25A96((int)dword_53EEC, 3, 1);              /* svc_play_sfx */

    for (n = 0; n < 6; ++n) {
        for (i = 0; i < found; ++i) {
            uint32_t off = *(const uint32_t *)
                (const void *)(atlas + 4u * (uint32_t)(n + 68) + 6u);
            sub_4EBAB(v19[i], (int16_t *)(atlas + off), 456);
        }
        sub_11EB0((char *)0xA0504, 320, (char *)(uintptr_t)dword_53A49 + VIEW_SRC_OFF,
                  456, 0x138u, 192);
        sub_17AA9(1);
    }

    for (j = 6; j < 12; ++j) {
        memmove((void *)(uintptr_t)dword_53A49, scratch, SCRATCH_SIZE);
        for (i = 0; i < found; ++i) {
            uint32_t off = *(const uint32_t *)
                (const void *)(atlas + 4u * (uint32_t)(j + 68) + 6u);
            sub_4EBAB(v19[i], (int16_t *)(atlas + off), 456);
        }
        sub_11EB0((char *)0xA0504, 320, (char *)(uintptr_t)dword_53A49 + VIEW_SRC_OFF,
                  456, 0x138u, 192);
        sub_17AA9(1);
    }

    free(scratch);
    sub_11CAC(0);
}
