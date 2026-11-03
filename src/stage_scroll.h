/* Scrolling of the level map: columns and rows pushed through the PPU buffer. */
#ifndef STAGE_SCROLL_H
#define STAGE_SCROLL_H
#include <stdint.h>

typedef struct Game Game;

void scroll_camera_x(Game *g); /* B7 $D7D6 */
void scroll_camera_y(Game *g); /* B7 $DC8B */

#endif
