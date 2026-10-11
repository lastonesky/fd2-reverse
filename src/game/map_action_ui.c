/* map_action_ui.c - interactive per-record map action driver (original 0x1BBDC,
 * 1058 bytes).  See map_action_ui.h for the semantic overview.
 *
 * Direct translation of the machine code at 0x1BBDC; the offline disassembly
 * (analysis/cseg01_disasm.txt, lines 14145..14460) is authoritative.
 *
 * Notes / decompiler artifacts:
 *   - the Watcom stack probe (`push 0x94; call 0x3702F`) allocates no frame and
 *     is dropped;
 *   - the two 16-byte locals are `rep movs` copies of the tile tables at 0x51F05
 *     (dseg02) and 0x53F32 (dseg02); Hex-Rays labels them `v16 = unk_51F05`
 *     / `v17 = unk_53F32`;
 *   - `v17.dword1 = 1` (the machine write at 1bc58) zeroes the first-reveal flag
 *     when map_reveal_reachable found no cell;
 *   - Hex-Rays types the record field byte as `char`, but the machine pushes the
 *     movzx'd int (0..255) to rec_slot_claim, so it is kept as `int` here;
 *   - `saved` (Hex-Rays v20) is the 64-bit qword_53AB1 latched *before* the
 *     action loop and used for map_slide_view; the flood calls re-read the live
 *     qword_53AB1, matching the fresh `mov ds:0x53ab1/ab5` loads;
 *   - the mode-2 tail reuses the mode-1 epilogue's `add esp,4` slot, so it does
 *     *not* call unit_metrics (faithfully reproduced by an early `return 0`).
 */
#include "fd2_game.h"
#include "map_action_ui.h"

#include <stdint.h>
#include <string.h>

/* live map cursor, read fresh at each use exactly as the machine does. */
#define MAP_X ((int)(uint32_t)qword_53AB1)
#define MAP_Y ((int)(uint32_t)(qword_53AB1 >> 32))

/* item table accessor 0x4E8BC: base 0x602AD, stride 23 (dseg03). */
static const uint8_t *item_table(int index)
{
    return (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x2AD], 23, index, 0);
}

/* 0x1BBDC */
int map_action_interact(int actor)
{
    uint8_t  list[52];               /* rec picks / action list (local v15)   */
    int      a_tiles[4];             /* patch tile indices from 0x51F05       */
    int      b_tiles[4];             /* patch tile indices from 0x53F32       */
    uint64_t saved;                  /* qword_53AB1 latched before the loop   */
    int      v2, v3, v7, v8, v21;
    const uint8_t *entry;

    memcpy(a_tiles, &fd2_data1[0x1F05], sizeof a_tiles);
    memcpy(b_tiles, &fd2_data1[0x3F32], sizeof b_tiles);

    if (rec_slot_free(actor) == 0)
        return -1;

    v2 = map_reveal_reachable(MAP_X, MAP_Y, NULL, 1, 1, 3);
    util_fix_records(dword_53A51);
    if (v2 == 0)
        b_tiles[1] = 1;
    ev5_173E7(b_tiles);
    map_cursor_patch_expand(a_tiles, b_tiles);
    do {
        v3 = map_cursor_dir_select(a_tiles, b_tiles);
    } while (v3 == 0);
    map_cursor_patch_collapse(a_tiles, b_tiles);
    map_view_update(0);
    saved = qword_53AB1;
    if (v3 == -1)
        return -1;

    if (dword_53C57 == 0) {
        for (;;) {
            if (box_popup(actor, 1) == 0)
                return 0;
            v7 = rec_field_byte(actor, dword_53C57);
            entry = item_table(v7);
            dword_51A83 = entry[18] + 2;
            v7 = map_reveal_reachable(MAP_X, MAP_Y, list, entry[16],
                                      entry[13] == 23, entry[21]);
            v8 = map_pick_record(entry[21], v7, list);
            util_fix_records(dword_53A51);
            dword_51A83 = 1;
            v21 = map_reveal_reachable(MAP_X, MAP_Y, list, entry[18], 0,
                                       entry[21]);
            util_fix_records(dword_53A51);
            if (entry[13] == 23) {
                const uint8_t *rec = (const uint8_t *)dword_53A45 +
                                     80u * (uint32_t)actor;

                if (rec[8] != 24 || *(const uint16_t *)(rec + 0x46) < 0x14)
                    v8 = -1;
                if (v8 != -1)
                    v8 = map_pick_record(6, list[0], 0);
                if (v8 != -1) {
                    qword_51CF9 = qword_53AB1;
                    dword_51A83 = 0;
                    dlg_portrait_glide(actor);
                    dword_51A83 = 1;
                }
            }
            if (v8 != -1) {
                map_action_router(actor, dword_53C57, v21, list);
                rec_flag_or80(actor);
                return 1;
            }
            dword_51A83 = 0;
            dlg_portrait_glide(actor);
            dword_51A83 = 1;
        }
    }

    if (dword_53C57 == 1) {
        void *buf;
        int   v11, v12, v18, v19, v13, v14;

        if (box_popup(actor, 0) == 0)
            goto label37;
        buf = guest_malloc(0x64);
        v11 = map_reveal_reachable(MAP_X, MAP_Y, (uint8_t *)buf, 1, 1, 3);
        v18 = map_pick_record(3, v11, buf);
        v12 = dlg_portrait_find();
        util_fix_records(dword_53A51);
        map_slide_view((int)(uint32_t)saved, (int)(uint32_t)(saved >> 32));
        guest_free(buf);
        if (v18 == -1)
            goto label37;
        v13 = rec_field_byte(actor, dword_53C57);
        if (rec_slot_claim(v12, v13) == -1) {
            v19 = dword_53C57;
            if (box_popup(v12, 0) == 0)
                goto label37;
            v14 = rec_field_byte(v12, dword_53C57);
            rec_slot_remove(v12, dword_53C57);
            rec_slot_claim(v12, v13);
            rec_slot_remove(actor, v19);
            rec_slot_claim(actor, v14);
        } else {
            rec_slot_remove(actor, dword_53C57);
        }
        dword_53C53 = 1;
        goto label37;
    }

    if (dword_53C57 != 2) {
        if (box_popup(actor, 0))
            rec_slot_remove(actor, dword_53C57);
        goto label37;
    }

    box_run(actor);
    return 0;

label37:
    unit_metrics(actor);
    return 0;
}
