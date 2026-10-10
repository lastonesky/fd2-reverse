/* box_popup.h - character/inventory popup box (original 0x17E0B .. 0x1C1C3).
 *
 * The "open a party member's box" interaction: a framed panel with the two
 * HP/MP meters, the numeric readouts, the item rows and a name stream, opened
 * with a slide-in animation, driven by the keyboard until a row is confirmed.
 *
 *   0x17FC0  box_status_draw  draw the stat panel (two meters, number
 *            readouts, three vm_run strings, frame tiles; the three item
 *            "slot present" bytes at record+37..39 gate the small tiles).
 *   0x184C0  box_items_draw   draw the item rows: skip slots whose record
 *            byte at +2*i+10 has bit 0x80 set, then tile+number per item type
 *            (tbl_ptr base 0x602AD stride 23) plus the 3-flag row.
 *   0x17EEF  box_open_init    load DATO.DAT (dword_53A85), stage the box
 *            frame, blit the mask strips and draw the panel.
 *   0x17E0B  box_open_anim    allocate the three 0xFA00 scratch buffers, run
 *            0x17EEF + 0x184C0 and the 12-step slide (fb_slide_step).
 *   0x1B9DE  box_input        one key step: highlight moves (up/down/page),
 *            confirm returns 1 (or -1 on ESC) through the item lookup.
 *   0x1B932  box_popup        run the box until the player confirms/cancels,
 *            slide out and free the buffers; returns "not cancelled".
 *   0x1C142  box_item_select  mark one item slot (clear the 0x40 bit of the
 *            same-half slots first), returns the slot.
 *   0x1C1C3  box_item_usable  is item id `item` in the member's item-class
 *            row (tbl_ptr base 0x6188A, stride 7, index = record byte +32)?
 *
 * ABI = cdecl.  Record pointers index the 80-byte table dword_53A45; `base`
 * is an absolute screen address.  Hand-restored, compile-verified, no
 * differential harness yet.
 */
#ifndef GAME_BOX_POPUP_H
#define GAME_BOX_POPUP_H

/* 0x17FC0 */
void box_status_draw(int rec, int base);

/* 0x184C0 */
void box_items_draw(int rec, int highlight, int base);

/* 0x17EEF */
void box_open_init(int rec, int base);

/* 0x17E0B */
void box_open_anim(int rec);

/* 0x1B9DE */
int box_input(int rec, int mode);

/* 0x1B932 */
int box_popup(int rec, int mode);

/* 0x1C142 */
int box_item_select(int rec, int slot);

/* 0x1C1C3 */
int box_item_usable(int rec, int item);

#endif /* GAME_BOX_POPUP_H */
