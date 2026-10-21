/* move_check.c - regression test for map_move_toward's movement budget.
 *
 * This is the test that would have caught the "villagers walk 10+ cells"
 * bug: map_move_toward must flood-fill and path-find with the record's
 * movement points (record +0x3B), not its cost-table profile (+0x20).
 *
 * The test links the real src/game/path.c + map_move.c and replaces only the
 * game services map_move_toward touches with small deterministic stubs.  The
 * map is empty, every terrain step costs 1, and the target is farther away
 * than the budget, so the only correct result is "walk exactly mp cells".
 */
#include "fd2_game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAP_W     24
#define MAP_H     24
#define MAP_CELLS (MAP_W * MAP_H)
#define REC_SIZE  80

extern void fd2_data_init(void);

static uint8_t g_map[4 + 4 * MAP_CELLS];
static uint8_t g_records[REC_SIZE];
static uint8_t g_cost_table[4 * 512];
static int     g_last_path_len;
static int     g_failures;

/* ---- stubs for the services map_move_toward calls --------------------- */

/* 0x1F183 - our synthetic records are never skipped. */
int rec_skip(int index)
{
    (void)index;
    return 0;
}

/* 0x145CD - no other units are present. */
void unit_mark_nearby(int mode)
{
    (void)mode;
}

/* 0x4DF4C - reset the per-cell path bytes: last byte 0xFF, flag masks. */
unsigned char util_fix_records(void *header)
{
    uint8_t *h = (uint8_t *)header;
    int      count = h[0] * h[2];
    int      i;

    h += 4;
    for (i = 0; i < count; ++i) {
        h[3] = 0xFF;
        h[2] &= 0x1F;
        h[1] &= 0x03;
        h += 4;
    }
    return 0xFF;
}

/* 0x146D1 - no other records whose cells must be excluded. */
void map_exclude_record_cells(int actor, int mode)
{
    (void)actor;
    (void)mode;
}

/* 0x14B16 - collect every path_mark'ed cell (remaining byte != 0xFF). */
int ev5_14B16(uint8_t *out)
{
    int y, x, n = 0;

    for (y = 0; y < MAP_H; ++y) {
        for (x = 0; x < MAP_W; ++x) {
            if (g_map[4 * (y * MAP_W + x) + 7] != 0xFF) {
                out[2 * n]     = (uint8_t)x;
                out[2 * n + 1] = (uint8_t)y;
                ++n;
            }
        }
    }
    return n;
}

/* 0x13488 - apply one direction per step and remember the path length. */
void map_scroll_path(int rec, const uint8_t *dirs, int count)
{
    int i;

    (void)rec;
    g_last_path_len = count;
    for (i = 0; i < count; ++i) {
        switch (dirs[i]) {
        case 0: ++g_records[1]; break;   /* down  */
        case 1: --g_records[0]; break;   /* left  */
        case 2: --g_records[1]; break;   /* up    */
        default: ++g_records[0]; break;  /* right */
        }
    }
}

/* 0x12D7B - no portrait animation in the test. */
void dlg_portrait_glide(int idx)
{
    (void)idx;
}

/* ---- test setup ------------------------------------------------------- */

static void setup(int profile, int mp)
{
    int x, y, row, i;

    fd2_data_init();

    memset(g_map, 0, sizeof g_map);
    g_map[0] = MAP_W;
    g_map[2] = MAP_H;
    for (y = 0; y < MAP_H; ++y) {
        for (x = 0; x < MAP_W; ++x) {
            uint8_t *cell = g_map + 4 + 4 * (y * MAP_W + x);

            cell[0] = 0;        /* terrain id 0 */
            cell[1] = 0;
            cell[2] = 0;
            cell[3] = 0xFF;     /* unvisited */
        }
    }

    /* terrain 0 -> cost-row index 0; every profile row costs 1 per step */
    memset(g_cost_table, 0, sizeof g_cost_table);
    g_cost_table[1] = 0;
    for (row = 0; row < 20; ++row) {
        for (i = 0; i < 20; ++i)
            fd2_data2[0x1646 + 20 * row + i] = 1;
    }

    dword_53A51  = g_map;
    dword_53A69  = g_cost_table;
    dword_53A45  = g_records;
    dword_53BEB  = 1;
    dword_53AC1  = MAP_W;
    dword_53AC5  = MAP_H;

    memset(g_records, 0, sizeof g_records);
    g_records[0] = 5;           /* x */
    g_records[1] = 5;           /* y */
    g_records[32] = (uint8_t)profile;   /* +0x20 cost-table profile */
    g_records[59] = (uint8_t)mp;        /* +0x3B movement points    */

    g_last_path_len = -1;
}

static void check_case(const char *what, int profile, int mp,
                       int tx, int ty, int want_len)
{
    int moved_ret, dx, dy, moved, path;

    setup(profile, mp);
    moved_ret = map_move_toward(tx, ty, 0, 0);

    dx = abs((int)g_records[0] - 5);
    dy = abs((int)g_records[1] - 5);
    moved = dx + dy;
    path  = (want_len > 0) ? g_last_path_len : 0;

    if (moved_ret != (want_len > 0) || moved != want_len || path != want_len) {
        printf("FAIL  %-42s profile=%2d mp=%2d moved=%d path=%d "
               "(want %d)\n",
               what, profile, mp, moved, path, want_len);
        ++g_failures;
    } else {
        printf("ok    %-42s profile=%2d mp=%2d path=%d\n",
               what, profile, mp, path);
    }

    /* Hard invariant: never spend more steps than the record's MP. */
    if (path > mp) {
        printf("FAIL  %-42s path length %d > mp %d\n", what, path, mp);
        ++g_failures;
    }
}

int main(void)
{
    printf("move_check: map_move_toward movement-budget regression\n");

    /* The old bug passed the profile (13/19) instead of MP (3/4). */
    check_case("villager profile must not be the budget", 13, 3, 22, 22, 3);
    check_case("high profile must not be the budget",      19, 4, 22, 22, 4);
    check_case("low profile still uses MP",                 1, 4, 22, 22, 4);
    check_case("zero MP cannot move",                       7, 0, 22, 22, 0);

    /* Same MP, different profiles -> identical distance. */
    check_case("profile 2 / mp 5",                          2, 5, 22, 22, 5);
    check_case("profile 18 / mp 5",                        18, 5, 22, 22, 5);

    if (g_failures != 0) {
        printf("move_check: %d FAILURE(S)\n", g_failures);
        return 1;
    }
    printf("move_check: all cases passed\n");
    return 0;
}
