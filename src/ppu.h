/* Minimal NES PPU. */
#ifndef PPU_H
#define PPU_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ppu_write(Game *g, int reg, uint8_t v); /* reg 0 = $2000 ... 7 = $2007 */
void ppu_read_status(Game *g);
void ppu_render(Game *g, uint8_t *out); /* 256x240 NES palette indices (0-63) */

#endif
