/* figure_view.c - unit figure/animation view helpers.  See figure_view.h.
 *
 * Direct translations of the machine code at the stated addresses; the offline
 * disassembly (analysis/cseg01_disasm.txt) is authoritative for the argument
 * order, the constants and the branch conditions.  All real callees are
 * already-restored assets:
 *   map_cell_info(0x12E38)   rec_skip(0x1F183)        rec_sub_table5(0x1CA89)
 *   ui_status_panel_at(0x2FACD)  rle_decode(0x4E98D)  rle_decode_lut(0x4E8D3)
 *   res_blit(0x2EB9F)        gfx_copy_rows(0x11EB0)   svc_play_sfx(0x25A96)
 *   svc_wait_ticks(0x17AA9)  outp/j___delay (CRT)     memset
 */
#include "fd2_game.h"
#include "figure_view.h"

#include <stdint.h>
#include <string.h>

#define VGA_FRAME 0xA0000u /* the visible 320x200 frame                      */
#define WORK_SIZE 0x1F400u /* 640-stride figure work buffer (a4 of 0x2F4D4)  */
#define SCR_SIZE  0xFA00u  /* 320-stride screen buffer (a3 of 0x2F4D4)       */

/* 0x30E25 - return the BG.DAT sub-image index for the shown unit.  Starts from
 * the per-mode default and, scanning the unit list backwards, takes each
 * record's map-cell byte 6 while the unit has no skip flag or the pick is still
 * zero.  Record numbers index the 80-byte table dword_53A45; the cell info goes
 * through the 8-byte output block (byte 6 is the DAT sub-image index). */
int figure_bg_pick(int count, const uint8_t *units)
{
    uint8_t cell[8];
    int     sel = byte_52470[dword_53C03];

    while (--count >= 0) {
        const uint8_t *rec = (const uint8_t *)dword_53A45 + 80 * units[count];

        map_cell_info(rec[0], rec[1], cell);
        if (!rec_skip(units[count]) || sel == 0)
            sel = cell[6];
    }
    return sel;
}

/* 0x2F4D4 - scroll the three BG.DAT strips dword_5413F in the 640-stride work
 * buffer and publish 320x200 windows to VGA.  The first pass walks i=9..0
 * decoding dword_5413F[i%3] at (0,50) and copying the window starting at
 * work+32*i; the buffer is then cleared, the current background decoded into the
 * 320-stride screen buffer and the status panel drawn over it before the figure
 * is stamped.  The second pass repeats the walk but with dword_5413F[(i+2)%3]
 * decoded at work+320.  The original leaves the last res_blit result in eax;
 * all callers ignore it. */
void figure_bg_scroll(int unit, const void *figure, void *screen, void *tmp,
                      const void *bg)
{
    uint8_t *work = (uint8_t *)tmp;
    int      i;

    for (i = 9; i >= 0; --i) {
        rle_decode((const void *)(uintptr_t)dword_5413F[i % 3], 0, 50, work,
                   640, -1);
        gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, work + 32 * i, 640,
                      320, 200);
    }
    memset(work, 0, WORK_SIZE);
    memset(screen, 0, SCR_SIZE);
    rle_decode(bg, 0, 50, screen, 320, -1);
    ui_status_panel_at((int)(uintptr_t)screen, unit);
    gfx_copy_rows(work, 640, screen, 320, 320, 200);
    res_blit((void *)(uintptr_t)figure, 0, work, 640, -1);
    for (i = 9; i >= 0; --i) {
        rle_decode((const void *)(uintptr_t)dword_5413F[(i + 2) % 3], 0, 50,
                   work + 320, 640, -1);
        gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, work + 32 * i, 640,
                      320, 200);
    }
}

/* 0x30E9D - play record `unit`'s figure group `figure` frame by frame.
 *
 * `stride` is the dword_53A6D palette-strip index chosen from `mode`.  For each
 * sub-sprite `sub` of `figure` (count at figure[2], offsets at figure[4*i+8]):
 *   - play sub[5] when `mode` is one of the sound modes;
 *   - on a posture change (sub[4]==1) consume the record (rec_sub_table5), draw
 *     the status panel and, for the non-menu modes, the bg/tai strips with the
 *     mode's palette strip plus a sound;
 *   - for each of the sub's sub[6] frames composite the 320-stride buffer into
 *     the 640-stride work buffer, stamp `figure`'s sub-sprite `i` and, for the
 *     idle modes, `base`'s `frame` in the order selected by record +6, copy the
 *     window to VGA, flash the DAC with the mode's palette entry for 6 frames,
 *     advance the base-frame counter and wait one tick. */
void figure_play(int unit, int mode, const void *figure, const void *base,
                 void *tmp, void *screen, const void *bg, const void *tai)
{
    const uint8_t *fig     = (const uint8_t *)figure;
    const uint8_t *bas     = (const uint8_t *)base;
    uint8_t       *work    = (uint8_t *)tmp;
    uint8_t       *scr     = (uint8_t *)screen;
    const uint8_t *rec     = (const uint8_t *)dword_53A45 + 80 * unit;
    int            stride;
    int            i;
    int            j;
    int            flash   = 0;
    uint8_t        frame   = 0;
    uint8_t        counter = 0;

    stride = 11;
    if (mode == 8 || mode == 32 || mode == 33)
        stride = 19;
    else if (mode > 3)
        stride = 15;

    for (i = 0; i < fig[2]; ++i) {
        const uint8_t *sub = fig + *(const uint32_t *)(fig + 4 * i + 8);

        if ((mode == 24 || (mode > 27 && mode < 31)) && sub[5] != 0)
            svc_play_sfx((const void *)(uintptr_t)dword_5414B, sub[5], 1);
        if (sub[4] == 1) {
            rec_sub_table5(unit, mode);
            ui_status_panel_at((int)(uintptr_t)scr, unit);
            if (mode < 10 || mode >= 32) {
                const uint8_t *strip =
                    dword_53A6D +
                    *(const uint32_t *)(dword_53A6D + 4 * stride + 6);

                flash = 6;
                rle_decode_lut(bg, 0, 50, scr, 320, strip);
                rle_decode_lut(tai, 164, 157, scr, 320, strip);
                svc_play_sfx((const void *)(uintptr_t)dword_54153, 0, 1);
            }
        }
        for (j = 0; j < sub[6]; ++j) {
            gfx_copy_rows(work, 320, scr, 320, 320, 200);
            if (rec[6] != 0) {
                res_blit((void *)(uintptr_t)fig, i, work, 320, -1);
                if (mode < 10 || mode == 28)
                    res_blit((void *)(uintptr_t)bas, frame, work, 320, -1);
            } else {
                if (mode < 10 || mode == 28)
                    res_blit((void *)(uintptr_t)bas, frame, work, 320, -1);
                res_blit((void *)(uintptr_t)fig, i, work, 320, -1);
            }
            gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, work, 320, 320,
                          200);
            if (flash != 0) {
                outp(0x3C8, 0);
                outp(0x3C9, byte_51AAD[mode]);
                outp(0x3C9, byte_51AD1[mode]);
                outp(0x3C9, byte_51AF5[mode]);
                j___delay(30);
                outp(0x3C8, 0);
                outp(0x3C9, 0);
                outp(0x3C9, 0);
                outp(0x3C9, 0);
                --flash;
            }
            if (++counter ==
                bas[*(const uint32_t *)(bas + 4 * frame + 8) + 6]) {
                counter = 0;
                if (++frame == bas[0])
                    frame = 0;
            }
            svc_wait_ticks(1);
        }
    }
}
