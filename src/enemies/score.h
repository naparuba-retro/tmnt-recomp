/* Score (BCD points). (functions shared with the other enemy files) */
#ifndef ENEMIES_SCORE_H
#define ENEMIES_SCORE_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void crate_contact(Game *g, int x);
void crate_hit(Game *g, int x);
void enemy_score(Game *g, int x);

#endif
