/* party_panel.c - map party-status panel: row painter + sliding band helpers.
 * See party_panel.h for the family and the ABI.
 *
 * Direct translations of the machine code at the stated addresses; the offline
 * disassembly is authoritative for the band geometry and clipping. Each band
 * is cdecl (base, step); negative
 * intermediate values clip the copied byte count exactly as the original.
 * 0x1B41D paints one row block through the already-restored assets
 * res_blit6(0x16886), dlg_draw_number(0x187D6), vm_run(0x15F84) and
 * ev5_1B5F1(0x1B5F1) / unit_exists(0x33499).
 */
#include "fd2_game.h"
#include "party_panel.h"

#include <string.h>

/* 0x1AF1E */
void party_panel_band_a(const uint8_t *src, int step)
{
    int width = 170;
    int srcy  = 75;
    int dsty  = 75;
    int i;
    const uint8_t *s;
    uint8_t       *d;

    if (step < 5) {
        srcy = 75 - 50 * (4 - step);
        dsty = srcy;
        if (srcy < 0) {
            width = srcy + 170;
            srcy  = 50 * (4 - step);
            dsty  = 0;
        }
    }

    s = src + srcy + 11840;
    d = dword_53A49 + dsty + 11840;
    for (i = 0; i < 117; ++i) {
        memmove(d, s, (size_t)width);
        s += 320;
        d += 320;
    }
}

/* 0x1AF99 */
void party_panel_band_b(const uint8_t *src, int step)
{
    int width = 170;
    int dsty  = 75;
    int i;
    const uint8_t *s;
    uint8_t       *d;

    if (step <= 4) {
        int t = 50 * (4 - step);
        dsty = t + 75;
        if (t + 245 > 320)
            width = 320 - dsty;
    }

    s = src + 11915;
    d = dword_53A49 + dsty + 11840;
    for (i = 0; i < 117; ++i) {
        memmove(d, s, (size_t)width);
        s += 320;
        d += 320;
    }
}

/* 0x1B019 */
void party_panel_band_c(const uint8_t *src, int step)
{
    int count = 17;
    int srcy  = 19;
    int dsty  = 19;
    int i;
    const uint8_t *s;
    uint8_t       *d;

    if (step < 3)
        return;

    if (step <= 7) {
        int t = 19 - 6 * (4 - (step - 3));
        dsty = t;
        if (t < 0) {
            srcy  = 6 * (4 - (step - 3));
            count = t + 17;
            dsty  = 0;
        }
    }

    s = src + srcy * 320 + 109;
    d = dword_53A49 + dsty * 320 + 109;
    for (i = 0; i < count; ++i) {
        memmove(d, s, 0x66);
        s += 320;
        d += 320;
    }
}

/* 0x1B0AD */
void party_panel_band_d(const uint8_t *src, int step)
{
    int width = 170;
    int dsty  = 155;
    int i;
    const uint8_t *s;
    uint8_t       *d;

    if (step < 5)
        return;

    if (step <= 9) {
        int t = 9 * (4 - (step - 5));
        dsty = t + 155;
        if (t + 171 > 200)
            width = t - 45;
    }

    s = src + 49675;
    d = dword_53A49 + dsty * 320 + 75;
    for (i = 0; i < width; ++i) {
        memmove(d, s, 0xAA);
        s += 320;
        d += 320;
    }
}

/* 0x1B14B */
void party_panel_band_e(const uint8_t *src, int step)
{
    int count = 15;
    int dsty  = 172;
    int i;
    const uint8_t *s;
    uint8_t       *d;

    if (step < 8)
        return;

    if (step <= 12) {
        int t = 4 * (4 - (step - 8));
        dsty = t + 172;
        if (t + 187 > 200)
            count = t - 28;
    }

    s = src + 55169;
    d = dword_53A49 + dsty * 320 + 129;
    for (i = 0; i < count; ++i) {
        memmove(d, s, 0x3F);
        s += 320;
        d += 320;
    }
}

/* 0x1B41D */
void party_panel_draw(uint8_t *dst, int pitch)
{
    void *frame = (void *)(uintptr_t)dword_53A81;
    void *strm  = (void *)(uintptr_t)dword_53A7D;
    int   sub;

    res_blit6(dst + 109 + 19 * pitch,  pitch, frame, 0x85);
    res_blit6(dst + 75 + 37 * pitch,   pitch, frame, 0x86);
    res_blit6(dst + 75 + 155 * pitch,  pitch, frame, 0x87);
    res_blit6(dst + 129 + 172 * pitch, pitch, frame, 0x88);

    dlg_draw_number(dst + 143 + 24 * pitch,  pitch, (int)dword_53C03 + 1, 42, 2);
    dlg_draw_number(dst + 188 + 24 * pitch,  pitch, (int)dword_53BEF, 42, 3);
    dlg_draw_number(dst + 140 + 176 * pitch, pitch, (int)dword_53BF3, 31, 8);
    dlg_draw_number(dst + 120 + 159 * pitch, pitch, ev5_1B5F1(0), 42, 2);
    dlg_draw_number(dst + 182 + 159 * pitch, pitch, ev5_1B5F1(2), 42, 2);
    dlg_draw_number(dst + 228 + 159 * pitch, pitch, ev5_1B5F1(1), 42, 2);

    sub = 2 * dword_53C03 + 597;
    vm_run(strm, sub, (int)(uintptr_t)(dst + 80 + 61 * pitch),
           pitch, 205, 76, 0, 19, 0);
    if (dword_53C03 == 16 && unit_exists(18) == 0)
        sub -= 2;
    vm_run(strm, sub + 1, (int)(uintptr_t)(dst + 80 + 116 * pitch),
           pitch, 205, 76, 0, 19, 0);
}
