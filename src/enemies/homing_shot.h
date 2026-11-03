/* Type $33: projectile that realigns on the player. (functions shared with the other enemy files) */
#ifndef ENEMIES_HOMING_SHOT_H
#define ENEMIES_HOMING_SHOT_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ai_homing_shot(Game *g, int x);
void homing_shot_state(Game *g, int x);

#endif
