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
 *
 * Two further helpers of the same family are used by the sibling figure
 * drivers sub_2EBE1 (0x2EBE1) and sub_2FB2C (0x2FB2C): 0x2F631 is the
 * figure_bg_scroll variant that also stamps the portrait strip, 0x2FE14 plays
 * a figure group frame by frame.
 */
#include "fd2_game.h"
#include "figure_view.h"

#include <stdint.h>
#include <stdio.h>
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

/* 0x2F631 - like figure_bg_scroll but also stamps the strip `portrait` at
 * (164,157) before the status panel and the figure.  `unit` selects the status
 * panel, `bg` is the background strip decoded at (0,50), `screen` the 0xFA00
 * 320-stride buffer and `work` the 640-stride 0x1F400 work buffer.  The first
 * pass scrolls the rotating BG strips into work+320 and publishes a window per
 * step; after the panel/figure stamp the second pass cycles the strips through
 * work.  Called by sub_2EBE1 as (unit, figure, portrait, screen, work, bg). */
void figure_bg_scroll_portrait(int unit, const void *figure,
                               const void *portrait, void *screen, void *work,
                               const void *bg)
{
    uint8_t *wrk = (uint8_t *)work;
    uint8_t *scr = (uint8_t *)screen;
    int      i;

    for (i = 1; i < 10; ++i) {
        rle_decode((const void *)(uintptr_t)dword_5413F[i % 3], 0, 50,
                   wrk + 320, 640, -1);
        gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, wrk + 32 * i, 640,
                      320, 200);
    }
    memset(wrk, 0, WORK_SIZE);
    memset(scr, 0, SCR_SIZE);
    rle_decode(bg, 0, 50, scr, 320, -1);
    rle_decode(portrait, 164, 157, scr, 320, -1);
    ui_status_panel_at((int)(uintptr_t)scr, unit);
    gfx_copy_rows(wrk + 320, 640, scr, 320, 320, 200);
    res_blit((void *)(uintptr_t)figure, 0, wrk + 320, 640, -1);
    for (i = 1; i <= 10; ++i) {
        rle_decode((const void *)(uintptr_t)dword_5413F[(i + 1) % 3], 0, 50,
                   wrk, 640, -1);
        gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, wrk + 32 * i, 640,
                      320, 200);
    }
}

/* 0x2FE14 - play `frames` animation frames of the figure group `figure` over
 * the cycling BG strips dword_5413F into the 640-stride work buffer `work`,
 * publishing a 320x200 window to VGA and waiting one tick per frame.  The
 * sub-sprite count is figure[0]; the sub-sprite at figure + *(uint32*)(figure +
 * 4*sub + 8) holds its frame count at +6.  Called by sub_2FB2C as
 * (figure, work, frames). */
void figure_frame_run(const void *figure, void *work, int frames)
{
    const uint8_t *fig     = (const uint8_t *)figure;
    uint8_t       *wrk     = (uint8_t *)work;
    int            strip   = 0;
    int            sub     = 0;
    int            counter = 0;
    int            i;

    for (i = 0; i < frames; ++i) {
        const uint8_t *s;

        memset(wrk, 0, WORK_SIZE);
        strip = (strip + 1) % 3;
        rle_decode((const void *)(uintptr_t)dword_5413F[strip], 0, 50, wrk, 640,
                   -1);
        res_blit((void *)(uintptr_t)fig, sub, wrk, 640, -1);
        gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, wrk, 640, 320, 200);
        s = fig + *(const uint32_t *)(fig + 4 * sub + 8);
        if (++counter == s[6]) {
            counter = 0;
            if (++sub == fig[0])
                sub = 0;
        }
        svc_wait_ticks(1);
    }
}

/* 0x2EBE1 - play one attack animation between the attacker record `attacker`
 * and the defender record `defender`, stamping the attacker's figure group
 * `figure` (FIGANI.DAT) and the defender's group `base` over the 640-stride
 * work buffer `work` / 320-stride `screen`, and applying the full attack
 * evaluation map_attack_result (0x2F7B6) as the drain proceeds.
 *
 * The outer loop repeats the whole animation once, or twice on a 3% roll
 * (`util_rand()%100 < 3`) and again when the attack result raises its effect
 * flag out[4]; a zero drain value (`v30`) forces the last repeat.  When demo
 * flag dword_54133 is zero the defender's HP word (+0x40) is snapshotted into
 * `v28`, the attack is resolved into `out[6]` and the meter drains by
 * `out[5]*done/postures`; each posture sub plays its sound and, while the
 * attack still misses (out[0]!=0), resets the shake index v8 to 5 and the
 * res_blit mode v37 to 33.  The first pass (when figure[1]!=0) pre-renders every
 * sub of `figure` before the bg scroll;
 * the main pass then alternately flashes the palette (out[2] -> blue, out[1]
 * -> white), stamps `base` offset by the shake table 0x5266C / strip table
 * 0x52684 (or a mirror of it when the attacker record byte +6 is clear),
 * copies the window to VGA, advances the base frame counter and waits a tick.
 * Returns 1 to the caller (the attack driver 0x2E2B0) on the early-out when
 * dword_54133==1 and every posture has drained, otherwise the last drain
 * value.  The machine reads the uninitialised frame slot `v28` when
 * dword_54133!=0; modelled as 0. */
int figure_attack_play(int attacker, int defender, const uint8_t *figure,
                       const uint8_t *base, uint8_t *work, uint8_t *screen,
                       const void *portrait, int snd)
{
    int      off[6];   /* pixel shake offsets, dword_5266C = {0,4,9,14,18,14} */
    int      strip[6]; /* strip index step,    dword_52684 = {0,2,4,6,8,10}   */
    int      out[6];   /* map_attack_result result block                     */
    uint8_t *rec_att = (uint8_t *)dword_53A45 + 80u * (uint32_t)attacker;
    uint8_t *rec_def = (uint8_t *)dword_53A45 + 80u * (uint32_t)defender;
    int      v8      = 0;  /* shake/strip index into off[]/strip[]            */
    int      v27     = 1;  /* repeat counter                                  */
    int      v28     = 0;  /* defender HP snapshot for the drain              */
    int      v30     = 0;  /* running drain value (machine return)            */
    int      v32     = 0;  /* number of posture subs (min 1)                  */
    int      v33;          /* posture subs drained so far                     */
    int      v37     = -1; /* res_blit mode for the flashing base stamp       */
    int      v38;          /* BG strip step for this frame                    */
    int      v12;          /* sub index                                       */
    int      i;
    int      j;
    uint8_t  base_frame = 0;
    uint8_t  counter    = 0;
    uint8_t  posture    = 0; /* "attack result raised out[4]" flag            */

    memcpy(off, &unk_5266C, sizeof(off));
    memcpy(strip, &unk_52684, sizeof(strip));

    for (i = 0; i < figure[0]; ++i)
        if (figure[*(const uint32_t *)(figure + 4 * i + 8) + 4] != 0)
            ++v32;
    if (v32 == 0)
        v32 = 1;

    if ((int)(util_rand() % 100) < 3)
        v27 = 2;

    for (;;) {
        int repeat = v27--;

        if (repeat == 0)
            break;

        v33 = 0;
        if (dword_54133 == 0) {
            v28 = *(const uint16_t *)(const void *)(rec_def + 0x40);
            map_attack_result(attacker, defender, out);
            if (posture == 0 && out[4] != 0) {
                posture = 1;
                ++v27;
            }
        }

        v12 = 0;
        if (dword_54133 != 0)
            memset(out, 0, sizeof(out));

        if (figure[1] != 0) {
            memset(work, 0, WORK_SIZE);
            if (rec_att[6] != 0) {
                while (v12 < figure[2]) {
                    const uint8_t *s =
                        figure + *(const uint32_t *)(figure + 4 * v12 + 8);

                    if (s[5] != 0)
                        svc_play_sfx((const void *)(intptr_t)snd, s[5], 1);
                    gfx_copy_rows(work + 320, 640, screen, 320, 320, 200);
                    res_blit((void *)(uintptr_t)figure, v12, work + 320, 640,
                             -1);
                    gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320,
                                  work + 320, 640, 320, 200);
                    svc_wait_ticks(s[6]);
                    ++v12;
                }
                figure_bg_scroll(defender, base, screen, work,
                                 (const void *)(uintptr_t)dword_5413B);
            } else {
                while (v12 < figure[2]) {
                    const uint8_t *s =
                        figure + *(const uint32_t *)(figure + 4 * v12 + 8);

                    if (s[5] != 0)
                        svc_play_sfx((const void *)(intptr_t)snd, s[5], 1);
                    gfx_copy_rows(work, 640, screen, 320, 320, 200);
                    res_blit((void *)(uintptr_t)figure, v12, work, 640, -1);
                    gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, work, 640,
                                  320, 200);
                    svc_wait_ticks(s[6]);
                    ++v12;
                }
                figure_bg_scroll_portrait(defender, base, portrait, screen,
                                          work,
                                          (const void *)(uintptr_t)dword_5413B);
            }
        }

        for (;;) {
            const uint8_t *s;

            if (v12 >= figure[0]) {
                if (v30 == 0)
                    v27 = 0;
                if (v27 != 0 && figure[1] == 1) {
                    memset(work, 0, WORK_SIZE);
                    if (rec_att[6] != 0) {
                        gfx_copy_rows(work, 640, screen, 320, 320, 200);
                        res_blit((void *)(uintptr_t)base, 0, work, 640, -1);
                        figure_bg_scroll_portrait(
                            attacker, figure, portrait, screen, work,
                            (const void *)(uintptr_t)dword_54137);
                    } else {
                        gfx_copy_rows(work + 320, 640, screen, 320, 320, 200);
                        res_blit((void *)(uintptr_t)base, 0, work + 320, 640,
                                 -1);
                        figure_bg_scroll(attacker, figure, screen, work,
                                         (const void *)(uintptr_t)dword_54137);
                    }
                }
                break;
            }

            s = figure + *(const uint32_t *)(figure + 4 * v12 + 8);

            if (s[4] != 0) {
                ++v33;
                v30 = v28 - out[5] * v33 / v32;
                if (v30 < 0)
                    v30 = 0;
                *(uint16_t *)(void *)(rec_def + 0x40) = (uint16_t)v30;
                if (dword_54133 == 0)
                    ui_status_panel_at((int)(uintptr_t)screen, defender);
                if (out[0] != 0) {
                    if (s[5] != 0)
                        svc_play_sfx((const void *)(intptr_t)snd, 0, 1);
                } else {
                    v8  = 5;
                    v37 = 33;
                    if (s[5] != 0)
                        svc_play_sfx((const void *)(intptr_t)snd, s[5], 1);
                }
            } else if (s[5] != 0) {
                svc_play_sfx((const void *)(intptr_t)snd, s[5], 1);
            }

            for (j = 0; j < s[6]; ++j) {
                const uint8_t *bs;

                v38 = strip[v8];
                if (figure[1] != 0)
                    v38 = 0;
                gfx_copy_rows(work + 16040, 400, screen, 320, 320, 200);
                if (rec_att[6] != 0) {
                    if ((s[7] & 1) != 0)
                        res_blit((void *)(uintptr_t)figure, v12, work + 16040,
                                 400, -1);
                    res_blit((void *)(uintptr_t)base, base_frame,
                             work + 16040 - off[v8] - 400 * v38, 400, v37);
                    if ((s[7] & 1) == 0)
                        res_blit((void *)(uintptr_t)figure, v12, work + 16040,
                                 400, -1);
                } else {
                    if ((s[7] & 1) == 0)
                        res_blit((void *)(uintptr_t)figure, v12, work + 16040,
                                 400, -1);
                    res_blit((void *)(uintptr_t)base, base_frame,
                             work + 16040 + off[v8] + 400 * v38, 400, v37);
                    if ((s[7] & 1) != 0)
                        res_blit((void *)(uintptr_t)figure, v12, work + 16040,
                                 400, -1);
                }
                gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, work + 16040,
                              400, 320, 200);

                if (s[4] == 1 && v33 == v32) {
                    if (out[2] != 0) {
                        outp(0x3C8, 0);
                        outp(0x3C9, 1);
                        outp(0x3C9, 0x20);
                        outp(0x3C9, 0);
                        j___delay(20);
                        outp(0x3C8, 0);
                        outp(0x3C9, 0);
                        outp(0x3C9, 0);
                        outp(0x3C9, 0);
                    }
                    if (out[1] != 0) {
                        outp(0x3C8, 0);
                        outp(0x3C9, 0x3F);
                        outp(0x3C9, 0x3F);
                        outp(0x3C9, 0x3F);
                        j___delay(20);
                        outp(0x3C8, 0);
                        outp(0x3C9, 0);
                        outp(0x3C9, 0);
                        outp(0x3C9, 0);
                        j___delay(40);
                    }
                }

                bs = base + *(const uint32_t *)(base + 4 * base_frame + 8);
                if ((uint8_t)(counter + 1) == bs[6]) {
                    counter = 0;
                    if ((uint8_t)(base_frame + 1) == base[0])
                        base_frame = 0;
                    else
                        ++base_frame;
                } else {
                    ++counter;
                }

                if (v8 != 0)
                    --v8;
                v37 = -1;
                svc_wait_ticks(1);
            }

            if (dword_54133 == 1 && v33 == v32)
                return 1;

            ++v12;
        }
    }
    return v30;
}

/* 0x2E95B - reload FD2.TMP into the global DATO buffer dword_53A61.  The whole
 * 0x32A00-byte file is read into a fresh block (the previous one is dropped by
 * the caller); the only caller is the attack-view driver 0x2E2B0.  Returns the
 * fclose result, which the caller ignores. */
int figure_tmp_load(void)
{
    FILE *fp = fopen(aFd2Tmp_1, aRb_15); /* "FD2.TMP", "rb" */

    dword_53A61 = guest_malloc(0x32A00);
    fread(dword_53A61, 1, 0x32A00, fp);
    return fclose(fp);
}

/* 0x2E9A8 - slide the two FIGANI plates apart over the figure work buffer and
 * flash the DAC, one strip at a time.  With record[6]==0 the plate `plate_a`
 * walks in from work+320 while `plate_b` stays fixed; with record[6]!=0 the
 * roles swap (plate_b fixed at work, plate_a walking) and the portrait strip is
 * decoded at work+10*i each step.  In both branches the 640-stride `work`
 * window is flattened into `screen` and published to VGA, and the DAC range is
 * faded by 6*i.  `flag` (the caller's defender-clear indicator) suppresses the
 * fixed-plate stamp and the first portrait decode.  `portrait` is a TAI.DAT
 * strip.  Argument order and branch conditions decoded from the disassembly;
 * the machine's final `jl 0x2E956` is the shared epilogue of 0x2E2B0, i.e. a
 * plain return. */
void figure_attack_scroll(int unit, int flag, const void *plate_a,
                          const void *plate_b, uint8_t *work, uint8_t *screen,
                          const void *portrait)
{
    const uint8_t *rec = (const uint8_t *)dword_53A45 + 80 * unit;
    int            i;

    if (rec[6] == 0) {
        if (flag == 0)
            rle_decode(portrait, 164, 157, screen, 320, -1);
        for (i = 8; i >= 0; --i) {
            gfx_copy_rows(work + 320, 640, screen, 320, 320, 200);
            res_blit((void *)(uintptr_t)plate_a, 0, work + 320 - 10 * i, 640,
                     -1);
            if (flag == 0)
                res_blit((void *)(uintptr_t)plate_b, 0, work + 320, 640, -1);
            gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, work + 320, 640,
                          320, 200);
            pal_fade_range(0, 255, 6 * i);
        }
        return;
    }

    for (i = 8; i >= 0; --i) {
        gfx_copy_rows(work, 640, screen, 320, 320, 200);
        if (flag == 0)
            res_blit((void *)(uintptr_t)plate_b, 0, work, 640, -1);
        rle_decode(portrait, 164, 157, work + 10 * i, 640, -1);
        res_blit((void *)(uintptr_t)plate_a, 0, work + 10 * i, 640, -1);
        gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, work, 640, 320, 200);
        pal_fade_range(0, 255, 6 * i);
    }
    rle_decode(portrait, 164, 157, screen, 320, -1);
}

/* 0x2E2B0 - the map attack view driver: shows `attacker`'s and `defender`'s
 * FIGANI figures over the BG.DAT scroll, with the TAI.DAT portraits, then plays
 * the attack animation (0x2EBE1) and, when the defender is cleared
 * (ev5_1F0DC(attacker,defender)==1), the counter animation, before reloading
 * FD2.TMP / FD2SHAP.DAT and restoring the map.  The demo flag
 * dword_54133 selects the two-plate branch (dword_54137/dword_5413F strips and
 * the second plate at 3*row+1).  Called by ev3_35730 as (0x11, arg).  The
 * machine's 0x64-byte stack probe (sub_3702F) is a chkstk and not a callee. */
void figure_attack_view(int attacker, int defender)
{
    const uint8_t *rec1    = (const uint8_t *)dword_53A45 + 80 * attacker;
    uint8_t       *rec2    = (uint8_t *)dword_53A45 + 80 * defender;
    const uint8_t *rec0;          /* the record whose byte 6 is clear */
    uint8_t        cell[8];       /* map_cell_info output (byte 6 = DAT index)*/
    uint8_t       *screen  = guest_malloc(0xFA00);  /* 320-stride buffer  (v4) */
    uint8_t       *work    = guest_malloc(0x1F400); /* 640-stride buffer  (v5) */
    void          *tai     = NULL; /* TAI.DAT strip            (v23) */
    void          *fig_def = NULL; /* defender FIGANI plate    (p)   */
    void          *fig_att = NULL; /* attacker FIGANI plate    (v22) */
    void          *fig_att_next = NULL; /* attacker plate 3*att+1 (v19) */
    void          *fig_def_next = NULL; /* defender plate 3*def+1 (v20) */
    int            att_row = rec1[7];  /* v15 */
    int            def_row = rec2[7];  /* v16 (before any swap) */
    int            bg_att;             /* v18 */
    int            bg_def;             /* v14 */
    int            bg_tai;             /* v8  */
    int            next_flag;          /* v17 */

    dword_5413B = NULL;
    memset(screen, 0, 0xFA00);

    if (rec1[6] != 0) {
        rec0 = rec2;
        rec2 = (uint8_t *)rec1;
    } else {
        rec0 = rec1;
    }

    bg_att = byte_52470[dword_53C03];
    if (!((rec0[32] == 0x13 || rec0[31] == 4 || rec0[31] == 5) &&
          bg_att != 0 && rec0[7] != 0x1c)) {
        map_cell_info(rec0[0], rec0[1], cell);
        bg_att = cell[6];
    }
    map_cell_info(rec2[0], rec2[1], cell);

    bg_tai = byte_52470[dword_53C03];
    bg_def = bg_tai;
    if ((rec2[32] == 0x13 || rec2[31] == 4 || rec2[31] == 5) &&
        rec2[7] != 0x1c) {
        if (bg_def == 0)
            bg_def = cell[6];
    } else {
        bg_tai = cell[6];
        bg_def = cell[6];
    }

    if (dword_54133 != 0) {
        bg_tai = dword_54133;
        bg_att = dword_54133;
        if (att_row == 0x1a || att_row == 0x36 || def_row == 0x37)
            bg_tai = 3;
    }

    tai = res_load(aTaiDat, tai, bg_tai);
    fig_def = res_load(aFiganiDat, fig_def, 3 * def_row);
    fig_att = res_load(aFiganiDat, fig_att, 3 * att_row);
    fig_att_next = res_load(aFiganiDat, fig_att_next, 3 * att_row + 1);
    next_flag = *((const uint8_t *)fig_att_next + 1);

    pal_fade_out();
    dword_5413B = (uint32_t)(uintptr_t)res_load(
        aBgDat, (void *)(uintptr_t)dword_5413B, bg_att);
    if (dword_54133 == 0)
        ui_status_panel_at((int)(uintptr_t)screen, attacker);
    if (ev5_1F0DC(attacker, defender) == 1 || dword_54133 != 0)
        fig_def_next = res_load(aFiganiDat, fig_def_next, 3 * def_row + 1);

    if (next_flag != 0) {
        dword_54137 = NULL;
        dword_5413F[0] = 0;
        dword_54143 = NULL;
        dword_54147 = NULL;
        dword_54137 = (uint32_t)(uintptr_t)res_load(aBgDat, NULL, bg_def);
        dword_5413F[0] =
            (uint32_t)(uintptr_t)res_load(aBgDat,
                                          (void *)(uintptr_t)dword_5413F[0], 0);
        dword_54143 =
            (uint32_t)(uintptr_t)res_load(aBgDat,
                                          (void *)(uintptr_t)dword_54143, 1);
        dword_54147 =
            (uint32_t)(uintptr_t)res_load(aBgDat,
                                          (void *)(uintptr_t)dword_54147, 2);
        if (rec1[6] == 0) {
            uint32_t tmp = dword_5413B;

            dword_5413B = dword_54137;
            dword_54137 = tmp;
        }
        rle_decode((const void *)(uintptr_t)dword_54137, 0, 50, screen, 320,
                   -1);
    } else {
        rle_decode((const void *)(uintptr_t)dword_5413B, 0, 50, screen, 320,
                   -1);
        if (dword_54133 == 0)
            ui_status_panel_at((int)(uintptr_t)screen, defender);
    }

    if (dword_54133 == 0) {
        dword_5414B = (uint32_t)(uintptr_t)ev5_314DE(fig_att_next);
        dword_5414F = (uint32_t)(uintptr_t)ev5_314DE(fig_def_next);
    }

    figure_attack_scroll(attacker, next_flag, fig_att, fig_def, work, screen,
                         tai);

    if (figure_attack_play(attacker, defender, fig_att_next, fig_def, work,
                           screen, tai, dword_5414B) != 0 &&
        ev5_1F0DC(attacker, defender) == 1 && dword_54133 == 0) {
        figure_attack_play(defender, attacker, fig_def_next, fig_att, work,
                           screen, tai, dword_5414F);
    }
    if (dword_54133 != 0) {
        dword_54133 = 1;
        figure_attack_play(defender, attacker, fig_def_next, fig_att, work,
                           screen, tai, dword_5414F);
    }

    if (next_flag == 0 && dword_54133 == 0) {
        gfx_copy_rows(work, 640, screen, 320, 320, 200);
        rle_decode(tai, 164, 157, work, 640, -1);
        res_blit((void *)(uintptr_t)fig_def, 0, work, 640, -1);
        res_blit((void *)(uintptr_t)fig_att, 0, work, 640, -1);
        gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, work, 640, 320, 200);
    }

    guest_free(screen);
    guest_free(work);
    guest_free(fig_def);
    guest_free(fig_att);
    guest_free(fig_att_next);
    guest_free((void *)(uintptr_t)dword_5413B);
    guest_free(tai);
    if (next_flag != 0) {
        guest_free((void *)(uintptr_t)dword_54137);
        guest_free((void *)(uintptr_t)dword_5413F[0]);
        guest_free((void *)(uintptr_t)dword_54143);
        guest_free((void *)(uintptr_t)dword_54147);
    } else {
        guest_free(fig_def_next);
    }

    if (dword_54133 == 0) {
        dword_53A49 = guest_malloc(0x25680);
        dword_53A5D = res_load(
            aFdshapDat, dword_53A5D,
            2 * *(const uint8_t *)(uintptr_t)dword_53A55);
        figure_tmp_load();
        svc_wait_ticks(6);
        pal_fade_out();
        memset((void *)(uintptr_t)0xA0000, 0, 0xFA00);
        map_view_update(1);
        svc_play_sfx((const void *)(uintptr_t)dword_5414B, -1, 1);
        if (dword_5414B != 0)
            guest_free((void *)(uintptr_t)dword_5414B);
        if (dword_5414F != 0)
            guest_free((void *)(uintptr_t)dword_5414F);
        pal_fade_in();
    }
}
