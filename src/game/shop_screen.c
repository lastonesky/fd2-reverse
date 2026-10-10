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

    dword_53C5B = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);
    dword_53C5F = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);
    dword_53C63 = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);

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

    dword_53C5B = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);
    dword_53C5F = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);
    dword_53C63 = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);

    memmove((void *)(uintptr_t)dword_53C5F, (const void *)(uintptr_t)0xA0000u,
            0xFA00u);
    memmove((void *)(uintptr_t)dword_53C63, (const void *)(uintptr_t)dword_53C5F,
            0xFA00u);

    dword_53F4E = 0;
    dword_53C57 = 0;
    dword_53F62 = (uint32_t)(uintptr_t)members;
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

    dword_53C5B = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);
    dword_53C5F = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);
    dword_53C63 = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);

    memmove((void *)(uintptr_t)dword_53C5F, (const void *)(uintptr_t)0xA0000u,
            0xFA00u);
    memmove((void *)(uintptr_t)dword_53C63, (const void *)(uintptr_t)dword_53C5F,
            0xFA00u);

    dword_53F4E = 0;
    dword_53C57 = 0;
    dword_53F62 = (uint32_t)(uintptr_t)members;
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
