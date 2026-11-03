/* Entity hit boxes and collisions (B6 $9535). (functions shared with the other stage_play files) */
#ifndef STAGE_PLAY_COLLISIONS_H
#define STAGE_PLAY_COLLISIONS_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void check_entity_collisions(Game *g);

#endif
