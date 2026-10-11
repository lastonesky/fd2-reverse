/* item_use.h - item / healing use effects (original 0x13FD4 .. 0x1DA16).
 *
 * The routines that apply a healing item to a party record, restore its
 * magic points, or play its short in-view animation:
 *
 *   0x1C916  unit_heal            add `amount` HP (90% fixed + up to 10%
 *            random, clamped to the record's max at +66) and book the
 *            experience in dword_53EC8; returns the amount added.
 *   0x1C8ED  unit_heal_by_action  the same, with `amount` taken from word 0
 *            of the tbl_619FD entry of the action.
 *   0x1C75E  unit_action_damage   resolve action `action` on a record: word 0
 *            of the action's tbl_619FD entry scaled by record+32's tbl_51F96
 *            power over 10, applied with the entry's chance byte (action
 *            10..12 dropped on a skipped record); returns the damage dealt.
 *   0x1C81F  unit_damage         deal `amount` damage to a record (90% fixed
 *            + up to 10% random, clamped at zero) and book the experience in
 *            dword_53EC8; returns the damage dealt.
 *   0x1C9DD  unit_restore_mp      the magic-point analogue of unit_heal on
 *            the record's second pool (+68, max at +70).
 *   0x1DA16  view_draw_unit_sprite  blit a unit's 24x24 sprite into the map
 *            bitmap at its cell (direction from record+3, frame from +2 and
 *            the animation phase dword_53C07/0x53C0B); mode 0 = plain,
 *            mode 2 = constant-colour.
 *   0x22AF6  item_apply           run the party-icon draw, then for every
 *            record in `list` heal 10 HP (or enqueue its cursor sprite when
 *            the item field is empty), and refresh the view.
 *   0x13FD4  item_use             the driver: glide the portrait, flash the
 *            sprite, wait, then raise the record's HP by max/5.
 *
 * ABI = cdecl.  Record pointers index the 80-byte table dword_53A45.  Hand
 * restored, compile-verified, no differential harness yet.  (The machine
 * passes a fourth argument to 0x4E127 that the callee never reads; the
 * verified sprite24_const takes three, so the extra argument is dropped.)
 */
#ifndef GAME_ITEM_USE_H
#define GAME_ITEM_USE_H

/* 0x1C916 */
int unit_heal(int rec, int amount);

/* 0x1C8ED */
int unit_heal_by_action(int rec, int action);

/* 0x1C9DD */
int unit_restore_mp(int rec, int amount);

/* 0x1C75E */
int unit_action_damage(int rec, int action);

/* 0x1C81F */
int unit_damage(int rec, int amount);

/* 0x1DA16 */
int view_draw_unit_sprite(uint8_t *dst, int pitch, int rec, int mode, int field);

/* 0x22AF6 */
int item_apply(int a1, int a2, int count, const uint8_t *list, int field);

/* 0x13FD4 */
int item_use(int rec);

#endif /* GAME_ITEM_USE_H */
