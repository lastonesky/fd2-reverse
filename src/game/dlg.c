/* dlg.c - FD2 dialogue-box helpers (source translation).
 *
 *   0x16559  dlg_blit_dato    - pure: globals passed in by src/repl.c
 *   0x16E24  dlg_scroll_text  - pure: box position passed in
 *   0x165AC  dlg_open_box     - app-level: original globals + services
 *   0x16B43  dlg_close_box    - app-level
 *   0x168B6  dlg_box_stage    - app-level (reads the frame resource global)
 *   0x1685C  dlg_frame_tile   - pure
 *   0x16C57  dlg_wait_key     - app-level (BDA tick / BIOS key / palette)
 *   0x164E8  dlg_type_step    - app-level (typed one character)
 *
 * The app-level half keeps the original addresses: the globals really do
 * live in the game's data segment (all four entries can be hooked without
 * wrapper code because the C reads/writes the same words the machine code
 * does), and a handful of services stay machine code while the port is
 * partial - they are heap-, timing- or BDA-coupled:
 *
 *   0x15E9E  save a VGA rectangle into a CRT block, blit a sprite on it
 *   0x15E71  restore such a block and free it (other callers hand it
 *            buffers from the Watcom CRT heap, so it cannot move to libc
 *            free yet - same reasoning as res.c, see src/repl.c)
 *   0x12CEA  glide the speaker portrait to (x, y)
 *   0x3790A  delay(10)
 *   0x4E381  flush the BIOS keyboard buffer (BDA 0x41A/0x41C)
 *   0x3706E  Watcom CRT malloc - pairs with the free inside 0x15E71
 *   0x4E310  read the BIOS tick word (BDA 0x46C - the C cannot address the
 *            BDA directly: Windows keeps page 0 unmappable)
 *   0x4EBE3  16-bit ROL rand (state in word_627B8)
 *   0x10620  is a key pending? (BDA 0x41A != 0x41C)
 *   0x4E31C  palette animation - writes the VGA DAC through ports 0x3C8/9
 *   0x370F0  int386(intno, inregs, outregs) - the BIOS key read at the end
 *            of dlg_wait_key
 * 0x4ECBF (gfx_save_rect) and 0x4ED0B (gfx_blit_block) are already
 * translated, so the C calls the translated versions directly.
 *
 * Verified against the machine code by src/boxcheck.c (whole-frame VGA,
 * the five stage snapshots, the global flag and a full event log of every
 * service call - ordering, arguments and VGA at each delay), by
 * src/keycheck.c for dlg_wait_key: its wait loop is driven by a hooked
 * palette step (deterministic BIOS tick + scripted key) with the VGA
 * snapshotted on every iteration, and by src/typecheck.c for dlg_type_step,
 * which fits the two services it ends with - svc_play_sfx and svc_wait_ticks
 * (src/game/svc.c) - into the same deterministic clock.
 */
#include "dlg.h"
#include "gfx.h"
#include "rle2.h"
#include "svc.h"
#include "rec.h"
#include "res.h"
#include "rle.h"
#include "map.h"
#include "sprite24.h"
#include "guest_mem.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "fd2_game.h"   /* de-guest C4: restored cross-module decls */
#include "../include/fd2_globals.h"

#define VGA_BASE fd2_vga

/* ------------------------------------------------ app-level part ------- */
/* The game's data segment (IDA names kept so the C reads like the original
 * decompilation). Addresses verified against E:\FD2\FD2.EXE.i64. */
#define VGA        (fd2_vga)

typedef void  (*delay_fn)(unsigned ms);
typedef void  (*flush_fn)(void);
typedef void  (*glide_fn)(int face_x, int face_y);
typedef void  *(*crt_alloc_fn)(size_t n);
typedef unsigned (*tick_fn)(void);          /* 0x4E310: BDA 0x46C word     */
typedef int     (*rand_fn)(void);           /* 0x4EBE3: ROL rand           */
typedef int     (*pending_fn)(void);        /* 0x10620: key waiting?       */
typedef void    (*palette_fn)(void);        /* 0x4E31C: DAC animation      */
typedef int     (*int386_fn)(int intno, const void *in, void *out);


/* The 310x86 frame drawn by dlg_open_box, as five growing tile stages:
 * (cols, lines) per stage, 16 px cells inside a 3 px border. */
static const uint8_t dlg_stage_cols[5]  = { 4, 8, 12, 16, 19 };
static const uint8_t dlg_stage_lines[5] = { 2, 3, 4, 5, 5 };

/* Sprite block the portrait sweep draws: the frame resource's offset table
 * starts at +6, and the sweep reads entry 0 as a sign-extended word. */
static const uint8_t *dlg_face_block(const uint8_t *frame)
{
    return frame + *(const int16_t *)(frame + 6);
}

/* 0x15E9E - snapshot helper: copy a w*h rectangle of `surface` into a fresh
 * record (8-byte gfx_save_rect header + pixels, on the *game* heap), then draw
 * the sprite `block` transparently at (x,y). The portrait sweep calls this
 * once per animation step (0x165AC/0x16B43 do). The returned record is handed
 * to 0x15E71, which restores and frees it. */
void *dlg_snap_save(const void *block, void *surface, int stride, int x, int y)
{
    int    w   = *(const int16_t *)block;
    int    h   = *(const int16_t *)((const uint8_t *)block + 2);
    int    off = y * stride + x;
    void  *rec = guest_malloc((size_t)(w * h + 8));

    gfx_save_rect(rec, w, h, surface, off, stride);
    gfx_blit_transparent((uint8_t *)surface + off, block, stride);
    return rec;
}

/* 0x15E71 - put the record back on the surface and free it. Called from
 * inside 0x165AC/0x16B43 and from several not-yet-translated functions, so the
 * record may have come from either the C helper above or the original 0x15E9E
 * / 0x15F0E - both allocate on the game heap, so guest_free is right. */
void dlg_snap_restore(void *record, void *surface, int stride)
{
    gfx_restore_rect(record, surface, stride);
    guest_free(record);
}

/* 0x165AC */
void *dlg_open_box(int face_x, int face_y, int rows)
{
    int vw, vh, steps, i, off;

    if (rows != 0) {
        dword_51A83 = 0;                       /* fast: no per-step wait   */
        map_slide_view(face_x, face_y);
        dword_51A83 = 1;

        vw    = 24 * dword_53AB9 + 4;
        vh    = 24 * dword_53ABD + 4;
        steps = dword_53AB9 + dword_53ABD;
        if (steps != 0) {
            const uint8_t *face = dlg_face_block(dword_53A81);

            for (i = 0; i <= steps; i++) {
                dword_53A18[0] = dlg_snap_save(face, VGA, 320,
                                               vw - i * (vw - 5) / steps,
                                               vh - i * (vh - rows) / steps);
                delay(10);
                kbd_flush();
                dlg_snap_restore(dword_53A18[0], VGA, 320);
            }
        }
    } else if (dword_53C67 == 0x728) {
        rows = 2;
    } else if (dword_53C67 == 0x9017) {
        rows = 112;
    }

    for (i = 0; i < 5; i++)
        dword_53A18[i] = malloc(26668);

    off = 320 * rows + 5;
    for (i = 0; i < 5; i++) {
        gfx_save_rect(dword_53A18[i], 310, 86, VGA, off, 320);
        dlg_box_stage(VGA, 320, 5, rows, dlg_stage_cols[i],
                      dlg_stage_lines[i]);
        if (i < 4)
            delay(10);
    }
    kbd_flush();
    return dword_53A18;
}

/* 0x16B43 */
void dlg_close_box(void **stages, int rows)
{
    int i;

    for (i = 4; i > 0; i--) {                 /* reverse of the open order */
        dlg_snap_restore(stages[i], VGA, 320);
        delay(10);
    }
    dlg_snap_restore(stages[0], VGA, 320);

    if (rows != 0) {
        int vw    = 24 * dword_53AB9 + 4;
        int vh    = 24 * dword_53ABD + 4;
        int steps = dword_53AB9 + dword_53ABD;

        if (steps != 0) {
            const uint8_t *face = dlg_face_block(dword_53A81);

            for (i = 0; i <= steps; i++) {
                stages[0] = dlg_snap_save(face, VGA, 320,
                                          5 - i * (5 - vw) / steps,
                                          rows - i * (rows - vh) / steps);
                delay(10);
                dlg_snap_restore(stages[0], VGA, 320);
            }
        }
    }
}

/* 0x168B6 - one stage of the box frame: 16 px tile grid `cols` x `lines`,
 * top border on row y0, left border at column x0, in a 3 px frame. */
void dlg_box_stage(void *surface, int stride, int x0, int y0,
                   int cols, int lines)
{
    const void  *table = dword_53A81;
    uint8_t     *base  = (uint8_t *)surface + stride * y0 + x0;
    uint8_t     *bottom = base + 3 * stride + lines * 16 * stride;
    uint8_t     *right  = base + 3 * stride + 16 * cols + 3;
    int          band   = (lines - 1) * 16 * stride;
    int          i, j, k, m;

    dlg_frame_tile(base, stride, table, 1);                 /* corners    */
    dlg_frame_tile(base + 16 * cols + 3, stride, table, 2);
    dlg_frame_tile(bottom, stride, table, 3);
    dlg_frame_tile(bottom + 16 * cols + 3, stride, table, 4);
    dlg_frame_tile(base + 3, stride, table, 5);             /* edges      */
    dlg_frame_tile(base + 19 + 16 * (cols - 2), stride, table, 6);
    dlg_frame_tile(bottom + 3, stride, table, 7);
    dlg_frame_tile(bottom + 19 + 16 * (cols - 2), stride, table, 8);
    dlg_frame_tile(base + 3 * stride, stride, table, 14);
    dlg_frame_tile(right, stride, table, 15);
    dlg_frame_tile(base + 3 * stride + band, stride, table, 16);
    dlg_frame_tile(right + band, stride, table, 17);

    for (i = 0; i < cols - 2; i++) {           /* top / bottom fill         */
        dlg_frame_tile(base + 16 * i + 19, stride, table, 9);
        dlg_frame_tile(bottom + 16 * i + 19, stride, table, 12);
    }
    for (j = 0; j < lines - 2; j++) {          /* left / right fill         */
        uint8_t *mid = base + 3 * stride + (j + 1) * 16 * stride;
        dlg_frame_tile(mid, stride, table, 10);
        dlg_frame_tile(mid + 16 * cols + 3, stride, table, 11);
    }
    for (k = 0; k < lines; k++)                /* interior                  */
        for (m = 0; m < cols; m++)
            dlg_frame_tile(base + 16 * m + 3 * stride + 3 + k * 16 * stride,
                           stride, table, 13);
}

/* 0x1685C */
void dlg_frame_tile(void *dest, int stride, const void *table, int idx)
{
    const uint8_t *base = (const uint8_t *)table;

    gfx_blit_block(dest, base + *(const uint32_t *)(base + 4 * idx + 6),
                   stride);
}

/* 0x16C57 - block until a key is pending, animating while waiting:
 * every BIOS tick the palette cycles (0x4E31C), the speaker tile flips
 * (frame resource tiles 18/19, three ticks each, only when speaker != 0)
 * and the mouth opens/closes (DATO sub-images 3 / 0, random hold time).
 * The key itself is read with INT 16h AH=10h into word_53A8D and the scan
 * code in AH is normalised (E0h/52h -> 1Ch, 53h -> 01h).
 *
 * The tick comparisons are 16-bit: the original sign-extends the BDA word
 * before subtracting (cwde / movsx), so this does too. */
void dlg_wait_key(int speaker)
{
    int     mouth_open = 0;             /* var_14: 1 = mouth (DATO 3) out */
    int     tile       = 18;            /* esi: speaker tile index        */
    int     fast       = 0;             /* edi: 3-tick tile flip counter  */
    int     base       = 0x47A0;        /* var_1C: text area row step     */
    uint8_t *area;                      /* var_20: text area base         */
    int     mouth_wait;                 /* ebp: ticks to hold the mouth   */
    int16_t tick_at;                    /* var_18                         */

    if (dword_53A51 == 0)
        base = 0x4770;

    tick_at    = (int16_t)pal_tick_word();
    mouth_wait = (int)(util_rand() % 30) + 2;
    area = (dword_53C67 == 0x728) ? VGA_P(0xA0B4Fu) : VGA_P(0xA951Fu);

    if (speaker == 1)
        dlg_frame_tile((void *)(uintptr_t)(area + base + 0x640), 320,
                       dword_53A81, 18);

    while (!kbd_pending()) {
        pal_anim_step();
        if ((int16_t)pal_tick_word() - tick_at < 2)
            continue;

        if (speaker == 1 && ++fast == 3) {          /* flip the tile   */
            fast = 0;
            if (++tile == 20)
                tile = 18;
            dlg_frame_tile((void *)(uintptr_t)(area + base + 0x640), 320,
                           dword_53A81, tile);
        }
        if (mouth_open) {                           /* close the mouth */
            dlg_blit_dato(dword_53A85, dword_53C67, 0);
            mouth_wait = (int)(util_rand() % 30) + 2;
            mouth_open = 0;
        } else if (mouth_wait-- == 0) {             /* open it         */
            dlg_blit_dato(dword_53A85, dword_53C67, 3);
            mouth_open = 1;
        }
        tick_at = (int16_t)pal_tick_word();
    }

    if (speaker == 1)
        dlg_frame_tile((void *)(uintptr_t)(area + base), 320,
                       dword_53A81, 13);

    byte_53A8E = 0x10;                  /* AH = 10h: BIOS read (enhanced) */
    int386(0x16, &word_53A8D, &word_53A8D);
    if (byte_53A8E == 0xE0 || byte_53A8E == 0x52)
        byte_53A8E = 0x1C;
    if (byte_53A8E == 0x53)
        byte_53A8E = 0x01;
}

/* 0x164E8 - one character of typewriter output. Called by the dialogue word
 * interpreter (0x15F84, the only caller) once per character: every second
 * character advances the mouth animation - the phase walks 0,1,2,3 and phase
 * 3 is drawn as sub-image 1, so the blitted sub-images repeat 1,2,1,0 and the
 * mouth closes again before the cycle starts over - then the typewriter click - sample index 2 of
 * the SFX bank - plays once and the loop waits a single BIOS tick.
 *
 * Both counters are private to this function (IDA: the only xrefs to
 * 0x53A10 / 0x53A14 are inside 0x164E8), but they are still the original
 * globals, kept because the loop's whole point is to carry state between
 * calls. Returns the last tick reading from svc_wait_ticks, which every
 * caller ignores. */
int dlg_type_step(void)
{
    int idx;

    if (++dword_53A14 == 2) {
        if (++dword_53A10 == 4)
            dword_53A10 = 0;
        idx = (dword_53A10 == 3) ? 1 : dword_53A10;
        dlg_blit_dato(dword_53A85, dword_53C67, idx);
        dword_53A14 = 0;
    }

    svc_play_sfx(dword_53EEC, 2, 1);
    return svc_wait_ticks(1);
}

void dlg_blit_dato(const void *dato_buf, int box_pos, int idx)
{
    const uint8_t *src = (const uint8_t *)dato_buf +
                         *(const uint32_t *)((const char *)dato_buf + 4 * idx);
    uint8_t       *dst = (uint8_t *)(uintptr_t)(VGA_BASE + (uint32_t)box_pos);

    if (box_pos == 0x9017)
        rle2_blit_mirror(dst, src, 320);   /* bottom box is mirrored */
    else
        rle2_blit(dst, src, 320);
}

void dlg_scroll_text(int box_pos)
{
    uint8_t *base;
    int      i, j, k;

    if (box_pos != 0x728 && box_pos != 0x9017)
        return;

    base = (box_pos == 0x728) ? VGA_P(0xA0B4Fu) : VGA_P(0xA951Fu);

    for (i = 0; i < 5; i++) {
        for (j = 0; j < 72; j++)
            memmove(base + 320 * j - 1, base + 320 * (j + 3) - 1, 208);
        memset(base + 23040, 0x4A, 208);
    }
    for (k = 0; k < 72; k++)
        memmove(base + 320 * k - 1, base + 320 * (k + 4) - 1, 208);
    memset(base + 23040, 0x4A, 208);
}

/* 0x12D7B - glide the portrait to the position stored in record `idx`
 * (dword_53A45 + 80*idx, first two bytes = x, y) via 0x12CEA. */
void dlg_portrait_glide(int idx)
{
    const uint8_t *e = (const uint8_t *)(uintptr_t)dword_53A45 + 80 * idx;

    map_slide_view(e[0], e[1]);
}

/* 0x134E4 - clear the "mouth open" flag (byte +3) of every portrait record
 * (stride 80, count dword_53BEB), then a 20 ms settle. */
void dlg_portrait_clear(void)
{
    uint8_t *p = (uint8_t *)(uintptr_t)dword_53A45;
    int      i;

    for (i = 0; i < dword_53BEB; i++) {
        p[3] = 0;
        p += 80;
    }
    delay(20);
}

/* 0x12C0D - find the portrait record whose (byte0,byte1) equals the 64-bit
 * position qword_53AB1 and which is not flagged by rec_flag(); -1 if none. */
int dlg_portrait_find(void)
{
    const uint8_t *p = (const uint8_t *)(uintptr_t)dword_53A45;
    int i;

    for (i = 0; i < dword_53BEB; i++, p += 80) {
        uint64_t v = (uint64_t)p[0] | ((uint64_t)p[1] << 32);
        if (v == qword_53AB1 && rec_flag(i) == 0)
            return i;
    }
    return -1;
}

/* 0x127E0 - draw the icon/portrait sprite of record `idx`.
 *
 * The record (dword_53A45 + 80*idx) gives the cell (x, y), the icon
 * resource p[2], a direction/mouth byte p[3] and a frame byte p[4]; p[38]
 * selects the "flip" variant. The sprite bank is dword_53A61, a 32-bit
 * offset table whose entry `mode + 12*p[2] + 3*p[3]` points at a 24x24
 * sprite24 stream; `mode` is the animation phase dword_53C07 (frame != 0)
 * or dword_53C0B (frame == 0), with 3 folded to 1 and p[38] forcing 0.
 * The two-frame wobble toggles dword_53A04 once per BIOS tick and offsets
 * the destination; p[5] bit 7 selects ramp24 recolouring, else plain.
 * Called by dlg_portraits_refresh and several state handlers.
 * Verified byte-for-byte against the machine code by src/mapcheck.c. */
void dlg_portrait_draw(int idx)
{
    const uint8_t *p = (const uint8_t *)(uintptr_t)dword_53A45 + 80 * idx;
    int32_t tick = (int32_t)(int16_t)pal_tick_word();
    int x, y, dir, frame, mouth, step, off, mode;
    const uint8_t *src;

    if (tick != dword_53A08) {
        dword_53A04 ^= 1;              /* low byte flips, like the machine */
        dword_53A08 = tick;
    }

    x = p[0]; y = p[1]; dir = p[3]; frame = p[4]; mouth = p[5];

    if (x < dword_53AA9 - 1 || x > dword_53AA9 + dword_51A87)
        return;
    if (y < dword_53AAD - 1 || y > dword_53AAD + dword_51A8B + 1)
        return;

    if (dir != 0) {
        if (dir == 1)
            step = -4;
        else if (dir == 2)
            step = -1824;
        else
            step = 4;
    } else {
        step = 1824;
    }

    off = 24 * (x - dword_53AA9) + 10944 * (y - dword_53AAD) + step * frame;
    if (p[38] != 0)
        off += dword_53A04;

    if (frame != 0)
        mode = dword_53C07;
    else
        mode = dword_53C0B;
    if (mode == 3)
        mode = 1;
    if (p[38] != 0)
        mode = 0;

    src = (const uint8_t *)(uintptr_t)dword_53A61
        + *(const uint32_t *)(uintptr_t)(dword_53A61
                                         + 4 * (mode + 12 * p[2] + 3 * dir));
    off += 30168;
    if (off < 0)
        return;

    if (mouth & 0x80)
        sprite24_ramp24(src, (uint8_t *)(uintptr_t)dword_53A49 + off, 456);
    else
        sprite24_plain(src, (uint8_t *)(uintptr_t)dword_53A49 + off, 456);
}

/* 0x127A9 - draw every unflagged portrait record, then refresh the map cell
 * sprites under the records. */
void dlg_portraits_refresh(void)
{
    int i;

    for (i = 0; i < dword_53BEB; i++)
        if (rec_flag(i) == 0)
            dlg_portrait_draw(i);
    map_refresh_records();
}

/* 0x187D6 - draw a decimal number as digit sprites from the box-frame
 * resource *(0x53A81). `digits` glyphs are placed 6 px apart starting at
 * `dst`; `base_index` is the sprite index of the digit '0' (each digit adds
 * its value). Values too wide for the digit count use dedicated sprites
 * (0x5D when 2 digits and value > 99, base+10 when 3 digits and value > 999). */
void dlg_draw_number(void *dst, int pitch, int value, int base_index, int digits)
{
    char fmt[8];
    char buf[16];
    int  i;

    if (value < 0)
        value = 0;
    strcpy(fmt, "%0.5d");
    if (digits == 3 && value > 999) {
        res_blit6(dst, pitch, dword_53A81, base_index + 10);
        return;
    }
    if (digits == 2 && value > 99) {
        res_blit6(dst, pitch, dword_53A81, 0x5D);
        return;
    }
    fmt[3] = (char)(digits + '0');
    sprintf(buf, fmt, value);
    for (i = 0; i < digits; i++) {
        int idx = (uint8_t)buf[i] + base_index - '0';
        res_blit6((uint8_t *)dst + 6 * i, pitch, dword_53A81, idx);
    }
}

/* 0x1875D - draw `value` with the sprite base chosen by whether it equals
 * `compare` (0x1F) or not (0x2A). */
void dlg_draw_number_pair(void *dst, int pitch, int value, int compare, int digits)
{
    dlg_draw_number(dst, pitch, value, (value == compare) ? 0x1F : 0x2A, digits);
}

/* 0x1AEB1 - signed 2-digit number: a sign sprite (0x83 positive / 0x84
 * negative) at `dst`, then the absolute value at dst + 8. */
void dlg_draw_number_signed(void *dst, int pitch, int value)
{
    int idx = 0x83;

    if (value < 0) {
        value = abs(value);
        idx   = 0x84;
    }
    rle_decode((uint8_t *)dword_53A81
                   + *(const uint32_t *)((uint8_t *)dword_53A81 + 4 * idx + 6),
               0, 0, dst, pitch, -1);
    dlg_draw_number((uint8_t *)dst + 8, pitch, value, 31, 2);
}
