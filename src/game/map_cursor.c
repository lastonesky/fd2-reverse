/* map_cursor.c - on-map direction-patch animation.  See map_cursor.h.
 *
 * Direct translations of the machine code at the stated addresses; the offline
 * disassembly (analysis/cseg01_disasm.txt) is authoritative for the argument
 * order, the constants and the frame order.  All real callees are
 * already-restored assets:
 *   dlg_portrait_find(0x12C0D)     svc_play_sfx(0x25A96)
 *   anim_frame_step(0x1297D)       map_render_view(0x11EEE)
 *   dlg_portraits_refresh(0x127A9) map_draw_cursor(0x1ACF3)
 *   gfx_blit_transparent(0x4ED34)  dlg_portrait_draw(0x127E0)
 *   gfx_copy_rows(0x11EB0)         kbd_flush(0x4E381)
 */
#include "fd2_game.h"
#include "map_cursor.h"

#include <string.h>

#define MAP_VIEW_OFF   0x8088u  /* dword_53A49 + 32904: first map-view byte */
#define MAP_VIEW_PITCH 456      /* 0x1C8: map-view stride in bytes         */
#define VGA_SPARE      0xA0504u /* 320x200 scratch page gfx_copy_rows uses */

#define BLOCK_ROWS     72       /* 0x48 rows of the stashed window block    */
#define BLOCK_BYTES    72       /* 0x48 bytes per row -> 72x72 block        */
#define BLOCK_SIZE     0x1440u  /* 0x1440 = 72*72 bytes                     */

#define PATCH_TILES    4        /* one blit per index-array entry           */

/* 0x175A9 - stash the 72x72-byte block the patch is stamped over.  The block
 * starts one map tile up/left of the view origin: the machine adds
 * 24*(dword_53AB9-1) + 10944*(dword_53ABD-1) on top of the +0x8088 view base.
 * The previous stash is released first; the new one is left in dword_53A71. */
void map_cursor_window_save(void)
{
    uint8_t *src;
    int row;

    if (dword_53A71 != 0)
        guest_free((void *)(uintptr_t)dword_53A71);
    dword_53A71 = (uint32_t)(uintptr_t)guest_malloc(BLOCK_SIZE);

    src = dword_53A49 + MAP_VIEW_OFF + 24 * (dword_53AB9 - 1) +
          10944 * (dword_53ABD - 1);
    for (row = 0; row < BLOCK_ROWS; ++row) {
        memmove((void *)(uintptr_t)(dword_53A71 + BLOCK_BYTES * (uint32_t)row),
                src, (size_t)BLOCK_BYTES);
        src += MAP_VIEW_PITCH;
    }
}

/* 0x17643 - copy the stashed block back over the same map-view rows.  The
 * original leaves the last memmove result in eax; every caller ignores it. */
void map_cursor_window_restore(void)
{
    uint8_t *dst;
    int row;

    dst = dword_53A49 + MAP_VIEW_OFF + 24 * (dword_53AB9 - 1) +
          10944 * (dword_53ABD - 1);
    for (row = 0; row < BLOCK_ROWS; ++row) {
        memmove(dst,
                (const void *)(uintptr_t)(dword_53A71 +
                                          BLOCK_BYTES * (uint32_t)row),
                (size_t)BLOCK_BYTES);
        dst += MAP_VIEW_PITCH;
    }
}

/* Stamp the four tiles of one frame at view + ofs[0..3].  `tbl` is the tile
 * bank dword_53A89: entry 3*a_tiles[j] + 2*b_tiles[j] is a byte offset into
 * it (32-bit, exactly as the scaled index [tbl + idx*4] of the machine). */
static void map_cursor_blit_tiles(uint8_t *view, const int *ofs,
                                  const int *a_tiles, const int *b_tiles)
{
    int j;

    for (j = 0; j < PATCH_TILES; ++j) {
        uint32_t tbl = dword_53A89;
        uint32_t idx = (uint32_t)(3 * a_tiles[j] + 2 * b_tiles[j]);
        uint32_t off = *(const uint32_t *)(uintptr_t)(tbl + 4u * idx);

        gfx_blit_transparent(view + ofs[j],
                             (const void *)(uintptr_t)(tbl + off),
                             MAP_VIEW_PITCH);
    }
}

/* 0x1741C - the opening animation: prepare the view (portrait lookup, sound,
 * frame step, view render, cursor, window save) then run four frames in which
 * the block is restored, the four patch tiles are stamped at diverging
 * offsets, the record portrait is redrawn and the 312x192 window is published.
 * The offset table starts at 0x390 for all four tiles and each frame applies
 * -0x8E8, -6, +6, +0x8E8. */
void map_cursor_patch_expand(const int *a_tiles, const int *b_tiles)
{
    int      ofs[PATCH_TILES] = { 0x390, 0x390, 0x390, 0x390 };
    uint8_t *view;
    int      found;
    int      frame;

    found = dlg_portrait_find();
    view = dword_53A49 + MAP_VIEW_OFF + 24 * dword_53AB9 +
           10944 * dword_53ABD;

    svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 8, 1);
    anim_frame_step();
    map_render_view(dword_53A49 + MAP_VIEW_OFF, MAP_VIEW_PITCH, 13, 8,
                    (int)dword_53AA9, dword_53AAD);
    dlg_portraits_refresh();
    map_draw_cursor(dword_53A49 + MAP_VIEW_OFF, MAP_VIEW_PITCH);
    map_cursor_window_save();

    for (frame = 0; frame < 4; ++frame) {
        ofs[0] -= 0x8E8;
        ofs[1] -= 6;
        ofs[2] += 6;
        ofs[3] += 0x8E8;

        map_cursor_window_restore();
        map_cursor_blit_tiles(view, ofs, a_tiles, b_tiles);
        if (found != -1)
            dlg_portrait_draw(found);
        gfx_copy_rows((void *)(uintptr_t)VGA_SPARE, 320,
                      dword_53A49 + MAP_VIEW_OFF, MAP_VIEW_PITCH, 0x138, 0xC0);
    }
}

/* 0x176B4 - the closing animation: same four-frame loop but the offsets
 * converge (-0x23A0/0x378/0x3A8/0x2AC0, each frame +0x8E8/+6/-6/-0x8E8), and
 * after the loop the block is restored once more and the keyboard flushed.
 * Unlike 0x1741C the original plays the sound before the portrait lookup and
 * never re-saves the block (0x1741C already did). */
void map_cursor_patch_collapse(const int *a_tiles, const int *b_tiles)
{
    int      ofs[PATCH_TILES] = { -0x23A0, 0x378, 0x3A8, 0x2AC0 };
    uint8_t *view;
    int      found;
    int      frame;

    svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 8, 1);
    found = dlg_portrait_find();
    view = dword_53A49 + MAP_VIEW_OFF + 24 * dword_53AB9 +
           10944 * dword_53ABD;

    for (frame = 0; frame < 4; ++frame) {
        ofs[0] += 0x8E8;
        ofs[1] += 6;
        ofs[2] -= 6;
        ofs[3] -= 0x8E8;

        map_cursor_window_restore();
        map_cursor_blit_tiles(view, ofs, a_tiles, b_tiles);
        if (found != -1)
            dlg_portrait_draw(found);
        gfx_copy_rows((void *)(uintptr_t)VGA_SPARE, 320,
                      dword_53A49 + MAP_VIEW_OFF, MAP_VIEW_PITCH, 0x138, 0xC0);
    }

    map_cursor_window_restore();
    kbd_flush();
}
