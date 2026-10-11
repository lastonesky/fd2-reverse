/* save_menu.h - the four-slot save/scenario selector (original 0x29AB2 /
 * 0x29BCB).
 *
 * The FD2.SAV image is 22987 bytes: four 2600-byte slots starting at
 * offset 12587, each holding a party/scenario block whose first byte (at
 * slot + 2560) is the scenario id.  This screen lists the four slots by
 * their scenario name (a word stream selected by that id) and lets the
 * player pick one:
 *
 *   0x29AB2  save_slot_rows    draw the four rows of `base` into `dst`; the
 *            row whose index equals `selected` is drawn in the highlight
 *            foreground (201) instead of 205, and dword_53AE1 is set to the
 *            row number + 1 before each row's stream (the name stream keys
 *            off it).  An id of 0xFF draws only the empty-slot label.
 *   0x29BCB  save_slot_select  stage the three shared 0xFA00 scratch buffers
 *            (dword_53C5B/F/63, taken from the guest heap), blit the panel
 *            sub-image, draw the rows, then run the Up/Down/Enter/Esc loop
 *            over dword_53C57.  `wait_keys` != 0 waits with the animated
 *            dlg_wait_key; otherwise it reads INT 16h AH=10h directly.
 *
 * Both are called by the save/load/new-game paths: 0x25EBB (scenario_menu),
 * 0x2968D (save) and 0x2986F (load).  True ABI is cdecl.  The Watcom stack
 * probe (`push size; call 0x3702F`) is dropped and the tail `jmp 0x28B3C`
 * (0x29AB2) / `jmp 0x26A73` (0x29BCB) are the shared four-register epilogues,
 * i.e. plain returns.  The 0xFA00 buffers are never freed here (the machine
 * leaves them in the globals for the caller to reuse), matching the other
 * shop/msg screens.  Hand-restored from the Hex-Rays decomp and the offline
 * disassembly (analysis/cseg01_disasm.txt); no differential harness yet.
 */
#ifndef GAME_SAVE_MENU_H
#define GAME_SAVE_MENU_H

#include <stdint.h>

/* 0x29AB2 - draw the four slot rows of the `base` image into `dst`. */
void save_slot_rows(int selected, int dst, int base);

/* 0x29BCB - run the four-slot selector over `base`; returns 1 on confirm,
 * -1 on cancel. */
int save_slot_select(int base, int wait_keys);

#endif /* GAME_SAVE_MENU_H */
