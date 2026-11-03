/* Type $A5: projectile thrown by an enemy. (functions shared with the other enemy files) */
#ifndef ENEMIES_THROWN_SHOT_H
#define ENEMIES_THROWN_SHOT_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ai_thrown_shot(Game *g, int x);
void thrown_shot_scenery_state(Game *g, int x);

#endif
