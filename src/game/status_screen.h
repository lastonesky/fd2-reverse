/* status_screen.h - party status/selection screen effects (original 0x26EDA ..
 * 0x2810B).
 *
 * A small driver plus two drawing helpers used by the status/party view:
 *
 *   0x2810B  status_names_draw   draw up to six party rows: the character
 *            portrait tile (sprite24_plain49) and the name stream (vm_run),
 *            with the highlighted row drawn in the bright palette.
 *   0x27079  status_panel_draw   advance the panel animation (dword_53F52/72)
 *            and blit the frame strips (mode 1/3), the icon row (mode 2) or
 *            the highlighted portrait (mode 0).
 *   0x26EDA  status_screen_run   the interactive loop: draw, wait for the BIOS
 *            tick, alternate portrait frames (dlg_blit_dato) and return the
 *            pending key (INT 16h) once one arrives.
 *
 * ABI = cdecl.  The screen coordinates are absolute VGA addresses (0xA0000
 * based).  The `layout` array in status_panel_draw is the four dwords copied
 * from the 0x52418 table by status_screen_run.  Hand-restored,
 * compile-verified, no differential harness yet.
 */
#ifndef GAME_STATUS_SCREEN_H
#define GAME_STATUS_SCREEN_H

/* 0x2810B */
void status_names_draw(int highlight, int base);

/* 0x27079 */
void status_panel_draw(int *layout, int mode);

/* 0x26EDA */
int status_screen_run(int mode);

#endif /* GAME_STATUS_SCREEN_H */
