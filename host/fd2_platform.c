/* fd2_platform.c - the DOS/CRT boundary the restored game still calls.
 *
 * src/lib/crt.h declares delay/outp/int386/fd2_mem_lock.  In port/ these were
 * serviced by dos.c (the software-interrupt + port trap); here the game calls
 * them directly, so this file implements just those four with host facilities
 * and keeps the VGA DAC capture that the palette presenter needs.
 *
 * The digital audio and music path does NOT go through here - it is the
 * lib/ail.h boundary implemented in host/fd2_audio.c.
 */
#include <stdint.h>
#include <stdio.h>

#include "fd2_video.h"     /* fd2_palette / fd2_palette_dirty */
#include "platform.h"
#include "../src/lib/crt.h"
#include "fd2_kernel.h"   /* fd2_kbd_wait/pop */

/* ---- timer --------------------------------------------------------------
 * Watcom delay(ms): busy-wait on the BIOS tick.  A host sleep is equivalent
 * for every caller (they only need "at least ms passed"). */
void delay(unsigned ms)
{
    plat_sleep_ms(ms);
}

/* ---- VGA DAC ports ------------------------------------------------------
 * The game drives the palette with out 0x3C8 (index) and out 0x3C9 (R,G,B),
 * 6 bits per channel.  The stored table is 8 bits per channel, so stretch
 * 0..63 -> 0..255 here (reading the raw value as 8 bits made every frame a
 * quarter as bright / half as saturated as the original). */
static int g_dac_index;
static int g_dac_component;

unsigned char fd2_outp(unsigned port, int value)
{
    switch (port & 0xFFFF) {
    case 0x03C8:
        g_dac_index = value & 0xFF;
        g_dac_component = 0;
        break;
    case 0x03C9: {
        uint8_t v = (uint8_t)value & 0x3Fu;
        if (g_dac_index >= 0 && g_dac_index < 256) {
            fd2_palette[g_dac_index * 3 + g_dac_component] =
                (uint8_t)((v << 2) | (v >> 4));
            fd2_palette_dirty = 1;
        }
        if (++g_dac_component == 3) {
            g_dac_component = 0;
            g_dac_index = (g_dac_index + 1) & 0xFF;
        }
        break;
    }
    default:
        break;              /* no other port is used by the game logic */
    }
    return 0;
}

/* ---- software interrupts -------------------------------------------------
 * Only INT 10h (video mode set) and INT 16h (keyboard) are reached from the
 * restored code.  Video modes are a no-op (we always have the 320x200 seam);
 * keyboard reads are serviced from the host key ring in fd2_kernel.c, so this
 * entry stays a safe stub until that is wired.  Callers only test the low
 * bits / read the REGS cell, never a return value. */
int int386(int intno, const void *in, void *out)
{
    if (intno == 0x16 && in && out) {
        uint8_t       *o  = (uint8_t *)out;
        const uint8_t *i  = (const uint8_t *)in;
        uint8_t        scan, ascii;

        switch (i[1]) {                       /* AH selects the function   */
        case 0x00:                            /* read key                  */
        case 0x10:                            /* enhanced read (game)      */
            fd2_kbd_wait(&scan, &ascii);
            o[0] = ascii;
            o[1] = scan;
            break;
        case 0x01:                            /* check (peek)              */
        case 0x11:
            if (fd2_kbd_pop(&scan, &ascii)) { o[0] = ascii; o[1] = scan; }
            else                            { o[0] = 0;     o[1] = 0;    }
            break;
        default:
            break;
        }
    }
    return 0;
}

/* ---- DPMI page lock -----------------------------------------------------
 * 0x3666C "lock linear region": a no-op on a flat host (bgm.c pins FDMUS). */
void fd2_mem_lock(void *addr, unsigned len)
{
    (void)addr; (void)len;
}

/* ---- CRT wrappers referenced by the restored code -----------------------
 * j___delay is the Watcom jump-table alias for delay; the game calls it from
 * ev_script.c etc. */
int j___delay(int ms)
{
    plat_sleep_ms((unsigned)ms);
    return 0;
}

/* ---- diagnostics: log an unhandled Windows exception -------------------
 * A GUI-subsystem exe dies silently on an access violation; this prints the
 * code and faulting address to the log so a in-game crash is actionable. */
#if defined(_WIN32)
#include <windows.h>
static LONG WINAPI fd2_unhandled(EXCEPTION_POINTERS *ep)
{
    fprintf(stderr, "fd2: CRASH code=%08lX addr=%p\n",
            (unsigned long)ep->ExceptionRecord->ExceptionCode,
            ep->ExceptionRecord->ExceptionAddress);
    fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}
void fd2_install_crash_handler(void) { SetUnhandledExceptionFilter(fd2_unhandled); }
#else
void fd2_install_crash_handler(void) { }
#endif
