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

#endif /* GAME_SHOP_SCREEN_H */
