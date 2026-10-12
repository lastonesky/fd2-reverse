/* save_menu.c - the FD2.SAV four-slot save/load family (0x2968D / 0x2986F /
 * 0x29AB2 / 0x29BCB).  See save_menu.h for the module overview and the ABI
 * notes.  Direct translation of the machine code; the offline disassembly
 * (analysis/cseg01_disasm.txt) is authoritative.
 */
#include "fd2_game.h"
#include "save_menu.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* 22987-byte FD2.SAV payload; the trailing dword (offset 0x59C7) is the
 * util_sum_tail4 checksum stamped by save_slot_write. */
#define SAVE_FILE_SIZE 0x59CBu

/* 0x29AB2 */
void save_slot_rows(int selected, uint8_t *dst, const uint8_t *base)
{
    int i;

    for (i = 0; i < 4; ++i) {
        uint8_t *v5  = dst + (320 * (19 * i + 119));
        int     fg   = (i == selected) ? 201 : 205;
        const uint8_t *slot = base + (2600 * i + 12587);
        uint8_t id = slot[2560];

        dword_53AE1 = i + 1;
        vm_run(dword_53A7D, 549,v5 + 10, 320, fg, 76, 0, 0, 0);
        if (id == 0xFF) {
            vm_run(dword_53A7D, 514,v5 + 88, 320, fg, 76, 0, 0, 0);
        } else {
            vm_run(dword_53A7D, id + 514,v5 + 40, 320, fg, 76, 0, 0, 0);
            vm_run(dword_53A7D, id + 550,v5 + 130, 320, fg, 76, 0, 0, 0);
        }
    }
}

/* 0x29BCB */
int save_slot_select(const uint8_t *base, int wait_keys)
{
    int ret = 0;
    int i;
    int key;

    dword_53C5B = guest_malloc(0xFA00u);
    dword_53C5F = guest_malloc(0xFA00u);
    dword_53C63 = guest_malloc(0xFA00u);

    memmove((void *)(uintptr_t)dword_53C5F, (const void *)(uintptr_t)VGA_P(0xA0000u),
            0xFA00u);
    memmove((void *)(uintptr_t)dword_53C63, (const void *)(uintptr_t)dword_53C5F,
            0xFA00u);

    /* panel sub-image at dword_53F66 + [dword_53F66+70], blitted at +35845. */
    rle2_blit((void *)(uintptr_t)(dword_53C63 + 0x8C05u),
              (const void *)(uintptr_t)
                  (dword_53F66 +
                   *(const uint32_t *)(uintptr_t)(dword_53F66 + 0x46u)),
              320);

    save_slot_rows((int)dword_53C57, dword_53C63, base);

    svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 5, 1);
    for (i = 5; i >= 0; --i)
        msg_blit_band(13 * i + 112, (void *)(uintptr_t)dword_53C5B,
                      (void *)(uintptr_t)dword_53C63);

    while (1) {
        if (wait_keys != 0) {
            dlg_wait_key(0);
        } else {
            byte_53A8E = 0x10;                  /* AH = 10h: BIOS key read */
            int386(0x16, &word_53A8D, &word_53A8D);
            if (byte_53A8E == 0xE0 || byte_53A8E == 0x52)
                byte_53A8E = 0x1C;
            if (byte_53A8E == 0x53)
                byte_53A8E = 0x01;
        }
        key = byte_53A8E;

        if (key == 0x50 && dword_53C57 != 3) {          /* Down */
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 7, 1);
            save_slot_rows((int)++dword_53C57, VGA_P(0xA0000u), base);
        } else if (key == 0x48 && dword_53C57 != 0) {   /* Up   */
            svc_play_sfx((const void *)(uintptr_t)dword_53EEC, 7, 1);
            save_slot_rows((int)--dword_53C57, VGA_P(0xA0000u), base);
        } else if (key == 0x1C || key == 0x39) {        /* Enter/Space */
            ret = 1;
        } else if (key == 0x01) {                       /* Esc   */
            ret = -1;
        }

        if (ret != 0)
            return ret;
    }
}

/* 0x2968D */
void save_slot_write(int wait_keys)
{
    uint8_t *buf;
    FILE    *fp;
    int      ret;

    buf = (uint8_t *)guest_malloc(SAVE_FILE_SIZE);          /* 0x2969B */
    fp  = fopen(aFd2Sav_5, aRb_5);                          /* FD2.SAV, rb */
    if (fp != NULL) {
        fread(buf, 1, SAVE_FILE_SIZE, fp);                  /* 0x373CA */
        util_deobfuscate(buf, SAVE_FILE_SIZE);              /* 0x4DF28 */
        fclose(fp);
    } else {
        memset(buf, 0xFF, SAVE_FILE_SIZE);
    }

    dword_53C57 = 0;
    do {
        ret = save_slot_select(buf, wait_keys);
        if (ret != -1) {
            uint8_t *slot = buf + 2600 * dword_53C57 + 12587;

            memmove(slot, (const void *)(uintptr_t)dword_53BF7, 0xA00u);
            slot += 0xA00;                                  /* +2560 header */
            slot[0] = (uint8_t)dword_53C03;                 /* state id     */
            slot[1] = (uint8_t)dword_53BFB;
            *(uint32_t *)(slot + 2) = dword_53BF3;
            slot[6] = byte_51AAB;
            slot[7] = byte_53AF9;
            slot[8] = byte_51E61;
            slot[9] = byte_51E62;

            fp = fopen(aFd2Sav_6, aWb_2);                   /* FD2.SAV, wb */
            *(uint32_t *)(buf + SAVE_FILE_SIZE - 4) =       /* 0x59C7 */
                (uint32_t)util_sum_tail4(buf, SAVE_FILE_SIZE);
            util_deobfuscate(buf, SAVE_FILE_SIZE);
            fwrite(buf, 1, SAVE_FILE_SIZE, fp);             /* 0x377A3 */
            fclose(fp);
            util_deobfuscate(buf, SAVE_FILE_SIZE);

            if (byte_523E7[dword_53C03] == 0) {
                msg_close_portrait();
                msg_open_portrait(byte_52387[0]);
                vm_run(dword_53A7D, 660,VGA_P(693452), 320, 205, 76, 74, 19, 1);
                dlg_blit_dato(dword_53A85, dword_53C67, 0);
                dlg_wait_key(0);
            }
        }
        msg_close_portrait();
    } while (ret != -1);

    guest_free(buf);
}

/* 0x2986F */
void save_slot_load(void)
{
    uint8_t *buf;
    FILE    *fp;
    int      ret;
    int      i;

    buf = (uint8_t *)guest_malloc(SAVE_FILE_SIZE);          /* 0x29881 */
    fp  = fopen(aFd2Sav_7, aRb_6);                          /* FD2.SAV, rb */
    if (fp != NULL) {
        fread(buf, 1, SAVE_FILE_SIZE, fp);
        util_deobfuscate(buf, SAVE_FILE_SIZE);
        fclose(fp);
    } else {
        memset(buf, 0xFF, SAVE_FILE_SIZE);
    }

    dword_53C57 = 0;
    do {
        ret = save_slot_select(buf, 1);
        msg_close_portrait();
        if (ret != -1) {
            uint8_t *slot = buf + 2600 * dword_53C57 + 12587;
            uint8_t  id   = slot[2560];                     /* scenario id  */

            if (id != 0xFF) {
                if (byte_523E7[id] != 0) {
                    msg_open_portrait(byte_52387[0]);
                    vm_run(dword_53A7D, 479,VGA_P(693452), 320, 205, 76, 74, 19, 1);
                } else {
                    memmove((void *)(uintptr_t)dword_53BF7, slot, 0xA00u);
                    slot += 0xA00;                          /* +2560 header */
                    dword_53C03 = slot[0];
                    dword_53BFB = slot[1];
                    dword_53BF3 = *(uint32_t *)(slot + 2);
                    byte_51AAB   = slot[6];
                    byte_53AF9   = slot[7];
                    byte_51E61   = slot[8];
                    byte_51E62   = slot[9];

                    if (dword_53A61 != NULL)
                        guest_free(dword_53A61);            /* free 'buf'  */
                    fp = fopen(aFdiconB24_3, aRb_7);        /* FDICON.B24 */
                    dword_53BDF = 0;
                    for (i = 0; i < (int)dword_53BFB; ++i)
                        unit_entry_data(
                            *(uint8_t *)(uintptr_t)(dword_53BF7 + 80 * i + 7),
                            fp);                            /* 0x11019    */
                    fclose(fp);
                    dword_53F56 = tbl_ptr(
                        (void *)&fd2_data2[0x238D], 31,
                        (int)dword_53C03, -31);             /* 0x4E809    */
                    ret = -1;
                    msg_open_portrait(byte_52387[0]);
                    vm_run(dword_53A7D, 478,VGA_P(693452), 320, 205, 76, 74, 19, 1);
                }
                dlg_blit_dato(dword_53A85, dword_53C67, 0);
                dlg_wait_key(0);
                msg_close_portrait();
            }
        }
    } while (ret != -1);

    guest_free(buf);
}
