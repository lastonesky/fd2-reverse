/* party_panel.h - map party-status panel: row painter + sliding band helpers
 * (original 0x1AF1E, 0x1AF99, 0x1B019, 0x1B0AD, 0x1B14B, 0x1B41D).
 *
 * These six routines are the parts of the world-map party pop-up that the
 * orchestrator 0x1B1E7 (still pending) drives. 0x1B41D paints the panel rows
 * (portrait/stat tiles through res_blit6, the stat numbers through
 * dlg_draw_number and the two name streams through vm_run) into a scratch
 * 0xFA00 buffer; the five band helpers then copy horizontal bands out of that
 * scratch buffer into the frame buffer dword_53A49 with a vertical slide
 * offset, so the panel rows appear to slide in (steps 0..11) and back out.
 *
 * The five bands differ only in their geometry (source/destination offsets,
 * row width and row count):
 *   0x1AF1E  170 bytes x 117 rows, source row 75 downwards, dest follows it
 *   0x1AF99  170 bytes x 117 rows, fixed source offset 11915, dest follows
 *   0x1B019  102 bytes x  17 rows, source offset 109 + 320*r
 *   0x1B0AD  170 bytes x  16 rows, fixed source offset 49675
 *   0x1B14B   63 bytes x  15 rows, fixed source offset 55169
 * Every band is a plain cdecl (int base, int step); the disassembly is the
 * authority for the constants and the clipping. Hand-restored,
 * compile-verified, no differential harness yet.
 */
#ifndef GAME_PARTY_PANEL_H
#define GAME_PARTY_PANEL_H

#include <stdint.h>

/* 0x1AF1E - slide the 170x117 band whose source row follows the step. */
void party_panel_band_a(const uint8_t *src, int step);

/* 0x1AF99 - slide the 170x117 band at fixed source offset 11915. */
void party_panel_band_b(const uint8_t *src, int step);

/* 0x1B019 - slide the 102x17 first stat band. */
void party_panel_band_c(const uint8_t *src, int step);

/* 0x1B0AD - slide the 170x16 second stat band. */
void party_panel_band_d(const uint8_t *src, int step);

/* 0x1B14B - slide the 63x15 third stat band. */
void party_panel_band_e(const uint8_t *src, int step);

/* 0x1B41D - paint one panel row block into dst at the given pitch. */
void party_panel_draw(uint8_t *dst, int pitch);

#endif /* GAME_PARTY_PANEL_H */
