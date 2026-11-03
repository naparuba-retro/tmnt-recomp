/* Type $AC: palette band that follows the scrolling. (functions shared with the other enemy files) */
#ifndef ENEMIES_PALETTE_BAND_H
#define ENEMIES_PALETTE_BAND_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ai_palette_band(Game *g, int x);
void palette_band_state(Game *g, int x);

#endif
