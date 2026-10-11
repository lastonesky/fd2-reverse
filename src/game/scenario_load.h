/* scenario_load.h - load a saved scenario and play its chapter intro
 * (original 0x10010, 1552 bytes; the third scenario_menu branch).
 *
 *   Reads FD2.SAV (0x59CB bytes), de-obfuscates it and compares the trailing
 *   dword against the stored sum (util_deobfuscate / util_sum_tail4); on a
 *   mismatch it shows the "load failed" portrait card.  It then loads the
 *   chapter resources (FDOTHER / FDFIELD / FDSHAP / FDTXT), rebuilds the
 *   FD2.TMP icon atlas from FDICON.B24 (unit_entry_data), republishes the
 *   saved scene state into the fd2_* globals and plays the 9 + 4 step
 *   fade-in intro.
 *
 * Reached from scenario_menu (0x25EBB, choice != 0/1) and the map driver
 * 0x19DF7 (still pending).  Hand-restored from the offline disassembly
 * (analysis/cseg01_disasm.txt, 0x10010..0x1061B); compile-verified and marked
 * 已还原未验证 (no differential harness yet).
 */
#ifndef GAME_SCENARIO_LOAD_H
#define GAME_SCENARIO_LOAD_H

/* 0x10010 */
void scenario_load(void);

#endif /* GAME_SCENARIO_LOAD_H */
