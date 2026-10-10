/* ev_script.c - event-script interpreter (original 0x1AA1D, 726 bytes).
 *
 * Called with (record_index, step_count, script_ptr): walks `step_count`
 * 3-byte script entries {op, u16 arg} for the record at record_index:
 *
 *   op 0 : portrait dialog. dword_53AD9 = arg + 181; when rec_slot_claim()
 *          returns -1 it opens the portrait, waits a key, then runs the map
 *          window (0x19953) + map_draw_text_window (0x197E5); if the window
 *          returned 1 and dword_53C57 == 0 it tries rec_field_byte (0x1B722) /
 *          rec_slot_remove (0x1B8E7) and *continues* the loop, else it re-opens
 *          the portrait and delays 200 ms.
 *   op 1 : portrait of rec[7], dword_53AE1 = arg, fixed vm(435) sequence with
 *          blit + wait-key + close, then dword_53BF3 += dword_53AE1.
 *   op 2 : j___delay(200) then funcs_1199C[arg](record_index) - the 1-arg
 *          handler table whose 90 entries are all source-ized.
 *   op 3 : vm_run with sub = arg.
 * A failing guard (rec[6] != 2) or exhausted steps ends the routine; the
 * shared epilogue 0x22BBE is a plain return.
 *
 * Note on 0x16559 (dlg_blit_dato): the machine-code ABI is ONE arg (the DATO
 * sub-image index; the buffer global dword_53A85 and box position dword_53C67
 * are read inside). The migrated port/ function takes those globals as explicit
 * parameters (harness-friendly, behaviour-identical), so new call sites pass
 * all three - PROGRESS 0-J.
 */
#include "fd2_game.h"
#include "ev_script.h"

#include <stdint.h>

/* 0x1AA1D */
void ev_script_run(int rec_index, int steps, int script)
{
    uint8_t *rec = (uint8_t *)(uintptr_t)dword_53A45
                 + 80u * (uint32_t)rec_index;
    int i;

    if (steps == 0)
        return;

    for (i = 0; i < steps; ++i) {
        const uint8_t *ent = (const uint8_t *)(uintptr_t)script + 3u * (uint32_t)i;
        int op  = ent[0];
        int arg = (int)*(const uint16_t *)(const void *)(ent + 1);

        sub_4E381();                                /* flush key buffer */
        if (op != 0) {
            switch (op) {
            case 1:
                if (rec[6] != 2)
                    return;
                sub_1956B(rec[7]);                  /* msg_open_portrait */
                dword_53AE1 = (uint32_t)arg;
                sub_15F84((int)dword_53A7D, 435, 0xA9F23, 320, 205, 76, 74, 19, 1);
                sub_16559((int)(uintptr_t)dword_53A85, dword_53C67, 0);
                sub_16C57(0);                       /* dlg_wait_key */
                sub_196CB();                        /* ev4_196CB */
                dword_53BF3 += dword_53AE1;
                break;
            case 2:
                j___delay(200);
                ((void (*)(int))funcs_1199C[arg])(rec_index);
                break;
            case 3:
                sub_15F84((int)dword_53A79, arg, 0xA0000, 320, 205, 76, 74, 19, 1);
                break;
            default:
                break;
            }
        } else {
            int v9;

            if (rec[6] != 2)
                return;
            dword_53AD9 = (uint32_t)(arg + 181);
            sub_1956B(rec[7]);
            sub_15F84((int)dword_53A7D, 432, 0xA9F23, 320, 205, 76, 74, 19, 1);
            if (sub_1BB8C(rec_index, arg) == -1) {
                sub_16559((int)(uintptr_t)dword_53A85, dword_53C67, 0);
                sub_16C57(0);
                sub_196CB();
                j___delay(100);
                sub_1956B(rec[7]);
                sub_15F84((int)dword_53A7D, 433, 0xA9F23, 320, 205, 76, 74, 19, 1);
                sub_16559((int)(uintptr_t)dword_53A85, dword_53C67, 0);
                v9 = sub_19953();
                sub_197E5();
                if (v9 != 1 || dword_53C57 != 0) {
                    sub_15F84((int)dword_53A7D, 434, 0xAB6E3, 320, 205, 76, 74, 19, 1);
                } else {
                    sub_196CB();
                    if (sub_1B932(rec_index, 0)) {
                        sub_1B722(rec_index, (int)dword_53C57);
                        sub_1B8E7(rec_index, (int)dword_53C57);
                        sub_1BB8C(rec_index, arg);
                        continue;
                    }
                    j___delay(100);
                    sub_1956B(rec[7]);
                    sub_15F84((int)dword_53A7D, 434, 0xA9F23, 320, 205, 76, 74, 19, 1);
                }
                j___delay(200);
            } else {
                sub_16559((int)(uintptr_t)dword_53A85, dword_53C67, 0);
                sub_16C57(0);
            }
            sub_196CB();
        }
    }
}
