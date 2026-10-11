/* map_shape.h - decode the FDSHAP-based 24x24 shape library into raw bitmaps
 * (original 0x1399C).
 *
 *   0x1399C  map_shape_lib_decode
 *            Read the current dword_53A5D sprite library - a 6-byte header
 *            {u16 24, u16 24, u16 count} followed by `count` u32 stream
 *            offsets and the RLE streams - allocate a library of header + raw
 *            24x24 pixel blocks, decode every stream into its 576-byte slot
 *            with sprite24_plain, and return the new buffer.  The callers
 *            (0x2000A, 0x21548, both still pending) save the old pointer,
 *            store the result into dword_53A5D, and sample the raw blocks
 *            through the 64x64 pointer table consumed by map_zoom_draw
 *             (0x1F558).
 *
 * Hand-restored from the offline disassembly (analysis/cseg01_disasm.txt,
 * 0x1399C..0x13A43).  The Watcom stack probe (`push 0x20; call 0x3702F`) is
 * dropped; the out-of-memory path is the shared `printf; exit(1)` tail at
 * 0x10056 (note it does NOT take scenario_load's int386 text-mode switch, which
 * sits earlier at 0x10034); the tail `jmp 0x10C49` is the shared epilogue that
 * returns the new pointer.  Compile-verified, no differential harness yet.
 */
#ifndef GAME_MAP_SHAPE_H
#define GAME_MAP_SHAPE_H

#include <stdint.h>

/* 0x1399C - decode dword_53A5D (RLE 24x24 shape library) into a new raw
 * 24x24 bitmap library; returns the guest-heap buffer. */
uint8_t *map_shape_lib_decode(void);

#endif /* GAME_MAP_SHAPE_H */
