/* main_sokol.c - sokol_app entry layer for the port3 native engine.
 *
 * Same shape port/main_sokol.c uses (window on the main thread, game on its
 * own): init_cb brings up the render backend and starts the engine, frame_cb
 * presents, event_cb translates keys to BIOS (scan, ascii) and pushes them
 * into the BDA ring.  The sapp_keycode -> fr_key table is port/'s, unchanged.
 */
#define SOKOL_NO_ENTRY
#include <stdio.h>
#include <string.h>
#include "fd2_kernel.h"
#include "render.h"
#include "keys.h"
#include "sokol_app.h"
#include "sokol_log.h"

static render_desc g_rd;

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


static void init_cb(void)
{
    if (render_init(&g_rd) != 0) {
        printf("fd2: render init failed (sokol)\n");
        sapp_quit();
        return;
    }
    printf("fd2: render backend = %s\n", render_name());
    fd2_engine_start();
}

static void frame_cb(void)
{
    fd2_engine_frame();
}

static void push_key(fr_key key, int down)
{
    uint8_t scan, ascii;

    if (key == FRK_NONE)
        return;
    scan = fr_key_scan(key);
    if (!scan)
        return;
    ascii = fr_key_extended(key) ? 0xE0 : fr_key_ascii(key);
    fd2_engine_key((uint8_t)(down ? scan : (scan | 0x80)), ascii);
}

static void event_cb(const sapp_event *e)
{
    switch (e->type) {
    case SAPP_EVENTTYPE_KEY_DOWN:
        if (e->key_repeat)
            break;
        if (sapp_to_fr_key((int)e->key_code) == FRK_ESCAPE &&
            (e->modifiers & SAPP_MODIFIER_CTRL)) {
            sapp_quit();
            break;
        }
        push_key(sapp_to_fr_key((int)e->key_code), 1);
        break;
    case SAPP_EVENTTYPE_KEY_UP:
        push_key(sapp_to_fr_key((int)e->key_code), 0);
        break;
    case SAPP_EVENTTYPE_QUIT_REQUESTED:
        sapp_quit();
        break;
    default:
        break;
    }
}

static void cleanup_cb(void)
{
    fd2_engine_shutdown();
    render_shutdown();
}

int main(int argc, char **argv)
{
    sapp_desc desc;

    memset(&desc, 0, sizeof desc);
    if (fd2_engine_init(argc, argv) != 0)
        return 1;

    g_rd.native_window = NULL;
    g_rd.logical_w = 320;
    g_rd.logical_h = 200;
    g_rd.scale = 3;

    desc.init_cb       = init_cb;
    desc.frame_cb      = frame_cb;
    desc.cleanup_cb    = cleanup_cb;
    desc.event_cb      = event_cb;
    desc.width         = g_rd.logical_w * g_rd.scale;
    desc.height        = g_rd.logical_h * g_rd.scale;
    desc.window_title  = "FlameDragon2 - port3 native (sokol)";
    desc.swap_interval = 1;
    desc.logger.func   = slog_func;

    sapp_run(&desc);
    return 0;
}
