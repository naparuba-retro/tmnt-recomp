/* Type $A6: walks, jumps and chases the player. (functions shared with the other enemy files) */
#ifndef ENEMIES_CHASER_JUMPER_H
#define ENEMIES_CHASER_JUMPER_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ai_chaser_jumper(Game *g, int x);
void chaser_substate_dispatch(Game *g, int x);

#endif
