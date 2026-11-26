/* ail_timbres.c - minimal reader for the Miles "Global Timbre Library" file.
 *
 * Byte layout (verified against FD2's SAMPLE.OPL, and matching ScummVM's
 * audio/miles_adlib.cpp):
 *
 *   repeat: [patch:u8][bank:u8][instrument_offset:u32 LE]
 *   until patch == bank == 0xFF
 *
 *   at instrument_offset:
 *     [size:u16 LE]      must be 14
 *     [transposition:i8]
 *     [r20op1][r40op1][r60op1][r80op1][rE0op1]
 *     [rC0]
 *     [r20op2][r40op2][r60op2][r80op2][rE0op2]
 *
 * op1 is the modulator, op2 is the carrier.  synth.c's opl2_patch keeps the
 * carrier in op[0] and the modulator in op[1], so the mapping is reversed
 * here on purpose.
 */
#include "ail_timbres.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AIL_MAX_TIMBRES 256

static ail_timbre g_timbres[AIL_MAX_TIMBRES];
static int        g_count;
static char       g_source[260];

static uint16_t rd16le(const uint8_t *p)
{
    return (uint16_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8));
}

static uint32_t rd32le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int ail_timbres_load(const char *path)
{
    FILE       *f;
    long        size;
    uint8_t    *data;
    uint32_t    i = 0;
    int         n = 0;

    if (!path || !*path)
        return 0;
    f = fopen(path, "rb");
    if (!f)
        return 0;
    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) <= 6 ||
        fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return 0;
    }
    data = (uint8_t *)malloc((size_t)size);
    if (!data || fread(data, 1, (size_t)size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return 0;
    }
    fclose(f);

    g_count = 0;
    while (i + 6 <= (uint32_t)size) {
        uint8_t  patch = data[i];
        uint8_t  bank  = data[i + 1];
        uint32_t off   = rd32le(data + i + 2);
        ail_timbre *t;

        i += 6;
        if (patch == 0xFF && bank == 0xFF)
            break;
        if (off + 14 > (uint32_t)size || rd16le(data + off) != 14)
            continue;
        if (g_count >= AIL_MAX_TIMBRES)
            break;

        t = &g_timbres[g_count++];
        t->bank  = bank;
        t->patch = patch;
        t->trans = (int8_t)data[off + 2];

        /* carrier = AIL op2 (offset +9..+13) */
        t->data.op[0].r20 = data[off + 9];
        t->data.op[0].r40 = data[off + 10];
        t->data.op[0].r60 = data[off + 11];
        t->data.op[0].r80 = data[off + 12];
        t->data.op[0].rE0 = data[off + 13];

        /* modulator = AIL op1 (offset +3..+7) */
        t->data.op[1].r20 = data[off + 3];
        t->data.op[1].r40 = data[off + 4];
        t->data.op[1].r60 = data[off + 5];
        t->data.op[1].r80 = data[off + 6];
        t->data.op[1].rE0 = data[off + 7];

        t->data.rC0 = data[off + 8];
    }
    free(data);
    n = g_count;

    if (n) {
        snprintf(g_source, sizeof g_source, "%s", path);
        printf("ail_timbres: %d instruments from %s\n", n, path);
    } else {
        g_source[0] = 0;
    }
    return n;
}

int ail_timbres_loaded(void)
{
    return g_count > 0;
}

const char *ail_timbres_source(void)
{
    return g_source;
}

const ail_timbre *ail_timbre_find(int bank, int patch)
{
    int k;

    for (k = 0; k < g_count; ++k) {
        if ((int)g_timbres[k].bank == bank &&
            (int)g_timbres[k].patch == patch)
            return &g_timbres[k];
    }
    return NULL;
}
