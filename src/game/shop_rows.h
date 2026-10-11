/* shop_rows.h - the row painters of the shop/place menu screens.
 *
 * Three scrollable-list row painters shared by the town-place screens driven
 * from sub_2670E (0x2670E, the place menu reached from scene_apply_state):
 *
 *   0x272D0  shop_item_rows         the item list: up to six entries in two
 *            columns, each an item-table type tile (59/60/61), the item name
 *            stream, a type sub-tile (64..67, or 41 for the rest), a signed
 *            price/stat number (record words +1/+5/+14, base 42) and a numeral
 *            field (item word +19, base 119).  `half_price` draws three
 *            quarters of that numeral (3*v >> 2).
 *   0x2A0C2  shop_party_bar_rows    up to three party records: the unit sprite
 *            (bank dword_53A61, frame 12*rec + dword_53F72 with 3 folded to 1),
 *            the name stream (+8)+1, two label streams (+31)+140 / (+32)+150
 *            and a 5-digit field rec[33] * (int16)word_52399[rec[32]-1] over an
 *            equipment-strip tile 15.
 *   0x2A694  shop_party_equip_rows  up to three party records: the same unit
 *            sprite and the first three name/label streams, plus a fourth
 *            label stream read through the (index-0x20)*2 table at 0x615FE
 *            (the shop's equipment list, passed as `equip`).
 *
 * All three take a scroll window (dword_53F4E) over an index array and select
 * the bright foreground colour (201) for the row matching `cursor` (else 205).
 * `dst` is the screen/VGA buffer base (0xA0000 or the staged buffer).
 *
 * Hand-restored from the Hex-Rays decomp and the offline disassembly
 * (analysis/cseg01_disasm.txt); the disassembly is authoritative for the
 * argument order, the row geometry and the sub-tile indices.  The callers
 * (0x275E6/0x27738/0x2A29D/0x2A857) are still pending.  No differential
 * harness yet.
 */
#ifndef GAME_SHOP_ROWS_H
#define GAME_SHOP_ROWS_H

#include <stdint.h>

/* 0x272D0 - draw the item-list rows.  `items` is the record-index array,
 * `cursor` the highlighted entry, `dst` the row buffer base. */
void shop_item_rows(int count, const uint8_t *items, int cursor, uint8_t *dst,
                    uint8_t half_price);

/* 0x2A0C2 - draw the party rows with a scale-number field. */
void shop_party_bar_rows(int count, uint8_t *dst, int cursor,
                         const uint8_t *members);

/* 0x2A694 - draw the party rows with the shop's equipment labels. */
void shop_party_equip_rows(int count, uint8_t *dst, int cursor,
                           const uint8_t *members, const uint8_t *equip);

#endif /* GAME_SHOP_ROWS_H */
