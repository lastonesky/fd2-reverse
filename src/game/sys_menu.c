/* sys_menu.c - in-game menu cluster (original 0x16F55, 0x1728C, 0x19DF7).
 * See sys_menu.h for the semantic overview.
 *
 * Direct translation of the machine code at 0x16F55, 0x1728C and 0x19DF7; the
 * offline disassembly (analysis/cseg01_disasm.txt) is authoritative.
 *
 * Notes / decompiler artifacts:
 *   - the Watcom stack probes (`push 0x54/0x3C; call 0x3702F`) allocate no frame
 *     and are dropped;
 *   - all three share ONE frame layout (`push ebx/esi/edi; sub esp,20h`), so
 *     0x1728C/0x19DF7 tail-jump into 0x16F55's epilogue at 0x16FDD (a plain
 *     `return`) and 0x19DF7's default path jumps into 0x16F55's body at 0x1716F.
 *     In C that shared "cancelled" frame is the helper sys_menu_cancel_frame()
 *     (VM sub-stream 412 + 200 ms + ev4_196CB) and the callers return 1;
 *   - the 16-byte locals are `rep movs` copies of the cursor tables at
 *     0x51E9F/0x53EF2 (0x16F55), 0x51EAF/0x53F02 (0x1728C) and 0x51EF5/0x53F22
 *     (0x19DF7) - all in dseg02 (fd2_data1);
 *   - Hex-Rays loses the second argument of 0x177FC at 0x16FA9 / 0x17334 /
 *     0x19EC6 (the machine loads ebx = [esp+0xc]); the call sites push both
 *     4-int arrays, hence map_cursor_dir_select(a_tiles, b_tiles);
 *   - 19DF7's `v20 = v0; v19 = v2; v18 = v1` prologue is a read of the picker
 *     helper's leftover stack slots, not inputs - dropped;
 *   - 19DF7's probe of FD2.SAV reads the file into a fresh 0x59CB buffer and
 *     frees it without using the bytes (only the open result matters); the
 *     save path preserves the un-modelled tail (0x30D5..0x59CB) of the existing
 *     file, but leaves it uninitialised when the file is absent - faithfully
 *     reproduced.
 */
#include "fd2_game.h"
#include "sys_menu.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* 0x3B124 - AIL_set_sequence_volume(handle, volume, ms).  The AIL entry is
 * called at its original address, exactly as src/game/bgm.c / svc.c do. */
typedef void (*ail_vol_fn)(void *h, int32_t volume, int32_t ms);
#define AIL_SEQ_VOLUME ((ail_vol_fn)(uintptr_t)0x0003B124u)

/* The shared "cancelled" dialogue frame living at 0x1716F inside 0x16F55's
 * body (VM sub-stream 412, a 200 ms pause, portrait close).  0x16F55's own
 * case-1/3 abort paths and 0x19DF7's default path all end here; only 0x19DF7's
 * case-1/2 abort paths follow it with kbd_flush. */
static void sys_menu_cancel_frame(void)
{
    vm_run(dword_53A7D, 412, 0xAB6E3, 320, 205, 76, 74, 19, 1);
    j___delay(200);
    ev4_196CB();
}

/* 0x16F55 */
int sys_menu_run(void)
{
    int      a_tiles[4];             /* 0x51E9F copy (patch arg 1)   */
    int      b_tiles[4];             /* 0x53EF2 copy (patch arg 2)   */
    uint8_t *rec0;
    uint64_t saved;
    int      v0, v2, i;

    memcpy(a_tiles, &fd2_data1[0x1E9F], sizeof a_tiles);
    memcpy(b_tiles, &fd2_data1[0x3EF2], sizeof b_tiles);
    dword_53C57 = 0;

    map_cursor_patch_expand(a_tiles, b_tiles);
    do {
        v0 = map_cursor_dir_select(a_tiles, b_tiles);
    } while (v0 == 0);
    map_cursor_patch_collapse(a_tiles, b_tiles);
    map_view_update(0);

    if (v0 == -1)
        return 1;

    switch ((int)dword_53C57) {
    case 0:
        return sys_menu_system();

    case 1:
        rec0 = (uint8_t *)dword_53A45;
        msg_open_portrait(rec0[7]);
        vm_run(dword_53A7D, 417, 0xA9F23, 320, 205, 76, 74, 19, 1);
        dlg_blit_dato(dword_53A85, dword_53C67, 0);
        v2 = map_window_run();
        map_draw_text_window();
        if (v2 != 1 || dword_53C57 != 0) {
            sys_menu_cancel_frame();
            return 1;
        }
        vm_run(dword_53A7D, 418, 0xAB6E3, 320, 205, 76, 74, 19, 1);
        j___delay(200);
        ev4_196CB();
        saved = qword_53AB1;
        dword_51A83 = 0;
        byte_51AAC = 0;
        for (i = 0; i < dword_53BEB; ++i) {
            uint8_t *r5 = (uint8_t *)dword_53A45 + 80u * (uint32_t)i;

            if ((r5[5] & 0x85) == 0 && r5[6] == 2) {
                map_slide_view(r5[0], r5[1]);
                dword_51A8F = 255;
                map_move_toward((int)(uint32_t)saved, (int)(saved >> 32), i, 1);
                if (dword_51A8F != 255)
                    ((void (*)(int))funcs_1199C[dword_51A8F])(i);
                dlg_portrait_clear();
                rec_flag_or80(i);
            }
        }
        map_view_update(0);
        party_change_map_seq();
        dword_51A83 = 1;
        byte_51AAC = 1;
        return 1;

    case 2:
        sys_menu_audio();
        return 0;

    case 3: {
        int v6;

        msg_open_portrait(75);
        vm_run(dword_53A7D, 419, 0xA9F23, 320, 205, 76, 74, 19, 1);
        dlg_blit_dato(dword_53A85, dword_53C67, 0);
        v6 = map_window_run();
        map_draw_text_window();
        if (v6 != 1 || dword_53C57 != 0) {
            sys_menu_cancel_frame();
            return 1;
        }
        vm_run(dword_53A7D, 420, 0xAB6E3, 320, 205, 76, 74, 19, 1);
        j___delay(200);
        ev4_196CB();
        byte_51AAC = 0;
        party_change_map_seq();
        byte_51AAC = 1;
        return 1;
    }

    default:
        return 0;
    }
}

/* 0x1728C */
void sys_menu_audio(void)
{
    int a_tiles[4];                  /* 0x53F02 copy (patch arg 2)   */
    int b_tiles[4];                  /* 0x51EAF copy (patch arg 1)   */
    int v0;

    memcpy(b_tiles, &fd2_data1[0x1EAF], sizeof b_tiles);
    memcpy(a_tiles, &fd2_data1[0x3F02], sizeof a_tiles);
    dword_53C57 = 0;

    for (;;) {
        b_tiles[0] = (byte_51E61 == 0) + 18;
        b_tiles[1] = (byte_51E62 == 0) + 20;
        b_tiles[2] = (byte_53AF9 != 0) + 22;
        b_tiles[3] = (byte_51AAB == 0) + 24;

        map_cursor_patch_expand(b_tiles, a_tiles);
        do {
            v0 = map_cursor_dir_select(b_tiles, a_tiles);
        } while (v0 == 0);
        map_cursor_patch_collapse(b_tiles, a_tiles);

        if (v0 == -1)
            return;

        if (dword_53C57 == 0) {
            byte_51E61 = (byte_51E61 == 0);
            AIL_SEQ_VOLUME(dword_53ED0, byte_51E61 ? 127 : 0, 1000);
        } else if (dword_53C57 == 2) {
            byte_53AF9 ^= 1;
        } else if (dword_53C57 == 3) {
            byte_51AAB ^= 1;
        } else {
            byte_51E62 = (byte_51E62 == 0);
        }
    }
}

/* 0x19DF7 */
int sys_menu_system(void)
{
    int      a_tiles[4];             /* 0x51EF5 copy (patch arg 1)   */
    int      b_tiles[4];             /* 0x53F22 copy (patch arg 2)   */
    uint8_t *buf, *v9, *f;
    FILE    *fp;
    int      v7, v8, v13, v14, i;

    memcpy(a_tiles, &fd2_data1[0x1EF5], sizeof a_tiles);
    memcpy(b_tiles, &fd2_data1[0x3F22], sizeof b_tiles);

    /* Probe FD2.SAV: existence only (the bytes are read then discarded). */
    fp = fopen(aFd2Sav, aRb_14);
    if (fp != NULL) {
        buf = (uint8_t *)guest_malloc(0x59CB);
        fread(buf, 1, 0x59CB, fp);
        fclose(fp);
        guest_free(buf);
    } else {
        b_tiles[2] = 1;
    }

    for (i = 0; i < dword_53BEB; ++i) {
        uint8_t *r6 = (uint8_t *)dword_53A45 + 80u * (uint32_t)i;

        if ((r6[5] & 1) == 0 && (r6[5] & 0x80) != 0)
            b_tiles[1] = 1;
    }

    map_cursor_patch_expand(a_tiles, b_tiles);
    do {
        v7 = map_cursor_dir_select(a_tiles, b_tiles);
    } while (v7 == 0);
    map_cursor_patch_collapse(a_tiles, b_tiles);
    map_view_update(0);

    if (v7 == -1)
        return 0;

    switch ((int)dword_53C57) {
    case 0:
        party_panel_run();
        return 1;

    case 1:
        msg_open_portrait(75);
        vm_run(dword_53A7D, 410, 0xA9F23, 320, 205, 76, 74, 19, 1);
        dlg_blit_dato(dword_53A85, dword_53C67, 0);
        v8 = map_window_run();
        map_draw_text_window();
        if (v8 == 1 && dword_53C57 == 0) {
            v9 = (uint8_t *)guest_malloc(0x59CB);
            fp = fopen(aFd2Sav_0, aRb);
            if (fp != NULL) {
                fread(v9, 1, 0x59CB, fp);
                util_deobfuscate(v9, 0x59CB);
                fclose(fp);
            } else {
                v9[0x3B2B] = 0xFF;
                v9[0x4553] = 0xFF;
                v9[0x4F7B] = 0xFF;
                v9[0x59A3] = 0xFF;
            }
            memmove(v9,          (void *)(uintptr_t)dword_53A55, 0x8A3u);
            memmove(v9 + 0x8A3,  (void *)(uintptr_t)dword_53BF7, 0xA00u);
            memmove(v9 + 0x12A3, dword_53A45, 80u * (uint32_t)dword_53BEB);
            memmove(v9 + 0x30A3, (void *)(uintptr_t)dword_53AD5, 0x20u);
            f = v9 + 0x30C3;
            f[0] = (uint8_t)dword_53BEF;
            f[1] = (uint8_t)dword_53BEB;
            f[2] = (uint8_t)dword_53C03;
            f[3] = (uint8_t)qword_53AA9;
            f[4] = (uint8_t)(qword_53AA9 >> 32);
            f[5] = (uint8_t)qword_53AB1;
            f[6] = (uint8_t)(qword_53AB1 >> 32);
            f[7] = (uint8_t)dword_53AB9;
            f[8] = (uint8_t)dword_53ABD;
            f[9] = (uint8_t)dword_53BFB;
            *(uint32_t *)(f + 0x0A) = dword_53BF3;
            f[0x0E] = (uint8_t)byte_53AF9;
            f[0x0F] = (uint8_t)byte_51AAB;
            f[0x10] = (uint8_t)byte_51E61;
            f[0x11] = (uint8_t)byte_51E62;

            fp = fopen(aFd2Sav_1, aWb);
            *(uint32_t *)(v9 + 0x59C7) = (uint32_t)util_sum_tail4(v9, 0x59CB);
            util_deobfuscate(v9, 0x59CB);
            fwrite(v9, 1, 0x59CB, fp);
            fclose(fp);
            guest_free(v9);

            vm_run(dword_53A7D, 411, 0xAB6E3, 320, 205, 76, 74, 19, 1);
        } else {
            vm_run(dword_53A7D, 412, 0xAB6E3, 320, 205, 76, 74, 19, 1);
        }
        j___delay(200);
        ev4_196CB();
        kbd_flush();
        return 1;

    case 2:
        msg_open_portrait(75);
        vm_run(dword_53A7D, 413, 0xA9F23, 320, 205, 76, 74, 19, 1);
        dlg_blit_dato(dword_53A85, dword_53C67, 0);
        v13 = map_window_run();
        map_draw_text_window();
        if (v13 == 1 && dword_53C57 == 0) {
            vm_run(dword_53A7D, 414, 0xAB6E3, 320, 205, 76, 74, 19, 1);
            j___delay(200);
            ev4_196CB();
            bgm_play(-1, 0);
            scenario_load();
            kbd_flush();
            return 1;
        }
        vm_run(dword_53A7D, 412, 0xAB6E3, 320, 205, 76, 74, 19, 1);
        j___delay(200);
        ev4_196CB();
        kbd_flush();
        return 1;

    default:
        msg_open_portrait(75);
        vm_run(dword_53A7D, 415, 0xA9F23, 320, 205, 76, 74, 19, 1);
        dlg_blit_dato(dword_53A85, dword_53C67, 0);
        v14 = map_window_run();
        map_draw_text_window();
        if (v14 != 1 || dword_53C57 != 0) {
            sys_menu_cancel_frame();       /* no kbd_flush on this shared tail */
            return 1;
        }
        vm_run(dword_53A7D, 416, 0xAB6E3, 320, 205, 76, 74, 19, 1);
        bgm_play(-1, 1);
        j___delay(200);
        ev4_196CB();
        return -1;
    }
}
