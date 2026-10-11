/* fd2_kernel.h - native host kernel for the restored port3 engine.
 *
 * The game is plain C now (no LE loader, no guest): fd2_kernel brings up the
 * data mirrors, starts the game's main on its own thread and advances the BIOS
 * tick / fills the BDA keyboard ring the game polls.  The entry layer
 * (main_sokol.c) owns the window and calls the engine_* hooks.
 */
#ifndef FD2_HOST_KERNEL_H
#define FD2_HOST_KERNEL_H

#include <stdint.h>

/* fd2_lowmem mirror base, pointer-sized (used by the game's BDA macros). */
extern uintptr_t dos_lowmem_base;

/* One-time bring-up: fd2_data_init(), low-memory base, default palette,
 * audio device, AIL_startup.  Returns 0 on success. */
int  fd2_engine_init(int argc, char **argv);

/* Start the game thread and the BIOS tick thread. */
void fd2_engine_start(void);

/* One presented frame (fd2_vga + palette -> render_present). */
void fd2_engine_frame(void);

/* 1 once the game thread has returned (the entry layer then quits its app
 * loop instead of presenting a frozen last frame forever). */
int  fd2_engine_game_over(void);

/* Inject one keystroke into the BDA ring (scan may carry the 0x80 release
 * bit).  Called from the entry layer's key events. */
void fd2_engine_key(uint8_t scan, uint8_t ascii);

/* Patch the ASCII byte of the most recently pushed make code (sokol delivers
 * the character in a CHAR event after KEY_DOWN). */
void fd2_engine_key_set_ascii(uint8_t ascii);

/* Stop everything (game usually already returned). */
void fd2_engine_shutdown(void);

/* BDA keyboard ring, used by fd2_platform.c's INT 16h service. */
void fd2_kbd_push(uint8_t scan, uint8_t ascii);
int  fd2_kbd_pop(uint8_t *scan, uint8_t *ascii);       /* 0 = empty        */
void fd2_kbd_wait(uint8_t *scan, uint8_t *ascii);      /* blocks           */

#endif /* FD2_HOST_KERNEL_H */
