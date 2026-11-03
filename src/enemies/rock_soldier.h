/* Types $49 / $4A: rock_soldier and its crescent. (functions shared with the other enemy files) */
#ifndef ENEMIES_ROCK_SOLDIER_H
#define ENEMIES_ROCK_SOLDIER_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ai_rock_soldier(Game *g, int x);
void ai_rock_soldier_crescent(Game *g, int x);
int landing_step(Game *g, int x);
void rock_soldier_walk_state(Game *g, int x);

#endif
