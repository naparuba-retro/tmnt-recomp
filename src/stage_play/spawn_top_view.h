/* Spawn generator of the top view (B4 $AB94): enemy list of the zone, the van. (functions shared with the other stage_play files) */
#ifndef STAGE_PLAY_SPAWN_TOP_VIEW_H
#define STAGE_PLAY_SPAWN_TOP_VIEW_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void spawn_enemies_near_camera(Game *g);
void spawn_table_pointer(Game *g);

#endif
