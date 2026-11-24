/* Smoke behind the tanks of the street (display only). A real 2D fluid ("stable fluids": the speed and the dye are carried by the speed, the pressure keeps the flow free of
 * divergence, the buildings are walls) on a grid of 128 x 120 cells of 2 pixels, in the manner of the smoke of Animal Well (a posterised dye, dithered like a checkerboard).
 * The tanks put dye and a push behind them at every frame; the scroll of the camera moves the whole field. Nothing of the game is written, and the state is outside `Game`. */
#ifndef SMOKE_H
#define SMOKE_H
#include <stdint.h>

typedef struct Game Game;

/* Once per logic frame: the emitters, and one step of the fluid. Does nothing (and forgets the smoke) when the option is off or the picture is not a street. */
void smoke_update(Game *g);

/* Draws the smoke on the 256x240 picture (0xRRGGBB), after the PPU rendering. */
void smoke_draw(Game *g, uint32_t *screen);

#endif
