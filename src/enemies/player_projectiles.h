/* Player projectiles: shuriken, sub-weapons, boomerang. (functions shared with the other enemy files) */
#ifndef ENEMIES_PLAYER_PROJECTILES_H
#define ENEMIES_PLAYER_PROJECTILES_H
#include <stdint.h>
#include <stdio.h>

typedef struct Game Game;

void ai_boomerang(Game *g, int x);
void ai_hostage_boss_shot(Game *g, int x);
void ai_shuriken(Game *g, int x);
void subweapon_offscreen(Game *g, int x);

#endif
