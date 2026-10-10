/* map_ai.c - the unit-AI target/action scorer.  See map_ai.h.
 *
 * Direct translations of the machine code at the stated addresses.  Uses the
 * already-verified assets: map_reveal_reachable (0x14818), map_scan_records
 * (0x149F8), ev5_14B16, util_fix_records (0x4DF4C), rec_slot_free (0x1B8A6),
 * rec_skip (0x1F183), rec_collect_slot_bits (0x1C269), the record table
 * dword_53A45 and the tbl_ptr accessors for 0x602AD / 0x619FD / 0x61646.
 */
#include "fd2_game.h"
#include "map_ai.h"

#include <stdint.h>

/* item table accessor 0x4E8BC: base 0x602AD, stride 23 (dseg03). */
static const uint8_t *item_entry(int index)
{
    return (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x2AD], 23, index, 0);
}

/* action table accessor 0x4E866: base 0x619FD, stride 7 (dseg03). */
static const uint8_t *action_entry(int index)
{
    return (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x19FD], 7, index, 0);
}

/* 0x15880 */
int map_ai_item_score(int item, int count, const uint8_t *list)
{
    const uint8_t *base = (const uint8_t *)dword_53A45;
    const uint8_t *eff = item_entry(item);
    int kind = eff[13];
    int sum = 0;
    int i;

    if (kind == 5 || kind == 13) {
        for (i = 0; i < count; ++i) {
            const uint8_t *r = base + REC_STRIDE * (uint32_t)list[i];
            int hp  = *(const uint16_t *)(const void *)(r + 64);
            int max = *(const uint16_t *)(const void *)(r + 66);
            int s;

            if (hp > max / 3)
                s = (hp > max / 2) ? 0 : 3;
            else
                s = 8;
            if ((int8_t)r[52] < 0)
                s *= 3;
            sum += s;
        }
    } else if (kind == 20 || kind == 21 || kind == 24) {
        int w = *(const uint16_t *)(const void *)(eff + 14);
        const uint8_t *act = action_entry(w);
        if (kind != 24)
            w = *(const uint16_t *)(const void *)act;
        for (i = 0; i < count; ++i) {
            const uint8_t *r = base + REC_STRIDE * (uint32_t)list[i];
            sum += (w >= *(const uint16_t *)(const void *)(r + 64)) ? 18 : 8;
        }
    }
    return sum;
}

/* 0x15DA2 */
int map_ai_flag_sum(int count, const uint8_t *list, int field, int weight)
{
    const uint8_t *base = (const uint8_t *)dword_53A45;
    int sum = 0;
    int i;

    for (i = 0; i < count; ++i)
        if (base[REC_STRIDE * list[i] + field] == 0)
            sum += weight;
    return sum;
}

/* 0x15B77 */
int map_ai_action_score(int action, int count, const uint8_t *list)
{
    const uint8_t *base = (const uint8_t *)dword_53A45;
    const uint8_t *eff = action_entry(action);
    int limit = *(const uint16_t *)(const void *)eff;
    int sum = 0;
    int i;

    if (action < 13) {
        for (i = 0; i < count; ++i) {
            const uint8_t *r;
            int s;

            if (action >= 10 && rec_skip(list[i]))
                continue;
            r = base + REC_STRIDE * (uint32_t)list[i];
            s = (*(const uint16_t *)(const void *)(r + 64) >= limit) ? 8 : 24;
            if (r[8] == 0)
                s = (int)((double)s * dbl_50144);
            sum += s;
        }
        return sum;
    }
    if (action < 17) {
        for (i = 0; i < count; ++i) {
            const uint8_t *r = base + REC_STRIDE * (uint32_t)list[i];
            int hp = *(const uint16_t *)(const void *)(r + 64);
            int mx = *(const uint16_t *)(const void *)(r + 66);
            int s;

            if (mx / 3 <= hp)
                s = (mx / 2 <= hp) ? 0 : 3;
            else
                s = 8;
            if ((r[52] & 1u) != 0)
                s *= 2;
            sum += s;
        }
        return sum;
    }
    if (action < 20)
        return map_ai_flag_sum(count, list, action + 17, 3);

    switch (action) {
    case 20:
        for (i = 0; i < count; ++i) {
            const uint8_t *r = base + REC_STRIDE * (uint32_t)list[i];
            if (r[37] != 0)
                sum += 6;
        }
        return sum;
    case 21:
        for (i = 0; i < count; ++i) {
            const uint8_t *r = base + REC_STRIDE * (uint32_t)list[i];
            if (r[38] != 0)
                sum += 6;
        }
        return sum;
    case 22:
        for (i = 0; i < count; ++i) {
            const uint8_t *r = base + REC_STRIDE * (uint32_t)list[i];
            if (r[39] == 0 && rec_collect_slot_bits(list[i], NULL) != 0)
                sum += 6;
        }
        return sum;
    case 26:
        return map_ai_flag_sum(count, list, 37, 4);
    case 27:
        return map_ai_flag_sum(count, list, 38, 4);
    default:
        return sum;
    }
}

/* 0x1567E */
int map_ai_pick_item_target(int actor, int require_empty)
{
    const uint8_t *base = (const uint8_t *)dword_53A45;
    const uint8_t *r = base + REC_STRIDE * (uint32_t)actor;
    uint8_t cells[32];
    void *p;
    int v17, v18, slots, i, j;

    dword_53C33 = 0;
    (void)tbl_ptr((void *)&fd2_data2[0x1646], 20, 0, 0);   /* 0x4E8A5(0) */
    v17 = r[0];
    v18 = r[1];
    p = guest_malloc(0x190u);
    slots = rec_slot_free(actor);
    if (slots == 0)
        return 0;

    for (i = 0; i < slots; ++i) {
        int item = r[2 * i + 11];
        const uint8_t *eff = item_entry(item);
        int band, rad, n;

        if (eff[16] > 0x0F) {
            band = 1;
            rad  = 1;
        } else {
            band = eff[16];
            rad  = 0;
        }
        if (eff[13] == 0)
            continue;

        map_reveal_reachable(v17, v18, NULL, band, rad, 0);
        n = ev5_14B16((uint8_t *)p);
        util_fix_records(dword_53A51);

        for (j = 0; j < n; ++j) {
            const uint8_t *cell = (const uint8_t *)p + 2u * (uint32_t)j;
            int cx = cell[0];
            int cy = cell[1];
            int v7, targets, score;

            if (require_empty != 0)
                v7 = eff[17];
            else
                v7 = (eff[17] == 0);

            if (eff[16] <= 0x0F)
                targets = map_reveal_reachable(cx, cy, cells, eff[18], 0, v7);
            else
                targets = map_scan_records(cx, cy, cells, v17, v18,
                                           eff[16] - 16, 0);

            util_fix_records(dword_53A51);
            if (targets == 0)
                continue;

            score = map_ai_item_score(item, targets, cells);
            if (score > (int)dword_53C33) {
                dword_53C33 = (uint32_t)score;
                dword_53C37 = (uint32_t)cx;
                dword_53C3B = (uint32_t)cy;
                dword_53C3F = (uint32_t)i;
            }
        }
    }
    guest_free(p);
    return 0;
}

/* 0x1598A */
int map_ai_pick_action_target(int actor, int require_empty)
{
    const uint8_t *base = (const uint8_t *)dword_53A45;
    const uint8_t *r = base + REC_STRIDE * (uint32_t)actor;
    uint8_t items[12];
    uint8_t cells[32];
    void *cost_row;
    void *p;
    int x, y, limit, n_items, i, j;
    int best = 0;   /* machine reads an uninitialised stack slot in the tie-break */

    dword_53C23 = 0;
    cost_row = tbl_ptr((void *)&fd2_data2[0x1646], 20, 0, 0);   /* 0x4E8A5(0) */
    x = r[0];
    y = r[1];
    p = guest_malloc(0x190u);
    limit = *(const uint16_t *)(const void *)(r + 68);
    n_items = rec_collect_slot_bits(actor, items);
    if (n_items == 0 || r[39] != 0)
        return 0;   /* original leaks p here; kept for fidelity */

    for (i = 0; i < n_items; ++i) {
        const uint8_t *act = action_entry(items[i]);
        int n;

        if (act[5] > limit)
            continue;
        path_mark((int)(uintptr_t)cost_row, x, y, act[3], (void *)dword_53A51,
                  (const void *)(uintptr_t)dword_53A69);
        n = ev5_14B16((uint8_t *)p);
        util_fix_records(dword_53A51);

        for (j = 0; j < n; ++j) {
            const uint8_t *cell = (const uint8_t *)p + 2u * (uint32_t)j;
            int cx = cell[0];
            int cy = cell[1];
            int filter, cnt, score;

            if (require_empty != 0)
                filter = act[6];
            else
                filter = (act[6] == 0);
            cnt = map_reveal_reachable(cx, cy, cells, act[4], 0, filter);
            util_fix_records(dword_53A51);
            if (cnt == 0)
                continue;

            score = map_ai_action_score(items[i], cnt, cells);
            if (score > (int)dword_53C23 ||
                (score == (int)dword_53C23 &&
                 *(const uint16_t *)(const void *)act > best)) {
                dword_53C23 = (uint32_t)score;
                dword_53C27 = (uint32_t)cx;
                dword_53C2B = (uint32_t)cy;
                dword_53C2F = (uint32_t)items[i];
                best = *(const uint16_t *)(const void *)act;
            }
        }
    }
    guest_free(p);
    return 0;
}
