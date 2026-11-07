/* Minimal PPU: registers ($2000-$2007), video memory, rendering of a whole frame.
 * No per-scanline timing: the frame is computed from the state of the registers at the end of the frame
 * (the game has no mid-frame scroll effect during the title and the intro). */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "game.h"

static int large_on;                 /* large view (key L): see ppu_set_large() */
static const PpuRowAbove *large_row; /* the row above the picture, or NULL when there is none */
static PpuWriteHook write_hook;      /* sees the writes to the name tables (the large view runs the game's row drawing on a copy) */

static int large_edges;     /* the screen is a picture of the map: the mask of the 8 left pixels and the black sprites of the right edge are not drawn */
static int large_blank_top; /* lines at the top that show only the backdrop (the pause screen, see large_view.c) */
static const PpuRowAbove *large_rows; /* the rows of the picture rebuilt from the map, by row of the name table (32 entries) */

void ppu_set_large(int on, const PpuRowAbove *row_above, const PpuRowAbove *rows) {
    large_on = on;
    large_row = row_above;
    large_rows = rows;
}

void ppu_set_large_edges(int on) {
    large_edges = on;
}

void ppu_set_large_blank_top(int lines) {
    large_blank_top = lines;
}

void ppu_set_write_hook(PpuWriteHook hook) {
    write_hook = hook;
}

static uint8_t *vram_ptr(Game *g, uint16_t a) {
    a &= 0x3FFF;
    if (a >= 0x3000 && a < 0x3F00) {
        a -= 0x1000;
    }
    if (a >= 0x2000 && a < 0x3000) {
        int nt = (a >> 10) & 3;
        return &g->ppu.vram[(g->ppu.mirror_v ? (nt & 1) : (nt >> 1)) * 0x400 +
                            (a & 0x3FF)]; /* mirroring horizontal : NT0=NT1, NT2=NT3 ; vertical : NT0=NT2, NT1=NT3 */
    }
    if (a >= 0x3F00) {
        int i = a & 0x1F;
        if ((i & 0x13) == 0x10) {
            i &= 0x0F; /* $3F10/14/18/1C = $3F00/04/08/0C */
        }
        return &g->ppu.pal[i];
    }
    return NULL; /* CHR-ROM */
}

void ppu_read_status(Game *g) {
    g->ppu.w = 0;
}

void ppu_write(Game *g, int reg, uint8_t val) {
    Ppu *p = &g->ppu;
    switch (reg) {
        case 0:
            p->ctrl = val;
            p->t = (uint16_t)((p->t & 0xF3FF) | (val & 3) << 10);
            break;
        case 1:
            p->mask = val;
            break;
        case 5:
            if (!p->w) {
                p->t = (uint16_t)((p->t & 0xFFE0) | val >> 3);
                p->x = val & 7;
            } else {
                p->t = (uint16_t)((p->t & 0x8C1F) | (val & 7) << 12 | (val & 0xF8) << 2);
            }
            p->w ^= 1;
            break;
        case 6:
            if (!p->w) {
                p->t = (uint16_t)((p->t & 0x00FF) | (val & 0x3F) << 8);
            } else {
                p->t = (uint16_t)((p->t & 0xFF00) | val);
                p->v = p->t;
            }
            p->w ^= 1;
            break;
        case 7: {
            g->ppu_writes_acc++;
            uint8_t *d = vram_ptr(g, p->v);
            if (d) {
                *d = val;
                if (write_hook) {
                    write_hook((uint16_t)(p->v & 0x3FFF), (int)(d - p->vram), val);
                }
            }
            p->v = (uint16_t)(p->v + ((p->ctrl & 4) ? 32 : 1));
            break;
        }
    }
}

static uint8_t chr_byte(Game *g, int sprite_bank_hi, uint16_t tile_addr) {
    int bank = sprite_bank_hi ? g->chr_bank1 : g->chr_bank0;
    return rom_chr(g->rom, bank, tile_addr);
}

/* One pass of the background from line `pass_from` down, with the scroll t/fx: fills out (colours) and bg_c (colour index, 0 = transparent).
 * Tile row, fine row and name table of each screen line follow the PPU: row 29 wraps to row 0 of the other name table,
 * but 31 wraps to 0 in the same table (a vertical scroll of 240-255 reads the attribute bytes as tiles). */
static void draw_background(Game *g, uint8_t *out, uint8_t *bg_c, int pass_from, uint16_t t, int fx) {
    Ppu *p = &g->ppu;
    int show_bg = p->mask & 0x08, bg_hi = (p->ctrl >> 4) & 1;
    int row_ty[240], row_fy[240], row_nt[240], line_cx[240], line_fx[240];
    int cx = t & 31, cc = (t >> 5) & 31, ff = (t >> 12) & 7, nn = (t >> 10) & 3;
    for (int y = pass_from; y < 240; y++) {
        row_ty[y] = cc;
        row_fy[y] = ff;
        row_nt[y] = nn;
        line_cx[y] = cx;
        line_fx[y] = fx;
        if (++ff == 8) {
            ff = 0;
            if (cc == 29) {
                cc = 0;
                nn ^= 2;
            } else if (cc == 31) {
                cc = 0;
            } else {
                cc++;
            }
        }
    }
    for (int y = pass_from; y < 240; y++) {
        uint8_t *bg_row = &bg_c[y * 256], *out_row = &out[y * 256];
        if (!show_bg || y < large_blank_top) {
            memset(bg_row, 0, 256);
            continue;
        }
        int sy = row_ty[y] * 8 + row_fy[y], ty = row_ty[y];
        /* The line is drawn in runs of pixels that belong to one tile (at most 8): a tile is decoded once for its whole run. */
        int x = 0;
        while (x < 256) {
            int sx = line_cx[y] * 8 + line_fx[y] + x;
            int nt = row_nt[y];
            if (sx >= 256) {
                nt ^= 1;
                sx &= 255;
            }
            int len = 8 - (sx & 7);
            if (len > 256 - x) {
                len = 256 - x;
            }
            if (x < 8 && !(p->mask & 2) && !(large_on && large_edges)) { /* the 8 leftmost pixels are hidden (not in the large view) */
                if (len > 8 - x) {
                    len = 8 - x;
                }
                memset(bg_row + x, 0, (size_t)len);
                x += len;
                continue;
            }
            int tx = sx >> 3;
            uint8_t tile, palette;
            if (large_on && ty >= 30) {
                /* Rows 30 and 31 are not rows of the picture: the PPU reads the attribute table as tiles there. With the large view, the tile of the row
                 * above the picture is used instead, and the backdrop colour where there is none. */
                int idx = (p->mirror_v ? (nt & 1) << 5 : 0) | tx;
                if (!large_row || !large_row->have[idx]) {
                    memset(bg_row + x, 0, (size_t)len);
                    x += len;
                    continue;
                }
                tile = large_row->tile[idx];
                palette = large_row->pal[idx];
            } else if (large_on && !p->mirror_v && large_rows && ((line_fx[y] + x) >> 3) == 32 && large_rows[ty].row_nt == ((nt >> 1) & 1) &&
                       large_rows[ty].wrap_have) {
                /* the partial column at the right edge: with one name table across, the video memory shows the first column again there */
                tile = large_rows[ty].wrap_tile;
                palette = large_rows[ty].wrap_pal;
            } else if (large_on && large_rows && (p->mirror_v || large_rows[ty].row_nt == ((nt >> 1) & 1)) &&
                       large_rows[ty].have[(p->mirror_v ? (nt & 1) << 5 : 0) | tx]) {
                /* a tile of the map where the video memory is not up to date (see large_view.c) */
                int idx = (p->mirror_v ? (nt & 1) << 5 : 0) | tx;
                tile = large_rows[ty].tile[idx];
                palette = large_rows[ty].pal[idx];
            } else {
                uint16_t base = (uint16_t)(0x2000 + nt * 0x400);
                tile = *vram_ptr(g, (uint16_t)(base + ty * 32 + tx));
                uint8_t attr = *vram_ptr(g, (uint16_t)(base + 0x3C0 + (ty >> 2) * 8 + (tx >> 2)));
                int shift = ((ty & 2) << 1) | (tx & 2);
                palette = (attr >> shift) & 3;
            }
            const uint8_t *colours = &p->pal[palette * 4];
            uint16_t a = (uint16_t)(tile * 16 + (sy & 7));
            uint8_t lo = chr_byte(g, bg_hi, a), hi = chr_byte(g, bg_hi, (uint16_t)(a + 8));
            for (int k = 0; k < len; k++) {
                int bit = 7 - ((sx & 7) + k);
                uint8_t c = (uint8_t)(((lo >> bit) & 1) | (((hi >> bit) & 1) << 1));
                bg_row[x + k] = c;
                if (c) {
                    out_row[x + k] = colours[c] & 0x3F;
                }
            }
            x += len;
        }
    }
}

/* Sprite 0 line: first opaque pixel over an opaque background; 240 = no hit. */
static int sprite0_hit_line(Game *g, const uint8_t *bg_c) {
    Ppu *p = &g->ppu;
    int hit = 240;
    int tall0 = (p->ctrl & 0x20) != 0, h0 = tall0 ? 16 : 8;
    uint8_t tile = p->oam[1], attr = p->oam[2];
    int sx0 = p->oam[3];
    for (int row = 0; row < h0 && hit == 240; row++) {
        int y = p->oam[0] + 1 + row;
        if (y >= 240) {
            break;
        }
        int r = (attr & 0x80) ? h0 - 1 - row : row;
        uint16_t a = tall0 ? (uint16_t)((tile & 0xFE) * 16 + (r & 7) + (r >= 8 ? 16 : 0)) : (uint16_t)(tile * 16 + r);
        int table = tall0 ? (tile & 1) : (p->ctrl >> 3) & 1;
        uint8_t lo = chr_byte(g, table, a), hb = chr_byte(g, table, (uint16_t)(a + 8));
        for (int i = 0; i < 8 && hit == 240; i++) {
            int x = sx0 + i;
            if (x > 254) {
                continue;
            }
            int bit = (attr & 0x40) ? i : 7 - i;
            if ((((lo >> bit) & 1) | (((hb >> bit) & 1) << 1)) && bg_c[y * 256 + x]) {
                hit = y;
            }
        }
    }
    return hit;
}

static int sprite_limit = 8; /* sprites per line: 8 on the NES */

void ppu_set_sprite_limit(int per_line) {
    sprite_limit = per_line;
}

int ppu_sprite_limit(void) {
    return sprite_limit;
}

static void draw_sprites(Game *g, uint8_t *out, const uint8_t *bg_c) {
    Ppu *p = &g->ppu;
    int tall = (p->ctrl & 0x20) != 0, h = tall ? 16 : 8;
    for (int y = 0; y < 240; y++) {
        if (y < large_blank_top) {
            continue; /* no sprite on the lines that show only the backdrop */
        }
        int count = 0;
        uint8_t done[256];
        memset(done, 0, sizeof done);
        for (int s = 0; s < 64; s++) {
            int sy = p->oam[s * 4] + 1, row = y - sy;
            if (row < 0 || row >= h) {
                continue;
            }
            if (++count > sprite_limit) {
                break; /* at most 8 sprites per line on the NES: the others disappear, and which ones changes every frame (the flicker) */
            }
            uint8_t tile = p->oam[s * 4 + 1], attr = p->oam[s * 4 + 2];
            int sx = p->oam[s * 4 + 3];
            if (large_on && large_edges && sx == 248 && tile == 0x0F) {
                continue; /* the column of black sprites that hides the right edge of the picture (it counted for the 8 per line) */
            }
            if (attr & 0x80) {
                row = h - 1 - row;
            }
            int hi_table;
            uint16_t a;
            if (tall) {
                hi_table = tile & 1;
                a = (uint16_t)((tile & 0xFE) * 16 + (row & 7) + (row >= 8 ? 16 : 0));
            } else {
                hi_table = (p->ctrl >> 3) & 1;
                a = (uint16_t)(tile * 16 + row);
            }
            uint8_t lo = chr_byte(g, hi_table, a), hb = chr_byte(g, hi_table, (uint16_t)(a + 8));
            for (int i = 0; i < 8; i++) {
                int x = sx + i;
                if (x > 255 || done[x] || (x < 8 && !(p->mask & 4) && !(large_on && large_edges))) {
                    continue;
                }
                int bit = (attr & 0x40) ? i : 7 - i;
                int c = ((lo >> bit) & 1) | (((hb >> bit) & 1) << 1);
                if (!c) {
                    continue;
                }
                done[x] = 1; /* the first opaque sprite wins */
                if ((attr & 0x20) && bg_c[y * 256 + x]) {
                    continue;
                }
                out[y * 256 + x] = p->pal[0x10 + (attr & 3) * 4 + c] & 0x3F;
            }
        }
    }
}

void ppu_render(Game *g, uint8_t *out) {
    Ppu *p = &g->ppu;
    int show_bg = p->mask & 0x08, show_spr = p->mask & 0x10;
    uint8_t backdrop = p->pal[0] & 0x3F;
    memset(out, backdrop, 256 * 240);
    if (!show_bg && !show_spr) {
        return; /* display off: backdrop colour */
    }

    uint8_t bg_c[256 * 240]; /* background colour index (0 = transparent) */
    memset(bg_c, 0, sizeof bg_c);
    draw_background(g, out, bg_c, 0, p->t, p->x);
    /* A split (status bar) changes the scroll after the sprite 0 line.
     * The split falls around pixel 16 of the line after the hit, depending on the phase of the game's idle loop:
     * a few pixels of that line differ from Mesen (3 pixels over 48 frames of the building 0 trace) */
    if (p->split_on && show_spr) {
        int hit = sprite0_hit_line(g, bg_c);
        if (hit != 240) { /* no hit: no split */
            memset(out + (hit + 1) * 256, backdrop, (size_t)(240 - (hit + 1)) * 256);
            draw_background(g, out, bg_c, hit + 1, p->split_v, p->split_x);
        }
    }
    if (show_spr) {
        draw_sprites(g, out, bg_c);
    }
}
