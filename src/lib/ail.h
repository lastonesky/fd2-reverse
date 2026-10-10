/* ail.h - Miles Sound System V3.02 API boundary (phase 1).
 *
 * AIL is statically linked library code (~0x37D3E..0x4D000, 51 named
 * entries + internals). Per README §5 it is treated as a LIBRARY BOUNDARY:
 * we do not hand-restore its implementation; we declare the entries the
 * game actually calls and provide adapters/stubs in phase 2.
 *
 * Signatures below come from call-site evidence in the restored callers
 * (main @0x25BF4, sub_25977 bgm, sub_25A96/25B45 sfx) cross-checked
 * against the Miles AIL 3.x API shape:
 *   AIL_install_*_INI()      -> driver handle (NULL = failed, tested)
 *   AIL_allocate_*_handle()  -> object handle bound to that driver
 * Handles are 32-bit in the original; stored in the game's dword_53Exx
 * globals, hence the uintptr_t round-trips in callers.
 */
#ifndef FD2_LIB_AIL_H
#define FD2_LIB_AIL_H

#include <stdint.h>

int32_t AIL_startup(void);                      /* 0x37D3E, result unused by main */
void    AIL_shutdown(void);                     /* 0x37ED8, main + 0x33FAF */
void   *AIL_install_DIG_INI(void);              /* 0x3908B -> HDIGDRIVER or NULL */
void   *AIL_install_MDI_INI(void);              /* 0x3AA72 -> HMDIDRIVER or NULL */
void   *AIL_allocate_sample_handle(void *dig);  /* 0x392D0 -> HSAMPLE (main x2) */
void   *AIL_allocate_sequence_handle(void *mdi);/* 0x3ACA3 -> HSEQUENCE */

/* TODO(phase 2/3): the other 10 entries the game calls (RE_MAP §3.1):
 *   AIL_init_sample, AIL_set_sample_address, AIL_set_sample_loop_count,
 *   AIL_start_sample, AIL_stop_sample, AIL_init_sequence, AIL_start_sequence,
 *   AIL_stop_sequence, AIL_set_sequence_volume, AIL_set_sequence_loop_count
 * - declare here when bgm.c/sfx.c are restored (sub_25977/sub_25A96). */

#endif /* FD2_LIB_AIL_H */
