/* map_sprites.h - map-view record/unit sprite overlays (original 0x1CB94,
 * 0x1CD17 and 0x1CAC7; the same family as map.c's 0x1C2DA/0x1C4CC
 * party-icon draws).
 *
 *   0x1CB94  map_draw_record_layer(base, count, list, frame)
 *            render the 13x8 map window into base+0x8088, then stamp the
 *            24x24 sprite of every unflagged, in-view record (table
 *            dword_53A45, stride 80).  Records named in `list` (count entries)
 *            get the transparent RLE overlay frame `frame` of the 0x53AD1
 *            animation bank instead of the solid 0x53A61 sprite selected by
 *            the record's kind byte and dword_53C0B.  One composite only; the
 *            driver alternates two frames (see 0x1CAC7).
 *
 *   0x1CD17  map_blink_units(unused, colour, count, list)
 *            snapshot the map view, then for ten frames redraw the records
 *            named in `list` with the rotating sprite24_ramp base colour
 *            byte_52006[colour], publishing each frame to 0xA0504 and waiting
 *            one BIOS tick; finally restores the snapshot and frees it.
 *
 *   0x1CAC7  map_record_layer_blink(a1, a2, count, list)
 *            composite the record layer twice - frame 74 onto dword_53A49 and
 *            frame 75 into a 0x25680 scratch buffer - then alternate the two
 *            312x192 windows onto 0xA0504 four times with a 90-unit delay per
 *            half; ends with map_view_update(0) and frees the scratch.
 *
 * ABI = cdecl, decoded from the offline disassembly (analysis/cseg01_disasm.txt).
 * 0x1CB94's tail `jmp 0x1317D` is the shared epilogue inside 0x1300D (a plain
 * return), so IDA's sub_1300D is a false callee; 0x1CD17's tail `jmp 0x1C4BC`
 * runs the guest_free of its scratch buffer and then returns.  The Watcom stack
 * probe (`push size; call 0x3702F`) is dropped.  Hand-restored, compile-verified,
 * no differential harness yet.
 */
#ifndef GAME_MAP_SPRITES_H
#define GAME_MAP_SPRITES_H

#include <stdint.h>

/* 0x1CB94 - composite the record sprites of the current map view onto `base`,
 * overlaying the records in `list` with animation frame `frame`. */
void map_draw_record_layer(uint8_t *base, int count, const uint8_t *list,
                           int frame);

/* 0x1CD17 - blink the records in `list` ten times over the map view, ramping
 * their palette through byte_52006[colour]. */
void map_blink_units(int unused, int colour, int count, const uint8_t *list);

/* 0x1CAC7 - blink the records in `list` by alternating the two record-layer
 * animation frames (74/75).  `unused1`/`unused2` are never read. */
void map_record_layer_blink(int unused1, int unused2, int count,
                            const uint8_t *list);

#endif /* GAME_MAP_SPRITES_H */
