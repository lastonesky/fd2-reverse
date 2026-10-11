/* map_record_engage.c - approach-and-run the selected map record.
 * See map_record_engage.h for the semantic overview.
 *
 * Direct translation of the machine code at 0x18890 (756 bytes) and 0x18B84
 * (233 bytes); the offline disassembly (analysis/cseg01_disasm.txt, lines
 * 10278..10560) is authoritative.
 *
 * Notes / decompiler artifacts:
 *   - the Watcom stack probe (`push 0x68; call 0x3702F` in 0x18890 and
 *     `push 0x2C; call 0x3702F` in 0x18B84) allocates no frame and is dropped;
 *   - `0x18B84` shares the epilogue at 0x18B7F (`pop ebp/edi/esi/ebx; ret`)
 *     which lives inside 0x18890's body; its tail `jmp 0x18B7F` is therefore a
 *     plain `return`, not a call - this is the false `sub_18890` callee the map
 *     records.  The shared epilogue also means 0x18B84 returns nothing: it is
 *     `void` (Hex-Rays' `int` + `JUMPOUT(0x18B7F)` are artifacts);
 *   - the 16-byte local `dirbuf` is a `rep movs` copy of the 4-dword patch tile
 *     array at 0x53F12 (dseg02); Hex-Rays labels it `v12 = unk_53F12` and sets
 *     `dirbuf[1] = 1` via `DWORD1(v12)`;
 *   - the record table accessor 0x4E8A5 is base 0x61646, stride 20 (dseg03);
 *   - `qword_53AB1` is the live map cursor target: `low` = target_x, `high` =
 *     target_y (dword_53AB5 is its high dword).  Both path_mark (0x4E390) and
 *     path_find (0x4E4F6) take it split into x/y;
 *   - when path_find returns 255 (no route) 0x18890 returns 1 *without* freeing
 *     the 0x3706E buffer - the original leaks it; reproduced verbatim;
 *   - `map_slide_view` is 0x12CEA, `map_scroll_path` 0x13488, `map_scroll_cell_tag`
 *     0x13A44, `unit_mark_nearby` 0x145CD, `map_exclude_record_cells` 0x146D1,
 *     `rec_skip` 0x1F183, `rec_flag_or80` 0x13512, `util_fix_records` 0x4DF4C;
 *   - 0x18B84 reads a never-written `esi` as the pre-redraw BIOS-tick baseline
 *     (uninitialised register local); it is modelled as 0, so the first poll
 *     draws immediately, exactly like the established tick-baseline precedents.
 */
#include "fd2_game.h"
#include "map_record_engage.h"

#include <stdint.h>
#include <string.h>

/* runtime-mapped BDA mirror (the machine's absolute 0x46C immediate). */
extern uint32_t dos_lowmem_base;
#define BDA_W(off) (*(volatile uint16_t *)(uintptr_t)(dos_lowmem_base + (off)))

/* live map cursor target, read fresh at each use exactly as the machine does. */
#define MAP_X ((int)(uint32_t)qword_53AB1)
#define MAP_Y ((int)(uint32_t)(qword_53AB1 >> 32))

/* movement-cost table accessor 0x4E8A5: base 0x61646, stride 20 (dseg03). */
static const void *cost_row_for(int profile)
{
    return tbl_ptr((void *)&fd2_data2[0x1646], 20, profile, 0);
}

/* 0x18B84 - pump the map animation until a key is pending. */
void map_wait_anim(int rec)
{
    int width = (dword_53AB9 >= 7) ? 2285 : 2436;
    int baseline = 0;                       /* uninitialised esi baseline */

    while (!kbd_pending()) {
        pal_anim_step();
        if (baseline != (int)(int16_t)BDA_W(0x46C)) {
            pal_anim_step();
            anim_frame_step();
            map_render_view(dword_53A49 + 32904, 456, 13, 8,
                            (int)(uint32_t)qword_53AA9,
                            (int)(uint32_t)(qword_53AA9 >> 32));
            map_reveal_cursor();
            dlg_portraits_refresh();
            ui_status_panel((char *)dword_53A49 + width + 32904, 456, rec);
            map_draw_cursor(dword_53A49 + 32904, 456);
            gfx_copy_rows((void *)(uintptr_t)VGA_P(0xA0504u), 320,
                          dword_53A49 + 32904, 456, 0x138, 192);
            baseline = (int)(int16_t)BDA_W(0x46C);
        }
    }
}

/* 0x18890 - approach the selected map record and run its on-map action. */
int map_record_engage(int rec_idx)
{
    const uint8_t *rec = (const uint8_t *)dword_53A45
                       + REC_STRIDE * (uint32_t)rec_idx;
    int      dirbuf[4];             /* copy of the 0x53F12 patch tile array */
    unsigned int size;
    int      profile;
    const void *cost_row;
    void    *p;
    int      low, high;             /* cursor target split (qword_53AB1) */
    int      picked;
    int      n;

    memcpy(dirbuf, &fd2_data1[0x3F12], sizeof dirbuf);

    dword_53C53 = 0;
    dword_51A8F = 255;

    size    = rec[59];
    profile = rec[32];
    if (rec_skip(rec_idx))
        profile = 19;
    else if (rec[7] == 28)
        profile = 16;

    cost_row = cost_row_for(profile);
    p        = guest_malloc(size);
    unit_mark_nearby(1);
    path_mark(cost_row, MAP_X, MAP_Y, (int)size, (void *)dword_53A51,
              (const void *)(uintptr_t)dword_53A69);
    map_exclude_record_cells(rec_idx, 1);

    low  = MAP_X;
    high = MAP_Y;
    map_wait_anim(rec_idx);
    picked = map_pick_record(4, 0, 0);
    util_fix_records(dword_53A51);

    if (picked != -1) {
        unit_mark_nearby(1);
        n = path_find(cost_row, low, high, (int)size, p, MAP_X, MAP_Y, 0,
                      (void *)dword_53A51,
                      (const void *)(uintptr_t)dword_53A69);
        util_fix_records(dword_53A51);
        dword_51A83 = 0;
        map_slide_view(low, high);
        dword_51A83 = 1;

        if (n == 0 || n == 255) {
            int r;

            if (n != 0)
                return 1;               /* no route: original leaks p here */
            do
                r = map_record_interact(rec_idx, dirbuf, 0);
            while (r == 0);
            if (r != -1) {
                map_scroll_cell_tag(rec[0], rec[1], 1);
                guest_free(p);
                return 1;
            }
            guest_free(p);
            if (dword_53C53 == 0)
                return (int)dword_53C53;
        } else {
            int r;

            map_scroll_path(rec_idx, (const uint8_t *)p, n);
            dlg_portrait_clear();
            if (rec[7] != 18 && rec[7] != 19 && rec[7] != 34)
                dirbuf[1] = 1;
            do
                r = map_record_interact(rec_idx, dirbuf, 1);
            while (r == 0);
            if (r != -1) {
                map_scroll_cell_tag(rec[0], rec[1], 1);
                guest_free(p);
                return r;
            }
            guest_free(p);
            if (dword_53C53 == 0) {
                uint8_t *wr = (uint8_t *)dword_53A45
                            + REC_STRIDE * (uint32_t)rec_idx;

                wr[0] = (uint8_t)low;
                wr[1] = (uint8_t)high;
                map_slide_view(low, high);
                return (int)dword_53C53;
            }
        }
        rec_flag_or80(rec_idx);
        map_scroll_cell_tag(rec[0], rec[1], 1);
        return (int)dword_53C53;
    }

    map_slide_view(low, high);
    guest_free(p);
    return 1;
}
