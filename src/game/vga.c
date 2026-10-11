/* vga.c - see vga.h (de-guest phase C3). */
#include "vga.h"

#ifndef FD2_GUEST_FIXED
/* Native host frame buffer (mode 13h, 320x200). */
static uint8_t vga_store[VGA_BYTES];
uint8_t *fd2_vga = vga_store;

void vga_init(void) { }
#else
/* Hybrid/32-bit guest: the real LE-mapped VGA window. */
uint8_t *fd2_vga = (uint8_t *)0x000A0000u;

void vga_init(void) { }
#endif
