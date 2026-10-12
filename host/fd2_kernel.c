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

/* The BDA ring is touched by several threads (the entry layer / typematic /
 * keyplay producers and the game thread's INT 16h consumer), so every access
 * goes through one mutex the way the real BIOS owned it alone.  The
 * condition variable wakes the game thread when a key arrives or when the
 * 18.2 Hz tick advances (stage 1: blocking waits instead of polling). */
static plat_mutex g_kbd_lock;
static plat_cond  g_kbd_cond;
volatile int      fd2_wait_depth;      /* >0 while parked in a wait */

void fd2_kbd_push(uint8_t scan, uint8_t ascii)
{
    uint16_t tail, next;

    plat_mutex_lock(&g_kbd_lock);
    tail = bda16(KB_TAIL);
    next = (uint16_t)(tail + 2);
    if (next >= KB_END)
        next = KB_BUF;
    if (next == bda16(KB_HEAD)) {
        plat_mutex_unlock(&g_kbd_lock);
        return;                          /* full: drop, like hardware       */
    }
    lowmem()[0x400 + tail]     = scan;
    lowmem()[0x400 + tail + 1] = ascii;
    g_last_off = tail;
    bda16_set(KB_TAIL, next);
    plat_cond_broadcast(&g_kbd_cond);
    plat_mutex_unlock(&g_kbd_lock);
}

int fd2_kbd_pop(uint8_t *scan, uint8_t *ascii)
{
    uint16_t head;
    int ok = 0;

    plat_mutex_lock(&g_kbd_lock);
    head = bda16(KB_HEAD);
    if (head != bda16(KB_TAIL)) {
        uint16_t next = (uint16_t)(head + 2);

        *scan  = lowmem()[0x400 + head];
        *ascii = lowmem()[0x400 + head + 1];
        if (next >= KB_END)
            next = KB_BUF;
        bda16_set(KB_HEAD, next);
        ok = 1;
    }
    plat_mutex_unlock(&g_kbd_lock);
    return ok;
}

/* kbd_flush (0x4E381): drop everything pending (tail = head). */
void fd2_kbd_flush(void)
{
    plat_mutex_lock(&g_kbd_lock);
    bda16_set(KB_TAIL, bda16(KB_HEAD));
    plat_mutex_unlock(&g_kbd_lock);
}

void fd2_kbd_wait(uint8_t *scan, uint8_t *ascii)
{
    uint16_t head;

    fd2_video_publish();              /* frame complete: hand it to the presenter */
    plat_mutex_lock(&g_kbd_lock);
    fd2_wait_depth++;
    while (bda16(KB_HEAD) == bda16(KB_TAIL))
        plat_cond_wait(&g_kbd_cond, &g_kbd_lock);
    head = bda16(KB_HEAD);
    *scan  = lowmem()[0x400 + head];
    *ascii = lowmem()[0x400 + head + 1];
    {
        uint16_t next = (uint16_t)(head + 2);
        if (next >= KB_END)
            next = KB_BUF;
        bda16_set(KB_HEAD, next);
    }
    fd2_wait_depth--;
    plat_mutex_unlock(&g_kbd_lock);
}

/* Block until the 16-bit BIOS tick advances (what the game's `while (tick ==
 * last);` loops used to spin on). */
void fd2_wait_tick(void)
{
    uint16_t t;

    fd2_video_publish();              /* frame complete: hand it to the presenter */
    plat_mutex_lock(&g_kbd_lock);
    fd2_wait_depth++;
    t = bda16(0x46C);
    while (bda16(0x46C) == t)
        plat_cond_wait(&g_kbd_cond, &g_kbd_lock);
    fd2_wait_depth--;
    plat_mutex_unlock(&g_kbd_lock);
}

/* Block until the tick advances or a key becomes pending - the idle loop of
 * every "animate while waiting for input" screen. */
void fd2_wait_tick_or_key(void)
{
    uint16_t t;

    fd2_video_publish();              /* frame complete: hand it to the presenter */
    plat_mutex_lock(&g_kbd_lock);
    fd2_wait_depth++;
    t = bda16(0x46C);
    while (bda16(0x46C) == t && bda16(KB_HEAD) == bda16(KB_TAIL))
        plat_cond_wait(&g_kbd_cond, &g_kbd_lock);
    fd2_wait_depth--;
    plat_mutex_unlock(&g_kbd_lock);
}

/* Game-thread delay(): publish the finished frame, then sleep while marking
 * the thread as parked so a long fixed pause (title screen, cutscene) is not
 * mistaken for a hang by the watchdog. */
void fd2_delay_ms(unsigned ms)
{
    fd2_video_publish();
    fd2_wait_depth++;
    plat_sleep_ms(ms);
    fd2_wait_depth--;
}

/* ---- threads ----------------------------------------------------------- */

static volatile int g_quit;

#if defined(_WIN32)
static DWORD g_game_tid;             /* set by the game thread itself */
#endif

static void tick_thread(void *arg)
{
    /* Drift-free 18.2 Hz: every tick is scheduled against an absolute
     * deadline from the monotonic clock instead of accumulating sleep error.
     * The word is advanced under the ring lock and wakes the waiters. */
    const uint64_t period = 54945u;      /* 1e6 / 18.2 Hz */
    uint64_t t0 = plat_now_us();
    uint64_t n = 0;
    (void)arg;
    while (!g_quit) {
        uint64_t due;

        n++;
        due = t0 + n * period;
        while (!g_quit && plat_now_us() < due)
            plat_sleep_us(due - plat_now_us());
        if (g_quit)
            break;
        plat_mutex_lock(&g_kbd_lock);
        (*(volatile uint32_t *)(lowmem() + 0x46C))++;
        plat_cond_broadcast(&g_kbd_cond);
        plat_mutex_unlock(&g_kbd_lock);
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
#if defined(_WIN32)
    g_game_tid = (DWORD)plat_thread_id();
#endif
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

/* ---- keystroke recording + stall watchdog (diagnostics) ---------------
 * FD2_KEYLOG=<path> writes every real-key event the game sees as
 *   <ms>,<scan>,<ascii>
 * one per line (ms since engine init, scan/ascii hex), flushed at once so a
 * freeze/crash keeps the record.  The matching scenario in the log is
 * "fd2: key @... ms ...".
 *
 * The watchdog samples two game-thread heartbeats (the boot-loop counter and
 * the count of published frames) plus the "parked in a wait" depth.  A long
 * title screen / cutscene keeps publishing (j___delay publishes too) and a
 * menu waiting for input is parked, so neither is mistaken for a hang.  If
 * neither heartbeat advances for FD2_STALL_MS (default 10000) AND the thread
 * is not parked for that whole time, it is a real hang: suspend the game
 * thread, capture RIP/RSP, scan its stack for return addresses into the exe,
 * and dump the BDA key ring and map globals.  Default is log-only; set
 * FD2_HANG_EXIT=1 to also close the window. */
extern volatile unsigned fd2_game_progress;
static FILE    *g_keylog;
static uint64_t g_keylog_base;
static unsigned g_stall_ms = 10000;
static int      g_hang_exit;      /* FD2_HANG_EXIT=1: close on a detected hang */

static void keylog_note(uint8_t scan, uint8_t ascii)
{
    uint64_t ms = plat_now_ms() - g_keylog_base;

    printf("fd2: key @%llu ms scan=%02X ascii=%02X\n",
           (unsigned long long)ms, (unsigned)scan, (unsigned)ascii);
    if (g_keylog) {
        fprintf(g_keylog, "%llu,%02X,%02X\n",
                (unsigned long long)ms, (unsigned)scan, (unsigned)ascii);
        fflush(g_keylog);
    }
}

/* ---- BIOS typematic auto-repeat emulation -----------------------------
 * The originals read INT 16h, so a held direction key kept moving because the
 * keyboard controller repeated its make code (typematic: ~500 ms delay, then
 * ~10.9 chars/s).  The entry layer deliberately forwards only the first make
 * (sokol key_repeat / the g_down[] dedup in main_sokol.c), so without this
 * the game would see one make per press and move exactly one cell.
 *
 * We reproduce the hardware rule "the last key pressed repeats": a make arms
 * the slot, a matching break disarms it, and a repeat thread pushes further
 * make codes straight into the BDA ring (not through keylog_note - those are
 * synthetic, the keylog must stay the real input).  Tune with
 * FD2_TYPEMATIC_DELAY (ms, default 500) and FD2_TYPEMATIC_RATE (ms, default
 * 90); FD2_TYPEMATIC=0 disables it. */
static int      g_rep_enabled = 1;
static int      g_rep_debug;
static unsigned g_rep_delay = 500;
static unsigned g_rep_period = 90;
static uint8_t  g_rep_scan;
static uint8_t  g_rep_ascii;
static int      g_rep_held;
static uint64_t g_rep_due;
static plat_mutex g_rep_lock;    /* guards the typematic slot (2 threads) */

static void repeat_note(uint8_t scan, uint8_t ascii)
{
    if (!g_rep_enabled)
        return;
    plat_mutex_lock(&g_rep_lock);
    if (scan & 0x80) {                       /* break: release the slot   */
        if (g_rep_held && g_rep_scan == (uint8_t)(scan & 0x7F))
            g_rep_held = 0;
        plat_mutex_unlock(&g_rep_lock);
        return;
    }
    g_rep_scan  = scan;                      /* last key pressed repeats  */
    g_rep_ascii = ascii;
    g_rep_held  = 1;
    g_rep_due   = plat_now_ms() + g_rep_delay;
    plat_mutex_unlock(&g_rep_lock);
}

static void repeat_thread(void *arg)
{
    (void)arg;
    for (;;) {
        int      fire;
        uint8_t  scan;
        uint8_t  ascii;

        plat_sleep_ms(10);
        if (g_quit)
            return;
        plat_mutex_lock(&g_rep_lock);
        fire  = g_rep_held && plat_now_ms() >= g_rep_due;
        scan  = g_rep_scan;
        ascii = g_rep_ascii;
        if (fire)
            g_rep_due = plat_now_ms() + g_rep_period;
        plat_mutex_unlock(&g_rep_lock);
        if (fire) {
            if (g_rep_debug)
                printf("fd2: repeat scan=%02X ascii=%02X\n",
                       (unsigned)scan, (unsigned)ascii);
            fd2_kbd_push(scan, ascii);
        }
    }
}

#if defined(_WIN32)
/* Capture the (suspended) game thread's stack.  Release x64 may omit frame
 * pointers, so instead of walking a frame chain we scan the raw stack for
 * 8-byte words inside the module's code range; the /MAP file resolves them. */
static void dump_game_stack(const char *why)
{
    HANDLE   th;
    CONTEXT  c;
    char     buf[8192];
    int      n = 0, i, printed = 0;
    uintptr_t base, text, tend, sp, v;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    SIZE_T   rd;
    HANDLE   h;
    DWORD    wrote;
    uint16_t hd, tl;

    if (!g_game_tid)
        return;
    th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT |
                    THREAD_QUERY_INFORMATION, FALSE, g_game_tid);
    if (!th)
        return;
    SuspendThread(th);
    c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    GetThreadContext(th, &c);

    base = (uintptr_t)GetModuleHandleA(NULL);
    dos  = (IMAGE_DOS_HEADER *)base;
    nt   = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    text  = base + nt->OptionalHeader.BaseOfCode;
    tend  = text + nt->OptionalHeader.SizeOfCode;
    sp    = (uintptr_t)c.Rsp;

    n += wsprintfA(buf + n,
                   "fd2: STALL %s progress=%u frames=%u rip=%p rsp=%p rbp=%p\r\n",
                   why, (unsigned)fd2_game_progress, (unsigned)g_frames,
                   (void *)c.Rip, (void *)c.Rsp, (void *)c.Rbp);
    n += wsprintfA(buf + n, "  stack code-returns (module text %p..%p):\r\n",
                   (void *)text, (void *)tend);
    for (i = 0; i < 0x10000 && n < (int)sizeof buf - 32; i++, sp += 8) {
        if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)sp, &v, 8, &rd)
            || rd != 8)
            break;
        if (v >= text && v < tend) {
            n += wsprintfA(buf + n, "  #%02d %p\r\n", printed++, (void *)v);
        }
    }
    hd = *(volatile uint16_t *)(lowmem() + KB_HEAD);
    tl = *(volatile uint16_t *)(lowmem() + KB_TAIL);
    n += wsprintfA(buf + n, "  kbd ring head=%04X tail=%04X busy=%d\r\n",
                   hd, tl, (int)(uint16_t)((tl - hd) & 0x1FFu));
    {
        extern unsigned char *dword_53A51, *dword_53A5D, *dword_53A69;
        extern unsigned char fd2_data1[];
        unsigned ac1, ac5, mode;
        int aa9, aad;
        memcpy(&ac1,  fd2_data1 + 0x3AC1, 4);
        memcpy(&ac5,  fd2_data1 + 0x3AC5, 4);
        memcpy(&aa9,  fd2_data1 + 0x3AA9, 4);
        memcpy(&aad,  fd2_data1 + 0x3AAD, 4);
        memcpy(&mode, fd2_data1 + 0x3C03, 4);
        n += wsprintfA(buf + n,
            "  map: 53A51=%p 53A5D=%p 53A69=%p AC1=%u AC5=%u AA9=%d AAD=%d mode=%u\r\n",
            (void *)dword_53A51, (void *)dword_53A5D, (void *)dword_53A69,
            ac1, ac5, aa9, aad, mode);
    }
    ResumeThread(th);
    CloseHandle(th);

    fwrite(buf, 1, (size_t)n, stdout);
    fflush(stdout);
    h = CreateFileA("fd2_stall.log", FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
                    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        SetFilePointer(h, 0, NULL, FILE_END);
        WriteFile(h, buf, (DWORD)n, &wrote, NULL);
        CloseHandle(h);
    }
}
#else
static void dump_game_stack(const char *why) { (void)why; }
#endif

static void stall_thread(void *arg)
{
    unsigned last_prog = 0, last_pub = 0;
    unsigned stalled_for = 0, idle_for = 0;
    int      announced = 0;
    (void)arg;
    for (;;) {
        plat_sleep_ms(1000);
        if (g_quit)
            return;
        if (fd2_wait_depth > 0)          /* parked on tick/keys/delay: fine */
            idle_for = 0;
        else
            idle_for += 1000;
        {
            /* Two liveness heartbeats: the boot loop counter and the published
             * frame counter.  A long title screen / cutscene keeps publishing
             * (j___delay publishes too), so it is not mistaken for a hang. */
            unsigned p = fd2_game_progress;
            unsigned q = fd2_video_publish_count;

            if (p != last_prog || q != last_pub) {
                last_prog = p;
                last_pub = q;
                stalled_for = 0;
                announced = 0;
                continue;
            }
            stalled_for += 1000;
            /* Genuine hang = neither heartbeat for the limit AND not parked in
             * any wait for that whole time.  Before the main loop starts the
             * game is loading resources, so allow a longer window. */
            {
                unsigned limit = (last_prog == 0) ? 30000u : g_stall_ms;

                if (stalled_for >= limit && idle_for >= g_stall_ms && !announced) {
                    printf("fd2: HANG detected (no frame for %u ms, not parked)%s\n",
                           stalled_for, g_hang_exit ? "; closing window" : "");
                    dump_game_stack("no frame/heartbeat (not parked in a wait)");
                    announced = 1;
                    if (g_hang_exit)
                        g_quit = 1;      /* opt-in; default is log-only  */
                }
            }
        }
    }
}

/* ---- replay (FD2_KEYPLAY=<path>) -------------------------------------
 * Feeds back a file written by FD2_KEYLOG ("<ms>,<scan>,<ascii>" lines) on
 * its original absolute timeline, so a reported freeze can be reproduced
 * exactly. */
#define KEYPLAY_MAX 1024
typedef struct { uint64_t ms; uint8_t scan, ascii; } fd2_keyev;
static fd2_keyev g_play[KEYPLAY_MAX];
static int       g_nplay;

static void parse_keyplay(void)
{
    const char *path = getenv("FD2_KEYPLAY");
    FILE *f;
    char  line[128];

    if (!path || !path[0])
        return;
    f = fopen(path, "r");
    if (!f) {
        printf("fd2: keyplay: cannot open %s\n", path);
        return;
    }
    while (fgets(line, sizeof line, f) && g_nplay < KEYPLAY_MAX) {
        unsigned long long ms;
        unsigned sc, asc;
        if (sscanf(line, "%llu,%x,%x", &ms, &sc, &asc) == 3) {
            g_play[g_nplay].ms   = ms;
            g_play[g_nplay].scan = (uint8_t)sc;
            g_play[g_nplay].ascii= (uint8_t)asc;
            g_nplay++;
        }
    }
    fclose(f);
    printf("fd2: keyplay: %d keys from %s\n", g_nplay, path);
}

static void keyplay_thread(void *arg)
{
    int i;
    (void)arg;
    for (i = 0; i < g_nplay && !g_quit; i++) {
        while (!g_quit && plat_now_ms() - g_keylog_base < g_play[i].ms)
            plat_sleep_ms(1);
        fd2_engine_key(g_play[i].scan, g_play[i].ascii);
    }
    printf("fd2: keyplay finished (%d keys)\n", g_nplay);
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
    plat_mutex_init(&g_kbd_lock);
    plat_mutex_init(&g_rep_lock);
    plat_cond_init(&g_kbd_cond);
    bda16_set(KB_HEAD, KB_BUF);
    bda16_set(KB_TAIL, KB_BUF);
    fd2_video_default_palette();
    fd2_video_init();
    synth_set_bank_path(NULL);           /* default timbre until a DLS is set */
    audio_init(11025);
    ail_set_format(11025, 8, 0);
    ail_set_master_volume(100);
    parse_keys();
    parse_keyplay();
    {
        const char *kl = getenv("FD2_KEYLOG");
        const char *sm = getenv("FD2_STALL_MS");
        const char *tp = getenv("FD2_TYPEMATIC");
        const char *td = getenv("FD2_TYPEMATIC_DELAY");
        const char *tr = getenv("FD2_TYPEMATIC_RATE");
        if (sm) g_stall_ms = (unsigned)atoi(sm);
        if (td) g_rep_delay = (unsigned)atoi(td);
        if (tr) g_rep_period = (unsigned)atoi(tr);
        if (getenv("FD2_HANG_EXIT"))
            g_hang_exit = 1;
        if (getenv("FD2_TYPEMATIC_DEBUG"))
            g_rep_debug = 1;
        if (tp && atoi(tp) == 0)
            g_rep_enabled = 0;
        g_keylog_base = plat_now_ms();
        if (kl && kl[0]) {
            g_keylog = fopen(kl, "w");
            if (!g_keylog)
                printf("fd2: keylog: cannot write %s\n", kl);
            else
                printf("fd2: keylog -> %s (<ms>,<scan>,<ascii> per line)\n", kl);
        }
    }
    printf("fd2: engine init (log=%s, keys=%d)\n", log, g_nkeys);
    return 0;
}

void fd2_engine_start(void)
{
    static int one = 1;
    if (plat_thread(game_thread, &one) != 0) {
        printf("fd2: failed to start the game thread\n");
        g_quit = 1;              /* frame_cb closes instead of a black window */
        return;
    }
    plat_thread(tick_thread, NULL);
    plat_thread(stat_thread, NULL);
    plat_thread(stall_thread, NULL);
    if (g_rep_enabled)
        plat_thread(repeat_thread, NULL);
    if (g_nkeys)
        plat_thread(key_thread, NULL);
    if (g_nplay)
        plat_thread(keyplay_thread, NULL);
}

void fd2_engine_frame(void)
{
    g_frames++;
    fd2_video_frame();
}

int fd2_engine_game_over(void)
{
    return g_quit;
}

void fd2_engine_key(uint8_t scan, uint8_t ascii)
{
    keylog_note(scan, ascii);
    repeat_note(scan, ascii);
    /* The real BIOS INT 9 handler stores only MAKE codes in the 0040:001E
     * ring (release codes are consumed internally); INT 16h never returns
     * them.  Pushing the break as well made the game see a phantom second
     * key - e.g. one Enter closed the NPC info panel / skipped the command
     * menu, and its ascii byte 0x0D even read as Enter in AL-based checks.
     * Keep the break for the typematic state above, drop it from the ring. */
    if (scan & 0x80)
        return;
    fd2_kbd_push(scan, ascii);
}

void fd2_engine_key_set_ascii(uint8_t ascii)
{
    plat_mutex_lock(&g_kbd_lock);
    if (g_last_off >= 0)
        lowmem()[0x400 + g_last_off + 1] = ascii;
    plat_mutex_unlock(&g_kbd_lock);
}

void fd2_engine_shutdown(void)
{
    g_quit = 1;
    if (g_keylog) {
        fclose(g_keylog);
        g_keylog = NULL;
    }
}
