/* sys_menu.h - in-game menu cluster (original 0x16F55, 0x1728C, 0x19DF7).
 *
 * The three functions are the game's map-side menu system, reached from the
 * event tick (0x117E7 calls 0x16F55 in a `while (sub_16F55() == 0)` loop):
 *
 *   0x16F55  sys_menu_run     the outer map command menu.  Snapshots the two
 *                             4-int cursor tables 0x51E9F / 0x53EF2, runs the
 *                             direction-patch picker (map_cursor_patch_expand /
 *                             map_cursor_dir_select / map_cursor_patch_collapse)
 *                             and dispatches on the picked direction dword_53C57:
 *                               0 -> sys_menu_system   (the save/load panel)
 *                               1 -> a map engage run (portrait + map window +
 *                                    party_change_map_seq over the records)
 *                               2 -> sys_menu_audio    (the sound toggles)
 *                               3 -> another dialogue run
 *                             returns 1/0/-1 (sub-menu value passed through).
 *
 *   0x1728C  sys_menu_audio   the four sound toggles (music byte_51E61, effects
 *                             byte_51E62, byte_53AF9, byte_51AAB).  Rebuilds the
 *                             0x51EAF patch table from the four flags and loops
 *                             the picker, flipping the chosen flag; picking music
 *                             on/off retunes AIL (0x3B124) immediately.
 *
 *   0x19DF7  sys_menu_system  the save/load/party panel menu: probes FD2.SAV,
 *                             flags the engaged records in the patch table, then
 *                             dispatches on dword_53C57:
 *                               0 -> party_panel_run   (0x1B1E7)
 *                               1 -> save the game to FD2.SAV
 *                               2 -> load scenario_load() from FD2.SAV
 *                               other -> the "quit to title" dialogue
 *                             returns 0 on cancel, 1 when a pick ran, -1 on the
 *                             quit path.
 *
 * ABI = cdecl, no arguments (all three).  Decoded from the offline disassembly
 * (analysis/cseg01_disasm.txt, 0x16F55/0x1728C/0x19DF7); it is the authority for
 * the argument order, constants and control flow.  The three share one frame
 * layout (`push ebx/esi/edi; sub esp,20h` with a Watcom stack probe up front),
 * so 0x1728C and 0x19DF7 tail-jump into 0x16F55's epilogue at 0x16FDD (a plain
 * `return`) and 0x19DF7's default path even jumps into 0x16F55's body at
 * 0x1716F - reproduced here as the local sys_menu_cancel_frame() helper plus an
 * explicit `return 1`.  Hand-restored, compile-verified, no differential
 * harness yet.
 */
#ifndef GAME_SYS_MENU_H
#define GAME_SYS_MENU_H

/* 0x16F55 - outer map command menu.  Returns 1 (cancelled / self-contained
 * branch), 0 (audio sub-menu closed), or -1 (the quit path of the save/load
 * panel). */
int sys_menu_run(void);

/* 0x1728C - the four sound-toggle menu. */
void sys_menu_audio(void);

/* 0x19DF7 - the save/load/party-panel menu.  Returns 0 on cancel, 1 when a
 * pick ran, -1 on the quit path. */
int sys_menu_system(void);

#endif /* GAME_SYS_MENU_H */
