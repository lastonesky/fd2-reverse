/* action_menu.h - map command/action list panel (original 0x1CEED).
 *
 *   0x1CEED  action_menu_draw   draw the available action rows of record
 *            `rec` into the absolute framebuffer address `base`, highlighting
 *            row `sel`.
 *
 * The panel is drawn on top of the box frame opened by box_open_init (0x17EEF).
 * It collects the indices of the set bits of record bytes +26..+30
 * (rec_collect_slot_bits, 0x1C269) - one row per available action - and lays
 * them out in columns of four rows:
 *   - the row's x is 100*(i/4)+18, its y is 22*(i%4)+103 scanlines;
 *   - the action name comes from text stream dword_53A7D sub `slot+441`
 *     (vm_run, 0x15F84), foreground 205 (201 for the highlighted row);
 *   - a frame tile (index 92, dlg_frame_tile 0x1685C) and a two-digit number
 *     (action-entry byte +5, dlg_draw_number 0x187D6) sit under it.
 * The action table is the 7-byte entry table at 0x619FD (accessed through
 * tbl_ptr, base &fd2_data2[0x19FD]).
 *
 * ABI = cdecl, three int arguments (record index, highlighted row, absolute
 * framebuffer address).  Called by the action select loop 0x1D51D, the menu
 * driver 0x1CFF0 (both still pending) and the box open animation 0x17AED.
 * Hand-restored, compile-verified, no differential harness yet.
 */
#ifndef GAME_ACTION_MENU_H
#define GAME_ACTION_MENU_H

/* 0x1CEED */
void action_menu_draw(int rec, int sel, int base);

#endif /* GAME_ACTION_MENU_H */
