/* ending_demo.h - the closing end-of-demo presentation driver (original 0x31529).
 *
 * 0x31529  ending_demo_run  show the closing presentation.  It decodes the
 *                           FDOTHER.DAT plates (entries 54/60/58/57/59) to the
 *                           VGA frame, runs the palette fades (pal_fade_out /
 *                           in / range / add), plays ANI.DAT animation 2 and a
 *                           40-frame and 200-frame FDOTHER sprite loop, page-
 *                           steps the ending text through msg_show_lines (the
 *                           dword_53C03 == 26 chapter uses a different page
 *                           set), displays the party figure showcase
 *                           (figure_showcase_run) and finally walks the two
 *                           20-entry portrait tables at 0x526E9/0x526FD,
 *                           overriding records 0/1's (+6 kind, +7 id) fields and
 *                           the demo flag dword_54133 before each
 *                           figure_attack_view(0, 1).
 *
 * Called argument-less by the end-of-demo state handlers scene_state_26
 * (0x250CC) and scene_state_29 (0x25757), which then enter a deliberate
 * infinite loop (the original really hangs after the demo).  The ABI is
 * cdecl/void: the machine's `int(void)` prototype is a chkstk artifact and the
 * returned eax is the free() result, which no caller reads.  See ending_demo.c
 * for the instruction-level derivation from analysis/cseg01_disasm.txt.  No
 * differential harness yet (unverified). */
#ifndef GAME_ENDING_DEMO_H
#define GAME_ENDING_DEMO_H

void ending_demo_run(void);

#endif /* GAME_ENDING_DEMO_H */
