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
static PpuRowAbove row_above;

/* What the game's row drawing writes to the video memory of the copy: which bytes, and where the first tile goes. */
static uint8_t written[0x800];
static int slot_nt, slot_row;

static void on_write(uint16_t addr, int vram_index, uint8_t value) {
    (void)value;
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

/* The name table that is just above the one shown at the top of the picture, as a physical table (index in the 2 KB of video memory). */
static int name_table_above(const Game *g) {
    int nn = (g->ppu.t >> 10) & 3;
    return g->ppu.mirror_v ? ((nn ^ 2) & 1) : ((nn ^ 2) >> 1);
}

/* The world position of the row that is just above the picture. The video memory holds the map in pages of 240 lines (30 rows), one name table per page,
 * alternately (B7 $DF19, split_y). The picture starts at the top of a name table, so the row above it is the last row of the previous page, which is
 * the page that has the other name table: of the two pages around the camera, the one that goes with the name table shown at the top is the first of the
 * picture. Returns 0 (and `*exists` = 0) when that page is the first of the map: there is nothing above. */
static unsigned row_above_y(const Game *source, unsigned camera_y, int *exists) {
    int nn = (source->ppu.t >> 10) & 3, nt = source->ppu.mirror_v ? (nn & 1) : (nn >> 1);
    unsigned best = 0, best_distance = 0xFFFFFF;
    for (unsigned page = (unsigned)nt; page < 0x80; page += 2) {
        unsigned top = page * 240, distance = top > camera_y ? top - camera_y : camera_y - top;
        if (distance < best_distance) {
            best_distance = distance;
            best = page;
        }
    }
    *exists = best > 0;
    return best * 240; /* the world position of the first row of the picture */
}

/* Rebuilds into `out` the row above the picture; returns 0 when the map has none. `slot_ok`, if not NULL, tells whether the row went to the slot of the
 * video memory that is just above the picture. With `first_row`, rebuilds the first row of the picture instead (tests: that one is on the screen, so the
 * video memory holds it for sure). */
static int build_row(const Game *source, PpuRowAbove *out, int *slot_ok, int first_row) {
    memset(out, 0, sizeof *out);
    Game *g = malloc(sizeof *g);
    if (!g) {
        return 0;
    }
    memcpy(g, source, sizeof *g);
    g->trace = NULL;
    unsigned camera_y = M(CameraYLo) | M(CameraYHi) << 8, camera_x = M(CameraXLo) | M(CameraXHi) << 8;
    int exists;
    unsigned y = row_above_y(source, camera_y, &exists), x0 = camera_x & 0xFFF0;
    if (!exists) { /* the picture starts at the top of the map: nothing is above */
        free(g);
        return 0;
    }
    if (!first_row) {
        y -= 8;
    }
    memset(written, 0, sizeof written);
    slot_row = -1;
    ppu_set_write_hook(on_write);
    /* two rows of 16 blocks cover the 256 pixels of the screen, whatever the camera's position inside a block */
    draw_row(g, y, x0);
    draw_row(g, y, x0 + 16);
    int first_slot_row = slot_row, first_slot_nt = slot_nt;
    if (y & 8) { /* the lower row of a pair: the attribute byte, shared by both, is sent with the upper one */
        draw_row(g, y - 8, x0);
        draw_row(g, y - 8, x0 + 16);
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
        int nn = (source->ppu.t >> 10) & 3, nt_top = mirror_v ? (nn & 1) : (nn >> 1);
        *slot_ok = first_row ? first_slot_row == 0 && (mirror_v || first_slot_nt == nt_top)
                             : first_slot_row == 29 && (mirror_v || first_slot_nt == name_table_above(source));
    }
    free(g);
    return found > 0;
}

static int garbage_lines(const Game *g) {
    return ((g->ppu.t >> 5) & 31) >= 30;
}

void large_view_prepare(Game *g) {
    if (!enabled) {
        ppu_set_large(0, NULL);
        return;
    }
    memset(&row_above, 0, sizeof row_above);
    if (garbage_lines(g)) {
        build_row(g, &row_above, NULL, 0);
    }
    ppu_set_large(1, &row_above);
}

/* --- tests --- */
static unsigned frames_checked, frames_with_row, state_changed, slot_wrong, fresh_frames, fresh_equal, fresh_different;
static unsigned frames_top_identical, frames_top_partial, frames_top_all_different;
static unsigned top_slot_wrong, top_missing, top_tiles_equal, top_tiles_different, top_pal_equal, top_pal_different;
static PpuRowAbove prev_built;
static int prev_valid, prev_nt;
static unsigned prev_frame;
static uint8_t prev_row[32];

/* Two things are checked. The state of the game must not change when the row is rebuilt. And the rebuilt row must be the one the game draws: when the
 * camera goes up the game writes that very row to the video memory, so the tiles it writes at the frame N+1 must be the ones rebuilt at the frame N. (The
 * video memory itself is not a reference before that: it holds the row only if the player came from there, and not always completely.) */
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
    if (!garbage_lines(g)) {
        return;
    }
    Game *before = malloc(sizeof *before);
    PpuRowAbove built;
    int slot_ok = 0;
    if (!before) {
        return;
    }
    memcpy(before, g, sizeof *before);
    int has_row = build_row(g, &built, &slot_ok, 0);
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
    if (build_row(g, &top, &top_slot_ok, 1)) {
        int nn = (g->ppu.t >> 10) & 3, nt_top = mirror_v ? (nn & 1) : (nn >> 1);
        top_slot_wrong += !top_slot_ok;
        unsigned eq_before = top_tiles_equal, diff_before = top_tiles_different;
        for (int col = 0; col < 32; col++) {
            int idx = (mirror_v ? nt_top << 5 : 0) | col;
            if (!top.have[idx]) {
                top_missing++;
                continue;
            }
            uint8_t attr = g->ppu.vram[nt_top * 0x400 + 0x3C0 + (col >> 2)];
            top_tiles_equal += top.tile[idx] == g->ppu.vram[nt_top * 0x400 + col];
            top_tiles_different += top.tile[idx] != g->ppu.vram[nt_top * 0x400 + col];
            top_pal_equal += top.pal[idx] == ((attr >> (col & 2)) & 3);
            top_pal_different += top.pal[idx] != ((attr >> (col & 2)) & 3);
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
    fprintf(stderr, "large view: %u frames with the lines, %u with a row above in the map (state changed by the rebuild: %u, row not in the slot above: %u)\n",
            frames_checked, frames_with_row, state_changed, slot_wrong);
    fprintf(stderr,
            "large view: first row of the picture rebuilt (it is on the screen): tiles %u identical, %u different, %u missing; palettes %u identical, %u "
            "different; row in a wrong slot: %u\n",
            top_tiles_equal, top_tiles_different, top_missing, top_pal_equal, top_pal_different, top_slot_wrong);
    fprintf(stderr, "large view: per frame, first row: %u identical, %u partly different, %u entirely different\n", frames_top_identical, frames_top_partial,
            frames_top_all_different);
    fprintf(stderr, "large view: the game wrote the row above in %u frames: %u tiles identical to the rebuilt ones, %u different\n", fresh_frames, fresh_equal,
            fresh_different);
}
