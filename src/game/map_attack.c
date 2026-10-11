/* map_attack.c - the unit attack result helpers (original 0x1E98C + 0x1ECC7),
 * the HP-bar drain (0x1E856) and the map attack sequence driver (0x1548E).
 *
 * Direct translation of the machine code at 0x1E98C/0x1ECC7/0x1E856/0x1548E;
 * the offline disassembly (analysis/cseg01_disasm.txt) is authoritative.
 *
 *   - the Watcom stack probe (`push size; call 0x3702F`) is dropped (false
 *     callee);
 *   - 0x1E98C's tail `jmp 0x1317D` and 0x1ECC7's tail `jmp 0x111B2` are the
 *     shared epilogues of other functions, i.e. plain returns here;
 *   - Hex-Rays gives 0x1ECC7 the prototype `int(int)` and renders its second
 *     argument as the frame-pointer-relative local `v31`.  The caller 0x1E856
 *     pushes both `a2` then `a1` before the call (1E90A/1E90E) and the callee
 *     reads [esp+0x50] as a2, so the real prototype is `int(int, int)`;
 *   - the map_cell_info call fills an 8-byte stack buffer (the original passes
 *     the [esp] scratch region); only out[5] (dword_53A69[tile].byte 1) is read;
 *   - v30 (record +0x21) is a byte local in the machine code and its `+30`
 *     wraps mod 256, so it stays a uint8_t here.
 */
#include "fd2_game.h"
#include "map_attack.h"

#include <stdint.h>

/* item table accessor 0x4E8BC: base 0x602AD, stride 23 (dseg03). */
static const uint8_t *item_entry(int index)
{
    return (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x2AD], 23, index, 0);
}

/* 0x1E98C */
void map_attack_sprite(int attacker, int defender)
{
    const uint8_t *r = (const uint8_t *)(uintptr_t)dword_53A45
                       + 80u * (uint32_t)defender;
    const uint8_t *eff =
        item_entry(rec_field_byte(attacker, rec_slot_find(attacker, 0)));
    const uint8_t *script =
        (const uint8_t *)(uintptr_t)tbl_u32((const void *)&fd2_data2[0x1955],
                                            eff[0]);
    int count  = script[0];
    int x      = 24 * (int)(r[0] - (uint32_t)dword_53AA9) + 4;
    int y      = 24 * (int)(r[1] - (uint32_t)dword_53AAD);
    int i;

    for (i = 0; i < count; ++i) {
        int sfx = script[2 * i + 2];

        if (sfx != 255) {
            if (byte_53C6B != 0)
                svc_play_sfx((const void *)(uintptr_t)dword_53B0F, 4, 1);
            else
                svc_play_sfx((const void *)(uintptr_t)dword_53B0F, sfx, 1);
        }
        if (byte_53C6B == 0 && i == 0)
            view_draw_unit_sprite(0xA0504, 320, defender, 2, 253);
        else if (byte_53C6B == 0 && i == 1)
            view_draw_unit_sprite(0xA0504, 320, defender, 0, 0);

        {
            void *saved = res_draw_subimage((const void *)(uintptr_t)dword_53AD1,
                                            (void *)0xA0000, 320, x, y,
                                            script[2 * i + 1]);
            j___delay(80);
            dlg_snap_restore(saved, (void *)0xA0000, 320);
        }
    }
}

/* 0x1ECC7 */
int map_attack_resolve(int attacker, int defender)
{
    const uint8_t *v2 = (const uint8_t *)(uintptr_t)dword_53A45
                        + 80u * (uint32_t)attacker;
    uint8_t *v3 = (uint8_t *)(uintptr_t)dword_53A45
                  + 80u * (uint32_t)defender;
    uint8_t info[8];
    const uint8_t *eff;
    int v7, v24;
    int v26, v27, v28, v22, v21, v20, v19, v29, v25;
    uint8_t v30;
    int v1, v23;

    byte_53C6B = 1;
    v26 = *(const uint16_t *)(const void *)(v2 + 0x48);
    v27 = *(const uint16_t *)(const void *)(v3 + 0x4A);
    v28 = *(const uint16_t *)(const void *)(v3 + 0x40);
    v22 = *(const uint16_t *)(const void *)(v3 + 0x42);
    v21 = *(const uint16_t *)(const void *)(v2 + 0x4C);
    v20 = *(const uint16_t *)(const void *)(v3 + 0x4E);
    v19 = v2[0x20] - 1;
    v30 = v2[0x21];
    v29 = v3[0x21];

    eff = item_entry(rec_field_byte(attacker, rec_slot_find(attacker, 0)));
    v7  = eff[9];
    v24 = eff[10];

    if (!rec_skip(attacker)) {
        map_cell_info(v2[0], v2[1], info);
        v26 += v26 * dword_51A12[info[5]] / 100;
    }
    if (!rec_skip(defender)) {
        map_cell_info(v3[0], v3[1], info);
        v27 += v27 * dword_51A2A[info[5]] / 100;
    }

    v25 = byte_524A8[v19];
    if (v7 == 4) {
        v25 += v24;
    } else if (v7 == 2) {
        if ((int)(util_rand() % 100) < v24) {
            v3[0x25] = (uint8_t)(util_rand() % 4 + 2);
            ev5_203BD(1, 32, 0);
            j___delay(20);
            pal_fade_add(0, 255, 0);
            j___delay(40);
            ev5_203BD(1, 32, 0);
            j___delay(20);
            pal_fade_add(0, 255, 0);
        }
    }

    if ((int)(util_rand() % 100) < v21 - v20) {
        byte_53C6B = 0;
        if ((int)(util_rand() % 100) < v25) {
            pal_fade_add(0, 255, 63);
            j___delay(20);
            pal_fade_add(0, 255, 0);
            j___delay(40);
            pal_fade_add(0, 255, 63);
            j___delay(20);
            pal_fade_add(0, 255, 0);
            v27 /= 2;
        }
        v1 = 9 * (v26 - v27) / 10;
        if (v1 < 0)
            v1 = 0;
        v23 = v1 / 9;
        if (v1 / 9 != 0)
            v1 += (int)(util_rand() % (uint32_t)v23);
        v28 -= v1;
        if (v28 < 0)
            v28 = 0;
    }

    *(uint16_t *)(void *)(v3 + 0x40) = (uint16_t)v28;

    if (v2[6] == 2) {
        int v13 = v3[7];

        if (v13 >= 0x44) {
            const uint8_t *v14 =
                (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x1AF9], 10,
                                         v13 - 68, 0);
            int v15;

            if ((v2[0x20] > 8u && v2[0x20] < 0x19u) || v2[8] == 28)
                v30 += 30;
            v15 = v29 * (int)v14[9] / v30;
            dword_53EC8 = (uint32_t)v15;
            if (v28 != 0)
                dword_53EC8 = (uint32_t)(v1 * v15 / v22);
        }
    }

    return v28;
}

/* 0x1E856 - drain the defender's HP bar after an attack.
 *
 * Direct translation of the machine code at 0x1E856; the offline disassembly
 * (analysis/cseg01_disasm.txt) is authoritative.  The Watcom stack probe
 * (`push 0x2c; call 0x3702F`) is dropped and the tail `jmp 0x1317D` is the
 * shared epilogue of other functions (a plain return).
 *
 * `defender`'s record supplies the HP word (+0x40 current / +0x42 max); the
 * pre-attack bar length is 70*cur/max segments, the target after each
 * map_attack_resolve is 69*remaining/max + 1.  The bar is drawn as decreasing
 * ui_meter_fixed segments at pos[0] + 320*(pos[1]+6) + 0xA0000 + 7.  The whole
 * drain repeats a second time when the attacker's item effect type is 3 or a
 * 3% roll hits (the machine's count-down do/while).
 *
 * Returns the defender's remaining HP: the machine keeps the last
 * map_attack_resolve result in [esp+8] and returns it (`mov eax,[esp+8];
 * jmp 0x1317D`), the caller 0x1548E tests it (`test eax,eax; setne`) to decide
 * whether the target survived for a counter-attack. */
int map_attack_bar_drain(int attacker, int defender, int *pos)
{
    const uint8_t *r = (const uint8_t *)(uintptr_t)dword_53A45
                       + 80u * (uint32_t)defender;
    const uint8_t *eff =
        item_entry(rec_field_byte(attacker, rec_slot_find(attacker, 0)));
    int count = (eff[9] == 3 || (util_rand() % 100u) < 3u) ? 2 : 1;
    int rem   = 0;

    for (;;) {
        int v9, v8;

        if (count == 0)
            break;
        --count;

        v9 = *(const uint16_t *)(const void *)(r + 66); /* max HP (word 33) */
        v8 = 70 * *(const uint16_t *)(const void *)(r + 64) / v9; /* cur 64 */
        rem = map_attack_resolve(attacker, defender);
        map_attack_sprite(attacker, defender);

        while (v8 >= 69 * rem / v9 + 1) {
            ui_meter_fixed(pos[0] + 320 * (pos[1] + 6) + 655360 + 7, 320, v8);
            j___delay(8);
            --v8;
        }

        if (rem == 0)
            break;
    }
    return rem;
}

/* 0x2F7B6 - full attack evaluation between two records (a sibling of
 * map_attack_resolve, called by the figure/combat driver 0x2EBE1).
 *
 * The machine code takes `a3` as an int[6] result block (esi = [esp+0x58]); it
 * shares the record offsets and the 0x51A12/0x51A2A/0x524A8 tables with
 * map_attack_resolve.  The level table 0x4E84F is base 0x61AF9 / stride 10 (the
 * offline disassembly of 0x4E84F shows `lea edx,ds:0x61af9`, i.e. fd2_data2
 * offset 0x1AF9).  The `+30` on the atk[0x21] byte wraps mod 256 (the machine
 * uses `add BYTE PTR`, so it stays a uint8_t here).  The tail leaves the
 * damage in EAX; no caller-visible epilogue jump. */
int map_attack_result(int attacker, int defender, int *out)
{
    uint8_t *atk = (uint8_t *)(uintptr_t)dword_53A45
                   + 80u * (uint32_t)attacker;
    uint8_t *def = (uint8_t *)(uintptr_t)dword_53A45
                   + 80u * (uint32_t)defender;
    uint8_t info[8];
    int v27, v29, v28, v20, v23, v21, v22;
    int v30, v26, v24, v8, v25;
    uint8_t v32, v31;
    const uint8_t *eff;

    v30 = 0;
    out[0] = 1;
    out[1] = 0;
    out[2] = 0;
    out[3] = 0;
    out[4] = 0;
    out[5] = 0;

    v27 = *(const uint16_t *)(const void *)(atk + 0x48);
    v29 = *(const uint16_t *)(const void *)(def + 0x4A);
    v28 = *(const uint16_t *)(const void *)(def + 0x40);
    v20 = *(const uint16_t *)(const void *)(def + 0x42);
    v23 = *(const uint16_t *)(const void *)(atk + 0x4C);
    v21 = *(const uint16_t *)(const void *)(def + 0x4E);
    v22 = atk[0x20] - 1;
    v32 = atk[0x21];
    v31 = def[0x21];

    eff = item_entry(rec_field_byte(attacker, rec_slot_find(attacker, 0)));
    v8  = eff[9];
    v25 = eff[10];

    if (!rec_skip(attacker)) {
        map_cell_info(atk[0], atk[1], info);
        v27 += v27 * dword_51A12[info[5]] / 100;
    }
    if (!rec_skip(defender)) {
        map_cell_info(def[0], def[1], info);
        v29 += v29 * dword_51A2A[info[5]] / 100;
    }

    v26 = byte_524A8[v22];
    switch (v8) {
    case 4:
        v26 += v25;
        break;
    case 2:
        if ((int)(util_rand() % 100) < v25) {
            def[0x25] = (uint8_t)(util_rand() % 4 + 2);
            out[2] = 1;
        }
        break;
    case 3:
        out[4] = 1;
        break;
    default:
        break;
    }

    if ((int)(util_rand() % 100) < v23 - v21) {
        out[0] = 0;
        if ((int)(util_rand() % 100) < v26) {
            v29 /= 2;
            out[1] = 1;
        }
        v30 = 9 * (v27 - v29) / 10;
        if (v30 < 0)
            v30 = 0;
        v24 = v30 / 9;
        if (v30 / 9 != 0)
            v30 += (int)(util_rand() % (uint32_t)v24);
        v28 -= v30;
        if (v28 < 0)
            v28 = 0;
    }

    if (atk[6] == 2) {
        int v13 = def[7];

        if (v13 >= 0x44) {
            const uint8_t *lv =
                (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x1AF9], 10,
                                         v13 - 68, 0);
            int v16;

            if ((atk[0x20] > 8u && atk[0x20] < 0x19u) || atk[8] == 28)
                v32 += 30;
            v16 = v31 * (int)lv[9] / v32;
            dword_53EC8 = v16;
            if (v28 != 0)
                dword_53EC8 = v30 * v16 / v20;
        }
    }

    out[5] = v30;
    return v30;
}

/* 0x1548E - run one map attack against the current target dword_53C4B.
 *
 * Direct translation of the machine code at 0x1548E; the offline disassembly
 * (analysis/cseg01_disasm.txt) is authoritative.  The Watcom stack probe
 * (`push 0x2c; call 0x3702F`) is dropped; the local script buffer is the
 * machine's [esp] scratch region (IDA's 24-byte v7; the original leaves it
 * uninitialised before map_collect_scripts fills it).
 *
 * `actor` glides its portrait in and walks toward the marked cell
 * (dword_53C43/dword_53C47, `mode`), the target dword_53C4B then faces it.
 * With the overlay on (byte_53AF9) it plays the status frame and drains the
 * target's HP bar; if the target is facing-adjacent and survives (the bar
 * drain's remaining-HP result is non-zero) the roles swap for a counter-attack
 * on the actor's own bar.  With the overlay off it plays figure_attack_view.
 * The tail clears the portrait, collects the party's +49 scripts, animates the
 * units, runs the scripts (ev_script_run) and the level-up screen. */
int map_attack_run(int actor, int mode)
{
    uint8_t scripts[24]; /* [esp-18h] scratch: 8 x 3-byte script entries */
    int     steps;
    int    *pos;

    dword_51A83 = 0;
    dlg_portrait_glide(actor);
    map_move_toward((int)dword_53C43, (int)dword_53C47, actor, mode);
    dword_51A83 = 1;
    dlg_portrait_glide((int)dword_53C4B);
    ev5_1F04A(actor, (int)dword_53C4B);

    if (byte_53AF9 != 0) {
        int rem;

        dword_51A83 = 0;
        map_view_update(0);
        dword_51A83 = 1;

        if (ev5_1F0DC(actor, (int)dword_53C4B) == 1)
            ev5_1F04A((int)dword_53C4B, actor);

        pos = map_status_overlay_play(actor, (int)dword_53C4B);
        map_status_gauge(0xA0000, 320, (int)dword_53C4B, pos);
        if (ev5_1F0DC(actor, (int)dword_53C4B) == 1)
            map_status_gauge(0xA0000, 320, actor, pos + 2);

        dlg_portrait_clear();
        rem = map_attack_bar_drain(actor, (int)dword_53C4B, pos);

        if (ev5_1F0DC(actor, (int)dword_53C4B) == 1 && rem != 0) {
            ev5_1F04A((int)dword_53C4B, actor);
            ev5_1F04A(actor, (int)dword_53C4B);
            map_status_frame(pos, actor, (int)dword_53C4B);
            map_attack_bar_drain((int)dword_53C4B, actor, pos + 2);
        }
    } else {
        figure_attack_view(actor, (int)dword_53C4B);
    }

    dlg_portrait_clear();
    steps = map_collect_scripts(scripts);
    map_view_update(0);
    map_units_animate();
    ev_script_run((int)dword_53C4B, steps, (int)(intptr_t)scripts);
    map_view_update(0);
    map_levelup_run((int)dword_53C4B);
    return 1;
}
