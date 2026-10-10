/* map_action.c - map action-effect handlers.  See map_action.h.
 *
 * Direct translations of the machine code at the stated addresses.  The offline
 * disassembly is authoritative for the argument order, the constants and the
 * control flow.  All callees are already-restored assets:
 *   map_draw_party_icons(_anim) (0x1C2DA/0x1C4CC), map_draw_status_popups
 *   (0x1DF58), map_enqueue_status (0x1E0DB), map_view_update (0x11CAC),
 *   ev4_1E1DC (0x1E1DC), rec_sub_table5 (0x1CA89), rec_slot_remove (0x1B8E7),
 *   unit_metrics (0x1B750) and the RNG util_rand (0x4EBE3).
 *
 * The Watcom stack probe (`push size; call 0x3702F`) is dropped.  The two
 * shared epilogues (`jmp 0x1317D`, `jmp 0x22BBE`) and the Hex-Rays `_CHP()`
 * float-to-int helper (call 0x37AF4) collapse into a plain return and a C
 * `(int)` cast, matching unit_metrics' 1.15 scaling and unit_heal's `% 100`.
 * Hex-Rays lists sub_1300D as a callee of 0x22721 and sub_22AF6 as a callee of
 * 0x22997; both are false (jumps into a neighbour's shared epilogue).
 */
#include "fd2_game.h"
#include "map_action.h"

#define REC_STRIDE 80

/* 0x21082 - add `value` to a record field, show it and drop a cost slot. */
int map_action_add_field(int a1, int value, int field, int slot, int count,
                         const uint8_t *list, int table_idx)
{
    uint8_t *rec;

    map_draw_party_icons_anim(a1, table_idx, count, list);
    map_draw_party_icons(a1, table_idx, count, list);

    rec = (uint8_t *)dword_53A45 + REC_STRIDE * list[0] + field;
    *(uint16_t *)(void *)rec += (uint16_t)value;

    map_enqueue_status(value, 94, list[0]);
    map_view_update(0);
    map_draw_status_popups();
    unit_metrics(a1);
    return (int)(intptr_t)rec_slot_remove(a1, slot);
}

/* 0x22721 - flag +34 and grow the +72 metric by (word * dbl_50210 + 1). */
int map_action_grow_x(int a1, int count, const uint8_t *list)
{
    int i;

    map_draw_party_icons_anim(a1, 17, count, list);
    map_draw_party_icons(a1, 17, count, list);

    for (i = 0; i < count; ++i) {
        uint8_t *rec = (uint8_t *)dword_53A45 + REC_STRIDE * list[i];
        int      v9  = rec[33];

        if (rec[32] > 8 && rec[32] < 0x19)
            v9 += 30;

        if (rec[34] != 0) {
            ev4_1E1DC(list[i]);
        } else {
            int v7;

            rec[34] = (uint8_t)((int32_t)util_rand() % 4 + 2);
            v7 = (int)((double)*(const uint16_t *)(const void *)(rec + 72)
                       * dbl_50210 + 1.0);
            map_enqueue_status(v7, 105, list[i]);
            *(uint16_t *)(void *)(rec + 72) += (uint16_t)v7;
            dword_53EC8 += 2u * (uint32_t)v9;
        }
    }

    map_view_update(0);
    map_draw_status_popups();
    return 0;
}

/* 0x22997 - flag +36 (widen) and grow the +76/+78 metrics by 15. */
int map_action_widen(int a1, int count, const uint8_t *list)
{
    int i;

    map_draw_party_icons_anim(a1, 19, count, list);
    map_draw_party_icons(a1, 19, count, list);

    for (i = 0; i < count; ++i) {
        uint8_t *rec = (uint8_t *)dword_53A45 + REC_STRIDE * list[i];
        int      v8  = rec[33];

        if (rec[32] > 8 && rec[32] < 0x19)
            v8 += 30;

        if (rec[36] != 0) {
            ev4_1E1DC(list[i]);
        } else {
            rec[36] = (uint8_t)((int32_t)util_rand() % 4 + 2);
            *(uint16_t *)(void *)(rec + 76) += 15;
            *(uint16_t *)(void *)(rec + 78) += 15;
            map_enqueue_status(15, 105, list[i]);
            dword_53EC8 += 2u * (uint32_t)v8;
        }
    }

    map_view_update(0);
    map_draw_status_popups();
    return 0;
}

/* 0x22C04 - clear bit 7 of record flag +5 (rec_flag_or80) and book exp. */
int map_action_clear_flag(int a1, int count, const uint8_t *list)
{
    int i;

    dword_53EC4 = 0;
    rec_sub_table5(a1, 25);
    map_draw_party_icons_anim(a1, 25, count, list);
    map_draw_party_icons(a1, 25, count, list);

    for (i = 0; i < count; ++i) {
        uint8_t *rec = (uint8_t *)dword_53A45 + REC_STRIDE * list[i];

        if ((rec[5] & 0x80) == 0) {
            ev4_1E1DC(list[i]);
        } else {
            int v5, v6;

            rec[5] &= (uint8_t)~0x80u;
            v5 = rec[33];
            v6 = rec[32];
            if (v6 > 8 && v6 < 25)
                v5 += 30;
            dword_53EC8 += 8u * (uint32_t)v5;
        }
    }

    map_view_update(0);
    if (dword_53EC4 != 0)
        map_draw_status_popups();
    return 0;
}

/* 0x226EA - charge the 0x619FD entry-18 cost, then grow-x the list. */
int map_action_grow_x_cost(int index, int count, const uint8_t *list)
{
    dword_53EC4 = 0;
    rec_sub_table5(index, 18);
    return map_action_grow_x(index, count, list);
}

/* 0x22960 - charge the 0x619FD entry-19 cost, then widen the list. */
int map_action_widen_cost(int index, int count, const uint8_t *list)
{
    dword_53EC4 = 0;
    rec_sub_table5(index, 19);
    return map_action_widen(index, count, list);
}
