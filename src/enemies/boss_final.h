/* Boss $79: the last boss and its sword ($51). (functions shared with the other enemy files) */
#ifndef ENEMIES_BOSS_FINAL_H
#define ENEMIES_BOSS_FINAL_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ai_final_boss_sword(Game *g, int x);
void boss_ai(Game *g, int x);

#endif
