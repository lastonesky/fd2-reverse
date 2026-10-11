/* map_records.c - map record-list builders.  See map_records.h.
 *
 * Direct translations of the machine code at the stated addresses; the offline
 * disassembly is authoritative for the field offsets, the constants and the
 * signed comparisons.  All callees are already-restored assets: memmove and
 * dlg_portrait_find (0x12C0D, the cell -> record lookup).  The two shared
 * epilogues borrowed from the following sibling functions (0x1B64D / 0x114FF)
 * are plain returns here, and the count is the function's EAX result.
 */
#include "fd2_game.h"
#include "map_records.h"

#include <string.h>

#define REC_STRIDE 80

/* 0x1B6B7 - collect the +49 script entry of every qualifying record. */
int map_collect_scripts(uint8_t *out)
{
    int count = 0;
    int i;

    for (i = 0; i < dword_53BEB; ++i) {
        const uint8_t *rec = (const uint8_t *)dword_53A45
                             + REC_STRIDE * (uint32_t)i;

        if ((rec[5] & 1) == 0 && rec[49] != 0xff
            && *(const uint16_t *)(const void *)(rec + 64) == 0)
            memmove(out + 3 * count++, rec + 49, 3);
    }
    return count;
}

/* 0x149F8 - collect the records on a line of map cells. */
int map_scan_records(int ref_x, int ref_y, uint8_t *out,
                     int start_x, int start_y, int steps, int want_empty)
{
    int32_t  dx = 0, dy = 0;      /* per-cell step, chosen by the axis/direction */
    int32_t  x = start_x, y = start_y;
    uint64_t save = qword_53AB1;  /* the caller's cell cursor {x, y} */
    int count = 0;
    int i;

    if (start_x == ref_x) {
        dy = (start_y <= ref_y) ? 1 : -1;
    } else if (start_x <= ref_x) {
        dx = 1;
    } else {
        dx = -1;
    }

    qword_53AB1 = (uint64_t)(uint32_t)start_x
                  | ((uint64_t)(uint32_t)start_y << 32);

    for (i = 0; i < steps; ++i) {
        int rec_index;

        x += dx;
        y += dy;
        /* the machine stores the two 32-bit halves of qword_53AB1 separately;
         * dlg_portrait_find reads the pair back as one 64-bit cell. */
        qword_53AB1 = (uint64_t)(uint32_t)x | ((uint64_t)(uint32_t)y << 32);

        if (x >= 0 && x < (int32_t)dword_53AC1
            && y >= 0 && y < (int32_t)dword_53AC5) {
            rec_index = dlg_portrait_find();
            if (rec_index != -1) {
                const uint8_t *rec = (const uint8_t *)dword_53A45
                                     + REC_STRIDE * (uint32_t)rec_index;
                if ((want_empty == 0 && rec[6] != 0)
                    || (want_empty != 0 && rec[6] == 0))
                    out[count++] = (uint8_t)rec_index;
            }
        }
    }

    qword_53AB1 = save;
    return count;
}
