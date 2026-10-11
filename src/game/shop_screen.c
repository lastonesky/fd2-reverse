/* shop_screen.c - the interactive list screens of the shop/place menu.
 * See shop_screen.h for the family, the ABI and the shared scratch buffers.
 *
 * Direct translations of the machine code at 0x275E6 / 0x27738 / 0x2A29D /
 * 0x2A857; the offline disassembly (analysis/cseg01_disasm.txt) is
 * authoritative.
 *
 *   - the Watcom stack probe (`push size; call 0x3702F`) is dropped; it does
 *     not allocate a frame (0x3702F restores ESP itself), so the arguments sit
 *     where the raw `[esp+N]` reads say;
 *   - 0x2A29D / 0x2A857 end with `jmp 0x26A73`, the shared epilogue
 *     (`mov eax,esi; pop ebp; pop edi; pop esi; pop ebx; ret`) matching their
 *     own four-register frame, i.e. a plain `return` of EAX;
 *   - the visible globals dword_53C57 (cursor) and dword_53F4E (scroll window)
 *     are 32-bit, so the signed `jl/jle/jge` tests of the machine are written
 *     with explicit int32_t casts;
 *   - the three 0xFA00 buffers are taken from the guest heap (guest_malloc),
 *     never the host heap, because pending machine-code callers share them.
 */
#include "fd2_game.h"
#include "shop_screen.h"
#include "shop_rows.h"

#include <stdint.h>
#include <string.h>

/* 0x27738 */
void shop_items_paint(int count, const uint8_t *items, uint8_t half_price)
{
    int i;

    dword_53C5B = guest_malloc(0xFA00u);
    dword_53C5F = guest_malloc(0xFA00u);
    dword_53C63 = guest_malloc(0xFA00u);

    memmove((void *)(uintptr_t)dword_53C5F, (const void *)(uintptr_t)0xA0000u,
            0xFA00u);
    memmove((void *)(uintptr_t)dword_53C63, (const void *)(uintptr_t)dword_53C5F,
            0xFA00u);

    /* panel sub-image at dword_53F66 + [dword_53F66+70], blitted at +35845. */
    rle2_blit((void *)(uintptr_t)(dword_53C63 + 35845u),
              (const void *)(uintptr_t)
                  (dword_53F66 +
                   *(const uint32_t *)(uintptr_t)(dword_53F66 + 70u)),
              320);

    shop_item_rows(count, items, (int)dword_53C57, (int)dword_53C63,
                   half_price);

    for (i = 5; i >= 0; --i)
        msg_blit_band(13 * i + 112, (void *)(uintptr_t)dword_53C5B,
                      (void *)(uintptr_t)dword_53C63);
}

/* 0x275E6 */
int shop_items_run(int count, const uint8_t *items, uint8_t half_price)
{
    int ret = 0;

    do {
        switch (status_screen_run(1)) {
        case 77:                                  /* right */
            if ((int32_t)count - 1 == (int32_t)dword_53C57)
                break;
            svc_play_sfx(dword_53EEC, 0, 1);
            ++dword_53C57;
            goto scroll_down;
        case 75:                                  /* left  */
            if (dword_53C57 == 0)
                break;
            svc_play_sfx(dword_53EEC, 0, 1);
            --dword_53C57;
            goto scroll_up_tail;
        case 72:                                  /* up    */
            if ((int32_t)dword_53C57 < 2)
                break;
            svc_play_sfx(dword_53EEC, 0, 1);
            dword_53C57 -= 2;
        scroll_up_tail:
            if ((int32_t)dword_53C57 < (int32_t)dword_53F4E) {
                dword_53F4E -= 2;
                screen_band_backward();
            }
            goto repaint;
        case 80:                                  /* down  */
            if ((int32_t)(count - 2) <= (int32_t)dword_53C57)
                break;
            svc_play_sfx(dword_53EEC, 0, 1);
            dword_53C57 += 2;
        scroll_down:
            if ((int32_t)(dword_53C57 - dword_53F4E) >= 6) {
                dword_53F4E += 2;
                screen_band_forward();
            }
        repaint:
            shop_item_rows(count, items, (int)dword_53C57, 0xA0000u,
                           half_price);
            break;
        case 28:                                  /* Enter */
        case 57:                                  /* Space */
            ret = 1;
            break;
        case 1:                                   /* Esc   */
            ret = -1;
            break;
        default:
            break;
        }
    } while (ret == 0);

    return ret;
}

/* 0x2A29D */
int shop_party_run(int count, const uint8_t *members)
{
    int ret = 0;
    int i;

    dword_53C5B = guest_malloc(0xFA00u);
    dword_53C5F = guest_malloc(0xFA00u);
    dword_53C63 = guest_malloc(0xFA00u);

    memmove((void *)(uintptr_t)dword_53C5F, (const void *)(uintptr_t)0xA0000u,
            0xFA00u);
    memmove((void *)(uintptr_t)dword_53C63, (const void *)(uintptr_t)dword_53C5F,
            0xFA00u);

    dword_53F4E = 0;
    dword_53C57 = 0;
    dword_53F62 = members;
    dword_53F5E = (uint32_t)count;

    /* panel sub-image at dword_53F66 + [dword_53F66+70], blitted at +35845. */
    rle2_blit((void *)(uintptr_t)(dword_53C63 + 35845u),
              (const void *)(uintptr_t)
                  (dword_53F66 +
                   *(const uint32_t *)(uintptr_t)(dword_53F66 + 70u)),
              320);

    shop_party_bar_rows(count, (int)dword_53C63, (int)dword_53C57, members);

    for (i = 5; i >= 0; --i)
        msg_blit_band(13 * i + 112, (void *)(uintptr_t)dword_53C5B,
                      (void *)(uintptr_t)dword_53C63);

    do {
        switch (status_screen_run(2)) {
        case 72:                                  /* up   */
            if (dword_53C57 == 0)
                break;
            --dword_53C57;
            if ((int32_t)dword_53C57 < (int32_t)dword_53F4E) {
                --dword_53F4E;
                screen_band_backward();
            }
        repaint:
            shop_party_bar_rows(count, 0xA0000u, (int)dword_53C57, members);
            break;
        case 80:                                  /* down */
            if ((int32_t)(count - 1) <= (int32_t)dword_53C57)
                break;
            ++dword_53C57;
            if ((int32_t)(dword_53C57 - dword_53F4E) >= 3) {
                ++dword_53F4E;
                screen_band_forward();
            }
            goto repaint;
        case 28:                                  /* Enter */
        case 57:                                  /* Space */
            ret = 1;
            break;
        case 1:                                   /* Esc   */
            ret = -1;
            break;
        default:
            break;
        }
    } while (ret == 0);

    return ret;
}

/* 0x2A857 */
int shop_party_equip_run(int count, const uint8_t *members,
                         const uint8_t *equip)
{
    int ret = 0;
    int i;

    dword_53C5B = guest_malloc(0xFA00u);
    dword_53C5F = guest_malloc(0xFA00u);
    dword_53C63 = guest_malloc(0xFA00u);

    memmove((void *)(uintptr_t)dword_53C5F, (const void *)(uintptr_t)0xA0000u,
            0xFA00u);
    memmove((void *)(uintptr_t)dword_53C63, (const void *)(uintptr_t)dword_53C5F,
            0xFA00u);

    dword_53F4E = 0;
    dword_53C57 = 0;
    dword_53F62 = members;
    dword_53F5E = (uint32_t)count;

    /* panel sub-image at dword_53F66 + [dword_53F66+70], blitted at +35845. */
    rle2_blit((void *)(uintptr_t)(dword_53C63 + 35845u),
              (const void *)(uintptr_t)
                  (dword_53F66 +
                   *(const uint32_t *)(uintptr_t)(dword_53F66 + 70u)),
              320);

    shop_party_equip_rows(count, (int)dword_53C63, (int)dword_53C57, members,
                          equip);

    for (i = 5; i >= 0; --i)
        msg_blit_band(13 * i + 112, (void *)(uintptr_t)dword_53C5B,
                      (void *)(uintptr_t)dword_53C63);

    do {
        switch (status_screen_run(2)) {
        case 72:                                  /* up   */
            if (dword_53C57 == 0)
                break;
            --dword_53C57;
            if ((int32_t)dword_53C57 < (int32_t)dword_53F4E) {
                --dword_53F4E;
                screen_band_backward();
            }
        repaint:
            shop_party_equip_rows(count, 0xA0000u, (int)dword_53C57, members,
                                  equip);
            break;
        case 80:                                  /* down */
            if ((int32_t)(count - 1) <= (int32_t)dword_53C57)
                break;
            ++dword_53C57;
            if ((int32_t)(dword_53C57 - dword_53F4E) >= 3) {
                ++dword_53F4E;
                screen_band_forward();
            }
            goto repaint;
        case 28:                                  /* Enter */
        case 57:                                  /* Space */
            ret = 1;
            break;
        case 1:                                   /* Esc   */
            ret = -1;
            break;
        default:
            break;
        }
    } while (ret == 0);

    return ret;
}

/* 0x28CBD */
int shop_sell_run(void)
{
    int16_t v9[6];
    int16_t v10[6];
    uint8_t items[20];
    int cursor;
    int n;
    int i;
    int slot;
    uint8_t *rec;
    const uint8_t *it;
    int item;

    memcpy(v9, &fd2_data1[0x2458], sizeof v9);
    memcpy(v10, &fd2_data1[0x2464], sizeof v10);

    for (;;) {
        dword_53F5E = dword_53BFB;
        if (menu_list_select() == -1)
            return -1;

        cursor = (int)dword_53C57;
        rec = (uint8_t *)(uintptr_t)dword_53A45 + 80u * (uint32_t)cursor;

        n = 0;
        for (i = 0; i < 8; ++i) {
            if (rec[2 * i + 10] < 0x80)
                items[n++] = rec[2 * i + 11];
        }

        if (n == 0) {
            dword_53AD9 = (uint32_t)rec[7] + 1;
            msg_open_portrait(byte_52387[dword_53F4A]);
            vm_run(dword_53A7D, v10[dword_53F4A], 0xA94CC, 320, 205, 76, 74,
                   19, 1);
            dlg_blit_dato(dword_53A85, dword_53C67, 0);
            dlg_wait_key(1);
            msg_close_portrait();
            continue;
        }

        dword_53C57 = 0;
        dword_53F4E = 0;
        shop_items_paint(n, items, 1);
        dword_53F5E = (uint32_t)n;
        if (shop_items_run(n, items, 1) == -1) {
            msg_close_portrait();
            continue;
        }

        slot = (int)dword_53C57;
        msg_close_portrait();

        item = rec_field_byte(cursor, (int)dword_53C57);
        dword_53AD9 = (uint32_t)(item + 181);
        it = (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x2AD], 23, item, 0);
        dword_53AE1 = (int32_t)((3u * *(const uint16_t *)(const void *)(it + 19))
                                >> 2);

        msg_open_portrait(byte_52387[dword_53F4A]);
        vm_run(dword_53A7D, v9[dword_53F4A], 0xA94CC, 320, 205, 76, 74, 19, 1);
        dlg_blit_dato(dword_53A85, dword_53C67, 0);
        if (map_window_run() == -1 || dword_53C57 == 1) {
            msg_close_portrait();
            continue;
        }
        msg_close_portrait();
        place_transition();
        place_money_gain((int)dword_53AE1);
        rec_slot_remove(cursor, slot);
        unit_metrics(cursor);
    }
}

/* 0x28EFE */
int shop_box_run(void)
{
    int save;

    for (;;) {
        dword_53F5E = dword_53BFB;
        if (menu_list_select() == -1)
            return -1;

        save = dword_53C67;
        box_run((int)dword_53C57);
        dword_53C67 = save;
        dword_53A85 = res_load(aDatoDat, dword_53A85,
                               byte_52387[dword_53F4A]);
    }
}

/* 0x2A07A */
int shop_usable_items(uint8_t *dst)
{
    int i;
    uint8_t n = 0;

    for (i = 0; i < (int)dword_53BFB; ++i) {
        if (rec_flag(i) == 1)
            dst[n++] = (uint8_t)i;
    }
    return n;
}

/* 0x2A43E */
int shop_buy_run(void)
{
    uint8_t items[32];
    int count;
    uint8_t *rec;
    int idx;

    for (;;) {
        count = shop_usable_items(items);
        if (count == 0) {
            msg_open_portrait(byte_5238B);
            vm_run(dword_53A7D, 588, 0xA94CC, 320, 205, 76, 74, 19, 1);
            dlg_blit_dato(dword_53A85, dword_53C67, 0);
            dlg_wait_key(0);
            msg_close_portrait();
            return 0;
        }

        msg_open_portrait(byte_5238B);
        vm_run(dword_53A7D, 589, 0xA94CC, 320, 205, 76, 74, 19, 1);
        dlg_blit_dato(dword_53A85, dword_53C67, 0);
        dlg_wait_key(1);
        msg_close_portrait();

        if (shop_party_run(count, items) == -1)
            return 0;
        msg_close_portrait();

        msg_open_portrait(byte_5238B);
        idx = items[dword_53C57];
        rec = (uint8_t *)(uintptr_t)dword_53A45 + 80u * (uint32_t)idx;
        dword_53AD9 = (uint32_t)rec[8] + 1;
        dword_53AE1 = (int32_t)(int16_t)word_52397[rec[32]] * (int32_t)rec[33];

        vm_run(dword_53A7D, 590, 0xA94CC, 320, 205, 76, 74, 19, 1);
        if (map_window_run() == -1 || dword_53C57 != 0) {
            msg_close_portrait();
            continue;
        }
        if ((int32_t)dword_53BF3 < (int32_t)dword_53AE1) {
            vm_run(dword_53A7D, 504, 0xAC44C, 320, 205, 76, 74, 19, 1);
            dlg_blit_dato(dword_53A85, dword_53C67, 0);
            dlg_wait_key(1);
            msg_close_portrait();
            continue;
        }

        place_money_spend((int)dword_53AE1);
        rec[5] = 0;
        *(uint16_t *)(void *)(rec + 0x40) =
            *(const uint16_t *)(const void *)(rec + 0x42);
        rle2_blit((void *)(uintptr_t)(dword_53C5F + 30405u),
                  (const void *)(uintptr_t)
                      (dword_53F66 +
                       *(const uint32_t *)(uintptr_t)(dword_53F66 + 10u)),
                  320);
        dlg_draw_number((void *)(uintptr_t)(dword_53C5F + 31696u), 320,
                        (int)dword_53BF3, 31, 8);
        msg_close_portrait();
        bgm_play(17, 1);
        place_transition();
        bgm_play(11, 1);
    }
}

/* 0x2872B - the place menu's first option: the buy screen driven from the
 * place's stock list.  `count` item ids are read from `items` (the bytes
 * 0x26A0D fills) and run through the interactive list (shop_items_paint /
 * shop_items_run).  For the confirmed item its descriptor is read from the
 * 23-byte table 0x602AD (base 0x602AD): entry byte [0] classifies it and word
 * +19 is its price (dword_53AE1).  When the class byte is < 0x20 the candidate
 * members are those box_item_usable accepts, otherwise every party member
 * qualifies; the player picks one (shop_party_stat_run with the usable-item
 * preview, or menu_list_select when the list is universal) and, if the member
 * has a free slot and enough money, rec_slot_claim takes the slot,
 * unit_metrics folds the item in and place_money_spend deducts the price
 * after the place transition.  The four per-place label word tables at
 * 0x52428/0x52434/0x52440/0x5244C are copied into the 20-dword scratch block
 * (member list bytes at the front, the tables at +0x20/+0x2c/+0x38/+0x44) and
 * the columns of each are selected by dword_53F4A, exactly as the machine. */
int shop_place_buy_run(int count, const uint8_t *items)
{
    int32_t        v12[20];
    uint8_t       *list = (uint8_t *)v12;
    int32_t        saved_window;
    int            ret, n, i, window_ret, member, item;
    const uint8_t *glyph;

    memcpy(&v12[11], &fd2_data1[0x2428], 12);
    memcpy(&v12[17], &fd2_data1[0x2434], 12);
    memcpy(&v12[8], &fd2_data1[0x2440], 12);
    memcpy(&v12[14], &fd2_data1[0x244C], 12);

    for (;;) {
        dword_53C57 = dword_53F6A;
        dword_53F4E = dword_53F6E;
        shop_items_paint(count, items, 0);
        ret = shop_items_run(count, items, 0);
        dword_53F6A = dword_53C57;
        dword_53F6E = dword_53F4E;
        msg_close_portrait();
        if (ret == -1)
            return 0;

        item = items[dword_53F6A];
        dword_53AD9 = (uint32_t)item + 181u;
        glyph = (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x2AD], 23, item, 0);
        dword_53AE1 = *(const uint16_t *)(const void *)(glyph + 19);

        n = 0;
        for (i = 0; i < (int)dword_53BFB; ++i) {
            if ((glyph[0] < 0x20u && box_item_usable(i, item) == 1) ||
                glyph[0] >= 0x20u)
                list[n++] = (uint8_t)i;
        }

        if (glyph[0] < 0x20u && n == 0) {
            msg_open_portrait(byte_52387[dword_53F4A]);
            vm_run(dword_53A7D, *((int16_t *)&v12[8] + dword_53F4A), 0xA94CC,
                   320, 205, 76, 74, 19, 1);
            dlg_blit_dato(dword_53A85, dword_53C67, 0);
            dlg_wait_key(1);
            msg_close_portrait();
            continue;
        }

        saved_window = (int32_t)dword_53F5E;
        dword_53F5E = (uint32_t)n;
        dword_53F62 = list;
        msg_open_portrait(byte_52387[dword_53F4A]);
        vm_run(dword_53A7D, *((int16_t *)&v12[11] + dword_53F4A), 0xA94CC,
               320, 205, 76, 74, 19, 1);
        dlg_blit_dato(dword_53A85, dword_53C67, 0);
        window_ret = map_window_run();
        map_draw_text_window();
        if (window_ret == -1 || dword_53C57 == 1) {
            msg_close_portrait();
            continue;
        }
        if ((int32_t)dword_53BF3 < (int32_t)dword_53AE1) {
            vm_run(dword_53A7D, *((int16_t *)&v12[17] + dword_53F4A), 0xAC44C,
                   320, 205, 76, 74, 19, 1);
            dlg_blit_dato(dword_53A85, dword_53C67, 0);
            dlg_wait_key(1);
            msg_close_portrait();
            continue;
        }

        msg_close_portrait();
        if (glyph[0] >= 0x20u) {
            dword_53F5E = dword_53BFB;
            ret = menu_list_select();
        } else {
            ret = shop_party_stat_run(n, list, item);
        }
        dword_53F5E = (uint32_t)saved_window;
        msg_close_portrait();
        if (ret == 1) {
            member = list[dword_53C57];
            if (rec_slot_free(member) == 8) {
                dword_53AD9 = (uint32_t)(*((const uint8_t *)dword_53A45 +
                                           80u * (uint32_t)member + 7u)) + 1u;
                msg_open_portrait(byte_52387[dword_53F4A]);
                vm_run(dword_53A7D, word_5238D[dword_53F4A], 0xA94CC,
                       320, 205, 76, 74, 19, 1);
                dlg_blit_dato(dword_53A85, dword_53C67, 0);
                dlg_wait_key(1);
                msg_close_portrait();
                continue;
            }
            rec_slot_claim(member, item);
            if (glyph[0] < 0x20u) {
                msg_open_portrait(byte_52387[dword_53F4A]);
                vm_run(dword_53A7D, *((int16_t *)&v12[14] + dword_53F4A),
                       0xA94CC, 320, 205, 76, 74, 19, 1);
                dlg_blit_dato(dword_53A85, dword_53C67, 0);
                i = list[dword_53C57];
                window_ret = map_window_run();
                map_draw_text_window();
                if (window_ret != -1 && dword_53C57 == 0) {
                    box_item_select(i, rec_slot_free(i) - 1);
                    unit_metrics(i);
                }
                msg_close_portrait();
            }
            place_transition();
            place_money_spend((int)dword_53AE1);
        }
    }
}

/* 0x28F65 - the place menu's fourth option: hand one item from a chosen member
 * to another.  Pick the source member (menu_list_select over the party), list
 * that member's occupied slots (state byte bit 7 clear) and run the item list
 * (shop_items_paint/run); on confirm pick the receiving member and, unless all
 * eight of its slots are occupied, move the item across (rec_field_byte /
 * rec_slot_remove on the source, rec_slot_claim on the target) and refold the
 * source's metrics.  The shared epilogue at 0x29619 (add esp,8; pop
 * edi/esi/ebx; ret) is a plain return whose EAX no caller reads, so the
 * function is void; the only exit is the source-selection cancel. */
void shop_give_run(void)
{
    uint8_t  items[20];
    int      n;
    int      i;
    int      source;
    int      slot;
    int      v6;
    int      item;
    uint8_t *rec;

    for (;;) {
        msg_open_portrait(byte_52387[dword_53F4A]);
        vm_run(dword_53A7D, 512, 0xA94CC, 320, 205, 76, 74, 19, 1);
        dlg_blit_dato(dword_53A85, dword_53C67, 0);
        dlg_wait_key(1);
        msg_close_portrait();

        dword_53F5E = dword_53BFB;
        if (menu_list_select() == -1)
            return;
        source = (int)dword_53C57;
        rec = (uint8_t *)(uintptr_t)dword_53A45 + 80u * (uint32_t)source;

        n = 0;
        for (i = 0; i < 8; ++i) {
            if (rec[2 * i + 10] < 0x80)
                items[n++] = rec[2 * i + 11];
        }

        if (n == 0) {
            dword_53AD9 = (uint32_t)rec[8] + 1;
            msg_open_portrait(byte_52387[dword_53F4A]);
            vm_run(dword_53A7D, 511, 0xA94CC, 320, 205, 76, 74, 19, 1);
            dlg_blit_dato(dword_53A85, dword_53C67, 0);
            dlg_wait_key(1);
            msg_close_portrait();
            continue;
        }

        dword_53C57 = 0;
        dword_53F4E = 0;
        shop_items_paint(n, items, 1);
        dword_53F5E = (uint32_t)n;
        if (shop_items_run(n, items, 1) == -1) {
            msg_close_portrait();
            continue;
        }

        slot = (int)dword_53C57;
        msg_close_portrait();

        msg_open_portrait(byte_52387[dword_53F4A]);
        vm_run(dword_53A7D, 510, 0xA94CC, 320, 205, 76, 74, 19, 1);
        dlg_blit_dato(dword_53A85, dword_53C67, 0);
        dlg_wait_key(1);
        msg_close_portrait();

        v6 = menu_list_select();
        msg_close_portrait();
        if (v6 == -1)
            continue;

        if (rec_slot_free((int)dword_53C57) == 8) {
            dword_53AD9 =
                (uint32_t)(*((const uint8_t *)dword_53A45 +
                             80u * (uint32_t)dword_53C57 + 8u)) + 1u;
            msg_open_portrait(byte_52387[dword_53F4A]);
            vm_run(dword_53A7D, word_5238D[dword_53F4A], 0xA94CC,
                   320, 205, 76, 74, 19, 1);
            dlg_blit_dato(dword_53A85, dword_53C67, 0);
            dlg_wait_key(1);
            msg_close_portrait();
            continue;
        }

        item = rec_field_byte(source, slot);
        rec_slot_remove(source, slot);
        rec_slot_claim((int)dword_53C57, item);
        unit_metrics(source);
    }
}

/* 0x279BC - the place menu.  Stage the current place's FDOTHER sub-image
 * (index 12, or 29 / 63 when dword_53F4A is 3 / 5) into the live frame, draw
 * the money readouts to the frame and to the dword_53C5F scratch, and loop on
 * menu_choice4 over the four options, dispatching to the buy screen (0x2872B),
 * the sell screen (0x28CBD), the character box (0x28EFE) or the item-give
 * screen (shop_give_run).  On cancel it plays the closing zoom-out through
 * place_zoom_blit and frees the resource.  `src` is the surface that zoom-out
 * blits from.  Returns 1 when dword_53F4A is 0, else 0 (the machine's EAX;
 * caller 0x2670E is still pending). */
int place_menu_run(const char *src)
{
    uint8_t        items[12];
    const uint8_t *ent;
    const void    *panel;
    int            v1 = 0;
    int            v10 = 0;
    int            index;
    int            n;
    int            i;
    uint8_t        choice;
    uint8_t        place;

    /* 0x4E809 = table accessor: base 0x6238D, stride 31, offset -31 (tables.h). */
    ent = (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x238D], 31,
                                   (int)dword_53C03, -31);
    place = ent[0];

    index = 12;
    if (dword_53F4A == 3)
        index = 29;
    else if (dword_53F4A == 5)
        index = 63;

    dword_53F66 = res_load(aFdotherDat,
                    (void *)(uintptr_t)dword_53F66, index);
    res_blit6((void *)(uintptr_t)0xA0000u, 320,
              (void *)(uintptr_t)dword_53F66, 0);
    pal_fade_in();
    j___delay(200);
    msg_open_portrait(byte_52387[dword_53F4A]);

    /* panel sub-image at dword_53F66 + [dword_53F66+10], blitted at +30405. */
    panel = (const void *)(uintptr_t)
        (dword_53F66 +
         *(const uint32_t *)(uintptr_t)(dword_53F66 + 10u));
    rle2_blit((void *)(uintptr_t)0xA76C5u, panel, 320);
    dlg_draw_number((void *)(uintptr_t)0xA7BD0u, 320, (int)dword_53BF3, 31, 8);
    rle2_blit((void *)(uintptr_t)(dword_53C5F + 30405u), panel, 320);
    dlg_draw_number((void *)(uintptr_t)(dword_53C5F + 31696u), 320,
                    (int)dword_53BF3, 31, 8);

    vm_run(dword_53A7D, (dword_53F4A == 1) ? 501 : 440, 0xA94CC,
           320, 205, 76, 74, 19, 1);
    dlg_blit_dato(dword_53A85, dword_53C67, 0);
    dword_53F6A = 0;
    dword_53F6E = 0;

    for (;;) {
        dword_53C57 = (uint32_t)v1;
        screen_curtain(0);
        choice = (uint8_t)menu_choice4();
        if (choice == 1)
            v1 = (int)dword_53C57;               /* keep the chosen slot */
        screen_curtain(1);
        n = place_name_copy((char *)items);
        dword_53F5E = (uint32_t)n;
        msg_close_portrait();

        if (choice == 1) {
            if (dword_53C57 == 0) {
                shop_place_buy_run(n, items);
            } else if ((uint32_t)choice == dword_53C57) {
                shop_sell_run();
            } else if (dword_53C57 == 2) {
                shop_box_run();
            } else {
                shop_give_run();
            }
            msg_open_portrait(byte_52387[dword_53F4A]);
            vm_run(dword_53A7D, (dword_53F4A == 1) ? 503 : 440, 0xA94CC,
                   320, 205, 76, 74, 19, 1);
            dlg_blit_dato(dword_53A85, dword_53C67, 0);
        }

        if (choice != 1)
            break;
    }

    res_blit6((void *)(uintptr_t)0xA0000u, 320,
              (void *)(uintptr_t)dword_53F66, 0);
    j___delay(200);
    pal_fade_out();

    for (i = 10; i >= 0; --i) {
        /* the machine's shl eax,7 is *128 here (the signed idiv quotient can be
           negative; both give the same 32-bit result). */
        place_zoom_blit(
            ((i * ((int)byte_52363[6 * place + dword_53F4A] - 150) / 10) * 128)
                + 20480,
            ((i * ((int)byte_52375[6 * place + dword_53F4A] - 100) / 10) * 128)
                + 12800,
            src, 128 - 9 * i);
        memmove((void *)(uintptr_t)0xA0000u, dword_53A49, 0xFA00u);
        pal_fade_range(0, 255, 4 * i);
    }

    guest_free((void *)(uintptr_t)dword_53F66);
    dword_53F66 = 0;
    if (dword_53F4A == 0)
        v10 = 1;
    return v10;
}
