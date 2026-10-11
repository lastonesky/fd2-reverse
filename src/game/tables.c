/* tables.c - generic index helpers for the obj0 table accessors.
 *
 * Translation of the eleven one-line accessors 0x4E7DD..0x4E8BC. Each is
 * base + stride*index + offset with a 32-bit unsigned multiply; 0x4E87D is
 * a dword lookup instead. See game/tables.h for the per-function constants.
 */
#include "tables.h"
#include "../include/fd2_globals.h"

void *tbl_ptr(void *base, int stride, int index, int offset)
{
    /* The original is 32-bit: base + stride*index + offset with wrap-around,
     * so a negative offset (the accessors use -31 / -64) subtracts.  On a
     * 64-bit host the raw (uint32_t)offset must be sign-extended before it is
     * added to the pointer, or -31 becomes +0xFFFFFFE1. */
    uint32_t off = (uint32_t)stride * (uint32_t)index + (uint32_t)offset;
    return (char *)base + (int32_t)off;
}

uint32_t tbl_u32(const void *base, int index)
{
    return *(const uint32_t *)((const char *)base + 4u * (uint32_t)index);
}

/* 0x4EB48 - the pointer table at 0x627D8 (one entry per argument). */
void *tbl_off627D8(int i)
{
    /* table at dseg03 0x627D8 -> mirror offset 0x27D8; its entries are
     * 4-byte GUEST addresses into dseg03 (0x62980...), so translate them to
     * the mirror instead of reinterpreting 8 bytes as a host pointer. */
    const uint8_t *tab = (const uint8_t *)&fd2_data2[0x27D8];
    uint32_t guest = *(const uint32_t *)(tab + 4u * (uint32_t)i);

    if (guest >= 0x60000u && guest < 0x64000u)
        return (void *)&fd2_data2[guest - 0x60000u];
    if (guest >= 0x50000u && guest < 0x556B0u)
        return (void *)&fd2_data1[guest - 0x50000u];
    return NULL;
}
