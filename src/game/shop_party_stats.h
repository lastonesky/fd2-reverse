/* shop_party_stats.h - the shop/place "party stat preview" row painter and its
 * stat-totals helper.
 *
 *   0x28632  unit_stat_totals      start from a character record's base four
 *            stats (+37/+39/+3E/+3E, signed 16-bit), add the bonuses (+1/+5/
 *            +3/+7) of one item entry (tbl_602AD) that the caller passes as
 *            `item`, then add every equipped slot's item entry whose type
 *            category (entry[0] <= 0x14 vs > 0x14) differs from the passed
 *            item's category.  The four 32-bit sums go to out[0..3]; nothing
 *            is written back to the record.
 *   0x2825B  shop_party_stat_rows  up to three party-member rows: the unit
 *            sprite (bank dword_53A61, frame 12*rec + dword_53F72 with 3 folded
 *            to 1), the name stream (+8)+1, then four current/prospective stat
 *            pairs.  Each pair draws the record's stored total (word +48/+4A/
 *            +4C/+4E) and the preview total from unit_stat_totals, coloured by
 *            ev5_2860A (0x1F equal / 0x2A lower / 0x77 higher); the label
 *            sub-images 0x4E/0x52/0x56/0x5A/0x5E come from the dword_53F66
 *            resource.  The row matching `cursor` uses the bright colour 201.
 *
 *   0x27F4A  shop_party_stat_run  the interactive screen driver: snapshot VGA
 *            into the three 0xFA00 scratch buffers, blit the panel, draw the
 *            rows, then run a key loop (Down 80 / Up 72, Enter 28 / Space 57
 *            confirm, Esc 1 cancel; returns 1 / -1).
 *
 * Sibling of the shop_rows painters (0x2A0C2 / 0x2A694) and of shop_party_run
 * (0x2A29D); it shares their VGA-snapshot / band-scroll idiom.  Hand-restored
 * from the Hex-Rays decomp and the offline disassembly (analysis/
 * cseg01_disasm.txt); the disassembly is authoritative for the argument order,
 * the stat offsets and the row geometry.  No differential harness yet.
 */
#ifndef GAME_SHOP_PARTY_STATS_H
#define GAME_SHOP_PARTY_STATS_H

#include <stdint.h>

/* 0x28632 - the four effective stats of record `rec_index` with `item`'s
 * bonuses (and every equipped slot in a mismatched type category) applied. */
void unit_stat_totals(int rec_index, int item, int out[4]);

/* 0x2825B - draw the party stat-preview rows.  `count` entries are read from
 * `members` starting at the dword_53F4E scroll window; `item` is the
 * prospective item previewed through unit_stat_totals; `cursor` is the
 * highlighted entry; `dst` is the row buffer base. */
void shop_party_stat_rows(int count, const uint8_t *members, int item,
                          int cursor, uint8_t *dst);

/* 0x27F4A - the preview screen driver.  `count` member indices are read from
 * `members`, `item` is the prospective item previewed through
 * unit_stat_totals.  Returns 1 on confirm, -1 on cancel. */
int shop_party_stat_run(int count, const uint8_t *members, int item);

#endif /* GAME_SHOP_PARTY_STATS_H */
