/* map_status.h - the map-view status frame: record sprites + HP gauges drawn
 * over the live map view (original 0x1E611 and its gauge helper 0x1E7F6).
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
 * stride and the same 0xA0504 publish.  Driven by 0x1548E (still pending).
 * Hand-restored and compile-verified; no differential harness yet.
 */
#ifndef GAME_MAP_STATUS_H
#define GAME_MAP_STATUS_H

/* 0x1E7F6 - draw record `rec`'s HP gauge at pos[0]/pos[1] below the frame base
 * `dst`; nothing is drawn while the current HP word (record+0x40) is zero. */
void map_status_gauge(int dst, int pitch, int rec, const int *pos);

/* 0x1E611 - draw the full status frame for the actor/target record pair. */
void map_status_frame(const int *pos, int actor, int target);

#endif /* GAME_MAP_STATUS_H */
