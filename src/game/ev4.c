/* ev4.c - FD2 scene/map/animation helpers, batch 4 (see ev4.h).
 * Written from the original disassembly (cdecl: last push = first argument).
 */
#include "ev4.h"

#include <stdint.h>
#include <string.h>

#include "guest_mem.h"
#include "fd2_game.h"   /* de-guest C4: restored cross-module decls */
#include "../include/fd2_globals.h"

/* --- services ---------------------------------------------------------- */
typedef int      (*vm_fn)(void *stream, int sub, int addr, int pitch, int fg,
                          int shadow, int bgfill, int line_step, int wait);
typedef void     (*view_fn)(int flag);              /* 0x11CAC */
typedef void     (*pal_range_fn)(int s, int e, int sub); /* 0x11D40 */
typedef void     (*gfx_rows_fn)(void *dst, int dp, const void *src, int sp, int w, int h);
typedef void     (*map_render_fn)(uint8_t *dst, int pitch, int w, int h, int ox, int oy);
typedef void     (*portrait_draw_fn)(int idx);      /* 0x127E0 */
typedef void     (*map_refresh_fn)(void);           /* 0x129EC */
typedef void     (*anim_step_fn)(void);             /* 0x1297D */
typedef void     (*portraits_refresh_fn)(void);     /* 0x127A9 */
typedef void     (*map_ping_fn)(int idx);           /* 0x32230 */
typedef void     (*flush_fn)(void);                 /* 0x4E381 */
typedef int      (*kbd_pending_fn)(void);           /* 0x10620 */
typedef void     (*map_cell_fn)(int x, int y, uint8_t *out); /* 0x12E38 */
typedef void     (*pal_anim_fn)(void);              /* 0x4E31C */
typedef int      (*int386_fn)(int, const void *, void *); /* 0x370F0 */
typedef void     (*msg_band_fn)(int y, void *dst, void *src); /* 0x1974C */
typedef void     (*delay_fn)(int ms);               /* 0x3790A */
typedef void *(*tbl_fn)(int sel);                   /* 0x4EB48 */
typedef int      (*wait_fn)(int n);                 /* 0x17AA9 */

extern uintptr_t dos_lowmem_base;
#define BDA_W(off) (*(volatile uint16_t *)(uintptr_t)(dos_lowmem_base + (off)))


/* --- globals ----------------------------------------------------------- */

#define REC_STRIDE 80
#define MAP_BASE   (dword_53A49 + 32904u)   /* 0x8088 */
#define SCR_BASE   fd2_vga

/* 0x1366A - scene move-animation. The script (sub_4EB48(sel)) is a list of
 * groups: [n][count, m, (idx,val)*m]...; count's bit7 selects the "place"
 * (no animation, maybe a full repaint) vs the "walk" form. */
void ev4_1366A(int sel)
{
    uint8_t  v17[32], v16[32];
    uint8_t *p = (uint8_t *)tbl_off627D8(sel);
    uint8_t  n = *p, v24;
    const uint8_t *q = p + 1;

    for (v24 = 0; v24 < n; v24++) {
        uint8_t v23 = *q;
        uint8_t v19 = q[1];
        uint8_t i, j, k;
        int     mm;
        q += 2;
        for (k = 0; k < v19; k++) { v17[k] = q[2 * k]; v16[k] = q[2 * k + 1]; }
        q += 2 * v19;

        if ((v23 & 0x80u) == 0) {
            for (i = 0; i < v23; i++) {
                for (j = 1; j < 7; j++) {
                    map_unit_ping(v17[0]);
                    for (k = 0; k < v19; k++) {
                        uint8_t *r = (uint8_t *)(uintptr_t)(dword_53A45 + REC_STRIDE * v17[k]);
                        r[3] = v16[k];
                        r[4] = j;
                    }
                    if (dword_53AFB == 0 || dword_53AFB == 0x40) {
                        map_view_update(0);
                    } else {
                        ++dword_53AFB;
                        map_view_update(1);
                        pal_fade_range(0, 0xFF, (int)dword_53AFB);
                    }
                    svc_wait_ticks(1);
                    kbd_flush();
                }
                for (k = 0; k < v19; k++) {
                    uint8_t *r = (uint8_t *)(uintptr_t)(dword_53A45 + REC_STRIDE * v17[k]);
                    uint8_t  d = v16[k];
                    if (d != 0) {
                        if (d == 1)      --r[0];
                        else if (d == 3) ++r[0];
                        else             --r[1];
                    } else {
                        ++r[1];
                    }
                    r[4] = 0;
                }
            }
        } else {
            v23 &= 0x7Fu;
            if (v23 != 0) {
                for (k = 0; k < v19; k++)
                    *(uint8_t *)(uintptr_t)(dword_53A45 + REC_STRIDE * v17[k] + 3) = v16[k];
                for (k = 0; k < v23; k++) {
                    map_view_update(0);
                    svc_wait_ticks(1);
                    kbd_flush();
                }
            } else {
                uint8_t *save;
                svc_wait_ticks(1);
                map_render_view((uint8_t *)(uintptr_t)MAP_BASE, 456, 13, 8,
                                (int)dword_53AA9, (int)dword_53AAD);
                save = dword_53A49;
                for (mm = 0; mm < (int)dword_53BEB; mm++) {
                    uint8_t *r = (uint8_t *)(uintptr_t)(dword_53A45 + REC_STRIDE * mm);
                    for (k = 0; k < v19; k++) {
                        if (mm == v17[k]) {
                            dword_53A49 = save - 0x1560;
                            r[3] = v16[k];
                        }
                    }
                    if ((r[5] & 1) == 0)
                        dlg_portrait_draw(mm);
                    dword_53A49 = save;
                }
                map_refresh_records();
                gfx_copy_rows((void *)(uintptr_t)VGA_P(0xA0504u), 320,
                              (const void *)(uintptr_t)MAP_BASE, 456, 312, 192);
                svc_wait_ticks(2);
                map_view_update(0);
                kbd_flush();
            }
        }
    }
    map_view_update(1);
}

/* 0x11AA8 - idle until a key is pending, animating the palette/map, then
 * read the key through BIOS INT 16h and normalise two aliases. */
int ev4_11AA8(void)
{
    while (!kbd_pending()) {
        pal_anim_step();
        word_539F0 = BDA_W(0x46C);
        if ((int16_t)word_539F0 != (int16_t)word_539F2) {
            map_view_update(0);
            word_539F2 = (uint16_t)BDA_W(0x46C);
        }
        fd2_wait_tick_or_key();
    }
    *(uint8_t *)(uintptr_t)((uintptr_t)&word_53A8D + 1) = 0x10;
    int386(0x16, &word_53A8D, &word_53A8D);
    {
        uint8_t k = *(uint8_t *)(uintptr_t)((uintptr_t)&word_53A8D + 1);
        if (k == 0xE0 || k == 0x52)
            *(uint8_t *)(uintptr_t)((uintptr_t)&word_53A8D + 1) = 0x1C;
        k = *(uint8_t *)(uintptr_t)((uintptr_t)&word_53A8D + 1);
        if (k == 0x53)
            *(uint8_t *)(uintptr_t)((uintptr_t)&word_53A8D + 1) = 1;
    }
    return *(uint8_t *)(uintptr_t)((uintptr_t)&word_53A8D + 1);
}

/* 0x11B48 */
void ev4_11B48(void)
{
    if (dword_53AB5 != 0) {
        if ((int32_t)dword_53ABD < 2 && dword_53AAD != 0) {
            --dword_53AB5; --dword_53AAD;
        } else {
            --dword_53AB5; --dword_53ABD;
            if (dword_51A83 == 0)
                return;
        }
    }
    map_view_update(0);
}

/* 0x11B9B */
void ev4_11B9B(void)
{
    if (dword_53AC5 - 1 != (int)dword_53AB5) {
        if ((int32_t)dword_53ABD <= 5 || dword_53AC5 - 8 == dword_53AAD) {
            ++dword_53AB5; ++dword_53ABD;
            if (dword_51A83 == 0)
                return;
        } else {
            ++dword_53AB5; ++dword_53AAD;
        }
    }
    map_view_update(0);
}

/* 0x11BFA */
void ev4_11BFA(void)
{
    if (dword_53AC1 - 1 != (int)dword_53AB1) {
        if ((int32_t)dword_53AB9 <= 10 || dword_53AC1 - 13 == dword_53AA9) {
            dword_53AB1 += 1; ++dword_53AB9;
            if (dword_51A83 == 0)
                return;
        } else {
            dword_53AB1 += 1; ++dword_53AA9;
        }
    }
    map_view_update(0);
}

/* 0x11C59 */
void ev4_11C59(void)
{
    if (dword_53AB1 != 0) {
        if ((int32_t)dword_53AB9 < 2 && dword_53AA9 != 0) {
            dword_53AB1 -= 1; --dword_53AA9;
        } else {
            dword_53AB1 -= 1; --dword_53AB9;
            if (dword_51A83 == 0)
                return;
        }
    }
    map_view_update(0);
}

/* 0x12263 - for every map cell of type 0x20 whose status byte is set, bump
 * the cell's record count and clear its tag byte. */
void ev4_12263(void)
{
    int i, j;
    for (i = 0; i < (int)dword_53AC5; i++) {
        for (j = 0; j < (int)dword_53AC1; j++) {
            uint8_t  out[8];
            uint16_t idx;
            uint8_t *cell;
            map_cell_info(j, i, out);
            idx = *(uint16_t *)(void *)(out + 2);
            if ((out[4] & 0x60) != 0x20)
                continue;
            if (*(uint8_t *)(uintptr_t)(idx + dword_53AD5) == 0)
                continue;
            cell = (uint8_t *)(uintptr_t)(dword_53A51
                                         + 4 * (uint32_t)(j + i * (int)dword_53AC1) + 4);
            ++*(uint16_t *)(void *)cell;
            cell[2] = 0;
        }
    }
}

/* 0x1E1DC */
void ev4_1E1DC(int rec)
{
    uint8_t  v11[4];
    uint8_t  *base = dword_53A45 + REC_STRIDE * (uint32_t)rec;
    int      v5 = *(uint8_t *)(uintptr_t)base;
    int      v6 = *(uint8_t *)(uintptr_t)(base + 1);
    int      i;

    *(uint32_t *)v11 = dword_5204A;
    if (v5 > (int)dword_53AA9 - 1 && v5 < (int)(dword_51A87 + dword_53AA9)
        && v6 >= (int)dword_53AAD - 1 && v6 <= (int)(dword_51A8B + dword_53AAD)) {
        for (i = 0; i < 4; i++) {
            uint8_t v10 = (i == 1) ? (uint8_t)(5 * i + 3) : (uint8_t)(5 * i + 2);
            (&dword_53D34)[i + dword_53EC4] = v10;
            (&dword_53DFC)[i + dword_53EC4] = (uint8_t)rec;
            (&dword_53C6C)[i + dword_53EC4] = v11[i];
        }
        dword_53EC4 += 4;
    }
}

/* 0x24B4D */
void ev4_24B4D(int frames)
{
    int i;
    map_render_view((uint8_t *)(uintptr_t)MAP_BASE, 456, 13, 9,
                    (int)dword_53AA9, (int)dword_53AAD);
    map_view_update(0);
    for (i = 0; i < frames; i++) {
        gfx_copy_rows((void *)(uintptr_t)VGA_P(0xA0504u), 320,
                      (const void *)(uintptr_t)(456 * (i & 1) + MAP_BASE), 456, 312, 192);
        delay(20);
    }
}

/* 0x196CB - close the scene portrait: blit five bands, restore the VGA
 * screen from dword_53C5F, free the three screen buffers, refresh. */
void ev4_196CB(void)
{
    int i;
    for (i = 1; i < 6; i++)
        msg_blit_band(13 * i + 112, (void *)(uintptr_t)dword_53C5B,
                      (void *)(uintptr_t)dword_53C63);
    memmove((void *)(uintptr_t)SCR_BASE, (const void *)(uintptr_t)dword_53C5F, 64000);
    guest_free((void *)(uintptr_t)dword_53C5B);
    guest_free((void *)(uintptr_t)dword_53C5F);
    guest_free((void *)(uintptr_t)dword_53C63);
    map_view_update(0);
}
