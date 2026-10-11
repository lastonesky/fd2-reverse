/* party_panel.c - map party-status panel: row painter + sliding band helpers.
 * See party_panel.h for the family and the ABI.
 *
 * Direct translations of the machine code at the stated addresses; the offline
 * disassembly is authoritative for the band geometry and clipping. Each band
 * is cdecl (base, step); negative
 * intermediate values clip the copied byte count exactly as the original.
 * 0x1B41D paints one row block through the already-restored assets
 * res_blit6(0x16886), dlg_draw_number(0x187D6), vm_run(0x15F84) and
 * ev5_1B5F1(0x1B5F1) / unit_exists(0x33499); 0x1B1E7 (party_panel_run) is the
 * driver that snapshots the map window and slides the panel in and out.
 */
#include "fd2_game.h"
#include "party_panel.h"

#include "../dos.h"

#include <stdint.h>
#include <string.h>

/* BIOS tick word, read through the low-memory mirror (map_zoom.c pattern). */
#define BDA_W(off) (*(volatile uint16_t *)(uintptr_t)(DOS_LOWMEM_BASE + (off)))

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
    vm_run(strm, sub,dst + 80 + 61 * pitch,
           pitch, 205, 76, 0, 19, 0);
    if (dword_53C03 == 16 && unit_exists(18) == 0)
        sub -= 2;
    vm_run(strm, sub + 1,dst + 80 + 116 * pitch,
           pitch, 205, 76, 0, 19, 0);
}

/* 0x1B1E7 - the world-map party pop-up orchestrator.  Snapshot the live map
 * window into `backdrop` (so the panel can be redrawn every frame without
 * re-rendering the map), paint the panel row block into `scratch`, then slide
 * the rows in over 12 steps (band_b/c/d/e).  Then hold: while no key is
 * pending, redraw the live map view and the panel placed at dword_53A49+0x7964
 * every time the BIOS tick differs from `baseline`, publishing the 312x192
 * window to the VGA staging page VGA_P(0xA0504u).  On a key: flush, slide the rows
 * back out over 12 steps (band_a/c/d/e), restore the backdrop, free the two
 * scratch buffers and flush again.
 *
 * Notes / decompiler artifacts:
 *   - the Watcom stack probe (`push 0x2C; call 0x3702F`) is dropped (0x3702F
 *     does not allocate a frame); the two mallocs (0x3706E) and frees
 *     (0x3776E) are the guest heap, and memmove/memset are libc;
 *   - `baseline` is the uninitialised register `ebp` read at 0x1B2FE and never
 *     written anywhere in the function: the original compares the BIOS tick
 *     word against garbage and never updates it, so the redraw block runs on
 *     essentially every pass.  Modelled as 0, following the same convention as
 *     map_wait_key (0x12DAC) and map_zoom_run (0x2000A);
 *   - Hex-Rays aliases the key byte (machine `[esp]`) with the return-address
 *     slot it calls `retaddr`; the byte is just a local `key`;
 *   - the tail `jmp 0x10C49` lands in unit_sprites_build's (0x10B4E) shared
 *     epilogue (`add esp,4; pop edi/esi/ebx; ret`), matching this function's
 *     frame exactly: a plain return.  `sub_10B4E` is therefore a false callee,
 *     as is the `sub_4E31C(v4)` argument the decompiler invents (pal_anim_step
 *     takes no argument: the machine pushes nothing before `call 0x4E31C`).
 */
void party_panel_run(void)
{
    uint8_t *backdrop = (uint8_t *)guest_malloc(0xFA00u);
    uint8_t *scratch  = (uint8_t *)guest_malloc(0xFA00u);
    int      baseline = 0;   /* uninitialised ebp tick baseline; see note above */
    int      i;

    memset(backdrop, 0, 0xFA00u);

    map_render_view(dword_53A49 + 0x8088, 456, 13, 8,
                    (int)dword_53AA9, dword_53AAD);
    dlg_portraits_refresh();
    gfx_copy_rows(backdrop + 0x504, 320, dword_53A49 + 0x8088, 456, 312, 192);

    party_panel_draw(scratch, 320);

    for (i = 0; i < 12; ++i) {
        memmove(dword_53A49, backdrop, 0xFA00u);
        party_panel_band_b(scratch, i);
        party_panel_band_c(scratch, i);
        party_panel_band_d(scratch, i);
        party_panel_band_e(scratch, i);
        memmove((void *)(uintptr_t)VGA_P(0xA0000u), dword_53A49, 0xFA00u);
    }

    for (;;) {
        uint8_t key = (uint8_t)kbd_pending();

        if ((int)(int16_t)BDA_W(0x46C) != baseline) {
            anim_frame_step();
            pal_anim_step();
            map_render_view(dword_53A49 + 0x8088, 456, 13, 8,
                            (int)dword_53AA9, dword_53AAD);
            dlg_portraits_refresh();
            party_panel_draw(dword_53A49 + 0x7964, 456);
            gfx_copy_rows((void *)(uintptr_t)VGA_P(0xA0504u), 320,
                          dword_53A49 + 0x8088, 456, 312, 192);
        }

        if (key != 0) {
            kbd_flush();
            for (i = 11; i >= 0; --i) {
                memmove(dword_53A49, backdrop, 0xFA00u);
                party_panel_band_a(scratch, i);
                party_panel_band_c(scratch, i);
                party_panel_band_d(scratch, i);
                party_panel_band_e(scratch, i);
                memmove((void *)(uintptr_t)VGA_P(0xA0000u), dword_53A49, 0xFA00u);
            }
            memmove((void *)(uintptr_t)VGA_P(0xA0000u), backdrop, 0xFA00u);
            guest_free(backdrop);
            guest_free(scratch);
            kbd_flush();
            return;
        }
    }
}
