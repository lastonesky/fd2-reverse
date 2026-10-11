/* item_menu.c - the map "use an item on a party member" screen helpers.
 * See item_menu.h.  Direct translations of the machine code at the stated
 * addresses; the offline disassembly (analysis/cseg01_disasm.txt) is
 * authoritative for the argument order, the constants and the branches. */
#include "fd2_game.h"
#include "item_menu.h"

#include "../dos.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BDA_W(off) (*(volatile uint16_t *)(uintptr_t)(DOS_LOWMEM_BASE + (off)))

#define VGA_FRAME fd2_vga /* the visible 320x200 frame                    */
#define WORK_SIZE 0x1F400u /* 640-stride figure work buffer                */
#define SCR_SIZE  0xFA00u  /* 320-stride screen snapshot                   */

/* 0x2AE0E - collect the usable party members.  A record qualifies when its
 * byte +33 is >= 0x14, its unit type (byte +7) is < 0x12 and not 7.  The record
 * number goes to `records`, the menu kind code to `kinds`: type+32, promoted to
 * type+50 when the item applies to the record (ev3_2AEDB with byte_523D5[type]
 * succeeds), and 52 for a valid type-9 reaction.  Returns the count. */
int item_menu_list(uint8_t *records, uint8_t *kinds)
{
    uint8_t n = 0;
    int i;

    for (i = 0; i < (int)dword_53BFB; ++i) {
        const uint8_t *rec = (const uint8_t *)dword_53A45 + 80u * (uint32_t)i;
        int type = rec[7];

        if (rec[33] >= 0x14 && rec[7] < 0x12 && type != 7) {
            records[n] = (uint8_t)i;
            kinds[n]   = (uint8_t)(type + 32);
            if (ev3_2AEDB(i, byte_523D5[type]) != -1)
                kinds[n] = (uint8_t)(type + 50);
            if (type == 9 && ev3_2AEDB(i, 90) != -1)
                kinds[n] = 52;
            ++n;
        }
    }
    return n;
}

/* 0x2AC7D - use the current item on record `rec`.  Opens the talking portrait,
 * runs the item vm script 595 (page address VGA_P(0xA951Fu), blits the DATO box and
 * pages through the five dialogue pages 490..494 at record +0x37/+0x39/+0x3e/
 * +0x42/+0x46, each driving the next.  When the record's tbl_615FE entry has a
 * reaction byte the second script 596 is played at 6080*page + VGA_P(0xA951Fu) and the
 * record's byte +0x3b advances by it.  Finally folds the equipment metrics and
 * resets the record's item state (byte +0x21/+0x3c, words +0x40/+0x44 from the
 * maxima at +0x42/+0x46). */
void item_menu_use(int rec)
{
    uint8_t *r = (uint8_t *)dword_53A45 + 80u * (uint32_t)rec;
    const uint8_t *tbl = (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x20A1],
                                                  11, r[7], 0);
    const uint8_t *lbl;
    int page;

    kbd_flush();
    msg_open_portrait(r[7]);
    dword_53AD9 = (uint32_t)r[0x20] + 150u;
    vm_run(dword_53A7D, 595,VGA_P(693535), 320, 205, 76, 74, 19, 1);
    dlg_blit_dato(dword_53A85, dword_53C67, 0);
    kbd_flush();

    page = msg_show_page((uint16_t *)(void *)(r + 0x37), tbl, 490, 1);
    page = msg_show_page((uint16_t *)(void *)(r + 0x39), tbl + 2, 491, page);
    page = msg_show_page((uint16_t *)(void *)(r + 0x3e), tbl + 4, 492, page);
    page = msg_show_page((uint16_t *)(void *)(r + 0x42), tbl + 6, 493, page);
    page = msg_show_page((uint16_t *)(void *)(r + 0x46), tbl + 8, 494, page);

    lbl = (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x15FE], 2, r[7], -64);
    if (lbl[1] != 0) {
        dword_53AE1 = lbl[1];
        vm_run(dword_53A7D, 596,VGA_P(6080 * page + 693535), 320, 205, 76, 74, 19, 1);
        dlg_wait_key(0);
        r[0x3b] = (uint8_t)(r[0x3b] + (uint8_t)dword_53AE1);
    }

    unit_metrics(rec);
    msg_close_portrait();
    r[0x21] = 1;
    r[0x3c] = 0;
    *(uint16_t *)(void *)(r + 0x40) = *(const uint16_t *)(const void *)(r + 0x42);
    *(uint16_t *)(void *)(r + 0x44) = *(const uint16_t *)(const void *)(r + 0x46);
    kbd_flush();
}

/* 0x2FB2C - the item-use figure animation.  Loads the three BG.DAT strips into
 * dword_5413F and the user's (record `rec`) and target (`group`) FIGANI.DAT
 * groups, then cycles the strips while stamping the figure frames and fading,
 * ending with the target's group and a restore of the saved frame. */
void item_menu_figure_play(int rec, int group)
{
    const uint8_t *r = (const uint8_t *)dword_53A45 + 80u * (uint32_t)rec;
    uint8_t *work;
    void *fig_user, *fig_target, *saved;
    int i, j;
    int strip = 0;

    dword_5413F[0] = 0;
    dword_54143 = 0;
    dword_54147 = 0;
    dword_5413F[0] = res_load(aBgDat, NULL, 0);
    dword_54143 = res_load(
        aBgDat, (void *)(uintptr_t)dword_54143, 1);
    dword_54147 = res_load(
        aBgDat, (void *)(uintptr_t)dword_54147, 2);

    work = (uint8_t *)guest_malloc(WORK_SIZE);
    fig_user   = res_load(aFiganiDat, NULL, 3 * (int)r[7]);
    fig_target = res_load(aFiganiDat, NULL, 3 * group);
    saved = guest_malloc(SCR_SIZE);
    memmove(saved, (const void *)(uintptr_t)VGA_FRAME, SCR_SIZE);

    pal_fade_out();
    svc_wait_ticks(1);

    for (i = 8; i >= 0; --i) {
        memset(work, 0, WORK_SIZE);
        if (BDA_W(0x46C) != 0)
            strip = (strip + 1) % 3;
        rle_decode((const void *)(uintptr_t)dword_5413F[strip], 0, 50, work,
                   640, -1);
        res_blit(fig_user, 0, work + 10 * i, 640, -1);
        gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, work, 640, 320, 200);
        pal_fade_range(0, 255, 6 * i);
    }

    figure_frame_run(fig_user, work, 16);

    for (j = 0; j < 20; ++j) {
        pal_fade_add(0, 255, 3 * j);
        j___delay(10);
    }

    memset(work, 0, WORK_SIZE);
    rle_decode((const void *)(uintptr_t)dword_5413F[0], 0, 50, work, 640, -1);
    res_blit(fig_target, 0, work + 10 * j, 640, -1);
    gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, work, 640, 320, 200);
    pal_fade_range(0, 255, 0);
    figure_frame_run(fig_target, work, 24);
    pal_fade_out();
    memmove((void *)(uintptr_t)VGA_FRAME, saved, SCR_SIZE);
    pal_fade_in();

    guest_free(saved);
    guest_free((void *)(uintptr_t)dword_5413F[0]);
    guest_free((void *)(uintptr_t)dword_54143);
    guest_free((void *)(uintptr_t)dword_54147);
    guest_free(fig_user);
    guest_free(fig_target);
    guest_free(work);
}

/* 0x2AA00 - the item-use screen driver (the caller of the three helpers above).
 *
 * Loop: collect the eligible party members, play the empty script 591 and
 * return when there are none; otherwise open the portrait, play script 592 and
 * let the player pick a member with the equipment party list.  On a cancel the
 * loop returns; on a confirm it re-opens the portrait, plays script 594, waits
 * inside the map window and -- only when the cursor row is 0 -- applies the
 * chosen item: remove the used slot, play the figure animation, stamp the
 * record's item byte +0x20 and unit-type byte +7 from the tbl_615FE(kind) row,
 * rebuild the FDICON.B24 atlas and re-run item_menu_use.  The machine's return
 * is its residual EAX and no caller reads it, so this returns 0. */
int item_menu_run(void)
{
    uint8_t kinds[32];
    uint8_t records[32];
    const uint8_t *party = (const uint8_t *)(uintptr_t)dword_53BF7;
    FILE *fp;
    int count, v2, v5;
    int v3, cursor_kind;
    uint8_t *rec;
    int i;

    for (;;) {
        count = item_menu_list(records, kinds);
        if (count == 0) {
            msg_open_portrait(byte_5238B);
            vm_run(dword_53A7D, 591,VGA_P(693452), 320, 205, 76, 74, 19, 1);
            dlg_blit_dato(dword_53A85, dword_53C67, 0);
            dlg_wait_key(0);
            msg_close_portrait();
            break;
        }

        msg_open_portrait(byte_5238B);
        vm_run(dword_53A7D, 592,VGA_P(693452), 320, 205, 76, 74, 19, 1);
        dlg_blit_dato(dword_53A85, dword_53C67, 0);
        dlg_wait_key(1);
        msg_close_portrait();
        kbd_flush();

        v2 = shop_party_equip_run(count, records, kinds);
        msg_close_portrait();
        if (v2 == -1)
            break;

        msg_open_portrait(byte_5238B);
        v3          = records[dword_53C57];
        cursor_kind = kinds[dword_53C57];
        rec = (uint8_t *)dword_53A45 + 80u * (uint32_t)v3;
        dword_53AD9 = (uint32_t)rec[7] + 1u;

        vm_run(dword_53A7D, 594,VGA_P(693452), 320, 205, 76, 74, 19, 1);
        kbd_flush();
        v5 = map_window_run();
        map_draw_text_window();
        msg_close_portrait();
        if (v5 == -1)
            continue;
        if (dword_53C57 != 0)
            continue;

        if (cursor_kind == 52)
            rec_slot_remove(v3, ev3_2AEDB(v3, 90));
        else if (cursor_kind >= 50)
            rec_slot_remove(v3, ev3_2AEDB(v3, byte_523D5[rec[7]]));

        bgm_play(16, 1);
        item_menu_figure_play(v3, cursor_kind);
        bgm_play(11, 0);

        rec[0x20] = *(const uint8_t *)tbl_ptr((void *)&fd2_data2[0x15FE], 2,
                                              cursor_kind, -64);
        rec[7] = (uint8_t)cursor_kind;

        if (dword_53A61 != NULL)
            guest_free(dword_53A61);          /* free `buf` */
        fp = fopen(aFdiconB24_4, aRb_8);      /* "FDICON.B24","rb" */
        dword_53BDF = 0;
        for (i = 0; i < (int)dword_53BFB; ++i)
            unit_entry_data(party[80 * i + 7], fp);
        fclose(fp);

        item_menu_use(v3);
        kbd_flush();
    }
    return 0;
}
