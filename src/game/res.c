/* res.c - resource loader - hand-restored from 0x111BA (phase 1, static only).
 *
 * Original: int __cdecl sub_111BA(int name, int p, int a3)   [235 bytes]
 * Real ABI: cdecl, 3 stack args (callers do `add esp,0Ch`), returns a pointer
 * (eax), which on the 32-bit target is interchangeable with int - we restore
 * the honest types here (see fd2_funcs.h for the raw positional prototype).
 *
 * Translation notes (every decision traceable to bytes):
 *  - `push 20h; call 0x3702F` prologue is the Watcom stack probe, NOT an
 *    argument - dropped (0x3702F = __chkstk thunk, `retn 4`).
 *  - `shl eax,2; add eax,6` = offset-table slot `4*index + 6`, computed in
 *    32-bit registers - kept as uint32_t so wrap-around inputs behave the
 *    same as the machine code.
 *  - the 8 bytes at that slot are two uint32 offsets: [start, end),
 *    resource size = end - start (read into a 2-word array; the original
 *    mallocs 8 bytes, reads, copies both words, then frees).
 *  - 0x373CA is Watcom's stdio read core used as fread(buf, 1, n, fp)
 *    (both call sites: fseek first, then read) -> libc fread.
 *  - both error paths end in `jmp loc_1005E` = the shared fatal tail
 *    `push 1; jmp exit` (mode-3 restore + message happen at the printf
 *    sites we already translated) -> exit(1).
 *  - free(oldbuf) happens BEFORE fopen: even a failing reload frees the old
 *    buffer. Kept in that order (callers depend on it).
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "res.h"

void *res_load(const char *filename, void *oldbuf, int index)
{
    FILE *fp;
    uint32_t offs[2];
    uint32_t start, end, n;
    void *buf;

    if (oldbuf != NULL)
        free(oldbuf);                       /* 0x111CF */

    fp = fopen(filename, "rb");             /* 0x111E1 */
    if (fp == NULL) {
        printf("\n\n File not found %s!!! \n\n", filename);   /* 0x111F8, 2 args */
        exit(1);                            /* 0x11200 -> loc_1005E */
    }

    /* slot = 4*index + 6 (uint32 math, 0x11213..0x1121A) */
    fseek(fp, (uint32_t)index * 4u + 6u, SEEK_SET);           /* 0x1121F */
    if (fread(offs, 1, 8, fp) != 8) {                         /* 0x1122D = 0x373CA */
        /* original ignores the read result - kept as-is */
    }
    start = offs[0];                        /* mov edi,[ebx]  0x11235 */
    end   = offs[1];                        /* mov eax,[ebx+4] 0x11237 */
    n = end - start;                        /* sub eax,edi     0x1123A */
    /* original mallocs 8 bytes for the pair, copies, frees (0x11205..0x11247) */

    buf = malloc(n);                        /* 0x11250 */
    if (buf == NULL) {
        printf("Out of Memory at Load %s Number:%d!!\n", filename, index);  /* 3 args */
        exit(1);                            /* 0x11273 -> loc_1005E */
    }

    fseek(fp, start, SEEK_SET);             /* 0x1127C */
    fread(buf, 1, n, fp);                   /* 0x1128E = 0x373CA */
    fclose(fp);                             /* 0x11297 */
    return buf;                             /* 0x1129F eax=ebx */
}
