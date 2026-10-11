/* scenario_load.c - load a saved scenario and play its chapter intro.
 *   original 0x10010, 1552 bytes (see scenario_load.h).
 *
 * Sources: analysis/decomp/10010_sub_10010.c + the offline disassembly
 * (analysis/cseg01_disasm.txt, 0x10010..0x1061B), which is authoritative for
 * argument order, constants and the shared tail.
 *
 * Decompiler artifacts / traps handled here:
 *   - the Watcom stack probe (`push 0x3C; call 0x3702F`) is dropped - it
 *     probes the guard page and does not allocate the frame;
 *   - the tail `jmp 0x22BBE` lands inside sub_22AF6's body on a shared
 *     epilogue (`add esp,4; pop ebp/edi/esi/ebx; ret`), i.e. a plain return,
 *     so the function-map callee sub_22AF6 is a false dependency;
 *   - Hex-Rays merges the adjacent dwords 0x53AA9/0x53AAD and 0x53AB1/0x53AB5
 *     into the qword views qword_53AA9 / qword_53AB1; the machine stores both
 *     halves as two zero-extended 32-bit byte writes, reproduced by packing
 *     two bytes into the 64-bit cells;
 *   - the three out-of-memory paths share one shape (word_53A8D = 3;
 *     int386(0x10); printf(msg); exit(1)) and are folded into oom_exit();
 *   - the CRT fopen/fread/fwrite/fclose map to libc (as in scene_core.c);
 *     malloc/free go through the guest-heap seam (guest_malloc/guest_free).
 *
 * The atlas cell `buf` (0x53A61) is freed first and then rebuilt by
 * unit_entry_data (0x11019) - its `dword_53BDF == 0` path reallocs it - so
 * the later FD2.TMP fwrite uses the freshly built atlas.
 */
#include "fd2_game.h"
#include "scenario_load.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAVE_FILE_SIZE 0x59CBu   /* FD2.SAV payload (22987 bytes)          */
#define STATE_OFFSET   0x8A3u    /* packed 0x8A3 state header inside FD2.SAV */
#define PARTY_OFFSET   0x12A3u   /* party record table (80 * count)        */
#define XTRA_OFFSET    0x30A3u   /* 0x20-byte extra block                  */
#define FIELD_OFFSET   0x30C3u   /* 18 scene-field bytes at the save tail  */
#define ATLAS_SIZE     0x32A00u  /* FD2.TMP atlas size (matches unit_load) */

/* The FD2.SAV open pair has no fd2_globals macro (its name sits at dsi 0x1B
 * and its mode at 0x18).  FDICON/TMP use the existing aFdiconB24/aFd2Tmp. */
#define SAVE_MODE ((const char *)&fd2_data1[0x18])   /* "rb"      */
#define SAVE_NAME ((const char *)&fd2_data1[0x1B])   /* "FD2.SAV" */

/* 0x10034 / 0x1019C / 0x102CC: text mode, then the Out of Memory message. */
static void oom_exit(const char *msg)
{
    word_53A8D = 3;
    int386(0x10, &word_53A8D, &word_53A8D);
    printf("%s", msg);
    exit(1);
}

/* 0x10010 */
void scenario_load(void)
{
    uint8_t *buf;
    uint8_t *field;
    void    *rec;
    FILE    *fp;
    int      i, j, k;

    buf = (uint8_t *)guest_malloc(SAVE_FILE_SIZE);          /* 0x10021 */
    if (buf == NULL)
        oom_exit(aOutOfMemory);

    fp = fopen(SAVE_NAME, SAVE_MODE);                       /* 0x10065 */
    fread(buf, 1, SAVE_FILE_SIZE, fp);
    fclose(fp);

    util_deobfuscate(buf, SAVE_FILE_SIZE);                  /* 0x10099 */
    field = buf + FIELD_OFFSET;                             /* esi        */
    if (util_sum_tail4(buf, SAVE_FILE_SIZE) !=              /* 0x100AD */
        *(uint32_t *)(buf + SAVE_FILE_SIZE - 4)) {
        msg_open_portrait(75);                              /* 0x100BD */
        vm_run(dword_53A7D, 436,VGA_P(696099), 320, 205, 76, 74, 19, 1);
        dlg_blit_dato(dword_53A85, dword_53C67, 0);
        dlg_wait_key(0);
        ev4_196CB();
    }

    pal_fade_out();                                         /* 0x1010A */

    memmove((void *)(uintptr_t)dword_53BF7, buf + STATE_OFFSET, 0xA00u);
    dword_53A65 = (uint8_t *)res_load(aFdotherDat, dword_53A65, 0);
    dword_53C03 = field[2];                                 /* 0x10143 */
    dword_53A59 = res_load(aFdfieldDat,
                    (void *)(uintptr_t)dword_53A59, 3 * dword_53C03 + 2);

    if (dword_53A55 != 0)
        guest_free((void *)(uintptr_t)dword_53A55);         /* free 'p'    */
    dword_53A55 = guest_malloc(STATE_OFFSET);
    if (dword_53A55 == 0)
        oom_exit(aOutOfMemory_0);
    memmove((void *)(uintptr_t)dword_53A55, buf, STATE_OFFSET);
    world_load_tiles();                                     /* 0x101D7 */

    dword_53A79 = res_load(aFdtxtDat,
                    (void *)(uintptr_t)dword_53A79, dword_53C03 + 1);
    dword_53A51 = (uint8_t *)res_load(aFdfieldDat, dword_53A51,
                    3 * dword_53C03);
    dword_53AC1 = (uint32_t)(int32_t)*(int16_t *)dword_53A51;         /* movsx */
    dword_53AC5 = (uint32_t)(int32_t)*((int16_t *)dword_53A51 + 1);
    i = 2 * ((uint8_t *)(uintptr_t)dword_53A55)[0];         /* 2 * p[0]   */
    dword_53A5D = (uint8_t *)res_load(aFdshapDat, dword_53A5D, i);
    dword_53A69 = res_load(aFdshapDat,
                    (void *)(uintptr_t)dword_53A69, i + 1);
    util_fix_records(dword_53A51);                          /* 0x10270    */
    dword_53BE7 = ((uint8_t *)(uintptr_t)dword_53A55)[1];
    dword_53BE3 = ((uint8_t *)(uintptr_t)dword_53A55)[2];
    dword_53BEB = field[1];                                 /* 0x10296    */

    if (dword_53A45 != NULL)
        guest_free(dword_53A45);
    dword_53A45 = guest_malloc(0x1E00u);                    /* 0x102B6    */
    if (dword_53A45 == NULL)
        oom_exit(aOutOfMemory_1);
    memmove(dword_53A45, buf + PARTY_OFFSET, 80u * (uint32_t)dword_53BEB);
    memmove((void *)(uintptr_t)dword_53AD5, buf + XTRA_OFFSET, 0x20u);

    if (dword_53A61 != NULL)
        guest_free(dword_53A61);                            /* free 'buf'  */
    fp = fopen(aFdiconB24, aRb_1);                          /* 0x10347     */
    dword_53BDF = 0;
    for (i = 0; i < dword_53BEB; ++i)
        ((uint8_t *)dword_53A45)[80 * i + 2] =              /* 0x1036A..   */
            (uint8_t)unit_entry_data(((uint8_t *)dword_53A45)[80 * i + 7], fp);
    fclose(fp);

    fp = fopen(aFd2Tmp, aWb_0);                             /* 0x103A9     */
    fwrite(dword_53A61, 1, ATLAS_SIZE, fp);
    fclose(fp);

    dword_53BEF = field[0];                                 /* 0x103DC     */
    qword_53AA9 = (uint64_t)field[3] | ((uint64_t)field[4] << 32);
    qword_53AB1 = (uint64_t)field[5] | ((uint64_t)field[6] << 32);
    dword_53AB9 = field[7];
    dword_53ABD = field[8];
    dword_53BFB = field[9];
    dword_53BF3 = *(uint32_t *)(field + 0x0A);
    byte_53AF9 = field[0x0E];
    byte_51AAB = field[0x0F];
    byte_51E61 = field[0x10];
    byte_51E62 = field[0x11];

    guest_free(buf);                                        /* 0x1044B     */
    guest_free((void *)(uintptr_t)dword_53A59);
    dword_53A59 = 0;

    bgm_play(byte_51E63[dword_53C03], 0);                   /* 0x1046C     */
    dword_51A83 = 0;
    ev4_12263();
    map_view_update(1);
    pal_fade_in();

    /* 9-step band fade at x = 84, plus the location number once j > 6. */
    for (j = 0; j < 9; ++j) {                               /* 0x104A1     */
        rec = res_draw_subimage(dword_53A81, (void *)VGA_P(0xA0000u),
                                320, 120, 84, j + 83);
        if (j > 6)
            dlg_draw_number((void *)VGA_P(0xA726Bu), 320, dword_53BEF, 42, 3);
        j___delay(70);
        if (j == 8)
            j___delay(500);
        dlg_snap_restore(rec, (void *)VGA_P(0xA0000u), 320);
    }

    /* 4-step map-window fade with k = 2,3,4,9 (the k == 5 slot is skipped). */
    for (k = 2; k < 6; ++k) {                               /* 0x1051F     */
        int kk;
        if (k == 5)
            k = 9;
        kk = k * k;
        rec = res_draw_subimage(dword_53A81, dword_53A49 + 0x8088,
                                456, 116, kk + 84, 91);
        dlg_draw_number(dword_53A49 + 0x812F + (kk + 0x5A) * 456,
                        456, dword_53BEF, 42, 3);
        gfx_copy_rows((void *)VGA_P(0xA0504u), 320, dword_53A49 + 0x8088,
                      456, 312, 192);
        svc_wait_ticks(1);
        dlg_snap_restore(rec, dword_53A49 + 0x8088, 456);
    }

    map_view_update(0);                                     /* 0x105EB     */
    j___delay(200);
    dword_53AE9 = 0;
    dword_51A83 = 1;
    kbd_flush();
}
