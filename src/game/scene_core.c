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
#include "../include/fd2_globals.h"
#include "../include/fd2_funcs.h"
#include "res.h"          /* res_load (0x111BA), verified in port/ (rescheck 160) */
#include "scene_core.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VGA_BASE 0xA0000u
#define REC_STRIDE 80u
#define SAVE_FILE_SIZE 0x59CBu   /* 22987 bytes = FD2.SAV payload */
#define SLOT_STRIDE 2600u        /* bytes per save slot */
#define SLOT_PARTY_OFF 12587u    /* party table (0xA00) + header inside slot */

/* ---- 0x25EBB ------------------------------------------------------------
 * main loop: v = scenario_menu();
 *   v == 0  -> inner loop (scene_event_tick)
 *   v == -1 -> outer restart
 *   else    -> quit
 */
int scenario_menu(void)
{
    uint32_t choice;
    char *buf;
    FILE *fp;
    int v8;
    char *v9;

    choice = (uint32_t)sub_1F894();                     /* 0x25EC8: main menu */

    if (choice == 0) {                                  /* 0x25ED1 NEW GAME */
        sub_1F882();                                    /* fade out        */
        dword_53C03 = 0;
        dword_53A65 = (uint32_t)(uintptr_t)res_load(aFdotherDat,
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
    dword_53F66 = (uint32_t)(uintptr_t)res_load(aFdotherDat,
                    (void *)(uintptr_t)dword_53F66, 13);      /* save UI gfx */
    sub_1F882();
    dword_53A65 = (uint32_t)(uintptr_t)res_load(aFdotherDat,
                    (void *)(uintptr_t)dword_53A65, 0);
    memset((void *)(uintptr_t)VGA_BASE, 0, 320u * 200u);
    sub_11D40(0, 255, 0);                               /* pal_fade_range  */

    buf = (char *)malloc(SAVE_FILE_SIZE);               /* 0x59CB */
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
        v8 = sub_29BCB((int)(uintptr_t)buf, 0);         /* slot-select dialog */
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
                sub_25A96((int)dword_53EEC, 0, 1);
                sub_11B48();
                return 0;
            case 80:                                    /* 'P' */
                sub_25A96((int)dword_53EEC, 0, 1);
                sub_11B9B();
                return 0;
            case 75:                                    /* 'K' */
                sub_25A96((int)dword_53EEC, 0, 1);
                sub_11C59();
                return 0;
            case 77:                                    /* 'M' */
                sub_25A96((int)dword_53EEC, 0, 1);
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
                sub_25A96((int)dword_53EEC, 7, 1);      /* sfx               */
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
