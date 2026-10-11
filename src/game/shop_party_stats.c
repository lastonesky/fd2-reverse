/* shop_party_stats.c - the shop/place "party stat preview" screen: the row
 * painter, its stat-totals helper and the interactive driver.  See
 * shop_party_stats.h for the family, the ABI and the stat offsets.
 *
 * Direct translations of the machine code at 0x28632 / 0x2825B / 0x27F4A; the
 * offline disassembly (analysis/cseg01_disasm.txt) is authoritative.
 *
 *   - the Watcom stack probe (`push size; call 0x3702F`) is dropped;
 *   - Hex-Rays drops sub_2860A's second argument (0x2860A compares two stack
 *     values: [esp+4] and [esp+8]); the machine pushes (current, preview) so
 *     the C passes both;
 *   - the one-argument table accessor 0x4E8BC becomes tbl_ptr with its
 *     base/stride from tables.h;
 *   - 0x27F4A ends with `jmp 0x26A73`, the shared epilogue (`mov eax,esi; pop
 *     ebp; pop edi; pop esi; pop ebx; ret`) matching its own four-register
 *     frame, i.e. a plain `return` of EAX (the 1 / -1 selection);
 *   - the three 0xFA00 buffers are taken from the guest heap (guest_malloc),
 *     never the host heap, because the sibling screens share them.
 */
#include "fd2_game.h"
#include "shop_party_stats.h"

#include <stdint.h>
#include <string.h>

/* item table accessor 0x4E8BC: base 0x602AD, stride 23 (dseg03). */
static const uint8_t *item_rec(int index)
{
    return (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x2AD], 23, index, 0);
}

/* unit sprite bank frame: bank + *(u32*)(bank + 4*(12*rec + frame)) */
static const void *unit_sprite(int rec, int frame)
{
    const uint8_t *bank = (const uint8_t *)(uintptr_t)dword_53A61;

    return bank + *(const int32_t *)(const void *)
                       (bank + 48u * (uint32_t)rec + 4u * (uint32_t)frame);
}

/* the dword_53F66 resource label sub-image at offset `off` */
static const void *label_sub(int off)
{
    const uint8_t *res = (const uint8_t *)(uintptr_t)dword_53F66;

    return res + *(const int32_t *)(const void *)(res + off);
}

/* 0x28632 */
void unit_stat_totals(int rec_index, int item, int out[4])
{
    const uint8_t *it = item_rec(item);
    int type = it[0];
    const uint8_t *rec = (const uint8_t *)dword_53A45
                         + 80u * (uint32_t)rec_index;
    int a0 = *(const int16_t *)(const void *)(it + 1)
             + *(const int16_t *)(const void *)(rec + 0x37);
    int a1 = *(const int16_t *)(const void *)(it + 5)
             + *(const int16_t *)(const void *)(rec + 0x39);
    int base = *(const int16_t *)(const void *)(rec + 0x3e);
    int a2 = base + *(const int16_t *)(const void *)(it + 3);
    int a3 = base + *(const int16_t *)(const void *)(it + 7);
    int i;

    for (i = 0; i < 8; i++) {
        const uint8_t *slot = rec + 2u * (uint32_t)i + 10u;
        const uint8_t *e = item_rec(slot[1]);

        if ((slot[0] & 0x40) != 0
            && ((type <= 20 && e[0] > 0x14)
                || (type > 20 && e[0] <= 0x14))) {
            a0 += *(const int16_t *)(const void *)(e + 1);
            a1 += *(const int16_t *)(const void *)(e + 5);
            a2 += *(const int16_t *)(const void *)(e + 3);
            a3 += *(const int16_t *)(const void *)(e + 7);
        }
    }

    out[0] = a0;
    out[1] = a1;
    out[2] = a2;
    out[3] = a3;
}

/* 0x2825B */
void shop_party_stat_rows(int count, const uint8_t *members, int item,
                          int cursor, int dst)
{
    int frame = (int)dword_53F72;
    int rows = count;
    int i;

    if (frame == 3)
        frame = 1;
    if (rows > 3)
        rows = 3;

    for (i = 0; i < rows; ++i) {
        int idx = members[i + dword_53F4E];
        const uint8_t *rec = (const uint8_t *)dword_53A45
                             + 80u * (uint32_t)idx;
        int totals[4];
        int cur0 = *(const uint16_t *)(const void *)(rec + 0x48);
        int cur1 = *(const uint16_t *)(const void *)(rec + 0x4A);
        int cur2 = *(const uint16_t *)(const void *)(rec + 0x4C);
        int cur3 = *(const uint16_t *)(const void *)(rec + 0x4E);
        int row  = 26 * i + 117;
        int fg   = (i + dword_53F4E == (uint32_t)cursor) ? 201 : 205;
        int v16  = 320 * (row + 4) + dst;
        int v11  = 320 * (row + 3) + dst;
        int v12  = 320 * (row + 12) + dst;
        int v17  = 320 * (row + 13) + dst;
        int color;

        unit_stat_totals(idx, item, totals);

        sprite24_plain49(unit_sprite(idx, frame),
                         (void *)(uintptr_t)(dst + 320 * row + 14), 320);

        vm_run(dword_53A7D, rec[8] + 1, v16 + 40, 320, fg, 76, 0, 0, 0);

        color = ev5_2860A(cur0, totals[0]);
        rle2_blit((void *)(uintptr_t)(v11 + 122), label_sub(78), 320);
        dlg_draw_number((void *)(uintptr_t)(v11 + 137), 320, cur0, color, 3);
        rle2_blit((void *)(uintptr_t)(v16 + 157), label_sub(94), 320);
        dlg_draw_number((void *)(uintptr_t)(v11 + 165), 320, totals[0], color, 3);

        color = ev5_2860A(cur1, totals[1]);
        rle2_blit((void *)(uintptr_t)(v12 + 122), label_sub(82), 320);
        dlg_draw_number((void *)(uintptr_t)(v12 + 137), 320, cur1, color, 3);
        rle2_blit((void *)(uintptr_t)(v17 + 157), label_sub(94), 320);
        dlg_draw_number((void *)(uintptr_t)(v12 + 165), 320, totals[1], color, 3);

        color = ev5_2860A(cur2, totals[2]);
        rle2_blit((void *)(uintptr_t)(v11 + 196), label_sub(86), 320);
        dlg_draw_number((void *)(uintptr_t)(v11 + 214), 320, cur2, color, 3);
        rle2_blit((void *)(uintptr_t)(v16 + 234), label_sub(94), 320);
        dlg_draw_number((void *)(uintptr_t)(v11 + 242), 320, totals[2], color, 3);

        color = ev5_2860A(cur3, totals[3]);
        rle2_blit((void *)(uintptr_t)(v12 + 196), label_sub(90), 320);
        dlg_draw_number((void *)(uintptr_t)(v12 + 214), 320, cur3, color, 3);
        rle2_blit((void *)(uintptr_t)(v17 + 234), label_sub(94), 320);
        dlg_draw_number((void *)(uintptr_t)(v12 + 242), 320, totals[3], color, 3);
    }
}

/* 0x27F4A */
int shop_party_stat_run(int count, const uint8_t *members, int item)
{
    int i;
    int ret = 0;

    dword_53C5B = guest_malloc(0xFA00u);
    dword_53C5F = guest_malloc(0xFA00u);
    dword_53C63 = guest_malloc(0xFA00u);

    memmove((void *)(uintptr_t)dword_53C5F, (const void *)(uintptr_t)0xA0000u,
            0xFA00u);
    memmove((void *)(uintptr_t)dword_53C63, (const void *)(uintptr_t)dword_53C5F,
            0xFA00u);

    dword_53F4E = 0;
    dword_53C57 = 0;

    /* panel sub-image at dword_53F66 + [dword_53F66+70], blitted at +35845. */
    rle2_blit((void *)(uintptr_t)(dword_53C63 + 35845u),
              (const void *)(uintptr_t)
                  (dword_53F66 +
                   *(const uint32_t *)(uintptr_t)(dword_53F66 + 70u)),
              320);

    shop_party_stat_rows(count, members, item, (int)dword_53C57,
                         (int)dword_53C63);

    for (i = 5; i >= 0; --i)
        msg_blit_band(13 * i + 112, (void *)(uintptr_t)dword_53C5B,
                      (void *)(uintptr_t)dword_53C63);

    do {
        switch (status_screen_run(2)) {
        case 80:                                  /* down */
            if ((int32_t)(count - 1) == (int32_t)dword_53C57)
                break;
            svc_play_sfx(dword_53EEC, 0, 1);
            ++dword_53C57;
            if ((int32_t)(dword_53C57 - dword_53F4E) >= 3) {
                ++dword_53F4E;
                screen_band_forward();
            }
        repaint:
            shop_party_stat_rows(count, members, item, (int)dword_53C57,
                                 0xA0000u);
            break;
        case 72:                                  /* up   */
            if (dword_53C57 == 0)
                break;
            svc_play_sfx(dword_53EEC, 0, 1);
            --dword_53C57;
            if ((int32_t)dword_53C57 < (int32_t)dword_53F4E) {
                --dword_53F4E;
                screen_band_backward();
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
