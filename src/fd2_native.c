/* fd2_native.c - storage for the native pointer cells declared in
 * fd2_native.h (de-guest phase C).  Standalone/native profile only; the
 * hybrid FD2_GUEST_FIXED profile keeps the LE segment-mirror cells.
 *
 * All cells start NULL: the original image cannot initialise a host pointer,
 * and every consumer loads/allocates before use (boot.c and the *_run drivers
 * assign them at entry, exactly as the machine code did at 0x53xxx). */
#include "include/fd2_native.h"

#ifndef FD2_GUEST_FIXED

void *dword_53ED0;
void *dword_53ED8;
void *dword_53EDC;
void *dword_53EE4;
void *dword_53EE8;

uint8_t *dword_53C5B;
uint8_t *dword_53C5F;
uint8_t *dword_53C63;
uint8_t *dword_52762;
uint8_t *dword_52766;
uint8_t *dword_53EEC;
uint8_t *dword_53A4D;
uint8_t *dword_53A89;
uint8_t *dword_53A6D;
uint8_t *dword_53A75;
uint8_t *dword_53A81;
uint8_t *dword_53A7D;
uint8_t *dword_53AD1;
uint8_t *dword_53AD5;
uint8_t *dword_53A49;
uint8_t *dword_53BF7;
uint8_t *dword_53A55;
uint8_t *dword_53A61;
uint8_t *dword_53A65;
uint8_t *dword_53A5D;
uint8_t *dword_53A51;
uint8_t *dword_53A59;
uint8_t *dword_53A69;
uint8_t *dword_53A79;
uint8_t *dword_53A45;
uint8_t *dword_53A71;
uint8_t *dword_53A85;
uint8_t *dword_53B0F;
uint8_t *dword_53B13;
uint8_t *dword_53AFF;
uint8_t *dword_53B03;
const uint8_t *dword_53C1B;
uint8_t *dword_53F56;
uint8_t *dword_53F5A;
const uint8_t *dword_53F62;
uint8_t *dword_53F66;

uint8_t *dword_54137;
uint8_t *dword_5413B;
uint8_t *dword_5414B;
uint8_t *dword_5414F;
uint8_t *dword_54153;
uint8_t *dword_5413F[3];

#endif /* !FD2_GUEST_FIXED */
