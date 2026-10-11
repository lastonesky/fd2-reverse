/* vga.h - the one seam onto the mode-13h frame buffer (de-guest phase C3).
 *
 * The original addresses the 320x200 VGA window as a flat linear address in
 * 0xA0000..0xAF9FF; every restored module used to cast 0xA0000 / 0xA0504 /
 * 0xA951F ... to a pointer.  On a native host there is no such mapping, so all
 * of that goes through this pointer: native builds point it at a host array,
 * the 32-bit hybrid profile points it at the real guest window.
 *
 * Addresses stay addresses in the *original* arithmetic (the VM threads a
 * linear address through its recursion), so VGA_P() converts a guest linear
 * address to the host pointer.
 */
#ifndef GAME_VGA_H
#define GAME_VGA_H

#include <stdint.h>

extern uint8_t *fd2_vga;                 /* mode-13h base (0xA0000 window) */

#define VGA_BYTES 0xFA00u                /* 320 * 200                    */

/* guest linear address -> host pointer */
#define VGA_P(addr) (fd2_vga + ((uint32_t)(addr) - 0xA0000u))

void vga_init(void);

#endif /* GAME_VGA_H */
