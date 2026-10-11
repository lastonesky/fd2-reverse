/* map_event.c - map cell "special event" handler (original 0x190AC, 1215 bytes).
 *
 * Direct translation of the machine code at 0x190AC; the offline disassembly
 * (analysis/cseg01_disasm.txt) is authoritative.  See map_event.h for the
 * semantic overview.
 *
 * Notes / decompiler artifacts:
 *   - the Watcom stack probe (`push 0x4C; call 0x3702F`) allocates no frame and
 *     is dropped;
 *   - Hex-Rays reads the cell flag index as `SHIWORD(v6)` (bits 16..31 of the
 *     first dword of the map_cell_info output) which is exactly the uint16 at
 *     out+2, i.e. `cell[6] & 0x1F`; here it is read as two bytes and
 *     sign-extended, matching the machine's `movsx edi, WORD [out+2]`;
 *   - `p` in the pseudo-code is the global pointer dword_53A55 (a 3-byte-per-
 *     flag event table), and dword_53AD5 is the parallel "handled" flag array;
 *     both are raw uint32_t pointer cells in fd2_globals.h and are therefore
 *     dereferenced through (uintptr_t);
 *   - the `funcs_1199C[v5](a1)` indirect call is the scene-handler table; it is
 *     cast to the 1-argument form exactly like ev_script.c/scene_core.c.
 */
#include "fd2_game.h"
#include "map_event.h"

#include <stdint.h>

/* 0x190AC */
int map_cell_event(int rec_index)
{
    uint8_t  info[16];               /* map_cell_info output (~8 bytes used) */
    const uint8_t *rec;              /* dword_53A45 + 80*rec_index           */
    uint8_t *ev;                     /* dword_53A55 + 3*flag                 */
    int      flag;                   /* cell flag value (info[2..3])         */
    uint8_t  tile;                   /* tile-info byte (info[4])             */
    int      r, kind, value;

    map_cell_info(dword_53AB1, dword_53AB5, info);
    flag = (int)(int16_t)(uint16_t)(info[2] | (info[3] << 8));
    tile = info[4];

    if ((tile & 0x60) == 0)
        return 0;
    if (((uint8_t *)(uintptr_t)dword_53AD5)[flag] != 0)
        return 0;

    rec = (const uint8_t *)dword_53A45 + 80u * (uint32_t)rec_index;

    kbd_flush();
    msg_open_portrait(rec[7]);
    vm_run(dword_53A7D, (tile & 0x20) ? 0x1A5 : 0x1AC,VGA_P(0xA9F23u), 320, 205, 76, 74, 19, 1);
    dlg_blit_dato(dword_53A85, dword_53C67, 0);
    r = map_window_run();
    map_draw_text_window();

    if (r != 1 || dword_53C57 != 0) {
        j___delay(100);
        vm_run(dword_53A7D, 0x19C,VGA_P(0xAB6E3u), 320, 205, 76, 74, 19, 1);
        j___delay(200);
        ev4_196CB();
        return 0;
    }

    svc_play_sfx2(dword_53EEC, 12, 1);
    j___delay(300);

    ev    = (uint8_t *)(uintptr_t)(dword_53A55 + 3u * (uint32_t)flag);
    kind  = ev[0];
    value = ev[1] | (ev[2] << 8);

    if (kind == 0) {
        dword_53AD9 = (uint32_t)(value + 181);
        vm_run(dword_53A7D, (tile & 0x20) ? 0x1A6 : 0x1AD,VGA_P(0xAB6E3u), 320, 205, 76, 74, 19, 1);
        if (rec_slot_claim(rec_index, value) != -1) {
            ((uint8_t *)(uintptr_t)dword_53AD5)[flag] = 1;
            dlg_blit_dato(dword_53A85, dword_53C67, 0);
            dlg_wait_key(0);
            ev4_196CB();
            ev4_12263();
            return 0;
        }
        dlg_blit_dato(dword_53A85, dword_53C67, 0);
        dlg_wait_key(0);
        ev4_196CB();
        j___delay(100);
        msg_open_portrait(rec[7]);
        vm_run(dword_53A7D, 0x1A7,VGA_P(0xA9F23u), 320, 205, 76, 74, 19, 1);
        dlg_blit_dato(dword_53A85, dword_53C67, 0);
        r = map_window_run();
        map_draw_text_window();
        if (r != 1 || dword_53C57 != 0) {
            vm_run(dword_53A7D, 0x1A8,VGA_P(0xAB6E3u), 320, 205, 76, 74, 19, 1);
        } else {
            ev4_196CB();
            if (box_popup(rec_index, 0)) {
                int rf = rec_field_byte(rec_index, dword_53C57);
                rec_slot_remove(rec_index, dword_53C57);
                rec_slot_claim(rec_index, value);
                *(uint16_t *)(uintptr_t)(dword_53A55 + 3u * (uint32_t)flag + 0x54)
                    = (uint16_t)rf;
                j___delay(100);
                msg_open_portrait(rec[7]);
                dword_53ADD = r + 181;
                vm_run(dword_53A7D, 0x1A9,VGA_P(0xA9F23u), 320, 205, 76, 74, 19, 1);
                dlg_blit_dato(dword_53A85, dword_53C67, 0);
                dlg_wait_key(0);
                ev4_196CB();
                return 0;
            }
            j___delay(100);
            msg_open_portrait(rec[7]);
            vm_run(dword_53A7D, 0x1A8,VGA_P(0xA9F23u), 320, 205, 76, 74, 19, 1);
        }
        j___delay(200);
        ev4_196CB();
        return 0;
    }

    if (kind == 1) {
        int sub;
        if (value != 0) {
            dword_53AE1 = value;
            sub = (tile & 0x20) ? 0x1AA : 0x1AE;
        } else {
            sub = (tile & 0x20) ? 0x1AB : 0x1AF;
        }
        vm_run(dword_53A7D, sub,VGA_P(0xAB6E3u), 320, 205, 76, 74, 19, 1);
        dlg_blit_dato(dword_53A85, dword_53C67, 0);
        dlg_wait_key(0);
        ev4_196CB();
        dword_53BF3 += (uint32_t)dword_53AE1;
        ((uint8_t *)(uintptr_t)dword_53AD5)[flag] = 1;
        ev4_12263();
        return 0;
    }

    j___delay(200);
    ev4_196CB();
    ((void (*)(int))funcs_1199C[value])(rec_index);
    return 0;
}
