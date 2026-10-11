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
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#endif

#include "fd2_video.h"
#include "fd2_audio.h"
#include "platform.h"
#include "audio.h"
#include "synth.h"
#include "../src/game/fd2_game.h"   /* boot.h: fd2_game_main, globals */

extern void fd2_data_init(void);
void fd2_install_crash_handler(void);

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

static int g_last_off = -1;

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
    g_last_off = tail;
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

static volatile unsigned g_frames;

/* Diagnostics: frame rate + BIOS tick rate once every 2 s. */
static void stat_thread(void *arg)
{
    unsigned f0 = 0; uint32_t t0 = 0; int first = 1;
    (void)arg;
    while (!g_quit) {
        plat_sleep_ms(2000);
        {
            unsigned f = g_frames;
            uint32_t tk = *(volatile uint32_t *)(lowmem() + 0x46C);
            if (!first) {
                printf("fd2: fps=%u  tick_rate=%.1f Hz", (f - f0) / 2,
                       (tk - t0) / 2.0);
                putchar(10);
                fflush(stdout);
            }
            f0 = f; t0 = tk; first = 0;
        }
    }
}

static void game_thread(void *arg)
{
    int argc = *(int *)arg;
    (void)argc;
    fd2_game_main(0, NULL);
    g_quit = 1;
}

/* Scripted keys for testing: FD2_KEYS="1C,50,1C" (hex BIOS scan codes),
 * injected FD2_KEY_DELAY ms apart (default 800).  An extended key (arrow /
 * nav) is auto-prefixed with the 0xE0 ascii byte the game normalises. */
static uint8_t g_keys[64];
static int     g_nkeys;
static int     g_key_delay = 800;

static uint8_t key_ascii(uint8_t sc)
{
    switch (sc) {
    case 0x1C: return 0x0D;   /* Enter  */
    case 0x39: return 0x20;   /* Space  */
    case 0x01: return 0x1B;   /* Esc    */
    case 0x0E: return 0x08;   /* Backsp */
    default:   return 0;
    }
}

static int is_extended_scan(uint8_t sc)
{
    return sc == 0x48 || sc == 0x50 || sc == 0x4B || sc == 0x4D ||
           sc == 0x47 || sc == 0x4F || sc == 0x49 || sc == 0x51 ||
           sc == 0x52 || sc == 0x53;
}

static void key_thread(void *arg)
{
    int i;
    (void)arg;
    plat_sleep_ms(1200);                 /* let the title screen appear */
    for (i = 0; i < g_nkeys && !g_quit; i++) {
        fd2_kbd_push(g_keys[i], is_extended_scan(g_keys[i]) ? 0xE0 : key_ascii(g_keys[i]));
        printf("fd2: autokey scan=%02X\n", (unsigned)g_keys[i]);
        plat_sleep_ms(g_key_delay);
    }
}

static void parse_keys(void)
{
    const char *s = getenv("FD2_KEYS");
    const char *d = getenv("FD2_KEY_DELAY");
    if (d) g_key_delay = atoi(d);
    if (!s || !s[0]) return;
    while (*s && g_nkeys < 64) {
        char *end;
        long v = strtol(s, &end, 16);
        if (end == s) break;
        g_keys[g_nkeys++] = (uint8_t)v;
        s = (*end == ',') ? end + 1 : end;
    }
}

int fd2_engine_init(int argc, char **argv)
{
    const char *log = getenv("FD2_LOG");
    if (!log) log = "fd2engine.log";
    if (freopen(log, "w", stdout)) { }
    if (freopen(log, "a", stderr)) { }
    setvbuf(stdout, NULL, _IONBF, 0);

    (void)argc; (void)argv;
#if defined(_WIN32)
    timeBeginPeriod(1);   /* 1 ms timer: BIOS tick really runs at 18.2 Hz */
#endif
    fd2_install_crash_handler();
    fd2_data_init();
    dos_lowmem_base = (uintptr_t)fd2_lowmem;
    bda16_set(KB_HEAD, KB_BUF);
    bda16_set(KB_TAIL, KB_BUF);
    fd2_video_default_palette();
    synth_set_bank_path(NULL);           /* default timbre until a DLS is set */
    audio_init(11025);
    ail_set_format(11025, 8, 0);
    ail_set_master_volume(100);
    parse_keys();
    printf("fd2: engine init (log=%s, keys=%d)\n", log, g_nkeys);
    return 0;
}

void fd2_engine_start(void)
{
    static int one = 1;
    plat_thread(game_thread, &one);
    plat_thread(tick_thread, NULL);
    plat_thread(stat_thread, NULL);
    if (g_nkeys)
        plat_thread(key_thread, NULL);
}

void fd2_engine_frame(void)
{
    g_frames++;
    fd2_video_frame();
}

void fd2_engine_key(uint8_t scan, uint8_t ascii)
{
    fd2_kbd_push(scan, ascii);
}

void fd2_engine_key_set_ascii(uint8_t ascii)
{
    if (g_last_off >= 0)
        lowmem()[0x400 + g_last_off + 1] = ascii;
}

void fd2_engine_shutdown(void)
{
    g_quit = 1;
}
