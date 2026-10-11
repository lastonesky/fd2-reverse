/* fb_slide.c - screen-buffer slide compositor helpers.
 *
 * Direct translations of the machine code; see fb_slide.h for the family and
 * the ABI. Each band copies `rows` 320-byte screen rows with memmove, taking
 * the source from `src + src_off + 320*i` and writing to
 * `dst + dst_off + 320*(x_eff) + 320*i`. Negative `x` shifts the copy by
 * `-x` and clips the byte count, exactly as the original (the disassembly is
 * the authority; the constants 0x8C0/0x8C5, 0x5C/0x91C and 0x5/0x7585 are the
 * per-band offsets inside the 0xFA00 buffers).
 *
 * 0x18409 assembles one step: it restores `dst` from `saved`, overlays the
 * three bands at the offsets selected by the step index x (0..11) and finally
 * copies the composed frame to the VGA framebuffer 0xA0000.
 */
#include "fb_slide.h"

#include <string.h>
#include <stdint.h>

/* 0x182AD */
void fb_slide_band_a(int x, char *dst, char *src)
{
    int      v3 = x;
    unsigned v4 = 86;
    int      v5 = 0;
    int      i;

    if (x < 0) {
        v4 = (unsigned)(x + 86);
        v5 = -x;
        v3 = 0;
    }
    for (i = 0; i < 86; ++i) {
        memmove((char *)((uintptr_t)dst + 2240 + v3 + 320 * i),
                (char *)((uintptr_t)src + 2245 + v5 + 320 * i),
                v4);
    }
}

/* 0x18312 */
void fb_slide_band_b(int x, char *dst, char *src)
{
    int v3 = 86;
    int v4 = 0;
    int i;

    if (x < 0) {
        v3 = x + 86;
        v4 = -x;
        x  = 0;
    }
    for (i = 0; i < v3; ++i) {
        memmove((char *)((uintptr_t)dst + 92 + 320 * x + 320 * i),
                (char *)((uintptr_t)src + 2332 + 320 * v4 + 320 * i),
                0xDF);
    }
}

/* 0x1839B */
void fb_slide_band_c(int x, char *dst, char *src)
{
    int v3 = 102;
    int i;

    if (x + 102 >= 200)
        v3 = 200 - x;
    for (i = 0; i < v3; ++i) {
        memmove((char *)((uintptr_t)dst + 5 + 320 * x + 320 * i),
                (char *)((uintptr_t)src + 30085 + 320 * i),
                0x136);
    }
}

/* 0x18409 */
void fb_slide_step(int x, char *dst, char *src, char *saved)
{
    memmove(dst, saved, 0xFA00);

    if (x < 6)
        fb_slide_band_a(5, dst, src);
    else
        fb_slide_band_a(5 - (16 * x - 96), dst, src);

    if (x >= 9 || x <= 2) {
        if (x <= 2)
            fb_slide_band_b(7, dst, src);
    } else {
        fb_slide_band_b(7 - (16 * x - 48), dst, src);
    }

    if (x < 6)
        fb_slide_band_c(16 * x + 94, dst, src);

    memmove((void *)0xA0000, dst, 0xFA00);
}
