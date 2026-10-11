/* figure_showcase.h - the intro party-figure showcase (original 0x31C49).
 *
 * 0x31C49  figure_showcase_run  fade the screen in, then walk the party table
 *                               (dword_53BFB records, backwards from the last
 *                               to 0) showing each member's FIGANI figure over
 *                               its record's background: slide the plate in
 *                               (figure_attack_scroll), step the base group 20
 *                               frames, play every sub-sprite of the +1 group,
 *                               then stamp the DATO dialogue box
 *                               (dlg_box_stage) under five vm_run text lines
 *                               and a blinking cursor while polling the
 *                               keyboard for up to 220 frames (440 for the
 *                               first member); each member ends with
 *                               pal_fade_out.
 *
 * Called once, argument-less, from the intro driver sub_31529 (0x31529).  The
 * ABI is cdecl/void: the machine's `int(void)` prototype is a chkstk artifact
 * and the `jmp 0x2E2A8` exit is a shared epilogue (plain return).  See
 * figure_showcase.c for the instruction-level derivation.  No differential
 * harness yet (unverified). */
#ifndef GAME_FIGURE_SHOWCASE_H
#define GAME_FIGURE_SHOWCASE_H

void figure_showcase_run(void);

#endif /* GAME_FIGURE_SHOWCASE_H */
