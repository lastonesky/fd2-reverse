/* screen_fx.c - full-screen transition effects.  See screen_fx.h.
 *
 * Direct translation of the machine code at 0x26CE4.  The sub-image blit goes
 * through the verified gfx_blit_transparent (0x4ED34); the scratch copy is the
 * game heap (guest_malloc/guest_free), matching the Watcom malloc/free pair.
 */
#include "fd2_game.h"
#include "screen_fx.h"

#include <stdint.h>
#include <string.h>

#define SCREEN_CURTAIN_TBL 0x00052408u  /* 4 dwords {-39,-13,13,39} */
#define SCREEN_CURTAIN_SRC 0x00053F66u  /* base of the sub-image pointer table */

/* 0x26CE4 */
void screen_curtain(int mode)
{
    int32_t  tbl[4];
    uint8_t *p;
    uint8_t *v2;
    int      i, j, k;

    memcpy(tbl, (const void *)(uintptr_t)SCREEN_CURTAIN_TBL, sizeof tbl);

    p = (uint8_t *)guest_malloc(0xFA00u);
    memmove(p, (const void *)(uintptr_t)0xA0000u, 0xFA00u);
    for (i = 0; i < 20; ++i)
        memset(p + 320 * i + 54281, 74, 104);

    memmove(dword_53A49, p, 0xFA00u);
    v2 = dword_53A49 + 54320;

    for (j = 0; j < 4; ++j) {
        memmove(dword_53A49, p, 0xFA00u);
        for (k = 0; k < 4; ++k) {
            const void *src = (const void *)(uintptr_t)
                (dword_53F66 + *(const uint32_t *)(uintptr_t)
                    (dword_53F66 + 8 * k + 18));
            int div = (mode == 0) ? (4 - j) : (j + 1);

            gfx_blit_transparent(v2 + tbl[k] / div, src, 320);
        }
        memmove((void *)(uintptr_t)0xA0000u, dword_53A49, 0xFA00u);
    }

    if (mode != 0)
        memmove((void *)(uintptr_t)0xA0000u, p, 0xFA00u);

    guest_free(p);
}

/* Screen-band scroll: a 284-byte-wide band at 0xA8FCA is memmoved from / to a
 * backing copy at 0xA974A (and 0xA99CA for the tail), with a 284x6 fill at
 * 0xAEC4A. The three passes plus delays make the sliding shutter. */

/* 0x27816 */
void screen_band_forward(void)
{
    int i, j, k;

    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 74; ++j)
            memmove((void *)(uintptr_t)(0xA8FCAu + 320 * j),
                    (const void *)(uintptr_t)(0xA974Au + 320 * j), 0x11C);
        for (k = 0; k < 6; ++k)
            memset((void *)(uintptr_t)(0xAEC4Au + 320 * k), 0x49, 0x11C);
        j___delay(10);
    }
    for (j = 0; j < 72; ++j)
        memmove((void *)(uintptr_t)(0xA8FCAu + 320 * j),
                (const void *)(uintptr_t)(0xA99CAu + 320 * j), 0x11C);
    for (k = 0; k < 8; ++k)
        memset((void *)(uintptr_t)(0xAE9CAu + 320 * k), 0x49, 0x11C);
}

/* 0x278E7 */
void screen_band_backward(void)
{
    int i, j, k;

    for (i = 0; i < 3; ++i) {
        for (j = 73; j >= 0; --j)
            memmove((void *)(uintptr_t)(0xA974Au + 320 * j),
                    (const void *)(uintptr_t)(0xA8FCAu + 320 * j), 0x11C);
        for (k = 0; k < 6; ++k)
            memset((void *)(uintptr_t)(0xA8FCAu + 320 * k), 0x49, 0x11C);
        j___delay(10);
    }
    for (j = 71; j >= 0; --j)
        memmove((void *)(uintptr_t)(0xA99CAu + 320 * j),
                (const void *)(uintptr_t)(0xA8FCAu + 320 * j), 0x11C);
    for (k = 0; k < 8; ++k)
        memset((void *)(uintptr_t)(0xA8FCAu + 320 * k), 0x49, 0x11C);
}
