/* opl3_emu.h - two-bank handle API over a Nuked OPL3 (YMF262) chip.
 *
 * synth.c addresses two "OPL chips" through the classic OPL_new/OPL_writeReg/
 * OPL_calc API.  A real Sound Blaster Pro 2 / SB16 has one YMF262 with two
 * register banks (0x000 and 0x100) and OPL3 newmode enabled.  This adapter
 * keeps that split intact:
 *
 *   OPL_new() #1  -> bank 0 handle (registers 0x00..0xFF)
 *   OPL_new() #2  -> bank 1 handle (registers 0x100..0x1FF)
 *   OPL_calc()    -> one shared OPL3 render, folded to mono
 *   OPL_calcStereo() -> the same render as L/R int16
 *
 * Both handles share one opl3_chip; OPL_reset() on either resets the whole
 * chip and re-applies the SB Pro 2 OPL3 enable sequence (0x105=1, 0x104=0).
 * That is intentional: synth.c's register filter drops 0x104/0x105, so the
 * adapter owns the OPL3-mode setup.
 *
 * Nuked OPL3 is LGPL-2.1; see LICENSE.LGPL-2.1.txt in this directory.
 */
#ifndef FD2_OPL3_EMU_H
#define FD2_OPL3_EMU_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct opl3_emu OPL;

OPL *OPL_new(uint32_t clk, uint32_t rate);
void OPL_delete(OPL *opl);
void OPL_reset(OPL *opl);
void OPL_setRate(OPL *opl, uint32_t rate);
void OPL_setQuality(OPL *opl, uint8_t q);
void OPL_setChipType(OPL *opl, uint8_t type);   /* ignored: always YMF262 */
void OPL_writeIO(OPL *opl, uint32_t reg, uint8_t val);
void OPL_writeReg(OPL *opl, uint32_t reg, uint8_t val);
int16_t OPL_calc(OPL *opl);
void OPL_calcStereo(OPL *opl, int32_t out[2]);

/* Return the bank (0/1) this handle maps to. */
int OPL3EMU_bank(const OPL *opl);

/* Last rendered stereo pair (valid after OPL_calc/OPL_calcStereo). */
void OPL3EMU_last_stereo(int16_t out[2]);

#ifdef __cplusplus
}
#endif

#endif /* FD2_OPL3_EMU_H */
