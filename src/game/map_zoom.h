/* map_zoom.h - the map zoom-in/out transition (originals 0x1F558 / 0x2000A).
 *
 *   0x1F558  map_zoom_draw   one animation frame: build the 64x64 table of
 *            source base pointers over the dword_53A5D work buffer and render
 *            the 312x192 window at a lerped fixed-point centre and zoom.
 *   0x2000A  map_zoom_run    the driver: decode the shape library, play the
 *            seven zoom-in frames, hold the zoomed view until a key (stepping
 *            the record-marker phase), then the six zoom-out frames.  Called
 *            from the scene event tick; the second driver, 0x21548, lives in
 *            map_action.c (map_action_zoom_resolve).
 */
#ifndef GAME_MAP_ZOOM_H
#define GAME_MAP_ZOOM_H

#include <stdint.h>

/* 0x1F558 - render the 312x192 map window into dword_53A49+0x504 at
 * `zoom`/128 scale, centred on the fixed-point point (x, y).  `cells` is the
 * caller's row-major table of 32-bit source base pointers into the
 * dword_53A5D buffer, indexed cells[row*64 + col]. */
void map_zoom_draw(int x, int y, int zoom, uint8_t *const *cells);

/* 0x2000A - run the whole zoom-in / hold / zoom-out transition; returns 0. */
int map_zoom_run(void);

#endif /* GAME_MAP_ZOOM_H */
