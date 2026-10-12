/* scene_core.c - P0 main-state-machine core - hand-restored (phase 4, static).
 *
 *   0x25EBB  scenario_menu      new-game / load-game / save-game selector
 *   0x117E7  scene_event_tick   one inner-loop event tick
 *
 * Sources: analysis/decomp/25EBB_sub_25EBB.c + 117E7_sub_117E7.c, cross-checked
 * against the verified assets that call into this layer
 * (port/src/game/scene.c scene_card/scene_state_*, fade.c, msg.c, svc.c, bgm.c).
 *
 * Translation rules applied (same as res.c/boot.c):
 *  - `push imm; call 0x3702F` prologue = Watcom __chkstk probe -> dropped.
 *  - stack-slot phantoms (a1: vN on no-arg callees) -> dropped; the real arity
 *    of sub_1F894/sub_11AA8 is 0 (see analysis/prototype_overrides.csv).
 *  - CRT calls map to libc (fopen/fread/malloc/free/memmove); 0x373CA is the
 *    Watcom fread core (same mapping as res.c).
 *  - 0x25EBB's "save-game" branch (menu choice != 0/1) just plays music around
 *    sub_10010(); kept verbatim though its role is unconfirmed.
 *  - record table access uses the 80-byte stride and 32-bit wrap semantics
 *    documented in data-layout.md §3 (rec.c is the authority).
 */
#include "fd2_game.h"
#include "res.h"          /* res_load (0x111BA), verified in port/ (rescheck 160) */
#include "scene_core.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VGA_BASE fd2_vga
/* REC_STRIDE (80) comes from the migrated rec.h */
#define SAVE_FILE_SIZE 0x59CBu   /* 22987 bytes = FD2.SAV payload */
#define SLOT_STRIDE 2600u        /* bytes per save slot */
#define SLOT_PARTY_OFF 12587u    /* party table (0xA00) + header inside slot */

/* IDA named the globals at 0x53A55 / 0x53A61 as bare 'p' / 'buf' (uncertainties
 * §3.5). They are scene buffers released by scene_apply_state; module-local
 * aliases keep those short names out of the shared globals header.
 * 0x53A61 is also the icon 32-bit offset table (RE_MAP §4 / round27). */
#define g_p_53A55   dword_53A55                          /* 'p'   */
#define g_buf_53A61 dword_53A61                          /* 'buf' */

/* low word of the BIOS tick counter at linear 0x46C (BDA 40:6C), read as
 * int16 like the machine code (movsx in scene_apply_state's delta math). */
static int16_t fd2_tick_low(void)
{
    return (int16_t)((uint16_t)fd2_lowmem[0x46C] |
                     ((uint16_t)fd2_lowmem[0x46D] << 8));
}

/* ---- 0x25EBB ------------------------------------------------------------
 * main loop: v = scenario_menu();
 *   v == 0  -> inner loop (scene_event_tick)
 *   v == -1 -> outer restart
 *   else    -> quit
 */
int scenario_menu(void)
{
    uint32_t choice;
    uint8_t *buf;
    FILE *fp;
    int v8;
    uint8_t *v9;

    choice = (uint32_t)sub_1F894();                     /* 0x25EC8: main menu */

    if (choice == 0) {                                  /* 0x25ED1 NEW GAME */
        sub_1F882();                                    /* fade out        */
        dword_53C03 = 0;
        dword_53A65 = res_load(aFdotherDat,
                        (void *)(uintptr_t)dword_53A65, 0);   /* palette     */
        dword_53BFB = 0;
        byte_51AAC = 0;
        ((void (*)(void))funcs_25E3A[dword_53C03])();   /* enter state 0   */
        sub_25977(byte_51E63[dword_53C03], 0);          /* BGM of state 0  */
        byte_51AAC = 1;
        sub_4E381();                                    /* flush key buffer */
        return 0;
    }

    if (choice != 1) {                                  /* save-game branch */
        sub_25977(-1, 0);
        sub_10010();                                    /* 0x10010 save game */
        sub_25977(byte_51E63[dword_53C03], 0);
        return 0;
    }

    /* ---- choice == 1: LOAD GAME (0x25F2E..) ---- */
    dword_53F66 = res_load(aFdotherDat,
                    (void *)(uintptr_t)dword_53F66, 13);      /* save UI gfx */
    sub_1F882();
    dword_53A65 = res_load(aFdotherDat,
                    (void *)(uintptr_t)dword_53A65, 0);
    memset((void *)(uintptr_t)VGA_BASE, 0, 320u * 200u);
    sub_11D40(0, 255, 0);                               /* pal_fade_range  */

    buf = (uint8_t *)malloc(SAVE_FILE_SIZE);           /* 0x59CB */
    fp = fopen(aFd2Sav_4, aRb_0);                       /* "FD2.SAV","rb"  */
    if (fp != NULL) {
        fread(buf, 1, SAVE_FILE_SIZE, fp);              /* 0x373CA = fread */
        sub_4DF28(buf, SAVE_FILE_SIZE);                 /* util_deobfuscate */
        fclose(fp);
    } else {
        memset(buf, 0xFF, SAVE_FILE_SIZE);              /* no save: fill 0xFF */
    }

    dword_53C57 = 0;                                    /* slot counter      */
    do {
        v8 = sub_29BCB(buf, 0);         /* slot-select dialog */
        if (v8 != -1) {
            v9 = buf + SLOT_STRIDE * dword_53C57 + SLOT_PARTY_OFF;
            memmove((void *)(uintptr_t)dword_53BF7, v9, 0xA00u);  /* party */
            v9 += 0xA00;                                /* +2560: header    */
            dword_53C03 = (uint8_t)v9[0];               /* saved state id   */
            dword_53BFB = (uint8_t)v9[1];
            dword_53BF3 = *(uint32_t *)(v9 + 2);
            byte_51AAB  = v9[6];
            byte_53AF9  = v9[7];
            byte_51E61  = v9[8];
            byte_51E62  = v9[9];
            if (dword_53C03 == 0xFF)
                v8 = 0;                                 /* sentinel: retry  */
        }
        sub_26996();                                    /* msg_close_portrait */
    } while (v8 == 0);

    free(buf);
    free((void *)(uintptr_t)dword_53F66);
    dword_53F66 = 0;

    if (v8 == 1) {                                      /* load accepted    */
        byte_51AAC = 0;
        v8 = sub_26152();                               /* apply loaded state */
        if (v8 == 0) {
            ((void (*)(void))funcs_25E3A[dword_53C03])();
            sub_25977(byte_51E63[dword_53C03], 0);
        }
        byte_51AAC = 1;
    }
    sub_4E381();
    return v8;
}

/* ---- 0x117E7 ------------------------------------------------------------
 * one event tick of the inner loop. `v1 = key` from sub_11AA8() (0x11AA8,
 * wait-for-key, already verified as ev4_11AA8 in port/).
 */
int scene_event_tick(void)
{
    int key;
    int i, idx, guard;
    char *rec;
    int sel;

    key = sub_11AA8();                                  /* wait key (1 arg in
                                                          IDA table = phantom) */

    if (key == 1 || key == 44 || key == 76) {
        /* cycle to the next "actor" record (flags clear, kind byte 2) */
        guard = 0;
        idx = (int)dword_53AE9;
        for (i = 0; i < (int)dword_53BEB; ++i) {
            rec = (char *)(uintptr_t)dword_53A45 + REC_STRIDE * (uint32_t)idx;
            if ((rec[5] & 0x85) == 0 && rec[6] == 2 && guard == 0) {
                sub_12D7B(idx);
                dword_53AE9 = (uint32_t)(idx + 1);
                if (idx + 1 == (int)dword_53BEB)
                    dword_53AE9 = 0;
                guard = 1;
            }
            if (++idx == (int)dword_53BEB)
                idx = 0;
        }
        sub_4E381();
        return 0;
    }

    if (key != 57 && key != 28) {
        if (key != 34) {
            switch (key) {
            case 59:                                    /* ';' */
            case 73:                                    /* 'I' */
                sub_2000A();
                return 0;
            case 60:                                    /* '<' */
            case 71: {                                  /* 'G' */
                int v13 = sub_12C0D();
                if (v13 != -1) {
                    rec = (char *)(uintptr_t)dword_53A45 + REC_STRIDE * (uint32_t)v13;
                    if (rec[7] != 121 && rec[31] != 10) {
                        sub_17AED(v13);
                        return 0;
                    }
                }
                break;
            }
            case 72:                                    /* 'H' */
                sub_25A96(dword_53EEC, 0, 1);
                sub_11B48();
                return 0;
            case 80:                                    /* 'P' */
                sub_25A96(dword_53EEC, 0, 1);
                sub_11B9B();
                return 0;
            case 75:                                    /* 'K' */
                sub_25A96(dword_53EEC, 0, 1);
                sub_11C59();
                return 0;
            case 77:                                    /* 'M' */
                sub_25A96(dword_53EEC, 0, 1);
                sub_11BFA();
                break;
            default:
                break;
            }
        }
        return 0;
    }

    /* key == 57 (0x39) or 28 (0x1C): confirm/act on the selected record */
    if (byte_51A42 != 0)
        --byte_51A42;

    sel = sub_12C0D();                                  /* current selection */
    if (sel != -1) {
        rec = (char *)(uintptr_t)dword_53A45 + REC_STRIDE * (uint32_t)sel;
        dword_53EC8 = 0;                                /* reset counter     */
        if (rec[7] != 121 && rec[31] != 10) {
            if (rec[6] == 2 && (int)rec[5] >= 0 && rec[38] == 0) {
                sub_25A96(dword_53EEC, 7, 1);      /* sfx               */
                while (sub_18890(sel) == 0)
                    ;                                   /* wait animation    */
            } else {
                sub_17AED(sel);
            }
            sub_11CAC(0);
            if (dword_53EC8 > 99)
                dword_53EC8 = 99;
            sub_1E292(sel);
            ((void (*)(int))funcs_1197B[dword_53C03])(sel);   /* per-state   */
            sub_13565();
            if (dword_51A8F != 255)
                ((void (*)(int))funcs_1199C[dword_51A8F])(sel);
            dword_51A8F = 255;
            sub_4E381();
            return 0;
        }
        return 0;
    }

    /* no selection yet: run the VM until it returns non-zero */
    do {
        i = sub_16F55();
    } while (i == 0);
    if (i == 1)
        return 0;
    return i;
}

/* ---- 0x26152 ------------------------------------------------------------
 * "apply state" card. Reconstructed from the offline disassembly
 * (objdump of analysis/seg_cseg01.bin, 0x26152..0x265FA) because the raw
 * pseudocode mis-modelled three things:
 *   - the 3-byte static table copied from 0x52405 = {0x0B,0x3D,0x3E} is a
 *     LOCAL array indexed by the first byte of sub_4E809(state);
 *   - the "old buffer" argument is the local at [esp+0] which is always 0;
 *   - the __PAIR64__ compare is two independent byte compares against
 *     sub_4E809(state)[2] and [1].
 * Buffers freed: dword_53A45, 'p'(0x53A55), dword_53A5D, dword_53A51, 'buf'(0x53A61).
 */
int scene_apply_state(void)
{
    uint8_t sel3[3];            /* [esp+4] copy of {0x0B,0x3D,0x3E} @0x52405    */
    uint8_t bgm_sel = 0;        /* v11 ([esp+0xc]): BGM index cycler 0..9        */
    uint8_t accepted = 0;       /* v10 ([esp+8])  : confirm result               */
    FILE *fp;
    int i, rc, v5;
    int16_t *tbl_src;

    memcpy(sel3, &fd2_data1[0x2405], 3);            /* 0x26172: movs 3 bytes */

    /* ---- release the previous scene buffers (0x2617F..0x2621A) ---- */
    if (dword_53A45) free((void *)(uintptr_t)dword_53A45);
    dword_53A45 = 0;
    if (g_p_53A55) free((void *)(uintptr_t)g_p_53A55);
    g_p_53A55 = 0;
    if (dword_53A5D) free((void *)(uintptr_t)dword_53A5D);
    dword_53A5D = 0;
    if (dword_53A51) free((void *)(uintptr_t)dword_53A51);
    dword_53A51 = 0;
    if (g_buf_53A61) free((void *)(uintptr_t)g_buf_53A61);
    g_buf_53A61 = 0;

    /* ---- reload the icons of every party record (0x2621A..0x2626A) ---- */
    fp = fopen("FDICON.B24", "rb");                 /* 0x5022e / 0x5022b */
    dword_53BDF = 0;
    for (i = 0; i < (int)dword_53BFB; ++i) {
        uint8_t *rec = (uint8_t *)(uintptr_t)dword_53BF7
                     + (uint32_t)i * REC_STRIDE;    /* eax*5 << 4 */
        sub_11019(rec[7], fp);
    }
    fclose(fp);

    /* ---- A: map-flow branch (byte_523E7[state] != 0) (0x2627F..0x2637A) -- */
    if (byte_523E7[dword_53C03] != 0) {
        memset((void *)(uintptr_t)VGA_BASE, 0, 320u * 200u);
        sub_11D40(0, 255, 0);
        sub_1956B(75);
        vm_run(dword_53A7D, 410, VGA_P(0xA9524u), 320, 205, 76, 74, 19, 1);
        /* 0x16559: the machine code pushes ONE arg (see 0x262D8) while port/'s
           dlg_blit_dato takes 3 and passed dlgcheck(800); port/'s own call sites
           pass (dword_53A85, dword_53C67, idx). ABI question recorded in PROGRESS 0-F. */
        sub_16559(dword_53A85, dword_53C67, 0);
        dword_53A51 = (uint8_t *)(uintptr_t)1;      /* non-NULL sentinel */
        rc = sub_19953();
        dword_53A51 = 0;
        sub_197E5();
        sub_26996();
        if (rc != -1 && dword_53C57 == 0) {
            dword_53F66 = res_load(aFdotherDat,
                            (void *)(uintptr_t)dword_53F66, 13);
            sub_2968D(0);
            free((void *)(uintptr_t)dword_53F66);
        }
        do {
            dword_53A45 = dword_53BF7;              /* borrow the party table */
            rc = sub_2AF28();
            dword_53A45 = 0;
        } while (rc == 0);
        sub_11D40(0, 255, 255);
        return 0;
    }

    /* ---- B: interactive scene-select (0x2637F..0x265EB) ----------------- */
    dword_53A5D = malloc(0x25680);
    /* 0x4E809 = table accessor: base 0x6238D, stride 31, offset -31 (tables.h).
       port/ implements it as the file-local static tbl_6238D, so call the
       migrated generic helper directly instead of an alias that cannot link. */
    dword_53F56 = tbl_ptr((void *)&fd2_data2[0x238D], 31,
                                               (int)dword_53C03, -31);
    v5 = *(uint8_t *)(uintptr_t)dword_53F56;                     /* [0] */
    sub_1F882();
    sub_25977(10, 0);
    dword_53F4A = 0;                                /* selection cursor */

    tbl_src = (int16_t *)(uintptr_t)res_load(aFdotherDat, NULL, sel3[v5]);
    sub_4E98D(tbl_src, 0, 0, dword_53A5D + 32904, 456, -1);
    free(tbl_src);
    dword_53F5A = 0;
    dword_53F5A = res_load(aFdotherDat, NULL, 10);
    sub_265EC();
    sub_1F525();
    sub_4E381();

    for (;;) {
        int16_t tick0;

        sub_265EC();                                /* 0x26434: redraw          */
        tick0 = fd2_tick_low();                     /* 0x26439: base tick       */
        while (sub_10620() == 0) {                  /* 0x26441: no key pending  */
            int delta = (int)fd2_tick_low() - (int)tick0;
            if ((uint32_t)delta >= 4) {             /* 0x2645C: >=4 or wrapped  */
                if (++dword_53F52 == 4)             /* 0x26465: frame 0..3      */
                    dword_53F52 = 0;
                sub_265EC();                        /* 0x2647E: next frame      */
                tick0 = fd2_tick_low();             /* 0x26483                  */
            }
        }

        ((uint8_t *)&word_53A8D)[1] = 0x10;         /* 0x2648D: AH=10h ext.key  */
        int386(0x16, &word_53A8D, &word_53A8D);     /* 0x264A0: INT 16h         */
        {
        uint8_t scan = ((uint8_t *)&word_53A8D)[1]; /* scan code in HIBYTE      */
        switch (scan) {
        case 0xE0:                                  /* 0x264AF..0x264C3         */
        case 0x52:
            ((uint8_t *)&word_53A8D)[1] = 0x1C;     /* normalise to Enter       */
            break;
        case 0x22:                                  /* 0x264CD: cycle BGM       */
            if (++bgm_sel == 10)
                bgm_sel = 0;
            sub_25977(bgm_sel, 0);
            break;
        case 0x4D:                                  /* 0x264FA: cursor left     */
            sub_25A96(dword_53EEC, 0, 1);
            if ((int32_t)--dword_53F4A < 0)         /* 0x2650C: 32-bit signed   */
                dword_53F4A = 4;
            break;
        case 0x4B:                                  /* 0x2652C: cursor right    */
            sub_25A96(dword_53EEC, 0, 1);
            if (++dword_53F4A > 4)
                dword_53F4A = 0;
            break;
        default: {                                  /* 0x26559: hotkey match    */
            uint8_t *ent = (uint8_t *)(uintptr_t)dword_53F56;
            if (scan == ent[2] && dword_53F4A == ent[1])
                dword_53F4A = 5;                    /* 0x26572: confirmed       */
            break;
        }
        }
        }

        if (((uint8_t *)&word_53A8D)[1] == 0x1C ||  /* Enter or space (AL=20h) */
            (uint8_t)word_53A8D == 0x20) {
            if (dword_53F4A != 2)
                sub_25A96(dword_53EEC, 1, 3);  /* 0x265A7: sfx             */
            accepted = (uint8_t)sub_2670E();        /* 0x265AF: commit          */
        }
        if (accepted == 0)
            continue;                               /* 0x265BF: back to 0x26434 */
        break;
    }

    free((void *)(uintptr_t)dword_53F5A);           /* 0x265CB                  */
    return dword_53F4A != 2;                        /* 0x265D3                  */
}

/* ---- 0x265EC -------------------------------------------------------------
 * Redraw the map view: copy the saved view buffer back, blit the location
 * panel and name, stamp the map cursor sprite for the current record cell and
 * publish the 312x192 window to the VGA staging page VGA_P(0xA0504u).  Reached from
 * scene_apply_state's interactive branch (0x26434 / 0x2647E) once per frame.
 * The machine leaves eax from gfx_copy_rows; the callers ignore the value.
 */
int scene_map_view_draw(void)
{
    const uint8_t *ent = (const uint8_t *)tbl_ptr((void *)&fd2_data2[0x238D],
                                                  31, (int)dword_53C03, -31);
    uint8_t  v2    = ent[0];
    uint32_t frame = dword_53F52;                   /* 0x53F52: 0..3 frame    */
    uint8_t *base  = dword_53A61;                   /* map animation bank     */
    uint8_t *src;
    int      col, row;

    memmove(dword_53A49, dword_53A5D, 0x25680u);    /* 0x53A5D -> 0x53A49      */
    rle2_blit(dword_53A49 + 0x1A20C,
              (const void *)(uintptr_t)dword_53F5A, 456);
    vm_run(dword_53A7D, (int)(dword_53F4A + 495),dword_53A49 + 0x1ACC4, 456, 205, 76, 74, 19, 0);

    if (frame == 3)                                 /* frame 3 aliases frame 1 */
        frame = 1;
    src = base + *(uint32_t *)(void *)(base + 4u * frame);
    col = byte_52363[6 * v2 + dword_53F4A];
    row = byte_52375[6 * v2 + dword_53F4A];
    sprite24_plain(src, dword_53A49 + 32904 + 456 * row + col, 456);
    gfx_copy_rows((void *)VGA_P(0xA0504u), 320, dword_53A49 + 32904, 456, 312, 192);

    return 0;
}
