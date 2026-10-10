/* box_popup.c - character/inventory popup box.  See box_popup.h.
 *
 * Direct translations of the machine code at the stated addresses.  Uses the
 * already-verified assets: ui_meter_bar(0x18795), dlg_draw_number(_pair)
 * (0x187D6/0x1875D), dlg_frame_tile(0x1685C), dlg_box_stage(0x168B6),
 * dlg_wait_key(0x16C57), res_load(0x111BA), res_blit6(0x16886),
 * rle2_blit(0x4EBFF), vm_run(0x15F84), svc_play_sfx(0x25A96),
 * kbd_flush(0x4E381), fb_slide_step(0x18409), rec_field_byte(0x1B722) and
 * tbl_ptr(0x4E8BC / 0x4E88E tables).
 */
#include "fd2_game.h"
#include "box_popup.h"

#include <stdint.h>
#include <string.h>

/* item table accessor 0x4E8BC: base 0x602AD, stride 23 (dseg03). */
static const uint8_t *item_rec(int index)
{
    return (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x2AD], 23, index, 0);
}

/* 0x17FC0 */
void box_status_draw(int rec, int base)
{
    const uint8_t *r = (const uint8_t *)dword_53A45 + 80u * (uint32_t)rec;
    int v13 = *(const int16_t *)(const void *)(r + 64);
    int v3  = *(const int16_t *)(const void *)(r + 66);
    int v12 = *(const int16_t *)(const void *)(r + 68);
    int v4  = *(const int16_t *)(const void *)(r + 70);
    int v5, v6, v7, v8, v9, i;

    ui_meter_bar(base + 10758, 320, 23, v13, v3);
    ui_meter_bar(base + 16838, 320, 26, v12, v4);

    dlg_draw_number_pair((void *)(uintptr_t)(base + 13387), 320, v13, v3, 3);
    dlg_draw_number_pair((void *)(uintptr_t)(base + 13413), 320, v3, v3, 3);
    dlg_draw_number_pair((void *)(uintptr_t)(base + 19147), 320, v12, v4, 3);
    dlg_draw_number_pair((void *)(uintptr_t)(base + 19173), 320, v4, v4, 3);

    dlg_draw_number((void *)(uintptr_t)(base + 10717), 320, r[33], 42, 2);
    dlg_draw_number((void *)(uintptr_t)(base + 14237), 320, r[60], 42, 2);
    dlg_draw_number((void *)(uintptr_t)(base + 17757), 320, r[59], 42, 2);

    v5 = (r[34] != 0) ? 119 : 42;
    dlg_draw_number((void *)(uintptr_t)(base + 21597), 320,
                    *(const int16_t *)(const void *)(r + 72), v5, 3);
    v6 = (r[35] != 0) ? 119 : 42;
    dlg_draw_number((void *)(uintptr_t)(base + 25437), 320,
                    *(const int16_t *)(const void *)(r + 74), v6, 3);
    dlg_draw_number((void *)(uintptr_t)(base + 17717), 320,
                    *(const int16_t *)(const void *)(r + 62), 42, 3);
    v7 = (r[36] != 0) ? 119 : 42;
    v8 = v7;
    dlg_draw_number((void *)(uintptr_t)(base + 21557), 320,
                    *(const int16_t *)(const void *)(r + 76), v7, 3);
    dlg_draw_number((void *)(uintptr_t)(base + 25397), 320,
                    *(const int16_t *)(const void *)(r + 78), v8, 3);

    vm_run(dword_53A7D, r[8] + 1, base + 4259, 320, 205, 76, 0, 0, 0);
    vm_run(dword_53A7D, r[31] + 140, base + 4371, 320, 205, 76, 0, 0, 0);
    vm_run(dword_53A7D, r[32] + 150, base + 4411, 320, 205, 76, 0, 0, 0);

    v9 = (r[6] == 0) ? 54 : 53;
    dlg_frame_tile((void *)(uintptr_t)(base + 9701), 320, dword_53A81, v9);

    for (i = 0; i < 3; ++i) {
        if (r[i + 37] != 0)
            dlg_frame_tile((void *)(uintptr_t)(35 * i + base + 21954), 320,
                           dword_53A81, i + 55);
    }
}

/* 0x184C0 */
void box_items_draw(int rec, int highlight, int base)
{
    const uint8_t *r = (const uint8_t *)dword_53A45 + 80u * (uint32_t)rec;
    int i;
    int v3 = 0;

    for (i = 0; i < 8; ++i) {
        int v15 = (int8_t)r[2 * i + 10];
        int v16, v5, v6, v7, v8, v9, v17, v18, v10, drawn = 1;
        const uint8_t *v13;

        if (v15 < 0)                 /* bit 0x80: empty slot */
            continue;

        v18 = 150 * (v3 / 4) + 42;
        v16 = r[2 * i + 11];
        v13 = item_rec(v16);

        if (v13[0] >= 0x15) {
            if (v13[0] >= 0x20)
                v5 = 61;
            else
                v5 = 60;
        } else {
            v5 = 59;
        }
        if (v15 & 0x40)
            v5 += 3;

        dlg_frame_tile((void *)(uintptr_t)(320 * (22 * (v3 & 3) + 101) +
                                           v18 + base - 29),
                       320, dword_53A81, v5);

        v6 = (i == highlight) ? 201 : 205;
        vm_run(dword_53A7D, v16 + 181,
               v18 + base + 320 * (22 * (v3 & 3) + 103), 320, v6, 76, 0, 0, 0);

        v7 = v13[0];
        v8 = 320 * (22 * (v3 & 3) + 107);
        v17 = v8 + v18 + base + 93;
        v9  = v18 + base + 68 + v8;

        v10 = 0;
        if (v7 >= 0x15) {
            if (v13[0] >= 0x20) {
                if (v7 == 32 && v13[13] == 5) {
                    dlg_frame_tile((void *)(uintptr_t)v9, 320, dword_53A81, 66);
                    v10 = *(const int16_t *)(const void *)(v13 + 14);
                } else if (v13[0] == 32 && v13[13] == 11) {
                    int v11 = 320 * (22 * (v3 & 3) + 107);

                    dlg_frame_tile((void *)(uintptr_t)(v11 + v18 + base + 68),
                                   320, dword_53A81, 67);
                    dlg_draw_number((void *)(uintptr_t)(v18 + base + 93 + v11),
                                    320, *(const int16_t *)(const void *)(v13 + 14),
                                    42, 3);
                    drawn = 0;
                } else {
                    res_blit6((void *)(uintptr_t)(v18 + base + 68 +
                                                  320 * (22 * (v3 & 3) + 107)),
                              320, dword_53A81, 41);
                    drawn = 0;
                }
            } else {
                dlg_frame_tile((void *)(uintptr_t)v9, 320, dword_53A81, 65);
                v10 = *(const int16_t *)(const void *)(v13 + 5);
            }
        } else {
            dlg_frame_tile((void *)(uintptr_t)v9, 320, dword_53A81, 64);
            v10 = *(const int16_t *)(const void *)(v13 + 1);
        }

        if (drawn)
            dlg_draw_number((void *)(uintptr_t)v17, 320, v10, 42, 3);

        ++v3;
    }
}

/* 0x17EEF */
void box_open_init(int rec, int base)
{
    const uint8_t *r;
    const uint8_t *dato;
    const void    *v2;

    dword_53C67 = 3208;
    r = (const uint8_t *)dword_53A45 + 80u * (uint32_t)rec;
    dword_53A85 = res_load(aDatoDat, dword_53A85, r[7]);

    dato = (const uint8_t *)dword_53A85;
    v2 = (const void *)(dato + dato[0]);

    dlg_box_stage((void *)(uintptr_t)base, 320, 5, 7, 5, 5);
    rle2_blit((void *)(uintptr_t)(dword_53C67 + base), v2, 320);
    rle2_blit((void *)(uintptr_t)(base + 2332),
              (const void *)(uintptr_t)
                  (*(const uint32_t *)(const void *)((uintptr_t)dword_53A81 + 86) +
                   (uintptr_t)dword_53A81), 320);
    rle2_blit((void *)(uintptr_t)(base + 30085),
              (const void *)(uintptr_t)
                  (*(const uint32_t *)(const void *)((uintptr_t)dword_53A81 + 90) +
                   (uintptr_t)dword_53A81), 320);

    box_status_draw(rec, base);
}

/* 0x17E0B */
void box_open_anim(int rec)
{
    int i;

    dword_53C5B = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);
    dword_53C5F = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);
    dword_53C63 = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);

    memmove((void *)(uintptr_t)dword_53C5F, (const void *)(uintptr_t)0xA0000u, 0xFA00u);
    memmove((void *)(uintptr_t)dword_53C63, (const void *)(uintptr_t)dword_53C5F, 0xFA00u);

    box_open_init(rec, (int)dword_53C63);
    box_items_draw(rec, -1, (int)dword_53C63);

    for (i = 11; i >= 0; --i) {
        if (i == 11 || i == 5)
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 5, 1);
        fb_slide_step(i, (char *)(uintptr_t)dword_53C5B,
                      (char *)(uintptr_t)dword_53C63,
                      (char *)(uintptr_t)dword_53C5F);
    }

    kbd_flush();
}

/* 0x1B9DE */
int box_input(int rec, int mode)
{
    int            count = 0;
    const uint8_t *r;
    int            i, key;

    box_items_draw(rec, dword_53C57, 655360);

    r = (const uint8_t *)dword_53A45 + 80u * (uint32_t)rec;
    for (i = 0; i < 8; ++i) {
        if ((int8_t)r[2 * i + 10] >= 0)
            ++count;
    }

    dlg_wait_key(0);
    key = byte_53A8E;   /* 0x16C57 returns its (remapped) scan code */
    switch (key) {
    case 1:
        return -1;
    case 28:
    case 57:
        if (mode == 0)
            return 1;
        return item_rec(r[2 * (int)dword_53C57 + 11])[13] != 0 ? 1 : 0;
    case 72:
        if (dword_53C57 != 0) {
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
            --dword_53C57;
            return 0;
        }
        svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
        dword_53C57 = (uint32_t)(count - 1);
        return 0;
    case 75:
        if (dword_53C57 >= 4) {
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
            dword_53C57 -= 4;
        }
        return 0;
    case 77:
        if ((int)dword_53C57 <= 3 && count - 4 > (int)dword_53C57) {
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
            dword_53C57 += 4;
        }
        return 0;
    case 80:
        if (count - 1 != (int)dword_53C57) {
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
            ++dword_53C57;
        } else {
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
            dword_53C57 = 0;
        }
        return 0;
    default:
        return 0;
    }
}

/* 0x1B932 */
int box_popup(int rec, int mode)
{
    int ret, i;

    box_open_anim(rec);
    dword_53C57 = 0;

    do {
        ret = box_input(rec, mode);
    } while (ret == 0);

    for (i = 0; i <= 11; ++i)
        fb_slide_step(i, (char *)(uintptr_t)dword_53C5B,
                      (char *)(uintptr_t)dword_53C63,
                      (char *)(uintptr_t)dword_53C5F);

    memmove((void *)(uintptr_t)0xA0000u, (const void *)(uintptr_t)dword_53C5F, 0xFA00u);

    guest_free((void *)(uintptr_t)dword_53C5B);
    guest_free((void *)(uintptr_t)dword_53C5F);
    guest_free((void *)(uintptr_t)dword_53C63);

    return ret != -1;
}

/* 0x1C1C3 */
int box_item_usable(int rec, int item)
{
    const uint8_t *r = (const uint8_t *)dword_53A45 + 80u * (uint32_t)rec;
    const uint8_t *cls = (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x188A], 7,
                                                  r[32], 0);
    uint8_t type = item_rec(item)[0];
    int i;

    for (i = 0; i < 6; ++i) {
        if (type == cls[i])
            return 1;
    }
    return 0;
}

/* 0x1C142 */
int box_item_select(int rec, int slot)
{
    uint8_t *r = (uint8_t *)dword_53A45 + 80u * (uint32_t)rec;
    int v3 = rec_field_byte(rec, slot);
    int i;

    for (i = 0; i < 8; ++i) {
        uint8_t *f = &r[2 * i + 10];

        if ((*f & 0x40) == 0)
            continue;
        if ((v3 < 0x80 && r[2 * i + 11] < 0x80)
            || (v3 >= 0x80 && r[2 * i + 11] >= 0x80))
            *f = 0;
    }
    r[2 * slot + 10] = 0x40;
    return slot;
}
