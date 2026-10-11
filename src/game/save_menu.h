/* save_menu.h - the FD2.SAV four-slot save/load family (original 0x2968D /
 * 0x2986F / 0x29AB2 / 0x29BCB).
 *
 * The FD2.SAV image is 22987 bytes: four 2600-byte slots starting at
 * offset 12587, each holding a party/scenario block whose first byte (at
 * slot + 2560) is the scenario id.  The two screen helpers list the four
 * slots by their scenario name (a word stream selected by that id) and let
 * the player pick one; the two drivers read/write the image around them:
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
 *   0x2968D  save_slot_write   the "save game" driver: read the current
 *            FD2.SAV image (or 0xFF-fill it when absent), run save_slot_select
 *            over it with the caller's `wait_keys`, and for the chosen slot
 *            copy the live party table (dword_53BF7, 0xA00 bytes) plus the
 *            ten-byte state header (dword_53C03/53BFB/53BF3, byte_51AAB/
 *            53AF9/51E61/51E62) into it, stamp the trailing sum
 *            (util_sum_tail4 at 0x59C7), obfuscate and write FD2.SAV back.
 *            When the saved state id maps to an in-game map (byte_523E7 == 0)
 *            it also opens the save-complete portrait card.
 *   0x2986F  save_slot_load    the "load game" driver: same image load, then
 *            save_slot_select with the animated wait; for the chosen slot it
 *            rejects empty (0xFF) slots and either shows the map-incompatible
 *            portrait (byte_523E7 != 0) or republishes the slot into the party
 *            table / state globals, rebuilds the FDICON.B24 atlas
 *            (unit_entry_data) and sets dword_53F56 (the 0x4E809 table
 *            accessor, tbl_ptr) before the confirmation card.
 *
 * All four are called by the save/load/new-game paths: 0x25EBB
 * (scenario_menu), 0x26152 (scene_apply_state), 0x2670E, 0x29300 and 0x2B777.
 * True ABI is cdecl.  The Watcom stack probe (`push size; call 0x3702F`) is
 * dropped and the tail `jmp 0x28B3C` (0x29AB2) / `jmp 0x26A73` (0x29BCB) are
 * the shared four-register epilogues, i.e. plain returns.  The 0xFA00 buffers
 * are never freed here (the machine leaves them in the globals for the caller
 * to reuse), matching the other shop/msg screens.  0x2968D/0x2986F use the
 * guest-heap seam (guest_malloc/guest_free for the 0x59CB image) and libc for
 * the CRT file pair, as scenario_load.c does.  Hand-restored from the Hex-Rays
 * decomp and the offline disassembly (analysis/cseg01_disasm.txt); no
 * differential harness yet.
 */
#ifndef GAME_SAVE_MENU_H
#define GAME_SAVE_MENU_H

#include <stdint.h>

/* 0x29AB2 - draw the four slot rows of the `base` image into `dst`. */
void save_slot_rows(int selected, uint8_t *dst, const uint8_t *base);

/* 0x29BCB - run the four-slot selector over `base`; returns 1 on confirm,
 * -1 on cancel. */
int save_slot_select(const uint8_t *base, int wait_keys);

/* 0x2968D - write the current game into the selected slot of the FD2.SAV
 * image; when wait_keys != 0 the selector waits for animated keys (the
 * new-game path passes the menu id, the save command passes 0). */
void save_slot_write(int wait_keys);

/* 0x2986F - pick a slot with the FD2.SAV image and republish it into the live
 * scene globals.  The machine return value is free()'s leftover and unused. */
void save_slot_load(void);

#endif /* GAME_SAVE_MENU_H */
