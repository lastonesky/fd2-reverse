/* reveal.h - expanding-circle portrait reveal (original 0x219AD .. 0x24618).
 *
 * Used when a scene opens a character close-up: the picture appears through a
 * growing circle whose per-row half-width is a circular arc, and the rows are
 * then palette-translated into the map bitmap. The three routines are:
 *
 *   0x219AD  reveal_arc   draw the rows [y0, y1) of a circle of `radius`
 *            centred at (cx, cy): for each row the half-width is
 *            (int)(sqrt(radius^2 - dy^2) * scale / 10), clamped to the 312-px
 *            screen, and the span is run through util_translate (0x4DEEC).
 *   0x22046  reveal_band  two reveal_arc passes (all rows, then the rows below
 *            the centre, after a portrait refresh) plus a solid
 *            (int)(radius*1.6)-half-width band for the rows [y0, cy).
 *   0x24618  portrait_reveal  the driver: renders the 13x8-tile view into a
 *            0x25680 scratch buffer, then steps a growing circle from the
 *            given x offset while displaying 456-stride 312x192 slices, and
 *            finally fades the palette in.
 *
 * Target buffer is the map bitmap dword_53A49 (stride 456, +0x8088 byte
 * offset). ABI is cdecl; the machine code ends in the shared epilogues
 * 0x1951B/0x10B46, i.e. plain returns here. Hand-restored, compile-verified,
 * no differential harness yet.
 */
#ifndef GAME_REVEAL_H
#define GAME_REVEAL_H

/* 0x219AD */
void reveal_arc(int cx, int cy, int radius, int scale, int y0, int y1,
                const void *table);

/* 0x22046 */
void reveal_band(int cx, int cy, int radius, int y0, int y1,
                 const void *table);

/* 0x24618 */
int portrait_reveal(int col, int row, int offset, int step);

/* 0x2189A */
int portrait_reveal_cell(int rec, int radius, int step);

#endif /* GAME_REVEAL_H */
