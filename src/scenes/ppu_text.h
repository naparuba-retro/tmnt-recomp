/* PPU text engine (B7 $F23F): fill blocks and typewriter texts. (functions shared with the other player files) */
#ifndef SCENES_PPU_TEXT_H
#define SCENES_PPU_TEXT_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

int ppu_script(Game *g, uint8_t in, uint8_t *ra);

#endif
