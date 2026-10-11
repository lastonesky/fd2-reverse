/* title_screen.c - opening / continue-screen helpers.  See title_screen.h.
 *
 * Direct translations of the machine code at the stated addresses. The
 * palette-destination global dword_53A65 points at the game's own 3-byte-per
 * -entry palette copy; the resource work goes through the already-restored
 * res_load (0x111BA), rle_decode (0x4E98D), res_blit6 (0x16886) and
 * gfx_copy_rows (0x11EB0); the fades through pal_fade_in/pal_fade_out
 * (0x1F525/0x1F882) and the tick waits through svc_wait_ticks (0x17AA9). The
 * Watcom outp (0x37AE5) is called through its original address, exactly like
 * fade.c does.
 */
#include "fd2_game.h"
#include "title_screen.h"

#include <stdint.h>
#include <string.h>

#define OUTP ((int (*)(unsigned, int))(uintptr_t)0x00037AE5u)

/* screen offsets the three option bands are blitted to (9 rows apart) */
#define TITLE_BAND0_DST 0x000ACD81u
#define TITLE_BAND1_DST 0x000AD8C1u
#define TITLE_BAND2_DST 0x000AE401u

/* 0x2DF01 */
void pal_blend_to(int first, int last, int step, int r, int g, int b)
{
    const uint8_t *pal = dword_53A65;
    int i;

    for (i = first; i < last; ++i) {
        OUTP(0x3C8u, i);
        OUTP(0x3C9u, r + ((int)pal[3 * i]     - r) * step / 40);
        OUTP(0x3C9u, g + ((int)pal[3 * i + 1] - g) * step / 40);
        OUTP(0x3C9u, b + ((int)pal[3 * i + 2] - b) * step / 40);
    }
}

/* 0x1FF79 */
void title_options_draw(void *image, int selected, int count)
{
    res_blit6((void *)(uintptr_t)TITLE_BAND0_DST, 320, image,
              selected == 0 ? 2 : 1);
    if (count > 1)
        res_blit6((void *)(uintptr_t)TITLE_BAND1_DST, 320, image,
                  selected == 1 ? 4 : 3);
    if (count > 2)
        res_blit6((void *)(uintptr_t)TITLE_BAND2_DST, 320, image,
                  selected == 2 ? 6 : 5);
}

/* 0x1F73F */
void title_frame_show(int image_index, int palette_index, const void *strip,
                      int row)
{
    const void *img;

    pal_fade_out();
    memset((void *)(uintptr_t)0xA0000u, 0, 0xFA00u);
    dword_53A65 = res_load(aFdotherDat, dword_53A65, palette_index);
    img = res_load(aFdotherDat, NULL, image_index);
    rle_decode(img, 0, 0, (void *)(uintptr_t)0xA0000u, 320, -1);
    pal_fade_in();
    svc_wait_ticks(1);
    svc_wait_ticks(6);
    pal_fade_out();
    dword_53A65 = res_load(aFdotherDat, dword_53A65, 101);
    gfx_copy_rows((void *)(uintptr_t)0xA0000u, 320,
                  (const uint8_t *)strip + 320u * (uint32_t)row, 320, 320, 200);
    pal_fade_in();
}

/* 0x1F81E */
void title_ani_play(int index, int delay_ms, int palette_index)
{
    if (palette_index != -1) {
        memset((void *)(uintptr_t)0xA0000u, 0, 0xFA00u);
        dword_53A65 = res_load(aFdotherDat, dword_53A65, palette_index);
    }
    pal_fade_range(0, 255, 0);
    ani_play(index, delay_ms, 0);
    pal_fade_out();   /* shared 0x1F882 tail block */
}
