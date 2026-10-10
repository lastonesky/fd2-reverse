/* scene_states.h - funcs_25E23[] transition handlers (index-named).
 *
 * Names follow the verified port/ assets (scene_state_00/03/10/12/14/18):
 * `scene_state_XX` = funcs_25E23[XX], the "leave current state" handler that
 * main's dword_53ECC==2 path runs before sub_26152()/funcs_25E3A[state].
 *
 * Common shape (see port/src/game/scene.c for the verified six):
 *   [service calls] -> vm_run(stream, sub, 0xA0000, 320, 205, 76, 74, 19, 1)
 *   -> unit_refresh / unit_add -> advance dword_53C03.
 * Shared tails in the machine code (0x231C6/0x231DF = vm+refresh+advance,
 * 0x231F2 = advance, 0x237C8 = unit_add(14)+advance, 0x239AC/0x239B1,
 * 0x15487/0x1B5EA = foreign epilogues) are spelled out per handler.
 */
#ifndef FD2_GAME_SCENE_STATES_H
#define FD2_GAME_SCENE_STATES_H

void scene_state_02(void);   /* 0x230F2 - funcs_25E23[2]  */
void scene_state_04(void);   /* 0x231F9 - funcs_25E23[4]  */
void scene_state_05(void);   /* 0x23296 - funcs_25E23[5]  */
void scene_state_08(void);   /* 0x235BC - funcs_25E23[8]  */
void scene_state_24(void);   /* 0x24DF2 - funcs_25E23[24] */
void scene_state_27(void);   /* 0x25464 - funcs_25E23[27] */

/* Call sites in not-yet-restored code keep the IDA names. */
#define sub_230F2 scene_state_02
#define sub_231F9 scene_state_04
#define sub_23296 scene_state_05
#define sub_235BC scene_state_08
#define sub_24DF2 scene_state_24
#define sub_25464 scene_state_27

#endif /* FD2_GAME_SCENE_STATES_H */
