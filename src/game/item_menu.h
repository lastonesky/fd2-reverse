/* item_menu.h - the map "use an item on a party member" screen helpers
 * (the three closure-ready callees of the still-pending driver sub_2AA00,
 * 0x2AA00).
 *
 * The driver draws the item panel and loops over the eligible party records;
 * the helpers live here:
 *
 *   0x2AE0E  item_menu_list        scan the record table and collect every
 *                                  member the current item can target: record
 *                                  byte +33 >= 0x14, unit type (record +7)
 *                                  < 0x12 and != 7.  Writes each record number
 *                                  into `records` and its menu kind code into
 *                                  `kinds` (type+32, promoted to type+50, or
 *                                  52 for a valid type-9 reaction) and returns
 *                                  the count.
 *   0x2AC7D  item_menu_use         use the item on record `rec`: open the
 *                                  talking portrait, play item script 595,
 *                                  page the five dialogue pages 490..494, then
 *                                  play script 596 when the record has a
 *                                  reaction, and stamp the record's item state.
 *   0x2FB2C  item_menu_figure_play play the item-use figure animation: cycle
 *                                  the three BG.DAT strips while stamping the
 *                                  user's and the target group's FIGANI.DAT
 *                                  frames, fading out and back in.
 *
 * ABI = cdecl; the argument order, the constants and the branch conditions were
 * decoded from the offline disassembly (analysis/cseg01_disasm.txt), which is
 * authoritative.  All real callees are already-restored assets (vm_run,
 * msg_*, unit_metrics, ev3_2AEDB, res_load/res_blit, rle_decode, gfx_copy_rows,
 * pal_fade_*, figure_frame_run, svc_wait_ticks, kbd_flush, tbl_ptr, guest_mem);
 * the item/table accessors go through tbl_ptr.  Hand-restored,
 * compile-verified, no differential harness yet (unverified).
 */
#ifndef GAME_ITEM_MENU_H
#define GAME_ITEM_MENU_H

#include <stdint.h>

/* 0x2AE0E - fill `records`/`kinds` with the usable party members and return
 * their count.  `records` receives the 80-byte-table indices, `kinds` the
 * per-entry menu code. */
int item_menu_list(uint8_t *records, uint8_t *kinds);

/* 0x2AC7D - apply the current item to record `rec` (portrait + dialogue pages
 * + reaction script + the record's item-state reset). */
void item_menu_use(int rec);

/* 0x2FB2C - play the item-use figure animation for record `rec` against FIGANI
 * group `group` over the scrolling BG.DAT strips. */
void item_menu_figure_play(int rec, int group);

#endif /* GAME_ITEM_MENU_H */
