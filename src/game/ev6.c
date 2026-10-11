/* ev6.c - FD2 scene-script handlers of funcs_1199C, first batch.
 * See ev6.h; written from the original disassembly (cdecl, last push = first
 * argument).  Services stay at their original addresses, exactly as ev2..ev5.
 */
#include "ev6.h"

#include <stdint.h>
#include "fd2_game.h"   /* de-guest C4: restored cross-module decls */
#include "../include/fd2_globals.h"

/* --- services (original addresses) ------------------------------------- */
typedef void (*vm_fn)(void *stream, int sub, int addr, int pitch, int fg,
                      int shadow, int bgfill, int line_step, int wait);
typedef void (*i1_fn)(int);
typedef void (*i2_fn)(int, int);
typedef void (*i3_fn)(int, int, int);
typedef void (*v0_fn)(void);
typedef int  (*i1i_fn)(int);
typedef int  (*delay_fn)(int);


/* --- globals ----------------------------------------------------------- */

#define REC 0x50

/* Tail of every handler: vm_run(stream, sub,VGA_P(0xA0000), 320, 205, 76, 74, 19, 1). */
static void vm_play(int sub)
{
    vm_run((void *)(uintptr_t)dword_53A79, sub,VGA_P(0xA0000u), 320,
                205, 76, 74, 19, 1);
}

#define R(i) (dword_53A45 + REC * (uint32_t)(i))

/* 0x34531 - idx[0]: two sprite loads with a vm_run between. */
void ev6_34531(int arg)
{
    (void)arg;
    unit_add(1);
    unit_sprites_build(3);
    ev2_135DD(5, 8);
    map_view_update(1);
    delay(100);
    ev4_1366A(7);
    kbd_flush();
    vm_play(11);
    dword_51A83 = 0;
    unit_sprites_build(7);
    map_view_update(1);
    delay(100);
    ev4_1366A(8);
    kbd_flush();
    vm_play(3);
    dlg_portrait_clear();
}

/* 0x3460B - idx[1]. */
void ev6_3460B(int arg)
{
    (void)arg;
    ev2_135DD(11, 16);
    unit_map_render(4);
    kbd_flush();
    map_view_update(1);
    ev4_1366A(3);
    dlg_portrait_clear();
    vm_play(4);
}

/* 0x34673 - idx[2]. */
void ev6_34673(int arg)
{
    (void)arg;
    ev2_135DD(0, 16);
    unit_map_render(5);
    kbd_flush();
    map_view_update(1);
    ev4_1366A(4);
    dlg_portrait_clear();
    vm_play(5);
}

/* 0x346CD - idx[3]. */
void ev6_346CD(int arg)
{
    (void)arg;
    ev2_135DD(11, 11);
    byte_53AFA = 1;
    unit_sprites_build(6);
    byte_53AFA = 0;
    map_view_update(1);
    ev4_1366A(6);
    dlg_portrait_clear();
    kbd_flush();
    vm_play(6);
}

/* 0x34778 - idx[6]: then stamp two bytes into records 5..10. */
void ev6_34778(int arg)
{
    int i;
    (void)arg;
    ev2_135DD(9, 1);
    delay(100);
    byte_53AFA = 1;
    unit_sprites_build(3);
    byte_53AFA = 0;
    ev4_1366A(13);
    delay(200);
    vm_play(4);
    for (i = 5; i < 11; i++) {
        *(uint8_t *)(uintptr_t)(R(i) + 53) = 26;
        *(uint8_t *)(uintptr_t)(R(i) + 54) = 15;
    }
}

/* 0x350BE - idx[5]: tail-merged with 0x34F38. */
void ev6_350BE(int arg)
{
    (void)arg;
    unit_sprites_build(1);
    vm_play(1);
}

/* 0x350C8 - idx[7]. */
void ev6_350C8(int arg)
{
    (void)arg;
    ev2_135DD(27, 5);
    byte_53AFA = 1;
    unit_sprites_build(2);
    byte_53AFA = 0;
    ev4_1366A(46);
    dlg_portrait_clear();
    vm_play(8);
}

/* 0x34818 - idx[9]: only when flag 6 is clear. */
void ev6_34818(int arg)
{
    (void)arg;
    if (rec_flag(6) != 0)
        return;
    unit_sprites_build(2);
    ev2_135DD(3, 0);
    delay(800);
    ev2_135DD(3, 17);
    delay(200);
    vm_play(4);
}

/* 0x348BB - idx[11]. */
void ev6_348BB(int arg)
{
    (void)arg;
    unit_sprites_build(2);
    vm_play(2);
}

/* 0x34940 - idx[14]. */
void ev6_34940(int arg)
{
    (void)arg;
    rec_status_set(37, 40, 0);
    rec_status_set(13, 24, 0);
    vm_play(3);
}

/* 0x34984 - idx[15]. */
void ev6_34984(int arg)
{
    (void)arg;
    dword_51A83 = 0;
    byte_53AFA = 1;
    unit_sprites_build(2);
    byte_53AFA = 0;
    ev2_135DD(14, 0);
    ev4_1366A(23);
    dlg_portrait_clear();
    rec_status_set(7, 12, 0);
    rec_status_set(33, 35, 0);
    vm_play(4);
}

/* 0x349EC - idx[16]. */
void ev6_349EC(int arg)
{
    (void)arg;
    unit_sprites_build(3);
    vm_play(5);
}

/* 0x34A1E - idx[17]: the tail at 0x34750 is a second vm_run, sub 7. */
void ev6_34A1E(int arg)
{
    (void)arg;
    rec_status_set(48, 51, 7);
    vm_play(6);
    ev4_1366A(24);
    vm_play(7);
}

/* 0x34B07 - idx[20]: tail-merged with the vm_run(1) block. */
void ev6_34B07(int arg)
{
    (void)arg;
    vm_play(1);
}

/* 0x34B6F - idx[22]: tail-merged with the vm_run(3) body. */
void ev6_34B6F(int arg)
{
    (void)arg;
    if (rec_flag(8) != 0)
        return;
    unit_sprites_build(1);
    vm_play(3);
}

/* 0x34B9A - idx[23]. */
void ev6_34B9A(int arg)
{
    (void)arg;
    rec_status_set(8, 28, 0);
    vm_play(4);
    if ((int)dword_53BEF >= 15)
        return;
    unit_sprites_build(2);
    ev2_135DD(5, 17);
    ev4_1366A(25);
    vm_play(5);
    ev2_135DD(5, 17);
    ev4_1366A(26);
    rec_flag_set1(33);
    dword_51A83 = 1;
}

/* 0x34C52 - idx[24]: bare vm_run(3); 0x34FC2 is byte-identical. */
void ev6_34C52(int arg)
{
    (void)arg;
    vm_play(3);
}

/* 0x34C7A - idx[25]: only when the status block's +16 byte is 1. */
void ev6_34C7A(int arg)
{
    (void)arg;
    if (*(uint8_t *)(uintptr_t)(dword_53AD5 + 16) != 1)
        return;
    byte_53AFA = 1;
    unit_sprites_build(2);
    byte_53AFA = 0;
    ev2_135DD(16, 10);
    ev4_1366A(30);
    vm_play(2);
    *(uint8_t *)(uintptr_t)(dword_53AD5 + 17) = 1;
}

/* 0x34D2F - idx[27]. */
void ev6_34D2F(int arg)
{
    (void)arg;
    ev2_135DD(8, 2);
    delay(100);
    unit_sprites_build((int)dword_53BEF);
    delay(100);
}

/* 0x34DD0 - idx[30]: clear records 12..33, seed record 11, two vm_runs. */
void ev6_34DD0(int arg)
{
    uint8_t *v7;
    int i;
    (void)arg;
    for (i = 12; i < 34; i++)
        *(uint8_t *)(uintptr_t)(R(i) + 52) = 0;
    *(uint8_t *)(uintptr_t)(dword_53A55 + 3) = (uint8_t)(dword_53BEF + 1);
    *(uint8_t *)(uintptr_t)(dword_53A55 + 6) = (uint8_t)(dword_53BEF + 2);
    v7 = dword_53A45 + 880;
    *(uint8_t *)(uintptr_t)(v7 + 5) = 0;
    *(uint8_t *)(uintptr_t)(v7 + 6) = 1;
    *(uint8_t *)(uintptr_t)(v7 + 7) = 6;
    *(uint8_t *)(uintptr_t)(v7 + 8) = 6;
    *(uint8_t *)(uintptr_t)(v7 + 0x31) = 0xFF;
    *(uint8_t *)(uintptr_t)(v7 + 0x34) = 0x80;
    *(uint16_t *)(uintptr_t)(v7 + 0x40) = 1;
    vm_play(2);
    unit_sprites_build(1);
    vm_play(3);
    dword_53EC8 = 0;
    *(uint8_t *)(uintptr_t)(dword_53AD5 + 16) = 2;
}

/* 0x34EB3 - idx[31]: four window moves around the current status value. */
void ev6_34EB3(int arg)
{
    (void)arg;
    unit_sprites_build(*(uint8_t *)(uintptr_t)(dword_53AD5 + 16));
    ++*(uint8_t *)(uintptr_t)(dword_53AD5 + 16);
    ev2_135DD(0, 0);
    delay(200);
    ev2_135DD(12, 0);
    delay(200);
    ev2_135DD(12, 11);
    delay(200);
    ev2_135DD(0, 11);
    delay(200);
}

/* 0x34F38 - idx[32]: bare load + vm_run(1); 0x350BE is byte-identical. */
void ev6_34F38(int arg)
{
    (void)arg;
    unit_sprites_build(1);
    vm_play(1);
}

/* 0x34FC2 - idx[34]: tail-merged with 0x34C52's vm_run(3) body. */
void ev6_34FC2(int arg)
{
    (void)arg;
    vm_play(3);
}

/* 0x34FCC - idx[35]. */
void ev6_34FCC(int arg)
{
    (void)arg;
    ev2_135DD(12, 5);
    byte_53AFA = 1;
    unit_sprites_build(2);
    byte_53AFA = 0;
    ev4_1366A(42);
    dlg_portrait_clear();
}

/* 0x35022 - idx[37]. */
void ev6_35022(int arg)
{
    (void)arg;
    vm_play(1);
    ev2_135DD(15, 34);
    byte_53AFA = 1;
    unit_sprites_build(3);
    byte_53AFA = 0;
    ev4_1366A(43);
    dlg_portrait_clear();
    ev2_135DD(0, 26);
    byte_53AFA = 1;
    unit_sprites_build(4);
    byte_53AFA = 0;
    ev4_1366A(44);
    dlg_portrait_clear();
    dword_51A83 = 1;
}
