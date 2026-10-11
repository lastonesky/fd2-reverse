/* scene_enter.h - funcs_25E3A[] "enter state" render handlers (index-named).
 *
 * main's dword_53ECC==2 path runs funcs_25E23[state] (leave, scene_states.c /
 * port scene.c) and then funcs_25E3A[state] (enter/render). Slots 5..25 are the
 * verified menu_* assets; this header covers the remaining slots.
 * All of them start with sub_205DA() = menu_reload_world and end either inline
 * or through the shared tails 0x3312D (vm_run + 0x134E4 + 0x12D7B(0)) and
 * 0x33594 (0x12D7B(0)), spelled out in C.
 */
#ifndef FD2_GAME_SCENE_ENTER_H
#define FD2_GAME_SCENE_ENTER_H

void scene_enter_00(void);   /* 0x3231B - funcs_25E3A[0]  */
void scene_enter_01(void);   /* 0x32D18 - funcs_25E3A[1]  */
void scene_enter_02(void);   /* 0x32E8C - funcs_25E3A[2]  */
void scene_enter_03(void);   /* 0x32FB2 - funcs_25E3A[3]  */
void scene_enter_04(void);   /* 0x33049 - funcs_25E3A[4]  */
void scene_enter_22(void);   /* 0x336A0 - funcs_25E3A[22] */
void scene_enter_26(void);   /* 0x33AF1 - funcs_25E3A[26] */
void scene_enter_27(void);   /* 0x33C9D - funcs_25E3A[27] */
void scene_enter_28(void);   /* 0x33DBA - funcs_25E3A[28] */

#define sub_3231B scene_enter_00
#define sub_32D18 scene_enter_01
#define sub_32E8C scene_enter_02
#define sub_32FB2 scene_enter_03
#define sub_33049 scene_enter_04
#define sub_336A0 scene_enter_22
#define sub_33AF1 scene_enter_26
#define sub_33C9D scene_enter_27
#define sub_33DBA scene_enter_28
#define sub_33E3C scene_enter_29

#endif /* FD2_GAME_SCENE_ENTER_H */
