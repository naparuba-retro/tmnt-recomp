/* The lighting of lighting.c applied to the picture of the game: where it is used (the sewers), the lights (the turtle) and the map of the solid squares, which come from the
 * game's own collision map. Display only: nothing in the game is read for anything but the picture. */
#ifndef LIGHTING_GAME_H
#define LIGHTING_GAME_H
#include <stdint.h>
#include "game.h"

void lighting_game_apply(Game *g, uint32_t *screen, int instant); /* lights the picture of the playfield (not the status bar) */

#endif
