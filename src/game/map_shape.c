/* map_shape.c - decode the FDSHAP-based 24x24 shape library (0x1399C).
 * See map_shape.h.  Uses sprite24_plain (0x4E22A), the guest heap
 * (0x3706E/0x3776E) and libc memset/printf/exit.
 */
#include "fd2_game.h"
#include "map_shape.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SHAPE_SPRITE_SIZE 576u  /* 24 * 24 decoded pixels per shape */

/* 0x1399C */
uint8_t *map_shape_lib_decode(void)
{
    const uint8_t *src   = dword_53A5D;
    int            count = *(const uint16_t *)(const void *)(src + 4);
    uint8_t       *lib;
    int            i;

    lib = (uint8_t *)guest_malloc(SHAPE_SPRITE_SIZE * (size_t)count + 6u);
    if (lib == NULL) {
        /* 0x139D4: push aOutOfMemoryAtR; jmp 0x10056 (printf + exit(1)). */
        printf("%s", aOutOfMemoryAtR);
        exit(1);
    }

    *(uint16_t *)(void *)(lib + 0) = 24;
    *(uint16_t *)(void *)(lib + 2) = 24;
    *(uint16_t *)(void *)(lib + 4) = (uint16_t)count;
    memset(lib + 6, 0, SHAPE_SPRITE_SIZE * (size_t)count);

    for (i = 0; i < count; ++i) {
        const uint8_t *shape =
            src + *(const uint32_t *)(const void *)(src + 4 * i + 6);

        sprite24_plain(shape, lib + SHAPE_SPRITE_SIZE * (size_t)i + 6, 24);
    }

    return lib;
}
