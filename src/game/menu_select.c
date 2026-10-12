/* menu_select.c - interactive selection loops of the place/scene menus.
 * See menu_select.h.
 *
 * Direct translations of the machine code at the stated addresses.  Uses the
 * already-verified assets: status_screen_run(0x26EDA), status_names_draw
 * (0x2810B), screen_band_forward/backward(0x27816/0x278E7), msg_blit_band
 * (0x1974C), rle2_blit(0x4EBFF), svc_play_sfx(0x25A96) and the guest heap
 * (guest_malloc).
 *
 * The disassembly is authoritative for the key codes and the signed
 * comparisons: the machine decodes the return of status_screen_run with
 * `cmp eax,imm; jge/jle` (signed) on the 32-bit globals dword_53C57 /
 * dword_53F4E / dword_53BFB, so those tests are written with explicit int32_t
 * casts to keep the original semantics.
 */
#include "fd2_game.h"
#include "menu_select.h"

#include <stdint.h>
#include <string.h>

/* 0x26E38 */
int menu_choice4(void)
{
    int ret = 0;

    do {
        switch (status_screen_run(0)) {
        case 75:                                  /* left  */
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
            if ((int32_t)--dword_53C57 < 0)
                dword_53C57 = 3;
            break;
        case 77:                                  /* right */
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
            if ((int32_t)++dword_53C57 > 3)
                dword_53C57 = 0;
            break;
        case 28:                                  /* Enter */
        case 57:                                  /* Space */
            ret = 1;
            break;
        case 1:                                   /* Esc   */
            ret = -1;
            break;
        default:
            break;
        }
    } while (ret == 0);

    return ret;
}

/* 0x27D33 */
int menu_list_select(void)
{
    int ret = 0;
    int i;

    dword_53C5B = guest_malloc(0xFA00u);
    dword_53C5F = guest_malloc(0xFA00u);
    dword_53C63 = guest_malloc(0xFA00u);

    memmove((void *)(uintptr_t)dword_53C5F, (const void *)(uintptr_t)VGA_P(0xA0000u),
            0xFA00u);
    memmove((void *)(uintptr_t)dword_53C63, (const void *)(uintptr_t)dword_53C5F,
            0xFA00u);

    dword_53F4E = 0;
    dword_53C57 = 0;

    /* panel sub-image at dword_53F66 + [dword_53F66+70], blitted at +35845. */
    rle2_blit((void *)(uintptr_t)(dword_53C63 + 35845u),
              (const void *)(uintptr_t)
                  (dword_53F66 +
                   *(const uint32_t *)(uintptr_t)(dword_53F66 + 70u)),
              320);

    status_names_draw(dword_53C57, dword_53C63);

    for (i = 5; i >= 0; --i)
        msg_blit_band(13 * i + 112, (void *)(uintptr_t)dword_53C5B,
                      (void *)(uintptr_t)dword_53C63);

    do {
        switch (status_screen_run(3)) {
        case 77:                                  /* down */
            if (dword_53BFB - 1 == dword_53C57)
                break;
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
            ++dword_53C57;
            goto scroll_down;
        case 75:                                  /* up */
            if (dword_53C57 == 0)
                break;
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
            --dword_53C57;
            goto scroll_up_tail;
        case 72:                                  /* page up */
            if ((int32_t)dword_53C57 < 2)
                break;
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
            dword_53C57 -= 2;
        scroll_up_tail:
            if ((int32_t)dword_53C57 < (int32_t)dword_53F4E) {
                dword_53F4E -= 2;
                screen_band_backward();
            }
            status_names_draw(dword_53C57, VGA_P(655360u));
            break;
        case 80:                                  /* page down */
            if ((int32_t)(dword_53BFB - 2) <= (int32_t)dword_53C57)
                break;
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
            dword_53C57 += 2;
        scroll_down:
            if ((int32_t)(dword_53C57 - dword_53F4E) >= 6) {
                dword_53F4E += 2;
                screen_band_forward();
            }
            status_names_draw(dword_53C57, VGA_P(655360u));
            break;
        case 28:                                  /* Enter */
        case 57:                                  /* Space */
            ret = 1;
            break;
        case 1:                                   /* Esc   */
            ret = -1;
            break;
        default:
            break;
        }
    } while (ret == 0);

    return ret;
}
