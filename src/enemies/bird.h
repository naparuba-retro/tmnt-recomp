/* Type $97: bird that glides around the player. (functions shared with the other enemy files) */
#ifndef ENEMIES_BIRD_H
#define ENEMIES_BIRD_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ai_bird(Game *g, int x);
void bird_glide_state(Game *g, int x);
void shrapnel_state(Game *g, int x);

#endif
