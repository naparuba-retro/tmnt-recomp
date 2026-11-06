/* Large view, see large_view.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "large_view.h"
#include "nmi.h"
#include "ppu.h"
#include "stage_map.h"

static int enabled;
static PpuRowAbove row_above, rows_top[3];
static int rows_top_ty[3];

/* What the game's row drawing writes to the video memory of the copy: which bytes, and where the first tile goes. */
static uint8_t written[0x800];
static int slot_nt, slot_row;

static void on_write(uint16_t addr, int vram_index, uint8_t value) {
    (void)value;
    if (vram_index < 0 || vram_index >= 0x800) { /* a palette write: the palettes follow the name tables in memory */
        return;
    }
    written[vram_index] = 1;
    if ((addr & 0x3FF) < 0x3C0 && slot_row < 0) {
        slot_nt = vram_index >> 10;
        slot_row = (vram_index & 0x3FF) >> 5;
    }
}

void large_view_set_enabled(int on) {
    enabled = on;
}

int large_view_enabled(void) {
    return enabled;
}

/* Draws one row of the map (16 blocks of 16x16) at the world position (x, y), as B7 $DD25 does when the camera goes up, and sends it to the video memory
 * of the (copy of the) game. */
static void draw_row(Game *g, unsigned y, unsigned x) {
    M(TmpA) = (uint8_t)y;
    M(TmpB) = (uint8_t)(y >> 8);
    M(Tmp8) = (uint8_t)x;
    M(Tmp9) = (uint8_t)(x >> 8);
    M(PpuBuffer) = 0;
    M(PpuBufferIndex) = 0;
    draw_map_row(g);
    flush_ppu_buffer(g);
}

/* The physical name table shown at the top of the picture, and the one above it. */
static int name_table_top(const Game *g) {
    int nn = (g->ppu.t >> 10) & 3;
    return g->ppu.mirror_v ? (nn & 1) : (nn >> 1);
}

static int name_table_above(const Game *g) {
    int nn = (g->ppu.t >> 10) & 3;
    return g->ppu.mirror_v ? ((nn ^ 2) & 1) : ((nn ^ 2) >> 1);
}

/* The world position of the first row of the picture's page. The video memory holds the map in pages of 240 lines (30 rows), one name table per page,
 * alternately (B7 $DF19, split_y). The page shown at the top is the one, among those that have the name table shown there, that is the nearest to the
 * camera. Page 0 is the first of the map. */
static unsigned page_top_y(const Game *source, unsigned camera_y, int ring_row, unsigned *page_number) {
    int nt = name_table_top(source);
    unsigned best = 0, best_distance = 0xFFFFFF;
    for (unsigned page = (unsigned)nt; page < 0x80; page += 2) {
        unsigned top = page * 240 + (unsigned)(ring_row > 0 ? ring_row * 8 : 0), distance = top > camera_y ? top - camera_y : camera_y - top;
        if (distance < best_distance) {
            best_distance = distance;
            best = page;
        }
    }
    *page_number = best_distance > 16 ? ~0u : best; /* the camera and the scroll registers disagree (a scene change): no rebuild */
    return best * 240;
}

/* Rebuilds into `out` a row of the level's map, with the tiles and the palettes of the level data (the game's own row drawing, run on a copy of the
 * state). `ring_row` is its row in the video memory, 0 to 29 (in the page shown at the top), or -1 for the row just above the picture, which is the last
 * row of the previous page; this one does not exist (returns 0) when the picture is in the first page of the map. `slot_ok`, if not NULL, tells whether
 * the row went to the slot of the video memory that was expected. */
static int build_row(const Game *source, PpuRowAbove *out, int *slot_ok, int ring_row) {
    memset(out, 0, sizeof *out);
    Game *g = malloc(sizeof *g);
    if (!g) {
        return 0;
    }
    memcpy(g, source, sizeof *g);
    g->trace = NULL;
    unsigned camera_y = M(CameraYLo) | M(CameraYHi) << 8, camera_x = M(CameraXLo) | M(CameraXHi) << 8, page;
    unsigned y = page_top_y(source, camera_y, ring_row, &page), x0 = (camera_x + 8) & 0xFFF0; /* the game's own start (scroll_camera_y, B7 $DC8B) */
    if (page == ~0u) {
        free(g);
        return 0;
    }
    if (ring_row < 0 && page == 0) { /* the picture starts at the top of the map: nothing is above */
        free(g);
        return 0;
    }
    y = y + (unsigned)(ring_row * 8); /* for the row above: the last row of the previous page */
    memset(written, 0, sizeof written);
    slot_row = -1;
    ppu_set_write_hook(on_write);
    /* One row of 16 blocks, as the game draws it (the video memory has 32 columns that wrap: a second pass at another column would write over the first
     * ones). The routine keeps state from one call to the next (the game calls it row after row), so it is called once for nothing, and the second
     * call is the one that counts. */
    draw_row(g, y, x0);
    memset(written, 0, sizeof written);
    slot_row = -1;
    draw_row(g, y, x0);
    int first_slot_row = slot_row, first_slot_nt = slot_nt;
    if (y & 8) { /* the lower row of a pair: the attribute byte, shared by both, is sent with the upper one */
        draw_row(g, y - 8, x0);
    }
    ppu_set_write_hook(NULL);
    int mirror_v = g->ppu.mirror_v, found = 0;
    if (first_slot_row >= 0) {
        for (int nt = 0; nt < 2; nt++) {
            for (int col = 0; col < 32; col++) {
                int i = nt * 0x400 + first_slot_row * 32 + col;
                if (!written[i]) {
                    continue;
                }
                int idx = (mirror_v ? nt << 5 : 0) | col;
                uint8_t attr = g->ppu.vram[nt * 0x400 + 0x3C0 + (first_slot_row >> 2) * 8 + (col >> 2)];
                out->have[idx] = 1;
                out->tile[idx] = g->ppu.vram[i];
                out->pal[idx] = (attr >> (((first_slot_row & 2) << 1) | (col & 2))) & 3;
                found++;
            }
        }
    }
    if (slot_ok) {
        int expected_row = ring_row < 0 ? 29 : ring_row, expected_nt = ring_row < 0 ? name_table_above(source) : name_table_top(source);
        *slot_ok = first_slot_row == expected_row && (mirror_v || first_slot_nt == expected_nt);
    }
    free(g);
    return found > 0;
}

/* The row of the video memory that the PPU shows first: its line (0-29, or 30 and 31 for the attribute table) and how many of its lines are above the top
 * of the picture. */
static int first_row(const Game *g) {
    return (g->ppu.t >> 5) & 31;
}

static int first_row_hidden_lines(const Game *g) {
    return (g->ppu.t >> 12) & 7;
}

static uint8_t vram_tile(const Game *g, int nt, int row, int col);

void large_view_prepare(Game *g) {
    if (!enabled) {
        ppu_set_large(0, NULL, NULL, NULL, 0, 0);
        return;
    }
    memset(&row_above, 0, sizeof row_above);
    memset(rows_top, 0, sizeof rows_top);
    int count = 0;
    if (first_row(g) >= 30) { /* the lines read the attribute table: the row above, or the backdrop */
        build_row(g, &row_above, NULL, -1);
    } else if (g->ram[GameMode] == 4) { /* in a level only: the map means nothing in the other scenes */
        /* The first rows of the picture (the 16 first lines, which a TV hid in part) are read from the map: the video memory does not always hold them up
         * to date. When the camera goes up, the game scrolls first and draws the new row later; and the palette of the lower row of a pair of rows is sent
         * with the upper one, so it is stale for a while. */
        for (int k = 0; k < 3 && first_row(g) + k < 30; k++) {
            if (build_row(g, &rows_top[count], NULL, first_row(g) + k)) {
                rows_top_ty[count++] = first_row(g) + k;
            }
        }
    }
    ppu_set_large(1, &row_above, rows_top, rows_top_ty, count, 16);
}

/* --- tests --- */
static unsigned frames_checked, frames_with_row, state_changed, slot_wrong, fresh_frames, fresh_equal, fresh_different;
static unsigned frames_top_identical, frames_top_partial, frames_top_all_different;
static unsigned top_slot_wrong, top_missing, top_tiles_equal, top_tiles_different, top_pal_equal, top_pal_different;
static unsigned partial_frames, partial_pal_frames_different, partial_tiles_equal, partial_tiles_different, partial_pal_equal, partial_pal_different;
static PpuRowAbove prev_built;
static int prev_valid, prev_nt;
static unsigned prev_frame;
static uint8_t prev_row[32];

/* The tiles and the palettes that the video memory holds for the row `row` of the name table `nt`, column `col`. */
static uint8_t vram_tile(const Game *g, int nt, int row, int col) {
    return g->ppu.vram[nt * 0x400 + row * 32 + col];
}

static uint8_t vram_palette(const Game *g, int nt, int row, int col) {
    uint8_t attr = g->ppu.vram[nt * 0x400 + 0x3C0 + (row >> 2) * 8 + (col >> 2)];
    return (attr >> (((row & 2) << 1) | (col & 2))) & 3;
}

/* Three things are checked. The state of the game must not change when a row is rebuilt. The row above the picture, rebuilt at the frame N, must be the
 * one the game draws when the camera goes up: the tiles it writes at the frame N+1 must be the same. And the rebuilt rows must be those of the video
 * memory when the row is on the screen: for the first row of the picture (always on the screen), and for the row that is shown by its lower lines at the
 * top, where the palettes of the video memory are the ones that are sometimes not up to date. */
void large_view_check(Game *g) {
    int nt_above = name_table_above(g), mirror_v = g->ppu.mirror_v;
    const uint8_t *row29 = &g->ppu.vram[nt_above * 0x400 + 29 * 32];
    if (prev_valid && g->frame == prev_frame + 1 && prev_nt == nt_above) {
        int any = 0;
        for (int col = 0; col < 32; col++) {
            int idx = (mirror_v ? nt_above << 5 : 0) | col;
            if (row29[col] == prev_row[col] || !prev_built.have[idx]) {
                continue;
            }
            any = 1;
            if (prev_built.tile[idx] == row29[col]) {
                fresh_equal++;
            } else {
                fresh_different++;
            }
        }
        fresh_frames += any;
    }
    prev_valid = 0;
    Game *before = malloc(sizeof *before);
    if (!before) {
        return;
    }
    memcpy(before, g, sizeof *before);
    if (g->ram[GameMode] == 4 && first_row(g) < 30 && first_row_hidden_lines(g) > 0) { /* the row shown by its lower lines at the top */
        PpuRowAbove partial;
        int row = first_row(g), nt_top = name_table_top(g);
        if (build_row(g, &partial, NULL, row)) {
            partial_frames++;
            int pal_diff = 0;
            for (int col = 0; col < 32; col++) {
                int idx = (mirror_v ? nt_top << 5 : 0) | col;
                if (!partial.have[idx]) {
                    continue;
                }
                partial_tiles_equal += partial.tile[idx] == vram_tile(g, nt_top, row, col);
                partial_tiles_different += partial.tile[idx] != vram_tile(g, nt_top, row, col);
                partial_pal_equal += partial.pal[idx] == vram_palette(g, nt_top, row, col);
                partial_pal_different += partial.pal[idx] != vram_palette(g, nt_top, row, col);
                pal_diff |= partial.pal[idx] != vram_palette(g, nt_top, row, col);
            }
            partial_pal_frames_different += pal_diff;
        }
    }
    if (first_row(g) < 30) {
        state_changed += memcmp(before, g, sizeof *g) != 0;
        free(before);
        return;
    }
    PpuRowAbove built;
    int slot_ok = 0;
    int has_row = build_row(g, &built, &slot_ok, -1);
    frames_checked++;
    state_changed += memcmp(before, g, sizeof *g) != 0;
    free(before);
    if (!has_row) {
        return;
    }
    frames_with_row++;
    slot_wrong += !slot_ok;
    /* The same rebuild, for the first row of the picture, which is on the screen: the video memory holds it, tiles and palettes. */
    PpuRowAbove top;
    int top_slot_ok = 0;
    if (build_row(g, &top, &top_slot_ok, 0)) {
        int nt_top = name_table_top(g);
        top_slot_wrong += !top_slot_ok;
        unsigned eq_before = top_tiles_equal, diff_before = top_tiles_different;
        for (int col = 0; col < 32; col++) {
            int idx = (mirror_v ? nt_top << 5 : 0) | col;
            if (!top.have[idx]) {
                top_missing++;
                continue;
            }
            top_tiles_equal += top.tile[idx] == vram_tile(g, nt_top, 0, col);
            top_tiles_different += top.tile[idx] != vram_tile(g, nt_top, 0, col);
            top_pal_equal += top.pal[idx] == vram_palette(g, nt_top, 0, col);
            top_pal_different += top.pal[idx] != vram_palette(g, nt_top, 0, col);
        }
        frames_top_identical += top_tiles_different == diff_before;
        frames_top_partial += top_tiles_different != diff_before && top_tiles_equal != eq_before;
        frames_top_all_different += top_tiles_different != diff_before && top_tiles_equal == eq_before;
    }
    prev_built = built;
    prev_valid = 1;
    prev_frame = g->frame;
    prev_nt = nt_above;
    memcpy(prev_row, row29, sizeof prev_row);
}

void large_view_report(void) {
    fprintf(stderr,
            "large view: %u frames with the lines of the attribute table, %u with a row above in the map (state changed by a rebuild: %u, row not in the slot "
            "above: %u)\n",
            frames_checked, frames_with_row, state_changed, slot_wrong);
    fprintf(stderr,
            "large view: first row of the picture rebuilt (it is on the screen): tiles %u identical, %u different, %u missing; palettes %u identical, %u "
            "different; row in a wrong slot: %u\n",
            top_tiles_equal, top_tiles_different, top_missing, top_pal_equal, top_pal_different, top_slot_wrong);
    fprintf(stderr, "large view: per frame, first row: %u identical, %u partly different, %u entirely different\n", frames_top_identical, frames_top_partial,
            frames_top_all_different);
    fprintf(stderr, "large view: the game wrote the row above in %u frames: %u tiles identical to the rebuilt ones, %u different\n", fresh_frames, fresh_equal,
            fresh_different);
    fprintf(stderr,
            "large view: row shown by its lower lines at the top, %u frames: tiles %u identical, %u different; palettes %u identical, %u different (in %u "
            "frames the video memory has a palette that differs from the map's)\n",
            partial_frames, partial_tiles_equal, partial_tiles_different, partial_pal_equal, partial_pal_different, partial_pal_frames_different);
}
