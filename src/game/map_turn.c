/* map_turn.c - per-record turn loops and the record-action dispatcher.
 * See map_turn.h.  Direct translations of the machine code at 0x13A9F,
 * 0x1D80B and 0x1D8BA.  The action cases reuse the already-verified assets:
 * map_ai_decide (0x14EF0), map_move_to_marked (0x14121), map_move_to_nearest
 * (0x13E9C), map_ai_pick_reach_target (0x14237), map_ai_pick_action_target
 * (0x1598A), map_ai_pick_item_target (0x1567E), map_ai_do_action (0x15311),
 * map_attack_run (0x1548E), map_move_toward (0x14B78), rec_find (0x12C60),
 * dlg_portrait_glide (0x12D7B), dlg_portrait_clear (0x134E4), item_use
 * (0x13FD4), rec_slot_claim (0x1BB8C), rec_flag_or80 (0x13512), rec_flag_set1
 * (0x32975), map_scroll_cell_tag (0x13A44), map_find_tag_cell (0x15DF3),
 * svc_play_sfx2 (0x25B45), ev4_12263 (0x12263), kbd_flush (0x4E381) and
 * map_view_update (0x11CAC), plus the funcs_1199C / funcs_1197B handler
 * tables.
 */
#include "fd2_game.h"
#include "map_turn.h"

/* 80-byte record `idx` of the table at dword_53A45. */
static uint8_t *rec_at(int idx)
{
    return (uint8_t *)dword_53A45 + REC_STRIDE * (uint32_t)idx;
}

/* 0x13A9F */
void map_record_action(int actor, int require_empty)
{
    uint8_t *rec = rec_at(actor);
    int action;
    uint8_t tag1;
    uint8_t tag2;
    int tag;
    int v = 0;

    if ((rec[5] & 5) != 0)
        return;                             /* 0x1317D shared epilogue */

    action = rec[52] & 0xF;
    tag1 = rec[53];
    tag2 = rec[54];
    tag = rec[61];

    switch (action) {
    case 0:
        goto ai_mark;                       /* LABEL_3 */

    case 1:
        if (map_ai_decide(actor, require_empty) != 0)
            goto finish;
        v = map_move_to_marked(actor, require_empty);
        goto check_move;

    case 2:
        if (map_ai_decide(actor, require_empty) != 0)
            goto finish;
        v = map_ai_pick_reach_target(actor, require_empty);
        goto check_move;

    case 3: {
        int idx;

        if (map_ai_decide(actor, require_empty) != 0)
            goto finish;
        idx = rec_find(tag1);
        if (idx == -1)
            goto move_plain;                /* LABEL_4 */
        dlg_portrait_glide(actor);
        if (map_move_toward(rec_at(idx)[0], rec_at(idx)[1], actor,
                            require_empty) == 0)
            item_use(actor);
        dword_51A83 = 0;
        goto finish;
    }

    case 4:
        dword_51A83 = 0;
        break;                              /* common tail */

    case 5:
        if (map_ai_decide(actor, require_empty) != 0)
            goto finish;
        dword_51A83 = 0;
        dlg_portrait_glide(actor);
        if (((const uint8_t *)(uintptr_t)dword_53AD5)[tag] == 0) {
            uint8_t cell[2];

            if (map_find_tag_cell(tag, cell) == 0) {
                uint8_t *r2 = rec_at(actor);

                if (map_move_toward(cell[0], cell[1], actor,
                                    require_empty) == 0)
                    item_use(actor);
                if (r2[0] == cell[0] && r2[1] == cell[1]) {
                    uint8_t kind = *(const uint8_t *)(uintptr_t)
                        (dword_53A55 + 3 * (uint32_t)tag + 0x53);
                    uint16_t val = *(const uint16_t *)(uintptr_t)
                        (dword_53A55 + 3 * (uint32_t)tag + 0x54);

                    if (kind < 2) {
                        r2[49] = kind;
                        *(uint16_t *)(r2 + 50) = val;
                        if (kind == 0)
                            rec_slot_claim(actor, val);
                    }
                    ((uint8_t *)(uintptr_t)dword_53AD5)[tag] = 1;
                    svc_play_sfx2(dword_53EEC, 12, 1);
                    ev4_12263();
                    r2[52] = 7;
                }
                goto finish;
            }
        }
        goto move_plain;                    /* LABEL_4 */

    case 7:
        dword_51A83 = 0;
        dlg_portrait_glide(actor);
        if (map_move_toward(tag1, tag2, actor, require_empty) == 0)
            item_use(actor);
        rec = rec_at(actor);
        if (rec[0] == tag1 && rec[1] == tag2)
            rec_flag_set1(actor);
        goto finish;

    case 8:
        return;

    case 9: {
        int idx = rec_find(tag1);

        if (idx == -1)
            goto ai_mark;                   /* LABEL_3 */
        dlg_portrait_glide(actor);
        v = map_move_toward(rec_at(idx)[0], rec_at(idx)[1], actor,
                            require_empty);
        goto check_move;
    }

    case 10:
        if (map_ai_decide(actor, require_empty) != 0)
            goto finish;
        dword_51A83 = 0;
        break;                              /* common tail */

    case 11:
        map_ai_pick_action_target(actor, require_empty);
        if ((int32_t)dword_53C23 >= 6)
            map_ai_do_action(actor, require_empty);
        map_ai_pick_reach_target(actor, require_empty);
        if ((int32_t)dword_53C4F < 6) {
            if (map_move_to_marked(actor, require_empty) == 0)
                item_use(actor);
        } else {
            map_attack_run(actor, require_empty);
        }
        goto finish;

    default:
        goto finish;
    }

    /* Cases 4 and 10 commit a plain walk toward the +53/+54 cell (0x13BEB). */
    dlg_portrait_glide(actor);
    v = map_move_toward(tag1, tag2, actor, require_empty);

check_move:                                 /* LABEL_21 (0x13C06) */
    if (v == 0)
        item_use(actor);
    goto finish;

ai_mark:                                    /* LABEL_3 (0x13AF3) */
    if (map_ai_decide(actor, require_empty) != 0)
        goto finish;

move_plain:                                 /* LABEL_4 (0x13B05) */
    if (map_move_to_marked(actor, require_empty) == 0) {
        v = map_move_to_nearest(actor, require_empty);
        goto check_move;
    }

finish:                                     /* LABEL_55 (0x13E5A) */
    rec = rec_at(actor);
    map_scroll_cell_tag(rec[0], rec[1], 1);
    rec_flag_or80(actor);
    dlg_portrait_clear();
    map_view_update(0);
}

/* 0x1D80B */
void map_turn_side1(void)
{
    int i;

    dword_51A83 = 0;
    for (i = 0; i < dword_53BEB; i++) {
        uint8_t *rec;

        dword_51A83 = 0;
        kbd_flush();
        rec = rec_at(i);
        dword_51A8F = 0xFF;
        if (rec[6] == 1 && (rec[5] & 0x81) == 0 && rec[38] == 0)
            map_record_action(i, 1);
        if (dword_51A8F != 0xFF)
            ((void (*)(int))funcs_1199C[dword_51A8F])(i);
        ((int (*)(int))funcs_1197B[dword_53C03])(i);
        if (dword_53ECC != 0)
            break;
    }
}

/* 0x1D8BA */
void map_turn_side0(void)
{
    int i;

    for (i = 0; i < dword_53BEB; i++) {
        uint8_t *rec;

        dword_51A83 = 0;
        kbd_flush();
        rec = rec_at(i);
        dword_51A8F = 0xFF;
        if (rec[6] == 0 && (rec[5] & 0x81) == 0 && rec[38] == 0) {
            map_ai_pick_action_target(i, rec[6]);
            map_ai_pick_item_target(i, 0);
            if ((int32_t)dword_53C23 >= 6 || (int32_t)dword_53C33 >= 6)
                map_record_action(i, 0);
        }
        if (dword_51A8F != 0xFF)
            ((void (*)(int))funcs_1199C[dword_51A8F])(i);
        ((int (*)(int))funcs_1197B[dword_53C03])(i);
        if (dword_53ECC != 0)
            return;
    }

    for (i = 0; i < dword_53BEB; i++) {
        uint8_t *rec;

        kbd_flush();
        rec = rec_at(i);
        dword_51A8F = 0xFF;
        if (rec[6] == 0 && (rec[5] & 0x81) == 0 && rec[38] == 0)
            map_record_action(i, 0);
        if (dword_51A8F != 0xFF)
            ((void (*)(int))funcs_1199C[dword_51A8F])(i);
        ((int (*)(int))funcs_1197B[dword_53C03])(i);
        if (dword_53ECC != 0)
            break;
    }
}
