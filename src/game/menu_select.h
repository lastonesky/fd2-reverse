/* menu_select.h - interactive selection loops of the place/scene menus
 * (original 0x26E38 and 0x27D33).
 *
 * Both are the small modal loops the town/place screens run on top of the
 * party status renderer (0x26EDA status_screen_run, 0x2810B
 * status_names_draw, 0x27816/0x278E7 screen_band_forward/backward):
 *
 *   0x26E38  menu_choice4     a four-slot wrapping selector: left/right move
 *            the cursor through dword_53C57 (0..3 with wrap), confirm (Enter
 *            /Space) returns 1 and cancel (Esc) returns -1.
 *   0x27D33  menu_list_select the scrolling list form over dword_53BFB
 *            entries: it allocates the three 0xFA00 screen buffers in the
 *            shared globals dword_53C5B/F/63, snapshots the VGA, lays out the
 *            first name rows (status_names_draw + msg_blit_band) and then
 *            scrolls the window (dword_53F4E) with the band effects. Up/Down
 *            move one row, PgUp/PgDn two; confirm returns 1, cancel -1.
 *
 * ABI = cdecl, no arguments. The 0x53C5B/F/63 buffers are the same screen
 * scratch globals msg.c and box_popup.c use; they are taken from the guest
 * heap (guest_malloc), never from the host heap. Hand-restored,
 * compile-verified, no differential harness yet.
 */
#ifndef GAME_MENU_SELECT_H
#define GAME_MENU_SELECT_H

/* 0x26E38 - four-slot wrapping selector. Returns 1 on confirm, -1 on cancel. */
int menu_choice4(void);

/* 0x27D33 - scrolling list selector over dword_53BFB entries. Returns 1 on
 * confirm, -1 on cancel. */
int menu_list_select(void);

#endif /* GAME_MENU_SELECT_H */
