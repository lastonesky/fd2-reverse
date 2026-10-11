/* map_status.h - the map-view status frame: record sprites + HP gauges drawn
 * over the live map view (original 0x1E611, its gauge helper 0x1E7F6 and the
 * anchor helper 0x1EC2A).
 *
 *   0x1E611  map_status_frame   render the 13x8 view window, draw the target
 *            (and, when present, the actor) record sprite with
 *            res_draw_subimage, overlay the 55-tile ui_meter_fixed bar and both
 *            record HP gauges (0x1E7F6), then publish the 312x192 window to VGA.
 *   0x1E7F6  map_status_gauge   place one record's HP gauge: 69*cur/max + 1
 *            segments starting 7 columns in and 6 rows down from the frame base.
 *
 * `pos` is a four-int screen-pixel block {x0, y0, x1, y1}; the first pair
 * belongs to `target`, the second to `actor`.  The layout mirrors
 * map_view_update (0x11CAC): same view offset dword_53A49+0x8088, same 456
 * stride and the same 0xA0504 publish.  Driven by 0x1548E (still pending),
 * whose 0x1EB05 helper fills the pos block via map_status_anchor (0x1EC2A).
 * Hand-restored and compile-verified; no differential harness yet.
 */
#ifndef GAME_MAP_STATUS_H
#define GAME_MAP_STATUS_H

/* 0x1E7F6 - draw record `rec`'s HP gauge at pos[0]/pos[1] below the frame base
 * `dst`; nothing is drawn while the current HP word (record+0x40) is zero. */
void map_status_gauge(uint8_t *dst, int pitch, int rec, const int *pos);

/* 0x1E611 - draw the full status frame for the actor/target record pair. */
void map_status_frame(const int *pos, int actor, int target);

/* 0x1EC2A - write the {x, y} screen anchor of record `rec` into out[0]/out[1]. */
void map_status_anchor(int *out, int rec);

/* 0x1EB05 - animate the actor/target status overlays: fill the two screen
 * anchors via map_status_anchor, then flash 10 frames of the portrait sprite
 * (res_draw_subimage sub-images 39..48) over the VGA page, each frame saved and
 * restored through dlg_snap_restore.  `actor` is drawn only when the pair
 * helper ev5_1F0DC reports a partner; returns the address of the target anchor
 * block dword_53A30 (the caller reads pos[0..3]).
 *
 * ABI note: Hex-Rays drops the second argument because the frozen prototype in
 * fd2_funcs.h says `int sub_1EB05(int)`; the machine reads [esp+0x18] (arg 2)
 * and the only caller (0x1548E) pushes both, so it is (int actor, int target). */
int *map_status_overlay_play(int actor, int target);

#endif /* GAME_MAP_STATUS_H */
