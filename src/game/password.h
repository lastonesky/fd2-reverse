/* password.h - FD2 password / code-entry screen.  See password.c. */
#ifndef GAME_PASSWORD_H
#define GAME_PASSWORD_H

#include <stdint.h>

/* 0x33FAF - run the password screen: loads FDOTHER.DAT items 96/97/98, shows
 * a random two-digit code over a six-glyph selection wheel and reads keys
 * (INT 16h) until three glyphs are chosen.  The machine leaves a value in EAX;
 * every call site ignores it, so the translation returns void. */
void password_screen_run(void);

/* 0x34366 - compose one screen frame: copy `screen` into a fresh 0xFA00
 * buffer, draw the two digits of `number` and the six wheel glyphs `glyphs`,
 * overlay the `entered_n` chosen glyphs `entered`, mark the cursor at `sel`,
 * then copy the buffer to the VGA frame. */
void password_frame_draw(void *image, const void *screen, int number,
                         const uint8_t *glyphs, int entered_n,
                         const uint8_t *entered, int sel);

/* 0x344B4 - blit sub-image `index` of the `image` container into `surface` at
 * column `col`, row `row`, stride `stride`. */
void password_glyph_blit(const void *image, int index, void *surface,
                         int stride, int col, int row);

#endif /* GAME_PASSWORD_H */
