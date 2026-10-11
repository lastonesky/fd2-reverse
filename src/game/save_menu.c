/* save_menu.c - the four-slot save/scenario selector (0x29AB2 / 0x29BCB).
 * See save_menu.h for the module overview and the ABI notes.  Direct
 * translation of the machine code; the offline disassembly
 * (analysis/cseg01_disasm.txt) is authoritative.
 */
#include "fd2_game.h"
#include "save_menu.h"

#include <stdint.h>
#include <string.h>

/* 0x29AB2 */
void save_slot_rows(int selected, int dst, int base)
{
    int i;

    for (i = 0; i < 4; ++i) {
        int     v5   = 320 * (19 * i + 119) + dst;
        int     fg   = (i == selected) ? 201 : 205;
        int     slot = 2600 * i + base + 12587;
        uint8_t id   = *(const uint8_t *)(uintptr_t)(slot + 2560);

        dword_53AE1 = i + 1;
        vm_run(dword_53A7D, 549, v5 + 10, 320, fg, 76, 0, 0, 0);
        if (id == 0xFF) {
            vm_run(dword_53A7D, 514, v5 + 88, 320, fg, 76, 0, 0, 0);
        } else {
            vm_run(dword_53A7D, id + 514, v5 + 40, 320, fg, 76, 0, 0, 0);
            vm_run(dword_53A7D, id + 550, v5 + 130, 320, fg, 76, 0, 0, 0);
        }
    }
}

/* 0x29BCB */
int save_slot_select(int base, int wait_keys)
{
    int ret = 0;
    int i;
    int key;

    dword_53C5B = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);
    dword_53C5F = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);
    dword_53C63 = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);

    memmove((void *)(uintptr_t)dword_53C5F, (const void *)(uintptr_t)0xA0000u,
            0xFA00u);
    memmove((void *)(uintptr_t)dword_53C63, (const void *)(uintptr_t)dword_53C5F,
            0xFA00u);

    /* panel sub-image at dword_53F66 + [dword_53F66+70], blitted at +35845. */
    rle2_blit((void *)(uintptr_t)(dword_53C63 + 0x8C05u),
              (const void *)(uintptr_t)
                  (dword_53F66 +
                   *(const uint32_t *)(uintptr_t)(dword_53F66 + 0x46u)),
              320);

    save_slot_rows((int)dword_53C57, (int)dword_53C63, base);

    svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 5, 1);
    for (i = 5; i >= 0; --i)
        msg_blit_band(13 * i + 112, (void *)(uintptr_t)dword_53C5B,
                      (void *)(uintptr_t)dword_53C63);

    while (1) {
        if (wait_keys != 0) {
            dlg_wait_key(0);
        } else {
            byte_53A8E = 0x10;                  /* AH = 10h: BIOS key read */
            int386(0x16, &word_53A8D, &word_53A8D);
            if (byte_53A8E == 0xE0 || byte_53A8E == 0x52)
                byte_53A8E = 0x1C;
            if (byte_53A8E == 0x53)
                byte_53A8E = 0x01;
        }
        key = byte_53A8E;

        if (key == 0x50 && dword_53C57 != 3) {          /* Down */
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 7, 1);
            save_slot_rows((int)++dword_53C57, 0xA0000, base);
        } else if (key == 0x48 && dword_53C57 != 0) {   /* Up   */
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 7, 1);
            save_slot_rows((int)--dword_53C57, 0xA0000, base);
        } else if (key == 0x1C || key == 0x39) {        /* Enter/Space */
            ret = 1;
        } else if (key == 0x01) {                       /* Esc   */
            ret = -1;
        }

        if (ret != 0)
            return ret;
    }
}
