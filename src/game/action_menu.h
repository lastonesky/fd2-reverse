/* action_menu.h - map command/action list panel (original 0x17AED/0x1CEED/
 * 0x1D51D/0x1CFF0).
 *
 *   0x17AED  action_menu_open   open the record's box (box_open_anim) and,
 *            when the record has any action bits, reveal the action list with
 *            a 7-step band wipe around action_menu_draw, then slide the box
 *            out and free the three scratch buffers.
 *   0x1CEED  action_menu_draw   draw the available action rows of record
 *            `rec` into the absolute framebuffer address `base`, highlighting
 *            row `sel`.
 *   0x1D51D  action_menu_select one key step: draw the list, navigate with
 *            up/down/page/left/right, and confirm/cancel (returns 1/0/-1).
 *   0x1CFF0  action_menu_run   the menu driver: stage the panel (box_open_init
 *            + action_menu_draw + a 12-step slide), run the selection loop,
 *            blit the snapshot back and execute the chosen action - either
 *            figure_effect_play (ids < 9, 0x18 or > 0x1B) or the funcs_1541F
 *            effect table - then collect and run the resulting event scripts.
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
 * ABI = cdecl.  action_menu_draw takes (record index, highlighted row,
 * absolute framebuffer address); action_menu_open and action_menu_run take
 * the record index; action_menu_select takes the record index and returns 1
 * (confirm), 0 (still running) or -1 (ESC).  action_menu_run returns -1 when
 * the player cancelled, 0 on the portrait glide-out path and 1 when the
 * action and its event script have run.
 * Hand-restored, compile-verified, no differential harness yet.
 */
#ifndef GAME_ACTION_MENU_H
#define GAME_ACTION_MENU_H

/* 0x17AED */
void action_menu_open(int rec);

/* 0x1CEED */
void action_menu_draw(int rec, int sel, uint8_t *base);

/* 0x1D51D */
int action_menu_select(int rec);

/* 0x1CFF0 */
int action_menu_run(int rec);

#endif /* GAME_ACTION_MENU_H */
