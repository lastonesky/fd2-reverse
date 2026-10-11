/* figure_view.h - unit figure/animation view helpers (original 0x30E25,
 * 0x2F4D4, 0x30E9D) plus the attack-animation driver 0x2EBE1.
 *
 * The driver sub_2CF30 (0x2CF30) shows a party member's animated figure
 * (FIGANI.DAT) over a scrolling background (BG.DAT) with an alternating
 * portrait strip (TAI.DAT); it is restored here as figure_party_show.  Its
 * fourth callee, sub_2E95B (0x2E95B), only re-reads FD2.TMP and is restored
 * here too.
 * Two sibling helpers of the same family, used by the drivers sub_2EBE1
 * (0x2EBE1) and sub_2FB2C (0x2FB2C), are restored here as well; the attack
 * driver sub_2EBE1 and its caller, the map attack view driver sub_2E2B0
 * (0x2E2B0) with its scroll helper sub_2E9A8 (0x2E9A8), are now restored here
 * too.
 *
 *   0x30E25  figure_bg_pick    pick the BG.DAT sub-image index for the shown
 *                              unit: start from byte_52470[dword_53C03] and,
 *                              scanning `units[count-1..0]`, replace it with the
 *                              record's map-cell byte 6 whenever the unit has no
 *                              "skip" flag (rec_skip) or the running pick is 0.
 *   0x2F4D4  figure_bg_scroll  scroll the three BG.DAT strips dword_5413F[0..2]
 *                              (rotating) through the 640-stride work buffer and
 *                              publish 320x200 windows to the screen, then stamp
 *                              the figure and the status panel in the middle.
 *   0x30E9D  figure_play       play one unit's figure group frame by frame: for
 *                              every sub-sprite of `figure` draw `frame` of
 *                              `base` (or vice versa, depending on record +6),
 *                              compositing the 640-stride buffer to VGA; flash
 *                              the DAC with the mode's palette entry and wait a
 *                              tick per frame.
 *   0x2F631  figure_bg_scroll_portrait  figure_bg_scroll variant used by
 *                              sub_2EBE1: also stamps the portrait strip at
 *                              (164,157) before the status panel and figure.
 *   0x2FE14  figure_frame_run play N frames of a figure group over the cycling
 *                              BG strips into the 640-stride work buffer,
 *                              publishing one 320x200 window per frame.
 *   0x2DFC8  figure_scene_play  play one unit's FIGANI group frame by frame
 *                              over the background of its map cell with the
 *                              status panel under it, then restore the map
 *                              (called by the AI action driver 0x15055).
 *
 * ABI is cdecl; argument order and the constants were decoded from the offline
 * disassembly (analysis/cseg01_disasm.txt), which is authoritative.  Every real
 * callee is an already-restored asset: map_cell_info (0x12E38), rec_skip
 * (0x1F183), rec_sub_table5 (0x1CA89), ui_status_panel_at (0x2FACD), rle_decode
 * (0x4E98D), rle_decode_lut (0x4E8D3), res_blit (0x2EB9F), gfx_copy_rows
 * (0x11EB0), svc_play_sfx (0x25A96), svc_wait_ticks (0x17AA9), the CRT
 * outp/j___delay, plus memcpy/memset.  The Watcom stack probe (`push size; call
 * 0x3702F`) is not a callee.  Hand-restored, compile-verified, no differential
 * harness yet (unverified).
 */
#ifndef GAME_FIGURE_VIEW_H
#define GAME_FIGURE_VIEW_H

#include <stdint.h>

/* 0x30E25 - return the BG.DAT sub-image index to use for the unit list
 * `units[0..count-1]`.  `indices` are record numbers into the 80-byte table
 * dword_53A45.  Scans from the last entry down to the first. */
int figure_bg_pick(int count, const uint8_t *units);

/* 0x2F4D4 - scroll the three BG.DAT strips dword_5413F in `tmp` (a 640-stride
 * 0x1F400 buffer) and copy 320x200 windows to the screen; `screen` is the
 * 0xFA00 320-stride buffer, `bg` the current background sub-image and `figure`
 * the LMI stream stamped by res_blit.  `unit` selects the status panel. */
void figure_bg_scroll(int unit, const void *figure, void *screen, void *tmp,
                      const void *bg);

/* 0x30E9D - play the figure group `figure` against the looping group `base` for
 * record `unit`; `mode` selects the palette flash / posture branch, `tmp` is the
 * 640-stride work buffer, `screen` the 320-stride buffer, `bg`/`tai` the two
 * DAT strips drawn on a posture change.  Leaves the last return value in eax;
 * all callers ignore it. */
void figure_play(int unit, int mode, const void *figure, const void *base,
                 void *tmp, void *screen, const void *bg, const void *tai);

/* 0x2F631 - figure_bg_scroll variant that also stamps the strip `portrait` at
 * (164,157) before the status panel and the figure; `work` is the 640-stride
 * work buffer.  `unit` selects the status panel, `bg` the background strip. */
void figure_bg_scroll_portrait(int unit, const void *figure,
                               const void *portrait, void *screen, void *work,
                               const void *bg);

/* 0x2FE14 - play `frames` animation frames of the figure group `figure` over
 * the cycling BG strips into `work` (640-stride), waiting one tick per frame. */
void figure_frame_run(const void *figure, void *work, int frames);

/* 0x2EBE1 - play one attack animation between the attacker record `attacker`
 * and the defender record `defender`: stamp the attacker's figure group
 * `figure` (FIGANI.DAT) and the defender's group `base` over the 640-stride
 * `work` buffer and the 320-stride `screen`, drain the defender's HP toward the
 * map_attack_result (0x2F7B6) damage, flash the palette on the result flags and
 * wait a tick per frame.  `portrait` is the TAI.DAT strip stamped by the
 * figure_bg_scroll_portrait variant, `snd` the FDOTHER sound bank.  Returns 1
 * on the early-out of the dword_54133==1 mode, otherwise the last drain value. */
int figure_attack_play(int attacker, int defender, const uint8_t *figure,
                       const uint8_t *base, uint8_t *work, uint8_t *screen,
                       const void *portrait, int snd);

/* 0x2E95B - reload FD2.TMP into the global DATO buffer dword_53A61 (the whole
 * 0x32A00-byte file).  Called by figure_attack_view; returns the fclose
 * result. */
int figure_tmp_load(void);

/* 0x2E9A8 - slide `plate_a` across the 640-stride `work` buffer (toward
 * work+320 when record[6]==0, otherwise away from work as work+10*i) and stamp
 * the fixed `plate_b`, flatten `work` into the 320-stride `screen`, publish it
 * to VGA and fade the DAC by 6*i each step.  `portrait` is a TAI.DAT strip
 * decoded at (164,157); `flag` suppresses the fixed plate stamp. */
void figure_attack_scroll(int unit, int flag, const void *plate_a,
                          const void *plate_b, uint8_t *work, uint8_t *screen,
                          const void *portrait);

/* 0x2DFC8 - play a unit's FIGANI animation frame by frame over the BG.DAT
 * background of its map cell with the status panel drawn under it: load the
 * TAI/FIGANI/BG strips, slide the two figure plates in (figure_attack_scroll),
 * then for every frame of the +1 group stamp it over the background, publish
 * the 320x200 window and wait the frame's tick count; finally reload
 * FD2SHAP.DAT, clear the frame, refresh the map and fade back in.  The machine
 * starts the frame loop from an uninitialised EDI (see figure_view.c). */
void figure_scene_play(int unit);

/* 0x2CF30 - the driver called by the effect dispatcher 0x2FF01: show the
 * figure groups of `units[0..count-1]` over the BG.DAT / TAI.DAT strips of
 * `unit` and drain each record's HP word (+0x40) over the posture sub-sprites.
 * `mode` (18/24/28/29) selects the palette variant and the drain step count.
 * The ABI is cdecl with four arguments (the machine's `int(int)` prototype is
 * a chkstk artifact; see figure_view.c). */
void figure_party_show(int unit, int mode, int count, const uint8_t *units);

/* 0x2E2B0 - the map attack view driver between records `attacker` and
 * `defender`: load their FIGANI/TAI/BG strips, stamp the two plates, play the
 * attack animation (figure_attack_play) and, when the defender is cleared,
 * the counter animation; then reload FD2.TMP / FD2SHAP.DAT and restore the
 * map.  The demo flag dword_54133 selects the two-strip branch.  Called by
 * ev3_35730 as (0x11, arg). */
void figure_attack_view(int attacker, int defender);

#endif /* GAME_FIGURE_VIEW_H */
