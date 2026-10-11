/* place_screen.h - the place menu's in-game option screen and its member
 * action list (original 0x29300 / 0x29620).
 *
 * dword_53F4A selects the current place; the place menu driver 0x2670E
 * dispatches to one screen per place type and hands it a private 0xFA00 copy of
 * the VGA frame (the zoom-out source).  0x29300 is the place-type-0 screen:
 *
 *   0x29300  place_screen_run   load FDOTHER entry 13, stage it into the frame
 *            and show the screen, then loop over the four-slot selector
 *            menu_choice4 (0x26E38), each step bracketed by the screen_curtain
 *            wipe (0x26CE4):
 *              - slot 0 opens the member action list (place_member_actions_run,
 *                0x29620);
 *              - slot 1 saves (save_slot_write, wait_keys = 1);
 *              - slot 2 loads (save_slot_load);
 *              - any other slot runs the text-window dialogue (map_window_run,
 *                0x19953) and, if accepted, returns 1 after a confirmation
 *                line + 200 ms delay.
 *            Every other step closes with the 586 word stream (0x249/0x24A
 *            bracket the screen).  On cancel it reblits FDOTHER 13, fades out
 *            and plays the same zoom-out the place menu ends with (11 steps of
 *            place_zoom_blit, 0x2921A), then frees the resource.
 *
 *   0x29620  place_member_actions_run   publish the party count into
 *            dword_53F5E and loop: pick a member with menu_list_select
 *            (0x27D33); on cancel (-1) return; otherwise open the member's box
 *            via action_menu_open (0x17AED), restore dword_53C67 and reload the
 *            DATO.DAT sub-image with res_load.  No differential harness yet.
 *
 * ABI = cdecl.  0x29300 takes the saved-frame pointer (`src`) and returns 1 when
 * the text-window option was accepted, else 0; 0x29620's EAX leftover is unused
 * and it is void.  The Watcom stack probe (`push size; call 0x3702F`) is
 * dropped: 0x3702F restores ESP itself, so the arguments sit where the raw
 * `[esp+N]` reads say.  The `<<7` of the machine is written as `*128` (the
 * quotient may be negative; both give the same 32-bit result).  dword_53F66 is
 * a guest-heap buffer from res_load, freed with guest_free.  Hand-restored from
 * the Hex-Rays decomp and the offline disassembly
 * (analysis/cseg01_disasm.txt), which is authoritative.
 */
#ifndef GAME_PLACE_SCREEN_H
#define GAME_PLACE_SCREEN_H

/* 0x29300 - run the place-type-0 option screen; `src` is the caller's saved
 * 0xFA00 frame used as the zoom-out source.  Returns 1 when the text-window
 * option was accepted, else 0. */
int place_screen_run(void *src);

/* 0x29620 - loop the party-member selector and open each chosen member's action
 * box (reloading DATO.DAT afterwards); returns on cancel. */
void place_member_actions_run(void);

#endif /* GAME_PLACE_SCREEN_H */
