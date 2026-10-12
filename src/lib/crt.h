/* crt.h - DOS/Watcom runtime boundary still pending the engine layer.
 *
 * De-guest phase C: the restored game code used to reach these through the
 * machine-code ORIG_* callbacks (absolute addresses in the CRT region).  They
 * are not game logic; they are the OS/CRT seam that phase C3 will finish by
 * routing INT 16h etc. through the platform layer.  Declared here so the
 * call sites no longer carry raw 0x37xxx/0x370F0 addresses.
 */
#ifndef FD2_LIB_CRT_H
#define FD2_LIB_CRT_H

#include <stdint.h>

/* Watcom delay(ms): busy-wait on the BIOS tick. */
void delay(unsigned ms);

/* Watcom outp(port, value): write a byte to an I/O port. */
unsigned char fd2_outp(unsigned port, int value);

/* Watcom int386(intno, in_regs, out_regs): software interrupt with the REGS
 * frame.  INT 16h keyboard reads and INT 10h mode sets go through here. */
int int386(int intno, const void *in, void *out);

/* DOS4GW/DPMI page lock (0x3666C -> 0x365DA): a no-op on a flat native
 * host; bgm.c used it to pin a freshly loaded FDMUS buffer. */
void fd2_mem_lock(void *addr, unsigned len);

/* Blocking timing / keyboard waits (native-host service, stage 1).  The
 * restored game used to busy-wait on the BIOS tick word and on the keyboard
 * ring; these park the thread on the host's monotonic-clock tick and key
 * queue instead.  They keep the same observable behaviour (a tick loop still
 * runs at 18.2 Hz) without spinning a core. */
void fd2_wait_tick(void);           /* until the BIOS tick word advances  */
void fd2_wait_tick_or_key(void);    /* ... or a key becomes pending       */
void fd2_kbd_flush(void);           /* BIOS 0x4E381 (tail = head)         */

#endif /* FD2_LIB_CRT_H */
