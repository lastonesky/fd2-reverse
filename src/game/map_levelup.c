/* map_levelup.c - the post-battle level-up screen (original 0x1E292 + 0x1D79C).
 *
 * Direct translation of the machine code at 0x1E292/0x1D79C; the offline
 * disassembly (analysis/cseg01_disasm.txt) is authoritative.
 *
 *   - the Watcom stack probe (`push size; call 0x3702F`) is dropped and the
 *     tail `jmp 0x1317D` is the shared epilogue of other functions (a plain
 *     return);
 *   - the class table accessor 0x4E821 (base 0x620A1, stride 11) and the gain
 *     table accessor 0x4E7F2 (base 0x626B3, stride 12) become tbl_ptr on
 *     fd2_data2 (dseg03);
 *   - the gain-bit mask table lives at 0x52024 in fd2_data1 (dseg02) and holds
 *     the eight byte masks {01,02,04,08,10,20,40,80};
 *   - dword_53EC8 is the experience counter folded into the record here, and
 *     dword_53AE1 is the "gained experience" figure the portrait page reads.
 */
#include "fd2_game.h"
#include "map_levelup.h"

#include <stdint.h>

/* 0x1D79C - set attribute-gain bit `gain` of record `rec`. */
void map_levelup_mark_gain(int rec, int gain)
{
    uint8_t *r = (uint8_t *)(uintptr_t)dword_53A45 + 80u * (uint32_t)rec;
    const uint8_t *mask = (const uint8_t *)&fd2_data1[0x2024];

    r[26 + gain / 8] |= mask[gain % 8];
}

/* 0x1E292 - the level-up screen for record `rec`. */
int map_levelup_run(int rec)
{
    uint8_t *r = (uint8_t *)(uintptr_t)dword_53A45 + 80u * (uint32_t)rec;
    uint32_t cls = r[7];
    const uint8_t *tbl;
    int page = 2;
    int xp;

    if (dword_53EC8 == 0 || (r[5] & 1u) != 0)
        return 0;
    if ((cls == 30u || cls == 31u) ? (r[33] == 99u) : (r[33] == 40u))
        return 0;

    /* class table 0x4E821: base 0x620A1, stride 11 (dseg03) */
    tbl = (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x20A1], 11, (int)cls, 0);
    xp  = (int)r[60] + (int)dword_53EC8;
    dword_53AE1 = (int32_t)dword_53EC8;

    kbd_flush();
    msg_open_portrait((int)cls);
    vm_run(dword_53A7D, 488, 693535, 320, 205, 76, 74, 19, 1);
    dlg_blit_dato(dword_53A85, dword_53C67, 0);

    while (xp >= 100) {
        int i;

        kbd_flush();
        ++r[33];
        vm_run(dword_53A7D, 489, 699615, 320, 205, 76, 74, 19, 1);

        page = msg_show_page((uint16_t *)(void *)(r + 0x37), tbl, 490, page);
        page = msg_show_page((uint16_t *)(void *)(r + 0x39), tbl + 2, 491, page);
        page = msg_show_page((uint16_t *)(void *)(r + 0x3e), tbl + 4, 492, page);
        page = msg_show_page((uint16_t *)(void *)(r + 0x42), tbl + 6, 493, page);
        page = msg_show_page((uint16_t *)(void *)(r + 0x46), tbl + 8, 494, page);

        if (tbl[10] != 0xFFu) {
            /* gain table 0x4E7F2: base 0x626B3, stride 12 (dseg03) */
            const uint8_t *g = (const uint8_t *)tbl_ptr(
                (void *)&fd2_data2[0x26B3], 12, tbl[10], 0);

            for (i = 0; i < 6; ++i) {
                if (r[33] == g[2 * i]) {
                    int gain = g[2 * i + 1];

                    dword_53AD9 = (uint32_t)(gain + 441);
                    map_levelup_mark_gain(rec, gain);
                    vm_run(dword_53A7D, 587, 6080 * page + 693535,
                           320, 205, 76, 74, 19, 1);
                }
            }
        }

        unit_metrics(rec);

        {
            uint8_t lvl = r[33];

            xp -= 100;
            if (((cls == 30u || cls == 31u) && lvl == 99u) || lvl == 30u)
                xp = 0;
        }
    }

    ev5_1E5C0(11);
    ev4_196CB();
    r[60] = (uint8_t)xp;
    dword_53EC8 = 0;

    return 0;
}
