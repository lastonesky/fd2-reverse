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

/* 0x2DFC8 - play a unit's figure animation over its map cell's background.
 * Loads TAI.DAT (index 3), the two FIGANI.DAT groups 3*rec[7] and 3*rec[7]+1
 * and the BG.DAT sub-image cell[6], fades out, stamps the status panel and the
 * background into the 320-stride screen buffer, opens the sound handle and
 * slides the figure's two plates in (figure_attack_scroll).  Then, for every
 * frame of the +1 group, it resets the 640-stride work buffer from the screen,
 * blits the frame over it, publishes the 320x200 window and waits the frame's
 * tick count (playing the frame's SFX first).  It frees the six buffers,
 * reloads FD2SHAP.DAT into dword_53A5D, then clears the visible frame, refreshes
 * the map view, stops the sound and fades back in.  Called only by the AI
 * action driver 0x15055, which ignores the result.
 *
 * The frame loop's counter is EDI, never written by this function (its entry
 * value is the caller's reachable-cell count); it is modelled as 0 here, as for
 * the other uninitialised-register locals (playbook 6). */
void figure_scene_play(int unit)
{
    uint8_t        cell[8];
    uint8_t       *rec    = (uint8_t *)dword_53A45 + 80 * unit;
    int            group  = rec[7];
    uint8_t       *screen;
    uint8_t       *work;
    const uint8_t *tai;
    const uint8_t *fig_a; /* FIGANI group 3*group   */
    const uint8_t *fig_b; /* FIGANI group 3*group+1 */
    int            i;

    dword_5413B = 0;
    guest_free((void *)(uintptr_t)dword_53A49);
    guest_free((void *)(uintptr_t)dword_53A5D);
    dword_53A5D = NULL;

    screen = guest_malloc(SCR_SIZE);
    work   = guest_malloc(WORK_SIZE);
    memset(screen, 0, SCR_SIZE);

    map_cell_info(rec[0], rec[1], cell);
    tai   = res_load(aTaiDat, NULL, 3);
    fig_a = res_load(aFiganiDat, NULL, 3 * group);
    fig_b = res_load(aFiganiDat, NULL, 3 * group + 1);
    pal_fade_out();
    dword_5413B = (uint32_t)(uintptr_t)res_load(aBgDat, NULL, cell[6]);
    ui_status_panel_at((int)(uintptr_t)screen, unit);
    rle_decode((const void *)(uintptr_t)dword_5413B, 0, 50, screen, 320, -1);
    dword_5414B = (uint32_t)(uintptr_t)ev5_314DE(fig_b);
    figure_attack_scroll(unit, 1, fig_a, fig_a, work, screen, tai);

    for (i = 0; i < fig_b[0]; ++i) {
        const uint8_t *f =
            fig_b + *(const uint32_t *)(const void *)(fig_b + 4 * i + 8);

        if (f[5] != 0)
            svc_play_sfx((const void *)(uintptr_t)dword_5414B, f[5], 1);
        gfx_copy_rows(work, 320, screen, 320, 320, 200);
        res_blit((void *)(uintptr_t)fig_b, i, work, 320, -1);
        gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, work, 320, 320, 200);
        svc_wait_ticks(f[6]);
    }

    guest_free(screen);
    guest_free(work);
    guest_free((void *)(uintptr_t)fig_a);
    guest_free((void *)(uintptr_t)fig_b);
    guest_free((void *)(uintptr_t)dword_5413B);
    guest_free((void *)(uintptr_t)tai);

    dword_53A49 = guest_malloc(0x25680);
    dword_53A5D = res_load(aFdshapDat, dword_53A5D,
                           2 * *(const uint8_t *)(uintptr_t)dword_53A55);
    svc_wait_ticks(6);
    pal_fade_out();
    memset((void *)(uintptr_t)VGA_FRAME, 0, SCR_SIZE);
    map_view_update(1);
    svc_play_sfx((const void *)(uintptr_t)dword_5414B, -1, 1);
    if (dword_5414B != 0)
        guest_free((void *)(uintptr_t)dword_5414B);
    pal_fade_in();
}

/* 0x2CF30 - the party figure presentation driven by the effect dispatcher
 * 0x2FF01.  `unit` selects the status-panel record and the FIGANI/BG base
 * strips; `mode` (18 default / 24 / 28 / 29) selects the palette variant and
 * how many postures the HP drain is split over; `units[0..count-1]` are the
 * records whose figure groups are played in turn.
 *
 * It frees the previous screen / shape / DATO buffers, loads the BG.DAT and
 * TAI.DAT strips of the displayed cell's map byte 6 and the BG strip picked by
 * figure_bg_pick, plus the FIGANI group 3*rec[7] and 3*rec[7]+2 and the three
 * rotating BG strips dword_5413F.  After fading out it loads every unit's
 * FIGANI group, slides the plates in (figure_attack_scroll) and plays the base
 * group (figure_play); then for each unit it snapshots the HP word (+0x40),
 * calls unit_damage with `sel*rec[+0x48]/10 - rec[+0x4a]`, restores the
 * snapshot and steps the posture sub-sprites of group +2.  A posture sub
 * (byte +4==1) consumes the record, drains the HP word over `posture_n`
 * postures and stamps the status panel; every frame substitute the shake table
 * 0x52656 at index `step` into the animation cursor (anim_cycle_frame), blits
 * the posture sprite and waits one tick.  Finally it frees every buffer, stops
 * the sound, reloads FD2SHAP.DAT / FD2.TMP and restores the map view.
 *
 * True ABI = cdecl, four stack arguments: the single call site (0x3003f) pushes
 * four values, but the `push 0x100; call 0x3702F` Watcom stack probe makes
 * Hex-Rays drop all but the first, so its `int(int)` prototype is wrong.
 * TRAP: the machine frees `v19` (the BG strip `bg_a`) twice on teardown; kept
 * as is. */
void figure_party_show(int unit, int mode, int count, const uint8_t *units)
{
    uint8_t        cell[8];       /* map_cell_info output (byte 6 = DAT index)*/
    uint8_t        shake[6];      /* 0x52656 = {0,4,9,14,18,14}               */
    const uint8_t *rec;           /* this unit's 80-byte record               */
    void          *p[30];         /* per-unit FIGANI group 3*rec[7]           */
    void          *bg_a   = NULL; /* v19  BG.DAT strip (cell[6])              */
    void          *tai    = NULL; /* v21  TAI.DAT strip (cell[6])             */
    void          *bg_pick = NULL;/* v23  BG.DAT strip (figure_bg_pick)       */
    void          *fig_a  = NULL; /* v20  FIGANI group 3*rec[7]               */
    void          *fig_b  = NULL; /* v30  FIGANI group 3*rec[7]+2             */
    uint8_t       *screen;        /* v28  320-stride 0xFA00 buffer            */
    uint8_t       *work;          /* v27  640-stride 0x1F400 buffer           */
    int            i;
    int            j;
    int            k;
    int            sel;           /* switch(mode) value                       */
    int            steps;         /* v22  sel * rec[+0x48] / 10               */
    int            base_hp;       /* v25  rec[+0x40] snapshot                 */
    int            dmg;           /* v24  clamped unit_damage result          */
    uint8_t        posture_n;     /* v35  8 for mode 28 else 1                */
    uint8_t        posture_i;     /* v34  postures drained so far             */
    uint8_t        step;          /* v33  shake index (5 on a posture)        */
    int            blit_mode;     /* v32  res_blit mode (33 on a posture)     */

    memcpy(shake, &unk_52656, sizeof(shake));

    guest_free(dword_53A61);      /* buf / DATO atlas                         */
    guest_free(dword_53A49);      /* screen frame                             */
    guest_free(dword_53A5D);      /* map shape buffer                         */
    dword_53A5D = NULL;

    for (i = 0; i < 30; ++i)
        p[i] = NULL;

    switch (mode) {
    case 24: sel = 15; break;
    case 28: sel = 20; break;
    case 29: sel = 12; break;
    default: sel = 18; break;
    }
    rec   = (const uint8_t *)dword_53A45 + 80 * unit;
    steps = sel * *(const int16_t *)(const void *)(rec + 0x48) / 10;

    map_cell_info(rec[0], rec[1], cell);

    bg_a    = res_load(aBgDat, NULL, cell[6]);
    tai     = res_load(aTaiDat, NULL, cell[6]);
    bg_pick = res_load(aBgDat, NULL, figure_bg_pick(count, units));

    dword_5413F[0] = 0;
    dword_54143    = 0;
    dword_54147    = 0;
    dword_5413F[0] =
        (uint32_t)(uintptr_t)res_load(aBgDat, NULL, 0);
    dword_54143 = (uint32_t)(uintptr_t)res_load(
        aBgDat, (void *)(uintptr_t)dword_54143, 1);
    dword_54147 = (uint32_t)(uintptr_t)res_load(
        aBgDat, (void *)(uintptr_t)dword_54147, 2);

    screen = guest_malloc(SCR_SIZE);
    work   = guest_malloc(WORK_SIZE);
    memset(screen, 0, SCR_SIZE);
    ui_status_panel_at((int)(uintptr_t)screen, unit);

    if (mode == 28) {
        rle_decode(bg_pick, 0, 50, screen, 320, -1);
        ui_status_panel_at((int)(uintptr_t)screen, units[0]);
    } else {
        rle_decode(bg_a, 0, 50, screen, 320, -1);
    }

    i      = 3 * rec[7];
    fig_a  = res_load(aFiganiDat, NULL, i);
    fig_b  = res_load(aFiganiDat, NULL, i + 2);
    dword_5414B = (uint32_t)(uintptr_t)ev5_314DE(fig_b);
    pal_fade_out();

    for (i = 0; i < count; ++i) {
        const uint8_t *r =
            (const uint8_t *)dword_53A45 + 80 * units[i];

        p[i] = res_load(aFiganiDat, p[i], 3 * r[7]);
    }

    figure_attack_scroll(unit, mode != 28, fig_a, p[0], work, screen, tai);
    figure_play(unit, mode, fig_b, p[0], work, screen, bg_a, tai);

    for (i = 0; i < count; ++i) {
        const uint8_t *r = (const uint8_t *)dword_53A45 + 80 * units[i];

        memset(work, 0, WORK_SIZE);
        gfx_copy_rows(work + 320, 640, (void *)(uintptr_t)VGA_FRAME, 320,
                      320, 200);
        if (mode != 28)
            figure_bg_scroll(units[i], p[i], screen, work, bg_pick);
        anim_cycle_frame(p[i], 0, work, 320);

        base_hp = *(const int16_t *)(const void *)(r + 0x40);
        dmg     = unit_damage(units[i],
                              steps - *(const int16_t *)(const void *)(r + 0x4a));
        if (dmg > base_hp)
            dmg = base_hp;
        *(uint16_t *)(void *)((uint8_t *)r + 0x40) = (uint16_t)base_hp;

        posture_n = (mode == 28) ? 8 : 1;
        posture_i = 0;

        for (j = ((const uint8_t *)fig_b)[2];
             (int)((const uint8_t *)fig_b)[0] > j; ++j) {
            const uint8_t *sub = (const uint8_t *)fig_b +
                *(const uint32_t *)(const void *)((const uint8_t *)fig_b +
                                                  4 * j + 8);

            if (sub[5] != 0)
                svc_play_sfx((const void *)(uintptr_t)dword_5414B, sub[5], 1);
            if (sub[4] == 1) {
                step      = 5;
                blit_mode = 33;
                ++posture_i;
                *(uint16_t *)(void *)((uint8_t *)r + 0x40) = (uint16_t)(
                    base_hp - (uint8_t)posture_i * dmg / posture_n);
                ui_status_panel_at((int)(uintptr_t)screen, units[i]);
            }
            for (k = 0; k < sub[6]; ++k) {
                uint8_t *win = work + 320;

                gfx_copy_rows(win, 640, screen, 320, 320, 200);
                anim_cycle_frame(p[i], blit_mode, win - shake[step], 640);
                res_blit(fig_b, j, win, 640, -1);
                gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, win, 640, 320,
                              200);
                svc_wait_ticks(1);
                if (step != 0)
                    --step;
                blit_mode = -1;
            }
        }
    }

    for (i = 0; i < count; ++i)
        guest_free(p[i]);
    guest_free(bg_a);
    guest_free(bg_pick);
    guest_free(tai);
    guest_free(bg_a);   /* the machine frees v19 a second time */
    guest_free(screen);
    guest_free(work);
    guest_free((void *)(uintptr_t)dword_5413F[0]);
    guest_free((void *)(uintptr_t)dword_54143);
    guest_free((void *)(uintptr_t)dword_54147);
    guest_free(fig_a);
    guest_free(fig_b);
    svc_play_sfx((const void *)(uintptr_t)dword_5414B, -1, 1);
    if (dword_5414B != 0)
        guest_free((void *)(uintptr_t)dword_5414B);

    dword_53A49 = guest_malloc(0x25680);
    i = *(const uint8_t *)(uintptr_t)dword_53A55;
    dword_53A5D = res_load(aFdshapDat, dword_53A5D, 2 * i);
    figure_tmp_load();
    svc_wait_ticks(6);
    pal_fade_out();
    memset((void *)(uintptr_t)VGA_FRAME, 0, SCR_SIZE);
    map_view_update(1);
    pal_fade_in();
}

/* 0x2D80D - the `mode >= 32` sibling of figure_party_show, driven by the effect
 * driver 0x2FF01 (modes 32..35).  Shows record `unit`'s FIGANI groups over the
 * BG/TAI strips of the unit's map cell, stamps the FDOTHER plate entry
 * `mode + 33`, plays the palette blend table at 0x5265C and finally runs the
 * map action for `mode` over `units[0..count-1]`.
 *
 * Faithful details worth noting:
 *  - the 16-byte blend table at 0x5265C is copied to the stack and read as four
 *    parallel byte columns: row = mode-32 gives r=0x5265C[row], g=0x52660[row],
 *    b=0x52664[row] and the FDOTHER entry index 0x52668[row].  Hex-Rays renders
 *    the copy as `qmemcpy(v21,"?355[]^",...)` and the index read as `v17[mode]`
 *    (a frame-base artifact); the disassembly (`2d81e..2d83e`, `2d985`,
 *    `2dc2c..2dc57`) is authoritative.
 *  - the FIGANI groups loaded are 3*rec[7] and 3*rec[7]+1 (NOT +2 as
 *    figure_party_show uses).
 *  - `single res_blit` of the plate over the screen for modes 33/34 only.
 *  - the k loop's sound effect differs per mode; every path ends with a
 *    svc_wait_ticks(2).
 *  - `dword_54153` gets the FDOTHER entry whose index is the blend table's 4th
 *    column byte; it is the sound bank freed at the end.
 * ABI = cdecl, four stack arguments (the chkstk probe hides them from
 * Hex-Rays).  Hand-restored, compile-verified, no differential harness yet. */
void figure_demo_show(int unit, int mode, int count, const uint8_t *units)
{
    uint8_t        cell[8];       /* map_cell_info output (byte 6 = DAT index) */
    uint8_t        tab[16];       /* 0x5265C blend table                      */
    const uint8_t *rec;           /* this unit's 80-byte record               */
    void          *tai;           /* TAI.DAT strip of the unit's cell         */
    void          *bg;            /* BG.DAT strip of the unit's cell          */
    void          *fig_a;         /* FIGANI group 3*rec[7]                    */
    void          *fig_b;         /* FIGANI group 3*rec[7]+1                  */
    void          *fd_plate;      /* FDOTHER entry mode+33                    */
    uint8_t       *screen;        /* 320-stride 0xFA00 buffer                 */
    uint8_t       *work;          /* 640-stride 0x1F400 buffer                */
    int            i;
    int            j;
    int            k;
    int            m;
    int            n;
    uint8_t        row;           /* mode - 32 (blend table row)              */

    memcpy(tab, (const void *)&dword_5265C, sizeof(tab));
    row = (uint8_t)(mode - 32);

    guest_free(dword_53A49);
    guest_free(dword_53A5D);
    dword_53A5D = NULL;

    rec = (const uint8_t *)dword_53A45 + 80 * unit;
    map_cell_info(rec[0], rec[1], cell);

    tai = res_load(aTaiDat, NULL, cell[6]);
    bg  = res_load(aBgDat, NULL, cell[6]);

    screen = guest_malloc(SCR_SIZE);
    work   = guest_malloc(WORK_SIZE);
    memset(screen, 0, SCR_SIZE);
    rle_decode(bg, 0, 50, screen, 320, -1);
    ui_status_panel_at((int)(uintptr_t)screen, unit);
    pal_fade_out();

    i     = 3 * rec[7];
    fig_a = res_load(aFiganiDat, NULL, i);
    fig_b = res_load(aFiganiDat, NULL, i + 1);

    fd_plate = res_load(aFdotherDat, NULL, mode + 33);
    dword_54153 = 0;
    dword_54153 =
        (uint32_t)(uintptr_t)res_load(aFdotherDat, NULL, tab[12 + row]);

    figure_attack_scroll(unit, 1, fig_a, NULL, work, screen, tai);
    figure_play(unit, mode, fig_b, fig_b, work, screen, bg, tai);
    svc_wait_ticks(6);

    for (i = 0; i < 8; ++i) {
        gfx_copy_rows(work, 640, screen, 320, 320, 200);
        res_blit(fig_a, 0, work + 20 * i, 640, -1);
        gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, work, 640, 320, 200);
        svc_wait_ticks(1);
    }
    for (j = 8; j >= 0; --j) {
        gfx_copy_rows(work, 640, screen, 320, 320, 200);
        res_blit(fd_plate, 0, work + 30 * j, 640, -1);
        gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, work, 640, 320, 200);
        svc_wait_ticks(1);
    }
    if (mode == 33 || mode == 34)
        res_blit(fd_plate, 0, screen, 320, -1);

    for (k = 1; k < *(const uint8_t *)fd_plate; ++k) {
        memmove(work, screen, SCR_SIZE);
        res_blit(fd_plate, k, work, 320, -1);
        gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, work, 320, 320, 200);
        if (mode == 34 && k == 2)
            svc_play_sfx((const void *)(uintptr_t)dword_54153, 1, 1);
        else if (mode == 35 && k == 1)
            svc_play_sfx2((const void *)(uintptr_t)dword_54153, 2, k);
        else if (mode == 33 && k == 6)
            svc_play_sfx((const void *)(uintptr_t)dword_54153, 1, 1);
        else if (mode == 32 && k == 1)
            svc_play_sfx2((const void *)(uintptr_t)dword_54153, 2, k);
        svc_wait_ticks(2);
    }

    for (m = 0; m <= 10; ++m) {
        if (m % 2 == 0)
            svc_play_sfx((const void *)(uintptr_t)dword_54153, 1, 1);
        memmove(work, screen, SCR_SIZE);
        res_blit(fd_plate, *(const uint8_t *)fd_plate - 2 + (m & 1), work, 320,
                 -1);
        gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, work, 320, 320, 200);
        svc_wait_ticks(2);
        pal_blend_to(0, 255, 40 - 4 * m, tab[row], tab[4 + row], tab[8 + row]);
    }

    guest_free(fd_plate);
    guest_free(bg);
    guest_free(tai);
    guest_free(screen);
    guest_free(work);
    guest_free(fig_a);
    guest_free(fig_b);

    dword_53A49 = guest_malloc(0x25680);
    i = 2 * *(const uint8_t *)(uintptr_t)dword_53A55;
    dword_53A5D = res_load(aFdshapDat, dword_53A5D, i);

    pal_blend_to(0, 255, 0, tab[row], tab[4 + row], tab[8 + row]);
    memset((void *)(uintptr_t)VGA_FRAME, 0, SCR_SIZE);
    map_view_update(1);
    for (n = 0; n <= 40; ++n) {
        pal_blend_to(0, 255, n, tab[row], tab[4 + row], tab[8 + row]);
        j___delay(6);
    }
    svc_play_sfx((const void *)(uintptr_t)dword_54153, -1, 1);
    guest_free((void *)(uintptr_t)dword_54153);
    dword_53EC4 = 0;
    ev5_1D4CB();

    switch (mode) {
    case 32:
        map_action_anim_resolve(unit, count, units, 32);
        break;
    case 33:
        for (i = 0; i < count; ++i)
            memset((uint8_t *)dword_53A45 + 80 * units[i] + 0x25, 0, 3);
        map_action_heal_list(unit, count, units, 800);
        break;
    case 34:
        map_action_grow_x(unit, count, units);
        dword_53EC4 = 0;
        map_action_grow_y(unit, count, units);
        dword_53EC4 = 0;
        map_action_widen(unit, count, units);
        break;
    case 35:
        map_action_damage_records(unit, 26, count, units, 37);
        dword_53EC4 = 0;
        map_action_damage_records(unit, 22, count, units, 39);
        dword_53EC4 = 0;
        map_action_damage_records(unit, 27, count, units, 38);
        break;
    default:
        break;
    }
    menu_stop_and_free_music();
}

/* funcs_30469[] @0x524C6, in table order (see fx.h).  The verified fx.c asset
 * keeps the emitters reachable through the original data-segment table; the
 * driver 0x2FF01 dispatches through it too.  Mirrored here by semantic name so
 * the standalone profile does not depend on the data segment holding host
 * function pointers. */
typedef int (*fx_frame_fn)(int rec, const void *buf, void *dst, int pitch,
                           int selector);

static const fx_frame_fn fx_table[10] = {
    fx_dots7, fx_dots8, fx_blob, fx_dots12, fx_dots6,
    fx_dots6b, fx_dots5, fx_dots3, fx_dots16, fx_toggle,
};

/* 0x2FF01 - the figure/effect dispatcher driven by the two call sites 0x15400
 * (sub_15311) and 0x1D43C (sub_1CFF0).
 *
 * mode >= 32 delegates to figure_demo_show (0x2D80D); mode 24 or 28..31
 * delegates to figure_party_show (0x2CF30).  Every other mode (< 24, 25..27,
 * 0..9 in practice) runs the inline effect: load the BG.DAT / TAI.DAT strips
 * of the shown cell (or the figure_bg_pick / per-mode default), the FIGANI
 * groups 3*rec[7] and 3*rec[7]+2 (or +1), the FDOTHER engine portraits and the
 * FDOTHER sound bank, fade out and scroll the plates in (figure_attack_scroll),
 * play the base group (figure_play), then drive the per-frame emitters
 * funcs_30469[mode] over a 640-stride 0x2A300 work buffer: a nine-frame lead-in
 * (selector 0/1/2), one drain block per unit in `units[0..count-1]` (selectors
 * 3..5, draining the record's HP word +0x40 toward the unit_action_damage
 * result), a tail (selector 6/7/8) and, for mode 9, the bright slide-out.  It
 * then frees every buffer, reloads FD2SHAP.DAT / FD2.TMP, clears the frame and
 * fades back in.
 *
 * The six little tables below are copied verbatim out of the original image at
 * 0x5269C (t88), 0x526AC (t78), 0x526BC (t98), 0x526C6 (tbc), 0x526CF (tb0) and
 * 0x526D9 (ta4 = "RRSTUVWXYZ"); the machine only ever indexes them with
 * mode < 10.  tbc's byte [9] is not copied by the machine (its 9-byte copy
 * stops at +8) and would read uninitialised stack on the mode==9 path; modelled
 * as 0 (playbook 6, same convention as the uninitialised-register locals).
 *
 * TRAP: Hex-Rays renders `malloc(0x2A300)` as `malloc(&loc_2A300)` (the
 * odd-immediate artifact) and the offset read `t98[mode]` as an out-of-bounds
 * `v42[mode - 4]`; the disassembly (2ffb8..2fff5, 301a3, 30656) is
 * authoritative.  The funcs_30469[a2] call sites use the mode as the table
 * index (`call [eax*4+0x524c6]`, eax = ebp = the mode argument).
 *
 * DEVIATION (playbook 5): this is a single 3876-byte function, above the
 * ~2500-byte large-single-function cap, taken as the approved keystone unblock
 * (its true closure is itself - every callee is already restored).  ABI is
 * cdecl with four stack arguments; the `push 0x164; call 0x3702F` Watcom probe
 * is a chkstk, not a callee and not a register parameter. */
void figure_effect_play(int unit, int mode, int count, const uint8_t *units)
{
    uint8_t  t98[10];   /* 0x526BC, indexed by mode                        */
    uint8_t  ta4[10];   /* 0x526D9 "RRSTUVWXYZ", indexed by mode           */
    uint8_t  tb0[10];   /* 0x526CF, indexed by mode                        */
    uint8_t  tbc[10];   /* 0x526C6 (9 bytes copied, [9] modelled 0)        */
    int      t88[4];    /* 0x5269C, indexed by v72                         */
    int      t78[4];    /* 0x526AC, indexed by v72                         */
    uint8_t  cell[8];   /* map_cell_info output (byte 6 = DAT index)       */
    const uint8_t *rec; /* this unit's 80-byte record                      */
    void    *groups[30];/* per-unit FIGANI group 3*rec[7]                  */
    void    *bg = NULL;    /* BG.DAT strip (v55)                           */
    void    *tai = NULL;   /* TAI.DAT strip (v56)                          */
    void    *fig_a = NULL; /* FIGANI group 3*rec[7]      (v57)             */
    void    *fig_b = NULL; /* FIGANI group 3*rec[7]+2 or +1 (v63)          */
    void    *p = NULL;     /* FDOTHER engine portrait    (p)               */
    uint8_t *screen;    /* 320-stride 0xFA00 buffer   (v69)                */
    uint8_t *work;      /* 640-stride 0x2A300 buffer  (v10)                */
    int      i, j, k, m, n, ii, jj, kk, mm, nn;
    int      v5;        /* TAI default from byte_52470[dword_53C03]        */
    int      v6;        /* figure_bg_pick result                           */
    int      v7;        /* BG default                                      */
    int      v54 = 11;  /* palette strip row base                          */
    int      v58 = 32;  /* panel vertical origin                           */
    int      v60;       /* *fig_b - 1                                      */
    int      v61;       /* frames returned by the current emitter          */
    int      v67 = 0;   /* row toggle for modes 3/7/9                      */
    int      v70;       /* drain posture index                             */
    int      v71 = 8;   /* vertical step into the strip                    */
    int      v72 = 3;   /* t88/t78 row                                     */
    int      v64 = -1;  /* horizontal direction                            */
    int      v73 = 0x4BA0; /* 640-stride stripe offset (19360)             */
    int      v65, v59;  /* record HP word before / after the action        */
    int      v74;       /* unit_action_damage returned 0                   */
    int      r23;       /* emitter selector-5 return                       */
    const void *strip;  /* palette strip pointer (v51)                     */

    memcpy(t88, &unk_5269C, sizeof(t88));
    memcpy(t78, &unk_526AC, sizeof(t78));
    memcpy(t98, &unk_526BC, sizeof(t98));
    memcpy(tbc, &unk_526C6, 9);          /* machine copies 9 bytes */
    memcpy(tb0, &unk_526CF, sizeof(tb0));
    memcpy(ta4, &fd2_data1[0x26D9], sizeof(ta4));
    tbc[9] = 0;

    if (mode >= 32) {
        figure_demo_show(unit, mode, count, units);
        return;
    }
    if (mode == 24 || mode > 27) {
        figure_party_show(unit, mode, count, units);
        return;
    }

    if (mode == 8) {
        v58 = 176;
        v54 = 19;
    } else if (mode > 3) {
        v58 = 176;
        v54 = 15;
    }

    guest_free(dword_53A61);              /* buf / DATO atlas */
    guest_free(dword_53A49);
    guest_free(dword_53A5D);
    dword_53A5D = NULL;
    for (i = 0; i < 30; ++i)
        groups[i] = NULL;

    rec = (const uint8_t *)dword_53A45 + 80 * unit;
    map_cell_info(rec[0], rec[1], cell);
    v5 = byte_52470[dword_53C03];
    if (!rec_skip(unit) || v5 == 0)
        v5 = cell[6];
    v6 = figure_bg_pick(count, units);
    v7 = v6;
    if (rec[6] == 0) {
        v7 = v5;
        v5 = v6;
    }

    bg  = res_load(aBgDat, NULL, v7);
    tai = res_load(aTaiDat, NULL, v5);
    screen = guest_malloc(0xFA00);
    work   = guest_malloc(0x2A300);
    memset(screen, 0, 0xFA00);
    ui_status_panel_at((int)(uintptr_t)screen, unit);
    ui_status_panel_at((int)(uintptr_t)screen, units[0]);
    rle_decode(bg, 0, 50, screen, 320, -1);

    i     = 3 * rec[7];
    fig_a = res_load(aFiganiDat, NULL, i);
    fig_b = res_load(aFiganiDat, NULL, i + 2);
    dword_54153 = 0;
    dword_54153 = (uint32_t)(uintptr_t)res_load(aFdotherDat, NULL, ta4[mode]);
    if (*(const uint16_t *)fig_b == 0)
        fig_b = res_load(aFiganiDat, fig_b, i + 1);
    pal_fade_out();

    for (j = 0; j < count; ++j) {
        const uint8_t *r = (const uint8_t *)dword_53A45 + 80 * units[j];

        groups[j] = res_load(aFiganiDat, groups[j], 3 * r[7]);
    }

    if (rec[6] != 0)
        i = tbc[mode];
    else
        i = tb0[mode];
    p = res_load(aFdotherDat, p, i);

    figure_attack_scroll(unit, 0, fig_a, groups[0], work, screen, tai);
    figure_play(unit, mode, fig_b, groups[0], work, screen, bg, tai);

    if (mode == 9) {
        for (k = 0; k <= 10; ++k) {
            gfx_copy_rows(work + 320, 640, screen, 320, 320, 200);
            if (k != 10)
                res_blit(fig_a, 0, work - 10 * k + 320, 640, -1);
            res_blit(groups[0], 0, work + 320, 640, -1);
            gfx_copy_rows((uint8_t *)0xA0000, 320, work + 320, 640, 320, 200);
        }
        j___delay(500);
    }

    v60 = *(const uint8_t *)fig_b - 1;
    v61 = fx_table[mode](unit, p, work, 320, 0);
    anim_cycle_frame(groups[0], 0, work, 640);
    for (m = 0; m < v61; ++m) {
        uint8_t *dst = work + v73;

        gfx_copy_rows(work + v73, 640, screen, 320, 320, 200);
        fx_table[mode](unit, p, dst, 640, 1);
        if (mode != 9)
            res_blit(fig_b, v60, dst, 640, -1);
        anim_cycle_frame(groups[0], -1, work + v73, 640);
        fx_table[mode](unit, p, work + v73, 640, 2);
        gfx_copy_rows((uint8_t *)0xA0000, 320, work + v73, 640, 320, 200);
        svc_wait_ticks(1);
    }

    for (n = 0; n < count; ++n) {
        uint8_t *r = (uint8_t *)dword_53A45 + 80 * units[n];

        v61 = fx_table[mode](unit, p, work, 320, 3);
        anim_cycle_frame(groups[0], 0, work, 640);
        v65 = *(const int16_t *)(const void *)(r + 0x40);
        v74 = (unit_action_damage(units[n], mode) == 0);
        v59 = *(const int16_t *)(const void *)(r + 0x40);
        *(uint16_t *)(void *)(r + 0x40) = (uint16_t)v65;
        v70 = 1;
        for (ii = 0; ii < v61; ++ii) {
            uint8_t *dst;

            if (mode == 7 || mode == 3 || mode == 9) {
                v67 ^= 1;
                gfx_copy_rows(work + v73 - 640 * v67, 640, screen, 320, 320,
                              200);
            } else {
                gfx_copy_rows(work + v73, 640, screen, 320, 320, 200);
            }

            dst = work + v73;
            fx_table[mode](unit, p, dst, 640, 4);
            if (mode != 9)
                res_blit(fig_b, v60, dst, 640, -1);

            if (v74) {
                anim_cycle_frame(groups[n], -1, work + v73, 640);
                fx_table[mode](unit, p, work + v73, 640, 5);
            } else {
                int v22 = v64 * t88[v72];
                int v53 = t78[v72];
                int off = v58 + (v71 << 8);

                --v71;
                anim_cycle_frame(groups[n], off,
                                 work + v73 + v22 + 640 * v53, 640);
                if (v71 == 1)
                    v71 = 8;
                if (v72 != 3)
                    ++v72;
                r23 = fx_table[mode](unit, p, work + v73, 640, 5);
                if (r23 == 1) {
                    int v24 = t98[mode];

                    if (v24 >= v70) {
                        *(uint16_t *)(void *)(r + 0x40) = (uint16_t)(v65 -
                            v70 * (v65 - v59) / v24);
                        ui_status_panel_at((int)(uintptr_t)screen, units[n]);
                        ++v70;
                    }
                    v72 = 0;
                    v64 = 1 - (int)(util_rand() % 3);
                }
            }
            gfx_copy_rows((uint8_t *)0xA0000, 320, work + v73, 640, 320, 200);
            svc_wait_ticks(1);
        }
        if (count - 1 != n) {
            fx_play(unit, p, fig_b, groups[n], work, screen, groups[n + 1],
                    mode);
            ui_status_panel_at((int)(uintptr_t)screen, units[n + 1]);
        }
    }

    v61 = fx_table[mode](unit, p, work, 320, 6);
    for (jj = 0; jj < v61; ++jj) {
        uint8_t *dst = work + v73;

        gfx_copy_rows(work + v73, 640, screen, 320, 320, 200);
        fx_table[mode](unit, p, dst, 640, 7);
        if (mode != 9)
            res_blit(fig_b, v60, dst, 640, -1);
        anim_cycle_frame(groups[count - 1], -1, work + v73, 640);
        fx_table[mode](unit, p, work + v73, 640, 8);
        gfx_copy_rows((uint8_t *)0xA0000, 320, work + v73, 640, 320, 200);
        svc_wait_ticks(1);
    }

    if (mode == 9) {
        for (kk = 7; kk >= 0; --kk) {
            gfx_copy_rows(work + 320, 640, screen, 320, 320, 200);
            res_blit(fig_a, 0, work - 10 * kk + 320, 640, -1);
            res_blit(groups[0], 0, work + 320, 640, -1);
            gfx_copy_rows((uint8_t *)0xA0000, 320, work + 320, 640, 320, 200);
        }
    }

    for (mm = 1; mm < 4; ++mm) {
        gfx_copy_rows(work, 320, screen, 320, 320, 200);
        strip = dword_53A6D +
            *(const uint32_t *)(dword_53A6D + 4 * (mm + v54) + 6);
        rle_decode_lut(bg, 0, 50, screen, 320, strip);
        rle_decode_lut(tai, 164, 157, screen, 320, strip);
        res_blit(fig_b, 0, work, 320, -1);
        anim_cycle_frame(groups[count - 1], -1, work, 320);
        gfx_copy_rows((uint8_t *)0xA0000, 320, work, 320, 320, 200);
        svc_wait_ticks(1);
    }

    gfx_copy_rows(work, 320, screen, 320, 320, 200);
    rle_decode(bg, 0, 50, screen, 320, -1);
    rle_decode(tai, 164, 157, screen, 320, -1);
    res_blit(fig_b, 0, work, 320, -1);
    anim_cycle_frame(groups[count - 1], -1, work, 320);
    gfx_copy_rows((uint8_t *)0xA0000, 320, work, 320, 320, 200);
    svc_play_sfx((const void *)(uintptr_t)dword_54153, -1, 1);
    guest_free((void *)(uintptr_t)dword_54153);
    guest_free(p);
    for (nn = 0; nn < count; ++nn)
        guest_free(groups[nn]);
    guest_free(screen);
    guest_free(work);
    guest_free(fig_a);
    guest_free(fig_b);
    guest_free(bg);
    guest_free(tai);

    dword_53A49 = guest_malloc(0x25680);
    dword_53A5D = res_load(aFdshapDat, dword_53A5D,
                           2 * *(const uint8_t *)(uintptr_t)dword_53A55);
    figure_tmp_load();
    svc_wait_ticks(8);
    pal_fade_out();
    memset((uint8_t *)0xA0000, 0, 0xFA00);
    map_view_update(1);
    pal_fade_in();
}
