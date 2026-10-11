/* shop_screen.h - the interactive list screens of the shop/place menu.
 *
 * The four screen drivers of the shop family whose row painters live in
 * shop_rows.h.  Each stages its picture in the three shared 0xFA00 screen
 * scratch buffers (the globals dword_53C5B / dword_53C5F / dword_53C63, taken
 * from the guest heap) and runs the party-status renderer's key loop:
 *
 *   0x27738  shop_items_paint      one-shot draw of the item list: allocate
 *            the three buffers, snapshot the VGA, blit the panel sub-image at
 *            dword_53C63 + 35845, paint the item rows into dword_53C63 and
 *            push the six 13-pixel bands to the screen.
 *   0x275E6  shop_items_run        the item list's interactive loop over
 *            `count` entries: mode-1 status screen, Up/Down (and PgUp/PgDn)
 *            scroll the window dword_53F4E by two, Left/Right move the cursor
 *            by one (clamped to the row ends); every move repaints through
 *            shop_item_rows.  Enter/Space return 1, Esc returns -1.
 *   0x2A29D  shop_party_run        the same loop for a party list (mode 2)
 *            drawn by shop_party_bar_rows: Up/Down move the cursor by one and
 *            scroll the window dword_53F4E by one.  Returns 1 / -1.
 *   0x2A857  shop_party_equip_run  the party-list variant whose fourth label
 *            stream comes from an equipment array (shop_party_equip_rows).
 *            Returns 1 / -1.
 *
 * The place-menu driver 0x279BC (place_menu_run) and its four options are
 * driven here too:
 *
 *   0x2872B  shop_place_buy_run the buy screen of the place menu's first
 *            option, driven from the place's stock list: run shop_items_paint/
 *            run over the list, look the confirmed item up in the 23-byte
 *            table 0x602AD (class byte +0, price word +19) and gather the
 *            members that can take it (box_item_usable), then claim a slot
 *            (rec_slot_claim + unit_metrics) and spend the price
 *            (place_money_spend).  Returns 0 on cancel.
 *   0x28CBD  shop_sell_run     the member item list: pick a member with
 *            menu_list_select, gather the state-bit-clear item slots of their
 *            record, run shop_items_run over them, and on confirm show the
 *            item's dialogue and, if the feedback window accepted, gain the
 *            sale price (place_money_gain) and remove the slot.  With no items
 *            it only shows the no-goods message.  Returns -1.
 *   0x28EFE  shop_box_run      run the character popup box (box_run) for the
 *            member picked with menu_list_select, preserving dword_53C67, then
 *            reload DATO.DAT for the current place.  Returns -1.
 *   0x2A43E  shop_buy_run      the buy screen: filter the usable records
 *            (0x2A07A), choose one with shop_party_run, and on confirm spend
 *            dword_53AE1 (price * count) and award the record; loops around the
 *            feedback window.  Returns 0.
 *   0x2A07A  shop_usable_items collect the indices whose rec_flag is 1 into the
 *            caller's buffer; returns the count.
 *
 * ABI = cdecl.  The three scratch buffers are the same globals msg.c,
 * menu_select.c and box_popup.c share; they are allocated with guest_malloc,
 * never the host heap.  The two `..._run` loops return the machine's EAX
 * (1 on confirm, -1 on cancel) through the shared epilogue at 0x26A73
 * (`mov eax,esi; pop ebp/edi/esi/ebx; ret`).  Hand-restored from the Hex-Rays
 * decomp and the offline disassembly (analysis/cseg01_disasm.txt); the
 * disassembly is authoritative for the argument count (the decompiler drops
 * the count/members arguments of 0x2A29D/0x2A857 because of the Watcom stack
 * probe).  No differential harness yet.
 */
#ifndef GAME_SHOP_SCREEN_H
#define GAME_SHOP_SCREEN_H

#include <stdint.h>

/* 0x27738 - stage and draw the shop item list once. */
void shop_items_paint(int count, const uint8_t *items, uint8_t half_price);

/* 0x275E6 - interactive item-list loop. Returns 1 on confirm, -1 on cancel. */
int shop_items_run(int count, const uint8_t *items, uint8_t half_price);

/* 0x2A29D - interactive party-list loop. Returns 1 on confirm, -1 on cancel. */
int shop_party_run(int count, const uint8_t *members);

/* 0x2A857 - interactive party-list loop with equipment labels. Returns 1 on
 * confirm, -1 on cancel. */
int shop_party_equip_run(int count, const uint8_t *members,
                         const uint8_t *equip);

/* 0x28CBD - the place menu's member item list (sell/inspect).  Returns -1. */
int shop_sell_run(void);

/* 0x28EFE - open the member's popup box, then reload DATO.DAT.  Returns -1. */
int shop_box_run(void);

/* 0x2A43E - the place menu's buy screen.  Returns 0. */
int shop_buy_run(void);

/* 0x2872B - the place menu's first option: the buy screen driven from the
 * place's stock list (`count` item ids in `items`).  Returns 0 on cancel. */
int shop_place_buy_run(int count, const uint8_t *items);

/* 0x2A07A - collect the indices whose rec_flag is 1 into `dst`; count. */
int shop_usable_items(uint8_t *dst);

/* 0x279BC - the place menu: stage the current place's FDOTHER sub-image, draw
 * the money readouts and loop on menu_choice4 over the four options, running
 * the chosen sub-screen (shop_place_buy_run / shop_sell_run / shop_box_run /
 * shop_give_run).  On cancel it plays the closing zoom-out and frees the
 * resource.  `src` is the surface that zoom-out blits from.  Returns 1 when
 * dword_53F4A is 0, else 0 (the machine's EAX; caller 0x2670E is pending). */
int place_menu_run(const char *src);

/* 0x28F65 - the fourth option: hand one item from a chosen member to another.
 * Both callers ignore EAX; the only exit is the member-selection cancel. */
void shop_give_run(void);

#endif /* GAME_SHOP_SCREEN_H */
