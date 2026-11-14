/* Fixed pitch key for AIL percussion instruments (drum notes 35..87).
 * Source: libADLMIDI ADL_Instrument.percussion_key_number of the Fat Man 2-op AIL bank. */
#ifndef FD2_OPL2_DRUMKEY_H
#define FD2_OPL2_DRUMKEY_H
#include <stdint.h>
static const uint8_t opl2_drum_key[53] = {
    35, 35, 52, 48, 58, 60, 47, 43, 49, 43, 51,
    43, 54, 57, 72, 60, 76, 84, 36, 65, 84, 83,
    84, 24, 77, 60, 65, 59, 51, 45, 71, 60, 58,
    53, 64, 71, 61, 61, 44, 40, 69, 68, 63, 74,
    60, 80, 64, 72, 73, 70, 68, 48, 53,
};
#endif
