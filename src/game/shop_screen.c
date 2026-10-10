/* shop_screen.c - the interactive list screens of the shop/place menu.
 * See shop_screen.h for the family, the ABI and the shared scratch buffers.
 *
 * Direct translations of the machine code at 0x275E6 / 0x27738 / 0x2A29D /
 * 0x2A857; the offline disassembly (analysis/cseg01_disasm.txt) is
 * authoritative.
 *
 *   - the Watcom stack probe (`push size; call 0x3702F`) is dropped; it does
 *     not allocate a frame (0x3702F restores ESP itself), so the arguments sit
 *     where the raw `[esp+N]` reads say;
 *   - 0x2A29D / 0x2A857 end with `jmp 0x26A73`, the shared epilogue
 *     (`mov eax,esi; pop ebp; pop edi; pop esi; pop ebx; ret`) matching their
 *     own four-register frame, i.e. a plain `return` of EAX;
 *   - the visible globals dword_53C57 (cursor) and dword_53F4E (scroll window)
 *     are 32-bit, so the signed `jl/jle/jge` tests of the machine are written
 *     with explicit int32_t casts;
 *   - the three 0xFA00 buffers are taken from the guest heap (guest_malloc),
 *     never the host heap, because pending machine-code callers share them.
 */
#include "fd2_game.h"
#include "shop_screen.h"
#include "shop_rows.h"

#include <stdint.h>
#include <string.h>

/* 0x27738 */
void shop_items_paint(int count, const uint8_t *items, uint8_t half_price)
{
    int i;

    dword_53C5B = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);
    dword_53C5F = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);
    dword_53C63 = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);

    memmove((void *)(uintptr_t)dword_53C5F, (const void *)(uintptr_t)0xA0000u,
            0xFA00u);
    memmove((void *)(uintptr_t)dword_53C63, (const void *)(uintptr_t)dword_53C5F,
            0xFA00u);

    /* panel sub-image at dword_53F66 + [dword_53F66+70], blitted at +35845. */
    rle2_blit((void *)(uintptr_t)(dword_53C63 + 35845u),
              (const void *)(uintptr_t)
                  (dword_53F66 +
                   *(const uint32_t *)(uintptr_t)(dword_53F66 + 70u)),
              320);

    shop_item_rows(count, items, (int)dword_53C57, (int)dword_53C63,
                   half_price);

    for (i = 5; i >= 0; --i)
        msg_blit_band(13 * i + 112, (void *)(uintptr_t)dword_53C5B,
                      (void *)(uintptr_t)dword_53C63);
}

/* 0x275E6 */
int shop_items_run(int count, const uint8_t *items, uint8_t half_price)
{
    int ret = 0;

    do {
        switch (status_screen_run(1)) {
        case 77:                                  /* right */
            if ((int32_t)count - 1 == (int32_t)dword_53C57)
                break;
            svc_play_sfx(dword_53EEC, 0, 1);
            ++dword_53C57;
            goto scroll_down;
        case 75:                                  /* left  */
            if (dword_53C57 == 0)
                break;
            svc_play_sfx(dword_53EEC, 0, 1);
            --dword_53C57;
            goto scroll_up_tail;
        case 72:                                  /* up    */
            if ((int32_t)dword_53C57 < 2)
                break;
            svc_play_sfx(dword_53EEC, 0, 1);
            dword_53C57 -= 2;
        scroll_up_tail:
            if ((int32_t)dword_53C57 < (int32_t)dword_53F4E) {
                dword_53F4E -= 2;
                screen_band_backward();
            }
            goto repaint;
        case 80:                                  /* down  */
            if ((int32_t)(count - 2) <= (int32_t)dword_53C57)
                break;
            svc_play_sfx(dword_53EEC, 0, 1);
            dword_53C57 += 2;
        scroll_down:
            if ((int32_t)(dword_53C57 - dword_53F4E) >= 6) {
                dword_53F4E += 2;
                screen_band_forward();
            }
        repaint:
            shop_item_rows(count, items, (int)dword_53C57, 0xA0000u,
                           half_price);
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

/* 0x2A29D */
int shop_party_run(int count, const uint8_t *members)
{
    int ret = 0;
    int i;

    dword_53C5B = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);
    dword_53C5F = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);
    dword_53C63 = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);

    memmove((void *)(uintptr_t)dword_53C5F, (const void *)(uintptr_t)0xA0000u,
            0xFA00u);
    memmove((void *)(uintptr_t)dword_53C63, (const void *)(uintptr_t)dword_53C5F,
            0xFA00u);

    dword_53F4E = 0;
    dword_53C57 = 0;
    dword_53F62 = (uint32_t)(uintptr_t)members;
    dword_53F5E = (uint32_t)count;

    /* panel sub-image at dword_53F66 + [dword_53F66+70], blitted at +35845. */
    rle2_blit((void *)(uintptr_t)(dword_53C63 + 35845u),
              (const void *)(uintptr_t)
                  (dword_53F66 +
                   *(const uint32_t *)(uintptr_t)(dword_53F66 + 70u)),
              320);

    shop_party_bar_rows(count, (int)dword_53C63, (int)dword_53C57, members);

    for (i = 5; i >= 0; --i)
        msg_blit_band(13 * i + 112, (void *)(uintptr_t)dword_53C5B,
                      (void *)(uintptr_t)dword_53C63);

    do {
        switch (status_screen_run(2)) {
        case 72:                                  /* up   */
            if (dword_53C57 == 0)
                break;
            --dword_53C57;
            if ((int32_t)dword_53C57 < (int32_t)dword_53F4E) {
                --dword_53F4E;
                screen_band_backward();
            }
        repaint:
            shop_party_bar_rows(count, 0xA0000u, (int)dword_53C57, members);
            break;
        case 80:                                  /* down */
            if ((int32_t)(count - 1) <= (int32_t)dword_53C57)
                break;
            ++dword_53C57;
            if ((int32_t)(dword_53C57 - dword_53F4E) >= 3) {
                ++dword_53F4E;
                screen_band_forward();
            }
            goto repaint;
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

/* 0x2A857 */
int shop_party_equip_run(int count, const uint8_t *members,
                         const uint8_t *equip)
{
    int ret = 0;
    int i;

    dword_53C5B = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);
    dword_53C5F = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);
    dword_53C63 = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);

    memmove((void *)(uintptr_t)dword_53C5F, (const void *)(uintptr_t)0xA0000u,
            0xFA00u);
    memmove((void *)(uintptr_t)dword_53C63, (const void *)(uintptr_t)dword_53C5F,
            0xFA00u);

    dword_53F4E = 0;
    dword_53C57 = 0;
    dword_53F62 = (uint32_t)(uintptr_t)members;
    dword_53F5E = (uint32_t)count;

    /* panel sub-image at dword_53F66 + [dword_53F66+70], blitted at +35845. */
    rle2_blit((void *)(uintptr_t)(dword_53C63 + 35845u),
              (const void *)(uintptr_t)
                  (dword_53F66 +
                   *(const uint32_t *)(uintptr_t)(dword_53F66 + 70u)),
              320);

    shop_party_equip_rows(count, (int)dword_53C63, (int)dword_53C57, members,
                          equip);

    for (i = 5; i >= 0; --i)
        msg_blit_band(13 * i + 112, (void *)(uintptr_t)dword_53C5B,
                      (void *)(uintptr_t)dword_53C63);

    do {
        switch (status_screen_run(2)) {
        case 72:                                  /* up   */
            if (dword_53C57 == 0)
                break;
            --dword_53C57;
            if ((int32_t)dword_53C57 < (int32_t)dword_53F4E) {
                --dword_53F4E;
                screen_band_backward();
            }
        repaint:
            shop_party_equip_rows(count, 0xA0000u, (int)dword_53C57, members,
                                  equip);
            break;
        case 80:                                  /* down */
            if ((int32_t)(count - 1) <= (int32_t)dword_53C57)
                break;
            ++dword_53C57;
            if ((int32_t)(dword_53C57 - dword_53F4E) >= 3) {
                ++dword_53F4E;
                screen_band_forward();
            }
            goto repaint;
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
