/* map_ai.c - the unit-AI target/action scorer.  See map_ai.h.
 *
 * Direct translations of the machine code at the stated addresses.  Uses the
 * already-verified assets: map_reveal_reachable (0x14818), map_scan_records
 * (0x149F8), ev5_14B16, util_fix_records (0x4DF4C), rec_slot_free (0x1B8A6),
 * rec_skip (0x1F183), rec_slot_find (0x1B83D), rec_field_byte (0x1B722),
 * rec_collect_slot_bits (0x1C269), path_mark (0x4E390), unit_mark_nearby
 * (0x145CD), map_cell_info (0x12E38), the record table dword_53A45 and the
 * tbl_ptr accessors for 0x602AD / 0x619FD / 0x61646 / 0x615FE.
 */
#include "fd2_game.h"
#include "map_ai.h"

#include <stdint.h>
#include <stdlib.h>

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

/* 0x146D1 - set the map cell byte (map + 7 + 4*(y*W + x)) of every record whose
 * status byte +6 matches `mode` (mode 0: +6 == 0, else +6 != 0) to 0xFF.  The
 * collector ev5_14B16 (0x14B16) skips every 0xFF cell, so this removes the
 * cells of the matching records from the reachable-cell scan.  `actor` itself
 * is skipped, as is any record whose +5 bit 0 is set. */
void map_exclude_record_cells(int actor, int mode)
{
    const uint8_t *r = (const uint8_t *)dword_53A45;
    int i;

    for (i = 0; i < dword_53BEB; ++i) {
        if (i != actor && (r[5] & 1) == 0) {
            if ((mode == 0 && r[6] == 0) || (mode != 0 && r[6] != 0)) {
                uint32_t cell = 7u + 4u * ((uint32_t)r[0]
                                           + (uint32_t)r[1] * dword_53AC1);
                dword_53A51[cell] = 0xFFu;
            }
        }
        r += REC_STRIDE;
    }
}

/* 0x1DEBE - 1 when record `target` is orthogonally adjacent to cell (x,y)
 * (|x - target[0]| + |y - target[1]| == 1), its state byte +38 is zero and the
 * item in its first claimed slot has effect byte +11 <= 1; -1 otherwise. */
int map_ai_target_adjacent(int target, int x, int y)
{
    const uint8_t *r =
        (const uint8_t *)dword_53A45 + REC_STRIDE * (uint32_t)target;
    int slot;

    if (r[38] == 0 && abs(x - r[0]) + abs(y - r[1]) == 1) {
        slot = rec_slot_find(target, 0);
        if (slot == -1)
            return -1;
        if (item_entry(rec_field_byte(target, slot))[11] <= 1u)
            return 1;
    }
    return -1;
}

/* 0x14237 - the third AI target picker, sibling of map_ai_pick_action_target /
 * map_ai_pick_item_target.  For the item in the first claimed slot of `actor`
 * it flood-fills the movement-reachable region (path_mark), excludes the cells
 * of the matching records, then for every remaining reachable cell scores each
 * record the item's range (item[12]) / radius (item[11]) can hit and keeps the
 * best target in dword_53C43/47 (cell), dword_53C4B (record index) and
 * dword_53C4F (score).  `mode` picks the exclusion/filter (0/1).  Always 0. */
int map_ai_pick_reach_target(int actor, int mode)
{
    const uint8_t *base = (const uint8_t *)dword_53A45;
    const uint8_t *r = base + REC_STRIDE * (uint32_t)actor;
    const uint8_t *it;
    uint8_t cell[8];
    void *cost_row, *v21, *v26, *p;
    int v27, v28, v7, v20, v6, v22, v23, v25, v24, v30, v33, v32, v34;
    int slot, i, j;

    dword_53C4F = 0;
    v34 = 0;
    v25 = 0;
    v27 = *(const uint16_t *)(const void *)(r + 72);
    v28 = *(const uint16_t *)(const void *)(r + 74);
    slot = rec_slot_find(actor, 0);
    if (slot == -1)
        return 0;

    it  = item_entry(rec_field_byte(actor, slot));
    v22 = it[11];
    v23 = it[12];
    v6  = r[59];
    v7  = r[0];
    v20 = r[1];
    cost_row = tbl_ptr((void *)&fd2_data2[0x1646], 20,
                       rec_skip(actor) ? 19 : r[32], 0);   /* 0x4E8A5 */
    v21 = guest_malloc(0x20u);
    v26 = guest_malloc(0x800u);
    if (mode == 0)
        v25 = 1;
    unit_mark_nearby(mode);
    path_mark((int)(uintptr_t)cost_row, v7, v20, v6, (void *)dword_53A51,
              (const void *)(uintptr_t)dword_53A69);
    map_exclude_record_cells(actor, mode);
    v24 = ev5_14B16((uint8_t *)v26);
    util_fix_records(dword_53A51);
    p = guest_malloc(0x64u);

    for (i = 0; i < v24; ++i) {
        const uint8_t *cp = (const uint8_t *)v26 + 2u * (uint32_t)i;
        int v35 = cp[0];
        int v36 = cp[1];

        v33 = v27;
        v32 = v28;
        if (rec_skip(actor)) {
            map_cell_info(v35, v36, cell);
            v33 = v27 * dword_51A12[cell[5]] / 100 + v27;
            v32 = v28 * dword_51A2A[cell[5]] / 100 + v28;
        }
        v30 = map_reveal_reachable(v35, v36, (uint8_t *)p, v23, v22, v25);
        util_fix_records(dword_53A51);

        for (j = 0; j < v30; ++j) {
            int v38 = ((const uint8_t *)p)[j];
            const uint8_t *tgt = base + REC_STRIDE * (uint32_t)v38;
            int v15 = *(const uint16_t *)(const void *)(tgt + 72);
            int v16 = *(const uint16_t *)(const void *)(tgt + 74);
            int v13, v12;

            if (rec_skip(v38)) {
                map_cell_info(tgt[0], tgt[1], cell);
                v15 += v15 * dword_51A12[cell[5]] / 100;
                v16 += v16 * dword_51A2A[cell[5]] / 100;
            }
            v13 = v33 - v16;
            v12 = (v33 - v16 <= 2) ? 0 : 8;
            if (v13 > *(const uint16_t *)(const void *)(tgt + 64)) {
                v13 *= 2;
                v12 = 18;
            }
            if (map_ai_target_adjacent(v38, v35, v36) == 1)
                v13 += v32 - v15;
            if (tgt[8] == 0)
                v13 = 3 * v13 / 2;
            if (v12 > (int)dword_53C4F
                || (v12 == (int)dword_53C4F && v13 > v34)) {
                v34 = v13;
                dword_53C43 = (uint32_t)v35;
                dword_53C47 = (uint32_t)v36;
                dword_53C4B = (uint32_t)v38;
                dword_53C4F = (uint32_t)v12;
            }
        }
    }

    guest_free(p);
    guest_free(v21);
    guest_free(v26);
    return 0;
}

/* 0x15055 - the AI item-use executor, chosen by the decision driver 0x14EF0
 * when the item target scores highest.  Reads the actor's chosen slot
 * dword_53C3F through rec_field_byte and the effect entry through the 0x4E8BC
 * accessor.  The effect byte +16 selects the reach: <= 0x0F scans the effect
 * range (item[18]) / radius 0 (map_reveal_reachable), >= 0x10 walks the actor
 * cell with map_scan_records and then plays the full-screen figure_summon
 * sequence and interpolates a new view centre from qword_53AB1.  Byte +17 is
 * the empty filter (inverted when `require_empty` is 0) and byte +18 the
 * effect band (also the dword_51A83 fade selector).  The collected targets go
 * to map_action_router, then the portrait is cleared.  Always 0. */
int map_ai_use_item(int actor, int require_empty)
{
    const uint8_t *rec =
        (const uint8_t *)dword_53A45 + REC_STRIDE * (uint32_t)actor;
    const uint8_t *eff;
    uint8_t list[32];
    int filter, kind, count, i;

    dword_53C57 = (uint32_t)rec_field_byte(actor, (int)dword_53C3F);
    eff = item_entry((int)dword_53C57);

    if (require_empty != 0)
        filter = eff[17];
    else
        filter = (eff[17] == 0);

    dlg_portrait_glide(actor);

    kind = eff[16];
    if (kind <= 0x0F)
        count = map_reveal_reachable((int)dword_53C37, (int)dword_53C3B,
                                     list, eff[18], 0, filter);
    else
        count = map_scan_records((int)dword_53C37, (int)dword_53C3B, list,
                                 rec[0], rec[1], kind - 16, 0);

    util_fix_records(dword_53A51);
    j___delay(200);
    dword_51A83 = eff[18] + 2;

    if (kind >= 0x10) {
        int32_t cx, cy, j;

        ev5_1F04A(actor, list[0]);
        map_view_update(1);
        svc_wait_ticks(1);
        svc_wait_ticks(2);
        figure_scene_play(actor);
        portrait_reveal_cell(actor, 80, -4);
        j___delay(200);
        for (i = 64; i >= 0; --i) {
            pal_fade_add(0, 255, i);
            j___delay(4);
        }
        dword_51A83 = 6;
        kind -= 16;
        cx = (int32_t)(dword_53C37 - (uint32_t)qword_53AB1) * kind
             + (int32_t)(uint32_t)qword_53AB1;
        if (cx < (int32_t)dword_53AC1) {
            if (cx < 0)
                cx = 0;
        } else {
            cx = (int32_t)(dword_53AC1 - 1u);
        }
        dword_53C37 = (uint32_t)cx;
        cy = (int32_t)(dword_53C3B - dword_53AB5) * kind
             + (int32_t)dword_53AB5;
        if (cy < (int32_t)dword_53AC5) {
            if (cy < 0)
                cy = 0;
        } else {
            cy = (int32_t)(dword_53AC5 - 1u);
        }
        dword_53C3B = (uint32_t)cy;
        dword_53C1F = 0;
        map_slide_view((int)dword_53C37, (int)dword_53C3B);
        dword_51A83 = 0;
        for (j = 1; j < 9; ++j) {
            dword_53C1F = j;
            map_view_update(1);
            svc_wait_ticks(1);
        }
        util_fix_records(dword_53A51);
        svc_wait_ticks(2);
        dlg_portrait_glide(list[0]);
    } else {
        map_slide_view((int)dword_53C37, (int)dword_53C3B);
    }

    map_action_router(actor, (int)dword_53C3F, count, list);
    dlg_portrait_clear();
    dword_53EC8 = 0;
    return 0;
}

/* 0x51D01 - the funcs_1541F dispatch table (28 entries).  The machine calls
 * `[eax*4 + 0x51D01]` with the action id; slot 23 is 0x2218A
 * (map_units_move_deploy, void), wrapped here so the table stays uniform. */
typedef int (*map_action_fn)(int, int, const uint8_t *);

static int map_action_entry23(int a1, int count, const uint8_t *list)
{
    map_units_move_deploy(a1, count, list);
    return 0;
}

static map_action_fn const action_table[28] = {
    map_action_entry00, map_action_entry01, map_action_entry02,
    map_action_entry03, map_action_entry04, map_action_entry05,
    map_action_entry06, map_action_entry07, map_action_entry08,
    map_action_entry09, map_action_entry10, map_action_entry11,
    map_action_entry12, map_action_entry13, map_action_entry14,
    map_action_entry15, map_action_entry16, map_action_grow_x_cost,
    map_action_grow_y_cost, map_action_widen_cost, map_action_entry20,
    map_action_entry21, map_action_entry22, map_action_entry23,
    map_action_entry16, map_action_clear_flag, map_action_entry26,
    map_action_entry27,
};

/* 0x15311 - execute the action map_ai_decide chose (see map_ai.h). */
int map_ai_do_action(int actor, int require_empty)
{
    const uint8_t *entry = action_entry((int)dword_53C2F);
    uint8_t list[32];   /* 0x14818/0x149F8 target list */
    uint8_t hit[12];    /* 4 x 3-byte +49 script entries for ev_script_run */
    int filter, count, steps;

    if (require_empty != 0)
        filter = entry[6];
    else
        filter = (entry[6] == 0);

    if ((int)dword_53C23 < 6)
        return 0;

    dlg_portrait_glide(actor);
    count = map_reveal_reachable((int)dword_53C27, (int)dword_53C2B, list,
                                 entry[4], 0, filter);
    util_fix_records(dword_53A51);
    j___delay(200);
    dword_51A83 = entry[4] + 2;
    map_slide_view((int)dword_53C27, (int)dword_53C2B);
    dword_51A83 = 0;
    map_view_update(0);

    if ((int)dword_53C2F >= 10 || byte_53AF9 != 0) {
        ev5_1D4CB();
        action_table[dword_53C2F](actor, count, list);
        menu_stop_and_free_music();
    } else {
        figure_effect_play(actor, (int)dword_53C2F, count, list);
    }

    steps = ev5_1B653(hit);
    map_view_update(0);
    map_units_animate();
    ev_script_run((int)dword_53C4B, steps, (int)(intptr_t)hit);
    map_view_update(0);
    dword_53EC8 = 0;
    dword_51A83 = 0;
    return 1;
}

/* 0x14EF0 - the AI decision driver (see map_ai.h). */
int map_ai_decide(int actor, int require_empty)
{
    const uint8_t *base;
    const uint8_t *rec;
    int flag, hp_delta;

    map_ai_pick_reach_target(actor, require_empty);
    map_ai_pick_action_target(actor, require_empty);
    map_ai_pick_item_target(actor, require_empty);

    /* read after the scorers, as the machine does (ds:0x53a45 reloaded) */
    base = (const uint8_t *)dword_53A45;
    rec  = base + REC_STRIDE * (uint32_t)actor;
    flag = rec[52] & 0x40;
    hp_delta = *(const uint16_t *)(const void *)(rec + 72)
             - *(const uint16_t *)(const void *)
                 (base + REC_STRIDE * (uint32_t)dword_53C4B + 74);

    if ((int)dword_53C4F < 6 && (int)dword_53C23 < 6 && (int)dword_53C33 < 6)
        return 0;

    if ((int)dword_53C4F > (int)dword_53C23 &&
        (int)dword_53C4F > (int)dword_53C33)
        goto attack;

    if ((int)dword_53C4F == (int)dword_53C23 &&
        (int)dword_53C4F > (int)dword_53C33) {
        if ((int)dword_53C2F >= 11) {
            if (flag == 0)
                goto action;
        } else if (*(const uint16_t *)(const void *)action_entry((int)dword_53C2F)
                   >= hp_delta) {
            goto action;
        }
        goto attack;
    }

    if ((int)dword_53C4F == (int)dword_53C33 &&
        (int)dword_53C4F > (int)dword_53C23) {
        if (flag != 0)
            goto attack;
        goto item;
    }

    if ((int)dword_53C23 > (int)dword_53C4F &&
        (int)dword_53C23 >= (int)dword_53C33)
        goto action;
    if ((int)dword_53C33 <= (int)dword_53C4F ||
        (int)dword_53C33 <= (int)dword_53C23)
        goto tail;

item:
    map_ai_use_item(actor, require_empty);
    goto tail;
action:
    map_ai_do_action(actor, require_empty);
    goto tail;
attack:
    map_attack_run(actor, require_empty);
tail:
    dword_51A83 = 0;
    return 1;
}
