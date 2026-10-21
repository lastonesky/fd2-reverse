/* unit_anim_check.c - regression test for map_units_animate's cell offsets.
 *
 * The death animation accepts records one cell outside the view (x=ox-1 or
 * y=oy-1).  Its destination was computed with a uint32_t cast, so a negative
 * row difference became +4 GiB and rle2_blit_trans wrote out of bounds.
 *
 * The test links the real map_units.c and replaces rle2_blit_trans with a
 * pointer-range validator.  No graphics are drawn.
 */
#include "fd2_game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAP_BYTES 0x25680
#define ATLAS_BYTES 0x1000
#define REC_SIZE 80

extern void fd2_data_init(void);

static uint8_t g_map[MAP_BYTES];
static uint8_t g_atlas[ATLAS_BYTES];
static uint8_t g_records[2 * REC_SIZE];
static int     g_failures;
static int     g_blits;

/* ---- stubs for map_units.c's dependencies ---------------------------- */

void rle2_blit_trans(void *dst, const void *src, int stride)
{
    uintptr_t d = (uintptr_t)dst;
    uintptr_t lo = (uintptr_t)g_map;
    uintptr_t hi = lo + MAP_BYTES;

    (void)src;
    (void)stride;
    ++g_blits;
    if (d < lo || d >= hi) {
        printf("FAIL  rle2_blit_trans dst=%p outside map buffer [%p,%p)\n",
               dst, (void *)lo, (void *)hi);
        ++g_failures;
    }
}

void map_render_view(uint8_t *dst, int pitch, int w, int h, int ox, int oy)
{ (void)dst; (void)pitch; (void)w; (void)h; (void)ox; (void)oy; }
void gfx_copy_rows(void *dst, int pitch, const void *src, int spitch,
                   int w, int h)
{ (void)dst; (void)pitch; (void)src; (void)spitch; (void)w; (void)h; }
void dlg_portrait_draw(int idx) { (void)idx; }
void dlg_portraits_refresh(void) { }
void map_refresh_records(void) { }
int svc_wait_ticks(int n) { (void)n; return 0; }
int svc_play_sfx(const void *bank, int index, int loops)
{ (void)bank; (void)index; (void)loops; return 0; }
int j___delay(int ms) { (void)ms; return 0; }
void map_view_update(int flag) { (void)flag; }
void map_slide_view(int x, int y) { (void)x; (void)y; }
void dlg_portrait_glide(int idx) { (void)idx; }
void *res_load(const char *name, void *old_buffer, int index)
{ (void)name; (void)index; return old_buffer; }
void rec_sub_table5(int actor, int index) { (void)actor; (void)index; }
void reveal_band(int cx, int cy, int step, int y0, int rows, const void *spr)
{ (void)cx; (void)cy; (void)step; (void)y0; (void)rows; (void)spr; }

/* ---- test ------------------------------------------------------------ */

static void setup(int rec_x, int rec_y, int ox, int oy)
{
    int i;

    fd2_data_init();
    memset(g_map, 0, sizeof g_map);
    memset(g_atlas, 0, sizeof g_atlas);
    memset(g_records, 0, sizeof g_records);

    /* Every atlas sub-image 68..79 points at the same dummy block. */
    for (i = 68; i <= 79; ++i)
        *(uint32_t *)(g_atlas + 6 + 4 * i) = 0x800;

    dword_53A49 = g_map;
    dword_53A81 = g_atlas;
    dword_53A45 = g_records;
    dword_53BEB = 1;
    dword_51A87 = 13;
    dword_51A8B = 8;
    qword_53AA9 = (uint64_t)(uint32_t)ox | ((uint64_t)(uint32_t)oy << 32);

    g_records[0] = (uint8_t)rec_x;
    g_records[1] = (uint8_t)rec_y;
    g_records[5] = 0;
    *(uint16_t *)(g_records + 64) = 0;   /* dead -> collected */

    g_blits = 0;
}

static void check_case(const char *what, int rec_x, int rec_y, int ox, int oy)
{
    int before = g_failures;

    setup(rec_x, rec_y, ox, oy);
    map_units_animate();
    if (g_failures != before) {
        printf("FAIL  %s (x=%d y=%d origin=%d,%d)\n", what, rec_x, rec_y, ox, oy);
    } else if (g_blits != 12) {
        printf("FAIL  %s: expected 12 blits, got %d\n", what, g_blits);
        ++g_failures;
    } else {
        printf("ok    %-42s x=%d y=%d origin=%d,%d\n",
               what, rec_x, rec_y, ox, oy);
    }
}

int main(void)
{
    printf("unit_anim_check: map_units_animate edge-cell offsets\n");

    check_case("dead unit one row above the view",
               9, 4, 9, 5);       /* y = oy-1: used to become +4 GiB */
    check_case("dead unit one column left of the view",
               8, 5, 9, 5);       /* x = ox-1 */
    check_case("dead unit inside the view",
               10, 6, 9, 5);

    if (g_failures != 0) {
        printf("unit_anim_check: %d FAILURE(S)\n", g_failures);
        return 1;
    }
    printf("unit_anim_check: all cases passed\n");
    return 0;
}
