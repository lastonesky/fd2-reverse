/* fd2_native.h - de-guest (phase C) native storage for pointer-valued cells.
 *
 * The generated fd2_globals.h models every data cell as a byte lvalue inside the
 * LE segment mirror:  `#define dword_53A49 (*(uint8_t **)&fd2_data1[0x3A49])`.
 * On the 32-bit guest that is bit-exact (the original stores a 4-byte flat
 * pointer in a 4-byte slot).  On a native 64-bit build it is WRONG: the pointer
 * view reads/writes 8 bytes and the cells sit 4 bytes apart, so neighbours
 * overlap and pointers get truncated.
 *
 * This header is included at the end of fd2_globals.h (standalone profile only).
 * For cells that hold POINTERS it #undefs the mirror macro and re-declares the
 * name as a real C variable, backed by src/fd2_native.c.  Scalars keep using the
 * mirror, so the 32-bit hybrid profile (FD2_GUEST_FIXED) and its differential
 * harnesses are untouched.
 *
 * Membership is evidence-based: every name below is assigned the result of
 * guest_malloc / malloc / res_load / a *_ptr / a resource loader (or was already
 * generated as a pointer view) in the restored sources.  See PROGRESS phase C.
 */
#ifndef FD2_NATIVE_H
#define FD2_NATIVE_H

#include <stdint.h>

#ifndef FD2_GUEST_FIXED   /* native/standalone profile only */

/* --- AIL / library handles: opaque pointers ------------------------------- */
#undef  dword_53ED0
extern  void *dword_53ED0;   /* AIL sequence handle      (0x53ED0, was 4B cell) */
#undef  dword_53ED8
extern  void *dword_53ED8;   /* AIL MDI driver           (0x53ED8) */
#undef  dword_53EDC
extern  void *dword_53EDC;   /* AIL DIG driver           (0x53EDC) */
#undef  dword_53EE4
extern  void *dword_53EE4;   /* AIL sample handle 1      (0x53EE4) */
#undef  dword_53EE8
extern  void *dword_53EE8;   /* AIL sample handle 2      (0x53EE8) */

/* --- heap / resource byte buffers ---------------------------------------- */
#undef  dword_53C5B
extern  uint8_t *dword_53C5B;   /* shared 0xFA00 screen scratch (0x53C5B) */
#undef  dword_53C5F
extern  uint8_t *dword_53C5F;   /* shared 0xFA00 screen scratch (0x53C5F) */
#undef  dword_53C63
extern  uint8_t *dword_53C63;   /* shared 0xFA00 screen scratch (0x53C63) */
#undef  dword_52762
extern  uint8_t *dword_52762;   /* ANI player screen        (0x52762) */
#undef  dword_52766
extern  uint8_t *dword_52766;   /* ANI player palette       (0x52766) */
#undef  dword_53EEC
extern  uint8_t *dword_53EEC;   /* sound-effect pack        (0x53EEC) */
#undef  dword_53A4D
extern  uint8_t *dword_53A4D;   /* FDOTHER resource         (0x53A4D) */
#undef  dword_53A89
extern  uint8_t *dword_53A89;   /* FDOTHER resource         (0x53A89) */
#undef  dword_53A6D
extern  uint8_t *dword_53A6D;   /* FDOTHER resource         (0x53A6D) */
#undef  dword_53A75
extern  uint8_t *dword_53A75;   /* FDOTHER resource         (0x53A75) */
#undef  dword_53A81
extern  uint8_t *dword_53A81;   /* FDOTHER resource         (0x53A81) */
#undef  dword_53A7D
extern  uint8_t *dword_53A7D;   /* FDTXT text pool          (0x53A7D) */
#undef  dword_53AD1
extern  uint8_t *dword_53AD1;   /* FDOTHER resource         (0x53AD1) */
#undef  dword_53AD5
extern  uint8_t *dword_53AD5;   /* 0x20 work heap           (0x53AD5) */
#undef  dword_53A49
extern  uint8_t *dword_53A49;   /* 0x25680 screen scratch   (0x53A49) */
#undef  dword_53BF7
extern  uint8_t *dword_53BF7;   /* 0xA00 party record table (0x53BF7) */
#undef  dword_53A55
extern  uint8_t *dword_53A55;   /* scenario state buffer    (0x53A55) */
#undef  dword_53A61
extern  uint8_t *dword_53A61;   /* FDICON.B24 atlas         (0x53A61) */
#undef  dword_53A65
extern  uint8_t *dword_53A65;   /* FDOTHER resource         (0x53A65) */
#undef  dword_53A5D
extern  uint8_t *dword_53A5D;   /* FDSHAP screen buffer     (0x53A5D) */
#undef  dword_53A51
extern  uint8_t *dword_53A51;   /* FDFIELD resource         (0x53A51) */
#undef  dword_53A59
extern  uint8_t *dword_53A59;   /* FDFIELD resource         (0x53A59) */
#undef  dword_53A69
extern  uint8_t *dword_53A69;   /* FDSHAP resource          (0x53A69) */
#undef  dword_53A79
extern  uint8_t *dword_53A79;   /* FDTXT resource           (0x53A79) */
#undef  dword_53A45
extern  uint8_t *dword_53A45;   /* record sprite layer      (0x53A45) */
#undef  dword_53A71
extern  uint8_t *dword_53A71;   /* map cursor window save   (0x53A71) */
#undef  dword_53A85
extern  uint8_t *dword_53A85;   /* DATO dialog buffer       (0x53A85) */
#undef  dword_53B0F
extern  uint8_t *dword_53B0F;   /* FDOTHER reveal bank      (0x53B0F) */
#undef  dword_53B13
extern  uint8_t *dword_53B13;   /* FDOTHER sfx bank         (0x53B13) */
#undef  dword_53AFF
extern  uint8_t *dword_53AFF;   /* world map buffer         (0x53AFF) */
#undef  dword_53B03
extern  uint8_t *dword_53B03;   /* world map buffer 2       (0x53B03) */
#undef  dword_53C1B
extern  const uint8_t *dword_53C1B;   /* current record pointer   (0x53C1B) */
#undef  dword_53F56
extern  uint8_t *dword_53F56;   /* glyph/hotkey table ptr   (0x53F56) */
#undef  dword_53F5A
extern  uint8_t *dword_53F5A;   /* FDOTHER resource         (0x53F5A) */
#undef  dword_53F62
extern  const uint8_t *dword_53F62;   /* shop member roster       (0x53F62) */
#undef  dword_53F66
extern  uint8_t *dword_53F66;   /* FDOTHER menu resource    (0x53F66) */

/* --- figure-view BG.DAT strips ------------------------------------------- */
#undef  dword_54137
extern  uint8_t *dword_54137;   /* figure view BG source    (0x54137) */
#undef  dword_5413B
extern  uint8_t *dword_5413B;   /* figure view BG source    (0x5413B) */
#undef  dword_5414B
extern  uint8_t *dword_5414B;   /* figure view sprite       (0x5414B) */
#undef  dword_5414F
extern  uint8_t *dword_5414F;   /* figure view sprite       (0x5414F) */
#undef  dword_54153
extern  uint8_t *dword_54153;   /* figure view sprite       (0x54153) */

/* dword_5413F is an alias for the first of the three BG strips (0x5413F);
 * dword_54143/0x54147 are strips [1]/[2] of the same run.  Model it as an
 * array so `dword_5413F[i % 3]` stays valid on 64-bit. */
#undef  dword_5413F
extern  uint8_t *dword_5413F[3];

/* dword_54143 / dword_54147 are the 2nd/3rd slots of the same run; alias them
 * into the array so both spellings observe one storage. */
#ifdef dword_54143
#undef dword_54143
#endif
#define dword_54143 dword_5413F[1]
#ifdef dword_54147
#undef dword_54147
#endif
#define dword_54147 dword_5413F[2]

#endif /* !FD2_GUEST_FIXED */
#endif /* FD2_NATIVE_H */
