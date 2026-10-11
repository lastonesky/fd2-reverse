/* mosaic_reveal.c - see mosaic_reveal.h.
 *
 * Direct translations of the machine code at 0x1A7BD / 0x1A7F1 / 0x4EB59 /
 * 0x1F42D / 0x1F1CC / 0x1F30A.  The disassembly is the authority for the
 * constants, the split offsets and the mosaic addressing; the Hex-Rays output
 * drops the important detail that 0x4EB59 keeps the source row pointer and
 * re-samples it on a block x block lattice.
 */
#include "fd2_game.h"
#include "mosaic_reveal.h"

#include <stdint.h>
#include <string.h>

#define REVEAL_MAP_OFF 0x8088u   /* dword_53A49 + 32904, the 456-stride window */
#define REVEAL_STRIDE  456

/* 0x1A7BD - (re)load the shared SFX bank that the 0x1A30B party-change loop
 * hands to the map-frame handlers.  The machine clears dword_53B0F and passes
 * NULL as res_load's old_buffer, i.e. it always allocates a fresh container
 * FDOTHER.DAT sub-image 64; nothing happens unless byte_53AF9 (the in-game
 * flag scene_core takes from the save header) is set. */
void reveal_bank_load(void)
{
    if (byte_53AF9 == 0)
        return;

    dword_53B0F = 0;
    dword_53B0F = res_load(aFdotherDat, NULL, 64);
}

/* 0x1A7F1 - release the bank (original: `push dword_53B0F; call 0x3776E`). */
void reveal_bank_free(void)
{
    if (byte_53AF9 != 0)
        guest_free((void *)(uintptr_t)dword_53B0F);
}

/* 0x4EB59 - block-replicate blit of the 312x192 window at src+0x504 into
 * dst+0x504 on a block x block lattice (block == 1 copies 1:1).  Every output
 * cell takes the source byte at the cell's top-left corner; within a row the
 * source advances by `block` once per cell, after each `block` rows it advances
 * by 320*block.  The machine computes 320*block with a 16-bit mul into bp, so
 * the row step is truncated to 16 bits (irrelevant for the 1..17 callers). */
void mosaic_blit(int block, uint8_t *dst, uint8_t *src)
{
    uint8_t *d = dst + 0x504;
    uint8_t *s = src + 0x504;
    uint32_t row_step = (uint16_t)(320 * block);   /* ebp: mov bp, 320*bl */
    uint8_t  rows = 0xC0;                          /* ah: 192 scanlines */
    uint8_t  dh = (uint8_t)block;                  /* rows left in this cell band */

    do {
        uint8_t *row = s;                          /* push esi / pop esi */
        uint16_t cx = 0x138;                       /* 312 output columns */
        uint8_t  dl = (uint8_t)block;              /* columns left in this cell */

        do {
            *d++ = *s;
            if (--dl == 0) {
                dl = (uint8_t)block;
                s += block;
            }
        } while (--cx);

        s = row;
        d += 8;                                    /* 312 written -> 320 stride */
        if (--dh == 0) {
            dh = (uint8_t)block;
            s += row_step;
        }
    } while (--rows);
}

/* 0x1F42D - one transient frame of the two sliding portrait strips.  Saves the
 * background under both strips inside the map window (dword_53A49+0x8088,
 * stride 456), draws sub-image `index` in the left strip at column 85-offset
 * and sub-image 81 in the right strip at column 165+offset (both on row 82),
 * copies the 312x192 window to the VGA scratch page VGA_P(0xA0504u), waits a tick and
 * restores the saved background.  Ends in the shared epilogue at 0x184BA
 * (add esp,0xC drops the two restore record args) - a plain return here. */
void reveal_slide_draw(int offset, int index)
{
    uint8_t *buf = dword_53A49 + REVEAL_MAP_OFF;
    void *left  = res_draw_subimage(dword_53A81, buf, REVEAL_STRIDE,
                                    85 - offset, 82, index);
    void *right = res_draw_subimage(dword_53A81, buf, REVEAL_STRIDE,
                                    offset + 165, 82, 81);

    gfx_copy_rows((void *)(uintptr_t)VGA_P(0xA0504u), 320, buf, REVEAL_STRIDE,
                  0x138, 192);
    svc_wait_ticks(1);
    dlg_snap_restore(left, buf, REVEAL_STRIDE);
    dlg_snap_restore(right, buf, REVEAL_STRIDE);
}

/* 0x1F1CC - mosaic coarsen (block 1..16) with the palette fading down,
 * compositing sub-image `index` (and 81) over each frame. */
void reveal_mosaic_out(int index)
{
    uint8_t *scratch = guest_malloc(0xFA00u);
    int i, j;

    memmove(scratch, (const void *)(uintptr_t)VGA_P(0xA0000u), 0xFA00u);

    for (i = 4; i >= 0; --i)
        reveal_slide_draw(25 * i, index);
    reveal_slide_draw(1, index);
    reveal_slide_draw(0, index);

    memmove(dword_53A49, scratch, 0xFA00u);
    memset(scratch, 0, 0xFA00u);

    for (j = 0; j < 16; ++j) {
        void *p;

        mosaic_blit(j + 1, scratch, dword_53A49);

        p = res_draw_subimage(dword_53A81, scratch, 320, 89, 86, index);
        guest_free(p);
        p = res_draw_subimage(dword_53A81, scratch, 320, 169, 86, 81);
        guest_free(p);

        pal_fade_range(16, 255, j);
        memmove((void *)(uintptr_t)VGA_P(0xA0000u), scratch, 0xFA00u);
        svc_wait_ticks(1);
    }

    guest_free(scratch);
    kbd_flush();
}

/* 0x1F30A - mosaic refine (block 17..1) with the palette fading back up, then
 * redraw the map window, refresh the portraits and re-run the slide frames. */
void reveal_mosaic_in(int index)
{
    uint8_t *scratch = guest_malloc(0xFA00u);
    int i, j;

    memset(scratch, 0, 0xFA00u);

    for (i = 16; i >= 0; --i) {
        void *p;

        mosaic_blit(i + 1, scratch, dword_53A49);

        p = res_draw_subimage(dword_53A81, scratch, 320, 89, 86, index);
        guest_free(p);
        p = res_draw_subimage(dword_53A81, scratch, 320, 169, 86, 81);
        guest_free(p);

        pal_fade_range(16, 255, i);
        memmove((void *)(uintptr_t)VGA_P(0xA0000u), scratch, 0xFA00u);
        svc_wait_ticks(1);
    }

    guest_free(scratch);

    map_render_view(dword_53A49 + REVEAL_MAP_OFF, REVEAL_STRIDE, 13, 8,
                    (int)dword_53AA9, dword_53AAD);
    dlg_portraits_refresh();

    for (j = 0; j <= 4; ++j)
        reveal_slide_draw(25 * j, index);
}
