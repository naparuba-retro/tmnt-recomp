/* Minimal NES PPU. */
#ifndef PPU_H
#define PPU_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ppu_write(Game *g, int reg, uint8_t v); /* reg 0 = $2000 ... 7 = $2007 */
void ppu_read_status(Game *g);
void ppu_render(Game *g, uint8_t *out); /* 256x240 NES palette indices (0-63) */

/* The large view (key L). In a level the game scrolls the picture 4 lines down, so that the first lines of the screen show the row above the map, which
 * does not exist: the PPU reads the attribute table there as if it were tiles (rows 30 and 31), which gives a band of random tiles. With the large view
 * on, those lines show instead the row above, when the level has one (`row_above`: tiles and palettes by name table column, 0-31 plus 32 for the second
 * name table when the mirroring is vertical), and the backdrop colour when it has not. Display only. */
typedef struct {
    uint8_t have[64];                       /* the tile of this column is known */
    uint8_t wrap_have, wrap_tile, wrap_pal; /* the tile of the partial column at the right edge (with one name table across it is the one of the left edge) */
    uint8_t row_nt;                         /* for the rows of the picture: the name table of the row (when the mirroring is horizontal) */
    uint8_t tile[64];
    uint8_t pal[64]; /* palette 0-3 of the tile */
} PpuRowAbove;
/* `rows` (32 entries, by row of the name table): tiles and palettes of the map that replace those of the video memory where it is not up to date: the
 * first rows of the picture (the game scrolls first and draws the new row later, and the palette of the lower row of a pair arrives with the upper one),
 * and the columns at the left and right edges (the game draws the map only from 8 pixels beyond the left edge, the black sprites and the mask hide the
 * rest). */
void ppu_set_large(int on, const PpuRowAbove *row_above, const PpuRowAbove *rows);

/* Called for every write of a tile or an attribute to the name tables: the address ($2000-$2FFF), the index in the 2 KB of video memory, the value. */
typedef void (*PpuWriteHook)(uint16_t addr, int vram_index, uint8_t value);
void ppu_set_write_hook(PpuWriteHook hook);

/* Sprites drawn per line: 8 like the NES (the default), or all of them (64) to remove the flicker. Display only: the game's memory does not change, and
 * neither does the replay tool, which keeps the default. */
void ppu_set_sprite_limit(int per_line);
int ppu_sprite_limit(void);

#endif
