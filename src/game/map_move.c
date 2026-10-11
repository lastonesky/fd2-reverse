/* map_move.c - walk an actor toward a map cell along a computed path.
 * See map_move.h.  Direct translations of the machine code at 0x14B78,
 * 0x14121 and 0x13E9C.  Uses the already-verified assets: path_mark (0x4E390),
 * path_find (0x4E4F6), ev5_14B16, util_fix_records (0x4DF4C), unit_mark_nearby
 * (0x145CD), map_exclude_record_cells (0x146D1), map_scroll_path (0x13488),
 * dlg_portrait_glide (0x12D7B), rec_skip (0x1F183), the record table
 * dword_53A45 and the tbl_ptr accessor for the 0x61646 movement-cost table.
 */
#include "fd2_game.h"
#include "map_move.h"

#include <stdint.h>
#include <stdlib.h>

/* movement-cost table accessor 0x4E8A5: base 0x61646, stride 20 (dseg03). */
static int cost_row_for(int profile)
{
    return (int)(uintptr_t)tbl_ptr((void *)&fd2_data2[0x1646], 20, profile, 0);
}

/* 0x14B78 */
int map_move_toward(int target_x, int target_y, int actor, int mode)
{
    const uint8_t *base = (const uint8_t *)dword_53A45;
    const uint8_t *rec = base + REC_STRIDE * (uint32_t)actor;
    uint8_t *map = (uint8_t *)dword_53A51;
    int profile = rec[32];
    int rec_x = rec[0];
    int rec_y = rec[1];
    int cost_row;
    void *dirs;
    uint8_t *reach;
    int cur_x = target_x;
    int cur_y = target_y;
    int cand_x = target_x;
    int cand_y = target_y;
    int n;

    if (rec_skip(actor))
        profile = 19;
    if (rec[8] == 0x1c)
        profile = 1;
    cost_row = cost_row_for(profile);
    dirs  = guest_malloc(0x20u);
    reach = (uint8_t *)guest_malloc(0x800u);
    unit_mark_nearby(mode);

    if (path_find(cost_row, rec_x, rec_y, profile, dirs, target_x, target_y, 0,
                  (void *)map, (const void *)(uintptr_t)dword_53A69) == 255) {
        util_fix_records(map);
        n = path_find(cost_row, rec_x, rec_y, 28, dirs, target_x, target_y, 1,
                      (void *)map, (const void *)(uintptr_t)dword_53A69);
        if (n != 255) {
            int wx = rec_x;
            int wy = rec_y;
            int i;

            cand_x = cur_x;
            cand_y = cur_y;
            util_fix_records(map);
            unit_mark_nearby(mode);
            path_mark(cost_row, rec_x, rec_y, profile, (void *)map,
                      (const void *)(uintptr_t)dword_53A69);
            for (i = 0; i < n; ++i) {
                int d = ((const uint8_t *)dirs)[i];

                if (d != 0) {
                    if (d == 1)
                        --wx;
                    else if (d == 2)
                        --wy;
                    else
                        ++wx;
                } else {
                    ++wy;
                }
                if (map[4u * (uint32_t)wx + 4u * (uint32_t)wy * dword_53AC1 + 7u]
                    != 255) {
                    cand_x = wx;
                    cand_y = wy;
                }
            }
            cur_x = cand_x;
            cur_y = cand_y;
        }
    }

    util_fix_records(map);
    unit_mark_nearby(mode);
    path_mark(cost_row, rec_x, rec_y, profile, (void *)map,
              (const void *)(uintptr_t)dword_53A69);
    map_exclude_record_cells(actor, mode);
    n = ev5_14B16(reach);

    cand_x = cur_x;
    cand_y = cur_y;
    {
        int best_man  = 255;
        int best_diff = 255;
        int j;

        for (j = 0; j < n; ++j) {
            int cx = reach[2 * j];
            int cy = reach[2 * j + 1];
            int dx = abs(cx - cur_x);
            int dy = abs(cy - cur_y);
            int man  = dx + dy;
            int diff = abs(dx - dy);

            if (man < best_man || (man == best_man && diff < best_diff)) {
                cand_x = cx;
                cand_y = cy;
                best_man  = man;
                best_diff = diff;
            }
        }
    }

    util_fix_records(map);
    unit_mark_nearby(mode);
    n = path_find(cost_row, rec_x, rec_y, profile, dirs, cand_x, cand_y, 0,
                  (void *)map, (const void *)(uintptr_t)dword_53A69);
    util_fix_records(map);
    if (n != 0)
        map_scroll_path(actor, (const uint8_t *)dirs, n);
    guest_free(dirs);
    guest_free(reach);
    return n != 0;
}

/* 0x14121 */
int map_move_to_marked(int actor, int mode)
{
    const uint8_t *base = (const uint8_t *)dword_53A45;
    const uint8_t *rec = base + REC_STRIDE * (uint32_t)actor;
    uint8_t *map = (uint8_t *)dword_53A51;
    int rec_x = rec[0];
    int rec_y = rec[1];
    int profile = rec[32];
    int cost_row;
    uint8_t cell[2];
    int moved = 0;

    if (rec_skip(actor))
        profile = 19;
    if (rec[8] == 0x1c)
        profile = 1;
    cost_row = cost_row_for(profile);
    unit_mark_nearby(mode);
    if (path_find(cost_row, rec_x, rec_y, 28, cell, 0, 0, 2,
                  (void *)map, (const void *)(uintptr_t)dword_53A69) == 255) {
        util_fix_records(map);
        return 0;
    }
    util_fix_records(map);
    if ((int)cell[0] == rec_x && (int)cell[1] == rec_y)
        return 0;
    dword_51A83 = 0;
    dlg_portrait_glide(actor);
    if (map_move_toward((int)cell[0], (int)cell[1], actor, mode) != 0)
        moved = 1;
    dword_51A83 = 1;
    return moved;
}

/* 0x13E9C */
int map_move_to_nearest(int actor, int mode)
{
    const uint8_t *base = (const uint8_t *)dword_53A45;
    const uint8_t *rec = base + REC_STRIDE * (uint32_t)actor;
    int rec_x = rec[0];
    int rec_y = rec[1];
    int best_x = -1;
    int best_y = 0;
    int best = 0xffff;
    int moved = 0;
    int i;

    for (i = 0; i < dword_53BEB; ++i) {
        const uint8_t *r = base + REC_STRIDE * (uint32_t)i;
        int d;
        int rx;
        int ry;

        if (!((mode == 0 && r[6] != 0) || (mode != 0 && r[6] == 0)))
            continue;
        rx = r[0];
        ry = r[1];
        d = abs(rec_x - rx) + abs(rec_y - ry);
        if (d < best) {
            best   = d;
            best_x = rx;
            best_y = ry;
        }
    }
    if (best_x == -1)
        return 0;
    if (best_x == rec_x && best_y == rec_y)
        return 0;
    dword_51A83 = 0;
    dlg_portrait_glide(actor);
    if (map_move_toward(best_x, best_y, actor, mode) != 0)
        moved = 1;
    dword_51A83 = 1;
    return moved;
}
