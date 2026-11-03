/* Top view: walking, doors, camera limits, the van. (functions shared with the other player files) */
#ifndef PLAYER_OVERHEAD_H
#define PLAYER_OVERHEAD_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void door_transition(Game *g);
void overhead_player(Game *g);

#endif
