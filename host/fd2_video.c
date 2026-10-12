/* fd2_video.c - fd2_vga + DAC palette -> BGRA -> render backend.
 *
 * The conversion is port/host.c's host_frame() lifted verbatim: the palette
 * table already holds 8-bit-per-channel values (outp stretches the 6-bit DAC
 * register), and the BGRA byte order is what BI_RGB DIBs use (byte0=blue,
 * byte1=green, byte2=red) - getting it backwards swaps red and blue.
 *
 * The frame buffer itself is fd2_vga (the C3 de-guest seam), not a fixed
 * 0xA0000 mapping, so this works in a plain 64-bit process.
 */
#include "fd2_video.h"

#include <stdio.h>
#include <string.h>
#include "platform.h"      /* plat_mutex: front-buffer handover */
#include "render.h"
#include "../src/game/vga.h"

uint8_t      fd2_palette[256 * 3];
volatile int fd2_palette_dirty = 1;

static uint32_t g_bgra[320 * 200];

/* Stage 2: front frame published by the game thread.  Only the game thread
 * touches fd2_vga / fd2_palette; the presenter reads this copy, and the two
 * sides hand it over under g_fb_lock. */
static uint8_t    g_front_vga[VGA_BYTES];
static uint8_t    g_front_palette[256 * 3];
static plat_mutex g_fb_lock;
static int        g_fb_ready;

volatile unsigned fd2_video_publish_count;

void fd2_video_default_palette(void)
{
    int i;
    for (i = 0; i < 256; i++) {
        fd2_palette[i * 3 + 0] = (uint8_t)(((i >> 5) & 7) * 255 / 7);
        fd2_palette[i * 3 + 1] = (uint8_t)(((i >> 2) & 7) * 255 / 7);
        fd2_palette[i * 3 + 2] = (uint8_t)((i & 3) * 255 / 3);
    }
    fd2_palette_dirty = 1;
}

void fd2_video_init(void)
{
    plat_mutex_init(&g_fb_lock);
    g_fb_ready = 1;
    fd2_video_publish();          /* initial (blank) frame */
}

void fd2_video_publish(void)
{
    if (!g_fb_ready)
        return;
    plat_mutex_lock(&g_fb_lock);
    memcpy(g_front_vga, fd2_vga, sizeof g_front_vga);
    memcpy(g_front_palette, fd2_palette, sizeof g_front_palette);
    plat_mutex_unlock(&g_fb_lock);
    fd2_video_publish_count++;
}

void fd2_video_frame(void)
{
    int i;

    plat_mutex_lock(&g_fb_lock);
    for (i = 0; i < 320 * 200; i++) {
        uint8_t idx = g_front_vga[i];
        const uint8_t *c = &g_front_palette[idx * 3];
        g_bgra[i] = ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | c[2];
    }
    plat_mutex_unlock(&g_fb_lock);
    render_present(g_bgra, 320, 200);
}

static void put_le16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

void fd2_video_screenshot(const char *path)
{
    uint8_t  hdr[54];
    uint32_t pix = 320u * 200u * 4u;
    FILE *f = fopen(path, "wb");

    if (!f) {
        printf("fd2: cannot write frame dump %s\n", path);
        return;
    }
    memset(hdr, 0, sizeof hdr);
    hdr[0] = 'B';
    hdr[1] = 'M';
    put_le32(hdr + 2, 54u + pix);
    put_le32(hdr + 10, 54u);
    put_le32(hdr + 14, 40u);
    put_le32(hdr + 18, 320u);
    put_le32(hdr + 22, (uint32_t)-200);   /* top-down, same order as g_bgra */
    put_le16(hdr + 26, 1u);
    put_le16(hdr + 28, 32u);
    put_le32(hdr + 34, pix);
    fwrite(hdr, 1, sizeof hdr, f);
    fwrite(g_bgra, 1, pix, f);
    fclose(f);
    printf("fd2: frame dumped to %s\n", path);
}
