/* map_zoom.h - one frame of the map zoom-in/out transition (original 0x1F558).
 *
 * The transition drivers (0x2000A and 0x21548, both still pending) build a
 * 64x64 table of source base pointers over the dword_53A5D work buffer, then
 * call this once per animation step with a lerped fixed-point centre and a
 * growing zoom, memmove-ing the result to the VGA frame.
 */
#ifndef GAME_MAP_ZOOM_H
#define GAME_MAP_ZOOM_H

#include <stdint.h>

/* 0x1F558 - render the 312x192 map window into dword_53A49+0x504 at
 * `zoom`/128 scale, centred on the fixed-point point (x, y).  `cells` is the
 * caller's row-major table of 32-bit source base pointers into the
 * dword_53A5D buffer, indexed cells[row*64 + col]. */
void map_zoom_draw(int x, int y, int zoom, const uint32_t *cells);

#endif /* GAME_MAP_ZOOM_H */
