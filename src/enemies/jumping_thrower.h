/* Types $42 / $43: jumps towards the player and throws a projectile. (functions shared with the other enemy files) */
#ifndef ENEMIES_JUMPING_THROWER_H
#define ENEMIES_JUMPING_THROWER_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ai_jumping_thrower(Game *g, int x);
void ai_thrower_recover(Game *g, int x);

#endif
