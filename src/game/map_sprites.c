/* map_sprites.c - map-view record/unit sprite overlays.  See map_sprites.h.
 *
 * Direct translations of the machine code at 0x1CB94, 0x1CD17 and 0x1CAC7;
 * the offline disassembly (analysis/cseg01_disasm.txt) is authoritative.  Uses the
 * already-verified map_render_view (0x11EEE), rle2_blit_trans (0x4EBAB),
 * sprite24_plain (0x4E22A), sprite24_ramp (0x4DF84), gfx_copy_rows (0x11EB0)
 * and svc_wait_ticks (0x17AA9), the guest heap (0x3706E/0x3776E) and the
 * 24x24 sprite libraries dword_53A61 (solid) / dword_53AD1 (overlay anim).
 */
#include "fd2_game.h"
#include "map_sprites.h"

#include <stdint.h>
#include <string.h>

/* 0x1CB94 */
void map_draw_record_layer(uint8_t *base, int count, const uint8_t *list,
                           int frame)
{
    const int      ox = (int)dword_53AA9;   /* view origin, read as a dword */
    const int      oy = dword_53AAD;
    uint8_t       *view = base + 0x8088u;
    const uint8_t *overlay =
        (const uint8_t *)(uintptr_t)(dword_53AD1 +
            *(const uint32_t *)(uintptr_t)(dword_53AD1 +
                4u * (uint32_t)frame + 6u));
    int i, j;

    map_render_view(view, 456, 13, 8, ox, oy);

    for (i = 0; i < dword_53BEB; ++i) {
        const uint8_t *rec =
            (const uint8_t *)(uintptr_t)dword_53A45 + 80u * (uint32_t)i;
        int x, y, kind, found;

        if ((rec[5] & 1) != 0)
            continue;
        x    = rec[0];
        y    = rec[1];
        kind = rec[2];
        if (!(x >= ox - 1 && x <= ox + dword_51A87 &&
              y >= oy - 1 && y <= oy + dword_51A8B + 1))
            continue;

        found = 0;
        for (j = 0; j < count; ++j)
            if (list[j] == i)
                found = 1;

        {
            uint8_t *dst = view + 24u * (uint32_t)(x - ox)
                + 10944u * (uint32_t)(y - oy) - 2736u;

            if (found) {
                rle2_blit_trans(dst, overlay, 456);
            } else {
                int            idx  = kind * 12 +
                    ((dword_53C0B == 3) ? 2 : dword_53C0B);
                const uint8_t *bank = dword_53A61;

                sprite24_plain(
                    bank + *(const uint32_t *)(uintptr_t)
                               (bank + 4u * (uint32_t)idx), dst, 456);
            }
        }
    }
}

/* 0x1CD17 */
void map_blink_units(int unused, int colour, int count, const uint8_t *list)
{
    const uint8_t *tab = (const uint8_t *)byte_52006; /* 30-byte ramp table */
    const int      ox  = (int)dword_53AA9;
    const int      oy  = dword_53AAD;
    uint8_t       *scratch = (uint8_t *)guest_malloc(0x25680u);
    int i, j;

    (void)unused;

    memmove(scratch, dword_53A49, 0x25680u);

    for (i = 0; i < 10; ++i) {
        memmove(dword_53A49, scratch, 0x25680u);
        for (j = 0; j < count; ++j) {
            const uint8_t *rec =
                (const uint8_t *)(uintptr_t)dword_53A45 +
                80u * (uint32_t)list[j];
            int      x = rec[0], y = rec[1], kind = rec[2];
            uint8_t *dst;
            int      idx;
            const uint8_t *bank;

            if (!(x >= ox - 1 && x <= ox + dword_51A87 &&
                  y >= oy - 1 && y <= oy + dword_51A8B + 1))
                continue;

            dst = dword_53A49 + 0x8088u + 24u * (uint32_t)(x - ox)
                + 10944u * (uint32_t)(y - oy) - 2736u;
            idx  = kind * 12 + ((dword_53C0B == 3) ? 2 : dword_53C0B);
            bank = dword_53A61;
            sprite24_ramp(bank + *(const uint32_t *)(uintptr_t)
                                   (bank + 4u * (uint32_t)idx),
                          dst, 456, tab[colour], 7 - i % 8);
        }
        gfx_copy_rows((void *)(uintptr_t)0xA0504u, 320,
                      dword_53A49 + 32904u, 456, 312, 192);
        svc_wait_ticks(1);
    }

    gfx_copy_rows((void *)(uintptr_t)0xA0504u, 320,
                  scratch + 32904u, 456, 312, 192);
    guest_free(scratch);
}

/* 0x1CAC7 - draw the record layer twice (animation frames 74 and 75, the
 * second into a scratch buffer) then blink the two 312x192 windows on
 * 0xA0504 four times.  The first two arguments are part of the 4-argument
 * ABI but are never read; the tail `jmp 0x14B72` is ev5_14B16's shared
 * epilogue (plain return). */
void map_record_layer_blink(int unused1, int unused2, int count,
                            const uint8_t *list)
{
    uint8_t *scratch;
    int      i;

    (void)unused1;
    (void)unused2;

    map_draw_record_layer(dword_53A49, count, list, 74);

    scratch = (uint8_t *)guest_malloc(0x25680u);
    map_draw_record_layer(scratch, count, list, 75);

    for (i = 0; i < 4; ++i) {
        gfx_copy_rows((void *)(uintptr_t)0xA0504u, 320,
                      dword_53A49 + 0x8088u, 456, 312, 192);
        j___delay(90);
        gfx_copy_rows((void *)(uintptr_t)0xA0504u, 320,
                      scratch + 0x8088u, 456, 312, 192);
        j___delay(90);
    }

    map_view_update(0);
    guest_free(scratch);
}
