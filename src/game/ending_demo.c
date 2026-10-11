/* ending_demo.c - the closing end-of-demo presentation driver (original 0x31529).
 *
 * Direct translation of the machine code at 0x31529..0x31bde; the offline
 * disassembly (analysis/cseg01_disasm.txt) is authoritative for the argument
 * order, the constants and the branch conditions.  Every real callee is an
 * already-restored asset:
 *   res_load(0x111BA)      res_blit(0x2EB9F)        gfx_copy_rows(0x11EB0)
 *   pal_fade_out(0x1F882)  pal_fade_in(0x1F525)     pal_fade_range(0x11D40)
 *   pal_fade_add(0x11DF2)  ani_play(0x20421)        rle_decode(0x4E98D)
 *   msg_show_lines(0x31BDF) figure_showcase_run(0x31C49)
 *   bgm_play(0x25977)      svc_wait_ticks(0x17AA9)  figure_attack_view(0x2E2B0)
 *   j___delay (CRT 0x3790A) + guest_malloc/guest_free (CRT 0x3706E/0x3776E)
 * The Watcom stack probe (`push 0x80; call 0x3702F`) is not a callee.
 *
 * Decompiler artifacts resolved against the disassembly:
 *   - `malloc(size: &loc_1F400)` / `memset(..., 0, 64000)` are the immediate
 *     sizes 0x1F400 (the 640-stride work buffer) and 0xFA00 (one 320x200 page).
 *   - The `sub_3702F(int)` prototype is the stack probe; the function has no
 *     stack arguments and returns void.
 *   - The three 20-byte tables copied from 0x526E9 / 0x526FD / 0x52711 live in
 *     dseg02; the first is held in a 24-byte local because the 200-frame loop
 *     writes a (dead) dword into its 6th slot (`v16[5] = ii % 4`).
 *   - The `dword_53A65` palette pointer is temporarily aimed at FDOTHER entry
 *     57 around each figure_attack_view call and restored afterwards.
 *   - Buffers for FDOTHER entries 60/58 and the scratch screen are never freed
 *     by the machine (only the work buffer, entry 57 and entry 59 are). */
#include "fd2_game.h"
#include "ending_demo.h"

#include <stdint.h>
#include <string.h>

#define VGA_FRAME  0xA0000u /* the visible 320x200 frame               */
#define WORK_SIZE  0x1F400u /* 640-stride sprite work buffer           */
#define SCR_SIZE   0xFA00u  /* one 320-stride 320x200 page             */

/* 0x526E9 / 0x526FD / 0x52711 (dseg02): record 0 portrait byte, record 1
 * portrait byte and the dword_54133 demo flag, one entry per displayed pair. */
static const uint8_t kPair0Id[20] = {
    0x33, 0x6E, 0x13, 0x69, 0x36, 0x75, 0x1E, 0x7B, 0x27, 0x7F,
    0x40, 0x51, 0x34, 0x7D, 0x1A, 0x73, 0x29, 0x5B, 0x1F, 0x7E
};
static const uint8_t kPair1Id[20] = {
    0x67, 0x14, 0x53, 0x1C, 0x7C, 0x26, 0x5D, 0x22, 0x70, 0x2C,
    0x56, 0x35, 0x50, 0x37, 0x78, 0x24, 0x6A, 0x3C, 0x7A, 0x32
};
static const uint8_t kDemoFlag[20] = {
    0x04, 0x03, 0x33, 0x0E, 0x19, 0x12, 0x28, 0x35, 0x16, 0x18,
    0x1C, 0x11, 0x1E, 0x1F, 0x32, 0x21, 0x22, 0x34, 0x24, 0x2F
};

/* 0x31529 - the closing presentation; see ending_demo.h. */
void ending_demo_run(void)
{
    uint8_t *work;             /* 640-stride 0x1F400 sprite work buffer    */
    uint8_t *screen;           /* 320-stride 0xFA00 scratch (never freed)  */
    void    *blit;             /* FDOTHER.DAT entry 54 (the plate frames)  */
    void    *pair1_buf;        /* FDOTHER.DAT entry 57                     */
    void    *pair0_buf;        /* FDOTHER.DAT entry 58                     */
    void    *plate;            /* FDOTHER.DAT entry 60 / 59                */
    uint32_t pair0_id[6];      /* kPair0Id + a dead 6th dword slot         */
    uint8_t  pair1_id[20];     /* kPair1Id                                 */
    uint8_t  demo_flag[20];    /* kDemoFlag -> dword_54133                 */
    uint8_t *saved_pal;        /* the machine's v10 (original dword_53A65) */
    int      v20 = 80;         /* work-buffer offset stepping up by 2      */
    int      v21 = 290;        /* work-buffer offset stepping down by 2/4  */
    int      v7;               /* pal_fade_range level in the 200 loop     */
    int      i;
    int      j;
    int      k;
    int      m;
    int      n;
    int      ii;
    int      jj;

    pair1_buf = NULL;
    pair0_buf = NULL;

    memcpy(pair0_id, kPair0Id, sizeof(kPair0Id));
    memcpy(pair1_id, kPair1Id, sizeof(kPair1Id));
    memcpy(demo_flag, kDemoFlag, sizeof(kDemoFlag));

    work   = guest_malloc(WORK_SIZE);
    screen = guest_malloc(SCR_SIZE);
    memset(screen, 0, 64000);

    blit = res_load(aFdotherDat, NULL, 54);
    res_blit(blit, 0, screen, 320, -1);
    pal_fade_out();
    memmove((void *)(uintptr_t)VGA_FRAME, screen, SCR_SIZE);
    pal_fade_in();
    j___delay(1000);
    ani_play(2, 100, 0);
    pal_fade_add(0, 255, 63);
    memmove((void *)(uintptr_t)VGA_FRAME, screen, SCR_SIZE);
    res_blit(blit, 9, (void *)(uintptr_t)VGA_FRAME, 320, -1);
    for (i = 63; i >= 0; --i) {
        pal_fade_add(0, 255, i);
        j___delay(4);
    }
    j___delay(2000);

    if (dword_53C03 == 26) {
        msg_show_lines(4, 17);
    } else {
        msg_show_lines(37, 2);
        msg_show_lines(21, 3);
        msg_show_lines(26, 4);
        msg_show_lines(105, 5);
        msg_show_lines(32, 6);
    }
    j___delay(500);

    for (j = 0; j < 3; ++j) {
        for (k = 63; k >= 0; --k) {
            pal_fade_add(0, 255, k);
            j___delay(4);
        }
        j___delay(200);
    }

    for (m = 12; m < 109; ++m) {
        res_blit(blit, m, (void *)(uintptr_t)VGA_FRAME, 320, -1);
        j___delay(20);
    }
    memmove((void *)(uintptr_t)VGA_FRAME, screen, SCR_SIZE);

    if (dword_53C03 == 26) {
        msg_show_lines(21, 18);
        msg_show_lines(24, 19);
        msg_show_lines(26, 20);
    } else {
        msg_show_lines(45, 7);
    }
    j___delay(2000);

    for (n = 0; n < 40; ++n) {
        gfx_copy_rows(work + 160, 640, screen, 320, 0x140, 200);
        res_blit(blit, n % 4 + 1, work + v21, 640, -1);
        res_blit(blit, n % 4 + 5, work + v20, 640, -1);
        v20 += 2;
        v21 -= 2;
        if (n < 25)
            v21 -= 2;
        j___delay(20);
        gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, work + 160, 640, 0x140,
                      200);
    }
    memmove(work, screen, SCR_SIZE);
    res_blit(blit, 1, work, 320, -1);
    res_blit(blit, 5, work, 320, -1);
    gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, work, 320, 0x140, 200);

    if (dword_53C03 == 26) {
        msg_show_lines(32, 21);
        msg_show_lines(36, 22);
        msg_show_lines(32, 23);
    } else {
        msg_show_lines(32, 8);
        msg_show_lines(36, 9);
    }

    v7 = 0;
    for (ii = 0; ii < 200; ++ii) {
        memmove(work, screen, SCR_SIZE);
        pair0_id[5] = (uint32_t)(ii % 4);       /* dead store (see header) */
        res_blit(blit, ii % 4 + 1, work, 320, -1);
        res_blit(blit, ii % 4 + 5, work, 320, -1);
        j___delay(20);
        gfx_copy_rows((void *)(uintptr_t)VGA_FRAME, 320, work, 320, 0x140, 200);
        if (ii > 135)
            ++v7;
        pal_fade_range(0, 255, v7);
    }

    guest_free(work);
    memset((void *)(uintptr_t)VGA_FRAME, 0, 64000);
    figure_showcase_run();
    memset((void *)(uintptr_t)VGA_FRAME, 0, 64000);
    bgm_play(-1, 1);
    svc_wait_ticks(50);

    plate = res_load(aFdotherDat, pair0_buf, 60);
    rle_decode(plate, 0, 0, (void *)(uintptr_t)VGA_FRAME, 320, -1);
    pal_fade_in();
    bgm_play(18, 0);
    svc_wait_ticks(80);
    pal_fade_out();
    memset((void *)(uintptr_t)VGA_FRAME, 0, 64000);

    pair0_buf = res_load(aFdotherDat, plate, 58);
    pair1_buf = res_load(aFdotherDat, pair1_buf, 57);

    saved_pal = dword_53A65;
    for (jj = 0; jj < 20; ++jj) {
        dword_53A65 = saved_pal;
        if (((const uint8_t *)pair0_id)[jj] >= 0x4C)
            *((uint8_t *)dword_53A45 + 6) = 0;
        else
            *((uint8_t *)dword_53A45 + 6) = 2;
        *((uint8_t *)dword_53A45 + 7) = ((const uint8_t *)pair0_id)[jj];
        if (pair1_id[jj] >= 0x4C)
            *((uint8_t *)dword_53A45 + 86) = 0;
        else
            *((uint8_t *)dword_53A45 + 86) = 2;
        *((uint8_t *)dword_53A45 + 87) = pair1_id[jj];
        dword_54133 = demo_flag[jj];

        figure_attack_view(0, 1);
        dword_53A65 = (uint8_t *)pair1_buf;

        pal_fade_range(0, 255, 0);
        svc_wait_ticks(20);
        res_blit(pair0_buf, jj, (void *)(uintptr_t)VGA_FRAME, 320, -1);
        svc_wait_ticks(78);
        pal_fade_out();
        memset((void *)(uintptr_t)VGA_FRAME, 0, 64000);
    }
    dword_53A65 = saved_pal;

    guest_free(pair1_buf);
    svc_wait_ticks(50);
    plate = res_load(aFdotherDat, pair0_buf, 59);
    rle_decode(plate, 0, 0, (void *)(uintptr_t)VGA_FRAME, 320, -1);
    pal_fade_in();
    guest_free(plate);
}
