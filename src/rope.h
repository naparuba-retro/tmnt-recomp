/* Rope between the rooftops (level 3). */
#ifndef ROPE_H
#define ROPE_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void rope_redraw(Game *g);         /* B7 $E415 */
void rope_player_state(Game *g);   /* B4 $A438 */
void scene_rope_start(Game *g);    /* B6 $A39B */
void scene_rope_continue(Game *g); /* B6 $A422 */

#endif
