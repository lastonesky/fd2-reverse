/* place_screen.c - the place menu's in-game option screen and its member action
 * list.  See place_screen.h for the overview, the ABI and the shared buffers.
 *
 * Direct translation of the machine code at 0x29300 / 0x29620; the offline
 * disassembly (analysis/cseg01_disasm.txt) is authoritative.
 */
#include "fd2_game.h"
#include "place_screen.h"

#include <stdint.h>
#include <string.h>

/* 0x29620 */
void place_member_actions_run(void)
{
    int ret;
    int saved_box;

    dword_53F5E = dword_53BFB;
    for (;;) {
        ret = menu_list_select();
        msg_close_portrait();
        saved_box = dword_53C67;                 /* 0x53C67 survives 0x17AED */
        if (ret == -1)
            return;
        action_menu_open((int)dword_53C57);
        dword_53C67 = saved_box;
        dword_53A85 = res_load(aDatoDat, dword_53A85, byte_52387[0]);
    }
}

/* 0x29300 */
int place_screen_run(void *src)
{
    const uint8_t *ent;
    uint8_t        v5;
    int            v1 = 0;
    int            v6;
    int            v2;
    int            i;

    /* 0x4E809 = table accessor: base 0x6238D, stride 31, offset -31 (tables.h). */
    ent = (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x238D], 31,
                                   (int)dword_53C03, -31);
    v5 = ent[0];

    dword_53F66 = (uint32_t)(uintptr_t)res_load(aFdotherDat,
                    (void *)(uintptr_t)dword_53F66, 13);
    res_blit6((void *)(uintptr_t)0xA0000u, 320,
              (void *)(uintptr_t)dword_53F66, 0);
    pal_fade_in();
    j___delay(200);
    msg_open_portrait(byte_52387[0]);
    vm_run(dword_53A7D, 585, 0xA94CC, 320, 205, 76, 74, 19, 1);
    dlg_blit_dato(dword_53A85, dword_53C67, 0);

    for (;;) {
        dword_53C57 = (uint32_t)v1;
        screen_curtain(0);
        v6 = menu_choice4();
        if (v6 == 1)
            v1 = (int)dword_53C57;               /* keep the chosen slot */
        screen_curtain(1);
        msg_close_portrait();
        if (v6 != 1)
            break;

        if (dword_53C57 == 0) {
            place_member_actions_run();
        } else if (dword_53C57 == 1) {
            save_slot_write(1);
        } else if (dword_53C57 == 2) {
            save_slot_load();
        } else {
            msg_open_portrait(byte_52387[0]);
            vm_run(dword_53A7D, 415, 0xA94CC, 320, 205, 76, 74, 19, 1);
            dlg_blit_dato(dword_53A85, dword_53C67, 0);
            v2 = map_window_run();
            map_draw_text_window();
            if (v2 != -1 && dword_53C57 == 0) {
                vm_run(dword_53A7D, 416, 0xAAc8C, 320, 205, 76, 74, 19, 1);
                j___delay(200);
                msg_close_portrait();
                return 1;
            }
            msg_close_portrait();
        }

        msg_open_portrait(byte_52387[0]);
        vm_run(dword_53A7D, 586, 0xA94CC, 320, 205, 76, 74, 19, 1);
        dlg_blit_dato(dword_53A85, dword_53C67, 0);
    }

    res_blit6((void *)(uintptr_t)0xA0000u, 320,
              (void *)(uintptr_t)dword_53F66, 0);
    j___delay(200);
    pal_fade_out();

    for (i = 10; i >= 0; --i) {
        /* 0x29300 zoom-out: the machine's shl eax,7 is *128 here (the signed
           idiv quotient can be negative; both give the same 32-bit result). */
        place_zoom_blit(
            ((i * ((int)byte_52363[6 * v5 + dword_53F4A] - 150) / 10) * 128)
                + 20480,
            ((i * ((int)byte_52375[6 * v5 + dword_53F4A] - 100) / 10) * 128)
                + 12800,
            (const char *)src, 128 - 9 * i);
        memmove((void *)(uintptr_t)0xA0000u, dword_53A49, 0xFA00u);
        pal_fade_range(0, 255, 4 * i);
    }

    guest_free((void *)(uintptr_t)dword_53F66);
    dword_53F66 = 0;
    return 0;
}
