/* place_menu.c - location/place-menu helpers (original 0x2921A, 0x28B41,
 * 0x26A0D, 0x26A7A, 0x26B91).  Direct translation of the machine code; see
 * place_menu.h for the module overview and ABI notes.
 */
#include "fd2_game.h"
#include "place_menu.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* 0x26A0D */
int place_name_copy(char *dst)
{
    const uint8_t *table = (const uint8_t *)(uintptr_t)dword_53F56;
    int            len;
    int            off;
    int            n = 0;
    int            i;

    if (dword_53F4A == 1) {
        len = 12;
        off = 3;
    } else if (dword_53F4A == 3) {
        len = 8;
        off = 15;
    } else {
        len = 8;
        off = 23;
    }
    for (i = 0; i < len; ++i) {
        uint8_t c = table[off + i];
        if (c == 0xFF)
            break;
        dst[n++] = (char)c;
    }
    return n;
}

/* 0x2921A.
 *
 * dest(i,j) = src[(x0 >> 7) + 320 * (y0 >> 7)] where x0 = src_x - 160*zoom + j*zoom
 * and y0 = src_y - 100*zoom + i*zoom, taken only when x0 in [0,0xA000) and y0 in
 * [0,0x6400).  The machine does truncating signed /128, which equals >>7 on the
 * non-negative values the guards leave (see the disassembly at 0x292e2/0x29290).
 */
void place_zoom_blit(int src_x, int src_y, const char *src, int zoom)
{
    uint8_t *dst = dword_53A49;
    int32_t  y0  = src_y - 100 * zoom;
    int      i;

    memset(dword_53A49, 0, 64000);
    for (i = 0; i < 200; ++i) {
        if ((uint32_t)y0 < 0x6400u) {
            int32_t x0 = src_x - 160 * zoom;
            int     j;
            for (j = 0; j < 320; ++j) {
                if ((uint32_t)x0 < 0xA000u)
                    dst[j] = (uint8_t)src[(x0 >> 7) + 320 * (y0 >> 7)];
                x0 += zoom;
            }
        }
        y0 += zoom;
        dst += 320;
    }
}

/* 0x28B41 */
void place_transition(void)
{
    int i;

    switch (dword_53F4A) {
    case 1:
        for (i = 0; i < 5; ++i) {
            res_blit6((void *)(uintptr_t)0xA38E9u, 320,
                      (void *)(uintptr_t)dword_53F66, i + 0x17);
            svc_wait_ticks(2);
        }
        dlg_blit_dato(dword_53A85, dword_53C67, 0);
        break;
    case 3:
        svc_wait_ticks(1);
        res_blit6((void *)(uintptr_t)0xA3154u, 320,
                  (void *)(uintptr_t)dword_53F66, 0x17);
        svc_wait_ticks(8);
        dlg_blit_dato(dword_53A85, dword_53C67, 0);
        break;
    case 4:
        dlg_blit_dato(dword_53A85, dword_53C67, 3);
        svc_wait_ticks(2);
        for (i = 0; i < 9; ++i) {
            res_blit6((void *)(uintptr_t)0xA2893u, 320,
                      (void *)(uintptr_t)dword_53F66, i + 0x17);
            svc_wait_ticks(2);
        }
        for (i = 0; i < 64; i += 2) {
            pal_fade_add(0, 255, i);
            j___delay(4);
        }
        svc_wait_ticks(10);
        for (i = 62; i >= 0; i -= 2) {
            pal_fade_add(0, 255, i);
            j___delay(4);
        }
        svc_wait_ticks(5);
        dlg_blit_dato(dword_53A85, dword_53C67, 0);
        break;
    case 5:
        for (i = 0; i < 7; ++i) {
            res_blit6((void *)(uintptr_t)0xA2383u, 320,
                      (void *)(uintptr_t)dword_53F66, i + 0x17);
            svc_wait_ticks(2);
        }
        break;
    default:
        break;
    }
    kbd_flush();
}

/* 0x26A7A - count `cur` (the displayed digits) up to `tgt` (the digits of the
 * new total), drawing glyph `roll + 9*cur` for each still-rolling column. */
void place_money_gain(int amount)
{
    uint8_t cur[9];
    uint8_t tgt[9];
    uint8_t roll[8];
    int     i, k, m, n, done;

    sprintf((char *)cur, a08d_1, (int)dword_53BF3);
    for (i = 0; i < 8; ++i)
        cur[i] -= '0';
    dword_53BF3 += (uint32_t)amount;
    sprintf((char *)tgt, a08d_2, (int)dword_53BF3);
    for (i = 0; i < 8; ++i)
        tgt[i] -= '0';

    for (;;) {
        done = 1;
        for (k = 0; k < 8; ++k) {
            if (cur[k] == tgt[k]) {
                roll[k] = 0;
            } else {
                roll[k] = 1;
                done    = 0;
            }
        }
        if (!done) {
            for (m = 0; m < 9; ++m) {
                for (n = 0; n < 8; ++n) {
                    if (roll[n] != 0) {
                        ev5_26C9B((void *)(uintptr_t)(6 * n + 0xA7A90u), 320,
                                  roll[n] + 9 * cur[n]);
                        if (++roll[n] == 10 && ++cur[n] == 10)
                            cur[n] = 0;
                    }
                }
                j___delay(10);
            }
        }
        if (done)
            break;
    }
}

/* 0x26B91 - the spend direction: `cur` counts down to `tgt`, glyph index
 * `9*cur + roll - 1` with `roll` starting at 9 and decrementing. */
void place_money_spend(int amount)
{
    uint8_t cur[9];
    uint8_t tgt[9];
    uint8_t roll[8];
    int     i, k, m, n, done;

    sprintf((char *)cur, a08d, (int)dword_53BF3);
    for (i = 0; i < 8; ++i)
        cur[i] -= '0';
    dword_53BF3 -= (uint32_t)amount;
    sprintf((char *)tgt, a08d_0, (int)dword_53BF3);
    for (i = 0; i < 8; ++i)
        tgt[i] -= '0';

    for (;;) {
        done = 1;
        for (k = 0; k < 8; ++k) {
            if (cur[k] == tgt[k]) {
                roll[k] = 0;
            } else {
                roll[k] = 9;
                done    = 0;
                if (--cur[k] == 0xFFu)
                    cur[k] = 9;
            }
        }
        if (!done) {
            for (m = 0; m < 9; ++m) {
                for (n = 0; n < 8; ++n) {
                    if (roll[n] != 0) {
                        ev5_26C9B((void *)(uintptr_t)(6 * n + 0xA7A90u), 320,
                                  9 * cur[n] + roll[n] - 1);
                        --roll[n];
                    }
                }
                j___delay(10);
            }
        }
        if (done)
            break;
    }
}

/* 0x2670E - enter the place selected in the place list (dword_53F4A).
 * `place` is the current location's table byte (sub_4E809 -> &fd2_data2[0x238D],
 * stride 31, offset -31).  Place 2 plays its scripted intro and, if the party
 * screen confirms, continues as the other places do: snapshot the VGA frame to
 * the guest heap, play the ten-step place_zoom_blit zoom-out (frame + 0xA0000
 * swapped every step with a pal_fade_range wash), then dispatch to the screen
 * for the place type - 0 -> place_screen_run, 4 -> place_screen4_run, anything
 * else 3/5 -> place_menu_run.  Place 2 returns the confirmed flag without a
 * closing bgm_play(10).  The machine's EAX is the screen result (ebp); all
 * callees are in the manifest and 0x29867 is the shared epilogue of the frame
 * (add esp,4; pop ebp/edi/esi/ebx; ret) so it is a plain return. */
int place_menu_commit(void)
{
    const uint8_t *ent;
    uint8_t        place;
    void          *frame;
    int            result = 0;
    int            i;

    ent   = (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x238D], 31,
                                     (int)dword_53C03, -31);
    place = ent[0];

    bgm_play(-1, 0);                        /* 0x26734: fade the track out   */

    if (dword_53F4A == 2) {
        int v0;
        int v1;

        msg_open_portrait(75);                                  /* 0x2674C */
        vm_run(dword_53A7D, 513, 693535, 320, 205, 76, 74, 19, 1); /* 0x26756 */
        dword_53A51 = (uint8_t *)(uintptr_t)1;
        dlg_blit_dato(dword_53A85, dword_53C67, 0);
        v0 = map_window_run();
        map_draw_text_window();
        dword_53A51 = 0;
        msg_close_portrait();
        if (v0 == -1 || dword_53C57 != 0)
            return 0;                                           /* 0x26804 */
        if ((dword_53C03 < 27 && (int32_t)dword_53BFB > 16) ||
            (dword_53C03 > 26 && (int32_t)dword_53BFB > 20)) {
            dword_53A45 = (void *)(uintptr_t)dword_53BF7;
            v1          = party_change_run();
            dword_53A45 = 0;
            if (v1 == 0)
                return 0;                                       /* 0x267F7 */
        }
        result = 1;                                             /* 0x267FD */
    }

    dword_53A45 = (void *)(uintptr_t)dword_53BF7;
    frame       = guest_malloc(0xFA00u);
    memmove(frame, (const void *)(uintptr_t)0xA0000u, 0xFA00u);
    for (i = 1; i <= 10; ++i) {
        int src_x = (byte_52363[6 * (int)place + (int)dword_53F4A] - 150)
                    * i / 10;
        int src_y = (byte_52375[6 * (int)place + (int)dword_53F4A] - 100)
                    * i / 10;

        place_zoom_blit(src_x * 128 + 20480, src_y * 128 + 12800,
                        (const char *)frame, 128 - 9 * i);
        memmove((void *)(uintptr_t)0xA0000u,
                (const void *)(uintptr_t)dword_53A49, 0xFA00u);
        pal_fade_range(0, 255, 4 * i);
    }
    pal_fade_range(0, 255, 64);
    memset((void *)(uintptr_t)0xA0000u, 0, 64000);

    if (dword_53F4A == 0) {
        bgm_play(13, 0);
        result = place_screen_run(frame);
    } else if (dword_53F4A == 4) {
        bgm_play(11, 0);
        result = place_screen4_run(frame);
    } else if (dword_53F4A != 2) {
        bgm_play(dword_53F4A == 3 ? 15 : 14, 0);
        result = place_menu_run((const char *)frame);
    }

    if (dword_53F4A != 2)
        bgm_play(10, 0);                                        /* 0x26970 */

    guest_free(frame);
    dword_53A45 = 0;
    return result;
}
