/* Pause / turtle selection screen, game over and continue. */
#ifndef PAUSE_H
#define PAUSE_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void pause_screen(Game *g);      /* B3 $A91C */
void scene_info_init(Game *g);   /* B7 $D179 */
void scene_info_update(Game *g); /* B7 $D1CA */

#endif
