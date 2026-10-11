/* scene_cutscene.h - scripted full-screen frame animation (original 0x24336).
 *
 *   0x24336  scene_cutscene_play   the last pending callee of the "leave"
 *            handler scene_state_20 (funcs_25E23[20], 0x240FA).  Plays a
 *            101-frame animation of FDOTHER.DAT entry 34 over the VGA frame
 *            buffer:
 *
 *              1. seed a 0xFA00 scratch buffer with the current screen and
 *                 res_blit sub-image 0 of the resource as the backdrop;
 *              2. frames 1..68: composite each frame over the backdrop and
 *                 copy it to the screen, advancing the DAC animation
 *                 (pal_anim_step) and waiting 3 ticks per frame;
 *              3. ani_play(0, 15, 0), brighten the whole DAC range
 *                 (pal_fade_add(0,255,63)), 100 ms delay, reset the DAC
 *                 (pal_fade_add(0,255,0)), 500 ms delay;
 *              4. frames 69..100: the same per-frame composite as step 2;
 *              5. free both buffers and refresh the map view
 *                 (map_view_update(0)).
 *
 * ABI = cdecl, no arguments, nothing returned.  Uses ev2_135DD (map scroll),
 * res_load/res_blit, pal_anim_step, svc_wait_ticks, kbd_flush, ani_play,
 * pal_fade_add, j___delay, map_view_update and guest_malloc/guest_free.
 * Hand-restored, compile-verified, no differential harness yet.
 */
#ifndef GAME_SCENE_CUTSCENE_H
#define GAME_SCENE_CUTSCENE_H

/* 0x24336 */
void scene_cutscene_play(void);

#endif /* GAME_SCENE_CUTSCENE_H */
