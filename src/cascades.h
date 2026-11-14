/* Waterfalls of the sewers (display only): columns of water that fall from a pipe to the floor, drawn on the 256x240 picture after the PPU rendering. Nothing of the game is
 * read but the camera and the area, and nothing is written. The look is the one of the waterfalls of Animal Well, which are a procedural shader over a rectangle (swell of sines,
 * toothed edges, foam where the swell is low) plus a row of foam at the foot; here it is computed pixel by pixel by the processor. They do not react to the player. */
#ifndef CASCADES_H
#define CASCADES_H
#include <stdint.h>

typedef struct Game Game;

/* Draws the waterfalls of the area on the picture. Does nothing when the option is off or the area has none. */
void cascades_draw(Game *g, uint32_t *screen);

#endif
