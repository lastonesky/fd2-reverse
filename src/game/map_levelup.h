/* map_levelup.h - the post-battle level-up screen (original 0x1E292) and its
 * gain-bit marker (0x1D79C).
 *
 * Both act on the 80-byte character record `rec` in the table dword_53A45:
 *
 *   0x1E292  map_levelup_run      spend the accumulated experience dword_53EC8
 *            (clamped to 99 by the map tick) in 100-point steps: while it is
 *            at least 100, bump the level byte (+0x21), show the level-up
 *            portrait dialogue pages (msg_show_page on the class table 0x4E821
 *            counters at +0x37..+0x46) and, for the class's gain row (0x4E7F2),
 *            mark each new attribute bit and play its script.  Ends with
 *            ev5_1E5C0(11)/ev4_196CB, stores the leftover into +0x3C and clears
 *            dword_53EC8.  Skips entirely for an empty XP counter, a bit0-set
 *            record (+5) or a maxed level (99 for classes 30/31, 40 otherwise).
 *   0x1D79C  map_levelup_mark_gain  set bit `gain % 8` of the byte at
 *            rec + 26 + gain/8, using the 8-entry bit-mask table at 0x52024.
 *
 * ABI = cdecl.  Hand-restored against the offline disassembly
 * (analysis/cseg01_disasm.txt); no differential harness yet.
 *
 * Note: `sub_1E292` is still called by id from the already-restored
 * scene_core.c's event tick, so the IDA-name alias below is kept (same pattern
 * as map_units.h's sub_1DB65/sub_22253).
 */
#ifndef GAME_MAP_LEVELUP_H
#define GAME_MAP_LEVELUP_H

/* 0x1E292 - run the level-up screen for record `rec`; returns 0 (the machine
 * leaves the last callee's eax and every caller ignores it). */
int map_levelup_run(int rec);

/* 0x1D79C - mark attribute-gain bit `gain` in record `rec`'s flag bitfield. */
void map_levelup_mark_gain(int rec, int gain);

#define sub_1E292 map_levelup_run

#endif /* GAME_MAP_LEVELUP_H */
