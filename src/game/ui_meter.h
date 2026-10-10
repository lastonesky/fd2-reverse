/* ui_meter.h - unit status-panel + meter-bar drawing (original 0x17D6F ..
 * 0x2FACD).
 *
 * A small family that draws the character status box (HP/MP meters and the
 * numeric readouts) out of the box-frame sprite resource dword_53A81:
 *
 *   0x17D6F  ui_meter_tiles  draw `count` meter tiles (dlg_frame_tile) at
 *            dst+1..dst+count, using tile, tile+1 (body) and tile+2 (end);
 *            when count==0 it draws the fixed 101-segment empty meter.
 *   0x18795  ui_meter_bar    convert a value/max pair into a segment count and
 *            forward to ui_meter_tiles.
 *   0x18C6D  ui_status_panel blit the panel background (rle2_blit) then the two
 *            meters, the number readouts and the portrait's name stream.
 *   0x2FACD  ui_status_panel_at  pick the screen offset for a record (the map
 *            HUD slot) and call ui_status_panel.
 *
 * Signatures are cdecl. The record index addresses the 80-byte character table
 * (dword_53A45 + 80*rec); `pitch` is the destination stride (always 320 here).
 * 0x18C6D and 0x17D6F end by jumping into the shared epilogue 0x1317D - a plain
 * return in C. These are hand-restored and compile-verified but have no
 * differential harness yet.
 */
#ifndef GAME_UI_METER_H
#define GAME_UI_METER_H

/* 0x17D6F */
void ui_meter_tiles(int dst, int pitch, int count, int tile);

/* 0x18795 */
void ui_meter_bar(int dst, int pitch, int tile, int value, int max);

/* 0x18C6D */
void ui_status_panel(char *dst, int pitch, int rec);

/* 0x2FACD */
void ui_status_panel_at(int dst, int rec);

#endif /* GAME_UI_METER_H */
