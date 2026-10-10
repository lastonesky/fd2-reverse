/* scene_core.h - P0 main-state-machine core (original 0x25EBB / 0x117E7 / 0x26152).
 *
 * Reached directly from main (0x25BF4):
 *   v = scenario_menu();      // 0x25EBB, once per outer loop
 *   r = scene_event_tick();   // 0x117E7, once per inner-loop tick
 *   ... (0x26152 = scene_apply_state, next batch)
 *
 * All three are cdecl void-argument functions (the `push imm; call 0x3702F`
 * prologue is the Watcom stack probe, not an argument - same artifact as
 * documented in port/docs/rounds/08 §37.3). Call sites keep the IDA names.
 */
#ifndef FD2_GAME_SCENE_CORE_H
#define FD2_GAME_SCENE_CORE_H

/* 0x25EBB - new-game / load-game selector, returns
 *   0  = restart outer loop (main calls scene_event_tick next),
 *  -1  = restart outer loop (via main's `v == -1` path),
 *  >0  = quit (main tears down).
 * Branches on sub_1F894() (main-menu choice): 0 = new game, 1 = load game,
 * else = save-game path. */
int scenario_menu(void);

/* 0x117E7 - one event tick of the inner loop. Returns
 *   0  = keep ticking, -1 = quit, >0 = back to the outer loop. */
int scene_event_tick(void);

#define sub_25EBB scenario_menu
#define sub_117E7 scene_event_tick

#endif /* FD2_GAME_SCENE_CORE_H */
