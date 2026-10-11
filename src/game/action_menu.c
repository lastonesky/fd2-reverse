/* action_menu.c - map command/action list panel.  See action_menu.h.
 *
 * Direct translation of the machine code at 0x17AED/0x1CEED/0x1D51D; the
 * offline disassembly (analysis/cseg01_disasm.txt) is authoritative.
 * Decompiler artifacts:
 *   - the Watcom stack probes (`push 0x18/0x5C/0x28; call 0x3702F`) allocate no
 *     frame and are dropped;
 *   - the trailing `jmp 0x15309` (0x1CEED) and `jmp 0x16F04` (0x1D51D) are
 *     shared epilogues owned by other functions, i.e. plain `return`s;
 *   - 0x17AED's tail `push ds:0x53C63; jmp 0x15E94` is `guest_free(dword_53C63)`
 *     followed by the shared `pop ebx; ret`; the machine leaves eax = the
 *     free() result but every caller ignores it, so the routine is void;
 *   - 0x1D51D's `v2 = v1` (an earlier stray eax) is dead; the real count comes
 *     from rec_collect_slot_bits and the key from dlg_wait_key (byte_53A8E).
 */
#include "fd2_game.h"
#include "action_menu.h"

#include <stdint.h>
#include <string.h>

/* action table accessor 0x4E866: base 0x619FD, stride 7 (dseg03). */
static const uint8_t *action_entry(int index)
{
    return (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x19FD], 7, index, 0);
}

/* 0x1CEED */
void action_menu_draw(int rec, int sel, uint8_t *base)
{
    uint8_t list[32];                  /* machine buffer is 0x20 bytes        */
    int count = rec_collect_slot_bits(rec, list);
    int i;

    for (i = 0; i < count; ++i) {
        int x  = 100 * (i / 4) + 18;
        int fg = (i == sel) ? 201 : 205;
        int y  = 320 * (22 * (i % 4) + 103);
        int y2 = 320 * (22 * (i % 4) + 108);
        const uint8_t *e = action_entry(list[i]);

        vm_run(dword_53A7D, list[i] + 441,base + (x + y), 320, fg, 76, 0, 0, 0);

        dlg_frame_tile(base + (y2 + x + 50),
                       320, dword_53A81, 92);
        dlg_draw_number(base + (y2 + x + 73),
                        320, e[5], 42, 2);
    }
}

/* 0x17AED - open the record's command box and animate its action list. */
void action_menu_open(int rec)
{
    int i, j;

    box_open_anim(rec);
    dlg_wait_key(0);

    if (rec_collect_slot_bits(rec, NULL) != 0) {
        svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 6, 1);

        for (i = 0; i <= 6; ++i) {
            memmove((void *)(uintptr_t)dword_53C5B,
                    (const void *)(uintptr_t)dword_53C5F, 0xFA00u);
            fb_slide_band_a(5, (char *)(uintptr_t)dword_53C5B,
                            (char *)(uintptr_t)dword_53C63);
            fb_slide_band_b(7, (char *)(uintptr_t)dword_53C5B,
                            (char *)(uintptr_t)dword_53C63);
            fb_slide_band_c(16 * i + 94, (char *)(uintptr_t)dword_53C5B,
                            (char *)(uintptr_t)dword_53C63);
            memmove((void *)(uintptr_t)VGA_P(0xA0000u),
                    (const void *)(uintptr_t)dword_53C5B, 0xFA00u);
        }

        memmove((void *)(uintptr_t)dword_53C5B,
                (const void *)(uintptr_t)dword_53C5F, 0xFA00u);
        fb_slide_band_a(5, (char *)(uintptr_t)dword_53C5B,
                        (char *)(uintptr_t)dword_53C63);
        fb_slide_band_b(7, (char *)(uintptr_t)dword_53C5B,
                        (char *)(uintptr_t)dword_53C63);
        memmove((void *)(uintptr_t)VGA_P(0xA0000u),
                (const void *)(uintptr_t)dword_53C5B, 0xFA00u);

        rle2_blit((void *)(uintptr_t)(dword_53C63 + 30085),
                  (const void *)(uintptr_t)
                      (*(const uint32_t *)(const void *)
                           ((uintptr_t)dword_53A81 + 90) +
                       (uintptr_t)dword_53A81), 320);

        action_menu_draw(rec, -1, dword_53C63);
        svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 5, 1);

        for (j = 6; j >= 0; --j) {
            memmove((void *)(uintptr_t)dword_53C5B,
                    (const void *)(uintptr_t)dword_53C5F, 0xFA00u);
            fb_slide_band_a(5, (char *)(uintptr_t)dword_53C5B,
                            (char *)(uintptr_t)dword_53C63);
            fb_slide_band_b(7, (char *)(uintptr_t)dword_53C5B,
                            (char *)(uintptr_t)dword_53C63);
            fb_slide_band_c(16 * j + 94, (char *)(uintptr_t)dword_53C5B,
                            (char *)(uintptr_t)dword_53C63);
            memmove((void *)(uintptr_t)VGA_P(0xA0000u),
                    (const void *)(uintptr_t)dword_53C5B, 0xFA00u);
        }

        dlg_wait_key(0);
    }

    for (i = 0; i <= 11; ++i) {
        if (i == 0 || i == 7)
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 6, 1);
        fb_slide_step(i, (char *)(uintptr_t)dword_53C5B,
                      (char *)(uintptr_t)dword_53C63,
                      (char *)(uintptr_t)dword_53C5F);
    }

    memmove((void *)(uintptr_t)VGA_P(0xA0000u),
            (const void *)(uintptr_t)dword_53C5F, 0xFA00u);

    guest_free((void *)(uintptr_t)dword_53C5B);
    guest_free((void *)(uintptr_t)dword_53C5F);
    guest_free((void *)(uintptr_t)dword_53C63);
}

/* 0x1D51D - one key step of the action menu. */
int action_menu_select(int rec)
{
    const uint8_t *r = (const uint8_t *)dword_53A45 + 80u * (uint32_t)rec;
    uint8_t list[32];
    int count, last, key;

    action_menu_draw(rec, (int)dword_53C57, VGA_P(655360u));

    count = rec_collect_slot_bits(rec, NULL);
    last  = count - 1;
    dlg_wait_key(0);
    key = byte_53A8E;

    switch (key) {
    case 0x48:                                  /* up                        */
        svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
        if (dword_53C57 != 0)
            --dword_53C57;
        else
            dword_53C57 = (uint32_t)last;
        return 0;
    case 0x50:                                  /* down                      */
        svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
        if (last == (int)dword_53C57)
            dword_53C57 = 0;
        else
            ++dword_53C57;
        return 0;
    case 0x4b:                                  /* left                      */
        if ((int)dword_53C57 >= 4) {
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
            dword_53C57 -= 4;
        }
        return 0;
    case 0x4d:                                  /* right                     */
        if (count - 4 > (int)dword_53C57) {
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
            dword_53C57 += 4;
        }
        return 0;
    case 0x1c:                                  /* Enter                     */
    case 0x39:                                  /* Space                     */
        rec_collect_slot_bits(rec, list);
        if (*(const uint16_t *)(const void *)(r + 68)
                < action_entry(list[dword_53C57])[5])
            return 0;
        return 1;
    case 0x01:                                  /* ESC                       */
        return -1;
    default:
        return 0;
    }
}

/* 0x1CFF0 - the action-menu driver: open the record's command panel, run the
 * selection loop (action_menu_select) and execute the chosen action.  See
 * action_menu.h for the decoded behaviour and the decompiler artifacts. */
int action_menu_run(int rec)
{
    uint8_t list[100];      /* machine v19: target list / figure work area  */
    uint8_t scripts[100];   /* machine v20: +49 3-byte script entries       */
    uint8_t slots[12];      /* machine v21: rec_collect_slot_bits output    */
    const uint8_t *e;       /* selected action's 7-byte 0x619FD entry       */
    int i, sel, choice, step, count, v9, v15, lo, hi;

    dword_53C5B = guest_malloc(0xFA00u);
    dword_53C5F = guest_malloc(0xFA00u);
    dword_53C63 = guest_malloc(0xFA00u);
    memmove((void *)(uintptr_t)dword_53C5F,
            (const void *)VGA_P(0xA0000u), 0xFA00u);
    memmove((void *)(uintptr_t)dword_53C63,
            (const void *)(uintptr_t)dword_53C5F, 0xFA00u);

    box_open_init(rec, dword_53C63);
    action_menu_draw(rec, -1, dword_53C63);
    for (i = 11; i >= 0; --i) {
        fb_slide_step(i, (char *)(uintptr_t)dword_53C5B,
                      (char *)(uintptr_t)dword_53C63,
                      (char *)(uintptr_t)dword_53C5F);
    }

    dword_53C57 = 0;
    do {
        choice = action_menu_select(rec);
    } while (choice == 0);

    for (i = 0; i <= 11; ++i) {
        fb_slide_step(i, (char *)(uintptr_t)dword_53C5B,
                      (char *)(uintptr_t)dword_53C63,
                      (char *)(uintptr_t)dword_53C5F);
    }
    memmove((void *)VGA_P(0xA0000u), (const void *)(uintptr_t)dword_53C5F, 0xFA00u);
    guest_free((void *)(uintptr_t)dword_53C5B);
    guest_free((void *)(uintptr_t)dword_53C5F);
    guest_free((void *)(uintptr_t)dword_53C63);

    if (choice == -1)
        return -1;

    rec_collect_slot_bits(rec, slots);
    sel = (int)dword_53C57;
    e = action_entry(slots[sel]);
    dword_51A83 = e[4] + 2;

    lo = (int)qword_53AB1;                  /* target cell x (qword low)    */
    hi = (int)(qword_53AB1 >> 32);          /* target cell y (qword high)   */

    if (e[3] != 0 && slots[sel] == 0x17) {
        step = map_reveal_reachable(lo, hi, list, e[3], 1, e[6]);
        v9 = map_pick_record(e[6], step, list);
        util_fix_records(dword_53A51);
        /* The picker moved the map cursor onto the chosen cell; the original
         * reads qword_53AB1 live here, so the effect range is centred on the
         * TARGET, not the actor's pre-pick cell. */
        count = map_reveal_reachable((int)qword_53AB1,
                                     (int)(qword_53AB1 >> 32),
                                     list, e[4], 0, e[6]);
        util_fix_records(dword_53A51);
        if (v9 != -1)
            v9 = map_pick_record(6, list[0], 0);
        if (v9 != -1) {
            qword_51CF9 = qword_53AB1;
            dword_51A83 = 0;
            dlg_portrait_glide(rec);
            dword_51A83 = 1;
        }
    } else if (e[3] == 0) {
        int radius = dword_51A83 - 2;
        dword_51A83 = 1;
        count = map_reveal_reachable(lo, hi, list, radius, 0, 0);
        v15 = 4;
        if (count == 0)
            v15 = 5;
        v9 = map_pick_record(v15, 0, list);
    } else {
        step = map_reveal_reachable(lo, hi, list, e[3], 0, e[6]);
        v9 = map_pick_record(e[6], step, list);
        util_fix_records(dword_53A51);
        /* As above: ref/target uses the live cursor (the picked cell); only
         * map_scan_records' start pair stays at the pre-pick origin. */
        if (slots[sel] == 0x1E)
            count = map_scan_records((int)qword_53AB1,
                                     (int)(qword_53AB1 >> 32),
                                     list, lo, hi, e[3] - 16, 1);
        else
            count = map_reveal_reachable((int)qword_53AB1,
                                         (int)(qword_53AB1 >> 32),
                                         list, e[4], 0, e[6]);
    }

    util_fix_records(dword_53A51);
    if (v9 == -1) {
        dword_51A83 = 0;
        dlg_portrait_glide(rec);
        dword_51A83 = 1;
        return 0;
    }

    dword_51A83 = 0;
    map_view_update(0);
    if (slots[sel] < 9u || slots[sel] == 0x18u || slots[sel] > 0x1Bu) {
        figure_effect_play(rec, slots[sel], count, list);
    } else {
        ev5_1D4CB();
        pal_flash_entry(slots[sel]);
        ((int (*)(int, int, const uint8_t *))funcs_1541F[slots[sel]])
            (rec, count, list);
        menu_stop_and_free_music();
    }

    step = map_collect_scripts(scripts);
    map_units_animate();
    ev_script_run(rec, step, scripts);
    dword_51A83 = 1;
    return 1;
}
