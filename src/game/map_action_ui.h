/* map_action_ui.h - interactive per-record map action driver (original 0x1BBDC).
 *
 *   0x1BBDC  map_action_interact   the cursor-driven "act on a record" menu.
 *            Called by the record-interaction driver 0x18D8C with a record
 *            index.  It opens the 4x4 direction patch (map_cursor_* over the
 *            copied tile tables at 0x51F05/0x53F32) to pick a facing, then
 *            branches on the selected action slot dword_53C57:
 *
 *              slot 0  loop: pop the item/stat box for the actor (box_popup),
 *                      flood-mark the reachable cells of the held item
 *                      (map_reveal_reachable), run the record picker
 *                      (map_pick_record); for item kind 23 additionally
 *                      require an intact record and confirm the cell, then
 *                      hand the choice to the action router (map_action_router)
 *                      and flag the actor (rec_flag_or80); returns 1 on a
 *                      confirmed action, 0 otherwise.
 *              slot 1  a party reorder: pick a record to swap (drawn into a
 *                      0x64 heap buffer), find its portrait, slide the view
 *                      back and splice the two records' equipment slots
 *                      (rec_slot_claim/remove); sets dword_53C53.
 *              slot 2  delegate to the item sub-menu (box_run).
 *              other   pop the box purely to drop one slot (rec_slot_remove).
 *
 * ABI = cdecl, one int argument (the actor record index).  Uses rec_slot_free,
 * map_reveal_reachable, util_fix_records, ev5_173E7, the map_cursor patch
 * primitives, map_view_update, box_popup, box_run, map_pick_record,
 * map_action_router, dlg_portrait_find/glide, map_slide_view, rec_field_byte,
 * rec_slot_claim/remove, rec_flag_or80, unit_metrics and guest_malloc/free.
 * Hand-restored, compile-verified, no differential harness yet.
 */
#ifndef GAME_MAP_ACTION_UI_H
#define GAME_MAP_ACTION_UI_H

/* 0x1BBDC */
int map_action_interact(int actor);

#endif /* GAME_MAP_ACTION_UI_H */
