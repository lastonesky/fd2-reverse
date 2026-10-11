/* fd2_kernel.c - native host kernel (see fd2_kernel.h).
 *
 * port/host.c ran the original machine code through an LE loader and a guest
 * thread; here the game is the restored C, so the kernel is much smaller:
 *
 *   init   : fd2_data_init() (segment mirrors) + low-memory base + palette
 *            + audio device + AIL_startup
 *   start  : game thread (fd2_game_main) + 18.2 Hz BIOS tick thread
 *   frame  : fd2_video_frame()
 *   input  : host key -> BDA ring (0040:001A head / 001C tail / 001E buffer)
 */
#include "fd2_kernel.h"

#include <stdint.h>
#include <stdio.h>

#include "fd2_video.h"
#include "fd2_audio.h"
#include "platform.h"
#include "audio.h"
#include "synth.h"
#include "../src/game/fd2_game.h"   /* boot.h: fd2_game_main, globals */

extern void fd2_data_init(void);

uintptr_t dos_lowmem_base;

/* BDA keyboard ring (offsets are the BIOS ones, relative to 0x400). */
#define KB_HEAD   0x41A
#define KB_TAIL   0x41C
#define KB_BUF    0x41E
#define KB_END    0x43E

static uint8_t *lowmem(void)
{
    return (uint8_t *)dos_lowmem_base;
}

static uint16_t bda16(int off)          { return *(volatile uint16_t *)(lowmem() + off); }
static void     bda16_set(int off, uint16_t v) { *(volatile uint16_t *)(lowmem() + off) = v; }

void fd2_kbd_push(uint8_t scan, uint8_t ascii)
{
    uint16_t tail = bda16(KB_TAIL);
    uint16_t next = (uint16_t)(tail + 2);

    if (next >= KB_END)
        next = KB_BUF;
    if (next == bda16(KB_HEAD))
        return;                          /* full: drop, like hardware       */
    lowmem()[0x400 + tail]     = scan;
    lowmem()[0x400 + tail + 1] = ascii;
    bda16_set(KB_TAIL, next);
}

int fd2_kbd_pop(uint8_t *scan, uint8_t *ascii)
{
    uint16_t head = bda16(KB_HEAD);

    if (head == bda16(KB_TAIL))
        return 0;
    *scan  = lowmem()[0x400 + head];
    *ascii = lowmem()[0x400 + head + 1];
    {
        uint16_t next = (uint16_t)(head + 2);
        if (next >= KB_END)
            next = KB_BUF;
        bda16_set(KB_HEAD, next);
    }
    return 1;
}

void fd2_kbd_wait(uint8_t *scan, uint8_t *ascii)
{
    while (!fd2_kbd_pop(scan, ascii))
        plat_sleep_ms(1);
}

/* ---- threads ----------------------------------------------------------- */

static volatile int g_quit;

static void tick_thread(void *arg)
{
    (void)arg;
    while (!g_quit) {
        (*(volatile uint32_t *)(lowmem() + 0x46C))++;
        plat_sleep_ms(55);               /* 18.2 Hz */
    }
}

static void game_thread(void *arg)
{
    int argc = *(int *)arg;
    (void)argc;
    fd2_game_main(0, NULL);
    g_quit = 1;
}

int fd2_engine_init(int argc, char **argv)
{
    (void)argv;
    fd2_data_init();
    dos_lowmem_base = (uintptr_t)fd2_lowmem;
    bda16_set(KB_HEAD, KB_BUF);
    bda16_set(KB_TAIL, KB_BUF);
    fd2_video_default_palette();
    synth_set_bank_path(NULL);           /* default timbre until a DLS is set */
    audio_init(11025);
    ail_set_format(11025, 8, 0);
    ail_set_master_volume(100);
    return 0;
}

void fd2_engine_start(void)
{
    static int one = 1;
    plat_thread(game_thread, &one);
    plat_thread(tick_thread, NULL);
}

void fd2_engine_frame(void)
{
    fd2_video_frame();
}

void fd2_engine_key(uint8_t scan, uint8_t ascii)
{
    fd2_kbd_push(scan, ascii);
}

void fd2_engine_shutdown(void)
{
    g_quit = 1;
}
