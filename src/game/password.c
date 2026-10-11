/* password.c - FD2 password / code-entry screen (0x33FAF + helpers 0x34366,
 * 0x344B4).  Direct translation of the machine code.
 *
 * 0x117E7 (the map-view handler) calls 0x33FAF to ask for a password.  The
 * screen:
 *   1. guest_malloc a 0xFA00 screen buffer and res_load (0x111BA) FDOTHER.DAT
 *      items 96 (the wheel sprite sheet), 97 (the backdrop) and 98.
 *   2. rle_decode (0x4E98D) item 97 onto the buffer; free it.
 *   3. pick a random two-digit code (0x4EBE3 util_rand % 100) and decode the
 *      matching 3-byte record of the table at 0x60181 (0x4DED4 util_rec3)
 *      into six glyph indices (0x34317 fills a 6-byte array).
 *   4. draw the frame (0x34366), fade in (0x1F525), then loop reading keys
 *      (INT 16h through the CRT int386 0x370F0) with the six-glyph wheel:
 *      left/right (4B/4D) move the cursor, enter (1C) appends the selected
 *      glyph (up to three), backspace (01) removes one.  Left/right/enter and
 *      backspace each play an SFX (0x25A96).
 *   5. free the buffers, fade out, clear the VGA frame (0x37910 memset),
 *      refresh the map view (0x11CAC) and fade in.
 *
 * Traps / decompiler artifacts:
 *  - chkstk: every function starts with `push size; call 0x3702F`; dropped.
 *  - The three entered glyphs are "checked" at 0x34215..0x3423A, but the
 *    mismatch branch at 0x34233 is an *unconditional* jump (the bytes are
 *    `39 D8 EB E9` = `cmp eax,ebx; jmp 0x3421E`), so the fail store at
 *    0x34235 is unreachable and the wrong-password bailout at 0x34243
 *    (rle_decode item 98 + j___delay(1000) + the retry counter at 0x34274 +
 *    AIL_shutdown + INT 10h text mode + printf("Sorry your password is
 *    wrong !!!") + exit(1)) is dead code.  Hex-Rays drops the whole block from
 *    its output for the same reason; it is not translated here.  As a result
 *    the screen always succeeds, and item 98 (loaded and freed) has no live
 *    use - reproduced anyway so the allocation/free sequence matches.
 *  - The live int386 is the INT 16h key read at 0x340DA (AH=0x10); the REGS
 *    block is a local whose AX sits at offset 0, so the scan code is the high
 *    byte (offset 1).  The key is remapped like the other key readers:
 *    E0/52/39 -> 1C and 53 -> 01.
 *
 * No differential harness exists for this screen; marked "restored, not
 * dynamically verified".
 */
#include "fd2_game.h"
#include "password.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* The DOS REGS block the CRT int386 reads/writes: AX at offset 0, so the
 * INT 16h scan code is the high byte (offset 1). */
#define REGS_AH(regs) (((uint8_t *)(regs))[1])

/* 0x344B4 */
void password_glyph_blit(const void *image, int index, void *surface,
                         int stride, int col, int row)
{
    const uint8_t *img = (const uint8_t *)image;
    const uint8_t *src = img + *(const int32_t *)(img + 4 * index + 6);

    sprite24_plain(src, (uint8_t *)surface + stride * row + col, stride);
}

/* 0x34366 */
void password_frame_draw(void *image, const void *screen, int number,
                         const uint8_t *glyphs, int entered_n,
                         const uint8_t *entered, int sel)
{
    char  buf[8];
    int   i;
    void *tmp = guest_malloc(0xFA00u);

    memmove(tmp, screen, 0xFA00u);
    sprintf(buf, a02d, number);          /* "%02d" */
    buf[0] -= 48;
    buf[1] -= 48;
    password_glyph_blit(image, buf[0] + 64, tmp, 320, 150, 75);
    password_glyph_blit(image, buf[1] + 64, tmp, 320, 162, 75);
    for (i = 0; i < 6; ++i)
        password_glyph_blit(image, glyphs[i], tmp, 320, 25 * i + 86, 132);
    for (i = 0; i < entered_n; ++i)
        password_glyph_blit(image, entered[i], tmp, 320, 32 * i + 116, 97);
    password_glyph_blit(dword_53A4D, 0, tmp, 320, 25 * sel + 86, 132);
    memmove((void *)(uintptr_t)VGA_P(0xA0000u), tmp, 0xFA00u);
    guest_free(tmp);
}

/* 0x33FAF */
void password_screen_run(void)
{
    uint8_t        regs[0x1C];
    uint8_t        glyphs[8];
    uint8_t        entered[3] = { 0, 0, 0 };
    int            entered_n = 0;
    int            sel = 2;
    int            code_n;
    const uint8_t *code;
    uint8_t        entered_ok;
    uint8_t        key;
    void          *buf;
    void          *img96, *img97, *img98;

    buf   = guest_malloc(0xFA00u);
    img96 = res_load(aFdotherDat, NULL, 96);
    img97 = res_load(aFdotherDat, NULL, 97);
    img98 = res_load(aFdotherDat, NULL, 98);
    rle_decode(img97, 0, 0, buf, 320, -1);
    guest_free(img97);
    pal_fade_out();

    code_n = (int)util_rand() % 100;
    code   = (const uint8_t *)util_rec3(&unk_60181, code_n);
    ev5_34317(code[0], glyphs);
    password_frame_draw(img96, buf, code_n, glyphs, 0, entered, sel);
    pal_fade_in();

    entered_ok = 0;
    do {
        password_frame_draw(img96, buf, code_n, glyphs, entered_n, entered, sel);
        kbd_flush();
        REGS_AH(regs) = 0x10;
        int386(0x16, regs, regs);
        key = REGS_AH(regs);
        if (key == 0xE0 || key == 0x52 || key == 0x39)
            key = 0x1C;
        if (key == 0x53)
            key = 0x01;

        switch (key) {
        case 0x4B:                                  /* left   */
            svc_play_sfx(dword_53EEC, 2, 1);
            sel = (sel == 0) ? 5 : sel - 1;
            break;
        case 0x4D:                                  /* right  */
            sel = (sel + 1) % 6;
            svc_play_sfx(dword_53EEC, 2, 1);
            break;
        case 0x1C:                                  /* enter  */
            svc_play_sfx(dword_53EEC, 7, 1);
            entered[entered_n++] = glyphs[sel];
            password_frame_draw(img96, buf, code_n, glyphs, entered_n,
                                entered, sel);
            if (entered_n == 3)
                entered_ok = 1;
            else
                ev5_34317(code[entered_n], glyphs);
            break;
        case 0x01:                                  /* backspace */
            if (entered_n != 0) {
                svc_play_sfx(dword_53EEC, 0, 1);
                ev5_34317(code[--entered_n], glyphs);
            }
            break;
        default:
            break;
        }
    } while (entered_ok == 0);

    /* 0x342B4: the entered glyphs are never rejected (dead check, see the
     * module doc), so the screen ends here. */
    byte_53A44 = 1;

    guest_free(img96);
    guest_free(buf);
    guest_free(img98);
    pal_fade_out();
    memset((void *)(uintptr_t)VGA_P(0xA0000u), 0, 0xFA00u);
    map_view_update(1);
    pal_fade_in();
}
