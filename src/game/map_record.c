/* map_record.c - on-map per-record interaction driver (original 0x18D8C,
 * 800 bytes).  See map_record.h for the semantic overview.
 *
 * Direct translation of the machine code at 0x18D8C; the offline disassembly
 * (analysis/cseg01_disasm.txt, lines 10662..10889) is authoritative.
 *
 * Notes / decompiler artifacts:
 *   - the Watcom stack probe (`push 0xB0; call 0x3702F`) allocates no frame and
 *     is dropped;
 *   - `dir_tiles` (the second argument, Hex-Rays `a2`) is the caller's 4-int
 *     direction-patch tile array: it is reset, handed to the map_cursor patch
 *     primitives and used to return three status bits (dir_tiles[0] = "no
 *     slot", [1] = "blocked", [2] = "no free slot"); its [32]/[33] are *not*
 *     touched here;
 *   - the 4-int local `a_tiles` is a `rep movs` copy of the table at 0x51ED5
 *     (dseg02); Hex-Rays labels it `v21 = unk_51ED5`;
 *   - the record pointer `rp` is dword_53A45 + 80*rec; the byte fields read are
 *     +0x20, +0x21 and +0x27 (Hex-Rays reads the first two as `v7[32]`/`v7[33]`
 *     and the third as `v7[39]`);
 *   - the table accessor 0x4E8BC is base 0x602AD, stride 23 (dseg03);
 *   - the `dword_53EC8 /= v18` is a *signed* idiv (`cdq; idiv ebx`), so it is
 *     spelled with an explicit int32_t cast (dword_53EC8 is uint32_t);
 *   - Hex-Rays renders the `jle` on rp[32] as an unsigned `> 8u` compare, but
 *     the byte is movzx'd so the signed and unsigned tests agree.
 */
#include "fd2_game.h"
#include "map_record.h"

#include <stdint.h>
#include <string.h>

/* live map cursor, read fresh at each use exactly as the machine does. */
#define MAP_X ((int)(uint32_t)qword_53AB1)
#define MAP_Y ((int)(uint32_t)(qword_53AB1 >> 32))

/* 0x4E8BC - item table 0x602AD, stride 23 (dseg03). */
static const uint8_t *rec_item_table(int index)
{
    return (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x2AD], 23, index, 0);
}

/* 0x18D8C */
int map_record_interact(int rec, int *dir_tiles, int reopen)
{
    int      a_tiles[4];             /* patch tile indices from 0x51ED5 */
    uint8_t  buf[100];               /* v20: 0x64-byte event-script scratch */
    uint8_t *rp;                     /* dword_53A45 + 80*rec */
    uint64_t saved;                  /* qword_53AB1 latched before the pick */
    int      v3, v8, v13, v17, v18, steps;
    int      p4, p5;

    memcpy(a_tiles, &fd2_data1[0x1ED5], sizeof a_tiles);

    dir_tiles[0] = 0;
    dword_53EC8  = 0;

    v3 = rec_slot_find(rec, 0);
    if (v3 == -1) {
        dir_tiles[0] = 1;
    } else {
        const uint8_t *it;

        it = rec_item_table(rec_field_byte(rec, v3));
        p5 = it[11];
        p4 = it[12];
        if (map_reveal_reachable(MAP_X, MAP_Y, NULL, p4, p5, 0) == 0)
            dir_tiles[0] = 1;
        util_fix_records(dword_53A51);
    }

    ev5_173E7(dir_tiles);
    map_cursor_patch_expand(a_tiles, dir_tiles);

    if (rec_slot_free(rec) == 0)
        dir_tiles[2] = 1;
    if (rec_collect_slot_bits(rec, NULL) == 0)
        dir_tiles[1] = 1;
    rp = (uint8_t *)dword_53A45 + 80u * (uint32_t)rec;
    if (rp[39] != 0)
        dir_tiles[1] = 1;
    ev5_173E7(dir_tiles);

    do
        v8 = map_cursor_dir_select(a_tiles, dir_tiles);
    while (v8 == 0);
    map_cursor_patch_collapse(a_tiles, dir_tiles);
    map_view_update(0);
    if (v8 == -1)
        return -1;

    if (dword_53C57 != 0) {
        if (dword_53C57 == 1) {
            do
                v17 = action_menu_run(rec);
            while (v17 == 0);
            if (v17 == -1)
                return 0;
            rec_flag_or80(rec);
            v18 = rp[33];
            if (rp[32] > 8)
                v18 += 30;
            dword_53EC8 = (uint32_t)((int32_t)dword_53EC8 / v18);
            return 1;
        }
        if (dword_53C57 == 2) {
            int v19;

            do
                v19 = map_action_interact(rec);
            while (v19 == 0);
            if (v19 == -1)
                return 0;
            dword_53EC8 = 0;
            return 1;
        }
        if (reopen == 0)
            item_use(rec);
        map_cell_event(rec);
        rec_flag_or80(rec);
        return 1;
    }

    /* dword_53C57 == 0: pick a record standing on the map and run its script. */
    saved = qword_53AB1;
    {
        uint8_t *p   = (uint8_t *)guest_malloc(0x64);
        int      sel = map_reveal_reachable(MAP_X, MAP_Y, p, p4, p5, 0);

        v13 = map_pick_record(0, sel, (int)(intptr_t)p);
        util_fix_records(dword_53A51);
        guest_free(p);
    }
    if (v13 == -1) {
        map_slide_view((int)(uint32_t)saved, (int)(uint32_t)(saved >> 32));
        return 0;
    }

    {
        int portrait = dlg_portrait_find();

        ev5_1F04A(rec, portrait);
        figure_attack_view(rec, portrait);
    }
    dlg_portrait_clear();
    steps = map_collect_scripts(buf);
    map_units_animate();
    ev_script_run(rec, steps, (int)(intptr_t)buf);
    rec_flag_or80(rec);
    kbd_flush();
    return 1;
}
