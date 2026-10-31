/* main_sokol.c - sokol_app entry layer for the FD2 native host.
 *
 * Same contract as main_win32.c (see host.h), driven by sokol callbacks
 * instead of a Win32 message pump:
 *
 *     main()  -> host_init() -> sapp_run()
 *                   init_cb    : render_init() + host_start()
 *                   frame_cb   : fd2_engine_frame() (+ --wshot capture)
 *                   event_cb   : key events -> fd2_engine_key()
 *                   cleanup_cb : render_shutdown() + host_shutdown()
 *
 * SOKOL_NO_ENTRY (defined in sokol_impl.c as well) keeps sokol from
 * hijacking main(), so entry.c still reserves the game address space before
 * the CRT allocates anything on Windows. On POSIX there is no fd2_entry (the
 * low window is free, le.c maps it explicitly), so a plain main() is used.
 *
 * Key translation has two implementations:
 *
 *   Windows: sapp_keycode -> VK (sapp_to_vk), then the same
 *            MapVirtualKeyA/ToAscii path main_win32.c uses - so both Win32
 *            entry layers generate identical keystrokes.
 *   POSIX:   sapp_keycode -> fr_key (src/keys.h) -> BIOS (scan, ascii); the
 *            character comes from sokol's SAPP_EVENTTYPE_CHAR event (its X11
 *            backend runs XLookupString internally), which replaces ToAscii.
 *
 * The window is sokol_app's: on Linux that is an X11 client, which runs
 * unchanged on a Wayland desktop through XWayland (docs/BACKEND.md §13.11). */

#define SOKOL_NO_ENTRY
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fd2_kernel.h"
#include "host_args.h"
#include "platform.h"
#include "render.h"
#include "keys.h"
#include "sokol_app.h"
#include "sokol_log.h"

#if defined(_WIN32)
#include <windows.h>
#endif

static render_desc g_rd;

/* --autoquit-ms=<ms> (or FD2_AUTOQUIT_MS): close the window automatically after this many
 * milliseconds.  Used when launching the engine for a short test/demo run so
 * an unattended game does not keep playing audio forever. */
static uint64_t g_autoquit_ms = 0;
static uint64_t g_t0_ms = 0;

#if defined(_WIN32)
/* -------------------------------------------------------------------------
 * Win32 path: keep the exact translation main_win32.c uses.
 * ---------------------------------------------------------------------- */

static int g_down[256];              /* VK held state, to drop key repeats */
static uint8_t g_down_scan[256];     /* scan code used on the make event    */

/* sapp_keycode -> Windows virtual key. The ASCII range maps 1:1 only for
 * letters, digits and space; punctuation and everything >= 256 does not. */
static int sapp_to_vk(int k)
{
    if ((k >= 'A' && k <= 'Z') || (k >= '0' && k <= '9') || k == ' ')
        return k;

    switch (k) {
    case SAPP_KEYCODE_ESCAPE:       return VK_ESCAPE;
    case SAPP_KEYCODE_ENTER:        return VK_RETURN;
    case SAPP_KEYCODE_TAB:          return VK_TAB;
    case SAPP_KEYCODE_BACKSPACE:    return VK_BACK;
    case SAPP_KEYCODE_INSERT:       return VK_INSERT;
    case SAPP_KEYCODE_DELETE:       return VK_DELETE;
    case SAPP_KEYCODE_RIGHT:        return VK_RIGHT;
    case SAPP_KEYCODE_LEFT:         return VK_LEFT;
    case SAPP_KEYCODE_DOWN:         return VK_DOWN;
    case SAPP_KEYCODE_UP:           return VK_UP;
    case SAPP_KEYCODE_PAGE_UP:      return VK_PRIOR;
    case SAPP_KEYCODE_PAGE_DOWN:    return VK_NEXT;
    case SAPP_KEYCODE_HOME:         return VK_HOME;
    case SAPP_KEYCODE_END:          return VK_END;
    case SAPP_KEYCODE_CAPS_LOCK:    return VK_CAPITAL;
    case SAPP_KEYCODE_SCROLL_LOCK:  return VK_SCROLL;
    case SAPP_KEYCODE_NUM_LOCK:     return VK_NUMLOCK;
    case SAPP_KEYCODE_PRINT_SCREEN: return VK_SNAPSHOT;
    case SAPP_KEYCODE_PAUSE:        return VK_PAUSE;
    case SAPP_KEYCODE_APOSTROPHE:   return VK_OEM_7;   /* ' */
    case SAPP_KEYCODE_COMMA:        return VK_OEM_COMMA;
    case SAPP_KEYCODE_MINUS:        return VK_OEM_MINUS;
    case SAPP_KEYCODE_PERIOD:       return VK_OEM_PERIOD;
    case SAPP_KEYCODE_SLASH:        return VK_OEM_2;
    case SAPP_KEYCODE_SEMICOLON:    return VK_OEM_1;
    case SAPP_KEYCODE_EQUAL:        return VK_OEM_PLUS;
    case SAPP_KEYCODE_LEFT_BRACKET: return VK_OEM_4;
    case SAPP_KEYCODE_BACKSLASH:    return VK_OEM_5;
    case SAPP_KEYCODE_RIGHT_BRACKET:return VK_OEM_6;
    case SAPP_KEYCODE_GRAVE_ACCENT: return VK_OEM_3;
    case SAPP_KEYCODE_KP_0:         return VK_NUMPAD0;
    case SAPP_KEYCODE_KP_1:         return VK_NUMPAD1;
    case SAPP_KEYCODE_KP_2:         return VK_NUMPAD2;
    case SAPP_KEYCODE_KP_3:         return VK_NUMPAD3;
    case SAPP_KEYCODE_KP_4:         return VK_NUMPAD4;
    case SAPP_KEYCODE_KP_5:         return VK_NUMPAD5;
    case SAPP_KEYCODE_KP_6:         return VK_NUMPAD6;
    case SAPP_KEYCODE_KP_7:         return VK_NUMPAD7;
    case SAPP_KEYCODE_KP_8:         return VK_NUMPAD8;
    case SAPP_KEYCODE_KP_9:         return VK_NUMPAD9;
    case SAPP_KEYCODE_KP_DECIMAL:   return VK_DECIMAL;
    case SAPP_KEYCODE_KP_DIVIDE:    return VK_DIVIDE;
    case SAPP_KEYCODE_KP_MULTIPLY:  return VK_MULTIPLY;
    case SAPP_KEYCODE_KP_SUBTRACT:  return VK_SUBTRACT;
    case SAPP_KEYCODE_KP_ADD:       return VK_ADD;
    case SAPP_KEYCODE_KP_ENTER:     return VK_RETURN;
    case SAPP_KEYCODE_LEFT_SHIFT:   return VK_LSHIFT;
    case SAPP_KEYCODE_LEFT_CONTROL: return VK_LCONTROL;
    case SAPP_KEYCODE_LEFT_ALT:     return VK_LMENU;
    case SAPP_KEYCODE_RIGHT_SHIFT:  return VK_RSHIFT;
    case SAPP_KEYCODE_RIGHT_CONTROL:return VK_RCONTROL;
    case SAPP_KEYCODE_RIGHT_ALT:    return VK_RMENU;
    default: break;
    }
    if (k >= SAPP_KEYCODE_F1 && k <= SAPP_KEYCODE_F12)
        return VK_F1 + (k - SAPP_KEYCODE_F1);
    return 0;
}

/* Keys the BIOS reports with an 0xE0 prefix - the same rule main_win32.c
 * applies, so both entry layers produce identical buffer contents. */
static int is_extended_vk(int vk)
{
    switch (vk) {
    case VK_UP: case VK_DOWN: case VK_LEFT: case VK_RIGHT:
    case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT:
    case VK_INSERT: case VK_DELETE:
        return 1;
    default:
        return 0;
    }
}

static uint8_t ascii_for_vk(int vk)
{
    BYTE ks[256];
    WORD ch = 0;
    UINT sc;

    if (is_extended_vk(vk))
        return 0xE0;
    if (!GetKeyboardState(ks))
        return 0;
    sc = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
    if (ToAscii((UINT)vk, sc, ks, &ch, 0) == 1 && ch < 0x100)
        return (uint8_t)ch;
    return 0;
}

/* BIOS scan code for a VK, including the Ctrl/Alt/Shift+F1..F10 forms the
 * game's hidden-shop table uses (0x5E.., 0x68.., 0x54..).  Plain
 * MapVirtualKeyA only returns the unmodified F-key make code (0x3B..0x44). */
static uint8_t scan_for_vk(int vk)
{
    fr_key k = fr_key_from_vk(vk);
    unsigned mods = 0;

    if (k >= FRK_F1 && k <= FRK_F10) {
        if (GetKeyState(VK_CONTROL) & 0x8000) mods |= FR_MOD_CTRL;
        if (GetKeyState(VK_MENU)    & 0x8000) mods |= FR_MOD_ALT;
        if (GetKeyState(VK_SHIFT)   & 0x8000) mods |= FR_MOD_SHIFT;
        return fr_key_scan_mod(k, mods);
    }
    return (uint8_t)MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
}

static void push_vk(int vk, int down)
{
    uint8_t sc;

    if (vk <= 0 || vk >= 256)
        return;
    /* drop repeats and unmatched key-ups: sokol reports auto-repeat as
     * KEY_DOWN with key_repeat = true, Win32 as repeated WM_KEYDOWN */
    if (down) {
        if (g_down[vk]) return;
        g_down[vk] = 1;
        sc = scan_for_vk(vk);
        g_down_scan[vk] = sc;
    } else {
        if (!g_down[vk]) return;
        g_down[vk] = 0;
        sc = g_down_scan[vk];
        if (!sc)
            sc = scan_for_vk(vk);
    }
    fd2_engine_key((uint8_t)(down ? sc : (sc | 0x80)), ascii_for_vk(vk));
}

/* --autokey/--keyplay runs on its own thread and injects by portable key.
 * fr_key -> VK (src/keys_win32.c) -> push_vk(), i.e. exactly the path a real
 * keystroke takes, so a replayed key and a pressed key are the same event. */
void input_post_key(fr_key key)
{
    int vk = fr_key_vk(key);

    if (!vk)
        return;
    printf("host: autokey %s (vk=%02X scan %02X)\n", fr_key_name(key), vk,
           (unsigned)MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC));
    push_vk(vk, 1);
    push_vk(vk, 0);
}

#else /* !_WIN32 */
/* -------------------------------------------------------------------------
 * POSIX path: sokol_keycode -> fr_key -> BIOS (scan, ascii).
 * ---------------------------------------------------------------------- */

static unsigned char g_down[FRK_COUNT]; /* fr_key held state, drop repeats */
static unsigned char g_down_scan[FRK_COUNT]; /* scan used on the make event */
static int           g_char_pending;    /* last make code may still be patched */

/* sapp_keycode -> portable key. sokol's X11 backend builds these codes from
 * XKB key *names*, so they identify a physical key regardless of layout
 * (the same property Windows VKs have via MapVirtualKeyA). */
static fr_key sapp_to_fr_key(int k)
{
    if ((k >= 'A' && k <= 'Z') || (k >= '0' && k <= '9')) {
        char c = (char)k;
        return fr_key_by_name(&c, 1);
    }
    switch (k) {
    case SAPP_KEYCODE_SPACE:        return FRK_SPACE;
    case SAPP_KEYCODE_APOSTROPHE:   return FRK_APOSTROPHE;
    case SAPP_KEYCODE_COMMA:        return FRK_COMMA;
    case SAPP_KEYCODE_MINUS:        return FRK_MINUS;
    case SAPP_KEYCODE_PERIOD:       return FRK_PERIOD;
    case SAPP_KEYCODE_SLASH:        return FRK_SLASH;
    case SAPP_KEYCODE_SEMICOLON:    return FRK_SEMICOLON;
    case SAPP_KEYCODE_EQUAL:        return FRK_EQUAL;
    case SAPP_KEYCODE_LEFT_BRACKET: return FRK_LEFT_BRACKET;
    case SAPP_KEYCODE_BACKSLASH:    return FRK_BACKSLASH;
    case SAPP_KEYCODE_RIGHT_BRACKET:return FRK_RIGHT_BRACKET;
    case SAPP_KEYCODE_GRAVE_ACCENT: return FRK_GRAVE;
    case SAPP_KEYCODE_ESCAPE:       return FRK_ESCAPE;
    case SAPP_KEYCODE_ENTER:        return FRK_RETURN;
    case SAPP_KEYCODE_TAB:          return FRK_TAB;
    case SAPP_KEYCODE_BACKSPACE:    return FRK_BACKSPACE;
    case SAPP_KEYCODE_INSERT:       return FRK_INSERT;
    case SAPP_KEYCODE_DELETE:       return FRK_DELETE;
    case SAPP_KEYCODE_RIGHT:        return FRK_RIGHT;
    case SAPP_KEYCODE_LEFT:         return FRK_LEFT;
    case SAPP_KEYCODE_DOWN:         return FRK_DOWN;
    case SAPP_KEYCODE_UP:           return FRK_UP;
    case SAPP_KEYCODE_PAGE_UP:      return FRK_PAGE_UP;
    case SAPP_KEYCODE_PAGE_DOWN:    return FRK_PAGE_DOWN;
    case SAPP_KEYCODE_HOME:         return FRK_HOME;
    case SAPP_KEYCODE_END:          return FRK_END;
    case SAPP_KEYCODE_CAPS_LOCK:    return FRK_CAPS_LOCK;
    case SAPP_KEYCODE_SCROLL_LOCK:  return FRK_SCROLL_LOCK;
    case SAPP_KEYCODE_NUM_LOCK:     return FRK_NUM_LOCK;
    case SAPP_KEYCODE_PAUSE:        return FRK_NONE;  /* no portable key */
    case SAPP_KEYCODE_PRINT_SCREEN: return FRK_NONE;
    case SAPP_KEYCODE_KP_0:         return FRK_KP_0;
    case SAPP_KEYCODE_KP_1:         return FRK_KP_1;
    case SAPP_KEYCODE_KP_2:         return FRK_KP_2;
    case SAPP_KEYCODE_KP_3:         return FRK_KP_3;
    case SAPP_KEYCODE_KP_4:         return FRK_KP_4;
    case SAPP_KEYCODE_KP_5:         return FRK_KP_5;
    case SAPP_KEYCODE_KP_6:         return FRK_KP_6;
    case SAPP_KEYCODE_KP_7:         return FRK_KP_7;
    case SAPP_KEYCODE_KP_8:         return FRK_KP_8;
    case SAPP_KEYCODE_KP_9:         return FRK_KP_9;
    case SAPP_KEYCODE_KP_DECIMAL:   return FRK_KP_DECIMAL;
    case SAPP_KEYCODE_KP_DIVIDE:    return FRK_KP_DIVIDE;
    case SAPP_KEYCODE_KP_MULTIPLY:  return FRK_KP_MULTIPLY;
    case SAPP_KEYCODE_KP_SUBTRACT:  return FRK_KP_SUBTRACT;
    case SAPP_KEYCODE_KP_ADD:       return FRK_KP_ADD;
    case SAPP_KEYCODE_KP_ENTER:     return FRK_KP_ENTER;
    case SAPP_KEYCODE_LEFT_SHIFT:   return FRK_LEFT_SHIFT;
    case SAPP_KEYCODE_LEFT_CONTROL: return FRK_LEFT_CONTROL;
    case SAPP_KEYCODE_LEFT_ALT:     return FRK_LEFT_ALT;
    case SAPP_KEYCODE_LEFT_SUPER:   return FRK_LEFT_SUPER;
    case SAPP_KEYCODE_RIGHT_SHIFT:  return FRK_RIGHT_SHIFT;
    case SAPP_KEYCODE_RIGHT_CONTROL:return FRK_RIGHT_CONTROL;
    case SAPP_KEYCODE_RIGHT_ALT:    return FRK_RIGHT_ALT;
    case SAPP_KEYCODE_RIGHT_SUPER:  return FRK_RIGHT_SUPER;
    case SAPP_KEYCODE_MENU:         return FRK_MENU;
    default: break;
    }
    if (k >= SAPP_KEYCODE_F1 && k <= SAPP_KEYCODE_F12)
        return (fr_key)(FRK_F1 + (k - SAPP_KEYCODE_F1));
    return FRK_NONE;
}

/* BIOS (scan, ascii) for a key. Extended keys have no character, so their
 * ascii byte is 0xE0 - the same convention main_win32.c uses. For the rest
 * push_fr_key() seeds the char from fr_key_ascii() and a following CHAR event
 * patches it in (X11's XLookupString only yields a Unicode codepoint for
 * keysyms it maps, so Return/Tab/Backspace never get a CHAR event). */
static uint8_t scan_ascii(fr_key key, uint8_t *scan)
{
    *scan = fr_key_scan(key);
    return fr_key_extended(key) ? 0xE0 : 0;
}

static void push_fr_key(fr_key key, int down, unsigned mods)
{
    uint8_t scan, ascii;

    if (key == FRK_NONE)
        return;
    if (down) {
        if (g_down[key]) return;
        g_down[key] = 1;
        scan = fr_key_scan_mod(key, mods);
        g_down_scan[key] = scan;
    } else {
        if (!g_down[key]) return;
        g_down[key] = 0;
        scan = g_down_scan[key];
        if (!scan)
            scan = fr_key_scan(key);
    }
    if (!scan)
        return;
    /* Seed the BIOS ascii byte here, exactly like input_post_key().  X11 only
     * sends a CHAR event for keysyms _sapp_x11_keysym_to_unicode() maps, and
     * Return/Tab/Backspace are not in that table - so without this, Enter
     * reaches the game as AL=0 and only Space (which does get a CHAR event)
     * confirms a menu selection.  A real CHAR event still overwrites it. */
    ascii = fr_key_extended(key) ? 0xE0 : fr_key_ascii(key);
    fd2_engine_key((uint8_t)(down ? scan : (scan | 0x80)), ascii);
    g_char_pending = down && !fr_key_extended(key);
}

/* fr_key -> BIOS directly: no window is needed, so --autokey/--keyplay work
 * even before sokol_app has delivered its first event. */
void input_post_key(fr_key key)
{
    uint8_t scan, ascii;

    if (key == FRK_NONE)
        return;
    ascii = scan_ascii(key, &scan);
    if (!scan)
        return;
    if (!fr_key_extended(key))
        ascii = fr_key_ascii(key);
    printf("host: autokey %s (scan %02X ascii %02X)\n",
           fr_key_name(key), scan, ascii);
    fd2_engine_key(scan, ascii);
    fd2_engine_key((uint8_t)(scan | 0x80), ascii);
}

#endif /* _WIN32 */

/* ------------------------------------------------------------- callbacks */

static void init_cb(void)
{
    if (render_init(&g_rd) != 0) {
        printf("host: render init failed (sokol)\n");
        sapp_quit();
        return;
    }
    printf("host: render backend = %s\n", render_name());
    fd2_tlog("render init done (D3D11 device + swapchain ready)");
    fd2_engine_start();
#if defined(_WIN32)
    {
        HWND hw = (HWND)sapp_win32_get_hwnd();
        RECT c, w; POINT pt;
        pt.x = 0; pt.y = 0;
        GetClientRect(hw, &c); GetWindowRect(hw, &w); ClientToScreen(hw, &pt);
        printf("sokol: window=%ldx%ld client=%ldx%ld clientOrigin=(%ld,%ld) sapp=%dx%d\n",
               w.right - w.left, w.bottom - w.top,
               c.right - c.left, c.bottom - c.top, pt.x, pt.y,
               sapp_width(), sapp_height());
    }
#endif
}

static void frame_cb(void)
{
    static int first_frame;

    fd2_engine_frame();
    if (!first_frame) {
        first_frame = 1;
        fd2_tlog("first frame presented to the window");
    }
    if (fd2_engine_game_over()) {
        sapp_quit();          /* game thread returned: close, do not freeze */
        return;
    }
    if (g_autoquit_ms && plat_now_ms() - g_t0_ms >= g_autoquit_ms) {
        printf("sokol: auto-quit after %llu ms\n", (unsigned long long)g_autoquit_ms);
        sapp_quit();
    }
}

/* --- close policy -------------------------------------------------------
 * The title-bar close button and the keyboard quit shortcut (Windows/Linux
 * Alt+F4, macOS Cmd+W) both arrive as a single QUIT_REQUESTED event
 * (WM_CLOSE / WM_DELETE_WINDOW / windowShouldClose), so the event itself
 * cannot tell them apart.  The *modifier state* can: the shortcut is
 * delivered while Alt (Cmd on macOS) is still held, the button is not.
 * Track that key and only cancel the request while it is down.
 *
 *   --close=button (default) title-bar button quits; Alt+F4/Cmd+W does not
 *   --close=both             both quit
 *   --close=none             neither (Ctrl+Esc still exits)
 */
enum { QUIT_BUTTON = 0, QUIT_BOTH, QUIT_NONE };
static int g_quit_policy = QUIT_BUTTON;
static int g_quit_mod_down;          /* Alt (X11) / Cmd (macOS) held */

/* Is the keyboard-quit modifier physically down right now?  On Windows we ask
 * the OS directly (sokol's scancode mapping does not always surface a
 * synthetic/Alt press, and GetAsyncKeyState is what the WM_CLOSE handler can
 * trust); on X11/macOS use the state sokol delivered via key events. */
static int quit_shortcut_held(void)
{
#if defined(_WIN32)
    return (GetAsyncKeyState(VK_MENU)  & 0x8000) != 0 ||
           (GetAsyncKeyState(VK_LMENU) & 0x8000) != 0 ||
           (GetAsyncKeyState(VK_RMENU) & 0x8000) != 0;
#else
    return g_quit_mod_down;
#endif
}

static void handle_quit_request(void)
{
    int block;

    switch (g_quit_policy) {
    case QUIT_BOTH: block = 0; break;
    case QUIT_NONE: block = 1; break;
    default:        block = quit_shortcut_held(); break;
    }
    if (block) {
        printf("sokol: keyboard quit shortcut blocked "
               "(close button still works; Ctrl+Esc exits)\n");
        sapp_cancel_quit();
    } else {
        printf("sokol: window close accepted (quitting)\n");
        sapp_quit();
    }
}

static void event_cb(const sapp_event *e)
{
    /* Close policy: remember whether the keyboard-quit modifier is held, and
     * handle the quit request before the per-OS key translation. */
    if (e->type == SAPP_EVENTTYPE_KEY_DOWN || e->type == SAPP_EVENTTYPE_KEY_UP) {
        int down = (e->type == SAPP_EVENTTYPE_KEY_DOWN);

#if defined(__APPLE__)
        if (e->key_code == SAPP_KEYCODE_LEFT_SUPER ||
            e->key_code == SAPP_KEYCODE_RIGHT_SUPER)
            g_quit_mod_down = down;
#else
        if (e->key_code == SAPP_KEYCODE_LEFT_ALT ||
            e->key_code == SAPP_KEYCODE_RIGHT_ALT)
            g_quit_mod_down = down;
#endif
    }
    if (e->type == SAPP_EVENTTYPE_QUIT_REQUESTED) {
        handle_quit_request();
        return;
    }

    /* Window focus: mute and/or pause per FD2_UNFOCUS (see fd2_kernel.c).
     * Handled for both entry paths before the key translation below. */
    switch (e->type) {
    case SAPP_EVENTTYPE_FOCUSED:
    case SAPP_EVENTTYPE_RESTORED:
        printf("sokol: window focused\n");
        fd2_set_focus(1);
        return;
    case SAPP_EVENTTYPE_UNFOCUSED:
    case SAPP_EVENTTYPE_ICONIFIED:
        printf("sokol: window unfocused\n");
        g_quit_mod_down = 0;         /* no key-up while unfocused */
        fd2_set_focus(0);
        return;
    default:
        break;
    }
#if defined(_WIN32)
    switch (e->type) {
    case SAPP_EVENTTYPE_KEY_DOWN: {
        int vk;
        if (e->key_repeat)
            break;
        vk = sapp_to_vk((int)e->key_code);
        /* same escape hatch as main_win32.c: Ctrl+Esc quits */
        if (vk == VK_ESCAPE && (e->modifiers & SAPP_MODIFIER_CTRL)) {
            sapp_quit();
            break;
        }
        if (0)
            break;                 /* autokey injects via input_post_key */
        push_vk(vk, 1);
        break;
    }
    case SAPP_EVENTTYPE_KEY_UP:
        if (0)
            break;
        push_vk(sapp_to_vk((int)e->key_code), 0);
        break;
    default:
        break;
    }
#else
    switch (e->type) {
    case SAPP_EVENTTYPE_KEY_DOWN: {
        fr_key key;
        if (e->key_repeat)
            break;
        key = sapp_to_fr_key((int)e->key_code);
        if (key == FRK_ESCAPE && (e->modifiers & SAPP_MODIFIER_CTRL)) {
            sapp_quit();
            break;
        }
        if (key == FRK_NONE || 0)
            break;
        push_fr_key(key, 1,
                    ((e->modifiers & SAPP_MODIFIER_CTRL)  ? FR_MOD_CTRL  : 0u) |
                    ((e->modifiers & SAPP_MODIFIER_ALT)   ? FR_MOD_ALT   : 0u) |
                    ((e->modifiers & SAPP_MODIFIER_SHIFT) ? FR_MOD_SHIFT : 0u));
        break;
    }
    case SAPP_EVENTTYPE_KEY_UP:
        if (0)
            break;
        g_char_pending = 0;        /* a break code never carries a char */
        push_fr_key(sapp_to_fr_key((int)e->key_code), 0, 0u);
        break;
    case SAPP_EVENTTYPE_CHAR:
        /* sokol's X11 backend runs XLookupString for us: this event carries
         * what ToAscii would have produced on Windows. */
        if (g_char_pending) {
            fd2_engine_key_set_ascii(e->char_code < 0x100
                                        ? (uint8_t)e->char_code : 0);
            g_char_pending = 0;
        }
        break;
    default:
        break;
    }
#endif
}

static void cleanup_cb(void)
{
    render_shutdown();
    fd2_engine_shutdown();
}

/* ------------------------------------------------------------------ main */

int main(int argc, char **argv)
{
    sapp_desc desc;

    memset(&desc, 0, sizeof desc);
    if (fd2_engine_init(argc, argv) != 0)
        return 1;

    {
        const char *aq = host_arg("AUTOQUIT_MS");
        const char *cl = host_arg("CLOSE");

        if (aq && *aq)
            g_autoquit_ms = (uint64_t)atoll(aq);
        if (cl) {
            if (strstr(cl, "both"))
                g_quit_policy = QUIT_BOTH;
            else if (strstr(cl, "none"))
                g_quit_policy = QUIT_NONE;
            else
                g_quit_policy = QUIT_BUTTON;
        }
        printf("fd2: close policy: %s\n",
               g_quit_policy == QUIT_BOTH ? "both" :
               g_quit_policy == QUIT_NONE ? "none" : "button");
        g_t0_ms = plat_now_ms();
    }

    fd2_tlog("main: entering sapp_run (window + D3D11 device are created inside)");

    g_rd.native_window = NULL;
    g_rd.logical_w = 320;
    g_rd.logical_h = 200;
    g_rd.scale = 3;

    desc.init_cb      = init_cb;
    desc.frame_cb     = frame_cb;
    desc.cleanup_cb   = cleanup_cb;
    desc.event_cb     = event_cb;
    desc.width        = g_rd.logical_w * g_rd.scale;
    desc.height       = g_rd.logical_h * g_rd.scale;
    desc.window_title = "FlameDragon2 - port3 native (sokol)";
    desc.swap_interval = 1;              /* vsync: replaces the 20 ms timer */
    desc.logger.func  = slog_func;

    sapp_run(&desc);                     /* returns when the app quits */
    return 0;
}
