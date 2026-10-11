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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* 0x2111A - the action router sub_20C6F's action-code 21 body (and also called
 * by 0x2D80D).  Same effect as map_action_resolve_layer without the status-flag
 * clear and without charging the 0x619FD cost: draw the animated party icons
 * with shape bank `bank`, alternate the record-layer animation frames, then
 * resolve action `bank` on every listed record (unit_action_damage) and queue
 * the number it dealt (or the idle cursor sprite on a miss).  It OWNS the
 * shared 0x21190 epilogue (`map_view_update(0); map_draw_status_popups()`),
 * which the siblings 0x21227/0x213B7/0x211A4/0x214AD jump into (false callee). */
int map_action_anim_resolve(int a1, int count, const uint8_t *list, int bank)
{
    int i;

    map_draw_party_icons_anim(a1, bank, count, list);
    map_record_layer_blink(a1, bank, count, list);

    for (i = 0; i < count; ++i) {
        int dmg = unit_action_damage(list[i], bank);

        if (dmg != 0)
            map_enqueue_status(dmg, 94, list[i]);
        else
            ev4_1E1DC(list[i]);
    }

    map_view_update(0);
    map_draw_status_popups();
    return 0;
}

/* 0x211A4 - the action router sub_20C6F's action-code 5 / 13 body (and called
 * by 0x2D80D).  Draws both party-icon passes with the fixed shape bank 13, then
 * heals each listed record by `amount` (unit_heal) and queues the recovered
 * number with glyph base 105.  Ends on the shared 0x21190 epilogue. */
int map_action_heal_list(int a1, int count, const uint8_t *list, int amount)
{
    int i;

    map_draw_party_icons_anim(a1, 13, count, list);
    map_draw_party_icons(a1, 13, count, list);

    for (i = 0; i < count; ++i)
        map_enqueue_status(unit_heal(list[i], amount), 105, list[i]);

    map_view_update(0);
    map_draw_status_popups();
    return 0;
}

/* 0x20C6F - the map action router the whole map-action family hangs off.
 *
 * rec_field_byte(actor, slot) selects an item entry through the 0x4E8BC
 * accessor (base 0x602AD, stride 23); its byte +13 is the action code and the
 * uint16 at +14 is the shape bank.  The action code picks the effect to run
 * over the `count` records named by `list`: healing (5/13), item_apply (6/7),
 * the field/flag writers 8/9/10/17/18/19, the restore-mp pass (11), the growth
 * writers 12/15/16, the damage passes 14/20/24/22, the record-layer resolve
 * (21) and the unit deploy (23).  Actions 5/6/7/11 then drop the router's
 * `slot` from the actor's equipment list (rec_slot_remove); every path ends on
 * the shared tail: zero dword_53EC8, release the FDOTHER effect bank
 * (menu_stop_and_free_music), collect the party's +49 script entries
 * (map_collect_scripts), blink the units (map_units_animate) and run the
 * scripts through the event-script interpreter (ev_script_run).
 *
 * The offline disassembly is authoritative: the Watcom stack probe
 * (`push 0x9C; call 0x3702F`) is dropped; rec_field_byte's second argument is
 * the caller's `slot` and the tail's 14 IDA arguments to menu_stop_and_free_music
 * are a decompiler artifact of the uninitialised 0x64-byte script buffer (the
 * machine pushes nothing).  Both callers (0x152F0, 0x1BE45) ignore the result,
 * so returning 0 keeps the void ev_script_run tail. */
int map_action_router(int actor, int slot, int count, const uint8_t *list)
{
    uint8_t        scripts[100];   /* 25 x 4-byte script-entry slots */
    int            steps;
    const uint8_t *entry;
    int            action;
    int            bank;
    int            i;

    ev5_1D4CB();
    dword_53EC4 = 0;

    entry  = (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x2AD], 23,
                                      rec_field_byte(actor, slot), 0);
    bank   = (int)(uint16_t)(entry[14] | (entry[15] << 8));
    action = entry[13];

    if (action == 5 || action == 13) {
        map_action_heal_list(actor, count, list, bank);
        if (action == 5)
            goto remove_slot;
        goto tail;
    }

    switch (action) {
    case 6:
        item_apply(actor, 20, count, list, 37);
        if (dword_53EC4 != 0)
            map_draw_status_popups();
        goto remove_slot;
    case 7:
        item_apply(actor, 21, count, list, 38);
        if (dword_53EC4 != 0)
            map_draw_status_popups();
        goto remove_slot;
    case 8:
        map_action_add_field(actor, bank, 55, slot, count, list, 17);
        goto tail;
    case 9:
        map_action_add_field(actor, bank, 57, slot, count, list, 18);
        goto tail;
    case 10:
        map_action_add_field(actor, bank, 62, slot, count, list, 19);
        goto tail;
    case 11:
        map_draw_party_icons_anim(actor, 13, count, list);
        map_draw_party_icons(actor, 13, count, list);
        for (i = 0; i < count; ++i) {
            uint8_t *rec = (uint8_t *)dword_53A45 + REC_STRIDE * list[i];

            if (*(uint16_t *)(void *)(rec + 70) != 0)
                map_enqueue_status(unit_restore_mp(list[i], bank), 105,
                                   list[i]);
            else
                ev4_1E1DC(list[i]);
        }
        map_view_update(0);
        map_draw_status_popups();
        goto remove_slot;
    case 12:
        map_action_widen(actor, count, list);
        goto tail;
    case 14:
        map_action_damage_records(actor, 27, count, list, 38);
        goto tail;
    case 15:
        map_action_grow_y(actor, count, list);
        goto tail;
    case 16:
        map_action_grow_x(actor, count, list);
        goto tail;
    case 17:
        map_action_add_field(actor, bank, 66, slot, count, list, 13);
        goto tail;
    case 18:
        map_action_add_field(actor, bank, 70, slot, count, list, 13);
        goto tail;
    case 19: {
        uint8_t *rec   = (uint8_t *)dword_53A45 + REC_STRIDE * list[0];
        uint8_t  saved = rec[60];

        /* rec[60] is saved and restored around the effect (the effect draws
         * the icon and the byte is masked out for its duration). */
        map_action_add_field(actor, bank, 59, slot, count, list, 19);
        rec[60] = saved;
        goto tail;
    }
    case 20:
    case 24:
        map_draw_party_icons_anim(actor, bank, count, list);
        map_blink_units(actor, bank, count, list);
        for (i = 0; i < count; ++i) {
            int dmg = unit_action_damage(list[i], bank);

            if (dmg != 0)
                map_enqueue_status(dmg, 94, list[i]);
            else
                ev4_1E1DC(list[i]);
        }
        map_view_update(0);
        map_draw_status_popups();
        goto tail;
    case 21:
        map_action_anim_resolve(actor, count, list, bank);
        goto tail;
    case 22:
        map_action_damage_records(actor, 22, count, list, 39);
        goto tail;
    case 23:
        map_units_move_deploy(actor, count, list);
        goto tail;
    default:
        goto tail;
    }

remove_slot:
    rec_slot_remove(actor, slot);

tail:
    dword_53EC8 = 0;
    menu_stop_and_free_music();
    steps = map_collect_scripts(scripts);
    map_units_animate();
    ev_script_run(actor, steps, scripts);
    return 0;
}

/* 0x21548 - shared body of the funcs_1541F entries 10/11/12.  Decode the
 * current shape library into a raw 24x24 bitmap library, build the 64x64
 * source-base table over it, then play the three-frame map zoom (fixed-point
 * centre and zoom taken from the 0x52096 / 0x520A2 / 0x520AE dword tables)
 * while re-drawing the unflagged record portraits, cycle the four scratch
 * frames into VGA_P(0xA0504u) for 60 ticks (playing SFX bank-13 sample 2 whenever
 * n < 43 and n % 6 == 0), and finally resolve action `table_idx` on every
 * listed record (queueing the dealt number, or the idle cursor sprite on a
 * miss), refresh the view and pop the status numbers.  `table_idx` is 10/11/12
 * for the three entries.  The Watcom stack probe (`push 0x6C; call 0x3702F`)
 * is dropped; the OOM path is `printf(aOutOfMemoryAtE); exit`). */
int map_action_zoom_resolve(int a1, int table_idx, int count,
                            const uint8_t *list)
{
    uint32_t xbase[3];
    uint32_t ybase[3];
    uint32_t zoom[3];
    uint8_t *saved_shape;
    uint8_t *frame;
    uint8_t *scratch0;
    uint8_t *scratch1;
    uint8_t *scratch2;
    uint8_t *pool[4];
    uint8_t **cells;
    int      i, j, k, m, n;

    /* 0x52096/0x520A2/0x520AE: the x base, y base and zoom of the three key
     * frames (each three dwords, copied before the library is rebuilt). */
    memcpy(xbase, &fd2_data1[0x2096], sizeof xbase);
    memcpy(ybase, &fd2_data1[0x20A2], sizeof ybase);
    memcpy(zoom,  &fd2_data1[0x20AE], sizeof zoom);

    saved_shape = dword_53A5D;
    dword_53A5D  = map_shape_lib_decode();
    dword_53EC4  = 0;
    rec_sub_table5(a1, table_idx);

    scratch0 = (uint8_t *)guest_malloc(0xFA00);
    scratch1 = (uint8_t *)guest_malloc(0x25680);
    scratch2 = (uint8_t *)guest_malloc(0x25680);
    if (scratch0 == NULL || scratch1 == NULL || scratch2 == NULL) {
        /* 0x215e7: push aOutOfMemoryAtE; call printf; push 0; jmp exit. */
        printf("%s", aOutOfMemoryAtE);
        exit(0);
    }

    frame    = dword_53A49;
    pool[0]  = frame;
    pool[1]  = scratch1;
    pool[2]  = scratch2;
    pool[3]  = scratch1;   /* the machine's 4th slot aliases the 2nd buffer */

    /* 64x64 table of 24x24 source bases over the decoded shape library. */
    cells = (uint8_t **)guest_malloc(64u * 64u * sizeof(uint8_t *));
    for (i = 0; i < (int)dword_53AC5; ++i) {
        for (j = 0; j < (int)dword_53AC1; ++j) {
            uint8_t cell[8];

            map_cell_info(j, i, cell);
            cells[64 * i + j] = dword_53A5D + 6 +
                                576u * *(uint16_t *)(void *)cell;
        }
    }

    for (k = 0; k < 3; ++k) {
        dword_53A49 = scratch0;
        map_zoom_draw((int)(xbase[k] + 3072u * (uint32_t)dword_53AA9
                            + 1536u * (uint32_t)dword_51A87),
                      (int)(ybase[k] + 3072u * (uint32_t)dword_53AAD
                            + 1536u * (uint32_t)dword_51A8B),
                      (int)zoom[k], cells);
        gfx_copy_rows(pool[k] + 0x8088, 456, scratch0 + 0x504, 320, 312, 192);
        dword_53A49 = pool[k];
        for (m = 0; m < dword_53BEB; ++m)
            if ((((uint8_t *)dword_53A45)[REC_STRIDE * m + 5] & 1) == 0)
                dlg_portrait_draw(m);
    }

    for (n = 0; n < 60; ++n) {
        if (n < 43 && n % 6 == 0)
            svc_play_sfx((const void *)(uintptr_t)dword_53B13, 13, 1);
        gfx_copy_rows((void *)(uintptr_t)VGA_P(0xA0504u), 320, pool[n % 4] + 0x8088,
                      456, 312, 192);
        j___delay(10);
    }

    guest_free(scratch0);
    guest_free(scratch1);
    guest_free(scratch2);
    dword_53A49 = frame;
    guest_free(dword_53A5D);
    dword_53A5D = saved_shape;
    map_view_update(0);

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

/* 0x21527 - funcs_1541F entry 10: run the map-zoom damage body with index 10. */
int map_action_entry10(int a1, int count, const uint8_t *list)
{
    return map_action_zoom_resolve(a1, 10, count, list);
}

/* 0x2185F - funcs_1541F entry 11: play SFX bank-13 sample 2 and reveal the
 * cell portrait at radius 15, step 10, then run the map-zoom damage body with
 * index 11.  The tail jumps into 0x2153b inside 0x21527 (not a call). */
int map_action_entry11(int a1, int count, const uint8_t *list)
{
    svc_play_sfx((const void *)(uintptr_t)dword_53B13, 2, 1);
    portrait_reveal_cell(a1, 15, 10);
    return map_action_zoom_resolve(a1, 11, count, list);
}

/* 0x21A9E - funcs_1541F entry 12: as entry 11 but radius 30, step 16, index 12. */
int map_action_entry12(int a1, int count, const uint8_t *list)
{
    svc_play_sfx((const void *)(uintptr_t)dword_53B13, 2, 1);
    portrait_reveal_cell(a1, 30, 16);
    return map_action_zoom_resolve(a1, 12, count, list);
}

/* 0x21B18 - shared body of the funcs_1541F entries 13/14/15/16/24.  Clear the
 * status-queue flag, draw both party-icon passes with shape bank `table_idx`,
 * charge cost `table_idx` via rec_sub_table5, then heal each listed record by
 * the amount read from the action's tbl_619FD entry (unit_heal_by_action) and
 * queue the recovered number with glyph base 105.  Ends on the shared 0x21190
 * epilogue (map_view_update(0) + map_draw_status_popups) inside 0x2111A,
 * which IDA records as a false callee. */
int map_action_heal_action_list(int a1, int table_idx, int count,
                                const uint8_t *list)
{
    int i;

    dword_53EC4 = 0;
    map_draw_party_icons_anim(a1, table_idx, count, list);
    map_draw_party_icons(a1, table_idx, count, list);
    rec_sub_table5(a1, table_idx);

    for (i = 0; i < count; ++i)
        map_enqueue_status(unit_heal_by_action(list[i], table_idx), 105,
                           list[i]);

    map_view_update(0);
    map_draw_status_popups();
    return 0;
}

/* 0x21AD9 - funcs_1541F entry 13: play SFX bank-13 sample 11 and zoom-reveal
 * the portrait (radius 1, step 2), then run the heal-by-action body with index
 * 13.  The tail jumps into 0x21b0b inside 0x21B18 (not a call). */
int map_action_entry13(int a1, int count, const uint8_t *list)
{
    svc_play_sfx((const void *)(uintptr_t)dword_53B13, 11, 1);
    portrait_reveal_zoom(1, 2);
    return map_action_heal_action_list(a1, 13, count, list);
}

/* 0x21B99 - funcs_1541F entry 14: play SFX bank-13 sample 11 and zoom-reveal
 * the portrait (radius 2, step 4), then run the heal-by-action body with index
 * 14.  The tail jumps into 0x21b0b inside 0x21B18 (not a call). */
int map_action_entry14(int a1, int count, const uint8_t *list)
{
    svc_play_sfx((const void *)(uintptr_t)dword_53B13, 11, 1);
    portrait_reveal_zoom(2, 4);
    return map_action_heal_action_list(a1, 14, count, list);
}

/* 0x2211C - funcs_1541F entry 15: play SFX bank-13 sample 11 and zoom-reveal
 * the portrait (radius 8, step 4), then run the heal-by-action body with index
 * 15.  The tail jumps into 0x21b0b inside 0x21B18 (not a call). */
int map_action_entry15(int a1, int count, const uint8_t *list)
{
    svc_play_sfx((const void *)(uintptr_t)dword_53B13, 11, 1);
    portrait_reveal_zoom(8, 4);
    return map_action_heal_action_list(a1, 15, count, list);
}

/* 0x22153 - funcs_1541F entries 16 and 24 (the dispatch table points both slots
 * at this address): play SFX bank-13 sample 11 and zoom-reveal the portrait
 * (radius 6, step 6), then run the heal-by-action body with index 16.  The tail
 * jumps into 0x21b0b inside 0x21B18 (not a call). */
int map_action_entry16(int a1, int count, const uint8_t *list)
{
    svc_play_sfx((const void *)(uintptr_t)dword_53B13, 11, 1);
    portrait_reveal_zoom(6, 6);
    return map_action_heal_action_list(a1, 16, count, list);
}
