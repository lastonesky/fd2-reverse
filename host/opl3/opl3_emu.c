/* opl3_emu.c - see opl3_emu.h.  One YMF262 behind two bank handles. */

#include "opl3_emu.h"
#include "nukedopl3.h"

#include <stdlib.h>
#include <string.h>

struct opl3_emu {
    int bank;                   /* 0 = register bank 0, 1 = bank 1 */
};

static opl3_chip g_chip;
static int       g_created;     /* OPL_new() calls so far */
static uint32_t  g_rate = 49716;
static int16_t   g_last[2];
static int       g_stereo_valid;

/* SB Pro 2 / SB16 initialisation that synth.c cannot emit: its register
 * filter drops 0x04/0x05 on both banks.  On an OPL3 those are 4-op mode and
 * newmode, so the adapter owns them. */
static void apply_sbpro2_init(void)
{
    OPL3_WriteReg(&g_chip, 0x105, 1);   /* newmode: OPL3 features enabled */
    OPL3_WriteReg(&g_chip, 0x104, 0);   /* all 18 channels are 2-op       */
    OPL3_WriteReg(&g_chip, 0x008, 0);   /* CSM / note-select off          */
    OPL3_WriteReg(&g_chip, 0x0BD, 0xC0);/* rhythm off, deep AM + VIB      */
}

OPL *OPL_new(uint32_t clk, uint32_t rate)
{
    OPL *o = (OPL *)calloc(1, sizeof(*o));
    if (!o)
        return NULL;
    o->bank = g_created & 1;
    if (g_created == 0) {
        g_rate = rate ? rate : 49716;
        memset(&g_chip, 0, sizeof(g_chip));
        OPL3_Reset(&g_chip, g_rate);
        apply_sbpro2_init();
    }
    ++g_created;
    (void)clk;                  /* YMF262 clock is fixed at 3579545 Hz */
    return o;
}

void OPL_delete(OPL *opl)
{
    if (!opl)
        return;
    if (g_created > 0)
        --g_created;
    free(opl);
}

void OPL_reset(OPL *opl)
{
    (void)opl;
    OPL3_Reset(&g_chip, g_rate);
    apply_sbpro2_init();
    g_last[0] = g_last[1] = 0;
    g_stereo_valid = 0;
}

void OPL_setRate(OPL *opl, uint32_t rate)
{
    (void)opl;
    if (rate) {
        g_rate = rate;
        OPL3_Reset(&g_chip, g_rate);
        apply_sbpro2_init();
    }
}

void OPL_setQuality(OPL *opl, uint8_t q)
{
    (void)opl; (void)q;
}

void OPL_setChipType(OPL *opl, uint8_t type)
{
    (void)opl; (void)type;      /* this adapter is always a YMF262 */
}

void OPL_writeIO(OPL *opl, uint32_t reg, uint8_t val)
{
    OPL_writeReg(opl, reg, val);
}

void OPL_writeReg(OPL *opl, uint32_t reg, uint8_t val)
{
    if (!opl)
        return;
    OPL3_WriteReg(&g_chip,
                  (uint16_t)((opl->bank ? 0x100u : 0u) | (reg & 0xFFu)),
                  val);
}

void OPL_calcStereo(OPL *opl, int32_t out[2])
{
    if (opl && opl->bank == 0) {
        OPL3_Generate(&g_chip, g_last);
        g_stereo_valid = 1;
    }
    out[0] = g_last[0];
    out[1] = g_last[1];
}

int16_t OPL_calc(OPL *opl)
{
    if (opl && opl->bank == 0) {
        OPL3_Generate(&g_chip, g_last);
        g_stereo_valid = 1;
        {
            int v = ((int)g_last[0] + (int)g_last[1]) >> 1;
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            return (int16_t)v;
        }
    }
    return 0;
}

int OPL3EMU_bank(const OPL *opl)
{
    return opl ? opl->bank : 0;
}

void OPL3EMU_last_stereo(int16_t out[2])
{
    out[0] = g_stereo_valid ? g_last[0] : 0;
    out[1] = g_stereo_valid ? g_last[1] : 0;
}
