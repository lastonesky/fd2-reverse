/* ani.c - ANI.DAT animation-frame command decoder.  See ani.h.
 *
 * Direct translations of the machine code at the stated addresses.  The two
 * output buffers and the frame-buffer size are the original data-segment
 * globals (word_52760 / dword_52762 / dword_52766), set once per animation by
 * ani_target_set; every handler writes through them so the C and the rest of
 * the game observe the same bytes.  The stream cursor is the original ESI: it
 * is an implicit in/out argument, modelled here as `const unsigned char **sp`.
 *
 * The ten handlers form the funcs_37012 dispatch table (dword table 0x5276A);
 * ani_run_frame indexes that table by the opcode byte, exactly as the machine
 * `call [ebx+0x5276A]` does.  The opcode space of the format is 0..9.
 */
#include "ani.h"
#include "guest_mem.h"
#include "kbd.h"
#include "res.h"
#include "svc.h"

#include "fd2_game.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ---- ESI-as-cursor byte/word readers (the original lodsb/lodsw) ---- */
static unsigned char rd8(const unsigned char **sp)
{
    return *(*sp)++;
}

static unsigned short rd16(const unsigned char **sp)
{
    unsigned short lo = *(*sp)++;
    unsigned short hi = *(*sp)++;
    return (unsigned short)(lo | (hi << 8));
}

static void fill_byte(unsigned char *dst, unsigned int n, unsigned char v)
{
    unsigned int i;

    for (i = 0; i < n; ++i)
        dst[i] = v;
}

static unsigned char *pal_buf(void)
{
    return (unsigned char *)(uintptr_t)dword_52766;
}

static unsigned char *fb_buf(void)
{
    return (unsigned char *)(uintptr_t)dword_52762;
}

/* Run-length expansion shared by opcodes 2 (palette) and 6 (frame buffer):
 * a byte with its top two bits set is a run (length = low 6 bits, followed by
 * the value byte), anything else is one literal byte.  Stops when the output
 * position equals `total` (the machine compares bx != total at the top). */
static void rle_expand(unsigned char *dst, unsigned int total,
                       const unsigned char **sp)
{
    unsigned int pos = 0;

    while (pos != total) {
        unsigned int al = rd8(sp);

        if ((al & 0xC0) == 0xC0) {
            unsigned int len = al & 0x3F;
            unsigned char v = rd8(sp);

            fill_byte(dst, len, v);
            dst += len;
            pos += len;
        } else {
            *dst++ = (unsigned char)al;
            ++pos;
        }
    }
}

/* 0x36E3D - opcode 0: fill the 768-byte palette with one grey byte. */
void ani_pal_fill(const unsigned char **sp)
{
    unsigned char v = rd8(sp);

    fill_byte(pal_buf(), 0x300, v);
}

/* 0x36E57 - opcode 1: copy a full 768-byte palette from the stream. */
void ani_pal_copy(const unsigned char **sp)
{
    memcpy(pal_buf(), *sp, 0x300);
    *sp += 0x300;
}

/* 0x36E65 - opcode 2: run-length expanded palette (768 bytes). */
void ani_pal_rle(const unsigned char **sp)
{
    rle_expand(pal_buf(), 0x300, sp);
}

/* 0x36EA7 - opcode 3: `n` palette blocks; each is a byte offset and a byte
 * length, both scaled by 3 (RGB triplets), followed by that many bytes.  The
 * count is a do/while in the machine. */
void ani_pal_blocks(const unsigned char **sp)
{
    unsigned char *pal = pal_buf();
    unsigned int count = rd8(sp);

    do {
        unsigned int off = 3u * rd8(sp);
        unsigned int len = 3u * rd8(sp);

        memcpy(pal + off, *sp, len);
        *sp += len;
    } while (--count);
}

/* 0x36EE0 - opcode 4: fill the `word_52760`-byte frame buffer with one byte. */
void ani_fb_fill(const unsigned char **sp)
{
    unsigned char v = rd8(sp);

    fill_byte(fb_buf(), word_52760, v);
}

/* 0x36F08 - opcode 5: copy `word_52760` bytes into the frame buffer. */
void ani_fb_copy(const unsigned char **sp)
{
    memcpy(fb_buf(), *sp, word_52760);
    *sp += word_52760;
}

/* 0x36F24 - opcode 6: run-length expanded frame buffer (`word_52760` bytes). */
void ani_fb_rle(const unsigned char **sp)
{
    rle_expand(fb_buf(), word_52760, sp);
}

/* 0x36F69 - opcode 7: `n` single pixels; each is a 16-bit offset and a byte. */
void ani_fb_pixels(const unsigned char **sp)
{
    unsigned char *fb = fb_buf();
    unsigned int count = rd16(sp);

    do {
        unsigned int off = rd16(sp);

        fb[off] = rd8(sp);
    } while (--count);
}

/* 0x36F82 - opcode 8: `n` filled runs {16-bit offset, byte length, byte value}. */
void ani_fb_fill_runs(const unsigned char **sp)
{
    unsigned char *fb = fb_buf();
    unsigned int count = rd16(sp);

    do {
        unsigned int off = rd16(sp);
        unsigned int len = rd8(sp);
        unsigned char v = rd8(sp);

        fill_byte(fb + off, len, v);
    } while (--count);
}

/* 0x36FAC - opcode 9: `n` copied runs {16-bit offset, byte length, bytes}. */
void ani_fb_copy_runs(const unsigned char **sp)
{
    unsigned char *fb = fb_buf();
    unsigned int count = rd16(sp);

    do {
        unsigned int off = rd16(sp);
        unsigned int len = rd8(sp);

        memcpy(fb + off, *sp, len);
        *sp += len;
    } while (--count);
}

/* 0x36FF4 - execute `count` commands through the funcs_37012 table. */
void ani_run_frame(short count, const unsigned char *cmds)
{
    static void (*const ops[10])(const unsigned char **) = {
        ani_pal_fill, ani_pal_copy, ani_pal_rle, ani_pal_blocks,
        ani_fb_fill, ani_fb_copy, ani_fb_rle, ani_fb_pixels,
        ani_fb_fill_runs, ani_fb_copy_runs
    };
    const unsigned char *sp = cmds;

    while (count != 0) {
        unsigned int op = *sp++;

        ops[op](&sp);
        --count;
    }
}

/* 0x36FD3 - bind the frame buffer (size + pointer) and the palette pointer.
 * The machine stores ax / eax (32-bit) into the three globals. */
void ani_target_set(uint16_t size, void *screen, void *palette)
{
    word_52760  = size;
    dword_52762 = screen;
    dword_52766 = palette;
}

/* 0x20421 - play the ANI.DAT container.  See ani.h for the format. */
int ani_play(int index, int delay_ms, int wait_key)
{
    void    *sfx;                             /* FDOTHER.DAT sample bank */
    void    *palette;                         /* 768-byte RGB palette */
    void    *scratch;                         /* frame/header scratch */
    uint8_t  hdr[8];                          /* per-frame 8-byte header */
    FILE    *fp;
    int      count;
    int      i;

    sfx = NULL;
    kbd_flush();
    if (index == 1)
        sfx = res_load(aFdotherDat, NULL, 78);
    palette = guest_malloc(0x300);
    scratch = guest_malloc(0xFA00);
    ani_target_set(0xFA00, (void *)(uintptr_t)0xA0000, palette);
    fp = fopen(aAniDat, aRb_13);              /* ANI.DAT, rb */
    fseek(fp, 4L * index + 6, SEEK_SET);
    fread(scratch, 1, 8, fp);                 /* directory entry */
    fseek(fp, (long)*(uint32_t *)scratch, SEEK_SET);
    fread(scratch, 1, 0xAD, fp);              /* container header */
    count = *(const int16_t *)((const uint8_t *)scratch + 0xA5);
    for (i = 0; i < count; ++i) {
        uint16_t n;

        fread(hdr, 1, 8, fp);                 /* {data size, cmd count} */
        n = *(const uint16_t *)hdr;
        fread(scratch, 1, n, fp);
        ani_run_frame(*(const int16_t *)(hdr + 2), scratch);
        if (index == 1 && i == 0)
            svc_play_sfx(sfx, 0, 1);
        j___delay(delay_ms);
        if (wait_key != 0 && kbd_pending())
            break;
        kbd_flush();
    }
    guest_free(palette);
    guest_free(scratch);
    if (sfx != NULL) {
        svc_play_sfx(sfx, -1, 1);             /* stop the sample */
        guest_free(sfx);
    }
    fclose(fp);
    return 0;
}
