/* Vines that hang from the ceilings of the dam (level 2, the swimming part) and bend around the turtle (display only: nothing of the game is read for anything but the
 * map, the camera and the box of the turtle, and nothing is written). The model is the one of the vines of Animal Well (a Verlet chain with exact segment lengths, pushed
 * out of a rectangle by a projection of the position), see TECHNICAL.md. The state is outside `Game`. */
#ifndef VINES_H
#define VINES_H
#include <stdint.h>

typedef struct Game Game;

/* Once per logic frame: finds the ceilings that are on the screen, creates and frees the vines, and moves them. Does nothing when the option is off or outside the dam. */
void vines_update(Game *g);

/* Draws the vines on the 256x240 picture (0xRRGGBB), after the PPU rendering. */
void vines_draw(Game *g, uint32_t *screen);

#endif
