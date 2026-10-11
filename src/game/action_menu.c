/* action_menu.c - map command/action list panel.  See action_menu.h.
 *
 * Direct translation of the machine code at 0x17AED/0x1CEED/0x1D51D; the
 * offline disassembly (analysis/cseg01_disasm.txt) is authoritative.
 * Decompiler artifacts:
 *   - the Watcom stack probes (`push 0x18/0x5C/0x28; call 0x3702F`) allocate no
 *     frame and are dropped;
 *   - the trailing `jmp 0x15309` (0x1CEED) and `jmp 0x16F04` (0x1D51D) are
 *     shared epilogues owned by other functions, i.e. plain `return`s;
 *   - 0x17AED's tail `push ds:0x53C63; jmp 0x15E94` is `guest_free(dword_53C63)`
 *     followed by the shared `pop ebx; ret`; the machine leaves eax = the
 *     free() result but every caller ignores it, so the routine is void;
 *   - 0x1D51D's `v2 = v1` (an earlier stray eax) is dead; the real count comes
 *     from rec_collect_slot_bits and the key from dlg_wait_key (byte_53A8E).
 */
#include "fd2_game.h"
#include "action_menu.h"

#include <stdint.h>
#include <string.h>

/* action table accessor 0x4E866: base 0x619FD, stride 7 (dseg03). */
static const uint8_t *action_entry(int index)
{
    return (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x19FD], 7, index, 0);
}

/* 0x1CEED */
void action_menu_draw(int rec, int sel, int base)
{
    uint8_t list[32];                  /* machine buffer is 0x20 bytes        */
    int count = rec_collect_slot_bits(rec, list);
    int i;

    for (i = 0; i < count; ++i) {
        int x  = 100 * (i / 4) + 18;
        int fg = (i == sel) ? 201 : 205;
        int y  = 320 * (22 * (i % 4) + 103);
        int y2 = 320 * (22 * (i % 4) + 108);
        const uint8_t *e = action_entry(list[i]);

        vm_run(dword_53A7D, list[i] + 441, x + base + y, 320, fg, 76, 0, 0, 0);

        dlg_frame_tile((void *)(uintptr_t)(y2 + x + base + 50),
                       320, dword_53A81, 92);
        dlg_draw_number((void *)(uintptr_t)(y2 + x + base + 73),
                        320, e[5], 42, 2);
    }
}

/* 0x17AED - open the record's command box and animate its action list. */
void action_menu_open(int rec)
{
    int i, j;

    box_open_anim(rec);
    dlg_wait_key(0);

    if (rec_collect_slot_bits(rec, NULL) != 0) {
        svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 6, 1);

        for (i = 0; i <= 6; ++i) {
            memmove((void *)(uintptr_t)dword_53C5B,
                    (const void *)(uintptr_t)dword_53C5F, 0xFA00u);
            fb_slide_band_a(5, (char *)(uintptr_t)dword_53C5B,
                            (char *)(uintptr_t)dword_53C63);
            fb_slide_band_b(7, (char *)(uintptr_t)dword_53C5B,
                            (char *)(uintptr_t)dword_53C63);
            fb_slide_band_c(16 * i + 94, (char *)(uintptr_t)dword_53C5B,
                            (char *)(uintptr_t)dword_53C63);
            memmove((void *)(uintptr_t)0xA0000u,
                    (const void *)(uintptr_t)dword_53C5B, 0xFA00u);
        }

        memmove((void *)(uintptr_t)dword_53C5B,
                (const void *)(uintptr_t)dword_53C5F, 0xFA00u);
        fb_slide_band_a(5, (char *)(uintptr_t)dword_53C5B,
                        (char *)(uintptr_t)dword_53C63);
        fb_slide_band_b(7, (char *)(uintptr_t)dword_53C5B,
                        (char *)(uintptr_t)dword_53C63);
        memmove((void *)(uintptr_t)0xA0000u,
                (const void *)(uintptr_t)dword_53C5B, 0xFA00u);

        rle2_blit((void *)(uintptr_t)(dword_53C63 + 30085),
                  (const void *)(uintptr_t)
                      (*(const uint32_t *)(const void *)
                           ((uintptr_t)dword_53A81 + 90) +
                       (uintptr_t)dword_53A81), 320);

        action_menu_draw(rec, -1, (int)dword_53C63);
        svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 5, 1);

        for (j = 6; j >= 0; --j) {
            memmove((void *)(uintptr_t)dword_53C5B,
                    (const void *)(uintptr_t)dword_53C5F, 0xFA00u);
            fb_slide_band_a(5, (char *)(uintptr_t)dword_53C5B,
                            (char *)(uintptr_t)dword_53C63);
            fb_slide_band_b(7, (char *)(uintptr_t)dword_53C5B,
                            (char *)(uintptr_t)dword_53C63);
            fb_slide_band_c(16 * j + 94, (char *)(uintptr_t)dword_53C5B,
                            (char *)(uintptr_t)dword_53C63);
            memmove((void *)(uintptr_t)0xA0000u,
                    (const void *)(uintptr_t)dword_53C5B, 0xFA00u);
        }

        dlg_wait_key(0);
    }

    for (i = 0; i <= 11; ++i) {
        if (i == 0 || i == 7)
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 6, 1);
        fb_slide_step(i, (char *)(uintptr_t)dword_53C5B,
                      (char *)(uintptr_t)dword_53C63,
                      (char *)(uintptr_t)dword_53C5F);
    }

    memmove((void *)(uintptr_t)0xA0000u,
            (const void *)(uintptr_t)dword_53C5F, 0xFA00u);

    guest_free((void *)(uintptr_t)dword_53C5B);
    guest_free((void *)(uintptr_t)dword_53C5F);
    guest_free((void *)(uintptr_t)dword_53C63);
}

/* 0x1D51D - one key step of the action menu. */
int action_menu_select(int rec)
{
    const uint8_t *r = (const uint8_t *)dword_53A45 + 80u * (uint32_t)rec;
    uint8_t list[32];
    int count, last, key;

    action_menu_draw(rec, (int)dword_53C57, 655360);

    count = rec_collect_slot_bits(rec, NULL);
    last  = count - 1;
    dlg_wait_key(0);
    key = byte_53A8E;

    switch (key) {
    case 0x48:                                  /* up                        */
        svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
        if (dword_53C57 != 0)
            --dword_53C57;
        else
            dword_53C57 = (uint32_t)last;
        return 0;
    case 0x50:                                  /* down                      */
        svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
        if (last == (int)dword_53C57)
            dword_53C57 = 0;
        else
            ++dword_53C57;
        return 0;
    case 0x4b:                                  /* left                      */
        if ((int)dword_53C57 >= 4) {
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
            dword_53C57 -= 4;
        }
        return 0;
    case 0x4d:                                  /* right                     */
        if (count - 4 > (int)dword_53C57) {
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 0, 1);
            dword_53C57 += 4;
        }
        return 0;
    case 0x1c:                                  /* Enter                     */
    case 0x39:                                  /* Space                     */
        rec_collect_slot_bits(rec, list);
        if (*(const uint16_t *)(const void *)(r + 68)
                < action_entry(list[dword_53C57])[5])
            return 0;
        return 1;
    case 0x01:                                  /* ESC                       */
        return -1;
    default:
        return 0;
    }
}
