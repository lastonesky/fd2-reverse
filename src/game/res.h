/* res.h - resource loader (original 0x111BA) - hand-restored, phase 1.
 *
 * Loads one resource out of a "DAT/LMI" container:
 *   [0..5]  6-byte file header
 *   [6..]   (count+1) x uint32 little-endian offsets, entry i spans
 *           [off[i], off[i+1])
 *
 * Evidence: disassembly of 0x111BA (analysis/decomp/111BA_sub_111BA.c and
 * the full listing in analysis/) + RE_MAP.md §4 (132 call sites, all
 * `push index; push oldbuf; push filename; call; add esp,0Ch`).
 */
#ifndef FD2_GAME_RES_H
#define FD2_GAME_RES_H

#include <stddef.h>

/* Returns a freshly malloc()ed buffer with resource `index` of `filename`.
 * If `oldbuf` != NULL it is free()d first (callers pass the previous buffer
 * to get "reload in place" semantics - main() relies on this).
 * Error paths print and exit(1) exactly like the original
 * (shared tail loc_1005E: push 1; jmp exit). */
void *res_load(const char *filename, void *oldbuf, int index);

/* Call sites in not-yet-restored modules keep the IDA name. */
#define sub_111BA res_load

#endif /* FD2_GAME_RES_H */
