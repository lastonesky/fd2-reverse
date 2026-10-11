/* fx.c - "effect animation" handlers of the funcs_30469[] dispatch table.
 *
 * See fx.h for the family, the ABI and the coverage. Each function below is a
 * direct translation of the machine code at its stated address. Everything
 * that persists between frames lives in the original game data segment and is
 * accessed through its original address (macros), so the C reads and writes
 * exactly the words the rest of the game sees. The read-only offset tables
 * (0x525B5 / 0x5261E / 0x52646) are copied out of the original image, never
 * hardcoded.
 *
 * The services are called through their original addresses on purpose: in the
 * host src/repl.c has already patched res_blit / svc_play_sfx(2) to C, and
 * src/fxcheck.c hooks them (plus util_rand) to recording stubs - one world for
 * both sides of the differential test.
 *
 * Original addresses:
 *   0x2B996  fx_dots7   funcs_30469[0]  7 particles
 *   0x2BB33  fx_dots8   funcs_30469[1]  8 particles
 *   0x2BD6C  fx_blob    funcs_30469[2]  no particle array
 *   0x2BF83  fx_advance helper             (called only by fx_blob)
 *   0x2BFD9  fx_dots12  funcs_30469[3]  12 particles
 *   0x2C217  fx_dots6   funcs_30469[4]  6 particles
 *   0x2C441  fx_dots6b  funcs_30469[5]  6 particles
 *   0x2C67D  fx_dots5   funcs_30469[6]  5 particles on an ellipse / spinner
 *   0x2CAFC  fx_dots3   funcs_30469[7]  3 particles
 *   0x2CCF4  fx_dots16  funcs_30469[8]  16 particles
 *   0x2CE1A  fx_toggle  funcs_30469[9]  two-frame flip
 *
 * The machine code of every handler ends by jumping into one of the shared
 * return epilogues of the tail of the sub_2C67D chunk (0x2C93B xor eax,eax /
 * 0x2C93D add esp,..) or into a sibling's epilogue (0x2BB2A, 0x2C439). Those
 * are not calls: the C simply `return`s and never jumps into machine code.
 *
 * fx_advance has its own five-argument cdecl ABI
 * (phase*, counter*, dst, pitch, buf); its second table lookup is relative to
 * `buf` (`*(int32_t *)(buf + 4*phase + 8)` is a sub-offset whose +6 byte is the
 * frame count), exactly as the machine code does.
 */
#include "fx.h"

#include <stdint.h>
#include <string.h>
#include <math.h>
#include "fd2_game.h"   /* de-guest C4: restored cross-module decls */
#include "../include/fd2_globals.h"
#include "gfx.h"
#include "res.h"
#include "svc.h"

/* --- services (original entry points) ---------------------------------- */
typedef int      (*res_blit_fn)(const void *buf, int index, void *dst,
                                int pitch, int mode);
typedef int      (*sfx_fn)(const void *bank, int index, int loops);
typedef uint32_t (*rand_fn)(void);


/* --- game globals (obj1 data segment) ---------------------------------- */

/* The particle arrays are plain pointers because the original addresses them
 * with an index that can exceed the game-visible count: dots3 case 0 writes
 * four entries into three-particle arrays (dword_540CB[3] at 0x540D7 and
 * dword_540DB[3] at 0x540E7 - each array is laid out with four dwords of
 * room). Using pointers keeps that well-defined here too. */







/* Read-only offset tables, copied from their original image addresses. */
#define FX_TABLE_DOTS7_DIR  0x000524EEu  /* 7 bytes                   */
#define FX_TABLE_DOTS7_POS  0x000524F5u  /* 7 dwords,  bias +148      */
#define FX_TABLE_DOTS7_ROW  0x00052511u  /* 7 dwords                  */
#define FX_TABLE_DOTS8_POS  0x0005252Du  /* 8 dwords,  bias +148      */
#define FX_TABLE_DOTS8_ROW  0x0005254Du  /* 8 dwords                  */
#define FX_TABLE_DOTS12_V19 0x0005256Du  /* 12 dwords, bias +20       */
#define FX_TABLE_DOTS12_V21 0x0005259Du  /* 12 bytes (3 dwords)       */
#define FX_TABLE_DOTS12_V20 0x000525A9u  /* 12 bytes (3 dwords)       */
#define FX_TABLE_DOTS6  0x000525B5u   /* 10 dwords, bias +143 */
#define FX_TABLE_DOTS6B 0x000525DDu   /* 10 dwords, bias +143 */
#define FX_TABLE_DOTS3  0x0005261Eu   /* 10 dwords, bias +130 */
#define FX_TABLE_DOTS16 0x00052646u   /* 16 bytes                  */

/* Byte 6 of character record `rec` (80-byte records; 0 biases the table). */
static uint8_t fx_rec_flag(int rec)
{
    return *(const uint8_t *)(uintptr_t)
           (dword_53A45 + 80u * (uint32_t)rec + 6u);
}

/* The game's random generator is signed-idiv'd by 2 in the machine code; keep
 * the signed remainder so the translation matches for any 32-bit input. */
static int fx_rand_mod2(void)
{
    return (int)((int32_t)util_rand() % 2);
}

/* 0x2B996 - funcs_30469[0]. Seven particles driven by a 7-byte direction
 * table (0x524EE): selector 3 seeds eight phases (!), 4 blits the dir==1
 * particles and fires a sample at phase 3, 5 blits the dir==0 particles and
 * advances every particle (done at 9). Table bias +148. */
int fx_dots7(int rec, const void *buf, void *dst, int pitch, int selector)
{
    uint8_t dir[7];
    int32_t pos[7], row[7];
    int     r = 0;
    int     i;

    memcpy(dir, (const void *)(uintptr_t)FX_TABLE_DOTS7_DIR, sizeof dir);
    memcpy(pos, (const void *)(uintptr_t)FX_TABLE_DOTS7_POS, sizeof pos);
    memcpy(row, (const void *)(uintptr_t)FX_TABLE_DOTS7_ROW, sizeof row);
    if (fx_rec_flag(rec) == 0) {
        for (i = 0; i < 7; ++i)
            pos[i] += 148;
    }

    switch ((uint8_t)selector) {
    case 3:
        /* the machine writes eight entries into the seven-particle array - */
        for (i = 0; i < 8; ++i)
            dword_53F76[i] = -2 * i;
        return 28;

    case 4:
        for (i = 0; i < 7; ++i) {
            int32_t v = dword_53F76[i];

            if (v == 3)
                svc_play_sfx((const void *)(uintptr_t)dword_54153, 1, 1);
            if ((uint32_t)v < 0x10u && dir[i] == 1)
                res_blit(buf, v,
                         (void *)((intptr_t)dst + pos[i] +
                                  (intptr_t)pitch * row[i]),
                         pitch, -1);
        }
        return 0;

    case 5:
        for (i = 0; i < 7; ++i) {
            int32_t v = dword_53F76[i];

            if ((uint32_t)v < 0x10u && dir[i] == 0)
                res_blit(buf, v,
                         (void *)((intptr_t)dst + pos[i] +
                                  (intptr_t)pitch * row[i]),
                         pitch, -1);
            ++dword_53F76[i];
            if (dword_53F76[i] == 9)
                r = 1;
        }
        return r;

    default:
        return 0;
    }
}

/* 0x2BB33 - funcs_30469[1]. Eight particles; the two halves of selector 4/5
 * swap the role of the first and the last four (the last four use particle
 * (i+4)%8 for the destination and the phase+15 sub-image), and every drawn
 * pixel is biased by +80. Done at 9, sample at 5. Table bias +148. */
int fx_dots8(int rec, const void *buf, void *dst, int pitch, int selector)
{
    int32_t pos[8], row[8];
    int     r = 0;
    int     i;

    memcpy(pos, (const void *)(uintptr_t)FX_TABLE_DOTS8_POS, sizeof pos);
    memcpy(row, (const void *)(uintptr_t)FX_TABLE_DOTS8_ROW, sizeof row);
    if (fx_rec_flag(rec) == 0) {
        for (i = 0; i < 8; ++i)
            pos[i] += 148;
    }

    switch ((uint8_t)selector) {
    case 3:
        for (i = 0; i < 8; ++i)
            dword_53F92[i] = -2 * i;
        return 31;

    case 4:
        for (i = 0; i < 4; ++i) {
            int32_t v = dword_53F92[i];

            if ((uint32_t)v < 0xFu)
                res_blit(buf, v,
                         (void *)((intptr_t)dst + pos[i] +
                                  (intptr_t)pitch * row[i] + 80),
                         pitch, -1);
        }
        for (; i < 8; ++i) {
            int32_t v = dword_53F92[i];
            int     j = (i + 4) % 8;

            if ((uint32_t)v < 0xFu)
                res_blit(buf, v + 15,
                         (void *)((intptr_t)dst + pos[j] +
                                  (intptr_t)pitch * row[j] + 80),
                         pitch, -1);
        }
        return 0;

    case 5:
        for (i = 0; i < 4; ++i) {
            int32_t v = dword_53F92[i];
            int     j = (i + 4) % 8;

            if ((uint32_t)v < 0xFu)
                res_blit(buf, v + 15,
                         (void *)((intptr_t)dst + pos[j] +
                                  (intptr_t)pitch * row[j] + 80),
                         pitch, -1);
        }
        for (; i < 8; ++i) {
            int32_t v = dword_53F92[i];

            if ((uint32_t)v < 0xFu)
                res_blit(buf, v,
                         (void *)((intptr_t)dst + pos[i] +
                                  (intptr_t)pitch * row[i] + 80),
                         pitch, -1);
        }
        for (i = 0; i < 8; ++i) {
            ++dword_53F92[i];
            if (dword_53F92[i] == 9)
                r = 1;
            if (dword_53F92[i] == 5)
                svc_play_sfx((const void *)(uintptr_t)dword_54153, 1, 1);
        }
        return r;

    default:
        return 0;
    }
}

/* 0x2BF83 - blit the current frame of a sub-image strip and advance the
 * (phase, counter) pair. `buf` is both the resource base and the offset-table
 * base: the frame count of sub-image `phase` sits at
 *   *(uint8_t *)(buf + *(int32_t *)(buf + 4*phase + 8) + 6).
 * Returns that frame count. cdecl ABI, five stack arguments. */
int fx_advance(uint8_t *phase, uint8_t *counter, void *dst, int pitch,
               const void *buf)
{
    const uint8_t *b = (const uint8_t *)buf;
    int            ret;

    res_blit(buf, (int)*phase, dst, pitch, -1);
    ret = *(const uint8_t *)(b + *(const int32_t *)(b + 4 * (int)*phase + 8) + 6);
    if ((uint8_t)(++*counter) == (uint8_t)ret) {
        *counter = 0;
        ++*phase;
    }
    return ret;
}

/* 0x2BD6C - funcs_30469[2]. No particle array: three bytes of state
 * (byte_53FB2 length/phase, byte_53FB3 pad, byte_53FB4 sub-image counter).
 * record[+6]==0 biases the two res_blit destinations by one pixel and gates
 * selectors 1/7 and 2/8 on fx_advance. */
int fx_blob(int rec, const void *buf, void *dst, int pitch, int selector)
{
    int v15 = (fx_rec_flag(rec) == 0);

    switch ((uint8_t)selector) {
    case 0:
        byte_53FB2 = 0;
        byte_53FB3 = 0;
        byte_53FB4 = 0;
        return 29;

    case 3:
        byte_53FB2 = 0x10;
        return 12;

    case 6:
        svc_play_sfx((const void *)(uintptr_t)dword_54153, 3, 1);
        byte_53FB2 = 10;
        return 10;

    case 1:
    case 7:
        if (v15 != 0)
            return 0;
        if (byte_53FB2 == 10 && (uint8_t)selector == 1)
            byte_53FB2 = 15;
        fx_advance(&byte_53FB2, &byte_53FB4, dst, pitch, buf);
        return 0;

    case 2:
    case 8:
        if (byte_53FB2 == 7)
            svc_play_sfx((const void *)(uintptr_t)dword_54153, 1, 1);
        if (v15 != 0) {
            if (byte_53FB2 == 10 && (uint8_t)selector == 2)
                byte_53FB2 = 15;
            fx_advance(&byte_53FB2, &byte_53FB4, dst, pitch, buf);
        }
        if (byte_53FB2 == 0x10)
            res_blit(buf, 0x10, dst, pitch, -1);
        return 0;

    case 4:
        if (v15 == 0)
            res_blit(buf, 15, (void *)((intptr_t)dst + 1 - pitch), pitch, -1);
        return 0;

    case 5:
        if (v15 != 0)
            res_blit(buf, 15, (void *)((intptr_t)dst - 1 - pitch), pitch, -1);
        res_blit(buf, (int)byte_53FB2, (void *)((intptr_t)dst + 1 - pitch),
                 pitch, -1);
        ++byte_53FB2;
        if (byte_53FB2 == 0x11) {
            svc_play_sfx((const void *)(uintptr_t)dword_54153, 2, 1);
            return 1;
        }
        if (byte_53FB2 == 0x12)
            byte_53FB2 = 0x10;
        return 0;

    default:
        return 0;
    }
}

/* 0x2BFD9 - funcs_30469[3]. Twelve particles over three rotating slots; the
 * slot picks a destination offset (v19), a sub-image base (v20) and a vertical
 * bias (v21). Parity byte_54017 gates the advance half (even frames only);
 * done at 3 only when the slot's vertical bias is 0. Record bias +20. */
int fx_dots12(int rec, const void *buf, void *dst, int pitch, int selector)
{
    int32_t v19[12];
    uint8_t v20[12], v21[12];
    int     r = 0;
    int     i;

    memcpy(v19, (const void *)(uintptr_t)FX_TABLE_DOTS12_V19, sizeof v19);
    memcpy(v21, (const void *)(uintptr_t)FX_TABLE_DOTS12_V21, sizeof v21);
    memcpy(v20, (const void *)(uintptr_t)FX_TABLE_DOTS12_V20, sizeof v20);
    if (fx_rec_flag(rec) == 0) {
        for (i = 0; i < 12; ++i)
            v19[i] += 20;
    }

    switch ((uint8_t)selector) {
    case 0:
        for (i = 0; i < 12; ++i) {
            dword_53FB5[i] = -2 * i;
            dword_53FE5[i] = i;
        }
        byte_54015 = 12;
        byte_54016 = 0;
        byte_54017 = 0;
        return 2;

    case 3:
        return 40;
    case 6:
        byte_54016 = 1;
        return 20;

    case 2:
    case 5:
    case 8:
        byte_54017 = (uint8_t)(((uint8_t)(byte_54017 + 1)) % 2);
        for (i = 0; i < 12; ++i) {
            int32_t v    = dword_53FB5[i];
            int     slot = dword_53FE5[i];

            if ((uint32_t)v < 0xBu)
                res_blit(buf, v + v20[slot],
                         (void *)((intptr_t)dst + v19[slot] -
                                  (intptr_t)pitch * v21[slot]),
                         pitch, -1);
            if (byte_54017 == 0) {
                if (v == 0 && v20[slot] != 0)
                    svc_play_sfx((const void *)(uintptr_t)dword_54153, 2, 1);
                ++dword_53FB5[i];
                if (dword_53FB5[i] == 3) {
                    /* the machine always reports "done" here; the second
                     * sample only fires when the slot's vertical bias is 0 */
                    if (v20[slot] == 0)
                        svc_play_sfx2((const void *)(uintptr_t)dword_54153, 1, 1);
                    r = 1;
                }
                if (dword_53FB5[i] == 0xB && byte_54016 == 0) {
                    byte_54015 = (uint8_t)(((uint8_t)(byte_54015 + 1)) % 12);
                    dword_53FE5[i] = byte_54015;
                    dword_53FB5[i] = 0;
                }
            }
        }
        return r;

    default:
        return 0;
    }
}

/* 0x2C441 - funcs_30469[5]. Six particles with a per-particle direction base
 * (6*(rand%2)) and a ten-slot rotation. Same shape as fx_dots6 with a table
 * bias of +143; sample bank 1 at particle 0 and bank 2 at particle 3. */
int fx_dots6b(int rec, const void *buf, void *dst, int pitch, int selector)
{
    int32_t pos[10];
    int     r = 0;
    int     i;

    memcpy(pos, (const void *)(uintptr_t)FX_TABLE_DOTS6B, sizeof pos);
    if (fx_rec_flag(rec) == 0) {
        for (i = 0; i < 10; ++i)
            pos[i] += 143;
    }

    switch ((uint8_t)selector) {
    case 0:
        for (i = 0; i < 6; ++i) {
            dword_54050[i] = -2 * i;
            dword_54068[i] = i;
            byte_54080[i]  = (uint8_t)(6 * fx_rand_mod2());
        }
        byte_54086 = 6;
        byte_54087 = 0;
        return 1;

    case 3:
        return 12;
    case 6:
        byte_54087 = 1;
        return 8;

    case 2:
    case 5:
    case 8:
        for (i = 0; i < 6; ++i) {
            int32_t v    = dword_54050[i];
            int     slot = dword_54068[i];

            if ((uint32_t)v < 6u)
                res_blit(buf, (int)((uint8_t)byte_54080[i] + v),
                         (void *)((intptr_t)dst + pos[slot]), pitch, -1);
            if (v == 0) {
                if (i == 0)
                    svc_play_sfx((const void *)(uintptr_t)dword_54153, 1, 1);
                else if (i == 3)
                    svc_play_sfx2((const void *)(uintptr_t)dword_54153, 1, 1);
            }
            ++dword_54050[i];
            if (dword_54050[i] == 2)
                r = 1;
            if (dword_54050[i] == 7 && byte_54087 == 0) {
                byte_54086   = (uint8_t)(((uint8_t)(byte_54086 + 1)) % 10);
                dword_54068[i] = byte_54086;
                dword_54050[i] = 0;
                byte_54080[i]  = (uint8_t)(6 * fx_rand_mod2());
            }
        }
        return r;

    default:
        return 0;
    }
}

/* 0x2C217 - funcs_30469[4]. selector 0 seeds six particles, 2/5/8 advance
 * them (blit while 0 <= phase < 7, sample at phase 0, "done" once a particle
 * passes 3 and its slot rotates every 10 frames), 3/6 are timing delays. */
int fx_dots6(int rec, const void *buf, void *dst, int pitch, int selector)
{
    int32_t pos[10];
    int     r = 0;
    int     i;

    memcpy(pos, (const void *)(uintptr_t)FX_TABLE_DOTS6, sizeof pos);
    if (fx_rec_flag(rec) == 0) {
        for (i = 0; i < 10; ++i)
            pos[i] += 143;
    }

    switch ((uint8_t)selector) {
    case 0:
        for (i = 0; i < 6; ++i) {
            dword_54018[i] = -2 * i;
            dword_54030[i] = i;
            byte_54048[i]  = (uint8_t)(7 * fx_rand_mod2());
        }
        byte_5404E = (uint8_t)i;      /* 6 */
        byte_5404F = 0;
        return 2;

    case 2:
    case 5:
    case 8:
        for (i = 0; i < 6; ++i) {
            int32_t v = dword_54018[i];

            if ((uint32_t)v < 7u)
                res_blit(buf, (int)((uint8_t)byte_54048[i] + v),
                         (void *)((intptr_t)dst + pos[dword_54030[i]]),
                         pitch, -1);
            if (v == 0)
                svc_play_sfx((const void *)(uintptr_t)dword_54153, 1, 1);
            ++dword_54018[i];
            if (dword_54018[i] == 3)
                r = 1;
            if (dword_54018[i] == 8 && byte_5404F == 0) {
                byte_5404E = (uint8_t)(((uint8_t)(byte_5404E + 1)) % 10);
                dword_54030[i] = byte_5404E;
                dword_54018[i] = 0;
                byte_54048[i]  = (uint8_t)(7 * fx_rand_mod2());
            }
        }
        return r;

    case 3:
        return 12;
    case 6:
        byte_5404F = 1;
        return 8;
    default:
        return 0;
    }
}

/* 0x2CAFC - funcs_30469[7]. Same shape as fx_dots6 with three particles and
 * a phase gate (byte_540ED): the advance/sample half runs only on even
 * phases, the blit runs every phase. */
int fx_dots3(int rec, const void *buf, void *dst, int pitch, int selector)
{
    int32_t pos[10];
    int     r = 0;
    int     k;

    memcpy(pos, (const void *)(uintptr_t)FX_TABLE_DOTS3, sizeof pos);
    if (fx_rec_flag(rec) == 0) {
        for (k = 0; k < 10; ++k)
            pos[k] += 130;
    }

    switch ((uint8_t)selector) {
    case 0:
        for (k = 0; k < 4; ++k) {
            dword_540CB[k] = -3 * k;
            dword_540DB[k] = k;
        }
        byte_540EB = (uint8_t)k;      /* 4 */
        byte_540EC = 0;
        byte_540ED = 0;
        return 2;

    case 2:
    case 5:
    case 8:
        byte_540ED = (uint8_t)(((uint8_t)(byte_540ED + 1)) % 2);
        for (k = 0; k < 3; ++k) {
            int32_t v = dword_540CB[k];

            if ((uint32_t)v < 5u)
                res_blit(buf, (int)v,
                         (void *)((intptr_t)dst + pos[dword_540DB[k]]),
                         pitch, -1);
            if (byte_540ED == 0) {
                /* particle 0 uses the first handle, particle 1 the second */
                if (v == 1) {
                    if (k == 0)
                        svc_play_sfx((const void *)(uintptr_t)dword_54153, 1, 1);
                    else if (k == 1)
                        svc_play_sfx2((const void *)(uintptr_t)dword_54153, 1, 1);
                }
                if (++dword_540CB[k] == 2)
                    r = 1;
                if (dword_540CB[k] == 7 && byte_540EC == 0) {
                    byte_540EB = (uint8_t)(((uint8_t)(byte_540EB + 1)) % 10);
                    dword_540DB[k] = byte_540EB;
                    dword_540CB[k] = 0;
                }
            }
        }
        return r;

    case 3:
        return 32;
    case 6:
        byte_540EC = 1;
        return 16;
    default:
        return 0;
    }
}

/* 0x2CCF4 - funcs_30469[8]. Sixteen particles over a 16-byte direction table;
 * blit while (unsigned)phase < 8, sample at 0 and 4 ("done" at 4). */
int fx_dots16(int rec, const void *buf, void *dst, int pitch, int selector)
{
    uint8_t tbl[16];
    int     r = 0;
    int     j;

    (void)rec;
    memcpy(tbl, (const void *)(uintptr_t)FX_TABLE_DOTS16, sizeof tbl);

    switch ((uint8_t)selector) {
    case 0:
        for (j = 0; j < 16; ++j)
            dword_540EE[j] = -2 * j;
        return 3;

    case 2:
    case 5:
        for (j = 0; j < 16; ++j) {
            int32_t v = dword_540EE[j];

            if ((uint32_t)v < 8u)
                res_blit(buf, tbl[j] + v, dst, pitch, -1);
            if (v == 0)
                svc_play_sfx((const void *)(uintptr_t)dword_54153, 1, 1);
            if (v == 4)
                svc_play_sfx2((const void *)(uintptr_t)dword_54153, 2, 1);
            if (++dword_540EE[j] == 4)
                r = 1;
        }
        return r;

    case 3:
        return 34;
    case 6:
        return 2;
    default:
        return 0;
    }
}

/* 0x2CE1A - funcs_30469[9]. A single sprite that alternates between two
 * frames; selector 5 walks the sub-image index up every two frames and fires
 * a sample at the two boundaries. The return value is "still animating". */
int fx_toggle(int rec, const void *buf, void *dst, int pitch, int selector)
{
    (void)rec;

    switch ((uint8_t)selector) {
    case 0:
        byte_5412F = 0;
        byte_5412E = 1;
        return 20;

    case 1:
    case 7:
        if (byte_5412F == 0)
            res_blit(buf, 0, dst, pitch, -1);
        byte_5412F ^= 1;
        return 0;

    case 3:
        return 60;

    case 4:
        /* machine code pushes index 0 here, not 4 (Hex-Rays' 9-arg view hides it) */
        res_blit(buf, 0, dst, pitch, -1);
        return 0;

    case 5: {
        uint8_t k = byte_5412E;

        res_blit(buf, (int)(k >> 1), dst, pitch, -1);
        if (byte_5412E == 6)
            svc_play_sfx((const void *)(uintptr_t)dword_54153, 1, 1);
        else if (byte_5412E == 36)
            svc_play_sfx2((const void *)(uintptr_t)dword_54153, 2, 1);
        ++byte_5412E;
        return (byte_5412E > 0x10u && byte_5412E < 0x2Cu) ? 1 : 0;
    }

    case 6:
        return 20;
    default:
        return 0;
    }
}

/* 0x2C67D - funcs_30469[6] (1151 bytes, the only entry with CRT sin/cos).
 *
 * Two effects share the function, selected by `selector`:
 *
 *  - selectors 1/2/7/8 place five particles on an ellipse. The frame angle is
 *    72*n degrees; the radii are byte_540C9 and the base y offset is v23 (30,
 *    or 90 when record[+6]==0). Exactly as the machine code (fild/fmul/fadd
 *    with __CHP+fistp, i.e. a truncating cast):
 *        x[n] = (int)(cos(a)*byte_540C9 + v23);
 *        y[n] = (int)(sin(a)*byte_540C9*1.2 + 30.0);
 *    The two read-only tables 0x52605 (5 dwords) / 0x52619 (5 bytes) are
 *    copied out of the image; when record[+6]==0 the first three dwords are
 *    negated and the first three bytes zeroed, and v23 becomes 90.
 *    Selector 2/8 then bump byte_540C9 by +6/-6 (radius animation).
 *
 *  - selectors 0/3/6 arm the spinner: 0 resets byte_540C9/CA and returns 7,
 *    6 fires a sample and seeds byte_540C9=42, 3 (once) resets the five slot
 *    phases dword_540B0[]=-n and counters byte_540C4[]=0 and rotates the
 *    dword_54090/98 and dword_540A4/AC pair, returning 12.
 *  - selectors 4/5 are the spinner frame: slot phase picks the sub-image
 *    (clamped to 4), a secondary counter byte_540C4[] walks 0..9, and every
 *    fifth frame a sample fires (slots 0/2 use svc_play_sfx, 1/3/4 use
 *    svc_play_sfx2); the return value is 1 on the frame the phase hits 2.
 *
 * All indices derived from byte_540C9 stay in 0..4 (the byte is either 4 or a
 * slot phase 0..4), so the 5-entry local tables are not overrun.
 */
#define FX_TABLE_DOTS5_X 0x00052605u  /* 5 dwords, first three negated  */
#define FX_TABLE_DOTS5_Y 0x00052619u  /* 5 bytes,  first three zeroed   */
#define FX_DEG2RAD 0.017453277483582497  /* float 0x3C8EFA2D, pi/180     */

int fx_dots5(int rec, const void *buf, void *dst, int pitch, int selector)
{
    int32_t  v15[5];
    uint8_t  v18[5];
    uint8_t  v23;
    uint8_t  v24 = 0;
    uint8_t  v25 = fx_rec_flag(rec);
    uint8_t  sel = (uint8_t)selector;
    int32_t *b0  = (int32_t *)(void *)dword_540B0;
    int32_t *x   = (int32_t *)(void *)dword_54088;
    int32_t *y   = (int32_t *)(void *)dword_5409C;
    uint8_t *c4  = (uint8_t *)byte_540C4;
    int      n;

    memcpy(v15, (const void *)(uintptr_t)FX_TABLE_DOTS5_X, sizeof v15);
    memcpy(v18, (const void *)(uintptr_t)FX_TABLE_DOTS5_Y, sizeof v18);
    v23 = 30;
    if (v25 == 0) {
        v23 = 90;
        for (n = 0; n < 3; ++n) {
            v15[n] = -v15[n];
            v18[n] = 0;
        }
    }

    if (sel == 0) {
        svc_play_sfx((const void *)(uintptr_t)dword_54153, 2, 1);
        byte_540C9 = 0;
        byte_540CA = 0;
        return 7;
    }
    if (sel == 3) {
        if (byte_540CA == 0) {
            int32_t t;

            for (n = 0; n < 5; ++n) {
                b0[n] = -n;
                c4[n] = 0;
            }
            t = dword_54090;
            dword_54090 = dword_54098;
            dword_54098 = t;
            dword_540A4 = dword_540AC;
            dword_540AC = byte_540C9;
            byte_540CA = 1;
        }
        return 12;
    }
    if (sel == 6) {
        svc_play_sfx((const void *)(uintptr_t)dword_54153, 3, 1);
        byte_540C9 = 42;
        return 7;
    }
    if (sel == 1 || sel == 2 || sel == 7 || sel == 8) {
        for (n = 0; n < 5; ++n) {
            double a = (double)(72 * n) * FX_DEG2RAD;

            x[n] = (int)(cos(a) * (double)byte_540C9 + (double)v23);
            y[n] = (int)(sin(a) * (double)byte_540C9 * 1.2 + 30.0);
            if (v25 != 0) {
                if (((sel == 1 || sel == 7) && n < 2) ||
                    ((sel == 2 || sel == 8) && n > 1)) {
                    res_blit(buf, 4,
                             (void *)((intptr_t)dst +
                                      (intptr_t)pitch * y[n] + x[n]),
                             pitch, -1);
                }
            } else if (sel == 2 || sel == 8) {
                res_blit(buf, 4,
                         (void *)((intptr_t)dst +
                                  (intptr_t)pitch * y[n] + x[n]),
                         pitch, -1);
            }
        }
        if (sel == 2)
            byte_540C9 += 6;
        else if (sel == 8)
            byte_540C9 -= 6;
        return 0;
    }
    if (sel != 4 && sel != 5)
        return 0;

    /* selector 4/5: five-slot spinner */
    for (n = 0; ; ++n) {
        if (n >= 5) {
            if (sel == 5) {
                int m;

                for (m = 0; m < 5; ++m) {
                    if (c4[m] != 0) {
                        res_blit(buf, c4[m],
                                 (void *)((intptr_t)dst +
                                          (intptr_t)pitch * (y[m] - 20) +
                                          x[m] - 60),
                                 pitch, -1);
                        if ((uint8_t)(++c4[m]) == 10)
                            c4[m] = 0;
                    }
                    if (b0[m] == 0) {
                        if (m != 0 && m != 2)
                            svc_play_sfx2((const void *)(uintptr_t)dword_54153, 1, 1);
                        else
                            svc_play_sfx((const void *)(uintptr_t)dword_54153, 1, 1);
                    }
                    if ((uint32_t)(++b0[m]) == 5u)
                        b0[m] = 0;
                    if (b0[m] == 2)
                        v24 = 1;
                }
            }
            return v24;
        }

        {
            int idx;

            if (b0[n] < 0)
                byte_540C9 = 4;
            else
                byte_540C9 = (uint8_t)b0[n];
            idx = (uint8_t)byte_540C9;
            if (idx == 1)
                c4[n] = 5;
            if (v25 != 0) {
                if ((sel == 4 && n < 2) || (sel == 5 && n > 1)) {
                    res_blit(buf, idx,
                             (void *)((intptr_t)dst +
                                      (intptr_t)pitch * (y[n] + v18[idx]) +
                                      x[n] + v15[idx]),
                             pitch, -1);
                }
            } else if (sel == 5) {
                res_blit(buf, idx,
                         (void *)((intptr_t)dst +
                                  (intptr_t)pitch * (y[n] + v18[idx]) +
                                  x[n] + v15[idx]),
                         pitch, -1);
            }
        }
    }
}

/* 0x31266 - play effect `sel` for nine frames over a saved background.
 *
 * The second `call funcs_30469[reg*4]` site (see fx.h) and the companion of
 * sub_2FF01. The machine code runs the `sel`-th emitter twice per frame - once
 * with selector 4 and once with selector 5 - over a 640-stride work stripe
 * 18880 bytes past `work`, copying `background` into it first and publishing
 * each composed frame to VGA (0xA0000). Two res_blits stamp the caller's own
 * LMI buffers on top: `layer0` at the byte index `layer0[0] - 1`, and
 * `layer1`/`layer2` (the first/second half of the loop) at a per-frame row
 * offset. A one-tick wait paces the animation.
 *
 * ABI = cdecl, eight stack arguments, decoded from the pushes of the single
 * call site in sub_2FF01 (0x30870..0x3089d: eight pushes, `add esp,0x20`).
 * The tail `jmp 0x2FE0C` is the shared epilogue of sub_2FE14 (plain return),
 * so the result is discarded and the function returns void. The `sel == 7 ||
 * sel == 3` paths alternate the background source between the stripe and the
 * stripe minus one 640-byte row (the flip toggle kept in a byte local).
 * Hand restored against analysis/cseg01_disasm.txt; no differential harness. */
void fx_play(int rec, const void *buf, const void *layer0, const void *layer1,
             void *work, const void *background, const void *layer2, int sel)
{
    uint8_t *scratch = (uint8_t *)work + 18880;
    int idx = ((const uint8_t *)layer0)[0] - 1;
    int step = (((uint8_t *)dword_53A45)[80 * rec + 6] == 0) ? 1 : -1;
    uint8_t flip = 0;
    int i;

    for (i = 1; i < 5; ++i) {
        if (sel == 7 || sel == 3) {
            flip ^= 1;
            gfx_copy_rows(scratch - 640 * flip, 640, background, 320, 320, 200);
        } else {
            gfx_copy_rows(scratch, 640, background, 320, 320, 200);
        }
        ((int (*)(int, const void *, void *, int, int))funcs_30469[sel])(
            rec, buf, scratch, 640, 4);
        res_blit((void *)layer0, idx, scratch, 640, -1);
        res_blit((void *)layer1, 0, scratch + step * 35 * i, 640, -1);
        ((int (*)(int, const void *, void *, int, int))funcs_30469[sel])(
            rec, buf, scratch, 640, 5);
        gfx_copy_rows((void *)0xA0000, 320, scratch, 640, 320, 200);
        svc_wait_ticks(1);
    }
    for (i = 4; i >= 0; --i) {
        if (sel == 7 || sel == 3) {
            flip ^= 1;
            gfx_copy_rows(scratch - 640 * flip, 640, background, 320, 320, 200);
        } else {
            gfx_copy_rows(scratch, 640, background, 320, 320, 200);
        }
        ((int (*)(int, const void *, void *, int, int))funcs_30469[sel])(
            rec, buf, scratch, 640, 4);
        res_blit((void *)layer0, idx, scratch, 640, -1);
        res_blit((void *)layer2, 0, scratch + step * 35 * i, 640, -1);
        ((int (*)(int, const void *, void *, int, int))funcs_30469[sel])(
            rec, buf, scratch, 640, 5);
        gfx_copy_rows((void *)0xA0000, 320, scratch, 640, 320, 200);
        svc_wait_ticks(1);
    }
}
