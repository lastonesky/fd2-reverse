/* item_use.c - item / healing use effects.  See item_use.h.
 *
 * Direct translations of the machine code at the stated addresses.  The
 * sprite blit, party-icon draw, status queue, portrait glide and view refresh
 * are the already-verified assets sprite24_const/plain (0x4E127/0x4E22A),
 * map_draw_party_icons(_anim) (0x1C2DA/0x1C4CC), map_enqueue_status
 * (0x1E0DB), ev4_1E1DC (0x1E1DC), dlg_portrait_glide (0x12D7B),
 * svc_wait_ticks (0x17AA9), svc_play_sfx (0x25A96), gfx_copy_rows (0x11EB0)
 * and map_view_update (0x11CAC); the RNG is util_rand (0x4EBE3).
 *
 * The random terms are signed remainders of util_rand() exactly as the CRT
 * idiv does (cf. fx_rand_mod2 in fx.c).
 */
#include "fd2_game.h"
#include "item_use.h"

#include <stdint.h>

/* 0x1C916 - heal `rec` by `amount`: 9/10 fixed plus (rand%100)/1000 of it,
 * clamped at the max at record+66.  The dword_53EC8 experience bookkeeping
 * weights the change by 40 * (record+33, +30 for records 9..24) / max. */
int unit_heal(int rec, int amount)
{
    uint8_t *r = (uint8_t *)dword_53A45 + 80u * (uint32_t)rec;
    int hp  = *(const uint16_t *)(const void *)(r + 64);
    int max = *(const uint16_t *)(const void *)(r + 66);
    int fixed  = amount * 9 / 10;
    int random = (int)((int32_t)util_rand() % 100) * amount / 1000;
    int newhp  = hp + fixed + random;
    int heal, v7;

    if (newhp > max)
        newhp = max;
    heal = newhp - hp;
    *(uint16_t *)(void *)(r + 64) = (uint16_t)newhp;

    v7 = r[33];
    if (r[32] > 8 && r[32] < 0x19)
        v7 += 30;
    if (r[7] < 0x4B && max != 0)
        dword_53EC8 += (uint32_t)(40 * v7 * heal / max);

    return fixed + random;
}

/* 0x1C8ED - heal record `rec` by word 0 of action `action`'s tbl_619FD entry
 * (0x619FD / stride 7); returns the amount unit_heal added. */
int unit_heal_by_action(int rec, int action)
{
    const uint8_t *act = (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x19FD],
                                                  7, action, 0);

    return unit_heal(rec, *(const int16_t *)(const void *)act);
}

/* 0x1C9DD - the magic-point twin of unit_heal: add `amount` to the second
 * pool at record+68 (90% fixed + up to 10% random, clamped at the max at
 * +70) and book the experience in dword_53EC8, weighted by 40 * record+33 /
 * max (no +30 bonus here).  Returns the amount added. */
int unit_restore_mp(int rec, int amount)
{
    uint8_t *r = (uint8_t *)dword_53A45 + 80u * (uint32_t)rec;
    int mp   = *(const uint16_t *)(const void *)(r + 68);
    int max  = *(const uint16_t *)(const void *)(r + 70);
    int fixed  = amount * 9 / 10;
    int random = (int)((int32_t)util_rand() % 100) * amount / 1000;
    int newmp  = mp + fixed + random;
    int delta;

    if (newmp > max)
        newmp = max;
    delta = newmp - mp;
    *(uint16_t *)(void *)(r + 68) = (uint16_t)newmp;

    if (r[7] < 0x4B && max != 0)
        dword_53EC8 += (uint32_t)(40 * (int)r[33] * delta / max);

    return fixed + random;
}

/* 0x1DA16 - blit record `rec`'s 24x24 sprite into the map bitmap at `dst`
 * (an absolute screen address).  Out-of-view records are dropped.  `mode` 0
 * is the plain sprite, 2 the constant-colour variant; `field` is the
 * record-relative animation phase byte. */
int view_draw_unit_sprite(int dst, int pitch, int rec, int mode, int field)
{
    const uint8_t *r = (const uint8_t *)dword_53A45 + 80u * (uint32_t)rec;
    int v6  = r[0];
    int v7  = r[1];
    int v8  = r[3];
    int v15 = r[4];
    int ox  = (int)qword_53AA9;
    int oy  = dword_53AAD;
    int v10, v11, v13;
    const uint8_t *base = (const uint8_t *)dword_53A61;
    const uint8_t *buf;

    if (v6 < ox - 1 || v6 > dword_51A87 + ox ||
        v7 < oy - 1 || v7 > dword_51A8B + oy + 1)
        return 0;

    if (v8 == 0)
        v10 = 4 * pitch;
    else if (v8 == 1)
        v10 = -4;
    else if (v8 == 2)
        v10 = -4 * pitch;
    else
        v10 = 4;

    v11 = (v15 != 0) ? dword_53C07 : dword_53C0B;
    if (v11 == 3)
        v11 = 1;

    buf = base + *(const int32_t *)(const void *)
                    (base + 4 * (12 * r[2] + 3 * v8 + v11));

    v13 = dst - 6 * pitch + 24 * pitch * (v7 - oy) + 24 * (v6 - ox);

    if (mode == 0)
        sprite24_plain(buf, (void *)(uintptr_t)(v13 + v10 * v15), pitch);
    else if (mode == 2)
        sprite24_const(buf, (void *)(uintptr_t)(v13 + v10 * v15), pitch);

    return 0;
}

/* 0x22AF6 - redraw the party icons, then for each record in `list` either
 * consume the item (heal 10, enqueue the "+N" status, clear the field byte,
 * book experience) or enqueue its idle cursor sprite. */
int item_apply(int a1, int a2, int count, const uint8_t *list, int field)
{
    int i;

    map_draw_party_icons_anim(a1, a2, count, list);
    map_draw_party_icons(a1, a2, count, list);

    for (i = 0; i < count; ++i) {
        uint8_t *v6 = (uint8_t *)dword_53A45 + 80u * list[i];
        int      v7 = v6[33];
        uint8_t *v8;
        uint8_t *v10 = (uint8_t *)list + i;

        if (v6[32] > 8 && v6[32] < 0x19)
            v7 += 30;

        v8 = v6 + field;
        if (*v8 != 0) {
            int r = unit_heal(*v10, 10);

            map_enqueue_status(r, 105, *v10);
            *v8 = 0;
            dword_53EC8 += 4u * (uint32_t)v7;
        } else {
            ev4_1E1DC(*v10);
        }
    }

    map_view_update(0);
    return 0;
}

/* 0x13FD4 - use a healing item on `rec`: only a wounded record with an empty
 * item/state pair (record+37/+38) can be healed.  The two sprite draws are
 * the flash; the record then gains max/5 HP. */
int item_use(int rec)
{
    uint8_t *r = (uint8_t *)dword_53A45 + 80u * (uint32_t)rec;
    int      hp  = *(const uint16_t *)(const void *)(r + 64);
    int      max = *(const uint16_t *)(const void *)(r + 66);
    int      v5;

    if (hp == max || r[37] != 0 || r[38] != 0)
        return 0;

    dword_51A83 = r[38];            /* 0: both were checked zero above */
    dlg_portrait_glide(rec);
    svc_wait_ticks(1);
    svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 4, 1);

    view_draw_unit_sprite((int)(intptr_t)dword_53A49 + 32904, 456, rec, 2, 253);
    gfx_copy_rows((void *)(uintptr_t)0xA0504u, 320,
                  dword_53A49 + 32904, 456, 0x138, 192);
    svc_wait_ticks(1);

    view_draw_unit_sprite((int)(intptr_t)dword_53A49 + 32904, 456, rec, 0, 0);
    gfx_copy_rows((void *)(uintptr_t)0xA0504u, 320,
                  dword_53A49 + 32904, 456, 0x138, 192);
    svc_wait_ticks(1);

    v5 = max / 5 + hp;
    if (v5 > max)
        v5 = max;
    *(uint16_t *)(void *)(r + 64) = (uint16_t)v5;
    dword_51A83 = 1;
    return 1;
}
