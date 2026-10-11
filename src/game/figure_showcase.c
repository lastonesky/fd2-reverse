/* figure_showcase.c - the intro party-figure showcase (original 0x31C49).
 *
 * Direct translation of the machine code at 0x31C49; the offline disassembly
 * (analysis/cseg01_disasm.txt) is authoritative for the argument order, the
 * constants and the branch conditions.  Every real callee is an
 * already-restored asset:
 *   world_load_party(0x1088D)  pal_fade_range(0x11D40)  gfx_copy_rows(0x11EB0)
 *   vm_run(0x15F84)            dlg_box_stage(0x168B6)    svc_wait_ticks(0x17AA9)
 *   pal_fade_out(0x1F882)      bgm_play(0x25977)         res_load(0x111BA)
 *   figure_attack_scroll(0x2E9A8)  anim_cycle_frame(0x311E5)  res_blit(0x2EB9F)
 *   rle_decode(0x4E98D)        rle2_blit(0x4EBFF)        kbd_pending(0x10620)
 *   kbd_flush(0x4E381)         util_rand(0x4EBE3)        guest_malloc/guest_free
 * The Watcom stack probe (`push size; call 0x3702F`) and the `jmp 0x2E2A8`
 * tail (a shared epilogue = plain return) are not callees.
 */
#include "fd2_game.h"
#include "figure_showcase.h"

#include <stdint.h>
#include <string.h>

#define VGA_FRAME 0xA0000u /* the visible 320x200 frame                      */
#define WORK_SIZE 0x1F400u /* 640-stride figure work buffer                  */
#define SCR_SIZE  0xFA00u  /* 320-stride screen / dialogue buffer            */
#define FADE_SIZE 0x36B00u /* the 320-stride fade-in scratch buffer          */

/* 0x31C49 - fade in, then walk the party table backwards showing each member's
 * FIGANI figure and DATO dialogue box; see figure_showcase.h. */
void figure_showcase_run(void)
{
    uint8_t       *fade_buf;   /* 320-stride fade-in scratch (shown to VGA)   */
    uint8_t       *work;       /* 640-stride figure work buffer               */
    uint8_t       *screen;     /* 320-stride background buffer                */
    uint8_t       *stage;      /* 320-stride dialogue-frame buffer            */
    const void    *portrait;   /* TAI.DAT strip (FDOTHER 3)                   */
    const void    *plate;      /* FDOTHER entry 56 blitted at (0,0)           */
    uint8_t       *fig_next;   /* FIGANI group 3*grp + 1                      */
    uint8_t       *fig_base;   /* FIGANI group 3*grp                          */
    const uint8_t *rec;        /* this member's 80-byte record                */
    uint8_t        fade;       /* fade-in level (40 down to 0 and back)       */
    uint8_t        idx;        /* record index shown for this member          */
    uint8_t        grp;        /* record field +7 (FIGANI group byte)         */
    uint8_t        cursor;     /* dialogue cursor blink countdown             */
    uint8_t        line_sub;   /* vm_run sub for the third name line          */
    int            col;        /* rle2 sub-image (0 or 12)                    */
    int            nframes;    /* dialogue frame budget (220 / 440)           */
    int            i;
    int            j;
    int            k;
    int            m;
    int            n;

    fig_next = NULL;
    fig_base = NULL;
    cursor   = 0;

    world_load_party(30);

    dword_53C67 = 3208;

    fade_buf = guest_malloc(FADE_SIZE);
    memset(fade_buf, 0, FADE_SIZE);
    vm_run((void *)(uintptr_t)dword_53A79, 44,
           (int)(uintptr_t)(fade_buf + 76848), 320, 205, 76, 0, 25, 0);

    fade = 40;
    for (i = 0; i < 500; ++i) {
        pal_fade_range(0, 255, fade);
        gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, fade_buf + 320 * i,
                      320, 0x140, 200);
        if (i < 200 && fade != 0 && i % 5 == 0)
            --fade;
        if (i > 300 && i % 5 == 0)
            ++fade;
        svc_wait_ticks(1);
    }
    guest_free(fade_buf);

    work     = guest_malloc(WORK_SIZE);
    screen   = guest_malloc(SCR_SIZE);
    stage    = guest_malloc(SCR_SIZE);
    portrait = res_load(aTaiDat, NULL, 3);
    plate    = res_load(aFdotherDat, NULL, 56);
    rle_decode(plate, 0, 0, screen, 320, -1);
    guest_free((void *)plate);
    bgm_play(4, 0);

    for (j = (int)dword_53BFB - 1; j >= 0; --j) {
        if (j != 0) {
            if (j == 1)
                idx = 0;
            else
                idx = (uint8_t)j;
        } else {
            idx = 1;
        }
        rec = (const uint8_t *)dword_53A45 + 80 * idx;
        grp = rec[7];
        fig_next = res_load(aFiganiDat, fig_next, 3 * grp + 1);
        fig_base = res_load(aFiganiDat, fig_base, 3 * grp);

        figure_attack_scroll(j, 1, fig_base, NULL, work, screen, portrait);
        anim_cycle_frame(fig_base, 0, work, 320);
        for (k = 0; k < 20; ++k) {
            gfx_copy_rows(work, 320, screen, 320, 0x140, 200);
            anim_cycle_frame(fig_base, -1, work, 320);
            gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, work, 320, 0x140,
                          200);
            svc_wait_ticks(1);
        }
        for (m = 0; m < fig_next[2]; ++m) {
            const uint8_t *sub =
                fig_next + *(const uint32_t *)(fig_next + 4 * m + 8);
            gfx_copy_rows(work, 320, screen, 320, 0x140, 200);
            res_blit(fig_next, m, work, 320, -1);
            gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, work, 320, 0x140,
                          200);
            svc_wait_ticks(sub[6]);
        }

        memmove(stage, screen, SCR_SIZE);
        dword_53A85 = res_load(aDatoDat, dword_53A85, grp);
        dlg_box_stage(stage, 320, 5, 7, 5, 5);

        nframes = (j != 0) ? 220 : 440;
        for (n = 0; n < nframes; ++n) {
            gfx_copy_rows(work, 320, stage, 320, 0x140, 200);
            if (cursor != 0)
                --cursor;
            else
                cursor = (uint8_t)((util_rand() & 0x1F) + 40);
            if (cursor >= 2)
                col = 0;
            else
                col = 12;
            rle2_blit(
                work + dword_53C67,
                (const void *)((uintptr_t)dword_53A85 +
                               *(const uint32_t *)((uintptr_t)dword_53A85 +
                                                   col)),
                320);
            anim_cycle_frame(fig_base, -1, work, 320);
            vm_run((void *)(uintptr_t)dword_53A79, 10,
                   (int)(uintptr_t)(work + 5865), 320, 205, 76, 0, 0, 0);
            vm_run(dword_53A7D, rec[8] + 1, (int)(uintptr_t)(work + 5915), 320,
                   205, 76, 0, 0, 0);
            vm_run((void *)(uintptr_t)dword_53A79, 11,
                   (int)(uintptr_t)(work + 12265), 320, 205, 76, 0, 0, 0);
            vm_run(dword_53A7D, rec[32] + 150, (int)(uintptr_t)(work + 12315),
                   320, 205, 76, 0, 0, 0);
            if (n >= 220)
                line_sub = 45;
            else
                line_sub = (uint8_t)(rec[8] + 12);
            vm_run((void *)(uintptr_t)dword_53A79, line_sub,
                   (int)(uintptr_t)(work + 32008), 320, 205, 76, 0, 20, 0);
            gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, work, 320, 0x140,
                          200);
            svc_wait_ticks(1);
            if (kbd_pending() != 0) {
                j = 1;
                kbd_flush();
            }
        }
        pal_fade_out();
    }

    guest_free(screen);
    guest_free(stage);
    guest_free(fig_next);
    guest_free((void *)portrait);
    guest_free(work);
}
