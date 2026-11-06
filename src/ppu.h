/* Minimal NES PPU. */
#ifndef PPU_H
#define PPU_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ppu_write(Game *g, int reg, uint8_t v); /* reg 0 = $2000 ... 7 = $2007 */
void ppu_read_status(Game *g);
void ppu_render(Game *g, uint8_t *out); /* 256x240 NES palette indices (0-63) */

/* Sprites drawn per line: 8 like the NES (the default), or all of them (64) to remove the flicker. Display only: the game's memory does not change, and
 * neither does the replay tool, which keeps the default. */
void ppu_set_sprite_limit(int per_line);
int ppu_sprite_limit(void);

#endif
