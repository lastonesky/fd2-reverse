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

void scene_state_01(void);   /* 0x22F37 - funcs_25E23[1]  */
void scene_state_02(void);   /* 0x230F2 - funcs_25E23[2]  */
void scene_state_04(void);   /* 0x231F9 - funcs_25E23[4]  */
void scene_state_05(void);   /* 0x23296 - funcs_25E23[5]  */
void scene_state_06(void);   /* 0x232E8 - funcs_25E23[6]  */
void scene_state_07(void);   /* 0x234BB - funcs_25E23[7]  */
void scene_state_09(void);   /* 0x235F9 - funcs_25E23[9]  */
void scene_state_11(void);   /* 0x237D5 - funcs_25E23[11] */
void scene_state_13(void);   /* 0x238DC - funcs_25E23[13] */
void scene_state_15(void);   /* 0x23A0A - funcs_25E23[15] */
void scene_state_16(void);   /* 0x23B5F - funcs_25E23[16] */
void scene_state_17(void);   /* 0x23CD5 - funcs_25E23[17] */
void scene_state_19(void);   /* 0x23E74 - funcs_25E23[19] */
void scene_state_20(void);   /* 0x240FA - funcs_25E23[20] */
void scene_state_21(void);   /* 0x244B6 - funcs_25E23[21] */
void scene_state_25(void);   /* 0x24E80 - funcs_25E23[25] */
void scene_state_26(void);   /* 0x250CC - funcs_25E23[26] */
void scene_state_28(void);   /* 0x2548C - funcs_25E23[28] */
void scene_state_29(void);   /* 0x25757 - funcs_25E23[29] */
void scene_state_22(void);   /* 0x24754 - funcs_25E23[22] */
void scene_state_23(void);   /* 0x24C1E - funcs_25E23[23] */
void scene_state_08(void);   /* 0x235BC - funcs_25E23[8]  */
void scene_state_24(void);   /* 0x24DF2 - funcs_25E23[24] */
void scene_state_27(void);   /* 0x25464 - funcs_25E23[27] */

/* Call sites in not-yet-restored code keep the IDA names. */
#define sub_22F37 scene_state_01
#define sub_230F2 scene_state_02
#define sub_231F9 scene_state_04
#define sub_23296 scene_state_05
#define sub_232E8 scene_state_06
#define sub_234BB scene_state_07
#define sub_235F9 scene_state_09
#define sub_237D5 scene_state_11
#define sub_238DC scene_state_13
#define sub_23A0A scene_state_15
#define sub_23B5F scene_state_16
#define sub_23CD5 scene_state_17
#define sub_23E74 scene_state_19
#define sub_240FA scene_state_20
#define sub_244B6 scene_state_21
#define sub_24E80 scene_state_25
#define sub_250CC scene_state_26
#define sub_2548C scene_state_28
#define sub_25757 scene_state_29
#define sub_24754 scene_state_22
#define sub_24C1E scene_state_23
#define sub_235BC scene_state_08
#define sub_24DF2 scene_state_24
#define sub_25464 scene_state_27

#endif /* FD2_GAME_SCENE_STATES_H */
