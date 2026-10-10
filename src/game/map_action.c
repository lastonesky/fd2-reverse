/* map_action.c - map action-effect handlers.  See map_action.h.
 *
 * Direct translations of the machine code at the stated addresses.  The offline
 * disassembly is authoritative for the argument order, the constants and the
 * control flow.  All callees are already-restored assets:
 *   map_draw_party_icons(_anim) (0x1C2DA/0x1C4CC), map_record_layer_blink
 *   (0x1CAC7), map_draw_status_popups (0x1DF58), map_enqueue_status (0x1E0DB),
 *   map_view_update (0x11CAC), ev4_1E1DC (0x1E1DC), rec_sub_table5 (0x1CA89),
 *   rec_slot_remove (0x1B8E7), unit_metrics (0x1B750), unit_damage (0x1C81F)
 *   and the RNG util_rand (0x4EBE3).
 *
 * The Watcom stack probe (`push size; call 0x3702F`) is dropped.  The two
 * shared epilogues (`jmp 0x1317D`, `jmp 0x22BBE`) and the Hex-Rays `_CHP()`
 * float-to-int helper (call 0x37AF4) collapse into a plain return and a C
 * `(int)` cast, matching unit_metrics' 1.15 scaling and unit_heal's `% 100`.
 * Hex-Rays lists sub_1300D as a callee of 0x22721 and sub_22AF6 as a callee of
 * 0x22997; both are false (jumps into a neighbour's shared epilogue).
 *
 * The funcs_1541F effect entries 0/1/2/3/8 (0x21206/0x2134B/0x21364/0x2137D/
 * 0x21494) all fall into the 0x2121A tail of 0x21206 and hence the shared body
 * map_action_resolve_list (0x21227); their tails land on the 0x21190 epilogue
 * inside 0x2111A, which IDA records as a false callee.  All callees here
 * (map_draw_party_icons(_anim), map_blink_units, rec_sub_table5,
 * unit_action_damage, map_enqueue_status, ev4_1E1DC, map_view_update,
 * map_draw_status_popups) are already-restored assets.
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

/* 0x22866 - flag +35 and grow the +74 metric by (word * dbl_50218 + 1). */
int map_action_grow_y(int a1, int count, const uint8_t *list)
{
    int i;

    map_draw_party_icons_anim(a1, 18, count, list);
    map_draw_party_icons(a1, 18, count, list);

    for (i = 0; i < count; ++i) {
        uint8_t *rec = (uint8_t *)dword_53A45 + REC_STRIDE * list[i];
        int      v9  = rec[33];

        if (rec[32] > 8 && rec[32] < 0x19)
            v9 += 30;

        if (rec[35] != 0) {
            ev4_1E1DC(list[i]);
        } else {
            int v7;

            rec[35] = (uint8_t)((int32_t)util_rand() % 4 + 2);
            v7 = (int)((double)*(const uint16_t *)(const void *)(rec + 74)
                       * dbl_50218 + 1.0);
            map_enqueue_status(v7, 105, list[i]);
            *(uint16_t *)(void *)(rec + 74) += (uint16_t)v7;
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

/* 0x2282F - funcs_1541F entry 18: charge the 0x619FD entry-18 cost, then
 * grow-y the list (the machine code really does push 0x12 here, same entry as
 * map_action_grow_x_cost's 0x226EA). */
int map_action_grow_y_cost(int index, int count, const uint8_t *list)
{
    dword_53EC4 = 0;
    rec_sub_table5(index, 18);
    return map_action_grow_y(index, count, list);
}

/* 0x22960 - charge the 0x619FD entry-19 cost, then widen the list. */
int map_action_widen_cost(int index, int count, const uint8_t *list)
{
    dword_53EC4 = 0;
    rec_sub_table5(index, 19);
    return map_action_widen(index, count, list);
}

/* 0x22AA8 - charge the 0x619FD entry-`index` cost, run item_apply on the
 * list with the per-record effect byte `field`, then drain the status queue.
 * This is the shared body of the funcs_1541F entries 20 and 21: 0x22A85 and
 * 0x22BC6 push their constants and `jmp 0x22A9B` (the tail inside this body). */
int map_action_charge_apply(int a1, int index, int count, const uint8_t *list,
                            int field)
{
    dword_53EC4 = 0;
    rec_sub_table5(a1, index);
    item_apply(a1, index, count, list, field);
    if (dword_53EC4 != 0)
        map_draw_status_popups();
    return 0;
}

/* 0x22A85 - funcs_1541F entry 20: charge index 20, apply effect byte 37. */
int map_action_entry20(int a1, int count, const uint8_t *list)
{
    return map_action_charge_apply(a1, 20, count, list, 37);
}

/* 0x22BC6 - funcs_1541F entry 21: charge index 21, apply effect byte 38. */
int map_action_entry21(int a1, int count, const uint8_t *list)
{
    return map_action_charge_apply(a1, 21, count, list, 38);
}

/* 0x22D1B - shared "damage the listed records" body of the funcs_1541F
 * entries 22/26/27.  It draws the party icons, then for every target whose
 * effect byte `field` is still empty (and whose +32 class is neither 25 nor 26)
 * it deals 10 damage with a 50% chance, queues the number, arms the effect
 * byte to rand 2..5 and books 8*rec[33] experience; the other half of the
 * targets just get their idle cursor sprite queued.  `table_idx` is the icon
 * shape bank handed down by map_action_charge_damage (= the cost index). */
int map_action_damage_records(int a1, int table_idx, int count,
                              const uint8_t *list, int field)
{
    int i;

    map_draw_party_icons_anim(a1, table_idx, count, list);
    map_draw_party_icons(a1, table_idx, count, list);

    for (i = 0; i < count; ++i) {
        uint8_t *rec = (uint8_t *)dword_53A45 + REC_STRIDE * list[i];
        int      dmg;

        if (rec[field] != 0 || rec[32] == 25 || rec[32] == 26) {
            ev4_1E1DC(list[i]);
            continue;
        }
        if ((int32_t)util_rand() % 100 >= 50) {
            ev4_1E1DC(list[i]);
            continue;
        }

        dmg = unit_damage(list[i], 10);
        map_enqueue_status(dmg, 94, list[i]);
        rec[field] = (uint8_t)((int32_t)util_rand() % 4 + 2);
        dword_53EC8 += 8u * (uint32_t)rec[33];
    }

    map_view_update(0);
    if (dword_53EC4 != 0)
        map_draw_status_popups();
    return 0;
}

/* 0x22CDA - shared body of the funcs_1541F entries 22/26/27: clear the
 * status-queue flag, charge cost `index` via rec_sub_table5, then run the
 * damage body using `index` as the icon shape bank.  The entries below push
 * their constants and fall into the 0x22BF7 tail of this body. */
int map_action_charge_damage(int a1, int index, int count, const uint8_t *list,
                             int field)
{
    dword_53EC4 = 0;
    rec_sub_table5(a1, index);
    return map_action_damage_records(a1, index, count, list, field);
}

/* 0x22BE1 - funcs_1541F entry 22: charge index 22, damage effect byte 39. */
int map_action_entry22(int a1, int count, const uint8_t *list)
{
    return map_action_charge_damage(a1, 22, count, list, 39);
}

/* 0x22CBF - funcs_1541F entry 26: charge index 26, damage effect byte 37. */
int map_action_entry26(int a1, int count, const uint8_t *list)
{
    return map_action_charge_damage(a1, 26, count, list, 37);
}

/* 0x22E41 - funcs_1541F entry 27: charge index 27, damage effect byte 38. */
int map_action_entry27(int a1, int count, const uint8_t *list)
{
    return map_action_charge_damage(a1, 27, count, list, 38);
}

/* 0x21227 - shared effect body of the funcs_1541F entries 0/1/2/3/8: clear
 * the status-queue flag, draw the party icons with shape bank `table_idx`,
 * blink the targets over the map view, charge cost `table_idx` via
 * rec_sub_table5, then resolve action `table_idx` on every listed record
 * (unit_action_damage) and queue the number it dealt (or the idle cursor
 * sprite on a miss).  Ends with the shared 0x21190 tail (map_view_update(0)
 * + map_draw_status_popups).  The entries below push their constant and fall
 * into this body via the 0x2121A tail of map_action_entry00. */
int map_action_resolve_list(int a1, int table_idx, int count,
                            const uint8_t *list)
{
    int i;

    dword_53EC4 = 0;
    map_draw_party_icons_anim(a1, table_idx, count, list);
    map_blink_units(a1, table_idx, count, list);
    rec_sub_table5(a1, table_idx);

    for (i = 0; i < count; ++i) {
        int dmg = unit_action_damage(list[i], table_idx);

        if (dmg != 0)
            map_enqueue_status(dmg, 94, list[i]);
        else
            ev4_1E1DC(list[i]);
    }

    map_view_update(0);
    map_draw_status_popups();
    return 0;
}

/* 0x21206 - funcs_1541F entry 0: resolve action 0 on the list. */
int map_action_entry00(int a1, int count, const uint8_t *list)
{
    return map_action_resolve_list(a1, 0, count, list);
}

/* 0x2134B - funcs_1541F entry 1: resolve action 1 on the list. */
int map_action_entry01(int a1, int count, const uint8_t *list)
{
    return map_action_resolve_list(a1, 1, count, list);
}

/* 0x21364 - funcs_1541F entry 2: resolve action 2 on the list. */
int map_action_entry02(int a1, int count, const uint8_t *list)
{
    return map_action_resolve_list(a1, 2, count, list);
}

/* 0x2137D - funcs_1541F entry 3: resolve action 3 on the list. */
int map_action_entry03(int a1, int count, const uint8_t *list)
{
    return map_action_resolve_list(a1, 3, count, list);
}

/* 0x21494 - funcs_1541F entry 8: resolve action 8 on the list. */
int map_action_entry08(int a1, int count, const uint8_t *list)
{
    return map_action_resolve_list(a1, 8, count, list);
}

/* 0x213B7 - shared effect body of the funcs_1541F entries 4/5/6/7: clear the
 * status-queue flag, draw the animated party icons with shape bank `table_idx`,
 * alternate the two record-layer animation frames, charge cost `table_idx` via
 * rec_sub_table5, then resolve action `table_idx` on every listed record and
 * queue the dealt number (or the idle cursor sprite on a miss).  Ends on the
 * shared 0x21190 tail inside 0x2111A (map_view_update(0) +
 * map_draw_status_popups), which IDA records as a false callee. */
int map_action_resolve_layer(int a1, int table_idx, int count,
                             const uint8_t *list)
{
    int i;

    dword_53EC4 = 0;
    map_draw_party_icons_anim(a1, table_idx, count, list);
    map_record_layer_blink(a1, table_idx, count, list);
    rec_sub_table5(a1, table_idx);

    for (i = 0; i < count; ++i) {
        int dmg = unit_action_damage(list[i], table_idx);

        if (dmg != 0)
            map_enqueue_status(dmg, 94, list[i]);
        else
            ev4_1E1DC(list[i]);
    }

    map_view_update(0);
    map_draw_status_popups();
    return 0;
}

/* 0x21396 - funcs_1541F entry 4: resolve action 4 on the list. */
int map_action_entry04(int a1, int count, const uint8_t *list)
{
    return map_action_resolve_layer(a1, 4, count, list);
}

/* 0x21449 - funcs_1541F entry 5: resolve action 5 on the list. */
int map_action_entry05(int a1, int count, const uint8_t *list)
{
    return map_action_resolve_layer(a1, 5, count, list);
}

/* 0x21462 - funcs_1541F entry 6: resolve action 6 on the list. */
int map_action_entry06(int a1, int count, const uint8_t *list)
{
    return map_action_resolve_layer(a1, 6, count, list);
}

/* 0x2147B - funcs_1541F entry 7: resolve action 7 on the list. */
int map_action_entry07(int a1, int count, const uint8_t *list)
{
    return map_action_resolve_layer(a1, 7, count, list);
}

/* 0x214AD - funcs_1541F entry 9: clear the status-queue flag, draw the animated
 * party icons with shape bank 9, charge cost 9, then resolve action 9 on the
 * first listed record only and queue the dealt number (or the idle cursor
 * sprite).  Ends on the shared 0x21190 tail. */
int map_action_entry09(int a1, int count, const uint8_t *list)
{
    int dmg;

    dword_53EC4 = 0;
    map_draw_party_icons_anim(a1, 9, count, list);
    rec_sub_table5(a1, 9);

    dmg = unit_action_damage(list[0], 9);
    if (dmg != 0)
        map_enqueue_status(dmg, 94, list[0]);
    else
        ev4_1E1DC(list[0]);

    map_view_update(0);
    map_draw_status_popups();
    return 0;
}
