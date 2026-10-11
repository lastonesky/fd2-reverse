/* particle.h - random-offset particle placement helper (original 0x21DB2).
 *
 * 0x21DB2 is the per-slot emitter of the smoke-plume animator 0x21BD0 (its
 * only caller): for particle slot `index` it draws one particle from the
 * shared util_rand stream and writes its screen position (16-bit word arrays
 * `x`/`y`) and a 1..8 sub-image selector (`kind`), offset from the centre
 * (cx, cy) with a random angle and a radius scaled by `radius`:
 *
 *   r          = ((rand() % 64) * radius) / 64 - 1      (signed idiv)
 *   angle      = (double)(rand() % 360) * dbl_501F8      (deg -> rad)
 *   x[index]   = (int)(cos(angle) * r + cx)
 *   y[index]   = (int)(sin(angle) * r + cy + dbl_50200)  (dbl_50200 = -8.0)
 *   kind[index]= rand() % 8 + 1
 *
 * The three rand() draws are taken radius-first, angle-second, kind-third; the
 * caller depends on that order in the shared stream.  The return value is the
 * kind byte (the machine leaves it in AL); the caller ignores it.
 */
#ifndef GAME_PARTICLE_H
#define GAME_PARTICLE_H

#include <stdint.h>

/* 0x21DB2 */
uint8_t particle_spawn(int radius, unsigned index, int16_t *x, int16_t *y,
                       uint8_t *kind, int cx, int cy);

#endif /* GAME_PARTICLE_H */
