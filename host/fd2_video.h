/* fd2_video.h - host-side frame presentation for the native port3 engine.
 *
 * The game writes its mode-13h frame into fd2_vga (src/game/vga.h, the C3
 * de-guest seam) and drives the DAC through outp() (fd2_platform.c).  This
 * module owns the captured palette and turns (fd2_vga, palette) into the
 * 320x200 BGRA image render.h expects - the same conversion port/host.c does,
 * kept in one place so every backend sees identical pixels.
 */
#ifndef FD2_HOST_VIDEO_H
#define FD2_HOST_VIDEO_H

#include <stdint.h>

/* DAC palette, 8 bits per channel (0x3C9 values are stretched 6->8 bit). */
extern uint8_t      fd2_palette[256 * 3];
extern volatile int fd2_palette_dirty;

/* 3-3-2 ramp, used until the game writes the DAC. */
void fd2_video_default_palette(void);

/* Convert fd2_vga + fd2_palette to BGRA and render_present() one frame. */
void fd2_video_frame(void);

/* Write the last converted BGRA frame as a 32bpp BMP (--screenshot). */
void fd2_video_screenshot(const char *path);

#endif /* FD2_HOST_VIDEO_H */
