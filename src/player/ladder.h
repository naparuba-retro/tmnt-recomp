/* Ladders (states 11 and 12). (functions shared with the other player files) */
#ifndef PLAYER_LADDER_H
#define PLAYER_LADDER_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void player_ladder(Game *g);
void player_ladder_enter(Game *g);

#endif
