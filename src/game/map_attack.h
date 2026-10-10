/* map_attack.h - the unit attack result helpers (original 0x1E98C / 0x1ECC7)
 * and the HP-bar drain driver 0x1E856.
 *
 * All three operate on the 80-byte character table dword_53A45.  `attacker`
 * selects the acting record (whose held item 0x4E8BC entry picks the effect),
 * `defender` the target record whose HP word (+0x40) is reduced:
 *
 *   0x1E98C  map_attack_sprite   play the attack's frame script - the SFX and
 *            the 24x24 target sprite come from the item's animation table
 *            entry (0x4E87D), each frame is a res_draw_subimage of the shape
 *            bank dword_53AD1 over 0xA0000 saved and restored via 0x15E71.
 *   0x1ECC7  map_attack_resolve  roll the hit, apply the flash sequences
 *            (ev5_203BD / pal_fade_add), compute the damage from the two
 *            records' attack/defence words and write it back to the defender's
 *            HP; returns the remaining HP.  Also updates the experience global
 *            dword_53EC8.
 *
 * ABI = cdecl.  Hand-restored against the offline disassembly
 * (analysis/cseg01_disasm.txt); no differential harness yet.
 */
#ifndef GAME_MAP_ATTACK_H
#define GAME_MAP_ATTACK_H

/* 0x1E98C - draw the attacker's attack animation on the defender's cell. */
void map_attack_sprite(int attacker, int defender);

/* 0x1ECC7 - resolve one attack (RNG hit roll + damage) and return the
 * defender's remaining HP word. */
int map_attack_resolve(int attacker, int defender);

/* 0x1E856 - animate the defender's HP bar draining from the pre-attack value
 * to the remaining HP returned by map_attack_resolve.  `pos` is the two-int
 * screen block {x, y} of the bar's top-left cell (from map_status_*); the bar
 * is drawn one ui_meter_fixed segment per tick.  Runs once, or twice for a
 * "double" effect (item byte 9 == 3) or a 3% roll. */
void map_attack_bar_drain(int attacker, int defender, int *pos);

#endif /* GAME_MAP_ATTACK_H */
