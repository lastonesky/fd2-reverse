/* map_window.c - interactive map/dialog window (original 0x19953, 1188 bytes).
 *
 * The routine the scene layer enters when dword_53A51 > 1 is set as a sentinel
 * (scene_apply_state / ev_script_run call it between 0x197E5 and the portraits).
 * Returns:
 *   1  when the player exits  (scan 0xE0 / 0x52 / 0x1C / 0x39)
 *  -1  on scan 1 / 0x53       (the callers branch on exactly these values)
 * and loops otherwise (0x4B/0x4D move the portrait selection dword_53C57).
 *
 * Structure (all verified against analysis/cseg01_disasm.txt):
 *   1. seed v1 = util_rand() % 30 + 2 (signed modulo of the full eax, as the
 *      machine code's `sar edx,31; idiv 0x1e` does);
 *   2. copy the 64000-byte VGA screen into dword_53C63 and unfold it into the
 *      view buffer dword_53A49 (200 rows of 320 into stride 456 at +31076);
 *   3. when dword_53A51 > 1, render the map view for the current origin;
 *   4. four 4-pixel reveal steps (86 rows of 310 px + two sprite blits at
 *      &(dword_53A49+0x1A59C)[off] with offs in {-4,-8,...} x {4,8,...},
 *      offsets from the 12-byte table at 0x51EED, pitch 456);
 *   5. the interactive loop: wait for a key while stepping the palette
 *      animation (pal_anim_step) every >=2 ticks of the sign-extended BIOS
 *      word at 0x46C, cycling dword_53C13 0..3; redraw the map window row band;
 *      blit the selected portrait (or a random idle frame every v1 frames,
 *      re-seeded to rand()%30+10); blit the two portrait sprites with the
 *      animation phase (dword_53C13/2 added for the selected side); copy the
 *      64000/310-byte bands and gfx_copy_rows the 320x192 area to VGA_P(0xA0504u).
 *
 * The same "&loc_1A599+3" and 12-byte/4-byte table strides as in map_draw_text_
 * window (0x197E5) apply; the shared epilogue at 0x13FCC is a plain return.
 */
#include "fd2_game.h"
#include "map_window.h"

#include <stdint.h>
#include <string.h>

#define VGA_BASE     fd2_vga
#define ROW_STRIDE   456u          /* view buffer pitch */
#define VIEW_SRC_OFF 32904u        /* dword_53A49 + 0x8088 */
#define BUF_OFF_1    31076u        /* dword_53A49 + 456*i + 31076 (full 200 rows) */
#define BUF_OFF_2    82153u        /* dword_53A49 + 456*i + 82153 (86-row band)   */
#define SAVE_OFF_2   35845u        /* dword_53C63 + 320*i + 35845                 */
#define SLIDE_BASE   0x1A59C       /* dword_53A49 + 0x1A59C (immediate trap)      */

/* signed 16-bit BIOS tick at 0x46C (the machine code uses movsx) */
static int32_t tick_word(void)
{
    return (int16_t)((uint16_t)fd2_lowmem[0x46C] |
                     ((uint16_t)fd2_lowmem[0x46D] << 8));
}

/* 0x19953 */
int map_window_run(void)
{
    const uint32_t *sel = (const uint32_t *)&fd2_data1[0x1EED];  /* 2 entry table */
    uint8_t *buf  = (uint8_t *)(uintptr_t)dword_53A49;
    uint8_t *save = (uint8_t *)(uintptr_t)dword_53C63;
    uint8_t *slide = buf + SLIDE_BASE;
    uint8_t *dato = (uint8_t *)(uintptr_t)dword_53A85;
    uint8_t *base89 = (uint8_t *)(uintptr_t)dword_53A89;
    int offs[2];
    int v1, i, j;
    uint8_t idle = 0;

    dword_53C57 = 0;
    v1 = (int)sub_4EBE3() % 30 + 2;
    offs[0] = 0;
    offs[1] = 0;

    memmove(save, (const void *)(uintptr_t)VGA_BASE, 0xFA00u);   /* 64000 */
    for (i = 0; i < 200; ++i)
        memmove(buf + ROW_STRIDE * (uint32_t)i + BUF_OFF_1,
                save + 320u * (uint32_t)i, 0x140u);

    if ((uint32_t)dword_53A51 > 1) {
        sub_1297D();
        sub_11EEE(buf + VIEW_SRC_OFF, 456, 13, 8,
                  (int)qword_53AA9, (int)(qword_53AA9 >> 32));
        sub_127A9();
    }

    for (j = 0; j < 4; ++j) {
        offs[0] -= 4;
        offs[1] += 4;
        for (i = 0; i < 86; ++i)
            memmove(buf + ROW_STRIDE * (uint32_t)i + BUF_OFF_2,
                    save + 320u * (uint32_t)i + SAVE_OFF_2, 0x136u);
        for (i = 0; i < 2; ++i)
            sub_4ED34(&slide[offs[i]],
                      (const void *)(base89 + *(const uint32_t *)
                          (const void *)(base89 + 12u * (uint32_t)sel[i])),
                      456);
        sub_11EB0((char *)VGA_P(0xA0504u), 320, (char *)buf + VIEW_SRC_OFF, 456, 0x138u, 192);
    }

    for (;;) {
        while (sub_10620() == 0) {
            sub_4E31C();                            /* pal_anim_step (0 args) */
            if ((uint32_t)(tick_word() - dword_53C17) >= 2) {
                if (++dword_53C13 == 4)
                    dword_53C13 = 0;
                dword_53C17 = (uint32_t)tick_word();
                if ((uint32_t)dword_53A51 > 1) {
                    sub_1297D();
                    sub_11EEE(buf + VIEW_SRC_OFF, 456, 13, 8,
                              (int)qword_53AA9, (int)(qword_53AA9 >> 32));
                    sub_127A9();
                }
                if (idle != 0) {
                    uint8_t *src = dato + *(const uint8_t *)(const void *)dato;
                    if (dword_53A51 != 0)
                        sub_4EC31(save + dword_53C67, (int16_t *)src, 320);
                    else
                        sub_4EBFF(save + dword_53C67, (int16_t *)src, 320);
                    v1 = (int)sub_4EBE3() % 30 + 10;
                    idle = 0;
                } else if (v1-- == 0) {
                    uint32_t off = *(const uint32_t *)
                        (const void *)(dato + 12u);    /* sub-image 3 */
                    uint8_t *src = dato + off;
                    if (dword_53A51 != 0)
                        sub_4EC31(save + dword_53C67, (int16_t *)src, 320);
                    else
                        sub_4EBFF(save + dword_53C67, (int16_t *)src, 320);
                    idle = 1;
                }
                if ((uint32_t)dword_53A51 <= 1) {
                    for (i = 0; i < 200; ++i)
                        memmove(buf + ROW_STRIDE * (uint32_t)i + BUF_OFF_1,
                                save + 320u * (uint32_t)i, 0x140u);
                } else {
                    for (i = 0; i < 86; ++i)
                        memmove(buf + ROW_STRIDE * (uint32_t)i + BUF_OFF_2,
                                save + 320u * (uint32_t)i + SAVE_OFF_2, 0x136u);
                }
                for (i = 0; i < 2; ++i) {
                    int v13 = 3 * (int)sel[i];
                    if (i == (int)dword_53C57)
                        v13 += (int)dword_53C13 / 2;
                    sub_4ED34(&slide[offs[i]],
                              (const void *)(base89 + *(const uint32_t *)
                                  (const void *)(base89 + 4u * (uint32_t)v13)),
                              456);
                }
                sub_11EB0((char *)VGA_P(0xA0504u), 320, (char *)buf + VIEW_SRC_OFF,
                          456, 0x138u, 192);
            }
        }

        ((uint8_t *)&word_53A8D)[1] = 0x10;         /* AH = 10h: extended key */
        int386(0x16, &word_53A8D, &word_53A8D);
        {
            uint8_t scan = ((uint8_t *)&word_53A8D)[1];

            if (scan == 0xE0 || scan == 0x52 || scan == 0x1C || scan == 0x39) {
                dword_53C13 = 0;
                return 1;
            }
            if (scan == 0x01 || scan == 0x53) {
                dword_53C13 = 0;
                return -1;
            }
            if (scan == 0x4B)
                dword_53C57 = 0;
            else if (scan == 0x4D)
                dword_53C57 = 1;
        }
    }
}
