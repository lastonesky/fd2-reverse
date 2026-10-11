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
#include <stdio.h>
#include <string.h>


/* screen offsets the three option bands are blitted to (9 rows apart) */
#define TITLE_BAND0_DST (fd2_vga + 0xCD81u)
#define TITLE_BAND1_DST (fd2_vga + 0xD8C1u)
#define TITLE_BAND2_DST (fd2_vga + 0xE401u)

/* 0x2DF01 */
void pal_blend_to(int first, int last, int step, int r, int g, int b)
{
    const uint8_t *pal = dword_53A65;
    int i;

    for (i = first; i < last; ++i) {
        fd2_outp(0x3C8u, i);
        fd2_outp(0x3C9u, r + ((int)pal[3 * i]     - r) * step / 40);
        fd2_outp(0x3C9u, g + ((int)pal[3 * i + 1] - g) * step / 40);
        fd2_outp(0x3C9u, b + ((int)pal[3 * i + 2] - b) * step / 40);
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
    memset((void *)(uintptr_t)VGA_P(0xA0000u), 0, 0xFA00u);
    dword_53A65 = res_load(aFdotherDat, dword_53A65, palette_index);
    img = res_load(aFdotherDat, NULL, image_index);
    rle_decode(img, 0, 0, (void *)(uintptr_t)VGA_P(0xA0000u), 320, -1);
    pal_fade_in();
    svc_wait_ticks(1);
    svc_wait_ticks(6);
    pal_fade_out();
    dword_53A65 = res_load(aFdotherDat, dword_53A65, 101);
    gfx_copy_rows((void *)(uintptr_t)VGA_P(0xA0000u), 320,
                  (const uint8_t *)strip + 320u * (uint32_t)row, 320, 320, 200);
    pal_fade_in();
}

/* 0x1F81E */
void title_ani_play(int index, int delay_ms, int palette_index)
{
    if (palette_index != -1) {
        memset((void *)(uintptr_t)VGA_P(0xA0000u), 0, 0xFA00u);
        dword_53A65 = res_load(aFdotherDat, dword_53A65, palette_index);
    }
    pal_fade_range(0, 255, 0);
    ani_play(index, delay_ms, 0);
    pal_fade_out();   /* shared 0x1F882 tail block */
}

/* 0x1F894 - the opening / continue screen, and the return value is the main
 * menu choice consumed by scenario_menu (0 = new game, 1 = load game,
 * anything else = save game).
 *
 * The FDOTHER.DAT logo is five 147-row sub-images (69..73) RLE-decoded into a
 * 735-row 'strip' scratch buffer and published to VGA one 320x200 window per
 * frame while `j` scrolls from row 535 down to 0 (so the picture slides up).
 * At fixed rows in `marks` a timed ANI.DAT animation is played and the frame
 * counter `frame` restarts; every 12th frame past a mark the palette is
 * reloaded from FDOTHER entry 101.  Any keypress (0x10620) aborts the scroll.
 * The screen then asks "continue?": FD2.SAV is read, de-obfuscated and
 * checksum-tested (the game's rolling cipher + util_sum_tail4 against the
 * trailer dword) to decide between 2 and 3 options, and the up/down/confirm
 * loop (INT 16h, scan in byte_53A8E, ASCII in the low byte of word_53A8D)
 * returns the highlighted option index. */
int title_screen_run(void)
{
    /* rows at which the logo animation steps to its next stage (0x5204E) */
    static const uint32_t marks[15] = {
        520, 430, 410, 340, 310, 300, 240, 180, 150, 130, 110, 87, 64, 22,
        1000
    };
    uint8_t *strip = NULL;   /* 735-row scroll buffer (0x396C0)         */
    uint8_t *logo  = NULL;   /* FDOTHER 74 strip source / 69..73 frames  */
    uint8_t *img   = NULL;   /* FDOTHER 7 option-band image             */
    uint8_t *snd;            /* FDOTHER 77 sound bank                   */
    uint8_t *save  = NULL;   /* FD2.SAV payload                         */
    FILE    *fp;
    int      choice = 0;     /* highlighted option (ebp in the machine)  */
    int      count  = 1;     /* number of options offered                */
    int      done   = 0;     /* set once an option is confirmed          */
    int      frame  = 12;    /* frames since the last `marks` step       */
    uint8_t  mark   = 0;     /* index into `marks`                       */
    int      i, j, k, m;

    snd = (uint8_t *)res_load(aFdotherDat, NULL, 77);          /* sound bank */
    memset((void *)(uintptr_t)VGA_P(0xA0000u), 0, 0xFA00u);
    dword_53A65 = (uint8_t *)res_load(aFdotherDat, dword_53A65, 76);
    pal_fade_range(0, 255, 64);
    logo = (uint8_t *)res_load(aFdotherDat, NULL, 74);
    rle_decode(logo, 0, 0, (void *)(uintptr_t)VGA_P(0xA0000u), 320, -1);
    pal_fade_in();
    svc_wait_ticks(1);
    svc_wait_ticks(30);
    pal_fade_out();
    dword_53A65 = (uint8_t *)res_load(aFdotherDat, dword_53A65, 99);
    memset((void *)(uintptr_t)VGA_P(0xA0000u), 0, 0xFA00u);
    pal_fade_range(0, 255, 0);
    ani_play(3, 90, 1);
    pal_fade_out();
    memset((void *)(uintptr_t)VGA_P(0xA0000u), 0, 0xFA00u);
    dword_53A65 = (uint8_t *)res_load(aFdotherDat, dword_53A65, 101);
    pal_fade_range(0, 255, 64);

    strip = (uint8_t *)guest_malloc(0x396C0u);
    memset(strip, 0, 0x396C0u);
    for (i = 0; i < 5; ++i) {
        logo = (uint8_t *)res_load(aFdotherDat, logo, i + 69);
        rle_decode(logo, 0, 147 * i, strip, 320, -1);
    }
    kbd_flush();
    if (dword_53A45 != NULL)
        guest_free(dword_53A45);
    dword_53A45 = guest_malloc(0xA0u);

    for (j = 535; ; --j) {
        if (j < 0)
            break;
        gfx_copy_rows((void *)(uintptr_t)VGA_P(0xA0000u), 320,
                      strip + 320u * (uint32_t)j, 320, 320, 200);
        if (j == 535)
            pal_fade_in();

        if (j == 10) {
            title_frame_show(75, 76, strip, 10);
        } else if (j == 25) {
            title_ani_play(0, 15, 0);
            goto anim_tail;
        } else if (j == 110) {
            pal_fade_out();
            title_ani_play(8, 90, 99);
            goto anim_tail;
        } else if (j == 210) {
            pal_fade_out();
            title_ani_play(6, 90, 99);
            title_ani_play(7, 50, 0);
            goto anim_tail;
        } else if (j == 330) {
            pal_fade_out();
            title_ani_play(4, 90, 99);
            title_ani_play(5, 50, 0);
            goto anim_tail;
        } else if (j == 450) {
            title_frame_show(100, 99, strip, 450);
        }
        goto mark_check;

    anim_tail:
        gfx_copy_rows((void *)(uintptr_t)VGA_P(0xA0000u), 320,
                      strip + 320u * (uint32_t)j, 320, 320, 200);
        dword_53A65 = (uint8_t *)res_load(aFdotherDat, dword_53A65, 101);
        pal_fade_in();

    mark_check:
        if ((int)marks[mark] == j) {
            frame = 0;
            svc_play_sfx(snd, 0, 1);
            dword_53A65 = (uint8_t *)res_load(aFdotherDat, dword_53A65, 102);
            pal_fade_range(0, 255, 0);
            ++mark;
        }
        if (frame == 11) {
            dword_53A65 = (uint8_t *)res_load(aFdotherDat, dword_53A65, 101);
            pal_fade_range(0, 255, 0);
        }
        ++frame;
        j___delay(30);
        if (j == 0)
            j___delay(1000);
        if (kbd_pending() != 0)
            break;
    }

    for (k = 40; k >= 0; --k) {
        pal_blend_to(0, 255, k, 63, 0, 0);
        j___delay(8);
    }
    j___delay(100);
    kbd_flush();
    guest_free(strip);
    guest_free(logo);

    img = (uint8_t *)res_load(aFdotherDat, img, 7);
    dword_53A65 = (uint8_t *)res_load(aFdotherDat, dword_53A65, 8);
    memset((void *)(uintptr_t)VGA_P(0xA0000u), 0, 0xFA00u);
    pal_fade_range(0, 255, 0);
    ani_play(1, 15, 1);
    svc_play_sfx2(snd, 3, 1);
    pal_fade_add(0, 255, 64);
    res_blit6((void *)(uintptr_t)VGA_P(0xA0000u), 320, img, 0);
    for (m = 0; m <= 40; ++m) {
        pal_blend_to(0, 255, m, 56, 60, 63);
        j___delay(8);
    }
    kbd_flush();

    fp = fopen(aFd2Sav_3, aRb_3);
    if (fp != NULL) {
        save = (uint8_t *)guest_malloc(0x59CBu);
        fread(save, 1, 0x59CBu, fp);
        fclose(fp);
        util_deobfuscate(save, 0x59CBu);
        if ((uint32_t)util_sum_tail4(save, 0x59CBu) == *(uint32_t *)(save + 0x59C7)) {
            count = 2;
            if (save[0x30C5] != 0xFF)
                count = 3;
        }
        guest_free(save);
    }

    title_options_draw(img, 0, count);
    while (done == 0) {
        title_options_draw(img, choice, count);
        byte_53A8E = 0x10;
        int386(0x16, &word_53A8D, &word_53A8D);
        if (byte_53A8E == 0x48) {                 /* up                      */
            svc_play_sfx(snd, 2, 1);
            if (choice != 0)
                --choice;
            else
                choice = count - 1;
        } else if (byte_53A8E == 0x50) {          /* down                    */
            svc_play_sfx(snd, 2, 1);
            if (choice == count - 1)
                choice = 0;
            else
                ++choice;
        } else if ((uint8_t)word_53A8D == 0x0D || (uint8_t)word_53A8D == 0x20
                   || byte_53A8E == 0xE0 || byte_53A8E == 0x52) {
            svc_play_sfx(snd, 1, 1);
            done = 1;
        }
    }

    for (i = 0; i < 4; ++i) {
        title_options_draw(img, -1, count);
        j___delay(80);
        title_options_draw(img, choice, count);
        j___delay(80);
    }
    pal_fade_out();
    memset((void *)(uintptr_t)VGA_P(0xA0000u), 0, 0xFA00u);
    guest_free(img);
    svc_play_sfx(snd, -1, 1);
    guest_free(snd);

    return choice;
}
