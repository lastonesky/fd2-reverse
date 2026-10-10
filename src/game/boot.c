/* boot.c - program entry (original main @ 0x25BF4, 711 bytes) - hand-restored.
 *
 * Structure recovered from the full disassembly (analysis/decomp/25BF4_main.c
 * is the raw Hex-Rays version; every deviation is noted below):
 *
 *   1. AIL audio bring-up (MDI first, then DIG; handles kept in dword_53Exx)
 *   2. load 8 resources from FDOTHER.DAT / FDTXT.DAT via res_load (0x111BA)
 *   3. allocate three heaps: 0x20 scratch, 0x25680 work buffer, 0xA00
 *      record table (= 80 x 32 slots, see RE_MAP §2 note 5)
 *   4. set VGA mode 13h through int386(0x10h) using the game's REGS cell at
 *      word_53A8D (the same cell is reused with AX=3 on exit)
 *   5. snapshot BIOS tick (movsx word [0x46C]) into dword_53C0F
 *   6. scramble the random table rand()%256 times (sub_4EBE3, 0x4EBE3)
 *   7. main state machine:
 *        outer: play BGM 18, enter scene (sub_25EBB)
 *        inner: run event tick (sub_117E7) until it returns non-zero
 *               dword_53ECC = pending transition request:
 *                 1 -> run sub_22E5C, force tick result 1 (outer restart)
 *                 2 -> state switch: funcs_25E23[state](); then
 *                      sub_26152(); if 0: funcs_25E3A[state]() + BGM of
 *                      the new state (byte_51E63[state]); if !=0: quit
 *               sub_4E381() = flush keyboard buffer (BDA 40:1C/40:1A)
 *        exit when sub_25EBB() returns >0 (or <-1), or tick returns -1
 *   8. teardown: AIL_shutdown, video mode 3, return (shared tail 0x16F04)
 *
 * Deviations from the raw pseudocode (all verified against bytes):
 *  - `push 1Ch; call 0x3702F` / `push 20h; call 0x3702F` prologues are the
 *    Watcom stack probe (__chkstk thunk), NOT arguments - dropped.
 *  - `malloc((offset loc_2567F+1))` renders an ADDRESS for what the bytes
 *    say is an immediate: 68 80 56 02 00 = push 0x25680 -> malloc(0x25680).
 *    (IDA prefers address display when the value sits next to a code item;
 *    classic size-immediate trap - see PROGRESS §1.5.)
 *  - `AIL_startup(a1: v3)` style garbage args come from the pre-fix arity
 *    guesses; real entries are void-argument (lib/ail.h).
 *  - `v18 = v2; v17 = v4` etc. were reads of register-arg phantom params
 *    (`sub_1EBB` was guessed __usercall); no such inputs exist - dropped.
 *  - main's return value: the machine code falls into the shared tail and
 *    returns whatever int386() left in eax; we return 0 (irrelevant to
 *    CRT, documented for honesty).
 *
 * Loop equivalence note (the raw C has one big nested do/while; the
 * structured form below is a 1:1 mapping of the branches at
 * 0x25DC6/0x25E74/0x25E7C/0x25E88/0x25E8F - see comments with addresses).
 */
#include "../include/fd2_globals.h"
#include "../include/fd2_funcs.h"
#include "res.h"
#include "boot.h"
#include "../lib/ail.h"

#include <stdlib.h>
#include <stdint.h>

int main(int argc, char **argv)
{
    void *mdi, *dig;
    int v, r;
    int quit = 0;
    int i, n;

    (void)argc; (void)argv;

    /* ---- 1. AIL bring-up (0x25C01..0x25C5E) ------------------------ */
    AIL_startup();                                   /* 0x25C01 */
    mdi = AIL_install_MDI_INI();                     /* 0x25C06 */
    dword_53ED8 = (uint32_t)(uintptr_t)mdi;
    if (mdi != NULL) {
        byte_53EF0 = 1;
        dword_53ED0 = (uint32_t)(uintptr_t)
            AIL_allocate_sequence_handle(mdi);       /* 0x25C1D */
    }
    dig = AIL_install_DIG_INI();                     /* 0x25C2B */
    dword_53EDC = (uint32_t)(uintptr_t)dig;
    if (dig != NULL) {
        byte_53EF1 = 1;
        dword_53EE4 = (uint32_t)(uintptr_t)
            AIL_allocate_sample_handle(dig);         /* 0x25C43 */
        dword_53EE8 = (uint32_t)(uintptr_t)
            AIL_allocate_sample_handle(dig);         /* 0x25C50 */
    }

    /* ---- 2. resource loads (0x25C63..0x25D2E) ----------------------
     * fd2_* globals hold heap pointers as 32-bit values (original idiom);
     * res_load frees the old buffer and returns the new one, so each line
     * is "reload in place". */
#define RELOAD(slot, file, idx) \
    ((slot) = (uint32_t)(uintptr_t)res_load((file), \
              (void *)(uintptr_t)(slot), (idx)))
    RELOAD(dword_53EEC, aFdotherDat, 31);   /* 0x25C63 sound-effect pack */
    RELOAD(dword_53A4D, aFdotherDat,  1);   /* 0x25C7D */
    RELOAD(dword_53A89, aFdotherDat,  2);   /* 0x25C97 */
    RELOAD(dword_53A6D, aFdotherDat,  3);   /* 0x25CB1 */
    RELOAD(dword_53A75, aFdotherDat,  4);   /* 0x25CCB */
    RELOAD(dword_53A81, aFdotherDat,  5);   /* 0x25CE5 */
    RELOAD(dword_53A7D, aFdtxtDat,    0);   /* 0x25CFF texts */
    RELOAD(dword_53AD1, aFdotherDat,  6);   /* 0x25D19 */
#undef RELOAD

    /* ---- 3. heaps (0x25D33..0x25D61) ------------------------------- */
    dword_53AD5 = (uint32_t)(uintptr_t)malloc(0x20);      /* 0x25D35 */
    dword_53A49 = (uint32_t)(uintptr_t)malloc(0x25680);   /* 0x25D47: push 0x25680 */
    dword_53BF7 = (uint32_t)(uintptr_t)malloc(0xA00);     /* 0x25D59: 80*32 records */

    /* ---- 4. VGA mode 13h (0x25D66..0x25D80) ------------------------
     * word_53A8D is the game's REGS cell for int386: AX at offset 0. */
    word_53A8D = 0x13;
    int386(0x10, &word_53A8D, &word_53A8D);

    /* ---- 5. BIOS tick snapshot (0x25D83..0x25D8B) ------------------
     * `mov eax,46Ch; movsx eax,word ptr [eax]` - sign-extended low word
     * of the BIOS tick counter at linear 0x46C (BDA 40:6C). */
    dword_53C0F = (uint32_t)(int32_t)(int16_t)
        (fd2_lowmem[0x46C] | (fd2_lowmem[0x46D] << 8));

    /* ---- 6. random-table scramble (0x25D90..0x25DAF) --------------- */
    n = rand() % 256;                    /* idiv 100h of rand() */
    for (i = 0; i < n; i++)
        sub_4EBE3();                     /* 0x4EBE3 ROL16 table step */

    /* ---- 7. main state machine ------------------------------------- */
    for (;;) {                                   /* outer @0x25DB1 */
        sub_25977(0x12, 0);                      /* play BGM 18 */

        v = sub_25EBB();                         /* enter scene state */
        if (v == 0) {                            /* 0x25DC8 jnz -> inner */
            for (;;) {                           /* inner @0x25DCE */
                r = sub_117E7();                 /* one event tick */

                if (dword_53ECC == 1) {          /* 0x25DD5 */
                    byte_51AAC = 0;
                    sub_22E5C();
                    byte_51AAC = 1;
                    dword_53ECC = 0;
                    r = 1;                       /* mov esi,1 @0x25DFB */
                } else if (dword_53ECC == 2) {   /* 0x25E02 state switch */
                    byte_51AAC = 0;
                    sub_25977(-1, 1);            /* stop current BGM */
                    funcs_25E23[dword_53C03]();  /* leave handler, 0 args */
                    r = sub_26152();
                    if (r == 0) {
                        funcs_25E3A[dword_53C03]();          /* enter */
                        sub_25977((unsigned)byte_51E63[dword_53C03], 0);
                    } else {
                        quit = 1;                /* 0x25E59 mov edi,1 */
                    }
                    byte_51AAC = 1;
                    dword_53ECC = 0;
                    sub_4E381();                 /* flush key buffer */
                }

                if (r == 0) continue;            /* 0x25E76 jz inner */
                if (r == -1) quit = 1;           /* 0x25E7C..0x25E81 */
                break;                           /* r > 0: back to outer */
            }
            if (quit) break;                     /* 0x25E8F test edi */
            /* r > 0 with v == 0: edi still 0 -> outer restart (0x25E91) */
        } else if (v == -1) {
            /* 0x25E88: cmp eax,-1 -> xor edi,eax with edi==eax==-1
             * makes edi 0 -> outer restart */
            continue;
        } else {
            quit = 1;                            /* v > 0 or v < -1 */
            break;
        }
    }

    /* ---- 8. teardown (0x25E97..0x25EB6 -> shared tail 0x16F04) ----- */
    AIL_shutdown();
    word_53A8D = 3;                              /* AX = mode 3 (text) */
    int386(0x10, &word_53A8D, &word_53A8D);
    /* 0x25EB6 jmp loc_16F04 = add esp,0Ch; pop edi/esi/ebx; retn
     * (the add esp cleans these very int386 args) -> plain return. */
    return 0;
}
