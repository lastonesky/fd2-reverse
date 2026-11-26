/* ail_timbres.h - AIL "Global Timbre Library" (SAMPLE.OPL / SAMPLE.AD) loader.
 *
 * FD2 ships the same 162-instrument bank as both SAMPLE.OPL and SAMPLE.AD.
 * MDI.INI selects SBPRO2.MDI, so the OPL3 build reads SAMPLE.OPL.  Instruments
 * are indexed by (bank, patch): bank 0 = 128 melodic programs, bank 127 = 34
 * percussion patches.  Each entry also carries a signed transposition; for
 * percussion that value *is* the fixed pitch, not an offset from the note.
 */
#ifndef FD2_AIL_TIMBRES_H
#define FD2_AIL_TIMBRES_H

#include "opl2_patches.h"

typedef struct {
    uint8_t    bank;
    uint8_t    patch;
    int8_t     trans;
    opl2_patch data;
} ail_timbre;

/* Parse a timbre file, replacing any previously loaded bank.  Returns the
 * number of instruments (0 on failure). */
int  ail_timbres_load(const char *path);
int  ail_timbres_loaded(void);
const char *ail_timbres_source(void);
const ail_timbre *ail_timbre_find(int bank, int patch);

#endif /* FD2_AIL_TIMBRES_H */
