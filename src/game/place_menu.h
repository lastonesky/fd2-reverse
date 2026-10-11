/* place_menu.h - location/place-menu helpers and the place-list entry driver
 * (original 0x2670E, 0x2921A, 0x28B41, 0x26A0D, 0x26A7A, 0x26B91).
 *
 * dword_53F4A selects the current place.  The menu ring itself is
 * menu_list_select (0x27D33); the place-list entry driver 0x2670E
 * (place_menu_commit) runs the selected place via the place/shop sub-screens in
 * shop_screen.c (0x279BC / 0x2872B / 0x28CBD) and place_screen.c (0x29300 /
 * 0x29620 / 0x29DAA).  These five helpers are the screen pieces those drivers
 * share:
 *
 *   0x26A0D  copy the current place's name string into a caller buffer, stop at
 *            a 0xFF terminator; returns the number of bytes copied.
 *   0x2921A  nearest-neighbour zoom blit of a 320x200 source into the live frame
 *            dword_53A49 (the zoom-out that ends the place menu).
 *   0x28B41  the place-specific wipe/fade transition (switch on dword_53F4A).
 *   0x26A7A  roll the money counter dword_53BF3 up by `amount` digit by digit.
 *   0x26B91  roll it down by `amount` digit by digit.
 *
 * 0x2670E returns the chosen screen's result (EBX/EBP in the machine); see
 * place_menu.c for the branch map and the intro block for place type 2.
 *
 * True ABI is cdecl; the argument order was decoded from the pushes in the
 * offline disassembly (analysis/cseg01_disasm.txt), which is the authority for
 * the constants and the branch conditions.  Every callee is an already-restored
 * asset: ev5_26C9B (0x26C9B), res_blit6 (0x16886), dlg_blit_dato (0x16559),
 * pal_fade_add (0x11DF2), svc_wait_ticks (0x17AA9), kbd_flush (0x4E381) and the
 * CRT j___delay.  The Watcom stack probe (`push size; call 0x3702F`) and the
 * shared epilogue at 0x26B8A are plain returns here.  Hand-restored,
 * compile-verified, no differential harness yet (unverified).
 */
#ifndef GAME_PLACE_MENU_H
#define GAME_PLACE_MENU_H

#include <stdint.h>

/* 0x2670E - enter the selected place: snapshot the frame, play the ten-step
 * zoom-out, then run the screen for the current place type.  Returns the
 * screen's result (1 for the place-2 intro when the party screen confirms,
 * 0 otherwise / on cancel). */
int place_menu_commit(void);

/* 0x26A0D - copy the current place's name (table at dword_53F56, sub-string
 * chosen by dword_53F4A) into `dst` up to 0xFF or the place's name length.
 * Returns the byte count. */
int place_name_copy(char *dst);

/* 0x2921A - zoom `src` (a 320x200 stride-320 surface) into dword_53A49.  `src_x`
 * and `src_y` are the source point, times 128 (fixed point); `zoom` is the
 * magnification times 128.  Nearest neighbour, clipped to the source surface;
 * the destination is cleared first. */
void place_zoom_blit(int src_x, int src_y, const char *src, int zoom);

/* 0x28B41 - play the transition for the current place (dword_53F4A): 1/3/4/5
 * each run their own band wipe / palette fade, then the first DATO sub-image is
 * shown and pending keys are dropped. */
void place_transition(void);

/* 0x26A7A - add `amount` to dword_53BF3 and animate the digits rolling up. */
void place_money_gain(int amount);

/* 0x26B91 - subtract `amount` from dword_53BF3 and animate the digits rolling
 * down. */
void place_money_spend(int amount);

#endif /* GAME_PLACE_MENU_H */
