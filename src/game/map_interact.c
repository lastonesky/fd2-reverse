/* map_interact.c - map-view record interaction loop.  See map_interact.h.
 *
 * Direct translations of the machine code at the stated addresses.  Uses the
 * already-verified assets ev4_11B48/11B9B/11BFA/11C59, dlg_portrait_glide
 * (0x12D7B), map_cell_info (0x12E38), map_view_update (0x11CAC), pal_anim_step
 * (0x4E31C), rec_flag (0x34894), rec_skip (0x1F183), svc_play_sfx (0x25A96),
 * kbd_pending (0x10620) and the 0x4E8A5 table (base 0x61646, stride 20).
 */
#include "fd2_game.h"
#include "map_interact.h"

#include "../dos.h"

#include <stdint.h>
#include <stdlib.h>

#define BDA_W(off) (*(volatile uint16_t *)(uintptr_t)(DOS_LOWMEM_BASE + (off)))

/* 0x14742 - collect records near (x, y). */
int map_find_records(int x, int y, int radius, uint8_t *out, int filter)
{
    int count = 0;
    int i;

    for (i = 0; i < dword_53BEB; ++i) {
        const uint8_t *r = (const uint8_t *)dword_53A45 + 80u * (uint32_t)i;
        int dist = abs((int)r[0] - x) + abs((int)r[1] - y);

        if ((r[5] & 1) == 0 && dist < radius &&
            ((filter == 0 && r[6] == 0) ||
             (filter == 1 && r[6] != 0) ||
             (filter == 2 && r[6] == 1) ||
             (filter == 3 && r[6] == 2))) {
            if (out != NULL)
                out[count] = (uint8_t)i;
            ++count;
        }
    }
    return count;
}

/* 0x12DAC - wait for a key with palette animation and map refresh. */
int map_wait_key(void)
{
    int32_t v0 = 0;             /* cached BIOS tick (the machine reads an
                                   uninitialised `esi` on the first pass) */

    while (kbd_pending() == 0) {
        pal_anim_step();
        if (v0 != (int16_t)BDA_W(0x46C)) {
            map_view_update(0);
            v0 = (int16_t)BDA_W(0x46C);
        }
        fd2_wait_tick_or_key();
    }

    byte_53A8E = 0x10;
    int386(0x16, &word_53A8D, &word_53A8D);
    if (byte_53A8E == 0xE0 || byte_53A8E == 0x52)
        byte_53A8E = 0x1C;
    if (byte_53A8E == 0x53)
        byte_53A8E = 0x01;

    return byte_53A8E;
}

/* 0x115B6 - the interactive map record picker. */
int map_pick_record(int mode, int sel, const uint8_t *list)
{
    int32_t v3  = sel;          /* v3: cycle length (0 when mode == 6) */
    int     v4  = 0;            /* v4: index into `list`               */
    int     v15 = 0;            /* v15: selected record (mode == 6)    */
    int     v14;
    int     key, i, v5;

    if (mode == 6) {
        v15 = sel;
        v3  = 0;
    }

    v14 = dword_51A83;
    if (v14 > 1)
        --v14;

    if (v3 != 0) {
        v5 = list[0];
        if (((const uint8_t *)dword_53A45)[80 * v5 + 7] != 0x79)
            dlg_portrait_glide(v5);
    }

    for (;;) {
        key = map_wait_key();
        if (key == 1)
            return -1;

        if (key == 57 || key == 28) {
            if (mode == 6) {
                int dup = 0;

                for (i = 0; i < dword_53BEB; ++i) {
                    const uint8_t *r = (const uint8_t *)dword_53A45 + 80u * (uint32_t)i;

                    if (i != v15 && (int)r[0] == (int)qword_53AB1 &&
                        (int)r[1] == (int)dword_53AB5 && rec_flag(i) == 0)
                        dup = 1;
                }
                if (!dup) {
                    const uint8_t *rec =
                        (const uint8_t *)dword_53A45 + 80u * (uint32_t)v15;
                    const uint8_t *bank;
                    uint8_t        cell[8];
                    int            v7 = rec[32];

                    if (rec[7] == 28)
                        v7 = 1;
                    if (rec_skip(v15))
                        v7 = 19;

                    bank = (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x1646],
                                                    20, v7, 0);
                    map_cell_info((int)qword_53AB1, (int)dword_53AB5, cell);
                    if (bank[cell[5]] != 20)
                        return 1;
                }
            } else if (mode != 5) {
                const uint8_t *m = (const uint8_t *)dword_53A51;

                if (m[4 * (int)qword_53AB1 +
                      4 * (int)dword_53AC1 * (int)dword_53AB5 + 7] != 255) {
                    if (mode == 4)
                        return 1;
                    if (map_find_records((int)qword_53AB1, (int)dword_53AB5,
                                         v14, NULL, mode) != 0)
                        return 1;
                }
            }
            continue;
        }

        if ((key == 44 || key == 76) && v3 != 0) {
            if (++v4 == v3)
                v4 ^= v3;
            v5 = ((const uint8_t *)(uintptr_t)list)[v4];
            if (((const uint8_t *)dword_53A45)[80 * v5 + 7] != 0x79)
                dlg_portrait_glide(v5);
            continue;
        }

        if (key == 72) {
            ev4_11B48();
        } else if (key == 80) {
            ev4_11B9B();
        } else if (key == 75) {
            ev4_11C59();
        } else if (key == 77) {
            ev4_11BFA();
        } else {
            continue;
        }
        svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
    }
}
