/* scene_cutscene.c - scripted full-screen frame animation.  See
 * scene_cutscene.h for the behavioural summary; this is a direct translation
 * of the machine code at 0x24336 (verified instruction-by-instruction against
 * analysis/cseg01_disasm.txt).
 *
 * The resource work goes through the already-restored res_load (0x111BA) /
 * res_blit (0x2EB9F); the DAC work through pal_anim_step (0x4E31C) and
 * pal_fade_add (0x11DF2); ticks through svc_wait_ticks (0x17AA9); the animation
 * through ani_play (0x20421); and the screen/window helper through ev2_135DD
 * (0x135DD) and map_view_update (0x11CAC).
 *
 * Traps / decompiler artifacts:
 *  - The `push 0x28; call 0x3702F` prologue is the Watcom stack probe (dropped).
 *  - `jmp 0x22BB7` at the function tail is a *shared epilogue*
 *    (map_view_update(0) + register restore + ret), not a call; Hex-Rays
 *    renders it as JUMPOUT and drops the refresh.  Spelled out here.
 *  - `malloc`/`memmove`/`free` are the Watcom CRT entries 0x3706E/0x3771C/
 *    0x3776E; the heap is the game heap, so guest_malloc/guest_free are used.
 *  - The loop counter `i` is one register carried across both loops (1..68 and
 *    69..100), so frame 68 is blitted once, not twice.
 *  - `pal_anim_step` (0x4E31C) is called in the first loop only.
 *
 * No differential harness; restored, not dynamically verified.
 */
#include "fd2_game.h"
#include "scene_cutscene.h"

#include <stdint.h>
#include <string.h>

#define VGA_BASE     fd2_vga
#define SCREEN_BYTES 0xFA00u

/* 0x24336 */
void scene_cutscene_play(void)
{
    uint8_t *backdrop;
    void    *image;
    int      i;

    ev2_135DD(14, 8);

    backdrop = (uint8_t *)guest_malloc(SCREEN_BYTES);
    memmove(backdrop, (void *)(uintptr_t)VGA_BASE, SCREEN_BYTES);

    image = res_load(aFdotherDat, NULL, 34);
    res_blit(image, 0, backdrop, 320, -1);

    for (i = 1; i < 69; ++i) {
        memmove(dword_53A49, backdrop, SCREEN_BYTES);
        res_blit(image, i, dword_53A49, 320, -1);
        memmove((void *)(uintptr_t)VGA_BASE, dword_53A49, SCREEN_BYTES);
        pal_anim_step();
        svc_wait_ticks(3);
        kbd_flush();
    }

    ani_play(0, 15, 0);
    pal_fade_add(0, 255, 63);
    j___delay(100);
    pal_fade_add(0, 255, 0);
    j___delay(500);

    for (; i < 101; ++i) {
        memmove(dword_53A49, backdrop, SCREEN_BYTES);
        res_blit(image, i, dword_53A49, 320, -1);
        memmove((void *)(uintptr_t)VGA_BASE, dword_53A49, SCREEN_BYTES);
        svc_wait_ticks(3);
        kbd_flush();
    }

    guest_free(backdrop);
    guest_free(image);

    map_view_update(0);
}
