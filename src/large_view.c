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
static PpuRowAbove row_above, screen_rows[32];

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
static unsigned page_top_y(int nt, unsigned camera_y, int ring_row, unsigned *page_number) {
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
    unsigned y = page_top_y(name_table_top(source), camera_y, ring_row, &page),
             x0 = (camera_x + 8) & 0xFFF0; /* the game's own start (scroll_camera_y, B7 $DC8B) */
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
                uint8_t attr = g->ram[(nt ? AttrBufferB : AttrBufferA) + (first_slot_row >> 2) * 8 + (col >> 2)];
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

/* One pass of the game's row drawing over the rows of the picture, on a copy of the state: what each row of the map gives for the 32 columns that start at
 * the world column `x0 / 8`, by position in these 32 (tile, palette, and the name table and column where the game puts it). */
typedef struct {
    int valid, slot_row;
    uint8_t nt[32], col[32], tile[32], pal[32];
} PassRow;

/* The palettes of a row, read in the attribute buffers of the game's memory (the byte of a pair of rows is up to date once the upper row has been drawn;
 * the video memory is not: the game sends the bytes in its own time). */
static void read_palettes(const Game *g, PassRow *r) {
    for (int rel = 0; rel < 32; rel++) {
        uint8_t attr = g->ram[(r->nt[rel] ? AttrBufferB : AttrBufferA) + (r->slot_row >> 2) * 8 + (r->col[rel] >> 2)];
        r->pal[rel] = (attr >> (((r->slot_row & 2) << 1) | (r->col[rel] & 2))) & 3;
    }
}

/* The rows are drawn from the bottom to the top, as the game does when the camera goes up: the routine keeps state from one row to the next (the upper
 * row of a pair completes the attributes that the lower one started). */
static void run_pass(const Game *source, unsigned x0, const unsigned *world_y, const int *valid, int count, PassRow *out) {
    Game *g = malloc(sizeof *g);
    if (!g) {
        return;
    }
    memcpy(g, source, sizeof *g);
    g->trace = NULL;
    /* The game keeps the attribute bytes in two buffers of its memory and adds the bits of each block to them: what a previous use of the video memory
     * left there would stay (a block of water with the palette of a building). A clean buffer gives the palettes of the map. */
    memset(&g->ram[AttrBufferA], 0, 0x40);
    memset(&g->ram[AttrBufferB], 0, 0x40);
    ppu_set_write_hook(on_write);
    int started = 0;
    for (int j = count - 1; j >= 0; j--) {
        if (!valid[j]) {
            continue;
        }
        unsigned y = world_y[j];
        if (!started) { /* one call for nothing first */
            started = 1;
            draw_row(g, y, x0);
        }
        memset(written, 0, sizeof written);
        slot_row = -1;
        draw_row(g, y, x0);
        if (slot_row < 0) {
            continue;
        }
        PassRow *r = &out[j];
        r->valid = 1;
        r->slot_row = slot_row;
        for (int nt = 0; nt < 2; nt++) {
            for (int col = 0; col < 32; col++) {
                if (!written[nt * 0x400 + slot_row * 32 + col]) {
                    continue;
                }
                int rel = (col - (int)(x0 >> 3)) & 31;
                r->nt[rel] = (uint8_t)nt;
                r->col[rel] = (uint8_t)col;
                r->tile[rel] = g->ppu.vram[nt * 0x400 + slot_row * 32 + col];
            }
        }
        if (!(y & 8)) { /* an upper row: it sends the attribute bytes of the pair */
            read_palettes(g, r);
            if (j + 1 < count && valid[j + 1] && out[j + 1].valid && world_y[j + 1] == y + 8) {
                read_palettes(g, &out[j + 1]);
            }
        } else if (j == 0 || !valid[j - 1] || world_y[j - 1] != y - 8) { /* a lower row whose upper row is not drawn here: draw it */
            memset(written, 0, sizeof written);
            draw_row(g, y - 8, x0);
            read_palettes(g, r);
        }
    }
    ppu_set_write_hook(NULL);
    free(g);
}

/* The rows of the picture rebuilt from the map: all the columns for the 3 first rows, and the 4 columns at each edge for all of them. The game draws a row of the map from 8 pixels beyond the left edge, over 256 pixels: the column at the left and the ones at the
 * right are not up to date in the video memory (they show tiles that wrap from the other side), which a TV hid with the mask and with black sprites. Two
 * passes, one at a multiple of 32 pixels and one 32 pixels after, give all the world columns of the picture. */
static int build_screen(const Game *g, PpuRowAbove *rows) {
    memset(rows, 0, 32 * sizeof *rows);
    unsigned camera_y = M(CameraYLo) | M(CameraYHi) << 8, camera_x = M(CameraXLo) | M(CameraXHi) << 8;
    int mirror_v = g->ppu.mirror_v, nn = (g->ppu.t >> 10) & 3, ty = first_row(g);
    /* The world column at the left of the picture follows the scroll registers of the PPU, which are not always where the game's camera is (they differ by a
     * few pixels): the value of the registers, brought to the camera's turn (the name tables wrap every 256 pixels, or 512 when there are two across). */
    int period = mirror_v ? 512 : 256;
    int scroll_x = (mirror_v ? ((g->ppu.t >> 10) & 1) << 8 : 0) | ((g->ppu.t & 31) << 3) | (g->ppu.x & 7);
    int turns = ((int)camera_x - scroll_x + period / 2 + 16 * period) / period - 16;
    unsigned world_left = (unsigned)(scroll_x + turns * period);
    int line_ty[30], valid[30], line_nt[30];
    unsigned world_y[30];
    int count = 0;
    for (int j = 0; j < 30; j++, count++) { /* (the last row of the picture, partial, shares its row with the first one, in the other name table) */
        line_ty[j] = ty;
        line_nt[j] = mirror_v ? 0 : nn >> 1;
        valid[j] = 0;
        if (ty < 30) {
            int nt = mirror_v ? (nn & 1) : (nn >> 1);
            unsigned page, top = page_top_y(nt, camera_y + (unsigned)(8 * j), ty, &page);
            if (page != ~0u) {
                world_y[j] = top + (unsigned)(ty * 8);
                valid[j] = 1;
            }
            if (ty == 29) {
                ty = 0;
                nn ^= 2;
            } else {
                ty++;
            }
        } else {
            ty = ty == 31 ? 0 : 31;
        }
    }
    static PassRow pass_a[30], pass_b[30];
    memset(pass_a, 0, sizeof pass_a);
    memset(pass_b, 0, sizeof pass_b);
    unsigned x_a = world_left & 0xFFE0; /* 32 pixels: an attribute byte covers 4 columns, and the routine builds it from the blocks it draws */
    run_pass(g, x_a, world_y, valid, count, pass_a);
    run_pass(g, x_a + 32, world_y, valid, count, pass_b);
    unsigned first_world_col = world_left >> 3;
    /* Is the screen a picture of the map at all? (the pause screen, the information screens and the other scenes are in mode 4 as well, or are drawn with the
     * same scroll): in a picture of the map the columns in the middle of the screen are exactly the ones that the map gives, which is checked here. Where they
     * are not, nothing is replaced. */
    int same = 0, different = 0;
    for (int j = 0; j < count; j++) {
        if (!valid[j] || line_ty[j] >= 30) {
            continue;
        }
        for (int k = 8; k < 24; k++) {
            int rel = (int)(first_world_col + (unsigned)k) - (int)(x_a >> 3);
            const PassRow *r = &pass_a[j];
            if (rel > 31) {
                rel -= 4;
                r = &pass_b[j];
            }
            if (!r->valid || rel < 0 || rel > 31) {
                continue;
            }
            if (g->ppu.vram[r->nt[rel] * 0x400 + line_ty[j] * 32 + r->col[rel]] == r->tile[rel]) {
                same++;
            } else {
                different++;
            }
        }
    }
    if (same + different == 0 || different * 4 > same + different) {
        return 0;
    }
    int fine = g->ppu.x != 0;
    for (int j = 0; j < count; j++) {
        if (!valid[j] || line_ty[j] >= 30) {
            continue;
        }
        PpuRowAbove *out = &rows[line_ty[j]];
        out->row_nt = (uint8_t)line_nt[j];
        for (int k = 0; k < 33; k++) {
            if (j >= 3 && k > 3 && k < 28) {
                continue;
            }
            if (k == 32 && !fine) {
                continue;
            }
            if (k == 32 && !mirror_v) { /* the same ring column as the first one: kept apart */
                int rel32 = (int)(first_world_col + 32) - (int)(x_a >> 3);
                const PassRow *r32 = &pass_a[j];
                if (rel32 > 31) {
                    rel32 -= 4;
                    r32 = &pass_b[j];
                }
                if (r32->valid && rel32 >= 0 && rel32 <= 31) {
                    out->wrap_have = 1;
                    out->wrap_tile = r32->tile[rel32];
                    out->wrap_pal = r32->pal[rel32];
                }
                continue;
            }
            /* the first column of the map (the 8 pixels that the mask hid) is not part of the playfield: it holds other things (the edge of a building); the
             * next one is shown instead */
            unsigned world_col = first_world_col + (unsigned)k;
            int rel = (int)(world_col == 0 ? 1 : world_col) - (int)(x_a >> 3);
            const PassRow *r = &pass_a[j];
            if (rel > 31) {
                rel -= 4;
                r = &pass_b[j];
            }
            if (!r->valid || rel < 0 || rel > 31) {
                continue;
            }
            int idx = (mirror_v ? r->nt[rel] << 5 : 0) |
                      (world_col & 31); /* (the column of the screen, which is not that of the tile read when the first column of the map is replaced) */
            out->have[idx] = 1;
            out->tile[idx] = r->tile[rel];
            out->pal[idx] = r->pal[rel];
        }
    }
    return 1;
}

void large_view_prepare(Game *g) {
    /* The pause screen only redraws the middle of the screen: its 2 first rows show what the video memory holds there (tiles and sprites of the scene that was
     * left), which a TV hid. The port's video memory is the original's (same name tables as BizHawk on the TAS): in the large view these lines show the
     * backdrop instead; without it, they are left as they are. */
    ppu_set_large_blank_top(enabled && g->ram[GameMode] == 4 && g->ram[PauseFlag] ? 16 : 0);
    if (!enabled) {
        ppu_set_large(0, NULL, NULL);
        ppu_set_large_edges(0);
        return;
    }
    memset(&row_above, 0, sizeof row_above);
    memset(screen_rows, 0, sizeof screen_rows);
    int edges = 0;            /* the screen is a picture of the map: its edges are shown (the mask of the 8 left pixels and the black sprites are not drawn) */
    if (first_row(g) >= 30) { /* the lines read the attribute table: the row above, or the backdrop */
        build_row(g, &row_above, NULL, -1);
    }
    if (g->ram[GameMode] == 4) { /* in a level only: the map means nothing in the other scenes */
        edges = build_screen(g, screen_rows);
    }
    ppu_set_large_edges(edges);
    ppu_set_large(1, &row_above, screen_rows);
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
