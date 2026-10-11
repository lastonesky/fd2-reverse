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
 * "double" effect (item byte 9 == 3) or a 3% roll.  Returns the defender's
 * remaining HP (the last map_attack_resolve result); the machine leaves it in
 * EAX (0x1E856 tail `mov eax,[esp+8]; jmp 0x1317D`). */
int map_attack_bar_drain(int attacker, int defender, int *pos);

/* 0x1548E - run one map attack against the current target dword_53C4B.
 *
 * `actor` glides in toward the marked cell (dword_53C43/dword_53C47, mode
 * `mode`) and faces the target; then, if the battle overlay is enabled
 * (byte_53AF9), it publishes the status frame, drains the target's HP bar
 * (map_attack_bar_drain) and - when the target survives and is
 * facing-adjacent - runs the counter-attack on the actor's own bar.  With the
 * overlay off it falls back to figure_attack_view.  Either way it finishes on
 * the shared tail: clear the portrait, publish the view, animate the units, run
 * the party's +49 scripts (ev_script_run), then the level-up screen.  Returns
 * 1. */
int map_attack_run(int actor, int mode);

/* 0x2F7B6 - the full attack evaluation shared by the figure/combat drivers
 * (caller 0x2EBE1): the same attack/defence roll as map_attack_resolve but it
 * writes a six-dword result block instead of touching the defender's HP.
 * `out[0]` = hit (0 on the RNG miss), `out[1]` = defence halved (critical),
 * `out[2]` = attacker counter/effect flag, `out[4]` = effect type 3 flag,
 * `out[5]` = the damage dealt; the damage is also returned.  Uses the item
 * effect type (item byte 9) and the level table 0x4E84F to book the experience
 * global dword_53EC8. */
int map_attack_result(int attacker, int defender, int *out);

#endif /* GAME_MAP_ATTACK_H */
