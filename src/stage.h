/* Game and level state machines. */
#ifndef STAGE_H
#define STAGE_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void restart_level_music(Game *g); /* B7 $CBF9 */
void wave_palette(Game *g);        /* B7 $F77D */
void clear_object_slots(Game *g);  /* B7 $EFDB */
void mode3_new_game(Game *g);
void stage_run(Game *g);
void set_scroll_from_camera(Game *g);

#endif
