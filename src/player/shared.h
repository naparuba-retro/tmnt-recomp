/* shared (functions shared with the other player files) */
#ifndef PLAYER_SHARED_H
#define PLAYER_SHARED_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void player_state_dispatch(Game *g);

#endif
