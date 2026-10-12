/* map_units.c - animate the unit sprites inside the map view (original 0x1DB65).
 *
 * Collects up to 30 records that are visible in the current view window
 * (record byte +5 bit0 clear, sprite word at +64 zero, coordinates inside
 * [ox-1 .. ox+w+?] x [oy-1 .. oy+h+1] where (ox,oy) is the view origin in
 * qword_53AA9 and (dword_51A87, dword_51A8B) the view size) and remembers the
 * destination cell address of each:
 *     dst = dword_53A49 + 10944*(y-1-oy) + 24*(x-1-ox) + 30168
 * (10944 = 456*24, the view buffer is at dword_53A49 + 32904).
 *
 * Then, if any were found it plays:
 *   13 frames: map_render_view + per-record dlg_portrait_draw (+kind byte j%4
 *              for spriteless records) + map_refresh_records + gfx_copy_rows
 *              + svc_wait_ticks(1);
 *   then marks those records' byte +5 = 1 and, on a 0x25680 scratch buffer
 *   (the same size the task buffer in main uses), plays 6 frames with atlas
 *   sub-images 68..73 (0x1DB65's caller passes FDOTHER.DAT entry 5 as
 *   dword_53A81) blitted into each remembered cell, then restores the scratch
 *   from a saved copy for 6 more frames (74..79) and frees it.
 * With no visible record it just marks them (+5 = 1) and returns.
 *
 * The `&loc_2567F + 1` and `malloc(&loc_2567F + 1)` renderings are the
 * size-immediate trap: both are 0x25680 (documented in PROGRESS §1.5).
 */
#include "fd2_game.h"
#include "map_units.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define VIEW_SRC_OFF  32904u      /* dword_53A49 + 0x8088 */
#define CELL_BASE     30168u      /* dword_53A49 + 0x75D8 */
#define CELL_STRIDE   10944u      /* 456 * 24 */
#define SCRATCH_SIZE  0x25680u

/* 0x1DB65 */
void map_units_animate(void)
{
    uint8_t *buf   = (uint8_t *)(uintptr_t)dword_53A49;
    uint8_t *atlas = (uint8_t *)(uintptr_t)dword_53A81;
    uint8_t *v19[30];
    uint8_t *scratch;
    int ox = (int)qword_53AA9;
    int oy = (int)(qword_53AA9 >> 32);
    int found = 0;
    int i, j, n;

    for (i = 0; i < (int)dword_53BEB; ++i) {
        uint8_t *rec = (uint8_t *)(uintptr_t)dword_53A45 + 80u * (uint32_t)i;
        int x = rec[0];
        int y = rec[1];

        if ((rec[5] & 1) == 0
            && *(const uint16_t *)(const void *)(rec + 64) == 0
            && x >= ox - 1 && x <= (int)dword_51A87 + ox
            && y >= oy - 1 && y <= (int)dword_51A8B + oy + 1) {
            v19[found++] = buf + CELL_STRIDE * (uint32_t)(y - 1 - oy)
                               + 24u * (uint32_t)(x - 1 - ox) + CELL_BASE;
        }
    }

    if (found == 0) {
        for (i = 0; i < (int)dword_53BEB; ++i) {
            uint8_t *rec = (uint8_t *)(uintptr_t)dword_53A45 + 80u * (uint32_t)i;
            if (*(const uint16_t *)(const void *)(rec + 64) == 0)
                rec[5] = 1;
        }
        return;
    }

    for (j = 0; j < 13; ++j) {
        sub_11EEE(buf + VIEW_SRC_OFF, 456, 13, 8, ox, oy);
        for (i = 0; i < (int)dword_53BEB; ++i) {
            uint8_t *rec = (uint8_t *)(uintptr_t)dword_53A45 + 80u * (uint32_t)i;
            if ((rec[5] & 1) == 0) {
                if (*(const uint16_t *)(const void *)(rec + 64) == 0)
                    rec[3] = (uint8_t)(j % 4);
                sub_127E0(i);
            }
        }
        sub_129EC();
        sub_11EB0((char *)VGA_P(0xA0504u), 320, (char *)(uintptr_t)dword_53A49 + VIEW_SRC_OFF,
                  456, 0x138u, 192);
        sub_17AA9(1);
    }

    for (i = 0; i < (int)dword_53BEB; ++i) {
        uint8_t *rec = (uint8_t *)(uintptr_t)dword_53A45 + 80u * (uint32_t)i;
        if (*(const uint16_t *)(const void *)(rec + 64) == 0)
            rec[5] = 1;
    }

    scratch = (uint8_t *)malloc(SCRATCH_SIZE);      /* 0x25680 */
    sub_11EEE(scratch + VIEW_SRC_OFF, 456, 13, 8, ox, oy);
    {
        uint8_t *saved = buf;
        dword_53A49 = scratch;
        sub_127A9();
        dword_53A49 = saved;
    }
    sub_25A96(dword_53EEC, 3, 1);              /* svc_play_sfx */

    for (n = 0; n < 6; ++n) {
        for (i = 0; i < found; ++i) {
            uint32_t off = *(const uint32_t *)
                (const void *)(atlas + 4u * (uint32_t)(n + 68) + 6u);
            sub_4EBAB(v19[i], (int16_t *)(atlas + off), 456);
        }
        sub_11EB0((char *)VGA_P(0xA0504u), 320, (char *)(uintptr_t)dword_53A49 + VIEW_SRC_OFF,
                  456, 0x138u, 192);
        sub_17AA9(1);
    }

    for (j = 6; j < 12; ++j) {
        memmove((void *)(uintptr_t)dword_53A49, scratch, SCRATCH_SIZE);
        for (i = 0; i < found; ++i) {
            uint32_t off = *(const uint32_t *)
                (const void *)(atlas + 4u * (uint32_t)(j + 68) + 6u);
            sub_4EBAB(v19[i], (int16_t *)(atlas + off), 456);
        }
        sub_11EB0((char *)VGA_P(0xA0504u), 320, (char *)(uintptr_t)dword_53A49 + VIEW_SRC_OFF,
                  456, 0x138u, 192);
        sub_17AA9(1);
    }

    free(scratch);
    sub_11CAC(0);
}

/* --- phases of the unit-move animation 0x22253 (see map_units.h) -------- */

/* 0x22470 - walk-in pass: for each of atlas dword_53AD1's 11 frames 114..124,
 * restore the map view from `scratch`, blit the frame into the cell at
 * (dst_x, dst_y), refresh the portrait layer and publish the 312x192 window
 * to VGA_P(0xA0504u) before waiting one tick.  Arguments decoded from the single call
 * site inside 0x22253 (analysis/cseg01_disasm.txt). */
void map_units_move_walk(int dst_x, int dst_y, void *scratch)
{
    const int ox = (int)dword_53AA9;
    const int oy = dword_53AAD;
    int i;

    for (i = 0; i < 11; ++i) {
        const void *sprite = (const void *)(uintptr_t)
            (dword_53AD1 + *(const uint32_t *)(uintptr_t)
                           (dword_53AD1 + 4u * (uint32_t)(i + 114) + 6u));

        memmove(dword_53A49, scratch, 0x25680u);
        rle2_blit_trans(dword_53A49 + 0x8088u + 24u * (uint32_t)(dst_x - ox)
                        + 10944u * (uint32_t)(dst_y - oy) + 456u,
                        sprite, 456);
        dlg_portraits_refresh();
        gfx_copy_rows((void *)(uintptr_t)VGA_P(0xA0504u), 320, dword_53A49 + 0x8088u,
                      456, 0x138, 192);
        svc_wait_ticks(1);
    }
}

/* 0x22547 - reveal pass: blit `sprite` into `scratch` at the same cell, then
 * for i = 5..0 restore the view from `scratch` and draw reveal_band around
 * (cx, cy) with atlas dword_53A6D's frame i (y0 = (cy / 5) * i), publishing
 * and delaying (j___delay(10)) each step.  Returns the last frame sprite left in EDI by
 * the shared epilogue 0x1E5B9 - 0x22253 feeds it straight back to reveal_band
 * as the table.  The recorded prototype (int(int)) is a chkstk artifact; the
 * six arguments below come from the single call site inside 0x22253. */
const void *map_units_move_reveal(int dst_x, int dst_y, void *scratch,
                                  int cx, int cy, const void *sprite)
{
    const int   ox   = (int)dword_53AA9;
    const int   oy   = dword_53AAD;
    const void *last = sprite;
    int i;

    rle2_blit_trans((uint8_t *)scratch + 0x8088u
                    + 24u * (uint32_t)(dst_x - ox)
                    + 10944u * (uint32_t)(dst_y - oy) + 456u,
                    sprite, 456);

    for (i = 5; i >= 0; --i) {
        const void *spr = (const void *)(uintptr_t)
            (dword_53A6D + *(const uint32_t *)(uintptr_t)
                           (dword_53A6D + 4u * (uint32_t)i + 6u));

        memmove(dword_53A49, scratch, 0x25680u);
        reveal_band(cx, cy, 11, (cy / 5) * i, 192, spr);
        gfx_copy_rows((void *)(uintptr_t)VGA_P(0xA0504u), 320, dword_53A49 + 0x8088u,
                      456, 0x138, 192);
        j___delay(10);
        last = spr;
    }

    svc_wait_ticks(1);
    svc_wait_ticks(1);
    return last;
}

/* 0x22656 - settle pass: the same reveal_band loop for i = 0..9 over the
 * dword_53A6D frames, but with y0 fixed at 0; only arguments 3..5 (scratch,
 * cx, cy) are read, the other four are kept for the 7-argument ABI that
 * 0x22253 uses and left unnamed.  Returns the last tick reading in EAX. */
int map_units_move_settle(int unused1, int unused2, void *scratch,
                          int cx, int cy, const void *unused3,
                          const void *unused4)
{
    int i, r = 0;

    (void)unused1;
    (void)unused2;
    (void)unused3;
    (void)unused4;

    for (i = 0; i < 10; ++i) {
        const void *spr = (const void *)(uintptr_t)
            (dword_53A6D + *(const uint32_t *)(uintptr_t)
                           (dword_53A6D + 4u * (uint32_t)i + 6u));

        memmove(dword_53A49, scratch, 0x25680u);
        reveal_band(cx, cy, 11, 0, 192, spr);
        gfx_copy_rows((void *)(uintptr_t)VGA_P(0xA0504u), 320, dword_53A49 + 0x8088u,
                      456, 0x138, 192);
        r = svc_wait_ticks(1);
    }

    return r;
}

/* 0x22253 - driver of the three move phases above.  `rec` names a record in
 * dword_53A45: its cell bytes are set to (rec_x, rec_y) while the walk /
 * reveal / settle phases are played at the cell (at_x, at_y) on a 0x25680
 * scratch copy of the map view.  When (rec_x, rec_y) already equals
 * (at_x, at_y) the sound-effect index becomes 1 (otherwise 0).  FDOTHER.DAT
 * entry 81 is loaded for that effect and freed at the end; the sprite blitted
 * into the cell comes from the dword_53AD1 overlay atlas at its +0x1F6
 * offset.  After the reveal a 24-row band (18 rows on the view's top line)
 * is memmove'd from the scratch copy into the frame buffer in 24-byte pieces,
 * 10 ms apart.  Argument order and the chkstk-suppressed frame come from the
 * four call sites 0x221FD / 0x2223F / 0x2541F / 0x25535. */
void map_units_move_play(int rec, int rec_x, int rec_y, int at_x, int at_y)
{
    void    *dato     = res_load(aFdotherDat, NULL, 81);
    uint8_t *scratch  = (uint8_t *)guest_malloc(0x25680u);
    uint8_t *record   = (uint8_t *)(uintptr_t)dword_53A45 + 80u * (uint32_t)rec;
    const int ox      = (int)dword_53AA9;
    const int oy      = dword_53AAD;
    const void *sprite = (const void *)(uintptr_t)
        (dword_53AD1 + *(const uint32_t *)(uintptr_t)(dword_53AD1 + 502u));
    const int cx = 24 * (int)dword_53AB9 + 12;
    const int cy = 24 * (int)dword_53ABD + 15;
    const void *last;
    uint8_t *dst, *src;
    uint8_t rows;
    int i;

    map_render_view(scratch + 0x8088u, 456, 13, 8, ox, oy);
    map_units_move_walk(at_x, at_y, scratch);

    svc_play_sfx(dato, (rec_x == at_x && rec_y == at_y) ? 1 : 0, 1);
    last = map_units_move_reveal(at_x, at_y, scratch, cx, cy, sprite);

    record[0] = (uint8_t)rec_x;
    record[1] = (uint8_t)rec_y;
    memmove(dword_53A49, scratch, 0x25680u);

    reveal_band(cx, cy, 11, 0, 192, last);

    {
        uint8_t *base = dword_53A49 + 24 * (at_x - ox) + 0x8088u;

        src  = base + 10944 * (at_y - oy) - 2736;
        dst  = (uint8_t *)(uintptr_t)dword_53A49
               + 24 * (at_x - ox) + 654724 + 7680 * (at_y - oy);
        rows = 24;
        if (at_y == oy) {
            rows = 18;
            src  = base + 10944 * (at_y - oy);
            dst += 1920;
        }
    }
    for (i = 0; i < rows; ++i) {
        memmove(dst, src, 0x18u);
        dst += 320;
        src += 456;
        j___delay(10);
    }

    map_units_move_settle(at_x, at_y, scratch, cx, cy, sprite, last);
    guest_free(scratch);
    guest_free(dato);
}

/* 0x2218A - deploy one map unit onto its (target) cell and follow it with the
 * view.  `target` points at the record index to deploy (the only call site,
 * 0x20C6F case 23, passes its unit-index array); `actor` is the acting record
 * handed to rec_sub_table5.  The sequence glides the unit's portrait, spends
 * one rec_sub_table5 entry, adds 10 * move-cost to the dword_53EC8 experience
 * bookkeeping (cost = record byte 33, +30 when byte 32 is a walkable terrain
 * 9..24), plays map_units_move_play from off-map (255,255) to the record's own
 * cell, slides the view to qword_51CF9 (low dword = x, high dword = y), and
 * replays the move onto that cell.  The middle argument is unused but keeps
 * the 3-argument ABI of the call site. */
void map_units_move_deploy(int actor, int unused, const uint8_t *target)
{
    const uint32_t x   = (uint32_t)qword_51CF9;
    const uint32_t y   = (uint32_t)(qword_51CF9 >> 32);
    const uint8_t  idx = *target;
    const uint8_t *rec = (const uint8_t *)(uintptr_t)dword_53A45
                         + 80u * (uint32_t)idx;
    uint32_t cost = rec[33];

    (void)unused;

    dlg_portrait_glide(idx);
    rec_sub_table5(actor, 23);

    if (rec[32] > 8u && rec[32] < 0x19u) {
        cost += 30u;
    }
    dword_53EC8 += 10u * cost;

    map_units_move_play(idx, 255, 255, rec[0], rec[1]);

    dword_51A83 = 0;
    map_slide_view((int)x, (int)y);
    map_units_move_play(idx, (int)x, (int)y, (int)x, (int)y);

    dword_51A83 = 1;
}
