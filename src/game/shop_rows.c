/* shop_rows.c - the row painters of the shop/place menu screens.
 * See shop_rows.h for the family, the ABI and the row geometry.
 *
 * Direct translations of the machine code at 0x272D0 / 0x2A0C2 / 0x2A694; the
 * offline disassembly (analysis/cseg01_disasm.txt) is authoritative.
 *
 *   - the Watcom stack probe (`push size; call 0x3702F`) is dropped;
 *   - 0x2A0C2's tail `jmp 0x26E30` is a shared epilogue (`add esp,0x14; pop
 *     ebp/edi/esi/ebx; ret`) that matches its own frame, i.e. a plain return;
 *   - the Hex-Rays `int` return is whatever the last callee leaves in eax and
 *     no caller reads it, so all three return void;
 *   - the one-argument table accessors 0x4E8BC / 0x4E7DD become tbl_ptr with
 *     their base/stride/offset from tables.h.
 */
#include "fd2_game.h"
#include "shop_rows.h"

#include <stdint.h>

/* item table accessor 0x4E8BC: base 0x602AD, stride 23 (dseg03). */
static const uint8_t *shop_item_rec(int index)
{
    return (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x2AD], 23, index, 0);
}

/* equipment-label accessor 0x4E7DD: base 0x615FE, stride 2, (index-0x20)*2. */
static const uint8_t *shop_equip_rec(int index)
{
    return (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x15FE], 2, index, -64);
}

/* unit sprite bank frame: bank + *(u32*)(bank + 4*(12*rec + frame)) */
static const void *shop_unit_sprite(int rec, int frame)
{
    const uint8_t *bank = (const uint8_t *)(uintptr_t)dword_53A61;

    return bank + *(const int32_t *)(const void *)
                       (bank + 48u * (uint32_t)rec + 4u * (uint32_t)frame);
}

/* 0x272D0 */
void shop_item_rows(int count, const uint8_t *items, int cursor, uint8_t *dst,
                    uint8_t half_price)
{
    int rows = count;
    int i;

    if (count > 6) {
        rows = 6;
        if (dword_53F4E + 6 > (uint32_t)count)
            rows = 5;
    }

    for (i = 0; i < rows; ++i) {
        uint8_t idx = items[i + dword_53F4E];
        const uint8_t *it = shop_item_rec(idx);
        int type = it[0];
        int off  = 148 * (i % 2) + 10;
        int fg   = (i + dword_53F4E == (uint32_t)cursor) ? 201 : 205;
        uint8_t *base = dst + (320 * (26 * (i / 2) + 121) + off);
        int tile = 59;

        if (type >= 0x15)
            tile = (type >= 0x20) ? 61 : 60;
        dlg_frame_tile((void *)(uintptr_t)
                           (dst + (320 * (26 * (i / 2) + 119) + off)),
                       320, dword_53A81, tile);

        vm_run(dword_53A7D, idx + 181,dst + (320 * (26 * (i / 2) + 122) + off + 28), 320,
               fg, 76, 0, 0, 0);

        if (type < 0x15) {
            dlg_frame_tile((void *)(uintptr_t)(base + 95), 320,
                           dword_53A81, 64);
            dlg_draw_number((void *)(uintptr_t)(base + 118), 320,
                            *(const int16_t *)(const void *)(it + 1), 42, 3);
        } else if (type < 0x20) {
            dlg_frame_tile((void *)(uintptr_t)(base + 95), 320,
                           dword_53A81, 65);
            dlg_draw_number((void *)(uintptr_t)(base + 118), 320,
                            *(const int16_t *)(const void *)(it + 5), 42, 3);
        } else if (type == 0x20 && it[13] == 5) {
            dlg_frame_tile((void *)(uintptr_t)(base + 95), 320,
                           dword_53A81, 66);
            dlg_draw_number((void *)(uintptr_t)(base + 118), 320,
                            *(const int16_t *)(const void *)(it + 14), 42, 3);
        } else if (type == 0x20 && it[13] == 11) {
            dlg_frame_tile((void *)(uintptr_t)(base + 95), 320,
                           dword_53A81, 67);
            dlg_draw_number((void *)(uintptr_t)(base + 118), 320,
                            *(const int16_t *)(const void *)(it + 14), 42, 3);
        } else {
            res_blit6((void *)(uintptr_t)
                          (dst + (off + 95 + 320 * (26 * (i / 2) + 123))),
                      320, dword_53A81, 41);
        }

        dlg_frame_tile((void *)(uintptr_t)
                           (dst + (off + 95 + 320 * (26 * (i / 2) + 131))),
                       320, (const void *)(uintptr_t)dword_53F66, 15);

        {
            uint32_t v = (uint32_t)it[19] | ((uint32_t)it[20] << 8);

            if (half_price)
                v = (3 * v) >> 2;
            dlg_draw_number((void *)(uintptr_t)
                                (dst + (320 * (26 * (i / 2) + 131) + off + 104)),
                            320, (int)v, 119, 5);
        }
    }
}

/* 0x2A0C2 */
void shop_party_bar_rows(int count, uint8_t *dst, int cursor,
                         const uint8_t *members)
{
    int frame = dword_53F72;
    int rows  = count;
    int i;

    if (frame == 3)
        frame = 1;
    if (count > 3)
        rows = 3;

    for (i = 0; i < rows; ++i) {
        uint8_t idx = members[i + dword_53F4E];
        const uint8_t *rec = (const uint8_t *)dword_53A45 + 80u * idx;
        int fg   = (i + dword_53F4E == (uint32_t)cursor) ? 201 : 205;
        uint8_t *base = dst + (320 * (26 * i + 121));
        int hp   = rec[33];
        int tier = rec[32] - 1;

        sprite24_plain49(shop_unit_sprite(idx, frame),
                         dst + (320 * (26 * i + 117) + 14),
                         320);

        vm_run(dword_53A7D, rec[8] + 1,base + 40, 320, fg, 76, 0, 0, 0);
        vm_run(dword_53A7D, rec[31] + 140,base + 130, 320, fg, 76, 0, 0, 0);
        vm_run(dword_53A7D, rec[32] + 150,base + 175, 320, fg, 76, 0, 0, 0);

        {
            uint8_t *strip = dst + (320 * (26 * i + 125));

            dlg_frame_tile(strip + 220, 320,
                           (const void *)(uintptr_t)dword_53F66, 15);
            dlg_draw_number(strip + 228, 320,
                            hp * (int)(int16_t)word_52399[tier], 119, 5);
        }
    }
}

/* 0x2A694 */
void shop_party_equip_rows(int count, uint8_t *dst, int cursor,
                           const uint8_t *members, const uint8_t *equip)
{
    int frame = dword_53F72;
    int rows  = count;
    int i;

    if (frame == 3)
        frame = 1;
    if (count > 3)
        rows = 3;

    for (i = 0; i < rows; ++i) {
        uint8_t idx = members[i + dword_53F4E];
        const uint8_t *rec = (const uint8_t *)dword_53A45 + 80u * idx;
        const uint8_t *eq  = shop_equip_rec(equip[i + dword_53F4E]);
        int fg   = (i + dword_53F4E == (uint32_t)cursor) ? 201 : 205;
        uint8_t *base = dst + (320 * (26 * i + 121));

        sprite24_plain49(shop_unit_sprite(idx, frame),
                         dst + (320 * (26 * i + 117) + 14),
                         320);

        vm_run(dword_53A7D, rec[8] + 1,base + 40, 320, fg, 76, 0, 0, 0);
        vm_run(dword_53A7D, rec[32] + 150,base + 130, 320, fg, 76, 0, 0, 0);
        vm_run(dword_53A7D, 593,base + 175, 320, fg, 76, 0, 0, 0);
        vm_run(dword_53A7D, eq[0] + 150,base + 239, 320, fg, 76, 0, 0, 0);
    }
}
