/* fd2_video.h - host-side frame presentation for the native port3 engine.
 *
 * The game writes its mode-13h frame into fd2_vga (src/game/vga.h, the C3
 * de-guest seam) and drives the DAC through outp() (fd2_platform.c).  This
 * module owns the captured palette and turns (fd2_vga, palette) into the
 * 320x200 BGRA image render.h expects - the same conversion port/host.c does,
 * kept in one place so every backend sees identical pixels.
 *
 * Stage 2 (double buffering): the game thread writes fd2_vga / fd2_palette
 * directly, and only the game thread touches them; the presenter reads a
 * `front` copy instead.  The game thread calls fd2_video_publish() whenever
 * it is about to stop drawing (parking on a tick/key, or delay()), so the
 * published frame is always a complete, stable one.  The front copy and the
 * palette are handed over under a mutex, which removes the old game-thread
 * write / render-thread read race (tearing + mid-fade palette).
 */
#ifndef FD2_HOST_VIDEO_H
#define FD2_HOST_VIDEO_H

#include <stdint.h>

/* DAC palette, 8 bits per channel (0x3C9 values are stretched 6->8 bit). */
extern uint8_t      fd2_palette[256 * 3];
extern volatile int fd2_palette_dirty;

/* 3-3-2 ramp, used until the game writes the DAC. */
void fd2_video_default_palette(void);

/* One-time bring-up: create the front-buffer handover lock and publish the
 * initial (blank) frame.  Call after fd2_video_default_palette(), before the
 * game thread starts. */
void fd2_video_init(void);

/* Game thread: copy fd2_vga + fd2_palette to the front buffer under the
 * handover lock.  Safe to call only from the game thread (it is the writer of
 * fd2_vga / fd2_palette). */
void fd2_video_publish(void);

/* Presenter: convert the published front frame to BGRA and render_present(). */
void fd2_video_frame(void);

/* Write the last converted BGRA frame as a 32bpp BMP (--screenshot). */
void fd2_video_screenshot(const char *path);

/* Monotonic count of published frames: the game thread's liveness heartbeat
 * (it publishes whenever it finishes a frame and stops drawing).  The hang
 * watchdog uses it in addition to fd2_game_progress. */
extern volatile unsigned fd2_video_publish_count;

#endif /* FD2_HOST_VIDEO_H */
