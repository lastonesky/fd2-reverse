/* action_menu.c - map command/action list panel.  See action_menu.h.
 *
 * Direct translation of the machine code at 0x1CEED; the offline disassembly
 * (analysis/cseg01_disasm.txt) is authoritative.  Decompiler artifacts:
 *   - the Watcom stack probe (`push 0x5C; call 0x3702F`) allocates no frame and
 *     is dropped;
 *   - the trailing `jmp 0x15309` is a shared epilogue owned by another function
 *     (`add esp,24h; pop ebp/edi/esi/ebx; ret`), i.e. a plain `return`.
 */
#include "fd2_game.h"
#include "action_menu.h"

#include <stdint.h>

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
