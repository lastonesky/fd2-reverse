/* status_screen.c - party status/selection screen.  See status_screen.h.
 *
 * Direct translations of the machine code at the stated addresses.  Uses the
 * already-verified sprite24_plain49(0x4E29C), gfx_blit_transparent(0x4ED34),
 * rle2_blit(0x4EBFF), vm_run(0x15F84), dlg_blit_dato(0x16559),
 * kbd_pending(0x10620) and util_rand(0x4EBE3).  The BIOS tick word is read
 * through the low-memory mirror exactly like anim.c.
 */
#include "fd2_game.h"
#include "status_screen.h"

#include "../dos.h"

#include <stdint.h>
#include <string.h>

#define BDA_W(off) (*(volatile uint16_t *)(uintptr_t)(DOS_LOWMEM_BASE + (off)))
#define HI_53A8D (((uint8_t *)&word_53A8D)[1])   /* AH of the REGS cell */

/* 0x2810B - draw the party name rows.  `highlight` selects the bright row
 * (record index), `base` is the screen offset of the panel. */
void status_names_draw(int highlight, uint8_t *base)
{
    int idx = dword_53F72;
    int count;
    int i;

    if (idx == 3)
        idx = 1;

    count = dword_53BFB;
    if (count > 6) {
        count = 6;
        if (dword_53F4E + 6 > dword_53BFB)
            count = 5;
    }

    for (i = 0; i < count; ++i) {
        const uint8_t *rec = (const uint8_t *)dword_53A45 +
                             80u * (uint32_t)(i + dword_53F4E);
        const uint8_t *bank = (const uint8_t *)(uintptr_t)dword_53A61;
        const void    *sprite =
            bank + *(const int32_t *)(const void *)
                       (bank + 48u * (uint32_t)(i + dword_53F4E) + 4u * (uint32_t)idx);
        int off = 132 * (i % 2);
        int y   = 26 * (i / 2);

        sprite24_plain49(sprite,
                         base + (off + 320 * (y + 117) + 14),
                         320);
        vm_run(dword_53A7D, rec[8] + 1,base + (off + 320 * (y + 121) + 40), 320,
               (i + dword_53F4E == highlight) ? 201 : 205, 76, 0, 0, 0);
    }
}

/* 0x27079 - advance the panel animation and blit the current frame. */
void status_panel_draw(int *layout, int mode)
{
    int half;
    int i;

    if ((dword_53F52 & 1) != 0)
        ++dword_53F72;
    if (dword_53F72 == 4)
        dword_53F72 = 0;

    half = dword_53F52 / 2;

    if (mode != 0) {
        if (mode == 1 || mode == 3) {
            if (mode == 3)
                status_names_draw(dword_53C57, VGA_P(655360u));

            if (dword_53F4E != 0)
                rle2_blit((void *)(uintptr_t)VGA_P(0xA972Au),
                          (const void *)(uintptr_t)
                              (dword_53F66 + *(const uint32_t *)(uintptr_t)
                                  (dword_53F66 + 4 * half + 50)), 320);
            else
                rle2_blit((void *)(uintptr_t)VGA_P(0xA972Au),
                          (const void *)(uintptr_t)
                              (dword_53F66 + *(const uint32_t *)(uintptr_t)
                                  (dword_53F66 + 74)), 320);

            if (dword_53F4E + 6 >= dword_53F5E)
                rle2_blit((void *)(uintptr_t)VGA_P(0xAE36Au),
                          (const void *)(uintptr_t)
                              (dword_53F66 + *(const uint32_t *)(uintptr_t)
                                  (dword_53F66 + 74)), 320);
            else
                rle2_blit((void *)(uintptr_t)VGA_P(0xAE36Au),
                          (const void *)(uintptr_t)
                              (dword_53F66 + *(const uint32_t *)(uintptr_t)
                                  (dword_53F66 + 4 * half + 58)), 320);
        } else if (mode == 2) {
            if (dword_53F4E != 0)
                rle2_blit((void *)(uintptr_t)VGA_P(0xA972Au),
                          (const void *)(uintptr_t)
                              (dword_53F66 + *(const uint32_t *)(uintptr_t)
                                  (dword_53F66 + 4 * half + 50)), 320);
            else
                rle2_blit((void *)(uintptr_t)VGA_P(0xA972Au),
                          (const void *)(uintptr_t)
                              (dword_53F66 + *(const uint32_t *)(uintptr_t)
                                  (dword_53F66 + 74)), 320);

            if (dword_53F4E + 3 >= dword_53F5E)
                rle2_blit((void *)(uintptr_t)VGA_P(0xAE36Au),
                          (const void *)(uintptr_t)
                              (dword_53F66 + *(const uint32_t *)(uintptr_t)
                                  (dword_53F66 + 74)), 320);
            else
                rle2_blit((void *)(uintptr_t)VGA_P(0xAE36Au),
                          (const void *)(uintptr_t)
                              (dword_53F66 + *(const uint32_t *)(uintptr_t)
                                  (dword_53F66 + 4 * half + 58)), 320);

            {
                int v3 = (dword_53F5E > 3) ? 3 : (int)dword_53F5E;
                int v4 = (dword_53F72 == 3) ? 1 : (int)dword_53F72;

                for (i = 0; i < v3; ++i) {
                    const uint8_t *bank = (const uint8_t *)(uintptr_t)dword_53A61;
                    int rec = *(const uint8_t *)(uintptr_t)
                                  (dword_53F62 + (uint32_t)(i + dword_53F4E));
                    const void *sprite =
                        bank + *(const int32_t *)(const void *)
                                   (bank + 48u * (uint32_t)rec + 4u * (uint32_t)v4);

                    sprite24_plain49(sprite,
                                     VGA_P(320 * (26 * i + 117) + 655374),
                                     320);
                }
            }
        }
    } else {
        gfx_blit_transparent(
            (void *)VGA_P(*(const uint32_t *)(const void *)
                              ((uintptr_t)layout + 4u * dword_53C57) + 709680u),
            (const void *)(uintptr_t)
                (dword_53F66 + *(const uint32_t *)(uintptr_t)
                    (dword_53F66 + 8u * dword_53C57 + 4 * half + 18)),
            320);
    }
}

/* 0x26EDA - run the status screen until a key is pressed; returns the BIOS
 * scan code (with the handful of remaps the machine applies). */
int status_screen_run(int mode)
{
    int layout[4];
    int16_t v10;
    int     v2;
    int     v11 = 0;
    int     i;

    memcpy(layout, (const void *)&unk_52418, sizeof layout);
    v10 = (int16_t)BDA_W(0x46C);
    v2  = (int)((int32_t)util_rand() % 50) + 8;
    dword_53F52 = 2;
    status_panel_draw(layout, mode);

    if (mode == 0) {
        for (i = 0; i < 4; ++i) {
            int v4 = 2 * i + 3;

            if (i == (int)dword_53C57)
                v4 = 2 * i + 4;
            gfx_blit_transparent(
                (void *)VGA_P((uint32_t)layout[i] + 709680u),
                (const void *)(uintptr_t)
                    (dword_53F66 + *(const uint32_t *)(uintptr_t)
                        (dword_53F66 + 4u * (uint32_t)v4 + 6)),
                320);
        }
    }

    do {
        int16_t tick = (int16_t)BDA_W(0x46C);

        if ((int)tick - (int)v10 >= 2 || tick < v10) {
            if (++dword_53F52 == 4)
                dword_53F52 = 0;
            status_panel_draw(layout, mode);

            if (v11 != 0) {
                dlg_blit_dato(dword_53A85, dword_53C67, 0);
                v2  = (int)((int32_t)util_rand() % 30) + 2;
                v11 = 0;
            } else {
                int old = v2;

                --v2;
                if (old == 0) {
                    dlg_blit_dato(dword_53A85, dword_53C67, 3);
                    v11 = 1;
                }
            }
            v10 = tick;
        }
        fd2_wait_tick_or_key();
    } while (kbd_pending() == 0);

    HI_53A8D = 16;
    int386(0x16, &word_53A8D, &word_53A8D);

    if (HI_53A8D == 0xE0 || HI_53A8D == 0x52)
        HI_53A8D = 0x1C;
    if (HI_53A8D == 0x53)
        HI_53A8D = 1;

    return HI_53A8D;
}
