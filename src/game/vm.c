/* vm.c - FD2 script/text VM (source translation of 0x15F84).
 *
 * The interpreter walks a signed int16 word stream. Negative words are
 * opcodes, everything else is a glyph index drawn into mode 13h memory:
 *
 *   -1            close the box (if open) and return        [end of stream]
 *   -2            new paragraph, no key wait
 *   -3            new paragraph, wait for a key + typewriter step
 *   -4 / -5       run sub-stream dword_53AD9 / dword_53ADD, keep its return
 *   -6            print the decimal digits of dword_53AE1 as glyphs
 *   -17 / -18     portrait/box for record `operand` in the top/bottom box
 *                 (operand is an id passed to rec_find; 39 = "keep the
 *                 record the interpreter is already holding")
 *   -19 / -20     same, but `operand` indexes the 80-byte record table at
 *                 dword_53A45 directly, no rec_find
 *   >= 0, others  draw one glyph and advance 16 bytes
 *
 * Boxes: dword_53C67 holds the box's VGA offset (0 = closed, 0x728 = top
 * box, 0x9017 = bottom). Opening a box resets the text origin to
 * VGA_P(0xA0000u) + 0xB4F / VGA_P(0xA0000u) + 0x951F and the paragraph counter to 0.
 *
 * The two registers that survive across opcodes matter:
 *   cur   (ebp)  current VGA write address, threaded through the recursion
 *   rec   (edi)  "the record/datum to draw from". It is *not* reset between
 *                opcodes: after every portrait op it is left pointing at
 *                `DATO buffer + its first byte` (that is where the RLE block
 *                handed to 0x4EBFF/0x4EC31 starts), and only -17 with
 *                operand 39 reads it without assigning it first.
 *                At function entry EDI is whatever the caller left there -
 *                IDA's `a5@<edi>` parameter is the stack-probe artifact, no
 *                caller sets it - so that one case cannot be reproduced
 *                exactly and falls back to the last rec_find record with a
 *                log line (vm: -17/39 ...). See docs/rounds/09-vm.md.
 *
 * Verified by src/vmcheck.c against the machine code: same stubs on both
 * sides, whole event sequence + globals + return value compared.
 */
#include "vm.h"
#include <stdio.h>
#include <string.h>
#include "fd2_game.h"   /* de-guest C4: restored cross-module decls */
#include "../include/fd2_globals.h"

#define VGA_BASE      fd2_vga
#define BOX_TOP       0x00000728        /* dword_53C67: upper dialog box     */
#define BOX_BOTTOM    0x00009017        /* dword_53C67: lower dialog box     */
#define ADDR_TOP      (fd2_vga + 0x0B4Fu)       /* text origin inside the top box    */
#define ADDR_BOTTOM   (fd2_vga + 0x951Fu)       /* ... inside the bottom box         */
#define RECORD_SIZE   80                /* stride of the dword_53A45 table   */
#define FACE_KEEP     39                /* -17 operand: keep the held record */

/* ------------------------------------------------ game data segment ---- */
/* IDA names kept so the C reads like the original decompilation; addresses
 * verified against E:\FD2\FD2.EXE.i64. */

/* ------------------------------------------- services at original addrs -- */
typedef void *(*open_box_fn)(int face_x, int face_y, int rows);
typedef void  (*close_box_fn)(void *box, int rows);
typedef void  (*dato_fn)(int mode);
typedef void  (*wait_fn)(int advance);
typedef void  (*scroll_fn)(void);
typedef int   (*type_step_fn)(void);
typedef int   (*rec_find_fn)(int id);
typedef void *(*res_fn)(const char *name, void *cur, int index);
typedef int   (*draw_fn)(void *font, int glyph, int addr, int pitch,
                         int fg, int shadow, int fill);
typedef void  (*blit_fn)(void *dst, const void *src, int len);
typedef int   (*pending_fn)(void);


/* Last-resort record pointer for a portrait op whose `rec` is NULL because
 * rec_find() found nothing.  The box mode is 0 in that case, so
 * dlg_open_box() never uses the two bytes open_and_blit() reads. */
static uint8_t vm_no_rec[2] = { 0, 0 };

static uint8_t *vm_face_rec(uint8_t *rec)
{
    if (rec == NULL)
        rec = (uint8_t *)dword_53A45;
    if (rec == NULL)
        rec = vm_no_rec;
    return rec;
}

/* Close whatever box is open - the same three calls in the same order the
 * machine code uses at the top of -1/-17/-18/-19/-20. */
static void close_open_box(void *box, int mode)
{
    if (!box)
        return;
    dlg_blit_dato(dword_53A85, dword_53C67, 0);
    dlg_wait_key(0);
    dlg_close_box((void **)box, mode);
}

/* Shared tail of -17/-19 (top box, RLE 0x4EBFF) and -18/-20 (bottom box,
 * RLE 0x4EC31): open the box on `rec`, blit the DATO image, reset the text
 * origin. `rec` is deliberately left pointing at the RLE block, exactly
 * where the machine code leaves EDI. */
static void open_and_blit(int box_id, uint8_t *rec, int mode, int rle_top,
                          void **box, uint8_t **rec_io, uint8_t **start, uint8_t **cur)
{
    uint8_t *buf;
    uint8_t *origin = (box_id == BOX_TOP) ? ADDR_TOP : ADDR_BOTTOM;

    *box = dlg_open_box(rec[0], rec[1], mode);

    buf    = (uint8_t *)dword_53A85;
    *rec_io = buf + *buf;                       /* EDI after the blit setup */
    if (rle_top)
        rle2_blit((void *)(uintptr_t)(VGA_BASE + (uint32_t)dword_53C67),
                     *rec_io, 320);
    else
        rle2_blit_mirror((void *)(uintptr_t)(VGA_BASE + (uint32_t)dword_53C67),
                     *rec_io, 320);

    *start = origin;
    *cur   = origin;
}

uint8_t *vm_run(void *stream, int sub, uint8_t *addr, int pitch,
                int fg, int shadow, int bgfill, int line_step, int wait)
{
    const uint8_t *s   = (const uint8_t *)stream;
    const int16_t *p   = (const int16_t *)(s +
                          *(const int16_t *)(s + 2 * sub));
    uint8_t *start = addr;                      /* arg_8: text origin      */
    uint8_t *cur   = addr;                      /* ebp: write address      */
    int      line  = 0;                         /* var_18: paragraph index */
    void    *box   = NULL;                      /* var_1C: open box handle */
    int      mode  = 0;                         /* var_20: box row count   */
    uint8_t *rec   = NULL;                      /* edi: record/datum       */
    char     num[12];
    int      op, i, len;

    for (;;) {
        const int16_t *next = p + 1;            /* var_28                  */

        op = *p;
        switch (op) {

        case -1:                                /* end of stream           */
            /* the original clears dword_53C67 only inside the `box != 0`
             * branch (0x164B1 jumps straight past the `mov ...,0`) */
            if (box) {
                close_open_box(box, mode);
                dword_53C67 = 0;
            }
            return cur;

        case -2:                                /* new paragraph, no wait  */
            if ((dword_53C67 == BOX_TOP || dword_53C67 == BOX_BOTTOM) &&
                line == 3) {
                dlg_scroll_text(dword_53C67);
                line--;
            }
            line++;
            cur = start + pitch * line_step * line;
            p = next;
            continue;

        case -3:                                /* new paragraph + wait    */
            if ((dword_53C67 == BOX_TOP || dword_53C67 == BOX_BOTTOM) &&
                line == 3) {
                dlg_scroll_text(dword_53C67);
                line--;
            }
            line++;
            cur = start + pitch * line_step * line;
            p = next;
            if (dword_53C67 == BOX_TOP || dword_53C67 == BOX_BOTTOM)
                dlg_blit_dato(dword_53A85, dword_53C67, 0);
            dlg_wait_key(1);
            wait = 1;
            continue;

        case -4:                                /* sub-stream (branch A)   */
            cur = vm_run(dword_53A7D, dword_53AD9, cur, pitch,
                         205, 76, 74, 19, 1);
            p = next;
            continue;

        case -5:                                /* sub-stream (branch B)   */
            cur = vm_run(dword_53A7D, dword_53ADD, cur, pitch,
                         205, 76, 74, 19, 1);
            p = next;
            continue;

        case -6:                                /* decimal number as glyphs*/
            sprintf(num, "%d", dword_53AE1);
            len = (int)strlen(num);
            for (i = 0; i < len; i++) {
                gfx_draw_glyph(dword_53A75, num[i] - '0', cur, pitch,
                          fg, shadow, bgfill);
                if (kbd_pending())
                    wait = 0;
                if (wait)
                    dlg_type_step();
                cur += 16;
            }
            p = next;
            continue;

        case -17: {                             /* top box, by record id   */
            int id = (uint16_t)p[1];

            close_open_box(box, mode);
            dword_53C67 = BOX_TOP;
            int found = rec_find(id);

            mode = (found == -1) ? 0 : 2;
            if (id != FACE_KEEP) {
                rec = (uint8_t *)dword_53C1B;
                /* rec_find() clears dword_53C1B at entry, so a miss leaves
                 * rec NULL here.  mode is 0 in that case and the operand
                 * itself is the DATO portrait id - keep it instead of
                 * dereferencing NULL. */
                if (rec != NULL)
                    id = rec[7];
            } else if (rec == NULL) {
                /* the only read of EDI before any write inside this call */
                printf("vm: -17/%d as the first portrait op - the original"
                       " reads the caller's EDI; falling back to the last"
                       " rec_find record\n", id);
                rec = (uint8_t *)dword_53C1B;
            }
            /* rec_find() zeroed dword_53C1B if no record carries this id.
             * mode is then 0, and dlg_open_box() ignores the two record
             * bytes open_and_blit() reads (rec[0]/rec[1]); any valid
             * pointer keeps the machine code's call sequence intact. */
            rec = vm_face_rec(rec);
            dword_53A85 = res_load("DATO.DAT", dword_53A85, id);
            p = next + 1;                       /* opcode + operand        */
            open_and_blit(BOX_TOP, rec, mode, 1, &box, &rec, &start, &cur);
            wait = 1;
            line = 0;
            continue;
        }

        case -18: {                             /* bottom box, by record id*/
            int id = (uint16_t)p[1];

            close_open_box(box, mode);
            dword_53C67 = BOX_BOTTOM;
            mode = (rec_find(id) == -1) ? 0 : 0x70;
            rec  = (uint8_t *)dword_53C1B;
            if (rec != NULL)
                id = rec[7];
            rec = vm_face_rec(rec);
            dword_53A85 = res_load("DATO.DAT", dword_53A85, id);
            p = next + 1;
            open_and_blit(BOX_BOTTOM, rec, mode, 0, &box, &rec, &start, &cur);
            wait = 1;
            line = 0;
            continue;
        }

        case -19: {                             /* top box, table index    */
            int idx = (uint16_t)p[1];

            close_open_box(box, mode);
            dword_53C67 = BOX_TOP;
            mode = 2;
            rec  = (uint8_t *)dword_53A45 + RECORD_SIZE * idx;
            dword_53A85 = res_load("DATO.DAT", dword_53A85, rec[7]);
            p = next + 1;
            open_and_blit(BOX_TOP, rec, mode, 1, &box, &rec, &start, &cur);
            wait = 1;
            line = 0;
            continue;
        }

        case -20: {                             /* bottom box, table index */
            int idx = (uint16_t)p[1];

            close_open_box(box, mode);
            dword_53C67 = BOX_BOTTOM;
            mode = 0x70;
            rec  = (uint8_t *)dword_53A45 + RECORD_SIZE * idx;
            dword_53A85 = res_load("DATO.DAT", dword_53A85, rec[7]);
            p = next + 1;
            open_and_blit(BOX_BOTTOM, rec, mode, 0, &box, &rec, &start, &cur);
            wait = 1;
            line = 0;
            continue;
        }

        default:                                /* one glyph              */
            gfx_draw_glyph(dword_53A75, op, cur, pitch, fg, shadow, bgfill);
            cur += 16;
            p = next;
            if (kbd_pending())
                wait = 0;
            if (wait)
                dlg_type_step();
            continue;
        }
    }
}
